#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QTimer>

class ProtectionManager;
class ProtectionDetector;
class LidarDriver;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QToolButton;
class QFrame;

/**
 * @brief 区域配置画布（基于 QGraphicsView + QGraphicsScene）
 *
 * 功能：
 *  - 直角坐标网格 + 距离/角度刻度
 *  - 270° 雷达扫描范围（灰色盲区扇形）
 *  - 雷达点云实时显示（绿色点）
 *  - Ctrl + 左键拖拽框选点云（高亮显示）
 *  - 绘制防护区域：多边形 / 矩形 / 圆形 / 扇形 / 铅笔自由绘制
 *  - 已配置区域实时显示（报警时变红）
 *  - 滚轮缩放
 *  - 右侧垂直工具栏
 */
class ZoneCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    enum class DrawMode
    {
        Select,    ///< 选择 / 拖动
        Polygon,   ///< 多边形
        Circle,    ///< 圆形
        Rectangle, ///< 矩形
        Sector,    ///< 扇形
        Pen        ///< 铅笔（自由绘制折线）
    };

    explicit ZoneCanvas(QWidget *parent = nullptr);
    ~ZoneCanvas() override;

    // --- 模块关联 ---
    void setProtectionManager(ProtectionManager *mgr);
    void setProtectionDetector(ProtectionDetector *det);
    void setLidarDriver(LidarDriver *drv);

    // --- 绘制模式 ---
    void setDrawMode(DrawMode mode);
    DrawMode drawMode() const { return m_mode; }

    // --- 参数 ---
    void setViewRange(double halfWidthMeters, double halfHeightMeters);
    void setScanRange(double startDeg, double endDeg);
    void setCartesianMode(bool cartesian);
    void setGridVisible(bool on);
    bool isGridVisible() const { return m_grid_visible; }

    // --- 区域显示（兼容旧接口） ---
    void setActiveZone(const QVector<QPointF> &points,const QColor &edge, const QColor &fill);
    void clearActiveZone();
    void setZone(const QVector<QPointF> &points, const QColor &edge, const QColor &fill);

    // --- 目标 Bank/Output（新建区域归属） ---
    void setTargetBank(int bank, int output);

    // --- 当前显示的 Bank（切换 Bank 后只显示该 Bank 下 3 个 OUTPUT 的图形） ---
    void setCurrentBank(int bank);
    int  currentBank() const { return m_current_bank; }

    // --- 光标坐标 ---
    QPointF cursorScenePos() const { return m_cursor_pos; }
    double  cursorAngle() const { return m_cursor_angle; }
    double  cursorRange() const { return m_cursor_range; }

    // --- 点云框选 ---
    int selectedPointCount() const { return m_selected_points.size(); }

signals:
    void cursorCoordinateChanged(double x, double y, double angle, double range);
    void polygonDrawn(const QVector<QPointF> &points);
    void rectangleDrawn(const QPointF &center, double w, double h);
    void circleDrawn(const QPointF &center, double radius);
    void sectorDrawn(double startAngle, double endAngle, double radius);
    void polylineDrawn(const QVector<QPointF> &points);
    void eraseRequested();
    void pointCloudSelected(int count);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onLidarData(const QVector<QPointF> &points);
    void onTimeout();
    void onZonesChanged();

private:
    void initScene();
    void buildToolBar();
    void drawGrid();
    void drawScanSector();
    void rebuildProtectionItems();
    /// 仅重建指定区域（zoneId）的图元，不全量重建
    void rebuildSingleZone(int zoneId);
    void updatePointcloud(const QVector<QPointF> &points);
    void clearDraft();
    void removeDraftItem();

    /// 顶点拖拽：根据新顶点位置更新区域数据并刷新图元
    void applyVertexDrag(int zoneId, int vertexIndex, const QPointF &radarPos);

    QPointF sceneToRadar(const QPointF &scenePos) const;
    QPointF radarToScene(const QPointF &radarPos) const;

    bool withinScanRange(const QPointF &radarPt) const;

    /// 将雷达坐标点限制到 y>=0 的上半区（扫描区），y<0 时截断到 y=0
    QPointF clampToUpperHalf(const QPointF &radarPt) const;
private:
    // 场景
    QGraphicsScene *m_scene = nullptr;

    // 显示参数
    double m_half_width = 7.5;
    double m_half_height = 5.0;
    double m_scale = 50.0;        ///< 像素/米
    double m_zoom = 1.0;
    bool m_cartesian = true;
    bool m_grid_visible = true;

    double m_scan_start = 45.0;
    double m_scan_end = 315.0;
    double m_max_range = 20.0;

    DrawMode m_mode = DrawMode::Select;

    // 点云（用单个 PathItem 绘制所有点，避免大量独立图元）
    QGraphicsPathItem *m_point_item = nullptr;
    QGraphicsPathItem *m_selected_item = nullptr;
    QVector<QPointF>    m_last_points;
    QVector<QPointF>    m_selected_points;

    // 网格图元
    QList<QGraphicsItem *> m_grid_items;
    QGraphicsItem *m_scan_sector_item = nullptr;

    // 草稿
    QVector<QPointF> m_draft_points;       ///< 多边形/铅笔顶点（雷达坐标）
    QGraphicsItem *m_draft_item{nullptr};
    QPointF m_draft_start;                 ///< 矩形/圆形起点（场景坐标）
    double m_draft_sectorS_start_angle = 0.0; ///< 扇形起始角度（度，雷达坐标）
    double m_draft_sector_radius = 0.0;     ///< 扇形半径（米，雷达坐标）
    bool m_drafting = false;

    // 框选
    bool m_selecting = false;
    QPointF m_select_start;
    QGraphicsItem *m_select_rect = nullptr;

    // 顶点拖拽
    bool m_dragging_vertex = false;
    int  m_drag_zone_id = -1;
    int  m_drag_vertex_index = -1;
    QPointF m_drag_start_pos;         ///< 拖拽起始鼠标位置（雷达坐标）
    QVector<QPointF> m_drag_init_points; ///< 拖拽起始时区域顶点快照
    ///< 扇形拖拽专用：0=起始端点，1=终止端点，-1=非扇形拖拽
    int  m_drag_sector_end = -1;

    // 激活区域显示（兼容旧接口）
    QVector<QPointF> m_activePoints;
    QColor m_activeEdge, m_activeFill;
    QGraphicsItem *m_activeItem = nullptr;

    // 光标
    QPointF m_cursor_pos;
    double m_cursor_angle = 0.0;
    double m_cursor_range = 0.0;

    // 目标 Bank/Output（新建区域归属）
    int m_targetBank = 0;
    int m_targetOutput = 1;

    // 当前显示的 Bank（切换 Bank 后画布只显示该 Bank 的图形）
    int m_current_bank = 0;

    // 模块
    ProtectionManager  *m_mgr{nullptr};
    ProtectionDetector *m_det{nullptr};
    LidarDriver        *m_drv{nullptr};

    // 报警状态快照（仅状态变化时才重建区域图元）
    bool m_lastAlarmState = false;

    // 模拟数据定时器
    QTimer m_simTimer;
    bool m_usingSim = true;

    // 工具栏按钮
    QToolButton *m_btn_select{nullptr};
    QToolButton *m_btn_rect{nullptr};
    //QToolButton *m_btn_circle{nullptr};
    QToolButton *m_btn_poly{nullptr};
    QToolButton *m_btn_sector{nullptr};
    QToolButton *m_btn_pen{nullptr};
    QToolButton *m_btn_erase{nullptr};
};
