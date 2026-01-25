# =============================================================================
# STAR RX72N Firmware - Unit Tests
# Host-compiled tests using Unity framework
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(star_rx72n_tests C)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    set(CMAKE_C_STANDARD 23)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

# Helper function to apply strict warnings/defines to our test targets only
function(apply_strict_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fshort-enums)
    endif()
    target_compile_definitions(${target} PRIVATE USE_MOCK_SCI_REGS UNIT_TEST)
endfunction()

# Helper function to apply common include paths per target
function(apply_test_includes target)
    target_include_directories(${target} BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/mocks
    )
    target_include_directories(${target} PRIVATE
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
        ${CMAKE_SOURCE_DIR}/../lib/rx_obstacle_detect/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_hal/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_bus/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_motor/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_pid/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_encoder/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_bq4050/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc
        ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen
        ${nanopb_SOURCE_DIR}
        ${unity_SOURCE_DIR}/src
    )
endfunction()

# Unity test framework (fetched from GitHub)
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.1
)
FetchContent_MakeAvailable(unity)

# nanopb library (fetched from GitHub for host testing)
FetchContent_Declare(
    nanopb
    GIT_REPOSITORY https://github.com/nanopb/nanopb.git
    GIT_TAG 0.4.9.1
)
FetchContent_MakeAvailable(nanopb)

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

# rx_obstacle_detect source
set(RX_OBSTACLE_DETECT_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_obstacle_detect/src/rx_obstacle_detect.c
)

# Mock motor source for obstacle detection testing
set(MOCK_MOTOR_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_motor.c
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

# Mock HAL sources for GPIO/ADC/RIIC testing
set(MOCK_HAL_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_gpio_hal.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_adc_hal.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_riic_hal.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_hal_regs.c
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

# Mock SMBus source for BQ4050 testing
set(MOCK_SMBUS_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_bus_smbus.c
)

# rx_bq4050 source
set(RX_BQ4050_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_bq4050/src/rx_bq4050.c
)

# rx_core sources for infrastructure testing
set(RX_ERROR_HANDLER_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/src/rx_error_handler.c
)

set(RX_PIN_VALIDATOR_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/src/rx_pin_validator.c
)

set(RX_REGISTER_GUARD_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_core/src/rx_register_guard.c
)

# rx_nanopb source (wrapper + generated protobuf files)
set(RX_NANOPB_SRC
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/src/rx_nanopb.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen/google/protobuf/timestamp.pb.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen/google/protobuf/duration.pb.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen/star/v1/common.pb.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen/star/v1/motor_control.pb.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_nanopb/inc/gen/star/v1/telemetry.pb.c
)

# Include directories are applied per target via apply_test_includes()

# =============================================================================
# Test Executables
# =============================================================================

# Test: CRC-32
add_executable(test_rx_crc32
    test_rx_crc32.c
    ${RX_CRC_SRC}
)
apply_strict_warnings(test_rx_crc32)
apply_test_includes(test_rx_crc32)
target_link_libraries(test_rx_crc32 unity)

# Test: CRC-8 (Dallas/Maxim for OneWire)
add_executable(test_rx_crc8
    test_rx_crc8.c
    ${RX_CRC_SRC}
)
apply_strict_warnings(test_rx_crc8)
apply_test_includes(test_rx_crc8)
target_link_libraries(test_rx_crc8 unity)

# Test: Frame Layer
add_executable(test_rx_frame
    test_rx_frame.c
    ${RX_FRAME_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_frame)
apply_test_includes(test_rx_frame)
target_link_libraries(test_rx_frame unity)

# Test: FEC Codec
add_executable(test_rx_fec
    test_rx_fec.c
    ${RX_FEC_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_fec)
apply_test_includes(test_rx_fec)
target_link_libraries(test_rx_fec unity)

# Test: HARQ Protocol
add_executable(test_rx_harq
    test_rx_harq.c
    ${RX_HARQ_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_harq)
apply_test_includes(test_rx_harq)
target_link_libraries(test_rx_harq unity)

# Test: USB CDC Driver
add_executable(test_rx_usb
    test_rx_usb.c
    ${RX_USB_SRC}
    ${MOCK_USB_SRC}
)
apply_strict_warnings(test_rx_usb)
apply_test_includes(test_rx_usb)
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
apply_strict_warnings(test_rx_usb_comm)
apply_test_includes(test_rx_usb_comm)
target_link_libraries(test_rx_usb_comm unity)

# Test: Time Interface
add_executable(test_rx_time
    test_rx_time.c
    ${MOCK_TIME_SRC}
)
apply_strict_warnings(test_rx_time)
apply_test_includes(test_rx_time)
target_link_libraries(test_rx_time unity)

# Test: HC-SR04 Ultrasonic Sensor
add_executable(test_rx_hcsr04
    test_rx_hcsr04.c
    ${RX_HCSR04_SRC}
    ${MOCK_HCSR04_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_hcsr04)
apply_test_includes(test_rx_hcsr04)
target_link_libraries(test_rx_hcsr04 unity)

# Test: Obstacle Detection
add_executable(test_rx_obstacle_detect
    test_rx_obstacle_detect.c
    ${RX_OBSTACLE_DETECT_SRC}
    ${RX_HCSR04_SRC}
    ${MOCK_HCSR04_SRC}
    ${MOCK_MOTOR_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_obstacle_detect)
apply_test_includes(test_rx_obstacle_detect)
target_link_libraries(test_rx_obstacle_detect unity)

# Test: UART HAL Driver
add_executable(test_uart_hal
    test_uart_hal.c
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_uart_hal)
apply_test_includes(test_uart_hal)
target_link_libraries(test_uart_hal unity)

# Test: GPIO HAL Driver
add_executable(test_gpio_hal
    test_gpio_hal.c
    ${MOCK_HAL_SRC}
)
apply_strict_warnings(test_gpio_hal)
apply_test_includes(test_gpio_hal)
target_link_libraries(test_gpio_hal unity)

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
apply_strict_warnings(test_rx_bus_uart)
apply_test_includes(test_rx_bus_uart)
target_link_libraries(test_rx_bus_uart unity)

# Test: Motor Driver (GPTW-based)
add_executable(test_rx_motor
    test_rx_motor.c
    ${RX_MOTOR_SRC}
    ${MOCK_GPTW_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_motor)
apply_test_includes(test_rx_motor)
target_link_libraries(test_rx_motor unity m)

# Test: GPTW Staggered Init API
add_executable(test_rx_gptw_staggered
    test_rx_gptw_staggered.c
    ${MOCK_GPTW_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_gptw_staggered)
apply_test_includes(test_rx_gptw_staggered)
target_link_libraries(test_rx_gptw_staggered unity m)

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
apply_strict_warnings(test_rx_bus_manager)
apply_test_includes(test_rx_bus_manager)
target_link_libraries(test_rx_bus_manager unity)

# Test: OneWire Bus Protocol
# Note: mocks directory must come FIRST to override hardware headers
add_executable(test_rx_bus_onewire
    test_rx_bus_onewire.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_bus/src/rx_bus_onewire.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_ONEWIRE_SRC}
)
apply_strict_warnings(test_rx_bus_onewire)
apply_test_includes(test_rx_bus_onewire)
target_link_libraries(test_rx_bus_onewire unity)

# Test: SPI Communication Layer
add_executable(test_rx_spi_comm
    test_rx_spi_comm.c
    ${RX_SPI_COMM_SRC}
    ${RX_FRAME_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_spi_comm)
apply_test_includes(test_rx_spi_comm)
target_link_libraries(test_rx_spi_comm unity)

# Test: PID Controller
add_executable(test_rx_pid
    test_rx_pid.c
    ${RX_PID_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_pid)
apply_test_includes(test_rx_pid)
target_link_libraries(test_rx_pid unity m)

# Test: MTU Encoder Driver (Quadrature Phase Counting)
# We need to use a wrapper source file that includes mock headers before the encoder
add_executable(test_rx_encoder
    test_rx_encoder.c
    ${MOCK_MTU_ENCODER_SRC}
    ${MOCK_LOG_SRC}
)
# Add mocks directory FIRST so mock headers override hardware headers
apply_strict_warnings(test_rx_encoder)
apply_test_includes(test_rx_encoder)
target_link_libraries(test_rx_encoder unity)

# Test: BQ4050 Battery Fuel Gauge Driver
add_executable(test_rx_bq4050
    test_rx_bq4050.c
    ${RX_BQ4050_SRC}
    ${MOCK_SMBUS_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bq4050)
apply_test_includes(test_rx_bq4050)
target_link_libraries(test_rx_bq4050 unity)

# Test: DS18B20 Temperature Sensor
add_executable(test_rx_ds18b20
    test_rx_ds18b20.c
    ${CMAKE_SOURCE_DIR}/../lib/rx_ds18b20/src/rx_ds18b20.c
    ${RX_CRC_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_ds18b20)
apply_test_includes(test_rx_ds18b20)
target_include_directories(test_rx_ds18b20 PRIVATE
    ${CMAKE_SOURCE_DIR}/../lib/rx_ds18b20/inc
)
target_link_libraries(test_rx_ds18b20 unity)

# Test: Error Handler (rx_core)
add_executable(test_rx_error_handler
    test_rx_error_handler.c
    ${RX_ERROR_HANDLER_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_error_handler)
apply_test_includes(test_rx_error_handler)
target_link_libraries(test_rx_error_handler unity)

# Test: Pin Validator (rx_core)
add_executable(test_rx_pin_validator
    test_rx_pin_validator.c
    ${RX_PIN_VALIDATOR_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_pin_validator)
apply_test_includes(test_rx_pin_validator)
target_link_libraries(test_rx_pin_validator unity)

# Test: Register Guard (rx_core)
add_executable(test_rx_register_guard
    test_rx_register_guard.c
    ${RX_REGISTER_GUARD_SRC}
)
apply_strict_warnings(test_rx_register_guard)
apply_test_includes(test_rx_register_guard)
target_link_libraries(test_rx_register_guard unity)

# Test: nanopb Protocol Buffer Integration
add_executable(test_rx_nanopb
    test_rx_nanopb.c
    ${RX_NANOPB_SRC}
    ${nanopb_SOURCE_DIR}/pb_common.c
    ${nanopb_SOURCE_DIR}/pb_encode.c
    ${nanopb_SOURCE_DIR}/pb_decode.c
)
apply_strict_warnings(test_rx_nanopb)
apply_test_includes(test_rx_nanopb)
target_compile_definitions(test_rx_nanopb PRIVATE
    PB_CONVERT_DOUBLE_FLOAT=1
)
# Suppress nanopb-generated warnings
target_compile_options(test_rx_nanopb PRIVATE
    -Wno-missing-braces
)
target_link_libraries(test_rx_nanopb unity m)

# =============================================================================
# CTest Integration
# =============================================================================

enable_testing()

add_test(NAME test_rx_crc32 COMMAND test_rx_crc32)
add_test(NAME test_rx_crc8 COMMAND test_rx_crc8)
add_test(NAME test_rx_frame COMMAND test_rx_frame)
add_test(NAME test_rx_fec COMMAND test_rx_fec)
add_test(NAME test_rx_harq COMMAND test_rx_harq)
add_test(NAME test_rx_usb COMMAND test_rx_usb)
add_test(NAME test_rx_usb_comm COMMAND test_rx_usb_comm)
add_test(NAME test_rx_time COMMAND test_rx_time)
add_test(NAME test_rx_hcsr04 COMMAND test_rx_hcsr04)
add_test(NAME test_rx_obstacle_detect COMMAND test_rx_obstacle_detect)
add_test(NAME test_uart_hal COMMAND test_uart_hal)
add_test(NAME test_gpio_hal COMMAND test_gpio_hal)
add_test(NAME test_rx_bus_uart COMMAND test_rx_bus_uart)
add_test(NAME test_rx_motor COMMAND test_rx_motor)
add_test(NAME test_rx_bus_manager COMMAND test_rx_bus_manager)
add_test(NAME test_rx_bus_onewire COMMAND test_rx_bus_onewire)
add_test(NAME test_rx_spi_comm COMMAND test_rx_spi_comm)
add_test(NAME test_rx_pid COMMAND test_rx_pid)
add_test(NAME test_rx_encoder COMMAND test_rx_encoder)
add_test(NAME test_rx_bq4050 COMMAND test_rx_bq4050)
add_test(NAME test_rx_ds18b20 COMMAND test_rx_ds18b20)
add_test(NAME test_rx_error_handler COMMAND test_rx_error_handler)
add_test(NAME test_rx_pin_validator COMMAND test_rx_pin_validator)
add_test(NAME test_rx_register_guard COMMAND test_rx_register_guard)
add_test(NAME test_rx_nanopb COMMAND test_rx_nanopb)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_rx_crc32 test_rx_crc8 test_rx_frame test_rx_fec test_rx_harq test_rx_usb test_rx_usb_comm test_rx_time test_rx_hcsr04 test_rx_obstacle_detect test_uart_hal test_gpio_hal test_rx_bus_uart test_rx_motor test_rx_bus_manager test_rx_bus_onewire test_rx_spi_comm test_rx_pid test_rx_encoder test_rx_bq4050 test_rx_ds18b20 test_rx_error_handler test_rx_pin_validator test_rx_register_guard test_rx_nanopb
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
