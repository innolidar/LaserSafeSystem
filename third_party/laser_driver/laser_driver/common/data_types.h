#pragma once
#include "common.h"
#ifdef LIDARDRIVER_EXPORTS
#define LIDARDRIVER_API __declspec(dllexport)
#else
#define LIDARDRIVER_API __declspec(dllimport)
#endif

enum class LidarType:int
{
  HA25=0,
  HA50=1,
};
enum class InputType:int
{
  Online=0,
  Pcap=1,
};
struct NetworkParam
{
    unsigned int port{0};
    std::string ip{""};
};
    
struct InputParam
{
    NetworkParam  pc_network_param{};
    NetworkParam  lidar_network_param{};
    std::string   pcap_file{""};
}; 

struct DecoderParam
{
  bool is_filter{false};
};
struct DeviceParam
{
  LidarType     lidar_type{LidarType::HA25};
  InputType     input_type{InputType::Online};
  InputParam    input_param{};
  DecoderParam  decoder_param{};
};