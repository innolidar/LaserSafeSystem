#pragma once
#include <laser_driver/common/data_types.h>
#include <laser_driver/common/point_cloud.h>
//#include <laser_driver/core/base/utils.h>
#include <laser_driver/api/export.h>


class  INNODRIVER_API LaserDriver
{
public:
    LaserDriver();
    ~LaserDriver();
    bool InitDevice(DeviceParam param);
    bool StartDevice();
    void StopDevice();
    std::string GetVersion();
    bool GetSerialNumber(std::string &serial_number);
    bool GetLidarInitStatus();
    bool GetVoltage(float &value);
    bool EraseFlash();
    bool GetVoltagePWM(float &value);
    bool GetTemperature(float &value);
    bool StartCalibration();
    bool CoderCalibrationRead(uint16_t* list,const int &size);
    bool CoderCalibrationWrite(uint16_t* list,const int &size);
    bool ZeroCalibrationRequest(const uint32_t &angle_flag);
    void RegisterCallBack(const std::function<void(std::shared_ptr<inno::LaserScan>)>& put_call_back,const std::function<std::shared_ptr<inno::LaserScan>()>& get_call_back);
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl{nullptr};
};
