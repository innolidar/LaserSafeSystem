#pragma once

#include <QWidget>
#include "ProtectionManager.h"

class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QRadioButton;
class QTableWidget;
class QGroupBox;
class QStackedWidget;

/**
 * @brief Bank 区域配置面板（对应 效果图3.jpeg 左侧）
 *
 * 布局：
 *  - 上部：Bank 列表（16 个 Bank，每组含 OUT1 红 / OUT2 黄 / OUT3 青 三条）
 *  - 下部：Bank 区域配置选项卡（OUT1 / OUT2 / OUT3 / Data）
 *      - OUT 页：形状、分辨率、多重采样率、起止角度、判定数据量
 *      - Data 页：坐标点表格（Pt / X(m) / Y(m)），可编辑
 *      - 底部：直角坐标 / 极坐标 单选
 */

class OutputWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OutputWidget(QWidget *parent = nullptr);
    ~OutputWidget();
private:
    void InitWidget();
public:
    QComboBox*  m_shapeCombo{nullptr};
    QSpinBox*   m_resolutionSpin{nullptr};   ///< 目标分辨率 2-10
    QSpinBox*   m_samplingSpin{nullptr};     ///< 多重采样率 2-10
    // 起止角度与判定信息
    QLabel*     m_angleLabel{nullptr};
    QLabel*     m_sampleLabel{nullptr};

};
class BankConfigPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BankConfigPanel(ProtectionManager *mgr, QWidget *parent = nullptr);

    /// 刷新所有显示
    void refresh();

signals:
    /// 激活区域发生变化
    void activeZoneChanged(int zoneId);
    /// 用户修改了区域参数
    void zoneModified();
    /// 坐标显示模式切换（true = 直角坐标）
    void coordinateModeChanged(bool cartesian);
    /// 目标 Bank/Output 发生变化（即使用户选中的 Bank+Output 下尚无区域，
    /// 也会通知 MainWindow，使其后续绘制的区域归属到此 Bank/Output）
    void targetBankOutputChanged(int bank, int output);

public:
    /// 获取当前目标 Bank（用户在列表中选中的 Bank）
    int targetBank() const { return m_targetBank; }
    /// 获取当前目标 Output（用户选中的 Output，1-3）
    int targetOutput() const { return m_targetOutput; }

private slots:
    void onBankSelectionChanged();
    void onOutputTabChanged(int index);
    void onCoordinateModeChanged();
    void onCellChanged(int row, int col);
private:
    void buildBankList();
    void buildOutputTabs();
    void buildPointTable();
    void buildParamBox();

    /// 当前激活区域
    ProtectionZone *currentZone() const;
    /// 填充坐标点表格
    void fillPointTable();
    /// 坐标模式下拉项索引 -> 极坐标/直角坐标
    bool isPolarMode() const;

    /// Bank 列表项索引 -> (bank, output)
    bool itemToZone(int listIndex, int &bank, int &output) const;
    /// 定位列表中 (bank, output) 对应的项索引
    int findItemIndex(int bank, int output) const;

    ProtectionManager *m_mgr = nullptr;

    // 上部 Bank 列表
    QListWidget *m_bankList = nullptr;

    // 下部配置
    QGroupBox*  m_paramBox{nullptr};
    QTabWidget *m_output_tabs = nullptr;
    
    // 坐标点表格
    QTableWidget *m_point_table = nullptr;
    QRadioButton *m_radioCartesian = nullptr;
    QRadioButton *m_radioPolar = nullptr;

    QLabel *m_titleLabel = nullptr;

    bool m_updating = false;   ///< 防止信号回环

    // 用户当前选中的目标 Bank/Output（即使该位置尚无区域，
    // 后续绘制也应归属到此 Bank/Output）
    int m_targetBank = 0;
    int m_targetOutput = 1;
};
