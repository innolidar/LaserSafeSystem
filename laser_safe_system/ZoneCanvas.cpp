#include "ZoneCanvas.h"
#include "ProtectionManager.h"
#include "ProtectionDetector.h"
#include "LidarDriver.h"
#include "GeoUtils.h"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsPathItem>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFrame>
#include <QRandomGenerator>
#include <QFontMetrics>
#include <qmath.h>
#include <limits>
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 图元 data() 键
static const int kDataZoneId   = 0;
static const int kDataIsVertex = 1;
static const int kDataIsPoint  = 2;
static const int kDataVertIdx  = 3;  ///< 顶点在 boundaryPoints() 中的索引

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------
ZoneCanvas::ZoneCanvas(QWidget *parent)
    : QGraphicsView(parent)
{
    initScene();
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundBrush(QColor(0, 0, 0));

    buildToolBar();

    //connect(&m_simTimer, &QTimer::timeout, this, &ZoneCanvas::onTimeout);
    //m_simTimer.start(50);
}

ZoneCanvas::~ZoneCanvas() = default;

// ---------------------------------------------------------------------------
// 场景初始化
// ---------------------------------------------------------------------------
void ZoneCanvas::initScene()
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    const int sceneSize = 2000;
    m_scene->setSceneRect(-sceneSize / 2, -sceneSize / 2, sceneSize, sceneSize);

    drawScanSector();
    drawGrid();

    // 点云用单个 PathItem（避免 271 个独立图元拖慢场景索引）
    m_point_item = new QGraphicsPathItem;
    m_point_item->setPen(Qt::NoPen);
    m_point_item->setBrush(QColor(0, 230, 0));
    m_point_item->setZValue(50);
    m_scene->addItem(m_point_item);

    // 选中点云
    m_selected_item = new QGraphicsPathItem;
    m_selected_item->setPen(QPen(QColor(255, 255, 0)));
    m_selected_item->setBrush(QColor(255, 180, 0));
    m_selected_item->setZValue(55);
    m_scene->addItem(m_selected_item);
}

// ---------------------------------------------------------------------------
// 右侧工具栏
// ---------------------------------------------------------------------------
void ZoneCanvas::buildToolBar()
{
    auto *bar = new QFrame(this);
    bar->setFrameShape(QFrame::NoFrame);
    bar->setFixedWidth(40);
    bar->setAttribute(Qt::WA_TranslucentBackground, false);

    bar->setStyleSheet(
        "QFrame { background: rgba(245,245,245,220); border: 1px solid #c8c8c8; }"
        "QToolButton { background: transparent; border: none; color: #1f6fb5; font-size: 15px; }"
        "QToolButton:hover { background: #dceaf7; }"
        "QToolButton:checked { background: #c9dff2; }");

    auto *lay = new QVBoxLayout(bar);
    lay->setContentsMargins(2, 3, 2, 3);
    lay->setSpacing(2);

    auto mk = [bar, lay](const QString &text, const QString &tip, bool checkable) {
        auto *b = new QToolButton(bar);
        b->setText(text);
        b->setToolTip(tip);
        b->setFixedSize(26, 26);
        b->setCheckable(checkable);
        b->setAutoRaise(false);
        b->setCursor(Qt::PointingHandCursor);
        lay->addWidget(b);
        return b;
    };

    m_btn_select = mk(QStringLiteral("▲"), tr("选择 / 拖动"), true);
    m_btn_rect   = mk(QStringLiteral("■"), tr("矩形区域"), true);
    //m_btn_circle = mk(QStringLiteral("●"), tr("圆形区域"), true);
    m_btn_poly   = mk(QStringLiteral("◆"), tr("多边形区域"), true);
    m_btn_sector = mk(QStringLiteral("◢"), tr("扇形区域"), true);
    m_btn_pen    = mk(QStringLiteral("✎"), tr("铅笔绘制"), true);
    m_btn_erase  = mk(QStringLiteral("◐"), tr("擦除区域"), false);

    m_btn_select->setChecked(true);

    lay->addStretch();
    bar->raise();

    QObject::connect(m_btn_select, &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Select); });
    QObject::connect(m_btn_rect,   &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Rectangle); });
    //connect(m_btn_circle, &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Circle); });
    QObject::connect(m_btn_poly,   &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Polygon); });
    QObject::connect(m_btn_sector, &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Sector); });
    QObject::connect(m_btn_pen,    &QToolButton::clicked, this, [this]() { setDrawMode(DrawMode::Pen); });
    QObject::connect(m_btn_erase,  &QToolButton::clicked, this, [this]() { emit eraseRequested(); });
}

// ---------------------------------------------------------------------------
// 模块关联
// ---------------------------------------------------------------------------
void ZoneCanvas::setProtectionManager(ProtectionManager *mgr)
{
    m_mgr = mgr;
    if (m_mgr) 
    {
        connect(m_mgr, &ProtectionManager::zonesChanged,   this, &ZoneCanvas::onZonesChanged);
        connect(m_mgr, &ProtectionManager::activeZoneChanged, this, [this](int){
            // 选中区域变化时重建图元，使只有选中图形显示特征点
            rebuildProtectionItems();
        });
        rebuildProtectionItems();
    }
}

void ZoneCanvas::setProtectionDetector(ProtectionDetector *det)
{
    m_det = det;
}

void ZoneCanvas::setLidarDriver(LidarDriver *drv)
{
    m_drv = drv;
    if (m_drv) 
    {
        QObject::connect(m_drv, &LidarDriver::pointCloudReceived,this, &ZoneCanvas::onLidarData);
        //m_usingSim = m_drv->isSimulated();
    }
}

// ---------------------------------------------------------------------------
// 模式与参数
// ---------------------------------------------------------------------------
void ZoneCanvas::setDrawMode(DrawMode mode)
{
    m_mode = mode;
    clearDraft();

    if (m_btn_select) m_btn_select->setChecked(mode == DrawMode::Select);
    if (m_btn_rect)   m_btn_rect->setChecked(mode == DrawMode::Rectangle);
    //if (m_btn_circle) m_btn_circle->setChecked(mode == DrawMode::Circle);
    if (m_btn_poly)   m_btn_poly->setChecked(mode == DrawMode::Polygon);
    if (m_btn_sector) m_btn_sector->setChecked(mode == DrawMode::Sector);
    if (m_btn_pen)    m_btn_pen->setChecked(mode == DrawMode::Pen);

    setCursor(mode == DrawMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void ZoneCanvas::setViewRange(double halfWidthMeters, double halfHeightMeters)
{
    m_half_width = halfWidthMeters;
    m_half_height = halfHeightMeters;
    m_max_range = qMax(halfWidthMeters, halfHeightMeters) * 1.5;
    drawGrid();
    drawScanSector();
}

void ZoneCanvas::setScanRange(double startDeg, double endDeg)
{
    m_scan_start = startDeg;
    m_scan_end = endDeg;
    drawGrid();
    drawScanSector();
}

void ZoneCanvas::setCartesianMode(bool cartesian)
{
    m_cartesian = cartesian;
}

void ZoneCanvas::setGridVisible(bool on)
{
    if (m_grid_visible == on)
    {
        return;
    }
    m_grid_visible = on;
    drawGrid();
}

void ZoneCanvas::setTargetBank(int bank, int output)
{
    bool is_changed = (m_targetOutput != output | m_targetBank != bank);
    m_targetBank = bank;
    m_targetOutput = output;
    // Output 切换后需重建图元，使只有选中 OUTPUT 的特征点显示
    if (is_changed)
    {
        rebuildProtectionItems();
    }
}

void ZoneCanvas::setCurrentBank(int bank)
{
    if (m_current_bank == bank)
    {
        return;
    } 
    m_current_bank = bank;
    rebuildProtectionItems();
}

// ---------------------------------------------------------------------------
// 兼容旧接口（setActiveZone / setZone）
// ---------------------------------------------------------------------------
void ZoneCanvas::setActiveZone(const QVector<QPointF> &points,
                               const QColor &edge, const QColor &fill)
{
    m_activePoints = points;
    m_activeEdge = edge;
    m_activeFill = fill;

    if (m_activeItem) 
    {
        m_scene->removeItem(m_activeItem);
        delete m_activeItem;
        m_activeItem = nullptr;
    }

    if (points.size() < 2)
        return;

    QPolygonF poly;
    for (const QPointF &p : points)
        poly << radarToScene(p);

    m_activeItem = m_scene->addPolygon(poly, QPen(edge, 2), QBrush(fill));
    m_activeItem->setZValue(28);
}

void ZoneCanvas::clearActiveZone()
{
    m_activePoints.clear();
    if (m_activeItem) {
        m_scene->removeItem(m_activeItem);
        delete m_activeItem;
        m_activeItem = nullptr;
    }
}

void ZoneCanvas::setZone(const QVector<QPointF> &points, const QColor &edge, const QColor &fill)
{
    setActiveZone(points, edge, fill);
}

// ---------------------------------------------------------------------------
// 坐标换算
// ---------------------------------------------------------------------------
QPointF ZoneCanvas::sceneToRadar(const QPointF &scenePos) const
{
    // 雷达坐标 y 向上为正，Qt 场景 y 向下为正，需要翻转 y
    return QPointF(scenePos.x() / m_scale, -scenePos.y() / m_scale);
}

QPointF ZoneCanvas::radarToScene(const QPointF &radarPos) const
{
    // 雷达坐标 y 向上为正，Qt 场景 y 向下为正，需要翻转 y
    return QPointF(radarPos.x() * m_scale, -radarPos.y() * m_scale);
}

bool ZoneCanvas::withinScanRange(const QPointF &radarPt) const
{
    double r = std::hypot(radarPt.x(), radarPt.y());
    if (r > m_max_range)
        return false;
    double a = GeoUtils::cartesianToPolar(radarPt).second;
    return GeoUtils::angleInRange(a, m_scan_start, m_scan_end);
}

QPointF ZoneCanvas::clampToUpperHalf(const QPointF &radarPt) const
{
    // 限制到上半区：y >= 0（形状在上半区绘制和拖动）
    if (radarPt.y() < 0.0)
        return QPointF(radarPt.x(), 0.0);
    return radarPt;
}

// ---------------------------------------------------------------------------
// 网格 + 坐标轴
// ---------------------------------------------------------------------------
void ZoneCanvas::drawGrid()
{
    for (auto *item : m_grid_items) {
        m_scene->removeItem(item);
        delete item;
    }
    m_grid_items.clear();

    double axisLen = m_max_range * m_scale;

    // 直角坐标网格（1m 一格）
    if (m_grid_visible) {
        QPen gridPen(QColor(80, 80, 80), 1, Qt::DotLine);
        int startX = int(std::floor(-m_half_width));
        int endX   = int(std::ceil(m_half_width));
        int startY = int(std::floor(-m_half_height));
        int endY   = int(std::ceil(m_half_height));

        for (int v = startX; v <= endX; ++v) {
            double p = v * m_scale;
            auto *h = m_scene->addLine(-axisLen, p, axisLen, p, gridPen);
            h->setZValue(0);
            m_grid_items.append(h);
        }
        for (int v = startY; v <= endY; ++v) {
            double p = v * m_scale;
            auto *vv = m_scene->addLine(p, -axisLen, p, axisLen, gridPen);
            vv->setZValue(0);
            m_grid_items.append(vv);
        }

        // 距离标签（类似尺子刻度）
        // X 轴标签：沿底边（场景 y = half_height*scale），标注每米 x 坐标
        double bottomEdge = m_half_height * m_scale;
        for (int v = startX; v <= endX; ++v) {
            if (v == 0)
                continue;  // 原点不标
            double px = v * m_scale;
            auto *label = m_scene->addSimpleText(QString::number(v));
            label->setPos(px - 6, bottomEdge + 4);
            label->setZValue(2);
            label->setBrush(QColor(180, 180, 180));
            m_grid_items.append(label);
        }
        // Y 轴标签：沿左边（场景 x = -half_width*scale），标注每米 y 坐标
        // 场景 y 向下为正，雷达 y = -场景y/m_scale
        double leftEdge = -m_half_width * m_scale;
        for (int v = startY; v <= endY; ++v) {
            if (v == 0)
                continue;  // 原点不标
            double py = v * m_scale;
            auto *label = m_scene->addSimpleText(QString::number(-v));
            label->setPos(leftEdge - 22, py - 8);
            label->setZValue(2);
            label->setBrush(QColor(180, 180, 180));
            m_grid_items.append(label);
        }
    }

    // 坐标轴
    QPen axisPen(QColor(255, 255, 255), 1.5);
    auto *xAxis = m_scene->addLine(-axisLen, 0, axisLen, 0, axisPen);
    auto *yAxis = m_scene->addLine(0, -axisLen, 0, axisLen, axisPen);
    xAxis->setZValue(5);
    yAxis->setZValue(5);
    m_grid_items.append(xAxis);
    m_grid_items.append(yAxis);

    // 角度刻度线（每 30°）
    QPen angPen(QColor(255, 255, 255, 80), 1, Qt::DotLine);
    for (int step = 0; step <= 9; ++step) 
    {
        double deg = GeoUtils::normalizeAngle(m_scan_start + step * 30.0);
        QPointF dir = radarToScene(GeoUtils::polarToCartesian(axisLen / m_scale, deg));
        auto *line = m_scene->addLine(0, 0, dir.x(), dir.y(), angPen);
        line->setZValue(0);
        m_grid_items.append(line);

        QPointF lp = radarToScene(GeoUtils::polarToCartesian(axisLen * 0.93 / m_scale, deg));
        int labelDeg = int(deg + 0.5) % 360;
        auto *label = m_scene->addSimpleText(QString::number(labelDeg) + "\u00b0");
        label->setPos(lp.x() - 10, lp.y() - 8);
        label->setZValue(2);
        label->setBrush(QColor(255, 255, 255));
        m_grid_items.append(label);
    }

    // 扫描边界线
    QPen boundPen(QColor(255, 255, 255, 180), 2, Qt::DashLine);
    for (double bound : {m_scan_start, m_scan_end}) 
    {
        QPointF dir = radarToScene(GeoUtils::polarToCartesian(axisLen / m_scale, bound));
        auto *line = m_scene->addLine(0, 0, dir.x(), dir.y(), boundPen);
        line->setZValue(4);
        m_grid_items.append(line);
    }

    // 中心雷达标记（红白小三角）
    {
        // 红色三角（朝下，表示雷达朝向）
        QPolygonF tri;
        tri << QPointF(-5, -7) << QPointF(5, -7) << QPointF(0, 4);
        auto *mark = m_scene->addPolygon(tri, QPen(QColor(200, 30, 30), 1),
                                         QBrush(QColor(220, 40, 40)));
        mark->setZValue(10);
        m_grid_items.append(mark);

        // 白色三角
        QPolygonF tri2;
        tri2 << QPointF(7, -5) << QPointF(14, -5) << QPointF(10.5, 3);
        auto *mark2 = m_scene->addPolygon(tri2, QPen(QColor(255, 255, 255), 1),
                                          QBrush(QColor(255, 255, 255)));
        mark2->setZValue(10);
        m_grid_items.append(mark2);
    }
}

// ---------------------------------------------------------------------------
// 盲区扇形（灰色楔形）
// ---------------------------------------------------------------------------
void ZoneCanvas::drawScanSector()
{
    if (m_scan_sector_item) 
    {
        m_scene->removeItem(m_scan_sector_item);
        delete m_scan_sector_item;
        m_scan_sector_item = nullptr;
    }

    // 扫描覆盖角度（逆时针从 start 到 end）
    double scanSweep = m_scan_end - m_scan_start;
    if (scanSweep < 0.0)
        scanSweep += 360.0;

    // 盲区跨度 = 360° - 扫描覆盖角度
    double blindSweep = 360.0 - scanSweep;
    if (blindSweep <= 0.0 || blindSweep >= 360.0)
        blindSweep = 90.0;

    // 盲区中线 = scanEnd + blindSweep/2（逆时针方向）
    double midDeg = GeoUtils::normalizeAngle(m_scan_end + blindSweep * 0.5);
    double aLeft  = GeoUtils::normalizeAngle(midDeg - blindSweep * 0.5);
    double aRight = GeoUtils::normalizeAngle(midDeg + blindSweep * 0.5);

    // 盲区三角形：原点 + 底边两端点，底边固定在视场最下边
    // 场景坐标：y 向下为正，视场底部 y = half_height * scale
    double bottomY = m_half_height * m_scale;
    double halfW = m_half_width * m_scale;

    // 底边两端点 x 由盲区两条边界角度决定：
    // 将角度方向投影到底边所在水平线上，左端取负、右端取正
    QPointF dirLeft  = radarToScene(GeoUtils::polarToCartesian(1.0, aLeft));
    QPointF dirRight = radarToScene(GeoUtils::polarToCartesian(1.0, aRight));
    // 底边端点 x：沿角度方向延伸到视场宽度边界
    double xLeft  = (dirLeft.x()  != 0.0) ? dirLeft.x()  / qAbs(dirLeft.y())  * bottomY : -halfW;
    double xRight = (dirRight.x() != 0.0) ? dirRight.x() / qAbs(dirRight.y()) * bottomY :  halfW;
    // 限制在视场宽度内
    xLeft  = qBound(-halfW, xLeft,  halfW);
    xRight = qBound(-halfW, xRight, halfW);

    QPolygonF poly;
    poly << QPointF(0, 0);                    // 原点（顶点）
    poly << QPointF(xLeft, bottomY);          // 底边左端
    poly << QPointF(xRight, bottomY);         // 底边右端

    m_scan_sector_item = m_scene->addPolygon(poly, QPen(QColor(255, 120, 0, 200), 2),QBrush(QColor(255, 100, 0, 60)));
    m_scan_sector_item->setZValue(-1);
}

// ---------------------------------------------------------------------------
// 草稿图元管理
// ---------------------------------------------------------------------------
void ZoneCanvas::removeDraftItem()
{
    if (m_draft_item) 
    {
        m_scene->removeItem(m_draft_item);
        delete m_draft_item;
        m_draft_item = nullptr;
    }
}

void ZoneCanvas::clearDraft()
{
    m_draft_points.clear();
    removeDraftItem();
    m_draft_start = QPointF();
    m_drafting = false;
}

// ---------------------------------------------------------------------------
// 点云
// ---------------------------------------------------------------------------
void ZoneCanvas::onLidarData(const QVector<QPointF> &points)
{
    updatePointcloud(points);
    if (m_det) 
    {
        m_det->detect(points);
        bool alarm = m_det->hasAlarm();
        if (alarm != m_lastAlarmState) 
        {
            m_lastAlarmState = alarm;
            rebuildProtectionItems();
        }
    }
}

void ZoneCanvas::onTimeout()
{
    if (!m_drv || m_usingSim) 
    {
        // 生成模拟点云
        QVector<QPointF> pts;
        const int count = 271;
        for (int i = 0; i < count; ++i) 
        {
            double deg = GeoUtils::normalizeAngle(m_scan_start + double(i));
            double dist = m_max_range * 0.8;

            // 模拟障碍物（分布在扫描范围 45°→315° 内不同角度，避开 315°~45° 盲区）
            if (deg > 50 && deg < 80)        dist = 6.0 + 0.5 * qSin(deg * 3.0);
            else if (deg > 90 && deg < 130)  dist = 8.5;
            else if (deg > 140 && deg < 170) dist = 5.0 + 0.8 * qCos(deg * 2.0);
            else if (deg > 190 && deg < 220) dist = 9.0;
            else if (deg > 240 && deg < 280) dist = 7.0;
            else if (deg > 290 && deg < 310) dist = 6.5;

            dist += 0.08 * QRandomGenerator::global()->bounded(100) / 100.0 - 0.04;
            pts.append(GeoUtils::polarToCartesian(dist, deg));
        }
        updatePointcloud(pts);
        if (m_det) 
        {
            m_det->detect(pts);
            bool alarm = m_det->hasAlarm();
            if (alarm != m_lastAlarmState) 
            {
                m_lastAlarmState = alarm;
                rebuildProtectionItems();
            }
        }
    }
}

void ZoneCanvas::updatePointcloud(const QVector<QPointF> &points)
{
    m_last_points = points;

    // 用单个 QPainterPath 绘制所有点，避免大量独立图元
    QPainterPath path;
    const double r = 2.0;  // 点半径
    for (const QPointF &p : points) {
        QPointF sp = radarToScene(p);
        path.addEllipse(sp, r, r);
    }
    m_point_item->setPath(path);

    // 选中点云同样用单个 PathItem
    QPainterPath selPath;
    if (!m_selected_points.isEmpty()) 
    {
        const double sr = 3.5;
        for (const QPointF &p : m_selected_points) 
        {
            QPointF sp = radarToScene(p);
            selPath.addEllipse(sp, sr, sr);
        }
    }
    m_selected_item->setPath(selPath);
}

// ---------------------------------------------------------------------------
// 防护区域图形重建
// ---------------------------------------------------------------------------
void ZoneCanvas::rebuildProtectionItems()
{
    // 移除旧的区域图元
    const auto items = m_scene->items();
    for (auto *item : items) 
    {
        if (item->data(kDataZoneId).toInt() > 0) 
        {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (!m_mgr)
    {
        return;
    }
    // 3 个 OUTPUT 的配色：OUT1 红 / OUT2 黄 / OUT3 青
    const QColor kOutFill[3] = {
        QColor(255,  80,  80, 60),
        QColor(240, 192,  74, 60),
        QColor( 94, 200, 232, 60)
    };
    const QColor kOutEdge[3] = {
        QColor(255,  80,  80, 220),
        QColor(240, 192,  74, 220),
        QColor( 94, 200, 232, 220)
    };

    auto zones=m_mgr->zones();
    for (const ProtectionZone &z:zones) 
    {
        if (!z.enabled)
        {
            continue;
        }
        // 按 Bank 过滤：只显示当前选中 Bank 下 3 个 OUTPUT 绑定的图形
        if (z.bankIndex != m_current_bank)
        {
            continue;
        }
        QVector<QPointF> poly = z.boundaryPoints();
        if (poly.size() < 2)
        {
            continue;
        }

        QPolygonF qpoly;
        for (const QPointF &p : poly)
        {
            qpoly << radarToScene(p);
        }

        // 按 OUTPUT 通道着色；报警时统一变红
        int colorIdx = qBound(0, z.outputIndex - 1, 2);
        QColor fill = z.alarmed ? QColor(255, 0, 0, 70) : kOutFill[colorIdx];
        QColor edge = z.alarmed ? QColor(255, 0, 0, 240) : kOutEdge[colorIdx];

        auto *item = m_scene->addPolygon(qpoly, QPen(edge, 2), QBrush(fill));
        item->setZValue(30);
        item->setData(kDataZoneId, z.id);

        // 顶点标记：仅显示被选中（激活）图形的特征点
        if (m_mgr && z.id == m_mgr->activeZoneId()) 
        {
            if (z.shape == ProtectionZone::Sector) 
            {
                // 扇形只显示 2 个特征点：弧的起始端点和终止端点
                // boundaryPoints 返回 [弧采样点0..segs, 圆心]
                // index 0 = 起始角端点，index segs = 终止角端点
                int end_index = poly.size() - 2; // 倒数第二个是弧最后一个采样点
                int feat_indices[2] = {0, end_index};
                for (int fi = 0; fi < 2; ++fi) 
                {
                    int vi = feat_indices[fi];
                    QPointF sp = radarToScene(poly[vi]);
                    auto *vp = m_scene->addEllipse(sp.x() - 5, sp.y() - 5, 10, 10,QPen(edge, 2), QBrush(Qt::white));
                    vp->setZValue(35);
                    vp->setData(kDataZoneId, z.id);
                    vp->setData(kDataIsVertex, 1);
                    vp->setData(kDataVertIdx, vi);
                }
            } 
            else 
            {
                for (int vi = 0; vi < poly.size(); ++vi) 
                {
                    // 多边形原点(0,0)不可拖拽，不显示特征点
                    if (z.shape == ProtectionZone::Polygon &&
                        std::hypot(poly[vi].x(), poly[vi].y()) < 0.01)
                        continue;

                    QPointF sp = radarToScene(poly[vi]);
                    auto *vp = m_scene->addEllipse(sp.x() - 4, sp.y() - 4, 8, 8,
                                                   QPen(edge, 1), QBrush(Qt::white));
                    vp->setZValue(35);
                    vp->setData(kDataZoneId, z.id);
                    vp->setData(kDataIsVertex, 1);
                    vp->setData(kDataVertIdx, vi);
                }
            }
        }

        // 区域标签
        QPointF c = radarToScene(z.centroid());
        auto *label = m_scene->addSimpleText(QString("B%1-O%2").arg(z.bankIndex).arg(z.outputIndex));
        label->setPos(c.x() + 5, c.y() - 10);
        label->setZValue(36);
        label->setBrush(edge);
        label->setData(kDataZoneId, z.id);
    }
}

void ZoneCanvas::rebuildSingleZone(int zoneId)
{
    // 移除该区域的旧图元
    const auto items = m_scene->items();
    for (auto *item : items) 
    {
        if (item->data(kDataZoneId).toInt() == zoneId) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (!m_mgr)
    {
        return;
    } 
    const ProtectionZone *zp = m_mgr->zone(zoneId);
    if (!zp || !zp->enabled)
    {
        return;
    }  

    // 非当前 Bank 的区域不显示图元
    if (zp->bankIndex != m_current_bank)
    {
        return;
    }
        

    const ProtectionZone &z = *zp;
    QVector<QPointF> poly = z.boundaryPoints();
    if (poly.size() < 2)
    {
        return;
    }  

    QPolygonF qpoly;
    for (const QPointF &p : poly)
    {
        qpoly << radarToScene(p);
    }
    // 按 OUTPUT 通道着色；报警时统一变红
    const QColor kOutFill[3] = {
        QColor(255,  80,  80, 60),
        QColor(240, 192,  74, 60),
        QColor( 94, 200, 232, 60)
    };
    const QColor kOutEdge[3] = {
        QColor(255,  80,  80, 220),
        QColor(240, 192,  74, 220),
        QColor( 94, 200, 232, 220)
    };
    int colorIdx = qBound(0, z.outputIndex - 1, 2);
    QColor fill = z.alarmed ? QColor(255, 0, 0, 70) : kOutFill[colorIdx];
    QColor edge = z.alarmed ? QColor(255, 0, 0, 240) : kOutEdge[colorIdx];

    auto *item = m_scene->addPolygon(qpoly, QPen(edge, 2), QBrush(fill));
    item->setZValue(30);
    item->setData(kDataZoneId, z.id);

    // 顶点标记：仅显示被选中（激活）图形的特征点
    if (m_mgr && z.id == m_mgr->activeZoneId()) 
    {
        if (z.shape == ProtectionZone::Sector) 
        {
            // 扇形只显示 2 个特征点：弧的起始端点和终止端点
            int end_index = poly.size() - 2;
            int feat_indices[2] = {0, end_index};
            for (int fi = 0; fi < 2; ++fi) 
            {
                int vi = feat_indices[fi];
                QPointF sp = radarToScene(poly[vi]);
                auto *vp = m_scene->addEllipse(sp.x() - 5, sp.y() - 5, 10, 10,QPen(edge, 2), QBrush(Qt::white));
                vp->setZValue(35);
                vp->setData(kDataZoneId, z.id);
                vp->setData(kDataIsVertex, 1);
                vp->setData(kDataVertIdx, vi);
            }
        } 
        else 
        {
            for (int vi = 0; vi < poly.size(); ++vi) 
            {
                // 多边形首尾点是原点(0,0)，不可拖拽，不显示特征点
                if (z.shape == ProtectionZone::Polygon &&
                    (vi == 0 || vi == poly.size() - 1))
                    continue;

                QPointF sp = radarToScene(poly[vi]);
                auto *vp = m_scene->addEllipse(sp.x() - 4, sp.y() - 4, 8, 8,
                                               QPen(edge, 1), QBrush(Qt::white));
                vp->setZValue(35);
                vp->setData(kDataZoneId, z.id);
                vp->setData(kDataIsVertex, 1);
                vp->setData(kDataVertIdx, vi);
            }
        }
    }

    // 区域标签
    QPointF c = radarToScene(z.centroid());
    auto *label = m_scene->addSimpleText(QString("B%1-O%2").arg(z.bankIndex).arg(z.outputIndex));
    label->setPos(c.x() + 5, c.y() - 10);
    label->setZValue(36);
    label->setBrush(edge);
    label->setData(kDataZoneId, z.id);
}

void ZoneCanvas::onZonesChanged()
{
    rebuildProtectionItems();
}

// ---------------------------------------------------------------------------
// 顶点拖拽：根据新顶点位置更新区域数据并刷新图元
// ---------------------------------------------------------------------------
void ZoneCanvas::applyVertexDrag(int zoneId, int vertexIndex, const QPointF &radarPos)
{
    ProtectionZone *z = m_mgr->zone(zoneId);
    if (!z)
        return;

    // 所有形状拖拽点统一限制到上半区（雷达 y>=0）
    QPointF clampedPos = clampToUpperHalf(radarPos);

    QVector<QPointF> poly = z->boundaryPoints();
    // 扇形拖拽用 m_drag_sector_end 判断端点，不依赖动态 poly.size()
    if (z->shape != ProtectionZone::Sector)
    {
        if (vertexIndex < 0 || vertexIndex >= poly.size())
            return;
    }

    switch (z->shape) {
    case ProtectionZone::Polygon: {
        // 多边形：原点 (0,0) 作为首尾点，不能拖拽
        // 按坐标判断：该顶点是否为原点
        if (vertexIndex >= 0 && vertexIndex < poly.size() &&
            std::hypot(poly[vertexIndex].x(), poly[vertexIndex].y()) < 0.01)
            return;

        // 使用拖拽起始时的顶点快照，只更新被拖拽的顶点，
        // 不对其他顶点重新排序，避免拖动一个点时其他点位置跳动
        if (m_drag_init_points.isEmpty())
            return;

        // 在初始快照中找到与拖拽点对应的顶点：
        // boundaryPoints() 对 Polygon 直接返回 points，索引一致
        if (vertexIndex >= m_drag_init_points.size())
            return;

        QVector<QPointF> newPoints = m_drag_init_points;
        newPoints[vertexIndex] = clampedPos;
        z->points = newPoints;
        break;
    }
    case ProtectionZone::Circle: 
    {
        // 圆形以原点为圆心，boundaryPoints 是 64 个点。
        // 拖拽任意点 → 改变半径
        double newR = std::hypot(clampedPos.x(), clampedPos.y());
        z->radius = qMax(0.1, newR);
        // 重新生成 points
        z->points.clear();
        const int segs = 64;
        for (int i = 0; i < segs; ++i) {
            double a = 360.0 * double(i) / double(segs);
            z->points.append(GeoUtils::polarToCartesian(z->radius, a));
        }
        break;
    }
    case ProtectionZone::Rectangle: 
    {
        // 矩形 4 个角点（addRectangle 顺序，angle=0）：
        //   points[0]=左下(y=0)  points[1]=右下(y=0)  points[2]=右上(y>0)  points[3]=左上(y>0)
        // 底边（points[0], points[1]）始终在 y=0 轴上，顶边在 y>0（上半区）。
        // 拖顶边角点(2/3) → 改变宽、高、中心
        // 拖底边角点(0/1) → 只改宽（高不变，底边保持 y=0）
        // 左右边不能跨过 x=0 轴：拖右边时 rightX>=0，拖左边时 leftX<=0
        if (vertexIndex < 0 || vertexIndex >= 4)
            return;
        if (m_drag_init_points.size() < 4)
            return;

        // 初始 4 个角点快照
        QPointF p0 = m_drag_init_points[0];  // 左下 (y=0)
        QPointF p1 = m_drag_init_points[1];  // 右下 (y=0)
        QPointF p2 = m_drag_init_points[2];  // 右上 (y>0)
        QPointF p3 = m_drag_init_points[3];  // 左上 (y>0)

        QPointF mouse = clampedPos;  // y>=0（上半区）

        double newCx, newCy, newW, newH;

        if (vertexIndex == 2)
        {
            // 拖右上角：右 x = mouse.x，顶边 y = mouse.y
            double leftX   = p3.x();
            double rightX  = qMax(mouse.x(), 0.0);  // 右边不能跨过 x=0
            double topY    = mouse.y();   // >= 0
            double bottomY = 0.0;
            newW = rightX - leftX;
            newH = topY - bottomY;
            newCx = (leftX + rightX) / 2.0;
            newCy = (topY + bottomY) / 2.0;  // >= 0
        }
        else if (vertexIndex == 3)
        {
            // 拖左上角：左 x = mouse.x，顶边 y = mouse.y
            double leftX   = qMin(mouse.x(), 0.0);  // 左边不能跨过 x=0
            double rightX  = p2.x();
            double topY    = mouse.y();
            double bottomY = 0.0;
            newW = rightX - leftX;
            newH = topY - bottomY;
            newCx = (leftX + rightX) / 2.0;
            newCy = (topY + bottomY) / 2.0;
        }
        else if (vertexIndex == 0)
        {
            // 拖左下角：只改宽，高不变
            double leftX   = qMin(mouse.x(), 0.0);  // 左边不能跨过 x=0
            double rightX  = p1.x();
            double topY    = p3.y();       // 顶边 y 保持不变（>0）
            double bottomY = 0.0;
            newW = rightX - leftX;
            newH = topY - bottomY;
            newCx = (leftX + rightX) / 2.0;
            newCy = (topY + bottomY) / 2.0;
        }
        else
        {
            // vertexIndex == 1，拖右下角：只改宽，高不变
            double leftX   = p0.x();
            double rightX  = qMax(mouse.x(), 0.0);  // 右边不能跨过 x=0
            double topY    = p2.y();       // 顶边 y 保持不变（>0）
            double bottomY = 0.0;
            newW = rightX - leftX;
            newH = topY - bottomY;
            newCx = (leftX + rightX) / 2.0;
            newCy = (topY + bottomY) / 2.0;
        }

        if (newW < 0.1 || newH < 0.1)
            return;

        z->width  = newW;
        z->height = newH;
        z->angle  = 0.0;
        // 重新计算 4 个角点（底边在 y=0，顶边在 y>0）
        double hw = newW / 2.0, hh = newH / 2.0;
        z->points.clear();
        z->points << QPointF(newCx - hw, 0.0)          // 左下 (y=0)
                  << QPointF(newCx + hw, 0.0)          // 右下 (y=0)
                  << QPointF(newCx + hw, newCy + hh)   // 右上 (y>0)
                  << QPointF(newCx - hw, newCy + hh);  // 左上 (y>0)
        break;
    }
    case ProtectionZone::Sector: 
    {
        // 扇形 boundaryPoints：[弧采样点0..segs, 圆心(0,0)]
        // 拖拽用 m_drag_sector_end 判断起始/终止端点，不依赖动态 poly 大小
        // 始终在上半区（y>=0）绘制：用两个端点的最小夹角，弧不经过下半区
        // 上半区角度范围：[90°, 270°]，跨 180° 连续 180°
        if (m_drag_sector_end < 0)
            return;

        double newR = std::hypot(clampedPos.x(), clampedPos.y());
        double newAngle = GeoUtils::cartesianToPolar(clampedPos).second;

        // 固定端点角度
        double fixedAngle = (m_drag_sector_end == 0) ? z->endAngle : z->startAngle;

        // 计算两角度之间的最小夹角（≤180°）
        double diff = GeoUtils::normalizeAngle(newAngle - fixedAngle);
        if (diff > 180.0)
            diff = 360.0 - diff;
        diff = qMax(3.0, diff);

        // 尝试正向：fixedAngle + diff
        double a2 = GeoUtils::normalizeAngle(fixedAngle + diff);
        // 判断 a2 是否在上半区：上半区是 [90°, 270°]
        auto inUpperHalf = [](double a) -> bool {
            return a >= 90.0 && a <= 270.0;
        };
        // 如果正向 a2 不在上半区，尝试反向
        if (!inUpperHalf(a2))
        {
            a2 = GeoUtils::normalizeAngle(fixedAngle - diff);
        }

        // 确保 fixedAngle 和 a2 都在上半区
        auto clampToHalf = [](double a) -> double {
            if (a < 90.0 || a > 270.0)
            {
                // 截断到最近的边界
                if (a < 90.0) return 90.0;
                return 270.0;
            }
            return a;
        };
        double a1 = clampToHalf(fixedAngle);
        a2 = clampToHalf(a2);

        // 确定 startAngle/endAngle：弧从 startAngle 逆时针到 endAngle，不经过下半区
        // 上半区跨 180°（90°→180°→270°），连续 180°，无需特殊映射
        double startMap = std::min(a1, a2);
        double endMap = std::max(a1, a2);
        z->startAngle = GeoUtils::normalizeAngle(startMap);
        z->endAngle = GeoUtils::normalizeAngle(endMap);

        z->radius = qMax(0.1, newR);

        // 同步更新 z->points：只放起始端点和终止端点（扇形的 2 个特征点）
        z->points.clear();
        z->points << GeoUtils::polarToCartesian(z->radius, z->startAngle);
        z->points << GeoUtils::polarToCartesian(z->radius, z->endAngle);
        break;
    }
    }
    // 仅重建被拖拽区域的图元（不走 updateZone → zonesChanged 全量重建链路）
    rebuildSingleZone(zoneId);
}

// ---------------------------------------------------------------------------
// 鼠标交互
// ---------------------------------------------------------------------------
void ZoneCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) 
    {
        // 右键擦除激活区域
        emit eraseRequested();
        return;
    }

    if (event->button() != Qt::LeftButton) 
    {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QPointF sp = mapToScene(event->pos());
    QPointF rp = sceneToRadar(sp);

    // Ctrl + 左键：框选点云
    if (event->modifiers() & Qt::ControlModifier) 
    {
        m_selecting = true;
        m_select_start = sp;
        if (m_select_rect) 
        {
            m_scene->removeItem(m_select_rect);
            delete m_select_rect;
        }
        m_select_rect = m_scene->addRect(sp.x(), sp.y(), 0, 0,
                                        QPen(QColor(255, 200, 0), 1, Qt::DashLine),
                                        QBrush(QColor(255, 200, 0, 25)));
        m_select_rect->setZValue(60);
        return;
    }

    // 选择模式下点击已有区域 → 激活；点击顶点 → 启动拖拽
    if (m_mode == DrawMode::Select && m_mgr) 
    {
        auto item = itemAt(event->pos());
        if (item && item->data(kDataZoneId).toInt() > 0) 
        {
            int zoneId = item->data(kDataZoneId).toInt();
            // 优先检测顶点：点击顶点 → 进入拖拽
            if (item->data(kDataIsVertex).toInt() > 0) 
            {
                m_dragging_vertex = true;
                m_drag_zone_id = zoneId;
                m_drag_vertex_index = item->data(kDataVertIdx).toInt();
                m_drag_sector_end = -1;  // 默认非扇形

                // 扇形特征点：根据 kDataVertIdx 判断起始/终止端点
                // 起始端点 vi=0；终止端点 vi 用 -1 标记（不依赖动态 poly 大小）
                if (const ProtectionZone *z0 = m_mgr->zone(zoneId))
                {
                    if (z0->shape == ProtectionZone::Sector)
                    {
                        m_drag_sector_end = (m_drag_vertex_index == 0) ? 0 : 1;
                    }
                }

                m_mgr->setActiveZoneId(zoneId);
                // 记录拖拽起始状态
                m_drag_start_pos = sceneToRadar(mapToScene(event->pos()));
                if (const ProtectionZone *z0 = m_mgr->zone(zoneId))
                {
                    m_drag_init_points = z0->points;
                }
                setCursor(Qt::ClosedHandCursor);
                // 拖拽期间暂停点云刷新，避免与全量重建竞争
                //m_simTimer.stop();
                return;
            }
            // 点击区域内部 → 仅激活
            m_mgr->setActiveZoneId(zoneId);
            return;
        }
    }

    // 绘制模式
    switch (m_mode) 
    {
    case DrawMode::Polygon:
    case DrawMode::Pen: 
    {
        // 限制到上半区（雷达 y>=0）
        rp = clampToUpperHalf(rp);
        if (!withinScanRange(rp))
        {
            break;
        }
        // 过滤连续重复点
        if (!m_draft_points.isEmpty()) 
        {
            QPointF last = m_draft_points.last();
            if (std::hypot(rp.x() - last.x(), rp.y() - last.y()) < 0.05)
            {
                break;
            }     
        }
        m_draft_points.append(rp);
        removeDraftItem();

        // 预览：原点作为首尾点，中间顶点按点击顺序连接
        QPolygonF poly;
        if (m_mode == DrawMode::Polygon)
            poly << radarToScene(QPointF(0, 0));  // 原点作为首点
        for (const QPointF &p : m_draft_points)
            poly << radarToScene(p);
        if (m_mode == DrawMode::Polygon)
            poly << radarToScene(QPointF(0, 0));  // 原点作为尾点

        auto *polyItem = m_scene->addPolygon(poly, QPen(QColor(230, 120, 40), 2),
                                              QBrush(QColor(230, 120, 40, 40)));
        polyItem->setZValue(40);
        m_draft_item = polyItem;
        break;
    }
    case DrawMode::Rectangle:
    case DrawMode::Circle:
        // 限制到上半区（场景 y<=0，Qt 场景 y 向下为正，上半区 y 为负）
        m_draft_start = QPointF(sp.x(), qMin(sp.y(), 0.0));
        m_drafting = true;
        break;
    case DrawMode::Sector:
    {
        // 扇形圆心固定在原点：按下时记录起始角度和初始半径
        // 限制到上半区（雷达 y>=0），角度自然在 90°~270°
        QPointF clamped = clampToUpperHalf(rp);
        m_draft_sectorS_start_angle = GeoUtils::cartesianToPolar(clamped).second;
        m_draft_sector_radius = std::hypot(clamped.x(), clamped.y());
        m_drafting = true;
        break;
    }
    default:
        break;
    }
    QGraphicsView::mousePressEvent(event);
}

void ZoneCanvas::mouseMoveEvent(QMouseEvent *event)
{
    QPointF sp = mapToScene(event->pos());
    QPointF rp = sceneToRadar(sp);

    // 更新光标坐标
    m_cursor_pos = rp;
    auto pr = GeoUtils::cartesianToPolar(rp);
    m_cursor_range = pr.first;
    m_cursor_angle = pr.second;
    emit cursorCoordinateChanged(rp.x(), rp.y(), m_cursor_angle, m_cursor_range);

    // 顶点拖拽：实时更新区域
    if (m_dragging_vertex && m_mgr) 
    {
        applyVertexDrag(m_drag_zone_id, m_drag_vertex_index, rp);
        return;
    }

    // 选择模式下：悬停在顶点上时显示手型光标
    if (m_mode == DrawMode::Select) 
    {
        auto item = itemAt(event->pos());
        if (item && item->data(kDataIsVertex).toInt() > 0)
        {
            setCursor(Qt::OpenHandCursor);
        } 
        else
        {
            setCursor(Qt::ArrowCursor);
        }   
    }

    // 框选预览
    if (m_selecting && m_select_rect) 
    {
        double x = qMin(m_select_start.x(), sp.x());
        double y = qMin(m_select_start.y(), sp.y());
        double w = qAbs(sp.x() - m_select_start.x());
        double h = qAbs(sp.y() - m_select_start.y());
        static_cast<QGraphicsRectItem *>(m_select_rect)->setRect(x, y, w, h);
        return;
    }

    // 矩形/圆形/扇形预览
    if (m_drafting) 
    {
        removeDraftItem();
        if (m_mode == DrawMode::Rectangle) 
        {
            // 限制到上半区（场景 y<=0），底边与 y=0 重合
            // 矩形关于 x=0 对称：宽度 = 两点 x 距离，center.x = 0
            double topY = qMin(m_draft_start.y(), sp.y());  // 顶边 y（<=0，更远的一侧）
            double bottomY = 0.0;  // 底边固定在 y=0
            double w = qAbs(sp.x() - m_draft_start.x());   // 宽度 = 两点 x 距离
            double h = bottomY - topY;  // 高度（正数）
            double leftX = -w / 2.0;    // 矩形左边 = -w/2（关于 x=0 对称）
            auto *rectItem = m_scene->addRect(leftX, topY, w, h,
                                              QPen(QColor(230, 120, 40), 2),
                                              QBrush(QColor(230, 120, 40, 40)));
            rectItem->setZValue(40);
            m_draft_item = rectItem;
        } 
        else if (m_mode == DrawMode::Circle) 
        {
            double dx = sp.x() - m_draft_start.x();
            double dy = sp.y() - m_draft_start.y();
            double r = qSqrt(dx * dx + dy * dy);
            auto *ellItem = m_scene->addEllipse(m_draft_start.x() - r,
                                                m_draft_start.y() - r,
                                                r * 2, r * 2,
                                                QPen(QColor(230, 120, 40), 2),
                                                QBrush(QColor(230, 120, 40, 40)));
            ellItem->setZValue(40);
            m_draft_item = ellItem;
        } 
        else if (m_mode == DrawMode::Sector) 
        {
            //  扇形圆心固定在原点(0,0)：
            //  起始角 = 按下时的角度，终止角 = 当前光标角度
            //  半径 = 当前光标到原点的距离
            //  限制到上半区（雷达 y>=0），上半区角度 [90°, 270°]
            QPointF clamped = clampToUpperHalf(rp);
            double curAng = GeoUtils::cartesianToPolar(clamped).second;
            double r = std::hypot(clamped.x(), clamped.y());
            m_draft_sector_radius = r;

            // 用最小夹角确定 startAng/endAng，确保弧在上半区
            double startAng = m_draft_sectorS_start_angle;
            double diff = GeoUtils::normalizeAngle(curAng - startAng);
            if (diff > 180.0)
                diff = 360.0 - diff;
            diff = qMax(3.0, diff);

            // 上半区判断：角度在 [90°, 270°]
            auto inUpperHalf = [](double a) -> bool {
                return a >= 90.0 && a <= 270.0;
            };
            // 尝试正向，如果 endAng 不在上半区则反向
            double endAng = GeoUtils::normalizeAngle(startAng + diff);
            if (!inUpperHalf(endAng))
                endAng = GeoUtils::normalizeAngle(startAng - diff);

            // 确保两个角度都在上半区
            auto clampToHalf = [](double a) -> double {
                if (a < 90.0 || a > 270.0)
                    return (a < 90.0) ? 90.0 : 270.0;
                return a;
            };
            startAng = clampToHalf(startAng);
            endAng = clampToHalf(endAng);

            // 上半区 [90°, 270°] 跨 180° 连续，直接排序
            startAng = GeoUtils::normalizeAngle(std::min(startAng, endAng));
            endAng = GeoUtils::normalizeAngle(std::max(startAng, endAng));
            double sweep = GeoUtils::normalizeAngle(endAng - startAng);
            if (sweep < 3.0) sweep = 3.0;
            if (sweep > 180.0) sweep = 180.0;

            QPolygonF poly;
            poly << QPointF(0, 0); // 圆心（场景原点）
            const int segs = qMax(8, int(sweep / 3.0) + 1);
            for (int i = 0; i <= segs; ++i) 
            {
                double a = startAng + sweep * double(i) / double(segs);
                QPointF p = GeoUtils::polarToCartesian(r, a);
                poly << radarToScene(p);
            }

            auto *secItem = m_scene->addPolygon(poly, QPen(QColor(230, 120, 40), 2),
                                                QBrush(QColor(230, 120, 40, 40)));
            secItem->setZValue(40);
            m_draft_item = secItem;
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ZoneCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    // 完成顶点拖拽
    if (m_dragging_vertex) {
        // 提交最终更新到 ProtectionManager（触发持久化/检测刷新）
        if (m_mgr && m_drag_zone_id >= 0) {
            const ProtectionZone *z = m_mgr->zone(m_drag_zone_id);
            if (z)
                m_mgr->updateZone(m_drag_zone_id, *z);
        }
        m_dragging_vertex = false;
        m_drag_zone_id = -1;
        m_drag_vertex_index = -1;
        m_drag_sector_end = -1;
        setCursor(Qt::ArrowCursor);
        // 恢复点云刷新定时器
        //m_simTimer.start(50);
        return;
    }

    // 完成框选
    if (m_selecting) {
        m_selecting = false;

        QPointF sp = mapToScene(event->pos());
        double x = qMin(m_select_start.x(), sp.x());
        double y = qMin(m_select_start.y(), sp.y());
        double w = qAbs(sp.x() - m_select_start.x());
        double h = qAbs(sp.y() - m_select_start.y());

        if (m_select_rect) {
            m_scene->removeItem(m_select_rect);
            delete m_select_rect;
            m_select_rect = nullptr;
        }

        m_selected_points.clear();
        for (const QPointF &p : m_last_points) {
            QPointF sp2 = radarToScene(p);
            if (sp2.x() >= x && sp2.x() <= x + w &&
                sp2.y() >= y && sp2.y() <= y + h)
                m_selected_points.append(p);
        }

        // 高亮重绘：用单个 PathItem 绘制
        QPainterPath selPath;
        for (const QPointF &p : m_selected_points) 
        {
            QPointF sp2 = radarToScene(p);
            selPath.addEllipse(sp2, 3.5, 3.5);
        }
        m_selected_item->setPath(selPath);

        emit pointCloudSelected(m_selected_points.size());
        return;
    }

    // 完成矩形/圆形/扇形绘制
    if (m_drafting) 
    {
        m_drafting = false;
        QPointF sp = mapToScene(event->pos());

        if (m_mode == DrawMode::Rectangle) 
        {
            // 限制到上半区（场景 y<=0），底边与 y=0 重合
            // 矩形关于 x=0 对称：宽度 = 两点 x 距离，center.x = 0
            double curY = qMin(sp.y(), 0.0);
            double sceneTopY = qMin(m_draft_start.y(), curY);  // 场景顶边 y（<=0）
            double radarTopY = -sceneTopY / m_scale;  // 雷达顶边 y（>=0，翻转 y 轴）
            double w = qAbs(sp.x() - m_draft_start.x()) / m_scale;  // 宽度 = 两点 x 距离
            double h = radarTopY;  // 高度 = 顶边到 y=0 的距离
            if (w > 0.05 && h > 0.05)
            {
                // 底边在雷达 y=0，顶边在雷达 y=h，center.x = 0（关于 x=0 对称），center.y = h/2
                emit rectangleDrawn(QPointF(0.0, h / 2), w, h);
            }
        } 
        else if (m_mode == DrawMode::Circle) 
        {
            double dx = sp.x() - m_draft_start.x();
            double dy = sp.y() - m_draft_start.y();
            double r = qSqrt(dx * dx + dy * dy) / m_scale;
            if (r > 0.05)
            {
                emit circleDrawn(sceneToRadar(m_draft_start), r);
            } 
        } 
        else if (m_mode == DrawMode::Sector) 
        {
            // 扇形圆心在原点：用最小夹角确定起止角，确保弧在上半区
            QPointF rp = sceneToRadar(sp);
            QPointF clamped = clampToUpperHalf(rp);
            double curAng = GeoUtils::cartesianToPolar(clamped).second;
            double r = m_draft_sector_radius;
            if (r > 0.05) 
            {
                double startAng = m_draft_sectorS_start_angle;
                double diff = GeoUtils::normalizeAngle(curAng - startAng);
                if (diff > 180.0)
                    diff = 360.0 - diff;
                diff = qMax(3.0, diff);

                // 上半区判断：角度在 [90°, 270°]
                auto inUpperHalf = [](double a) -> bool {
                    return a >= 90.0 && a <= 270.0;
                };
                double endAng = GeoUtils::normalizeAngle(startAng + diff);
                if (!inUpperHalf(endAng))
                    endAng = GeoUtils::normalizeAngle(startAng - diff);

                auto clampToHalf = [](double a) -> double {
                    if (a < 90.0 || a > 270.0)
                        return (a < 90.0) ? 90.0 : 270.0;
                    return a;
                };
                startAng = clampToHalf(startAng);
                endAng = clampToHalf(endAng);

                // 上半区 [90°, 270°] 跨 180° 连续，直接排序
                startAng = GeoUtils::normalizeAngle(std::min(startAng, endAng));
                endAng = GeoUtils::normalizeAngle(std::max(startAng, endAng));
                emit sectorDrawn(startAng, endAng, r);
            }
        }

        clearDraft();
        setDrawMode(DrawMode::Select);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void ZoneCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event)

    if ((m_mode == DrawMode::Polygon || m_mode == DrawMode::Pen) && m_draft_points.size() >= 2) 
    {
        // 双击的第二次 press 会额外添加一个重复顶点，移除末尾连续重复点
        while (m_draft_points.size() >= 2) 
        {
            QPointF last = m_draft_points.last();
            QPointF prev = m_draft_points[m_draft_points.size() - 2];
            if (std::hypot(last.x() - prev.x(), last.y() - prev.y()) < 0.01)
            {
                m_draft_points.removeLast();
            } 
            else
            {
                break;
            }   
        }

        if (m_mode == DrawMode::Polygon && m_draft_points.size() >= 2)
        {
            // 多边形：原点作为首尾点，中间顶点按点击顺序连接
            QVector<QPointF> finalPoints;
            finalPoints << QPointF(0, 0);
            for (const QPointF &p : m_draft_points)
                finalPoints << p;
            finalPoints << QPointF(0, 0);

            if (GeoUtils::polygonIsSelfIntersecting(finalPoints))
            {
                // 自交：按相对于质心的极角排序，确保不自交（凸多边形）
                QPointF centroid(0, 0);
                for (const QPointF &p : finalPoints)
                    centroid += p;
                centroid /= finalPoints.size();

                std::sort(finalPoints.begin(), finalPoints.end(),
                          [&centroid](const QPointF &a, const QPointF &b) {
                              double angA = std::atan2(a.y() - centroid.y(), a.x() - centroid.x());
                              double angB = std::atan2(b.y() - centroid.y(), b.x() - centroid.x());
                              return angA < angB;
                          });
            }

            emit polygonDrawn(finalPoints);
        }
        else if (m_mode == DrawMode::Pen && m_draft_points.size() >= 2)
        {
            // 铅笔：按点击顺序的折线
            emit polylineDrawn(m_draft_points);
        }
    }

    clearDraft();
    setDrawMode(DrawMode::Select);
}

void ZoneCanvas::wheelEvent(QWheelEvent *event)
{
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_zoom = qBound(0.2, m_zoom * factor, 10.0);
    scale(factor, factor);
}

void ZoneCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) 
    {
        clearDraft();
        setDrawMode(DrawMode::Select);
        return;
    }
    if (event->key() == Qt::Key_Delete && m_mgr && m_mgr->activeZoneId() >= 0) 
    {
        m_mgr->removeZone(m_mgr->activeZoneId());
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void ZoneCanvas::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);

    // 工具栏保持在右上角
    if (m_btn_select) 
    {
        auto *bar = m_btn_select->parentWidget();
        if (bar)
        {
            bar->move(width() - bar->width() - 4, 4);
        }  
    }
}
