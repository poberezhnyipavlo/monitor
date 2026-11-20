cmake_minimum_required(VERSION 4.2)
project(monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RRD REQUIRED IMPORTED_TARGET librrd)

# LMDB: pkg-config if it knows the library, otherwise search for the header and the library
# (Homebrew's lmdb may come without a pkg-config file).
pkg_check_modules(LMDB QUIET IMPORTED_TARGET lmdb)
if(LMDB_FOUND)
    set(LMDB_TARGET PkgConfig::LMDB)
else()
    find_path(LMDB_INCLUDE_DIR lmdb.h PATHS /opt/homebrew/include /usr/local/include REQUIRED)
    find_library(LMDB_LIBRARY lmdb PATHS /opt/homebrew/lib /usr/local/lib REQUIRED)
    add_library(lmdb_found INTERFACE)
    target_include_directories(lmdb_found INTERFACE ${LMDB_INCLUDE_DIR})
    target_link_libraries(lmdb_found INTERFACE ${LMDB_LIBRARY})
    set(LMDB_TARGET lmdb_found)
endif()

include(FetchContent)
FetchContent_Declare(
        metric_protocol
        GIT_REPOSITORY https://github.com/poberezhnyipavlo/metric-protocol.git
        GIT_TAG v3.0.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(metric_protocol)

add_executable(
        monitor src/main.cpp
        src/metrics/ISystemMetrics.h
        src/metrics/MetricFactory.cpp
        src/metrics/MetricFactory.h
        src/network/Socket.h
        src/core/Logger.cpp
        src/core/Logger.h
        src/metrics/FormatMetric.h
        src/storage/SystemMetricStorage.cpp
        src/storage/SystemMetricStorage.h
        src/storage/RrdMutex.h
        src/storage/StorageExceptions.h
        src/storage/BaseRrdStorage.h
        src/network/ILineReceiver.h
        src/network/LineSplitter.cpp
        src/network/TcpLineReceiver.h
        src/network/TcpLineReceiver.cpp
        src/network/UdpLineReceiver.cpp
        src/network/UdpLineReceiver.h
        src/ingest/LinePrinters.cpp
        src/ingest/LinePrinters.h
        src/storage/lmdb/Lmdb.cpp
        src/storage/lmdb/Lmdb.h
)

if(APPLE)
    target_sources(monitor PRIVATE
            src/metrics/MacOSMetric.cpp
            src/metrics/MacOSMetric.h
    )
elseif(UNIX AND NOT APPLE)
    target_sources(monitor PRIVATE
            src/metrics/LinuxMetric.cpp
            src/metrics/LinuxMetric.h
    )
endif()

# Node data storage: a library, so the tests can link it without main.cpp.
add_library(
        node_storage STATIC
        src/storage/lmdb/Lmdb.cpp
        src/storage/lmdb/Lmdb.h
)
target_include_directories(node_storage PUBLIC src)
target_link_libraries(node_storage PUBLIC ${LMDB_TARGET} metric_protocol::metric_protocol)

target_include_directories(monitor PRIVATE src)
target_link_libraries(monitor PRIVATE PkgConfig::RRD node_storage)
