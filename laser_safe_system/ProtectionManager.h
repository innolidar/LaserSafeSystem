#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include "ProtectionZone.h"

/**
 * @brief 防护区域管理器
 *
 * 组织结构（SW-RS-001 / SW-RS-002）：
 *   16 个 Bank，每个 Bank 包含 3 个输出区域（OUT1 / OUT2 / OUT3）。
 *   其中一个区域可被设置为"当前激活的安全区域"。
 */
class ProtectionManager : public QObject
{
    Q_OBJECT
public:
    static constexpr int kBankCount = 16;
    static constexpr int kOutputCount = 3;

    explicit ProtectionManager(QObject *parent = nullptr);

    /// 创建区域并返回 ID
    int addZone(const ProtectionZone &zone);
    int addPolygon(const QVector<QPointF> &points, int bank = 0, int output = 1);
    int addCircle(const QPointF &center, double radius, int bank = 0, int output = 1);
    int addRectangle(const QPointF &center, double w, double h, double angle = 0,int bank = 0, int output = 1);
    int addSector(double startAngle, double endAngle, double radius,int bank = 0, int output = 1);

    bool updateZone(int id, const ProtectionZone &zone);
    bool removeZone(int id);
    void clearZones();

    QVector<ProtectionZone> zones() const;
    ProtectionZone *zone(int id);
    const ProtectionZone *zone(int id) const;

    /// 按 Bank + 输出通道查找区域
    ProtectionZone *zoneAt(int bank, int output);
    /// 某个 Bank 下的所有区域
    QVector<ProtectionZone> zonesOfBank(int bank) const;

    /// 查找 Bank 下指定输出通道的区域（const 版本）
    const ProtectionZone *zoneAtConst(int bank, int output) const;

    /// 当前激活的区域 ID（-1 表示无）
    int activeZoneId() const;
    void setActiveZoneId(int id);
    ProtectionZone *activeZone();

    /// 已启用的区域总数
    int enabledZoneCount() const;

    /// 复制激活区域到同 Bank 的空闲输出通道，返回新区域 ID（-1 表示失败）
    int copyActiveZone();

    /// 擦除激活区域的几何图形（保留配置参数）
    bool eraseActiveZone();

    /// 保存到 JSON 文件
    bool saveToFile(const QString &path) const;
    /// 从 JSON 文件加载
    bool loadFromFile(const QString &path);

signals:
    void zonesChanged();
    void zoneAdded(int id);
    void zoneRemoved(int id);
    void zoneUpdated(int id);
    void activeZoneChanged(int id);
private:
    int nextFreeOutput(int bank) const;
private:
    QVector<ProtectionZone> m_zones{};
    int                     m_next_id{1};
    int                     m_active_id{-1};
};
