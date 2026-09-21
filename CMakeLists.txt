cmake_minimum_required(VERSION 4.3)
project(monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(RRD_INCLUDE_DIR rrd.h PATHS /opt/homebrew/opt/rrdtool/include)
find_library(RRD_LIBRARY rrd PATHS /opt/homebrew/opt/rrdtool/lib)

add_executable(
        monitor src/main.cpp
        src/metrics/isystem_metrics.h
        src/metrics/macos_metric.cpp
        src/metrics/macos_metric.h
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

target_include_directories(monitor PRIVATE src ${RRD_INCLUDE_DIR})
target_link_libraries(monitor PRIVATE ${RRD_LIBRARY})
