# soa_tracer SOME/IP nodes (udp / tcp / tp). Built only when SOMEIP_ENABLE_TRACING
# is ON, which the soa_tracer parent sets. The parent build provides the
# `soatracer_tp` target and the tracepoint headers (../../tracepoints/include,
# i.e. soatracer-nodes/tracepoints) — mirroring how embeddedrtps/tracer is wired.
# Standalone embeddedSOMEIP builds leave SOMEIP_ENABLE_TRACING OFF, so these
# parent-provided dependencies are never referenced.

macro(add_someip_tracer_node name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tracepoints/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/someIp/service_discovery
  )
  # lwIP's def.h #defines htons/htonl as macros; combined with the glibc network
  # headers pulled in by the LTTng tracepoint header this hijacks glibc's htonl
  # declaration and clashes. The node never calls htons/ntohs directly, so turn
  # lwIP's user byte-order macros off for these TUs (intended lwIP escape hatch).
  target_compile_definitions(${name} PRIVATE TRACE_PERF=1 TRACE_ALL=1 TRACE_LOG=0
    LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS)
  # The embeddedSOMEIP repo builds the core library with -fno-exceptions, but the
  # tracer nodes use nlohmann::json + try/catch, so re-enable exceptions for these
  # executables (overrides the inherited flag; the someIp lib stays -fno-exceptions).
  target_compile_options(${name} PRIVATE -fexceptions)
  target_link_libraries(${name} PRIVATE someIp pthread soatracer_tp)
endmacro()

add_someip_tracer_node(soatracer-udp-embeddedsomeip udp_embeddedsomeip_node.cpp)
add_someip_tracer_node(soatracer-tcp-embeddedsomeip tcp_embeddedsomeip_node.cpp)
add_someip_tracer_node(soatracer-tp-embeddedsomeip  tp_embeddedsomeip_node.cpp)
