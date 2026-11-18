#include "BankConfigPanel.h"
#include "GeoUtils.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QListWidget>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>
#include <qmath.h>
#include <iostream>

namespace
{
/// Bank 列表中 OUT 条目的配色（对应 效果图3：OUT1 红 / OUT2 黄 / OUT3 青）
struct OutColor { const char *bg; const char *fg; };

const OutColor kOutColors[3] = {
    { "#e05a5a", "#ffffff" },   // OUT1  红
    { "#f0c04a", "#5a4600" },   // OUT2  黄
    { "#5ec8e8", "#0d3a4a" }    // OUT3  青
};
}

OutputWidget::OutputWidget(QWidget *parent)
    : QWidget(parent)
{
    InitWidget();
}

OutputWidget::~OutputWidget()
{

}

void OutputWidget::InitWidget()
{
    auto *form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->addItem(tr("多边形"), int(ProtectionZone::Polygon));
    //m_shapeCombo->addItem(tr("圆形"),   int(ProtectionZone::Circle));
    m_shapeCombo->addItem(tr("矩形"),   int(ProtectionZone::Rectangle));
    m_shapeCombo->addItem(tr("扇形"),   int(ProtectionZone::Sector));

    m_resolutionSpin = new QSpinBox(this);
    m_resolutionSpin->setRange(2, 10);
    m_resolutionSpin->setValue(3);

    m_samplingSpin = new QSpinBox(this);
    m_samplingSpin->setRange(2, 10);
    m_samplingSpin->setValue(2);
    m_samplingSpin->setSuffix("x");

    m_angleLabel = new QLabel("-", this);
    m_sampleLabel = new QLabel("-", this);
    m_sampleLabel->setWordWrap(true);

    form->addRow(tr("形状:"), m_shapeCombo);
    form->addRow(tr("分辨率:"), m_resolutionSpin);
    form->addRow(tr("多重采样:"), m_samplingSpin);
    form->addRow(tr("起止角度:"), m_angleLabel);
    form->addRow(tr("判定数据:"), m_sampleLabel);
}

BankConfigPanel::BankConfigPanel(ProtectionManager *mgr, QWidget *parent)
    : QWidget(parent), m_mgr(mgr)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(2, 2, 2, 2);
    root->setSpacing(4);

    buildBankList();
    buildOutputTabs();

    root->addWidget(m_bankList, 3);
    root->addWidget(m_paramBox, 4);

    QObject::connect(m_mgr, &ProtectionManager::zonesChanged, this, &BankConfigPanel::refresh);
    QObject::connect(m_mgr, &ProtectionManager::activeZoneChanged, this, [this](int id) {
        refresh();
        emit activeZoneChanged(id);
    });
}

// ---------------------------------------------------------------------------
// 上部：Bank 列表（每 Bank 一组，组内三行彩色 OUT 条目）
// ---------------------------------------------------------------------------
void BankConfigPanel::buildBankList()
{
    m_bankList = new QListWidget(this);
    m_bankList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_bankList->setUniformItemSizes(false);
    m_bankList->setSpacing(0);
    m_bankList->setToolTip(tr("16 Banks × 3 outputs"));

    // 每个 Bank 占 4 项：1 个标题（Bank 编号）+ 3 个 OUT 条目
    for (int bank = 0; bank < ProtectionManager::kBankCount; ++bank) 
    {
        auto *head = new QListWidgetItem(QString::number(bank), m_bankList);
        head->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        head->setData(Qt::UserRole, QVariant::fromValue(QPair<int, int>(bank, 0)));   // output=0 表示标题
        QFont f = head->font();
        f.setBold(true);
        head->setFont(f);
        head->setTextAlignment(Qt::AlignCenter);
        head->setBackground(QColor(238, 238, 238));
        head->setForeground(QColor(40, 40, 40));

        for (int out = 1; out <= ProtectionManager::kOutputCount; ++out) 
        {
            auto *it = new QListWidgetItem(QString("OUTPUT %1").arg(out), m_bankList);
            it->setSizeHint(QSize(335, 50));
            it->setData(Qt::UserRole, QVariant::fromValue(QPair<int, int>(bank, out)));
            it->setBackground(QColor(kOutColors[out - 1].bg));
            it->setForeground(QColor(kOutColors[out - 1].fg));
            it->setTextAlignment(Qt::AlignCenter);
        }
    }

    m_bankList->setCurrentRow(1);   // 默认选中 Bank0 / OUT1

    QObject::connect(m_bankList, &QListWidget::currentRowChanged,this, &BankConfigPanel::onBankSelectionChanged);
}

// ---------------------------------------------------------------------------
// 下部：Bank 区域配置选项卡
// ---------------------------------------------------------------------------
void BankConfigPanel::buildOutputTabs()
{
    m_paramBox = new QGroupBox(tr("Bank 区域配置"), this);
    QVBoxLayout* layout=new QVBoxLayout(m_paramBox);
    m_output_tabs = new QTabWidget(m_paramBox);
    m_output_tabs->setDocumentMode(true);

    // ---- OUT1 / OUT2 / OUT3 参数页 ----
    buildParamBox();
    
    // ---- Data 坐标页 ----
    QWidget *dataPage = new QWidget(m_output_tabs);
    auto *dataLayout = new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(4, 6, 4, 4);
    buildPointTable();
    dataLayout->addWidget(m_point_table);

    auto *modeBox = new QHBoxLayout;
    m_radioCartesian = new QRadioButton(QStringLiteral("直角坐标"), dataPage);
    m_radioPolar = new QRadioButton(QStringLiteral("极坐标"), dataPage);
    m_radioCartesian->setChecked(true);
    modeBox->addWidget(m_radioCartesian);
    modeBox->addWidget(m_radioPolar);
    modeBox->addStretch();
    dataLayout->addLayout(modeBox);

    QObject::connect(m_radioCartesian, &QRadioButton::toggled, this, &BankConfigPanel::onCoordinateModeChanged);
    QObject::connect(m_radioPolar, &QRadioButton::toggled, this, &BankConfigPanel::onCoordinateModeChanged);

    m_output_tabs->addTab(dataPage, tr("Data"));
    layout->addWidget(m_output_tabs);

    QObject::connect(m_output_tabs, &QTabWidget::currentChanged, this, &BankConfigPanel::onOutputTabChanged);
}

void BankConfigPanel::buildParamBox()
{
    
    for(int i=0;i<3;i++)
    {
        /*QWidget *paramPage = new QWidget(m_output_tabs);
        auto *paramLayout = new QVBoxLayout(paramPage);
        paramLayout->setContentsMargins(4, 6, 4, 4);
        
        //paramLayout->addWidget(m_paramBox);
        paramLayout->addWidget(widget);
        paramLayout->addStretch();
        QString flag=QString("OUT%1").arg(i);
        m_output_tabs->addTab(paramPage, flag);*/
        OutputWidget* widget=new OutputWidget();
        QString flag=QString("OUT%1").arg(i);
        m_output_tabs->addTab(widget, flag);
        //m_output_tabs->addTab(paramPage, "OUT2");
        //m_output_tabs->addTab(paramPage, "OUT3");
    }
}

void BankConfigPanel::buildPointTable()
{
    m_point_table = new QTableWidget(0, 3, this);
    m_point_table->setHorizontalHeaderLabels({"Pt", "X(m)", "Y(m)"});
    m_point_table->verticalHeader()->setVisible(false);
    m_point_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_point_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_point_table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    QObject::connect(m_point_table, &QTableWidget::cellChanged,this, &BankConfigPanel::onCellChanged);
}

// ---------------------------------------------------------------------------
// 刷新
// ---------------------------------------------------------------------------
void BankConfigPanel::refresh()
{
    if (m_updating)
    {
        return;
    }
    m_updating = true;

    // --- Bank 列表：更新各 OUT 条目的状态 ---
    for (int i = 0; i < m_bankList->count(); ++i) 
    {
        QListWidgetItem *it = m_bankList->item(i);
        auto pr = it->data(Qt::UserRole).value<QPair<int, int>>();
        int bank = pr.first, out = pr.second;
        if (out == 0)
        {
            continue;
        }

        ProtectionZone *z = m_mgr->zoneAt(bank, out);
        if (z) 
        {
            it->setText(z->alarmed ? QString("OUTPUT %1  ALARM").arg(out)
                                   : QString("OUTPUT %1  SET").arg(out));
            it->setBackground(QColor(kOutColors[out - 1].bg));
            it->setForeground(QColor(kOutColors[out - 1].fg));
        } 
        else 
        {
            it->setText(QString("OUTPUT %1").arg(out));
            it->setBackground(QColor(250, 250, 250));
            it->setForeground(QColor(150, 150, 150));
        }
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }

    // --- 标题：Bank 区域配置[当前 Bank] ---
    int curBank = 0;
    int curRow = m_bankList->currentRow();
    if (curRow >= 0) 
    {
        auto pr = m_bankList->item(curRow)->data(Qt::UserRole).value<QPair<int, int>>();
        curBank = pr.first;
    }
    m_paramBox->setTitle(tr("Bank 区域配置[%1]").arg(curBank));

    // --- 参数与坐标表 ---
    ProtectionZone* z = currentZone();
    
    //
    if (z) 
    {   
        int index=(z->id-1)%3;
        
        OutputWidget* widget=(OutputWidget*)m_output_tabs->widget(index);
        //OutputWidget* widget=(OutputWidget*)m_output_tabs->currentWidget();
        widget->m_shapeCombo->setCurrentIndex(widget->m_shapeCombo->findData(int(z->shape)));
        widget->m_resolutionSpin->setValue(z->targetResolution);
        widget->m_samplingSpin->setValue(z->multiSampling);

        widget->m_angleLabel->setText(QString("%1° / %2°").arg(z->effectiveStartAngle(), 0, 'f', 2).arg(z->effectiveEndAngle(), 0, 'f', 2));

        // SW-RS-012：判定数据量（16 Bank x 3 区域，总量不超过 16k）
        QVector<ZoneSample> samples = z->computeSamples(16000);
        widget->m_sampleLabel->setText(tr("%1 点 (上限 16k)").arg(samples.size()));
    } 
    else 
    {
        //widget->m_angleLabel->setText("-");
        //widget->m_sampleLabel->setText("-");
    }

    fillPointTable();

    m_updating = false;
}

void BankConfigPanel::fillPointTable()
{
    m_point_table->setRowCount(0);
    ProtectionZone *z = currentZone();
    if (!z)
    {
        return;
    }  

    QVector<QPointF> pts = z->points;
    bool polar = isPolarMode();

    // 表头随坐标模式变化
    if (polar)
    {
        m_point_table->setHorizontalHeaderLabels(QStringList() << "Pt" << "Angle(°)" << "R(m)");
    }
    else
    {
        m_point_table->setHorizontalHeaderLabels(QStringList() << "Pt" << "X(m)" << "Y(m)");
    }
    for (int i = 0; i < pts.size(); ++i) 
    {
        int row = m_point_table->rowCount();
        m_point_table->insertRow(row);

        auto *idx = new QTableWidgetItem(QString::number(i + 1));
        idx->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_point_table->setItem(row, 0, idx);

        if (polar) 
        {
            auto pr = GeoUtils::cartesianToPolar(pts[i]);
            m_point_table->setItem(row, 1, new QTableWidgetItem(
                QString::number(pr.second, 'f', 2)));
            m_point_table->setItem(row, 2, new QTableWidgetItem(
                QString::number(pr.first, 'f', 3)));
        } 
        else 
        {
            m_point_table->setItem(row, 1, new QTableWidgetItem(
                QString::number(pts[i].x(), 'f', 3)));
            m_point_table->setItem(row, 2, new QTableWidgetItem(
                QString::number(pts[i].y(), 'f', 3)));
        }
    }
}

bool BankConfigPanel::isPolarMode() const
{
    return m_radioPolar && m_radioPolar->isChecked();
}

ProtectionZone *BankConfigPanel::currentZone() const
{
    if (!m_mgr)
    {
        return nullptr;
    } 
    return m_mgr->zone(m_mgr->activeZoneId());
}


bool BankConfigPanel::itemToZone(int listIndex, int &bank, int &output) const
{
    if (listIndex < 0 || listIndex >= m_bankList->count())
    {
        return false;
    }
    auto pr = m_bankList->item(listIndex)->data(Qt::UserRole).value<QPair<int, int>>();
    bank = pr.first;
    output = pr.second;
    return output > 0;
}

int BankConfigPanel::findItemIndex(int bank, int output) const
{
    for (int i = 0; i < m_bankList->count(); ++i) 
    {
        auto pr = m_bankList->item(i)->data(Qt::UserRole).value<QPair<int, int>>();
        if (pr.first == bank && pr.second == output)
        {   
            return i;
        }    
    }
    return -1;
}

// ---------------------------------------------------------------------------
// 交互
// ---------------------------------------------------------------------------
void BankConfigPanel::onBankSelectionChanged()
{
    if (m_updating)
    {
        return;
    }

    int bank = 0, output = 0;
    if (!itemToZone(m_bankList->currentRow(), bank, output))
    {
        return;
    }

    // 更新目标 Bank/Output（即使该位置尚无区域，后续绘制也归属到此）
    if (output >= 1 && output <= ProtectionManager::kOutputCount) 
    {
        m_targetBank = bank;
        m_targetOutput = output;
    } 
    else 
    {
        // 选中的是 Bank 标题行（output=0）：保留 Output Tab 的当前选择
        m_targetBank = bank;
        m_targetOutput = m_output_tabs->currentIndex() + 1;
    }
    emit targetBankOutputChanged(m_targetBank, m_targetOutput);

    // 仅激活用户选中的 Bank+Output 下已存在的区域。
    // 不再"退而选择该 Bank 下任意已配置区域"——否则用户选中 OUTPUT3
    // 但 OUTPUT1 有区域时会错误激活 OUTPUT1，导致 Output Tab 跳转、
    // 后续绘制绑到错误的 OUTPUT。
    ProtectionZone *z = m_mgr->zoneAt(bank, output);

    if (z) 
    {
        m_updating = true;
        m_mgr->setActiveZoneId(z->id);
        m_updating = false;

        // 同步输出选项卡
        int idx = qBound(0, z->outputIndex - 1, 2);
        if (m_output_tabs->currentIndex() != idx)
        {
            m_output_tabs->setCurrentIndex(idx);
        }    
    } 
    else 
    {
        // 该 Bank+Output 下尚无区域：清除激活状态，保持用户选中的 Output Tab
        m_updating = true;
        if (m_mgr->activeZoneId() >= 0)
        {
            m_mgr->setActiveZoneId(-1);
        }
        int idx = qBound(0, m_targetOutput - 1, 2);
        if (m_output_tabs->currentIndex() != idx)
        {
            m_output_tabs->setCurrentIndex(idx);
        }
        m_updating = false;
    }
}

void BankConfigPanel::onOutputTabChanged(int index)
{
    if (m_updating || index < 0 || index > 2)
    {
        return;
    }

    int output = index + 1;
    int bank = 0, curOut = 0;
    if (!itemToZone(m_bankList->currentRow(), bank, curOut))
    {
        return;
    } 

    // 更新目标 Output：用户切到 OUT2 就表示要往该 Bank 的 OUT2 绘制
    m_targetBank = bank;
    m_targetOutput = output;
    emit targetBankOutputChanged(m_targetBank, m_targetOutput);

    ProtectionZone *z = m_mgr->zoneAt(bank, output);
    if (z && m_mgr->activeZoneId() != z->id) 
    {
        m_updating = true;
        m_mgr->setActiveZoneId(z->id);
        m_updating = false;
        OutputWidget* widget=(OutputWidget*)m_output_tabs->currentWidget();
        if (widget) 
        {
            std::cout<<"onOutputTabChanged z id:"<<z->id<<std::endl;
            widget->m_shapeCombo->setCurrentIndex(widget->m_shapeCombo->findData(int(z->shape)));
            widget->m_resolutionSpin->setValue(z->targetResolution);
            widget->m_samplingSpin->setValue(z->multiSampling);

            widget->m_angleLabel->setText(QString("%1° / %2°").arg(z->effectiveStartAngle(), 0, 'f', 2).arg(z->effectiveEndAngle(), 0, 'f', 2));

            // SW-RS-012：判定数据量（16 Bank x 3 区域，总量不超过 16k）
            QVector<ZoneSample> samples = z->computeSamples(16000);
            widget->m_sampleLabel->setText(tr("%1 点 (上限 16k)").arg(samples.size()));
        } 
        fillPointTable();
    }
}

void BankConfigPanel::onCoordinateModeChanged()
{
    if (m_updating)
    {
        return;
    }   
    fillPointTable();
    emit coordinateModeChanged(!isPolarMode());
}

void BankConfigPanel::onCellChanged(int row, int col)
{
    if (m_updating)
    {
        return;
    }
    ProtectionZone *z = currentZone();
    if (!z || row < 0 || row >= z->points.size())
    {
        return;
    } 

    QTableWidgetItem *itemA = m_point_table->item(row, 1);
    QTableWidgetItem *itemR = m_point_table->item(row, 2);
    if (!itemA || !itemR)
    {
        return;
    }
    double v1 = itemA->text().toDouble();
    double v2 = itemR->text().toDouble();

    QPointF np;
    if (isPolarMode()) 
    {
        // 极坐标：角度(度) + 半径(米)
        np = GeoUtils::polarToCartesian(v2, v1);
    } 
    else 
    {
        // 直角坐标：X + Y
        np = QPointF(v1, v2);
    }

    z->points[row] = np;

    m_updating = true;
    m_mgr->updateZone(z->id, *z);
    m_updating = false;

    emit zoneModified();
}



