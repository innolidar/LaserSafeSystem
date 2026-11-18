#include "LidarDriver.h"
#include "GeoUtils.h"
#include <QDataStream>
#include <QDateTime>
#include <QRandomGenerator>
#include <qmath.h>
#include <laser_driver/api/laser_driver.h>
#include <laser_driver/common/point_cloud.h>
#include <laser_driver/common/sync_queue.h>
struct LidarDriver::Impl
{
    Impl()
    {
        m_laser_driver=std::make_shared<LaserDriver>();
    }
    ~Impl()
    {
        Stop();

        m_laser_driver.reset();
        m_laser_driver=nullptr;
    }
    bool Init(const DeviceParam &param)
    {
        m_laser_driver->RegisterCallBack(std::bind(&LidarDriver::Impl::PutLaserScan,this,std::placeholders::_1),std::bind(&LidarDriver::Impl::GetLaserScan,this));
        if(!m_laser_driver->InitDevice(param))
        {
            std::cout<<"lidar driver init failed!"<<std::endl;
            return 0;
        }
        std::cout<<"lidar driver init success!"<<std::endl;
        return true;
    }
    bool Start()
    {
        if(m_laser_driver->StartDevice()==false)
        {
            std::cout<<"lidar driver start failed!"<<std::endl;
            return false;
        }
        m_device_start=true;
        return StartShow();
    }
    void Stop()
    {
        StopShow();
        if(m_laser_driver && m_device_start)
        {
            m_laser_driver->StopDevice();
            m_device_start=false;
        }
    }
    void RegisterCallBack(const std::function<void(QVector<QPointF>)> &call_back)
    {
        m_call_back=call_back;
    }
private:
    void StopShow()
    {
        if(m_thread.joinable())
        {
            m_start=false;
            m_thread.join();
        }
    }
    bool StartShow()
    {
        m_start=true;
        m_thread=std::thread([this](){
            while(m_start)
            {
                auto cloud=m_clouds.PopWait(1000);
                if(cloud!=nullptr)
                {
                    QVector<QPointF> points;
                    points.reserve(cloud->scans.size());
                    for (int i = 0; i < cloud->scans.size(); ++i) 
                    {
                        points.append(QPointF(cloud->scans[i].x, cloud->scans[i].y));
                    }
                    m_call_back(points);
                    m_free_clouds.Push(cloud);
                }
                //std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        });
        return m_start;
    }
    std::shared_ptr<inno::LaserScan> GetLaserScan()
    {
        auto cloud=m_free_clouds.Pop();
        if(cloud)
        {
            cloud->ranges.clear();
            cloud->intensities.clear();
            cloud->scans.clear();
        }
        return std::make_shared<inno::LaserScan>();
    }
    void PutLaserScan(std::shared_ptr<inno::LaserScan> cloud)
    {
        m_clouds.Push(cloud);   
    }
private:
    bool                                    m_start{false};
    bool                                    m_device_start{false};
    std::thread                             m_thread{};
    std::shared_ptr<LaserDriver>            m_laser_driver{nullptr};
    std::function<void(QVector<QPointF>)>   m_call_back{nullptr};
    inno::SyncQueue<std::shared_ptr<inno::LaserScan>> m_clouds{};
    inno::SyncQueue<std::shared_ptr<inno::LaserScan>> m_free_clouds{};
    
};
LidarDriver::LidarDriver(QObject *parent)
    : QObject(parent)
    , m_impl(std::make_shared<LidarDriver::Impl>())
{
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
    m_impl->RegisterCallBack([this](QVector<QPointF> points){
        emit pointCloudReceived(points);
    });
}

LidarDriver::~LidarDriver()
{
    m_impl.reset();
    m_impl=nullptr;
}
bool LidarDriver::Init(const DeviceParam &param)
{
    return m_impl->Init(param);  // 调用具体实现类的初始化方法
}
bool LidarDriver::Start()
{
    return m_impl->Start();
}

void LidarDriver::Stop()
{
    return m_impl->Stop();
}