#include "ProtectionDetector.h"
#include "GeoUtils.h"

ProtectionDetector::ProtectionDetector(QObject *parent)
    : QObject(parent)
{
    
}

void ProtectionDetector::setManager(ProtectionManager *mgr)
{
    m_mgr = mgr;
}

void ProtectionDetector::setAlarmThreshold(int count)
{
    m_threshold = qMax(1, count);
}

int ProtectionDetector::alarmThreshold() const
{
    return m_threshold;
}

bool ProtectionDetector::hasAlarm() const
{
    if (!m_mgr)
    {
        return false;
    } 
    for (const ProtectionZone &z : m_mgr->zones()) 
    {
        if (z.enabled && z.alarmed)
        {
            return true;
        }
    }
    return false;
}

bool ProtectionDetector::pointInZone(const QPointF &pt, const ProtectionZone &z)
{
    switch (z.shape) 
    {
    case ProtectionZone::Circle:
    {
        return pointInCircle(pt, z.centroid(), z.radius);
    }
    case ProtectionZone::Sector: 
    {
        // 扇形：极角落在区间内 且 半径不超
        auto pr = GeoUtils::cartesianToPolar(pt);
        if (!GeoUtils::angleInRange(pr.second, z.startAngle, z.endAngle))
        {
            return false;
        }
        return pr.first <= z.radius;
    }

    case ProtectionZone::Polygon:
    case ProtectionZone::Rectangle:
    default:
    {
        return pointInPolygon(pt, z.boundaryPoints());
    }
    }
}

QVector<int> ProtectionDetector::detect(const QVector<QPointF> &points)
{
    QVector<int> triggered;
    if (!m_mgr)
    {
        return triggered;
    }
    for (ProtectionZone &z : m_mgr->zones()) 
    {
        if (!z.enabled) 
        {
            z.alarmed = false;
            continue;
        }

        int hitCount = 0;
        for (const QPointF &p : points) 
        {
            if (pointInZone(p, z)) 
            {
                hitCount++;
                if (hitCount >= m_threshold)
                {
                     break; // 已达阈值，提前退出
                } 
            }
        }

        bool wasAlarmed = z.alarmed;
        z.alarmed = (hitCount >= m_threshold);

        if (z.alarmed) 
        {
            triggered.append(z.id);
            if (!wasAlarmed)
            {
                emit alarmTriggered(z.id, hitCount);
            }
        } 
        else 
        {
            if (wasAlarmed)
            {
                emit alarmCleared(z.id);
            }    
        }
    }
    emit detectionUpdated();
    return triggered;
}

// ---------------------------------------------------------------------------
// 几何判断
// ---------------------------------------------------------------------------

/// 射线法判断点是否在多边形内
bool ProtectionDetector::pointInPolygon(const QPointF &pt, const QVector<QPointF> &poly)
{
    if (poly.size() < 3)
    {
        return false;
    } 

    bool inside = false;
    int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) 
    {
        const QPointF &pi = poly[i];
        const QPointF &pj = poly[j];

        bool intersect = ((pi.y() > pt.y()) != (pj.y() > pt.y())) &&
                         (pt.x() < (pj.x() - pi.x()) * (pt.y() - pi.y()) /
                            (pj.y() - pi.y()) + pi.x());
        if (intersect)
        {
            inside = !inside;
        }   
    }
    return inside;
}

bool ProtectionDetector::pointInCircle(const QPointF &pt, const QPointF &center, double r)
{
    double dx = pt.x() - center.x();
    double dy = pt.y() - center.y();
    return (dx * dx + dy * dy) <= r * r;
}
