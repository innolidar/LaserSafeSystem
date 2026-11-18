#pragma once 

namespace inno
{
struct Scan
{
    float       x{0.f};
    float       y{0.f};
    uint16_t    angle{0};
    uint16_t    distance{0};
    uint16_t    intensity{0};
};
struct LaserHeader
{
    double stamp{0.f}; //第一束激光采集的时间戳
    std::string frame_id{"innolidar"}; //激光雷达所在的坐标系名称 (如 "laser_link")
};

struct LaserScan 
{
    // 1. 标准消息头
    LaserHeader header; 
    // 2. 角度几何参数 (单位: 弧度 rad)
    float angle_min;        // 扫描起始角度
    float angle_max;        // 扫描终止角度
    float angle_increment;  // 相邻两个测量点之间的角度增量

    // 3. 时间参数 (单位: 秒 s)
    float time_increment;   // 相邻两次激光测量的时间间隔
    float scan_time;        // 当前帧扫描与下一帧扫描的时间间隔

    // 4. 测距范围限制 (单位: 米 m)
    float range_min;        // 最小有效检测距离
    float range_max;        // 最大有效检测距离

    // 5. 核心数据数组
    std::vector<float> ranges{};      // 距离数据数组，长度由角度参数决定
    std::vector<float> intensities{}; // 强度数据数组 (可选)，长度通常与 ranges 一致
    std::vector<Scan>  scans{};
};
}
