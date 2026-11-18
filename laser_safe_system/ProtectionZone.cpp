#include "ProtectionZone.h"
#include "GeoUtils.h"
#include <QJsonValue>
#include <algorithm>

// ===========================================================================
// JSON 序列化
// ===========================================================================
QJsonObject ProtectionZone::toJson() const
{
    QJsonObject obj;
    obj["id"]          = id;
    obj["bankIndex"]   = bankIndex;
    obj["outputIndex"] = outputIndex;
    obj["name"]        = name;
    obj["shape"]       = static_cast<int>(shape);
    obj["enabled"]     = enabled;

    QJsonArray ptsArr;
    for (const QPointF &p : points) {
        QJsonArray pair;
        pair.append(p.x());
        pair.append(p.y());
        ptsArr.append(pair);
    }
    obj["points"] = ptsArr;

    obj["radius"]  = radius;
    obj["width"]   = width;
    obj["height"]  = height;
    obj["angle"]   = angle;
    obj["startAngle"] = startAngle;
    obj["endAngle"]   = endAngle;
    obj["targetResolution"] = targetResolution;
    obj["multiSampling"]    = multiSampling;
    return obj;
}

void ProtectionZone::fromJson(const QJsonObject &obj)
{
    id          = obj["id"].toInt(0);
    bankIndex   = obj["bankIndex"].toInt(0);
    outputIndex = obj["outputIndex"].toInt(1);
    name        = obj["name"].toString();
    shape       = static_cast<Shape>(obj["shape"].toInt(static_cast<int>(Polygon)));
    enabled     = obj["enabled"].toBool(true);

    points.clear();
    const QJsonArray ptsArr = obj["points"].toArray();
    for (const QJsonValue &v : ptsArr) {
        const QJsonArray pair = v.toArray();
        if (pair.size() >= 2)
            points.append(QPointF(pair[0].toDouble(), pair[1].toDouble()));
    }

    radius  = obj["radius"].toDouble();
    width   = obj["width"].toDouble();
    height  = obj["height"].toDouble();
    angle   = obj["angle"].toDouble();
    startAngle = obj["startAngle"].toDouble();
    endAngle   = obj["endAngle"].toDouble();
    targetResolution = obj["targetResolution"].toInt(3);
    multiSampling    = obj["multiSampling"].toInt(2);
    alarmed = false;
}

ProtectionZone ProtectionZone::zoneFromJson(const QJsonObject &obj)
{
    ProtectionZone z;
    z.fromJson(obj);
    return z;
}

// ===========================================================================
// 几何
// ===========================================================================
QVector<QPointF> ProtectionZone::boundaryPoints() const
{
    QVector<QPointF> poly;

    switch (shape) 
    {
    case Polygon:
        poly = points;
        break;

    case Circle: 
    {
        const int segs = 64;
        for (int i = 0; i < segs; ++i) 
        {
            double a = 360.0 * double(i) / double(segs);
            poly.append(GeoUtils::polarToCartesian(radius, a));
        }
        break;
    }

    case Rectangle: 
    {
        // points 存的是绝对坐标（含中心偏移），直接返回
        poly = points;
        break;
    }

    case Sector: 
    {
        // 扇形：从起始角逆时针扫到终止角，再回到圆心
        // 始终在上半区（y>=0）绘制：startAngle/endAngle 在 [90°, 270°]
        // sweep = 逆时针从 start 到 end 的角度差，≤180°，不经过下半区
        double s = GeoUtils::normalizeAngle(startAngle);
        double e = GeoUtils::normalizeAngle(endAngle);
        double sweep = GeoUtils::normalizeAngle(e - s);
        if (sweep > 180.0)
            sweep = 360.0 - sweep;  // 取最小夹角，不经过下半区
        if (sweep < 3.0)
            sweep = 3.0;
        const int segs = qMax(8, int(sweep / 3.0) + 1);
        for (int i = 0; i <= segs; ++i) 
        {
            double a = s + sweep * double(i) / double(segs);
            poly.append(GeoUtils::polarToCartesian(radius, a));
        }
        poly.append(QPointF(0, 0)); // 圆心
        break;
    }
    }
    return poly;
}

QPointF ProtectionZone::centroid() const
{
    if (shape == Sector)
        return QPointF(0, 0);
    if (shape == Circle)
        return QPointF(0, 0);

    if (!points.isEmpty()) {
        QPointF c(0, 0);
        for (const QPointF &p : points)
            c += p;
        return c / double(points.size());
    }
    return QPointF(0, 0);
}

// ===========================================================================
// 起始/终止角度（SW-RS-011）
// ===========================================================================
void ZoneUtils::minMaxAngleVertices(const QVector<QPointF> &poly, int &minIdx, int &maxIdx,
                                    double &minAngle, double &maxAngle)
{
    minIdx = maxIdx = -1;
    minAngle = 360.0;
    maxAngle = 0.0;
    bool has = false;

    for (int i = 0; i < poly.size(); ++i) 
    {
        double a = GeoUtils::cartesianToPolar(poly[i]).second;
        if (!has) 
        {
            minAngle = maxAngle = a;
            minIdx = maxIdx = i;
            has = true;
            continue;
        }
        if (a < minAngle) { minAngle = a; minIdx = i; }
        if (a > maxAngle) { maxAngle = a; maxIdx = i; }
    }
}

double ProtectionZone::effectiveStartAngle() const
{
    if (shape == Sector)
        return GeoUtils::alignAngle(startAngle);

    QVector<QPointF> poly = boundaryPoints();
    int minIdx, maxIdx;
    double minA, maxA;
    ZoneUtils::minMaxAngleVertices(poly, minIdx, maxIdx, minA, maxA);
    return GeoUtils::alignAngle(minA);
}

double ProtectionZone::effectiveEndAngle() const
{
    if (shape == Sector)
    {
        return GeoUtils::alignAngle(endAngle);
    }
    QVector<QPointF> poly = boundaryPoints();
    int minIdx, maxIdx;
    double minA, maxA;
    ZoneUtils::minMaxAngleVertices(poly, minIdx, maxIdx, minA, maxA);
    return GeoUtils::alignAngle(maxA);
}

// ===========================================================================
// 判定数据计算（SW-RS-012）
// ===========================================================================
QVector<ZoneSample> ProtectionZone::computeSamples(int maxSamples) const
{
    QVector<ZoneSample> result;

    QVector<QPointF> poly = boundaryPoints();
    if (poly.size() < 2)
    {
        return result;
    } 

    double startA = effectiveStartAngle();
    double endA = effectiveEndAngle();

    // 计算逆时针扫描区间
    double sweep = endA - startA;
    if (sweep <= 0.0)
    {
        sweep += 360.0;
    }

    // 采样步长：0.25° 对齐
    const double step = GeoUtils::kAngleAlign;
    int count = int(std::ceil(sweep / step)) + 1;
    if (count > maxSamples)
    {
        count = maxSamples;
    } 
    if (count < 1)
    {
        count = 1;
    }

    // 均匀取样（当采样点受限时按比例放大步长）
    double actualStep = (count > 1) ? sweep / double(count - 1) : sweep;

    for (int i = 0; i < count; ++i) 
    {
        double ang = GeoUtils::alignAngle(startA + actualStep * double(i));

        double r = -1.0;
        if (shape == Sector) 
        {
            // 扇形：边界半径固定
            r = (GeoUtils::angleInRange(ang, startA, endA)) ? radius : -1.0;
        } 
        else if (shape == Circle) 
        {
            r = radius;
        } 
        else 
        {
            r = GeoUtils::polygonMaxRayDistance(poly, ang);
        }
        ZoneSample s;
        s.angle = ang;
        s.range = (r < 0.0) ? 0.0 : r;
        result.append(s);
    }
    return result;
}
