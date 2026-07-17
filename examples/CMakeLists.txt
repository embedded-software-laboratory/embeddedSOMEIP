# Host (POSIX) examples, all lwIP-free (BSD-socket backend).
#
#   D#_*  functional demo pairs (client + server) that run on loopback, one per
#         feature: D1 request/response, D2 request/response over SD, D3
#         field/event group, D4 fire-and-forget echo.
#   T#_*  desktop latency clients that measure RTT against the STM32 server
#         (one per SOMEIP_TESTCASE) and write a CSV.

function(add_example TARGET_NAME SOURCE_FILE)
  add_executable(${TARGET_NAME} ${SOURCE_FILE})
  target_link_libraries(${TARGET_NAME} PRIVATE someIp Threads::Threads)
  target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
  )
endfunction()

# --- functional demo pairs (loopback) --------------------------------------
# D1/D2/D4 still use the pre-rename API (eSomeIP, sd_service_info, ...) and do
# not compile against the current library; re-enable once ported.
#add_example(D1_rr_server D1_rr_server.cpp)
#add_example(D1_rr_client D1_rr_client.cpp)
#add_example(D2_sd_rr_server D2_sd_rr_server.cpp)
#add_example(D2_sd_rr_client D2_sd_rr_client.cpp)
add_example(D3_field_server D3_field_server.cpp)
add_example(D3_field_client D3_field_client.cpp)
#add_example(D4_ff_server D4_ff_server.cpp)
#add_example(D4_ff_client D4_ff_client.cpp)

# --- desktop latency clients (test cases 1-5) ------------------------------
# Transport is a single compile-time choice; it must match the transport the
# STM32 server was flashed with (SOMEIP_TRANSPORT).
set(SWEEP_TRANSPORT "UDP" CACHE STRING "Transport the latency clients use: UDP or TCP")
set_property(CACHE SWEEP_TRANSPORT PROPERTY STRINGS UDP TCP)
if(SWEEP_TRANSPORT STREQUAL "TCP")
  set(SWEEP_TRANSPORT_VALUE 2)
else()
  set(SWEEP_TRANSPORT_VALUE 1) # UDP
endif()

function(add_latency_example TARGET_NAME SOURCE_FILE)
  add_example(${TARGET_NAME} ${SOURCE_FILE})
  target_compile_definitions(${TARGET_NAME} PRIVATE SWEEP_TRANSPORT=${SWEEP_TRANSPORT_VALUE})
endfunction()

add_latency_example(T1_rr_latency T1_rr_latency.cpp)
add_latency_example(T2_sd_ttfm T2_sd_ttfm.cpp)
add_latency_example(T3_notify_rr T3_notify_rr.cpp)
add_latency_example(T4_sd_ttfn T4_sd_ttfn.cpp)
add_latency_example(T5_tp_rtt T5_tp_rtt.cpp)

# --- plain-UDP baselines (no SOME/IP) --------------------------------------
# Raw-socket clients that drive the STM32 plain-UDP echo image (UDP_BASELINE=1)
# to establish the network+stack latency floor for T1 (RR) and T3 (notify).
# Always UDP, so no SWEEP_TRANSPORT define is needed (plain add_example).
add_example(B1_udp_rr     B1_udp_rr.cpp)
add_example(B3_udp_notify B3_udp_notify.cpp)
