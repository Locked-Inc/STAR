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

# rx_crc source (public API + software implementation)
set(RX_CRC_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_crc/src/rx_crc32.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_crc/src/rx_crc32_sw.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_crc/src/rx_crc8.c
)

# rx_frame source
set(RX_FRAME_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/src/rx_frame.c
    ${RX_CRC_SRC}
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

# USB mock sources
set(MOCK_USB_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_usb_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_usb0_regs.c
)

# Log mock source
set(MOCK_LOG_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_log.c
)

# Time mock source
set(MOCK_TIME_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/src/mock_time.c
)

# rx_usb source (only rx_usb.c, hardware mocked)
set(RX_USB_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb/src/rx_usb.c
)

# rx_usb_comm source
set(RX_USB_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb_comm/src/rx_usb_comm.c
)

# Include directories for library headers
include_directories(
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_crc/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_fec/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_harq/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb_comm/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_ds18b20/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/inc
    ${CMAKE_SOURCE_DIR}/mocks
    ${unity_SOURCE_DIR}/src
)

# =============================================================================
# Test Executables
# =============================================================================

# Test: CRC-32
add_executable(test_rx_crc32
    test_rx_crc32.c
    ${RX_CRC_SRC}
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

# Test: USB CDC Driver
add_executable(test_rx_usb
    test_rx_usb.c
    ${RX_USB_SRC}
    ${MOCK_USB_SRC}
)
target_compile_definitions(test_rx_usb PRIVATE UNIT_TEST)
target_link_libraries(test_rx_usb unity)

# Test: USB Communication Layer
add_executable(test_rx_usb_comm
    test_rx_usb_comm.c
    ${RX_USB_COMM_SRC}
    ${RX_USB_SRC}
    ${RX_FRAME_SRC}
    ${MOCK_USB_SRC}
    ${MOCK_LOG_SRC}
    ${MOCK_TIME_SRC}
)
target_compile_definitions(test_rx_usb_comm PRIVATE UNIT_TEST)
target_link_libraries(test_rx_usb_comm unity)

# Test: Time Interface
add_executable(test_rx_time
    test_rx_time.c
    ${MOCK_TIME_SRC}
)
target_link_libraries(test_rx_time unity)

# Test: DS18B20 Temperature Sensor
add_executable(test_rx_ds18b20
    test_rx_ds18b20.c
    ${RX_CRC_SRC}
)
target_link_libraries(test_rx_ds18b20 unity)

# =============================================================================
# CTest Integration
# =============================================================================

enable_testing()

add_test(NAME test_rx_crc32 COMMAND test_rx_crc32)
add_test(NAME test_rx_frame COMMAND test_rx_frame)
add_test(NAME test_rx_fec COMMAND test_rx_fec)
add_test(NAME test_rx_harq COMMAND test_rx_harq)
add_test(NAME test_rx_usb COMMAND test_rx_usb)
add_test(NAME test_rx_usb_comm COMMAND test_rx_usb_comm)
add_test(NAME test_rx_time COMMAND test_rx_time)
add_test(NAME test_rx_ds18b20 COMMAND test_rx_ds18b20)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_rx_crc32 test_rx_frame test_rx_fec test_rx_harq test_rx_usb test_rx_usb_comm test_rx_time test_rx_ds18b20
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
