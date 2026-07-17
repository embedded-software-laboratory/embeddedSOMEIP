project(someIp LANGUAGES CXX)

# --- Service Discovery ---
add_subdirectory(someIp/service_discovery)

# --- Tracepoints ---
include_directories(tracepoints/include)
add_subdirectory(tracepoints)

# --- Core library ---
add_library(someIp STATIC
    ${PROJECT_SOURCE_DIR}/someIp/communication/udp/UdpDriver.cpp
    ${PROJECT_SOURCE_DIR}/someIp/communication/tcp/TcpDriver.cpp
    ${PROJECT_SOURCE_DIR}/someIp/communication/LwipUdpTransport.cpp
    ${PROJECT_SOURCE_DIR}/someIp/communication/LwipTcpTransport.cpp
    ${PROJECT_SOURCE_DIR}/someIp/communication/LinuxUdpTransport.cpp
    ${PROJECT_SOURCE_DIR}/someIp/communication/LinuxTcpTransport.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/Header.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/TPHeader.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/Package.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/PackageRx.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/PackageTx.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/MessageId.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/Service.cpp
    ${PROJECT_SOURCE_DIR}/someIp/structs/Method.cpp
    ${PROJECT_SOURCE_DIR}/someIp/ESomeIp.cpp
    ${PROJECT_SOURCE_DIR}/someIp/ThreadPool.cpp
    ${PROJECT_SOURCE_DIR}/someIp/storages/ThreadSafeCircularBuffer.cpp
    ${PROJECT_SOURCE_DIR}/someIp/handler/RxContext.cpp
    ${PROJECT_SOURCE_DIR}/someIp/handler/RxHandler.cpp
    ${PROJECT_SOURCE_DIR}/someIp/handler/Dispatcher.cpp
    ${PROJECT_SOURCE_DIR}/someIp/handler/ServiceHandler.cpp
    ${PROJECT_SOURCE_DIR}/someIp/transport-protocol/Segmenter.cpp
    ${PROJECT_SOURCE_DIR}/someIp/transport-protocol/TpPackage.cpp
    ${PROJECT_SOURCE_DIR}/someIp/transport-protocol/utils/TxSessionManager.cpp
    ${PROJECT_SOURCE_DIR}/someIp/transport-protocol/Reassembler.cpp
    ${PROJECT_SOURCE_DIR}/someIp/utils/Serializer.cpp
    ${PROJECT_SOURCE_DIR}/someIp/utils/Deserializer.cpp
    ${PROJECT_SOURCE_DIR}/someIp/utils/PoolTrace.cpp
)

target_include_directories(someIp PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(someIp  PUBLIC
    lwip
    embedded_some_ip_tp
    someip_sd
)

# When enabled, make_default_populator() (ESomeIp.cpp) registers the POSIX
# LinuxUdpTransport instead of the lwIP UDP/TCP transports. PUBLIC so every
# consumer (the examples) also compiles its own #ifdef SOMEIP_LINUX_UDP_BACKEND
# branches and example IP constants consistently.
if(SOMEIP_LINUX_UDP_BACKEND)
    target_compile_definitions(someIp PUBLIC SOMEIP_LINUX_UDP_BACKEND)
    message(STATUS "someIp transport backend: POSIX UDP sockets (SOMEIP_LINUX_UDP_BACKEND=ON)")
else()
    message(STATUS "someIp transport backend: lwIP netif (default)")
endif()
