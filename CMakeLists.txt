cmake_minimum_required(VERSION 3.15)

# -------------------------------------------------------
# Project setup
#
# Host (POSIX) build of the EmbeddedSOMEIP library. This build is lwIP-free:
# the transport layer uses BSD sockets (LinuxUdpTransport / LinuxTcpTransport).
# The lwIP backend exists only on the STM32 target, which is built by the
# STM32CubeIDE project (see the .cproject), NOT by this CMakeLists. Therefore
# SOMEIP_PLATFORM_STM32 is intentionally never defined here.
# -------------------------------------------------------
set(BINARY_NAME embeddedSomeIP)
project(${BINARY_NAME} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Recommended compiler warnings and options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -Wpedantic -fno-non-call-exceptions -fno-exceptions -fPIC")

find_package(Threads REQUIRED)

# -------------------------------------------------------
# Core library source set (lwIP-free)
#
# The STM32-only lwIP files are excluded:
#   communication/udp/UdpDriver.cpp
#   communication/tcp/TcpDriver.cpp
#   communication/LwipUdpTransport.cpp
#   communication/LwipTcpTransport.cpp
# The POSIX transports (LinuxUdpTransport / LinuxTcpTransport) are included.
# -------------------------------------------------------
set(SOMEIP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/someIp")

set(SOMEIP_CORE_SOURCES
    ${SOMEIP_SRC_DIR}/communication/LinuxUdpTransport.cpp
    ${SOMEIP_SRC_DIR}/communication/LinuxTcpTransport.cpp
    ${SOMEIP_SRC_DIR}/structs/Header.cpp
    ${SOMEIP_SRC_DIR}/structs/TPHeader.cpp
    ${SOMEIP_SRC_DIR}/structs/Package.cpp
    ${SOMEIP_SRC_DIR}/structs/PackageRx.cpp
    ${SOMEIP_SRC_DIR}/structs/PackageTx.cpp
    ${SOMEIP_SRC_DIR}/structs/MessageId.cpp
    ${SOMEIP_SRC_DIR}/structs/Service.cpp
    ${SOMEIP_SRC_DIR}/structs/Method.cpp
    ${SOMEIP_SRC_DIR}/ESomeIp.cpp
    ${SOMEIP_SRC_DIR}/ThreadPool.cpp
    ${SOMEIP_SRC_DIR}/storages/ThreadSafeCircularBuffer.cpp
    ${SOMEIP_SRC_DIR}/handler/RxContext.cpp
    ${SOMEIP_SRC_DIR}/handler/RxHandler.cpp
    ${SOMEIP_SRC_DIR}/handler/Dispatcher.cpp
    ${SOMEIP_SRC_DIR}/handler/ServiceHandler.cpp
    ${SOMEIP_SRC_DIR}/transport-protocol/Segmenter.cpp
    ${SOMEIP_SRC_DIR}/transport-protocol/TpPackage.cpp
    ${SOMEIP_SRC_DIR}/transport-protocol/utils/TxSessionManager.cpp
    ${SOMEIP_SRC_DIR}/transport-protocol/Reassembler.cpp
    ${SOMEIP_SRC_DIR}/utils/PoolTrace.cpp
    ${SOMEIP_SRC_DIR}/utils/Serializer.cpp
    ${SOMEIP_SRC_DIR}/utils/Deserializer.cpp
)

# Service Discovery sources (all lwIP-free after platform gating)
file(GLOB_RECURSE SOMEIP_SD_SOURCES
    "${SOMEIP_SRC_DIR}/service_discovery/*.cpp"
)

add_library(someIp STATIC
    ${SOMEIP_CORE_SOURCES}
    ${SOMEIP_SD_SOURCES}
)

target_include_directories(someIp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/someIp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/someIp/structs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/someIp/service_discovery
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tracepoints/include
)

target_link_libraries(someIp PUBLIC Threads::Threads)

# -------------------------------------------------------
# Examples (host smoke test)
# -------------------------------------------------------
option(SOMEIP_BUILD_APPS "Build the example executables" ON)
if(SOMEIP_BUILD_APPS)
    add_subdirectory(examples)
endif()

# -------------------------------------------------------
# Tests
# -------------------------------------------------------
option(SOMEIP_BUILD_TESTS "Build the unit/integration tests" ON)
if(SOMEIP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# -------------------------------------------------------
# soa_tracer SOME/IP nodes (added last so the someIp target exists)
# -------------------------------------------------------
option(SOMEIP_ENABLE_TRACING "Build the soa_tracer SOME/IP nodes (needs the parent's soatracer_tp)" OFF)
if(SOMEIP_ENABLE_TRACING)
    add_subdirectory(tracer)
endif()
