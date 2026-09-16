cmake_minimum_required(VERSION 4.3)
project(monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
        monitor src/main.cpp
        src/metrics/isystem_metrics.h
        src/metrics/macos_metric.cpp
        src/metrics/macos_metric.h
        src/metrics/metric_factory.cpp
        src/metrics/metric_factory.h
)

target_include_directories(monitor PRIVATE src)
