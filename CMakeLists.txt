cmake_minimum_required(VERSION 4.2)
project(monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RRD REQUIRED IMPORTED_TARGET librrd)

add_executable(
        monitor src/main.cpp
        src/metrics/ISystemMetrics.h
        src/metrics/MetricFactory.cpp
        src/metrics/MetricFactory.h
        src/network/socket.h
        src/core/logger.cpp
        src/core/logger.h
        src/metrics/FormatMetric.h
        src/storage/SystemMetricStorage.cpp
        src/storage/SystemMetricStorage.h
        src/storage/RrdMutex.h
        src/storage/StorageExceptions.h
        src/storage/BaseRrdStorage.h
)

if(APPLE)
    target_sources(monitor PRIVATE
            src/metrics/MacosMetric.cpp
            src/metrics/MacosMetric.h
    )
elseif(UNIX AND NOT APPLE)
    target_sources(monitor PRIVATE
            src/metrics/LinuxMetric.cpp
            src/metrics/LinuxMetric.h
    )
endif()

target_include_directories(monitor PRIVATE src)
target_link_libraries(monitor PRIVATE PkgConfig::RRD)
