#include "control_widget.h"
#include "ZoneCanvas.h"
#include "ProtectionManager.h"
#include "ProtectionDetector.h"
#include "LidarDriver.h"
#include "BankConfigPanel.h"
#include "GeoUtils.h"

struct ControlWidget::Impl
{
    Impl()
    {

    }
    ~Impl()
    {

    }
    void InitWidget(ControlWidget* parent)
    {
        m_parent=parent;
    }
private:
    ControlWidget*      m_parent{nullptr};
    // 中央控件
    ZoneCanvas*         m_canvas{nullptr};

    // 业务模块
    ProtectionManager*  m_mgr{nullptr};
    ProtectionDetector* m_det{nullptr};
    LidarDriver*        m_drv{nullptr};
    BankConfigPanel*    m_bankPanel{nullptr};
};

ControlWidget::ControlWidget(QWidget* parent)
:QWidget(parent)
,m_impl(std::make_shared<ControlWidget::Impl>())
{
    InitWidget();
}
ControlWidget::~ControlWidget()
{
    m_impl.reset();
    m_impl=nullptr;
}
/**
 * @brief ControlWidget类的初始化界面函数
 * 该函数用于初始化ControlWidget类的所有界面元素，包括但不限于按钮、标签、布局等
 * 通常在构造函数之后调用，确保所有UI组件正确设置
 */
void ControlWidget::InitWidget()
{
    // 函数体为空，表示该函数可能在子类中被重写
    // 或者该功能尚未实现，需要后续添加初始化代码
    m_impl->InitWidget(this);
}
