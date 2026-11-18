#pragma once

#include <QPointF>
#include <QVector>
#include <QtMath>

/**
 * @brief 坐标系与几何工具
 *
 * 角度定义：
 *   - 0° 位置位于 -Y 方向（x=0 轴下半轴）
 *   - 角度逆时针为正
 *   - 有效范围 [0, 360)
 *
 * 坐标系：y 轴正方向为上方（Qt 场景中 y 向下为正，这里取反使物理上方为正）
 *
 * 极坐标转换：
 *   x = r * sin(theta)
 *   y = -r * cos(theta)   （0° 朝 -Y 即正下方）
 */
namespace GeoUtils
{
/// 角度对齐步长（HA25 分辨率 0.25°）
constexpr double kAngleAlign = 0.25;

/// 角度归一化到 [0, 360)
inline double normalizeAngle(double deg)
{
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg;
}

/// 角度对齐到 0.25° 网格
inline double alignAngle(double deg)
{
    return std::round(normalizeAngle(deg) / kAngleAlign) * kAngleAlign;
}

/// 极坐标 -> 直角坐标（y 轴向上为正，0° 朝 -Y 正下方）
inline QPointF polarToCartesian(double radius, double angleDeg)
{
    double rad = qDegreesToRadians(normalizeAngle(angleDeg));
    return QPointF(radius * qSin(rad), -radius * qCos(rad));
}

/// 直角坐标 -> 极坐标，返回 {半径, 角度(度)}
inline QPair<double, double> cartesianToPolar(const QPointF &p)
{
    double r = std::hypot(p.x(), p.y());
    double deg = qRadiansToDegrees(std::atan2(p.x(), -p.y()));
    return {r, normalizeAngle(deg)};
}

/// 判断角度是否落在 [start, end] 的逆时针区间内
inline bool angleInRange(double angle, double start, double end)
{
    double a = normalizeAngle(angle);
    double s = normalizeAngle(start);
    double e = normalizeAngle(end);
    if (s <= e)
        return a >= s && a <= e;
    // 跨越 0° 的区间
    return a >= s || a <= e;
}

/// 将角度限制到上半区（y>=0），即 [90°, 270°]
/// 0° 朝 -Y（正下方），180° 朝 +Y（正上方）
/// y>=0 对应角度在 90°~180°（右上方）或 180°~270°（左上方）
/// 角度落在下半区 [0°, 90°) ∪ (270°, 360°) 时，截断到最近的 90° 或 270°
inline double clampAngleToUpperHalf(double deg)
{
    double a = normalizeAngle(deg);
    // 上半区：90°~270°
    if (a >= 90.0 && a <= 270.0)
        return a;
    // 下半区：截断到最近的边界
    if (a < 90.0)
        return 90.0;    // 0°~90° → 90°
    return 270.0;       // 270°~360° → 270°
}

/// 线段与射线（原点出发，方向 angleDeg）的交点距离，无交点返回 -1
inline double raySegmentIntersection(const QPointF &p1, const QPointF &p2,
                                      double angleDeg)
{
    // 射线方向单位向量
    QPointF dir = polarToCartesian(1.0, angleDeg);
    // 解 P1 + t*(P2-P1) = s*dir
    QPointF seg = p2 - p1;
    double det = seg.x() * (-dir.y()) - seg.y() * (-dir.x());
    if (std::abs(det) < 1e-12)
        return -1.0;

    QPointF diff = -p1;
    double t = (diff.x() * (-dir.y()) - diff.y() * (-dir.x())) / det;
    double s = (seg.x() * diff.y() - seg.y() * diff.x()) / det;

    if (t < 0.0 || t > 1.0 || s < 0.0)
        return -1.0;
    return s;
}

/// 多边形与射线求最大交点距离（用于区域判定上限）
inline double polygonMaxRayDistance(const QVector<QPointF> &poly, double angleDeg)
{
    double maxR = -1.0;
    for (int i = 0; i < poly.size(); ++i) {
        const QPointF &p1 = poly[i];
        const QPointF &p2 = poly[(i + 1) % poly.size()];
        double r = raySegmentIntersection(p1, p2, angleDeg);
        if (r > maxR)
            maxR = r;
    }
    return maxR;
}

/// 判断两线段是否相交（不含共享端点的情况）
/// p1-p2 和 p3-p4
inline bool segmentsIntersect(const QPointF &p1, const QPointF &p2,
                              const QPointF &p3, const QPointF &p4)
{
    auto cross = [](const QPointF &o, const QPointF &a, const QPointF &b) -> double {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };
    double d1 = cross(p3, p4, p1);
    double d2 = cross(p3, p4, p2);
    double d3 = cross(p1, p2, p3);
    double d4 = cross(p1, p2, p4);
    // 标准跨立实验：d1*d2<0 且 d3*d4<0 表示严格相交
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
        return true;
    return false;
}

/// 检查封闭多边形是否有边交叉（自交）
/// poly 为顶点列表，自动首尾相连
inline bool polygonIsSelfIntersecting(const QVector<QPointF> &poly)
{
    int n = poly.size();
    if (n < 4)
        return false;  // 三角形不可能自交
    for (int i = 0; i < n; ++i)
    {
        QPointF a1 = poly[i];
        QPointF a2 = poly[(i + 1) % n];
        for (int j = i + 2; j < n; ++j)
        {
            // 跳过相邻边（共享端点）
            if (j == i || (j + 1) % n == i)
                continue;
            QPointF b1 = poly[j];
            QPointF b2 = poly[(j + 1) % n];
            if (segmentsIntersect(a1, a2, b1, b2))
                return true;
        }
    }
    return false;
}

} // namespace GeoUtils
