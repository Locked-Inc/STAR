# =============================================================================
# STAR RX72N Firmware - Unit Tests
# Host-compiled tests using Unity framework
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(star_rx72n_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Unity test framework (fetched from GitHub)
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(unity)

# =============================================================================
# Library Sources (copied for host compilation)
# =============================================================================

# Force software CRC for host testing (hardware CRC not available)
add_compile_definitions(RX_CRC32_USE_SOFTWARE)

# rx_crc32 source (public API + software implementation)
set(RX_CRC32_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_crc32.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_crc32_sw.c
)

# rx_frame source
set(RX_FRAME_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_frame.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_crc32.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_crc32_sw.c
)

# rx_fec source
set(RX_FEC_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_fec/src/rx_fec.c
)

# rx_harq source
set(RX_HARQ_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_harq/src/rx_harq.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_fec/src/rx_fec.c
)

# Include directories for library headers
include_directories(
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_fec/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_harq/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/inc
    ${unity_SOURCE_DIR}/src
)

# =============================================================================
# Test Executables
# =============================================================================

# Test: CRC-32
add_executable(test_rx_crc32
    test_rx_crc32.c
    ${RX_CRC32_SRC}
)
target_link_libraries(test_rx_crc32 unity)

# Test: Frame Layer
add_executable(test_rx_frame
    test_rx_frame.c
    ${RX_FRAME_SRC}
)
target_link_libraries(test_rx_frame unity)

# Test: FEC Codec
add_executable(test_rx_fec
    test_rx_fec.c
    ${RX_FEC_SRC}
)
target_link_libraries(test_rx_fec unity)

# Test: HARQ Protocol
add_executable(test_rx_harq
    test_rx_harq.c
    ${RX_HARQ_SRC}
)
target_link_libraries(test_rx_harq unity)

# =============================================================================
# CTest Integration
# =============================================================================

enable_testing()

add_test(NAME test_rx_crc32 COMMAND test_rx_crc32)
add_test(NAME test_rx_frame COMMAND test_rx_frame)
add_test(NAME test_rx_fec COMMAND test_rx_fec)
add_test(NAME test_rx_harq COMMAND test_rx_harq)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_rx_crc32 test_rx_frame test_rx_fec test_rx_harq
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
