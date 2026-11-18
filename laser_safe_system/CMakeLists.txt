cmake_minimum_required(VERSION 3.14)
project(LaserSafeSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

# ---------------------------------------------------------------------------
# Qt 查找（支持 Qt5 与 Qt6）
# ---------------------------------------------------------------------------
# 先尝试 Qt6，失败则退回 Qt5
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_VERSION ${Qt6_VERSION})
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
    set(QT_VERSION_MAJOR 5)
    set(QT_VERSION ${Qt5_VERSION})
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
endif()

message(STATUS "Using Qt version: ${QT_VERSION}")

# 修复部分 CMake 版本中 AUTOMOC 无法找到 Qt5::moc 目标的问题
if(NOT TARGET Qt${QT_VERSION_MAJOR}::moc)
    # 尝试从 Qt 包目录中定位 moc 工具并创建 imported target
    find_program(QT_MOC_EXECUTABLE NAMES moc moc.exe
        HINTS "${QT_DIR}/../../../bin" "${QT_DIR}/../../bin"
        NO_DEFAULT_PATH)
    if(NOT QT_MOC_EXECUTABLE)
        find_program(QT_MOC_EXECUTABLE NAMES moc moc.exe)
    endif()
    if(QT_MOC_EXECUTABLE)
        add_executable(Qt${QT_VERSION_MAJOR}::moc IMPORTED)
        set_target_properties(Qt${QT_VERSION_MAJOR}::moc PROPERTIES
            IMPORTED_LOCATION "${QT_MOC_EXECUTABLE}")
        message(STATUS "Found moc: ${QT_MOC_EXECUTABLE}")
    endif()
endif()

# ---------------------------------------------------------------------------
# 源文件
# ---------------------------------------------------------------------------
set(SOURCES
    main.cpp
    MainWindow.cpp
    BankConfigPanel.cpp
    ZoneCanvas.cpp
    #LidarDisplayWidget.cpp
    ProtectionManager.cpp
    ProtectionDetector.cpp
    ProtectionZone.cpp
    LidarDriver.cpp
    control_widget.cpp
    device_param_dialog.cpp
)

set(HEADERS
    MainWindow.h
    BankConfigPanel.h
    ZoneCanvas.h
    #LidarDisplayWidget.h
    ProtectionManager.h
    ProtectionDetector.h
    ProtectionZone.h
    GeoUtils.h
    LidarDriver.h
    control_widget.h
    device_param_dialog.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 链接的是静态库 liblaser_driver.a，需要定义 INNODRIVER_EXPORTS，
# 否则 INNODRIVER_API 会被展开为 __declspec(dllimport)，
# 导致链接器查找带 __imp_ 前缀的符号而失败。
target_compile_definitions(${PROJECT_NAME} PRIVATE INNODRIVER_EXPORTS)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${LASER_DRIVER_DIR}/lib/win/x64/liblaser_driver.a
        ${LASER_DRIVER_DIR}/lib/win/x64/libwpcap.a
        ws2_32
    )
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${LASER_DRIVER_DIR}/lib/linux/x86_64/liblaser_driver.a
        pcap
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_LIBRARIES})
# ---------------------------------------------------------------------------
# 平台相关配置
# ---------------------------------------------------------------------------
if(WIN32)
    # 所有模式下均隐藏控制台窗口
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON)
else()
    # Linux 下无需特殊处理
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin)
