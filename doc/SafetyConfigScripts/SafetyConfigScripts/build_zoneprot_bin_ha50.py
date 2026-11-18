#!/usr/bin/env python3
"""Convert a SafetyConfigTool BIN into the legacy ZoneProt_DataType BIN."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import parse_safety_bin as safety  # noqa: E402


HEADER = 0xAA55AA55
BANK_COUNT = 16
ZONE_COUNT = 3
FLAT_ZONE_COUNT = BANK_COUNT * ZONE_COUNT
DATA_CAPACITY = 0x1FE00  # 130,560 bytes

# Offsets produced by the C++ structure in the supplied tests (default
# alignment, no #pragma pack). sizeof(ZoneProt_DataType) == 16268.
OFF_HEADER = 0
ANGLE_LIMIT_START_X100 = 9000
ANGLE_LIMIT_END_X100 = 27000
OFF_BANK_NUM = 4
OFF_ZONE_NUM = 6
OFF_MULTI_SAMPLE = 8
OFF_TARGET_SAMPLE_RATE = 56
OFF_ZONE_SAMPLE_RATE = 104
OFF_ZONE_START_ANGLE = 106
OFF_ZONE_END_ANGLE = 202
OFF_ZONE_SHAPE_TYPE = 298
OFF_ZONE_SECTOR_LENGTH = 346
OFF_DATA = 394 + 48 
STRUCT_SIZE =  131_004 


def choose_run(zone: dict) -> tuple[int, int] | None:
    """Return the enclosing non-zero run in 905-point index coordinates."""
    ranges = zone["angle_ranges"]
    if not ranges:
        return None
    return ranges[0]["start_index"], ranges[-1]["end_index"]

def target_grid(run: tuple[int, int], sample_rate_x100: int) -> tuple[int, int]:
    start_index, end_index = run
    source_start_x100 = round(safety.index_to_angle(start_index) * 100)
    source_end_x100 = round(safety.index_to_angle(end_index) * 100)
    first_tick = max(
        -(-source_start_x100 // sample_rate_x100),
        -(-ANGLE_LIMIT_START_X100 // sample_rate_x100),
    )
    last_tick = min(
        source_end_x100 // sample_rate_x100,
        ANGLE_LIMIT_END_X100 // sample_rate_x100 - 1,
    )
    return first_tick, last_tick


def choose_sample_rate(parsed: dict) -> int:
    for sample_rate_x100 in (10, 20):
        total = 0
        for bank in parsed["banks"]:
            for zone in bank["zones"]:
                if not zone["enabled"]:
                    continue
                run = choose_run(zone)
                if run is None:
                    continue
                first_tick, last_tick = target_grid(run, sample_rate_x100)
                if last_tick >= first_tick:
                    total += last_tick - first_tick + 1
        if total <= DATA_CAPACITY:
            return sample_rate_x100
    raise ValueError("data[0x1FE00] is too small even at 1-degree sampling")

def convert(source: Path, destination: Path) -> dict:
    parsed = safety.parse_file(source)
    output = bytearray(STRUCT_SIZE)

    struct.pack_into("<IHH", output, OFF_HEADER, HEADER, BANK_COUNT, ZONE_COUNT)
    target_sample_rate_x100 = choose_sample_rate(parsed)
    target_step_deg = target_sample_rate_x100 / 100.0
    struct.pack_into("<H", output, OFF_ZONE_SAMPLE_RATE, target_sample_rate_x100)

    data_cursor = 0
    zone_reports = []

    for bank in parsed["banks"]:
        bank_index = bank["bank_index"]
        for zone_index, zone in enumerate(bank["zones"]):
            flat = bank_index * ZONE_COUNT + zone_index

            # The source format stores these once per Bank. The legacy format
            # stores one byte per Zone, so replicate them across all 3 zones.
            output[OFF_MULTI_SAMPLE + flat] = bank["multi_sampling"] & 0xFF
            output[OFF_TARGET_SAMPLE_RATE + flat] = (
                bank["target_resolution"] & 0xFF
            )

            if not zone["enabled"]:
                continue

            run = choose_run(zone)
            if run is None:
                continue
            start_index, end_index = run

            first_tick, last_tick = target_grid(run, target_sample_rate_x100)
            if last_tick < first_tick:
                continue
            start_angle_x100 = first_tick * target_sample_rate_x100
            end_angle_x100 = (last_tick + 1) * target_sample_rate_x100
            struct.pack_into(
                "<H", output, OFF_ZONE_START_ANGLE + flat * 2, start_angle_x100
            )
            struct.pack_into(
                "<H", output, OFF_ZONE_END_ANGLE + flat * 2, end_angle_x100
            )

            legacy_shape = 0 if zone["shape_type"] == 5 else 1
            output[OFF_ZONE_SHAPE_TYPE + flat] = legacy_shape

            source_values = []
            for tick in range(first_tick, last_tick ):
                target_angle = tick * target_step_deg
                source_position = (
                    (target_angle - safety.ANGLE_START_DEG)
                    / safety.ANGLE_STEP_DEG
                )
                left = max(start_index, min(end_index, int(source_position)))
                right = min(end_index, left + 1)
                fraction = max(0.0, min(1.0, source_position - left))
                left_value = zone["distances_mm"][left]
                right_value = zone["distances_mm"][right]
                source_values.append(
                    round(left_value + (right_value - left_value) * fraction)
                )

            if data_cursor + 2*len(source_values) > DATA_CAPACITY:
                raise ValueError(
                    f"data[0x1FE00] 容量不足: 需要至少 "
                    f"{data_cursor + 2*len(source_values)} 字节"
                )
            # output[
            #     OFF_DATA + data_cursor : OFF_DATA + data_cursor + len(source_values)
            # ] = bytes(source_values)

            # '<' 表示小端序，'H' 表示 uint16。如果是大端序，请将 '<' 改为 '>'
            # 根据 source_values 的长度动态生成打包格式字符串，例如 3 个数就是 '<3H'
            pack_format = f"<{len(source_values)}H"

            # 1. 将数值打包为二进制字节流
            binary_data = struct.pack(pack_format, *source_values)

            # 2. 写入到输出缓冲中（长度自动翻倍，因为每个 uint16 占 2 字节）
            output[
                OFF_DATA + data_cursor : OFF_DATA + data_cursor + len(binary_data)
            ] = binary_data



            if legacy_shape == 0 and source_values:
                # Same 100-mm unit used by data[] in the supplied C++ tests.
                struct.pack_into("<H", output, OFF_ZONE_SECTOR_LENGTH + flat * 2, source_values[0] )    
            else :
                data_cursor += 2 *len(source_values)

            zone_reports.append(
                {
                    "bank": bank_index,
                    "zone": zone_index + 1,
                    "flat_index": flat,
                    "start_angle_x100": start_angle_x100,
                    "end_angle_x100": end_angle_x100,
                    "sample_count": len(source_values),
                    "data_offset": data_cursor,
                }
            )
            

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)
    return {
        "output_size": len(output),
        "data_used": data_cursor,
        "data_capacity": DATA_CAPACITY,
        "zone_sample_rate": target_sample_rate_x100,
        "zones": zone_reports,
    }


def verify(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) != STRUCT_SIZE:
        raise ValueError(f"输出长度错误: {len(data)} != {STRUCT_SIZE}")
    header, banks, zones = struct.unpack_from("<IHH", data, OFF_HEADER)
    return {
        "header": f"0x{header:08X}",
        "bank_num": banks,
        "zone_num": zones,
        "zone_sample_rate": struct.unpack_from("<H", data, OFF_ZONE_SAMPLE_RATE)[0],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 ZoneProt_DataType BIN")
    parser.add_argument("source", type=Path, help="SafetyConfigTool 原始 BIN")
    parser.add_argument("output", type=Path, help="输出的 ZoneProt BIN")
    args = parser.parse_args()
    try:
        report = convert(args.source, args.output)
        check = verify(args.output)
        print(f"已生成: {args.output.resolve()}")
        print(f"结构体长度: {report['output_size']} 字节")
        print(f"data使用: {report['data_used']}/{report['data_capacity']} 字节")
        print(
            f"header={check['header']}, bank_num={check['bank_num']}, "
            f"zone_num={check['zone_num']}, sample_rate={check['zone_sample_rate']}"
        )
        for zone in report["zones"]:
            print(
                f"Bank{zone['bank']} Zone{zone['zone']}: "
                f"angle={zone['start_angle_x100']}..{zone['end_angle_x100']}, "
                f"samples={zone['sample_count']}, data+0x{zone['data_offset']:X}"
            )
        return 0
    except (OSError, ValueError, KeyError, struct.error) as error:
        print(f"生成失败: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
