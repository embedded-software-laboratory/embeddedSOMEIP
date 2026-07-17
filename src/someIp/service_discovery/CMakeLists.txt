project(someip_sd LANGUAGES CXX)

file(GLOB_RECURSE SD_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(someip_sd STATIC ${SD_SRC})

target_include_directories(someip_sd PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../structs
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tracepoints/include
)

target_link_libraries(someip_sd PUBLIC
    lwip
    connector
    someIp
    embedded_some_ip_tp
)

# --- Optional: Enable debug flags when needed ---
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(someip_sd PRIVATE SD_DEBUG=1)
endif()

# --- Optional: Add examples ---
# add_subdirectory(examples)
# add_executable(sd_send examples/send.cpp)
# target_link_libraries(sd_send PRIVATE someip_sd)

