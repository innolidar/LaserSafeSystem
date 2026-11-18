#include "MainWindow.h"
#include "ZoneCanvas.h"
#include "ProtectionManager.h"
#include "ProtectionDetector.h"
#include "LidarDriver.h"
#include "BankConfigPanel.h"
#include "GeoUtils.h"

#include <QAction>
#include <QToolBar>
#include <QDockWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QHeaderView>
#include <QApplication>
#include <QGroupBox>
#include <QFileInfo>
#include <qmath.h>
#include <iostream>
#include <QActionGroup>
#include "device_param_dialog.h"
struct MainWindow::Impl
{
    Impl()
    {

    }
    ~Impl()
    {

    }
    void InitWidget(MainWindow* parent)
    {
        m_parent=parent;
        m_mgr = new ProtectionManager(m_parent);
        m_det = new ProtectionDetector(m_parent);
        m_det->setManager(m_mgr);
        m_drv = new LidarDriver(m_parent);
        /*DeviceParam param;
        param.lidar_type=LidarType::HA25;
        //param.pc_network_param.ip="192.168.2.177";
        std::string ip="192.168.2.177";
        param.input_param.pc_network_param.ip=ip;
        param.input_param.pc_network_param.port=9000;
        ip="192.168.2.72";
        param.input_param.lidar_network_param.ip=ip;
        param.input_param.lidar_network_param.port=5000;
        m_drv->Init(param);
        m_drv->Start();*/
        //m_drv->setSimulated(true);
        createCentralWidget();
        createActions();
        createToolBar();
        createDockPanels();
        m_status_label = new QLabel(tr("就绪"));
        m_alarm_label = new QLabel(tr("安全"));
        m_alarm_label->setStyleSheet("color: green; font-weight: bold; padding: 0 10px;");
        m_cursor_label = new QLabel(tr("角度: 0.00°   距离: 0.00 m"));
        m_coord_label = new QLabel(tr("X: 0.000 m   Y: 0.000 m"));
        m_parent->statusBar()->addWidget(m_status_label);
        m_parent->statusBar()->addPermanentWidget(m_cursor_label);
        m_parent->statusBar()->addPermanentWidget(m_coord_label);
        m_parent->statusBar()->addPermanentWidget(m_alarm_label);

        m_parent->setWindowTitle(tr("激光安全防护系统 v2.0"));
        m_parent->resize(1180, 720);

        QObject::connect(m_det, &ProtectionDetector::alarmTriggered, [this](int zoneId, int pointCount){
            this->onAlarmTriggered(zoneId,pointCount);
        });
        QObject::connect(m_det, &ProtectionDetector::alarmCleared, [this](int zoneId){
            this->onAlarmCleared(zoneId);
        });
        onZonesChanged();
    }
private:
// ---------------------------------------------------------------------------
// 中央绘图区
// ---------------------------------------------------------------------------
    void createCentralWidget()
    {
        auto *central = new QWidget(m_parent);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(3);

        // 标题 "区域配置[Bank]"
        auto *title_label = new QLabel(tr("区域配置"), central);
        title_label->setAlignment(Qt::AlignCenter);
        QFont tf = title_label->font();
        tf.setPointSizeF(qMax(9.0, tf.pointSizeF() + 1.0));
        tf.setBold(false);
        title_label->setFont(tf);
        title_label->setStyleSheet("color: #333;");
        layout->addWidget(title_label);
        m_zonetitle_label = title_label;

        m_canvas = new ZoneCanvas(central);
        m_canvas->setViewRange(7.5, 5.0);
        // 270° 雷达覆盖范围：45° -> 315°（逆时针），盲区在 315°~45°（下方，0° 方向附近）
        m_canvas->setScanRange(45.0, 315.0);
        m_canvas->setCartesianMode(true);

        // 接入点云数据通路 + 防护区域管理 + 碰撞检测
        m_canvas->setProtectionManager(m_mgr);
        m_canvas->setProtectionDetector(m_det);
        m_canvas->setLidarDriver(m_drv);

        layout->addWidget(m_canvas, 1);
        m_parent->setCentralWidget(central);

        // ZoneCanvas 信号
        QObject::connect(m_canvas, &ZoneCanvas::polygonDrawn,[this](const QVector<QPointF> &points){
            this->onPolygonDrawn(points);
        });
        QObject::connect(m_canvas, &ZoneCanvas::rectangleDrawn,[this](const QPointF &center, double w, double h){
            this->onRectangleDrawn(center,w,h);
        });
        QObject::connect(m_canvas, &ZoneCanvas::circleDrawn,[this](const QPointF &center, double radius){
            this->onCircleDrawn(center,radius);
        });
        QObject::connect(m_canvas, &ZoneCanvas::sectorDrawn,    [this](double startAngle, double endAngle, double radius){
            this->onSectorDrawn(startAngle, endAngle, radius);
        });
        QObject::connect(m_canvas, &ZoneCanvas::polylineDrawn,  [this](const QVector<QPointF> &points){
            this->onPolylineDrawn(points);
        });
        QObject::connect(m_canvas, &ZoneCanvas::eraseRequested, [this](){
            this->onEraseRequested();
        });
        QObject::connect(m_canvas, &ZoneCanvas::cursorCoordinateChanged,[this](double x, double y, double angle, double range){
            this->onCursorCoordinate(x,y,angle,range);
        });
        QObject::connect(m_canvas, &ZoneCanvas::pointCloudSelected,[this](int count){
            this->onPointCloudSelected(count);
        });
    }
// ---------------------------------------------------------------------------
// 动作
// ---------------------------------------------------------------------------
    void createActions()
    {
        m_act_new = new QAction(tr("新建"), m_parent);
        m_act_new->setShortcut(QKeySequence::New);
        m_act_open = new QAction(tr("打开"), m_parent);
        m_act_open->setShortcut(QKeySequence::Open);
        m_act_save = new QAction(tr("保存"), m_parent);
        m_act_save->setShortcut(QKeySequence::Save);
        m_act_saveas = new QAction(tr("另存为"), m_parent);
        m_act_saveas->setShortcut(QKeySequence::SaveAs);

        QObject::connect(m_act_new,    &QAction::triggered, [this](){
            this->onNewConfig();
        });
        QObject::connect(m_act_open,   &QAction::triggered, [this](){
            this->onOpenConfig();
        });
        QObject::connect(m_act_save,   &QAction::triggered, [this](){
            this->onSaveConfig();
        });
        QObject::connect(m_act_saveas, &QAction::triggered, [this](){
            this->onSaveConfigAs();
        });

        //m_act_delete = new QAction(tr("删除"), m_parent);
        //m_act_copy   = new QAction(tr("复制"), m_parent);
        m_act_clear  = new QAction(tr("清空"), m_parent);
        m_act_erase  = new QAction(tr("擦除"), m_parent);
        /*QObject::connect(m_act_delete, &QAction::triggered, [this](){
            this->onDeleteZone();
        });
        QObject::connect(m_act_copy,   &QAction::triggered, [this](){
            this->onCopyZone();
        });*/
        QObject::connect(m_act_clear,  &QAction::triggered, [this](){
            this->onClearZones();
        });
        QObject::connect(m_act_erase,  &QAction::triggered, [this](){
            this->onEraseZone();
        });

        m_act_sim = new QAction(tr("雷达"), m_parent);
        m_act_sim->setCheckable(true);
        m_act_sim->setChecked(true);
        m_act_sim->setToolTip(tr("切换雷达数据"));
        QObject::connect(m_act_sim, &QAction::toggled, [this](bool on){
            this->onSimToggled(on);
        });

        m_act_grid = new QAction(tr("网格"), m_parent);
        m_act_grid->setCheckable(true);
        m_act_grid->setChecked(true);
        m_act_grid->setToolTip(tr("显示 / 隐藏网格"));
        QObject::connect(m_act_grid, &QAction::toggled, [this](bool on){
            this->onGridToggled(on);
        });
    }
    void createToolBar()
    {
        auto *tb = m_parent->addToolBar(tr("文件"));
        tb->setMovable(false);
        tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tb->addAction(m_act_new);
        tb->addAction(m_act_open);
        tb->addAction(m_act_save);
        tb->addAction(m_act_saveas);
        tb->addSeparator();
        //tb->addAction(m_act_copy);
        tb->addAction(m_act_erase);
        //tb->addAction(m_act_delete);
        tb->addAction(m_act_clear);
        tb->addSeparator();
        tb->addAction(m_act_sim);
        tb->addAction(m_act_grid);
    }
// ---------------------------------------------------------------------------
// 左侧面板
// ---------------------------------------------------------------------------
    void createDockPanels()
    {
        auto *bank_dock = new QDockWidget(tr("Bank 配置"), m_parent);
        bank_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        bank_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

        m_bank_panel = new BankConfigPanel(m_mgr, bank_dock);
        bank_dock->setWidget(m_bank_panel);
        m_parent->addDockWidget(Qt::LeftDockWidgetArea, bank_dock);
        bank_dock->setMinimumWidth(210);

        QObject::connect(m_bank_panel, &BankConfigPanel::activeZoneChanged,[this](int zoneId){
            this->onActiveZoneChanged(zoneId);
        });
        QObject::connect(m_bank_panel, &BankConfigPanel::targetBankOutputChanged,[this](int bank, int output){
            this->onTargetBankOutputChanged(bank,output);
        });
        QObject::connect(m_bank_panel, &BankConfigPanel::zoneModified,[this](){
            this->onZonesChanged();
        });
        QObject::connect(m_bank_panel, &BankConfigPanel::coordinateModeChanged,[this](bool cartesian) {
            m_canvas->setCartesianMode(cartesian);
        });
    }
// ---------------------------------------------------------------------------
// 区域增删改
// ---------------------------------------------------------------------------
    void onNewConfig()
    {
        if (QMessageBox::question(m_parent, tr("确认"),tr("清空当前所有区域配置？")) != QMessageBox::Yes)
        {
            return;
        }
        m_mgr->clearZones();
        m_current_file.clear();
        m_parent->setWindowTitle(tr("激光安全防护系统 v2.0"));
        m_status_label->setText(tr("已新建配置"));
    }

    void onOpenConfig()
    {
        QString path = QFileDialog::getOpenFileName(m_parent, tr("打开区域配置"),"", tr("JSON 文件 (*.json)"));
        if (path.isEmpty())
        {
            return;
        }

        if (m_mgr->loadFromFile(path)) 
        {
            m_current_file = path;
            m_parent->setWindowTitle(tr("激光安全防护系统 v2.0 - %1").arg(QFileInfo(path).fileName()));
            m_status_label->setText(tr("已打开: %1").arg(path));
        } 
        else 
        {
            QMessageBox::warning(m_parent, tr("错误"), tr("打开失败"));
        }
    }

    void onSaveConfig()
    {
        if (m_current_file.isEmpty()) 
        {
            onSaveConfigAs();
            return;
        }
        if (m_mgr->saveToFile(m_current_file))
        {
            m_status_label->setText(tr("已保存: %1").arg(m_current_file));
        }
        else
        {
            QMessageBox::warning(m_parent, tr("错误"), tr("保存失败"));
        }        
    }

    void onSaveConfigAs()
    {
        QString path = QFileDialog::getSaveFileName(m_parent, tr("另存为"),"zones.json", tr("JSON 文件 (*.json)"));
        if (path.isEmpty())
        {
            return;
        }
        if (m_mgr->saveToFile(path)) 
        {
            m_current_file = path;
            m_parent->setWindowTitle(tr("激光安全防护系统 v2.0 - %1").arg(QFileInfo(path).fileName()));
            m_status_label->setText(tr("已保存: %1").arg(path));
        } 
        else 
        {
            QMessageBox::warning(m_parent, tr("错误"), tr("保存失败"));
        }
    }

    void onDeleteZone()
    {
        if (m_mgr->activeZoneId() < 0) 
        {
            QMessageBox::information(m_parent, tr("提示"), tr("请先选择一个区域"));
            return;
        }
        m_mgr->removeZone(m_mgr->activeZoneId());
    }

    void onCopyZone()
    {
        if (m_mgr->copyActiveZone() >= 0)
        {
            onZonesChanged();
        }  
    }

    void onClearZones()
    {
        if (QMessageBox::question(m_parent, tr("确认"),tr("清空所有防护区域？")) == QMessageBox::Yes)
        {
            m_mgr->clearZones();
        }   
    }

    void onEraseZone()
    {
        m_mgr->eraseActiveZone();
        onZonesChanged();
    }

    // ---------------------------------------------------------------------------
    // 状态刷新
    // ---------------------------------------------------------------------------
    void onZonesChanged()
    {
        refreshCanvas();
    }

    /// 刷新画布：显示激活区域
    void refreshCanvas()
    {
        if (!m_canvas)
        {
            return;
        }

        ProtectionZone *z = m_mgr->activeZone();

        // 激活区域不属于当前 Bank 时不清除 ProtectionItems（由 setCurrentBank 管理），
        // 但必须清除旧接口遗留的 activeZone 图元，避免显示其他 Bank 的图形
        if (!z || z->bankIndex != m_target_bank)
        {
            m_canvas->clearActiveZone();
            if (m_zonetitle_label)
            {
                m_zonetitle_label->setText(tr("区域配置[Bank%1-OUT%2]").arg(m_target_bank).arg(m_target_output));
            }
            return;
        }

        QVector<QPointF> poly = z->boundaryPoints();
        QColor edge = z->alarmed ? QColor(230, 40, 40) : QColor(220, 60, 60);
        QColor fill = z->alarmed ? QColor(230, 40, 40, 70) : QColor(220, 60, 60, 45);

        m_canvas->setActiveZone(poly, edge, fill);

        if (m_zonetitle_label)
        {
            m_zonetitle_label->setText(tr("区域配置[%1]").arg(z->bankIndex));
        }
    }

    void onActiveZoneChanged(int zoneId)
    {
        // 激活区域变化时仅刷新画布显示，不覆盖用户在 BankConfigPanel 中
        // 明确选定的 target Bank/Output（否则选中 OUTPUT3 但 OUTPUT1 已有区域时，
        // targetOutput 会被错误地改回 1，导致后续绘制绑到错误的 OUTPUT）
        m_canvas->setTargetBank(m_target_bank, m_target_output);
        m_status_label->setText(tr("当前区域: %1").arg(zoneId));
        refreshCanvas();
    }

    void onTargetBankOutputChanged(int bank, int output)
    {
        // 用户在 BankConfigPanel 中切换了 Bank 或 Output Tab。
        // 记住目标位置，使后续绘制的新区域归属到此 Bank/Output，
        // 即使该 Bank+Output 下当前尚无区域。
        m_target_bank = bank;
        m_target_output = output;
        m_canvas->setTargetBank(bank, output);
        // 切换 Bank 后，画布只显示该 Bank 下 3 个 OUTPUT 绑定的图形
        m_canvas->setCurrentBank(bank);
        if (m_zonetitle_label)
        {
            m_zonetitle_label->setText(tr("区域配置[Bank%1-OUT%2]").arg(bank).arg(output));
        }  
    }

    void onCursorCoordinate(double x, double y, double angle, double range)
    {
        if (m_cursor_label)
        {
            m_cursor_label->setText(tr("角度: %1°   距离: %2 m").arg(angle, 0, 'f', 2).arg(range, 0, 'f', 2));
        
        }
        if (m_coord_label)
        {
            m_coord_label->setText(tr("X: %1 m   Y: %2 m").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
        }     
    }

    void onPointCloudSelected(int count)
    {
        m_status_label->setText(tr("已选中 %1 个点").arg(count));
    }

    void onAlarmTriggered(int zoneId, int pointCount)
    {
        m_alarm_label->setText(tr("报警! 区域 %1 (%2 点)").arg(zoneId).arg(pointCount));
        m_alarm_label->setStyleSheet("color: red; font-weight: bold; padding: 0 10px;");
        m_status_label->setText(tr("区域 %1 触发报警").arg(zoneId));
        onZonesChanged();
    }

    void onAlarmCleared(int zoneId)
    {
        if (!m_det->hasAlarm()) 
        {
            m_alarm_label->setText(tr("安全"));
            m_alarm_label->setStyleSheet("color: green; font-weight: bold; padding: 0 10px;");
        }
        m_status_label->setText(tr("区域 %1 报警解除").arg(zoneId));
        onZonesChanged();
    }

    void onSimToggled(bool on)
    {
        //m_drv->setSimulated(on);
        if(m_is_link)
        {
            m_drv->Stop();
            m_is_link=false;
        }
        else
        {
            DeviceParamDialog device_param_dialog(m_param);
            if(device_param_dialog.exec())
            {
                device_param_dialog.GetParam(m_param);
                if(m_drv->Init(m_param))
                {
                    m_is_link=m_drv->Start();
                }
            }
        }
       
    }

    void onGridToggled(bool on)
    {
        m_canvas->setGridVisible(on);
    }

    // ---------------------------------------------------------------------------
    // 画布绘制回调
    // ---------------------------------------------------------------------------
    void onPolygonDrawn(const QVector<QPointF> &points)
    {
        int activeId = m_mgr->activeZoneId();
        if (activeId > 0)
        {
            const ProtectionZone *z = m_mgr->zone(activeId);
            if (z)
            {
                ProtectionZone updated = *z;
                updated.shape = ProtectionZone::Polygon;
                updated.points = points;
                m_mgr->updateZone(activeId, updated);
                m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
                return;
            }
        }
        m_mgr->addPolygon(points, m_target_bank, m_target_output);
        m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
    }

    void onRectangleDrawn(const QPointF &center, double w, double h)
    {
        int activeId = m_mgr->activeZoneId();
        if (activeId > 0)
        {
            const ProtectionZone *z = m_mgr->zone(activeId);
            if (z)
            {
                ProtectionZone updated = *z;
                updated.shape = ProtectionZone::Rectangle;
                updated.width = w;
                updated.height = h;
                updated.angle = 0;
                double hw = w / 2.0, hh = h / 2.0;
                updated.points.clear();
                updated.points << QPointF(center.x() - hw, center.y() - hh)
                               << QPointF(center.x() + hw, center.y() - hh)
                               << QPointF(center.x() + hw, center.y() + hh)
                               << QPointF(center.x() - hw, center.y() + hh);
                m_mgr->updateZone(activeId, updated);
                m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
                return;
            }
        }
        m_mgr->addRectangle(center, w, h, 0, m_target_bank, m_target_output);
        m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
    }

    void onCircleDrawn(const QPointF &center, double radius)
    {
        int activeId = m_mgr->activeZoneId();
        if (activeId > 0)
        {
            const ProtectionZone *z = m_mgr->zone(activeId);
            if (z)
            {
                ProtectionZone updated = *z;
                updated.shape = ProtectionZone::Circle;
                updated.radius = radius;
                updated.points.clear();
                const int segs = 64;
                for (int i = 0; i < segs; ++i)
                {
                    double a = 360.0 * double(i) / double(segs);
                    updated.points.append(center + GeoUtils::polarToCartesian(radius, a));
                }
                m_mgr->updateZone(activeId, updated);
                m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
                return;
            }
        }
        m_mgr->addCircle(center, radius, m_target_bank, m_target_output);
        m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
    }

    void onSectorDrawn(double startAngle, double endAngle, double radius)
    {
        int activeId = m_mgr->activeZoneId();
        if (activeId > 0)
        {
            const ProtectionZone *z = m_mgr->zone(activeId);
            if (z)
            {
                ProtectionZone updated = *z;
                updated.shape = ProtectionZone::Sector;
                updated.startAngle = startAngle;
                updated.endAngle = endAngle;
                updated.radius = radius;
                // z->points 只放起始端点和终止端点（扇形的 2 个特征点）
                updated.points.clear();
                updated.points << GeoUtils::polarToCartesian(radius, startAngle);
                updated.points << GeoUtils::polarToCartesian(radius, endAngle);
                m_mgr->updateZone(activeId, updated);
                m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
                return;
            }
        }
        m_mgr->addSector(startAngle, endAngle, radius, m_target_bank, m_target_output);
        m_canvas->setDrawMode(ZoneCanvas::DrawMode::Select);
    }

    void onPolylineDrawn(const QVector<QPointF> &points)
    {
        // 铅笔绘制：作为多边形区域处理
        onPolygonDrawn(points);
    }

    void onEraseRequested()
    {
        onEraseZone();
    }
public:
    MainWindow* m_parent{nullptr};
    bool        m_is_link{false};
    DeviceParam m_param{};
    // 中央控件
    ZoneCanvas *m_canvas = nullptr;

    // 业务模块
    ProtectionManager  *m_mgr = nullptr;
    ProtectionDetector *m_det = nullptr;
    LidarDriver        *m_drv = nullptr;
    BankConfigPanel    *m_bank_panel = nullptr;

    // 工具栏动作
    QAction *m_act_new = nullptr;
    QAction *m_act_open = nullptr;
    QAction *m_act_save = nullptr;
    QAction *m_act_saveas = nullptr;

    //QAction *m_act_delete = nullptr;
    //QAction *m_act_copy = nullptr;
    QAction *m_act_clear = nullptr;
    QAction *m_act_erase = nullptr;

    QAction *m_act_sim = nullptr;
    QAction *m_act_grid = nullptr;

    // 区域列表（浮动窗口形式，不再使用 Dock）
    QTableWidget *m_zone_table = nullptr;

    // 中央画布标题 "区域配置[Bank]"
    QLabel *m_zonetitle_label = nullptr;

    // 当前文件路径
    QString m_current_file;

    // 状态栏
    QLabel *m_status_label = nullptr;
    QLabel *m_alarm_label = nullptr;
    QLabel *m_cursor_label = nullptr;
    QLabel *m_coord_label = nullptr;

    // 用户在 BankConfigPanel 中选中的目标 Bank/Output。
    // 即使该 Bank+Output 下尚无区域，后续绘制也会归属到此 Bank/Output。
    int m_target_bank{0};
    int m_target_output{1};
};
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    ,m_impl(std::make_shared<MainWindow::Impl>())
{
    InitWidget();
}

void MainWindow::InitWidget()
{
    m_impl->InitWidget(this);
}

MainWindow::~MainWindow()
{
    m_impl.reset();
    m_impl=nullptr;
}
