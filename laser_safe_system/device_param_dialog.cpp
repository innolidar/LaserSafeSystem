#include "device_param_dialog.h"
#include <QCloseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QSettings>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QCoreApplication>
#include <QComboBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
struct DeviceParamDialog::Impl
{
    Impl(const DeviceParam& param)
    {
        m_param=param;
        m_choose_result=Rejected;
        m_root_folder=QCoreApplication::applicationDirPath();
        std::cout<<"m_root_folder:"<<m_root_folder.toStdString()<<std::endl;
    }
    ~Impl()
    {

    }
    void InitWidget(DeviceParamDialog *parent)
    {
        m_parent=parent;
        QGridLayout* layout=new QGridLayout(parent);
        layout->setContentsMargins(1, 1, 1, 1);
        layout->setSpacing(2);
        QSettings settings(m_root_folder+"/param_config.ini", QSettings::IniFormat);

        QLabel* lidar_type_label=new QLabel(u8"雷达类型:");
        QComboBox* lidar_type_combox=new QComboBox();
        lidar_type_combox->addItems({"HA25","HA50"});
        layout->addWidget(lidar_type_label,0,0,1,1);
        layout->addWidget(lidar_type_combox,0,1,1,3);
        lidar_type_combox->setCurrentIndex((int)m_param.lidar_type);
        QObject::connect(lidar_type_combox,&QComboBox::currentTextChanged,[this](QString text){
            if(text=="HA50")
            {
                m_param.lidar_type=LidarType::HA50;
            }
            else
            {
                m_param.lidar_type=LidarType::HA25;
            }
        });

        QLabel* local_ip_label=new QLabel(u8"本地IP:");
        m_param.input_param.pc_network_param.ip=settings.value("local/ip", "192.168.2.177").toString().toStdString();
        QLineEdit* local_ip_edit=new QLineEdit(QString::fromStdString(m_param.input_param.pc_network_param.ip));
        QRegularExpression regex(R"(^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$)");
        QRegularExpressionValidator *validator =new QRegularExpressionValidator(regex, local_ip_edit);
        local_ip_edit->setValidator(validator);
        QObject::connect(local_ip_edit,&QLineEdit::textChanged,[this](const QString &text){
            if(!text.isEmpty())
            {
                m_param.input_param.pc_network_param.ip=text.toStdString();
            }
        });
        m_param.input_param.pc_network_param.port=settings.value("local/port", 8).toInt();
        QLabel* local_port_label=new QLabel(u8"本地端口:");
        QLineEdit* local_port_edit=new QLineEdit(QString::number(m_param.input_param.pc_network_param.port));
        QObject::connect(local_port_edit,&QLineEdit::textChanged,[this](const QString &text){
            if(!text.isEmpty())
            {
                m_param.input_param.pc_network_param.port=text.toInt();
            }
        });
        layout->addWidget(local_ip_label,1,0,1,1);
        layout->addWidget(local_ip_edit,1,1,1,3);
        layout->addWidget(local_port_label,2,0,1,1);
        layout->addWidget(local_port_edit,2,1,1,3);

        QLabel* lidar_ip_label=new QLabel(u8"雷达IP:");
        m_param.input_param.lidar_network_param.ip=settings.value("lidar/ip", "192.168.1.72").toString().toStdString();
        QLineEdit* lidar_ip_edit=new QLineEdit(QString::fromStdString(m_param.input_param.lidar_network_param.ip));
        QRegularExpressionValidator *lidar_validator =new QRegularExpressionValidator(regex, lidar_ip_edit);
        lidar_ip_edit->setValidator(lidar_validator);
        QObject::connect(lidar_ip_edit,&QLineEdit::textChanged,[this](const QString &text){
            if(!text.isEmpty())
            {
                m_param.input_param.lidar_network_param.ip=text.toStdString();
            }
        });
        m_param.input_param.lidar_network_param.port=settings.value("lidar/port", 8080).toInt();
        QLabel* lidar_port_label=new QLabel(u8"雷达端口:");
        QLineEdit* lidar_port_edit=new QLineEdit(QString::number(m_param.input_param.lidar_network_param.port));
        QObject::connect(lidar_port_edit,&QLineEdit::textChanged,[this](const QString &text){
            if(!text.isEmpty())
            {
                m_param.input_param.lidar_network_param.port=text.toInt();
            }
        });
        layout->addWidget(lidar_ip_label,3,0,1,1);
        layout->addWidget(lidar_ip_edit,3,1,1,3);
        layout->addWidget(lidar_port_label,4,0,1,1);
        layout->addWidget(lidar_port_edit,4,1,1,3);
        QPushButton *ok_btn=new QPushButton("ok");
        QPushButton *cancel_btn=new QPushButton("cancel");
        QObject::connect(ok_btn,&QPushButton::clicked,[this](){
            m_choose_result=Accepted;
            SaveConfig();
            m_parent->close();
        });

        QObject::connect(cancel_btn,&QPushButton::clicked,[this](){
            m_choose_result=Rejected;
            m_parent->close();
        });
        layout->addWidget(ok_btn,5,1,1,1);
        layout->addWidget(cancel_btn,5,2,1,1);
    }
    int GetAccepted() const
    {
        return m_choose_result;
    }
    void SaveConfig()
    {
        QSettings settings(m_root_folder+"/param_config.ini", QSettings::IniFormat);
        settings.setValue("local/ip", QString::fromStdString(m_param.input_param.pc_network_param.ip));       // 字符串
        settings.setValue("local/port", m_param.input_param.pc_network_param.port);  // 整数

        settings.setValue("lidar/ip", QString::fromStdString(m_param.input_param.lidar_network_param.ip));       // 字符串
        settings.setValue("lidar/port", m_param.input_param.lidar_network_param.port);
        
    }
    void GetParam(DeviceParam& param)
    {
        param=m_param;
    }
private:
    DeviceParamDialog*  m_parent{nullptr};
    QString             m_root_folder{""};
    int                 m_choose_result{Rejected};
    DeviceParam         m_param{};
};

DeviceParamDialog::DeviceParamDialog(const DeviceParam& param,QWidget* parent)
:QDialog(parent)
,m_impl(std::make_shared<DeviceParamDialog::Impl>(param))
{
    this->InitWidget();
    this->setWindowTitle("设备配置");
    this->setFixedSize({280,150});
}

DeviceParamDialog::~DeviceParamDialog()
{
    m_impl.reset();
    m_impl=nullptr;
}

void DeviceParamDialog::InitWidget()
{
    m_impl->InitWidget(this);
}
int DeviceParamDialog::exec()
{
    this->setWindowModality(Qt::ApplicationModal);
    show();
    m_event_loop = new QEventLoop(this);
    m_event_loop->exec();
    return m_impl->GetAccepted();
}

void DeviceParamDialog::closeEvent(QCloseEvent* event)
{
    if(m_event_loop!=nullptr)
    {
        m_event_loop->exit();
    }
    event->accept();
}

void DeviceParamDialog::GetParam(DeviceParam& param)
{
    m_impl->GetParam(param);
}