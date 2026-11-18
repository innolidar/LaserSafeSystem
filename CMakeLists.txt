
cmake_minimum_required(VERSION 3.16)
project(LaserSafeSystem VERSION 1.0.0 LANGUAGES CXX)

if (CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "---Build type Release---")
endif()

message(STATUS "================ TOOLCHAIN ================")
message(STATUS "CMAKE_GENERATOR       = ${CMAKE_GENERATOR}")
message(STATUS "CMAKE_C_COMPILER     = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER   = ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_SIZEOF_VOID_P  = ${CMAKE_SIZEOF_VOID_P}")
message(STATUS "CMAKE_SYSTEM_NAME    = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "============================================")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(LASER_DRIVER_DIR ${CMAKE_SOURCE_DIR}/third_party/laser_driver)
include_directories(${LASER_DRIVER_DIR})


if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

add_subdirectory(laser_safe_system)