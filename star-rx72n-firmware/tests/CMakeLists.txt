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

# UART mock sources
set(MOCK_UART_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_sci_regs.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_uart_hw.c
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

# rx_hcsr04 source
set(RX_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_hcsr04/src/rx_hcsr04.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_hcsr04/src/rx_hcsr04_hal_mock.c
)

# HC-SR04 mock source
set(MOCK_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_hcsr04_hw.c
)

# Mock GPTW source for motor testing
set(MOCK_GPTW_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_gptw.c
)

# Mock RSPI source for SPI communication testing
set(MOCK_RSPI_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rspi.c
)

# rx_spi_comm source
set(RX_SPI_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_spi_comm/src/rx_spi_comm.c
)

# rx_motor source
set(RX_MOTOR_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_motor/src/rx_motor.c
)

# rx_pid source
set(RX_PID_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_pid/src/rx_pid.c
)

# Mock MTU encoder source for encoder testing
set(MOCK_MTU_ENCODER_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_mtu_encoder.c
)

# rx_encoder source
set(RX_ENCODER_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_encoder/src/rx_mtu_encoder.c
)

# Include directories for library headers
include_directories(
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../lib/rx_frame/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_crc/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_fec/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_harq/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_usb_comm/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_spi_comm/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_hcsr04/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_hcsr04/src
    ${CMAKE_SOURCE_DIR}/../lib/rx_hal/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_motor/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_pid/inc
    ${CMAKE_SOURCE_DIR}/../lib/rx_encoder/inc
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

# Test: HC-SR04 Ultrasonic Sensor
add_executable(test_rx_hcsr04
    test_rx_hcsr04.c
    ${RX_HCSR04_SRC}
    ${MOCK_HCSR04_SRC}
)
target_link_libraries(test_rx_hcsr04 unity)

# Test: UART HAL Driver
add_executable(test_uart_hal
    test_uart_hal.c
    ${MOCK_UART_SRC}
)
target_compile_definitions(test_uart_hal PRIVATE UNIT_TEST)
target_link_libraries(test_uart_hal unity)

# rx_bus sources for bus abstraction tests (using mock bus manager)
set(RX_BUS_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_bus_manager.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/src/rx_bus_config.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/src/rx_bus_uart.c
)

# Test: UART Bus Abstraction Layer
# Note: Don't include MOCK_LOG_SRC - mock_uart_hw.c provides uart stubs
add_executable(test_rx_bus_uart
    test_rx_bus_uart.c
    ${RX_BUS_SRC}
    ${MOCK_UART_SRC}
)
target_include_directories(test_rx_bus_uart PRIVATE
    ${CMAKE_SOURCE_DIR}/mocks
)
target_compile_definitions(test_rx_bus_uart PRIVATE UNIT_TEST)
target_link_libraries(test_rx_bus_uart unity)

# Test: Motor Driver (GPTW-based)
add_executable(test_rx_motor
    test_rx_motor.c
    ${RX_MOTOR_SRC}
    ${MOCK_GPTW_SRC}
    ${MOCK_LOG_SRC}
)
target_compile_definitions(test_rx_motor PRIVATE UNIT_TEST)
target_link_libraries(test_rx_motor unity m)

# =============================================================================
# Bus Manager and OneWire Tests
# =============================================================================

# Bus config source (used by both mock and real implementations)
set(RX_BUS_CONFIG_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/src/rx_bus_config.c
)

# Mock bus manager sources
set(MOCK_BUS_MANAGER_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_bus_manager.c
    ${RX_BUS_CONFIG_SRC}
)

# OneWire mock sources
set(MOCK_ONEWIRE_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_gpio.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_crc.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_log.c
)

# Test: Bus Manager (uses mock implementation for testing)
add_executable(test_rx_bus_manager
    test_rx_bus_manager.c
    ${MOCK_BUS_MANAGER_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_log.c
)
target_include_directories(test_rx_bus_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/mocks
)
target_compile_definitions(test_rx_bus_manager PRIVATE UNIT_TEST)
target_link_libraries(test_rx_bus_manager unity)

# Test: OneWire Bus Protocol
# Note: mocks directory must come FIRST to override hardware headers
add_executable(test_rx_bus_onewire
    test_rx_bus_onewire.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/src/rx_bus_onewire.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_ONEWIRE_SRC}
)
target_include_directories(test_rx_bus_onewire BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/mocks
)
target_compile_definitions(test_rx_bus_onewire PRIVATE UNIT_TEST)
target_link_libraries(test_rx_bus_onewire unity)

# Test: SPI Communication Layer
add_executable(test_rx_spi_comm
    test_rx_spi_comm.c
    ${RX_SPI_COMM_SRC}
    ${RX_FRAME_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_LOG_SRC}
)
target_compile_definitions(test_rx_spi_comm PRIVATE UNIT_TEST)
target_link_libraries(test_rx_spi_comm unity)

# Test: PID Controller
add_executable(test_rx_pid
    test_rx_pid.c
    ${RX_PID_SRC}
    ${MOCK_LOG_SRC}
)
target_compile_definitions(test_rx_pid PRIVATE UNIT_TEST)
target_link_libraries(test_rx_pid unity m)

# Test: MTU Encoder Driver (Quadrature Phase Counting)
# We need to use a wrapper source file that includes mock headers before the encoder
add_executable(test_rx_encoder
    test_rx_encoder.c
    ${MOCK_MTU_ENCODER_SRC}
    ${MOCK_LOG_SRC}
)
# Add mocks directory FIRST so mock headers override hardware headers
target_include_directories(test_rx_encoder BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/mocks
)
target_compile_definitions(test_rx_encoder PRIVATE UNIT_TEST)
target_link_libraries(test_rx_encoder unity)

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
add_test(NAME test_rx_hcsr04 COMMAND test_rx_hcsr04)
add_test(NAME test_uart_hal COMMAND test_uart_hal)
add_test(NAME test_rx_bus_uart COMMAND test_rx_bus_uart)
add_test(NAME test_rx_motor COMMAND test_rx_motor)
add_test(NAME test_rx_bus_manager COMMAND test_rx_bus_manager)
add_test(NAME test_rx_bus_onewire COMMAND test_rx_bus_onewire)
add_test(NAME test_rx_spi_comm COMMAND test_rx_spi_comm)
add_test(NAME test_rx_pid COMMAND test_rx_pid)
add_test(NAME test_rx_encoder COMMAND test_rx_encoder)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_rx_crc32 test_rx_frame test_rx_fec test_rx_harq test_rx_usb test_rx_usb_comm test_rx_time test_rx_hcsr04 test_uart_hal test_rx_bus_uart test_rx_motor test_rx_bus_manager test_rx_bus_onewire test_rx_spi_comm test_rx_pid test_rx_encoder
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
