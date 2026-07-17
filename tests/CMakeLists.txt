add_subdirectory(thirdparty/googletest)

enable_testing()

set(BINARY_NAME_TEST ${BINARY_NAME}_test)
set(BINARY_NAME_LATENCY_TEST ${BINARY_NAME}_latency_test)

# All test sources except the latency benchmarks (separate binary).
file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/latency/.*")

# Latency benchmark sources.
file(GLOB_RECURSE LATENCY_TEST_SOURCES LIST_DIRECTORIES false latency/*.h latency/*.cpp)

# rooted at tests/ and at the stack source, so test files can move within tests/
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/someIp
)

# Main test executable (unit + integration + e2e).
add_executable(${BINARY_NAME_TEST} ${TEST_SOURCES})
add_test(NAME ${BINARY_NAME_TEST} COMMAND ${BINARY_NAME_TEST})
target_link_libraries(${BINARY_NAME_TEST} PUBLIC gtest gtest_main gmock gmock_main someIp)

# built on demand, not in ctest. TODO ThreadPoolLatency hangs, no set_owner_api()
add_executable(${BINARY_NAME_LATENCY_TEST} EXCLUDE_FROM_ALL ${LATENCY_TEST_SOURCES})
target_link_libraries(${BINARY_NAME_LATENCY_TEST} PUBLIC gtest gtest_main gmock gmock_main someIp)
