#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <laser_driver/common/data_types.h>

/**
 * @brief 雷达数据驱动
 *
 * 支持两种数据来源：
 *  1. UDP 网络数据（真实雷达通过 UDP 广播扫描数据）
 *  2. 模拟数据（无雷达时用于演示）
 *
 * 数据帧格式（简化）：
 *   起始角度(float,度) | 角分辨率(float,度) | 点数(uint16) | 距离数组(float,米)
 *   以网络字节序传输。
 */
class LidarDriver : public QObject
{
    Q_OBJECT
public:
    explicit LidarDriver(QObject *parent = nullptr);
    ~LidarDriver() override;

    bool Init(const DeviceParam &param);
    bool Start();
    void Stop();
signals:
    /// 每帧点云（直角坐标，米）
    void pointCloudReceived(const QVector<QPointF> &points);
    //void connectionStatusChanged(bool connected);
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl{nullptr};
};
