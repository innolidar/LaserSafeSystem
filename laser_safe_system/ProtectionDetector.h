#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include "ProtectionManager.h"

/**
 * @brief 防护检测器：判断雷达点是否进入防护区域并发出报警
 */
class ProtectionDetector : public QObject
{
    Q_OBJECT
public:
    explicit ProtectionDetector(QObject *parent = nullptr);

    void setManager(ProtectionManager *mgr);

    /// 设置触发报警所需的最小点数
    void setAlarmThreshold(int count);
    int alarmThreshold() const;

    /// 输入雷达点云（直角坐标，米），返回触发报警的区域 ID 列表
    QVector<int> detect(const QVector<QPointF> &points);

    /// 当前是否有任何区域处于报警状态
    bool hasAlarm() const;

    /// 判定某个点是否落在指定区域内
    static bool pointInZone(const QPointF &pt, const ProtectionZone &z);

signals:
    void alarmTriggered(int zoneId, int pointCount);
    void alarmCleared(int zoneId);
    void detectionUpdated();
private:
    static bool pointInPolygon(const QPointF &pt, const QVector<QPointF> &poly);
    static bool pointInCircle(const QPointF &pt, const QPointF &center, double r);
private:
    ProtectionManager*  m_mgr{nullptr};
    int                 m_threshold{1};
};
