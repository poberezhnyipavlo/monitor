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
        GIT_TAG v3.1.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(metric_protocol)

# gRPC + protobuf: brew install grpc (Mac) / libgrpc++-dev protobuf-compiler-grpc (Ubuntu).
# protobuf: its own CMake config if installed (Homebrew), otherwise CMake's FindProtobuf (Ubuntu).
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(gRPC CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# monitor.proto -> C++ code in build/generated: monitor.pb.{h,cc} (messages, protoc) and
# monitor.grpc.pb.{h,cc} (service, the gRPC plugin). Regenerated when the .proto changes.
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_FILE ${PROTO_DIR}/monitor/v1/monitor.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(GENERATED_FILES
        ${GENERATED_DIR}/monitor/v1/monitor.pb.cc
        ${GENERATED_DIR}/monitor/v1/monitor.pb.h
        ${GENERATED_DIR}/monitor/v1/monitor.grpc.pb.cc
        ${GENERATED_DIR}/monitor/v1/monitor.grpc.pb.h
)
add_custom_command(
        OUTPUT ${GENERATED_FILES}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
        COMMAND protobuf::protoc
        --proto_path=${PROTO_DIR}
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "protoc: monitor.proto"
)
add_library(monitor_proto STATIC ${GENERATED_FILES})
target_include_directories(monitor_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(monitor_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

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
        src/ingest/NodeIngest.cpp
        src/ingest/NodeIngest.h
        src/storage/RrdSeriesStore.cpp
        src/storage/RrdSeriesStore.h
        src/api/MonitorService.cpp
        src/api/MonitorService.h
        src/api/LiveFeed.cpp
        src/api/LiveFeed.h
        src/security/HmacSha256.cpp
        src/security/HmacSha256.h
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
        src/storage/NodeKeys.cpp
        src/storage/NodeKeys.h
        src/storage/LmdbStore.cpp
        src/storage/LmdbStore.h
        src/storage/ISeriesStore.h
)
target_include_directories(node_storage PUBLIC src)
target_link_libraries(node_storage PUBLIC ${LMDB_TARGET} metric_protocol::metric_protocol)

target_include_directories(monitor PRIVATE src)
target_link_libraries(monitor PRIVATE PkgConfig::RRD node_storage monitor_proto gRPC::grpc++_reflection OpenSSL::Crypto)

option(MONITOR_BUILD_TESTS "Build monitor tests" ON)
if(MONITOR_BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(monitor_tests
            test/NodeKeysTest.cpp
            test/LmdbStoreTest.cpp
            test/LiveFeedTest.cpp
            src/api/LiveFeed.cpp
            test/HmacSha256Test.cpp
            src/security/HmacSha256.cpp
            src/security/HmacSha256.h
    )
    target_link_libraries(monitor_tests PRIVATE node_storage GTest::gtest_main OpenSSL::Crypto)

    include(GoogleTest)
    gtest_discover_tests(monitor_tests)
endif()
