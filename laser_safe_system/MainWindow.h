#pragma once

#include <QMainWindow>
#include <memory>

/**
 * @brief 主窗口（对应 效果图3.jpeg）
 *
 * 布局：
 *  - 顶部工具栏：新建 / 打开 / 保存 / 另存为
 *  - 左侧 Dock：Bank 区域配置面板（Bank 列表 + OUT1/OUT2/OUT3/Data 配置）
 *  - 中央：区域配置绘图画布（ZoneCanvas），顶部标题 "区域配置[Bank]"
 *  - 底部状态栏：坐标信息、报警状态
 */

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    void InitWidget();
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl{nullptr};
};
