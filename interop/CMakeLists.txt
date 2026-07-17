
cmake_minimum_required(VERSION 3.15)
project(someip_interop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# embeddedSOMEIP tool (links the someIp lib built from the repo root)
# ---------------------------------------------------------------------------
set(SOMEIP_BUILD_APPS OFF CACHE BOOL "" FORCE)         # no examples/evaluation
set(SOMEIP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SOMEIP_ENABLE_TRACING OFF CACHE BOOL "" FORCE)

# interop/ lives inside the embeddedSOMEIP repo, so the stack is one level up
set(ESOMEIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${ESOMEIP_DIR} ${CMAKE_BINARY_DIR}/embeddedsomeip)

# someIp is built -fno-exceptions, these tools need exceptions for the STL
add_executable(embeddedsomeip_interop embeddedsomeip_interop.cpp)
target_compile_options(embeddedsomeip_interop PRIVATE -fexceptions)
target_link_libraries(embeddedsomeip_interop PRIVATE someIp pthread)

# request-response variant (same wire stack, REQUEST/RESPONSE instead of events)
add_executable(embeddedsomeip_rr embeddedsomeip_rr.cpp)
target_compile_options(embeddedsomeip_rr PRIVATE -fexceptions)
target_link_libraries(embeddedsomeip_rr PRIVATE someIp pthread)

# ---------------------------------------------------------------------------
# vsomeip tool (reference peer, no vsomeip modifications)
# ---------------------------------------------------------------------------
find_package(vsomeip3 QUIET)
if(vsomeip3_FOUND)
  add_executable(vsomeip_interop vsomeip_interop.cpp)
  target_link_libraries(vsomeip_interop PRIVATE vsomeip3 pthread)
  add_executable(vsomeip_rr vsomeip_rr.cpp)
  target_link_libraries(vsomeip_rr PRIVATE vsomeip3 pthread)
  message(STATUS "vsomeip3 found; building vsomeip_interop + vsomeip_rr")
else()
  message(WARNING "vsomeip3 not found (set CMAKE_PREFIX_PATH=/home/davidk/embedded/vsomeip-install); skipping vsomeip_interop")
endif()
