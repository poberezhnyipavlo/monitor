cmake_minimum_required(VERSION 4.2)
project(monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RRD REQUIRED IMPORTED_TARGET librrd)

add_executable(
        monitor src/main.cpp
        src/metrics/isystem_metrics.h
        src/metrics/metric_factory.cpp
        src/metrics/metric_factory.h
        src/network/socket.h
        src/core/logger.cpp
        src/core/logger.h
        src/metrics/format_metric.h
        src/storage/system_metric_storage.cpp
        src/storage/system_metric_storage.h
        src/storage/rrd_mutex.h
        src/storage/storage_exceptions.h
        src/storage/base_rrd_storage.h
)

if(APPLE)
    target_sources(monitor PRIVATE
            src/metrics/macos_metric.cpp
            src/metrics/macos_metric.h
    )
elseif(UNIX AND NOT APPLE)
    target_sources(monitor PRIVATE
            src/metrics/linux_metric.cpp
            src/metrics/linux_metric.h
    )
endif()

target_include_directories(monitor PRIVATE src)
target_link_libraries(monitor PRIVATE PkgConfig::RRD)
