#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
解析 SafetyConfigTool 生成的 BIN 配置文件。

已经确认的文件结构：

- 全局区域：1292 字节
- Bank 数量：16
- 每个 Bank：12016 字节
- 每个 Bank 包含 3 个区域
- 每个区域描述：256 字节
- 每个区域距离数组：905 个 uint16_t
- 距离单位：毫米
- 角度起点：90.0°
- 角度步进：0.2°
- 每个 Bank 末尾有 CRC32
- 文件末尾有总 CRC32

目标分辨率、多重采样字段：

- Bank + 0x412：目标分辨率，uint16，小端
- Bank + 0x418：多重采样，uint8
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import struct
import sys
import zlib
from pathlib import Path
from typing import Any, Iterable


# ============================================================
# 文件结构常量
# ============================================================

EXPECTED_FILE_SIZE = 193_552

GLOBAL_SIZE = 1_292

BANK_COUNT = 16
BANK_SIZE = 12_016
BANK_DATA_SIZE = BANK_SIZE - 4

ZONE_COUNT = 3

ZONE_DESCRIPTOR_OFFSET = 12
ZONE_DESCRIPTOR_SIZE = 256

DISTANCE_OFFSET = 1_520
DISTANCE_SLOT_SIZE = 2_744
DISTANCE_COUNT = 905

ANGLE_START_DEG = 90.0
ANGLE_STEP_DEG = 0.2

# Bank 内公共参数
TARGET_RESOLUTION_OFFSET = 0x412
MULTI_SAMPLING_OFFSET = 0x418


SHAPE_NAMES = {
    0: "未设置",
    2: "矩形/四边形",
    5: "圆形/圆弧",
    6: "自由多边形",
}


# ============================================================
# 基础读取函数
# ============================================================

def read_u16(data: bytes, offset: int) -> int:
    """读取小端 uint16。"""
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    """读取小端 uint32。"""
    return struct.unpack_from("<I", data, offset)[0]


def index_to_angle(index: int) -> float:
    """
    将905距离数组索引转换为界面角度。

    index 0   = 90.0°
    index 1   = 90.2°
    index 904 = 270.8°
    """
    return ANGLE_START_DEG + index * ANGLE_STEP_DEG


def angle_to_index(angle_deg: float) -> int:
    """将界面角度转换为最接近的距离数组索引。"""
    return round((angle_deg - ANGLE_START_DEG) / ANGLE_STEP_DEG)


def ui_angle_from_xy(x: float, y: float) -> float:
    """
    将 BIN 中的 XY 坐标转换为上位机界面角度。

    内部使用标准数学角度，上位机角度比标准数学角度增加90°。
    """
    return (math.degrees(math.atan2(y, x)) + 90.0) % 360.0


# ============================================================
# 距离有效区间分析
# ============================================================

def find_nonzero_runs(
    values: Iterable[int],
) -> list[tuple[int, int]]:
    """
    查找距离数组中的连续非零区间。

    返回：
        [
            (start_index, end_index),
            ...
        ]
    """
    indexes = [
        index
        for index, value in enumerate(values)
        if value != 0
    ]

    if not indexes:
        return []

    runs: list[tuple[int, int]] = []

    start = indexes[0]
    previous = indexes[0]

    for index in indexes[1:]:
        if index != previous + 1:
            runs.append((start, previous))
            start = index

        previous = index

    runs.append((start, previous))

    return runs


# ============================================================
# 区域解析
# ============================================================

def parse_zone(
    data: bytes,
    bank_offset: int,
    zone_index: int,
) -> dict[str, Any]:

    descriptor_offset = (
        bank_offset
        + ZONE_DESCRIPTOR_OFFSET
        + zone_index * ZONE_DESCRIPTOR_SIZE
    )

    zone_id = data[descriptor_offset]
    shape_type = data[descriptor_offset + 1]
    point_count = data[descriptor_offset + 2]

    # 每个点由 float x + float y 组成，共8字节
    max_point_count = (ZONE_DESCRIPTOR_SIZE - 3) // 8

    if point_count > max_point_count:
        raise ValueError(
            f"区域点数异常："
            f"zone={zone_index + 1}, "
            f"point_count={point_count}, "
            f"max={max_point_count}"
        )

    points: list[dict[str, float]] = []

    for point_index in range(point_count):
        point_offset = (
            descriptor_offset
            + 3
            + point_index * 8
        )

        x, y = struct.unpack_from(
            "<ff",
            data,
            point_offset,
        )

        radius = math.hypot(x, y)
        ui_angle = ui_angle_from_xy(x, y)

        points.append(
            {
                "point_index": point_index,
                "x_m": x,
                "y_m": y,
                "radius_m": radius,
                "ui_angle_deg": ui_angle,
            }
        )

    # 每个区域的905点距离数组
    distance_offset = (
        bank_offset
        + DISTANCE_OFFSET
        + zone_index * DISTANCE_SLOT_SIZE
    )

    distances = list(
        struct.unpack_from(
            f"<{DISTANCE_COUNT}H",
            data,
            distance_offset,
        )
    )

    runs = find_nonzero_runs(distances)

    angle_ranges: list[dict[str, Any]] = []

    for start_index, end_index in runs:
        angle_ranges.append(
            {
                "start_index": start_index,
                "end_index": end_index,
                "start_angle_deg": index_to_angle(
                    start_index
                ),
                "end_angle_deg": index_to_angle(
                    end_index
                ),
                "point_count": (
                    end_index - start_index + 1
                ),
            }
        )

    nonzero_distances = [
        value
        for value in distances
        if value != 0
    ]

    return {
        "zone_index": zone_index,
        "zone_id": zone_id,
        "enabled": False,

        "shape_type": shape_type,
        "shape_name": SHAPE_NAMES.get(
            shape_type,
            f"未知类型({shape_type})",
        ),

        "point_count": point_count,

        "descriptor_offset": descriptor_offset,
        "distance_offset": distance_offset,

        "points": points,
        "angle_ranges": angle_ranges,

        "nonzero_distance_count": len(
            nonzero_distances
        ),

        "min_distance_mm": (
            min(nonzero_distances)
            if nonzero_distances
            else None
        ),

        "max_distance_mm": (
            max(nonzero_distances)
            if nonzero_distances
            else None
        ),

        # 完整905点距离数组
        "distances_mm": distances,
    }


# ============================================================
# Bank 解析
# ============================================================

def parse_bank(
    data: bytes,
    bank_index: int,
) -> dict[str, Any]:

    bank_offset = (
        GLOBAL_SIZE
        + bank_index * BANK_SIZE
    )

    declared_bank_size = read_u32(
        data,
        bank_offset,
    )

    enabled_zone_mask = read_u32(
        data,
        bank_offset + 8,
    )

    # 目标分辨率：Bank + 0x412，uint16
    target_resolution = read_u16(
        data,
        bank_offset
        + TARGET_RESOLUTION_OFFSET,
    )

    # 多重采样：Bank + 0x418，uint8
    multi_sampling = data[
        bank_offset
        + MULTI_SAMPLING_OFFSET
    ]

    zones = [
        parse_zone(
            data,
            bank_offset,
            zone_index,
        )
        for zone_index in range(ZONE_COUNT)
    ]

    # 根据 Bank 的区域启用位图设置状态
    for zone in zones:
        zone_index = zone["zone_index"]

        zone["enabled"] = bool(
            enabled_zone_mask
            & (1 << zone_index)
        )

    # Bank CRC 位于 Bank 最后4字节
    bank_crc_offset = (
        bank_offset
        + BANK_DATA_SIZE
    )

    stored_crc = read_u32(
        data,
        bank_crc_offset,
    )

    calculated_crc = (
        zlib.crc32(
            data[
                bank_offset:
                bank_crc_offset
            ]
        )
        & 0xFFFFFFFF
    )

    return {
        "bank_index": bank_index,
        "offset": bank_offset,

        "declared_size": declared_bank_size,

        "enabled": enabled_zone_mask != 0,
        "enabled_zone_mask": enabled_zone_mask,

        "target_resolution": target_resolution,
        "multi_sampling": multi_sampling,

        "parameter_offsets": {
            "target_resolution_relative": (
                TARGET_RESOLUTION_OFFSET
            ),
            "target_resolution_absolute": (
                bank_offset
                + TARGET_RESOLUTION_OFFSET
            ),

            "multi_sampling_relative": (
                MULTI_SAMPLING_OFFSET
            ),
            "multi_sampling_absolute": (
                bank_offset
                + MULTI_SAMPLING_OFFSET
            ),
        },

        "crc32_offset": bank_crc_offset,
        "crc32_stored": f"0x{stored_crc:08X}",
        "crc32_calculated": (
            f"0x{calculated_crc:08X}"
        ),
        "crc32_ok": stored_crc == calculated_crc,

        "zones": zones,
    }


# ============================================================
# 完整文件解析
# ============================================================

def parse_file(path: Path) -> dict[str, Any]:

    data = path.read_bytes()

    minimum_size = (
        GLOBAL_SIZE
        + BANK_COUNT * BANK_SIZE
        + 4
    )

    if len(data) < minimum_size:
        raise ValueError(
            f"文件过短："
            f"{len(data)} 字节，"
            f"至少需要 {minimum_size} 字节"
        )

    magic = data[0:4]

    declared_header_size = read_u32(
        data,
        4,
    )

    declared_file_size = read_u32(
        data,
        8,
    )

    banks = [
        parse_bank(data, bank_index)
        for bank_index in range(BANK_COUNT)
    ]

    file_crc_offset = len(data) - 4

    stored_file_crc = read_u32(
        data,
        file_crc_offset,
    )

    calculated_file_crc = (
        zlib.crc32(data[:-4])
        & 0xFFFFFFFF
    )

    return {
        "file": str(path.resolve()),

        "actual_file_size": len(data),
        "expected_file_size": EXPECTED_FILE_SIZE,
        "size_ok": (
            len(data) == EXPECTED_FILE_SIZE
        ),

        "magic_hex": magic.hex(" ").upper(),

        "declared_header_size": (
            declared_header_size
        ),

        "declared_file_size": (
            declared_file_size
        ),

        "angle_mapping": {
            "distance_count": DISTANCE_COUNT,
            "start_angle_deg": ANGLE_START_DEG,
            "step_deg": ANGLE_STEP_DEG,
            "end_angle_deg": index_to_angle(
                DISTANCE_COUNT - 1
            ),
        },

        "file_crc32_offset": file_crc_offset,
        "file_crc32_stored": (
            f"0x{stored_file_crc:08X}"
        ),
        "file_crc32_calculated": (
            f"0x{calculated_file_crc:08X}"
        ),
        "file_crc32_ok": (
            stored_file_crc
            == calculated_file_crc
        ),

        "banks": banks,
    }


# ============================================================
# 命令行输出
# ============================================================

def print_summary(
    result: dict[str, Any],
    show_empty: bool = False,
) -> None:

    mapping = result["angle_mapping"]

    print(f"文件: {result['file']}")

    print(
        f"长度: {result['actual_file_size']} 字节 "
        f"({'正确' if result['size_ok'] else '异常'})"
    )

    print(
        f"文件头声明长度: "
        f"{result['declared_file_size']} 字节"
    )

    print(
        f"角度映射: "
        f"index 0..{mapping['distance_count'] - 1} "
        f"=> "
        f"{mapping['start_angle_deg']:.1f}°"
        f".."
        f"{mapping['end_angle_deg']:.1f}°，"
        f"步进 {mapping['step_deg']:.1f}°"
    )

    print(
        f"文件 CRC32: "
        f"{result['file_crc32_stored']} "
        f"({'正确' if result['file_crc32_ok'] else '错误'})"
    )

    for bank in result["banks"]:

        if not show_empty and not bank["enabled"]:
            continue

        print()

        print(
            f"Bank {bank['bank_index']}: "
            f"offset=0x{bank['offset']:X}, "
            f"mask=0x{bank['enabled_zone_mask']:X}, "
            f"CRC="
            f"{'正确' if bank['crc32_ok'] else '错误'}"
        )

        print(
            f"  Bank参数: "
            f"目标分辨率="
            f"{bank['target_resolution']} "
            f"(+0x{TARGET_RESOLUTION_OFFSET:X}, "
            f"uint16)，"
            f"多重采样="
            f"{bank['multi_sampling']}x "
            f"(+0x{MULTI_SAMPLING_OFFSET:X}, "
            f"uint8)"
        )

        for zone in bank["zones"]:

            if (
                not show_empty
                and not zone["enabled"]
            ):
                continue

            status = (
                "启用"
                if zone["enabled"]
                else "未启用"
            )

            print(
                f"  区域{zone['zone_id']} "
                f"[{status}]: "
                f"{zone['shape_name']}，"
                f"点数={zone['point_count']}，"
                f"距离非零="
                f"{zone['nonzero_distance_count']}"
                f"/{DISTANCE_COUNT}，"
                f"范围="
                f"{zone['min_distance_mm']}"
                f".."
                f"{zone['max_distance_mm']} mm"
            )

            if zone["angle_ranges"]:
                ranges = ", ".join(
                    (
                        f"{angle_range['start_angle_deg']:.1f}°"
                        f".."
                        f"{angle_range['end_angle_deg']:.1f}° "
                        f"(index "
                        f"{angle_range['start_index']}"
                        f".."
                        f"{angle_range['end_index']})"
                    )
                    for angle_range
                    in zone["angle_ranges"]
                )

                print(
                    f"    距离有效角度: {ranges}"
                )

            else:
                print("    距离有效角度: 无")

            for point_index, point in enumerate(
                zone["points"],
                start=1,
            ):
                print(
                    f"    点{point_index}: "
                    f"("
                    f"{point['x_m']:.6f}, "
                    f"{point['y_m']:.6f}"
                    f") m，"
                    f"界面角度="
                    f"{point['ui_angle_deg']:.3f}°，"
                    f"半径="
                    f"{point['radius_m']:.6f} m"
                )


# ============================================================
# CSV 导出
# ============================================================

def write_csv_files(
    result: dict[str, Any],
    directory: Path,
) -> None:

    directory.mkdir(
        parents=True,
        exist_ok=True,
    )

    for bank in result["banks"]:

        for zone in bank["zones"]:

            if not zone["enabled"]:
                continue

            csv_path = directory / (
                f"bank{bank['bank_index']}"
                f"_zone{zone['zone_id']}"
                f"_distances.csv"
            )

            with csv_path.open(
                "w",
                newline="",
                encoding="utf-8-sig",
            ) as file:

                writer = csv.writer(file)

                writer.writerow(
                    [
                        "index",
                        "angle_deg",
                        "distance_mm",
                        "valid",
                    ]
                )

                for index, distance in enumerate(
                    zone["distances_mm"]
                ):
                    writer.writerow(
                        [
                            index,
                            f"{index_to_angle(index):.1f}",
                            distance,
                            int(distance != 0),
                        ]
                    )

            print(f"已写出: {csv_path}")


# ============================================================
# 参数解析
# ============================================================

def build_argument_parser() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description=(
            "解析 SafetyConfigTool "
            "生成的 BIN 配置文件"
        )
    )

    parser.add_argument(
        "bin_file",
        type=Path,
        help="待解析的 BIN 文件",
    )

    parser.add_argument(
        "--json",
        dest="json_file",
        type=Path,
        help="将完整解析结果写入 JSON 文件",
    )

    parser.add_argument(
        "--csv-dir",
        type=Path,
        help=(
            "将每个启用区域的905个距离值"
            "写入指定 CSV 目录"
        ),
    )

    parser.add_argument(
        "--show-empty",
        action="store_true",
        help="同时显示未启用的 Bank 和区域",
    )

    return parser


# ============================================================
# 主程序
# ============================================================

def main() -> int:

    parser = build_argument_parser()
    args = parser.parse_args()

    try:
        result = parse_file(args.bin_file)

        print_summary(
            result,
            show_empty=args.show_empty,
        )

        if args.json_file:
            args.json_file.parent.mkdir(
                parents=True,
                exist_ok=True,
            )

            args.json_file.write_text(
                json.dumps(
                    result,
                    ensure_ascii=False,
                    indent=2,
                ),
                encoding="utf-8",
            )

            print(
                f"已写出: {args.json_file}"
            )

        if args.csv_dir:
            write_csv_files(
                result,
                args.csv_dir,
            )

        return 0

    except (
        OSError,
        ValueError,
        struct.error,
    ) as error:

        print(
            f"解析失败: {error}",
            file=sys.stderr,
        )

        return 1


if __name__ == "__main__":
    raise SystemExit(main())