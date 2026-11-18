#pragma once
#include <memory>
#include <QDialog>
#include <QWidget>
#include <QEventLoop>
#include <laser_driver/common/data_types.h>
class DeviceParamDialog:public QDialog
{
public:
    DeviceParamDialog(const DeviceParam& param,QWidget* parent=nullptr);
    ~DeviceParamDialog();
public:
    int exec() override;
    void GetParam(DeviceParam& param);
private Q_SLOTS:
private:
    void InitWidget();
    void closeEvent(QCloseEvent* event) override;
private:
    struct Impl;
    std::shared_ptr<Impl>   m_impl{nullptr};
    QEventLoop*             m_event_loop{nullptr};
};