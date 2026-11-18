#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

/**
 * @brief 单个判定点的射线采样结果（SW-RS-012）
 */
struct ZoneSample
{
    double angle = 0.0;   ///< 角度（度），已按 0.25° 对齐
    double range = 0.0;   ///< 该角度射线上区域边界的距离（米）
};

/**
 * @brief 防护区域数据结构
 *
 * 形状类型：
 *   - Polygon   多边形
 *   - Circle    圆形
 *   - Rectangle 矩形
 *   - Sector    扇形（起始角度 / 终止角度 / 半径）
 */
struct ProtectionZone
{
    enum Shape
    {
        Polygon = 0,
        Circle,
        Rectangle,
        Sector
    };
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    /// 依据形状生成用于绘制与判定的边界多边形（场景坐标，米）
    QVector<QPointF> boundaryPoints() const;

    /// 起始角度（度）。矩形/多边形取顶点最小极角，扇形取显式起始角
    double effectiveStartAngle() const;
    /// 终止角度（度）
    double effectiveEndAngle() const;

    /// 计算判定数据（SW-RS-012）：按对齐后的角度步长求射线与边界交点
    /// @param maxSamples 采样点数量上限
    QVector<ZoneSample> computeSamples(int maxSamples) const;

    /// 区域几何中心
    QPointF centroid() const;

    /// 从 JSON 对象构造
    static ProtectionZone zoneFromJson(const QJsonObject &obj);
public:
    /// 区域唯一 ID
    int id = 0;
    /// 区域所属 Bank（0-15）与输出通道（1-3）
    int bankIndex = 0;
    int outputIndex = 1;

    QString name;
    Shape shape = Polygon;

    /// 多边形顶点（Polygon / Rectangle / Sector 的边界折线）
    QVector<QPointF> points;
    /// 圆形半径 / 扇形外半径（米）
    double radius = 0.0;
    /// 矩形宽高（米）
    double width = 0.0;
    double height = 0.0;
    /// 矩形旋转角度（度）
    double angle = 0.0;

    /// 扇形起始角 / 终止角（度，逆时针，0° 朝 -Y）
    double startAngle = 0.0;
    double endAngle = 0.0;

    /// 目标分辨率 (2-10)，SW-RS-003
    int targetResolution = 3;
    /// 多重采样率 (2x-10x)，SW-RS-003
    int multiSampling = 2;

    bool enabled = true;
    bool alarmed = false;
};

namespace ZoneUtils
{
/// 极角最小值/最大值对应的顶点索引（用于区域起始/终止角）
void minMaxAngleVertices(const QVector<QPointF> &poly, int &minIdx, int &maxIdx,
                         double &minAngle, double &maxAngle);
}
