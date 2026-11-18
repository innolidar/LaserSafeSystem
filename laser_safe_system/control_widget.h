#pragma once
#include <QWidget>
#include <memory>

class ControlWidget:public QWidget
{
    Q_OBJECT
public:
    ControlWidget(QWidget* parent=nullptr);
    ~ControlWidget() override;
private:
    void InitWidget();
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl{nullptr};
};