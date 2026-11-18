#pragma once
// Windows 平台需要显式声明导入导出
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #if defined(INNODRIVER_EXPORTS) || defined(INNO_LIDAR_SDK_EXPORTS)
        #define INNODRIVER_API __declspec(dllexport)
    #else
        #define INNODRIVER_API __declspec(dllimport)
    #endif
#else
    // Linux/macOS 默认符号可见
    #define INNODRIVER_API __attribute__((visibility("default")))
#endif