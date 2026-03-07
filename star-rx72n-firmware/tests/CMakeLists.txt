# =============================================================================
# STAR RX72N Firmware - Unit Tests
# Host-compiled tests using Unity framework
# =============================================================================

cmake_minimum_required(VERSION 3.21)  # 3.21+ required for ctest --output-junit
project(star_rx72n_tests C)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    set(CMAKE_C_STANDARD 23)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

# Helper function to apply strict warnings/defines to our test targets only
function(apply_strict_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-error=cpp -Wno-error=format-truncation=)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fshort-enums)
    endif()
    target_compile_definitions(${target} PRIVATE USE_MOCK_SCI_REGS UNIT_TEST RX_SIMULATOR_MODE USB_LOG_MIRROR=0)
endfunction()

# Helper function to apply common include paths per target
function(apply_test_includes target)
    target_include_directories(${target} BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/mocks
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/../libs/rx_dmaca/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_frame/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_crc/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_fec/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_harq/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_core/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_usb/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_usb_comm/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_spi_comm/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_spi_link/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_frame_ascii/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_comm_manager/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_hcsr04/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_hcsr04/src
        ${CMAKE_SOURCE_DIR}/../libs/rx_obstacle_detect/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_hal/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_bus/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_motor/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_pid/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_session/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_encoder/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen
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

# rx_crc source (public API + software implementation)
# Hardware backend guarded by #ifdef __RX__, so no special define needed for host tests
set(RX_CRC_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_crc/src/rx_crc.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_crc/src/rx_crc_sw.c
)

# rx_dmaca source (real driver for test_rx_dmaca; mocked via mock_rx_dmaca.c for rx_crc tests)
set(RX_DMACA_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_dmaca/src/rx_dmaca.c
)

# rx_dmaca mock source (for rx_crc DMA backend tests)
set(MOCK_DMACA_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_dmaca.c
)

# rx_frame source
set(RX_FRAME_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_frame/src/rx_frame.c
    ${RX_CRC_SRC}
)

# rx_fec source
set(RX_FEC_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_fec/src/rx_fec.c
)

# rx_harq source
set(RX_HARQ_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_harq/src/rx_harq.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_fec/src/rx_fec.c
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
    ${CMAKE_SOURCE_DIR}/mocks/mock_time.c
)

# rx_usb source (only rx_usb.c, hardware mocked)
set(RX_USB_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_usb/src/rx_usb.c
)

# rx_usb_comm source
set(RX_USB_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_usb_comm/src/rx_usb_comm.c
)

# rx_hcsr04 source
set(RX_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_hcsr04/src/rx_hcsr04.c
)

# HC-SR04 mock source (includes IRQ mode mocks)
set(MOCK_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_hcsr04_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/rx_hcsr04_hal_mock.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_hcsr04_irq.c
)

# rx_obstacle_detect source
set(RX_OBSTACLE_DETECT_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_obstacle_detect/src/rx_obstacle_detect.c
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
    ${CMAKE_SOURCE_DIR}/../libs/rx_spi_comm/src/rx_spi_comm.c
)

# rx_spi_link source
set(RX_SPI_LINK_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_spi_link/src/rx_spi_link.c
)

# rx_frame_ascii source
set(RX_FRAME_ASCII_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_frame_ascii/src/rx_frame_ascii.c
)

# rx_session source
set(RX_SESSION_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_session/src/rx_session.c
)

# rx_comm_manager source
set(RX_COMM_MANAGER_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_comm_manager/src/rx_comm_manager.c
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
    ${CMAKE_SOURCE_DIR}/../libs/rx_motor/src/rx_motor.c
)

# rx_pid source
set(RX_PID_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_pid/src/rx_pid.c
)

# Mock MTU encoder source for encoder testing
set(MOCK_MTU_ENCODER_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_mtu_encoder.c
)

# rx_encoder source
set(RX_ENCODER_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_encoder/src/rx_mtu_encoder.c
)

# rx_core sources for infrastructure testing
set(RX_ERROR_HANDLER_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_error_handler.c
)

set(RX_PIN_VALIDATOR_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_pin_validator.c
)

set(RX_REGISTER_GUARD_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_register_guard.c
)

# rx_nanopb source (wrapper + generated protobuf files)
set(RX_NANOPB_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/src/rx_nanopb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/google/protobuf/timestamp.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/google/protobuf/duration.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/common.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/motor_control.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/telemetry.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/ui.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/controller.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/gateway_service.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/configuration.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/diagnostics.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/firmware_update.pb.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_nanopb/inc/gen/star/v1/wire.pb.c
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

# Test: USB CDC Multi-Port (Composite Device)
add_executable(test_rx_usb_multiport
    test_rx_usb_multiport.c
    ${RX_USB_SRC}
    ${MOCK_USB_SRC}
)
apply_strict_warnings(test_rx_usb_multiport)
apply_test_includes(test_rx_usb_multiport)
target_link_libraries(test_rx_usb_multiport unity)

# Test: USB Communication Layer
add_executable(test_rx_usb_comm
    test_rx_usb_comm.c
    ${RX_USB_COMM_SRC}
    ${RX_USB_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
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
# Undefine RX_SIMULATOR_MODE for uart_hal test: test needs real uart_debug_*
# functions that route through mock UART hardware, not the simulator stubs
# that write directly to stdout.
target_compile_options(test_uart_hal PRIVATE -URX_SIMULATOR_MODE)
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

# Test: DRV8263H-Q1 Motor Driver (chip-level control)
add_executable(test_rx_drv8263
    test_rx_drv8263.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_drv8263/src/rx_drv8263.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_drv8263_port.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_drv8263)
apply_test_includes(test_rx_drv8263)
# Mock drv8263/ dir must come BEFORE all others to shadow rx72n_port_regs.h
# with RAM-based mock port registers instead of hardware addresses
target_include_directories(test_rx_drv8263 BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/mocks/drv8263
)
target_include_directories(test_rx_drv8263 PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_drv8263/inc
)
set_target_properties(test_rx_drv8263 PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)
target_link_libraries(test_rx_drv8263 unity m)

# =============================================================================
# Bus Manager and OneWire Tests
# =============================================================================

# Bus config source (used by both mock and real implementations)
set(RX_BUS_CONFIG_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_config.c
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
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
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
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_onewire.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_ONEWIRE_SRC}
)
apply_strict_warnings(test_rx_bus_onewire)
apply_test_includes(test_rx_bus_onewire)
target_link_libraries(test_rx_bus_onewire unity)

# Test: GPIO Bus Abstraction
add_executable(test_rx_bus_gpio
    test_rx_bus_gpio.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_gpio.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bus_gpio)
apply_test_includes(test_rx_bus_gpio)
target_link_libraries(test_rx_bus_gpio unity)
set_target_properties(test_rx_bus_gpio PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# Test: ADC Bus Abstraction
add_executable(test_rx_bus_adc
    test_rx_bus_adc.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_adc.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bus_adc)
apply_test_includes(test_rx_bus_adc)
target_link_libraries(test_rx_bus_adc unity)
set_target_properties(test_rx_bus_adc PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# Test: I2C Bus Abstraction
add_executable(test_rx_bus_i2c
    test_rx_bus_i2c.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_i2c.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bus_i2c)
apply_test_includes(test_rx_bus_i2c)
target_link_libraries(test_rx_bus_i2c unity)
set_target_properties(test_rx_bus_i2c PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# Test: SPI Communication Layer
add_executable(test_rx_spi_comm
    test_rx_spi_comm.c
    ${RX_SPI_COMM_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_spi_comm)
apply_test_includes(test_rx_spi_comm)
target_link_libraries(test_rx_spi_comm unity)

# Test: SPI Link Layer (HARQ + FEC)
add_executable(test_rx_spi_link
    test_rx_spi_link.c
    ${RX_SPI_LINK_SRC}
    ${RX_SPI_COMM_SRC}
    ${RX_HARQ_SRC}
    ${RX_FEC_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_spi_link)
apply_test_includes(test_rx_spi_link)
target_link_libraries(test_rx_spi_link unity)

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

# Test: DS18B20 Temperature Sensor
add_executable(test_rx_ds18b20
    test_rx_ds18b20.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_ds18b20/src/rx_ds18b20.c
    ${RX_CRC_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_ds18b20)
apply_test_includes(test_rx_ds18b20)
target_include_directories(test_rx_ds18b20 PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_ds18b20/inc
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
# Suppress nanopb-generated warnings
target_compile_options(test_rx_nanopb PRIVATE
    -Wno-missing-braces
)
target_link_libraries(test_rx_nanopb unity m)

# Test: Session State (shared across transports)
add_executable(test_rx_session
    test_rx_session.c
    ${RX_SESSION_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_session)
apply_test_includes(test_rx_session)
target_link_libraries(test_rx_session unity)

# Test: Frame ASCII Formatter
add_executable(test_rx_frame_ascii
    test_rx_frame_ascii.c
    ${RX_FRAME_ASCII_SRC}
    ${RX_FRAME_SRC}
    ${MOCK_UART_SRC}
)
apply_strict_warnings(test_rx_frame_ascii)
apply_test_includes(test_rx_frame_ascii)
target_include_directories(test_rx_frame_ascii PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_frame_ascii/inc
)
target_link_libraries(test_rx_frame_ascii unity)

# Test: Communication Manager
add_executable(test_rx_comm_manager
    test_rx_comm_manager.c
    ${RX_COMM_MANAGER_SRC}
    ${RX_FRAME_ASCII_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
    ${RX_USB_COMM_SRC}
    ${RX_USB_SRC}
    ${RX_SPI_COMM_SRC}
    ${RX_SPI_LINK_SRC}
    ${RX_FEC_SRC}
    ${RX_HARQ_SRC}
    ${MOCK_USB_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_comm_manager)
apply_test_includes(test_rx_comm_manager)
target_include_directories(test_rx_comm_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_frame_ascii/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_comm_manager/inc
)
target_link_libraries(test_rx_comm_manager unity)

# Test: Independent Watchdog Timer (IWDT) Driver
add_executable(test_rx_iwdt
    test_rx_iwdt.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_iwdt.c
    ${MOCK_TIME_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_iwdt)
apply_test_includes(test_rx_iwdt)
target_link_libraries(test_rx_iwdt unity)

# Test: GPIO (PORT) Register Definitions
add_executable(test_rx_gpio
    test_rx_gpio.c
)
apply_strict_warnings(test_rx_gpio)
apply_test_includes(test_rx_gpio)
target_link_libraries(test_rx_gpio unity)

# Test: MPC (Multi-Function Pin Controller) Register Definitions
add_executable(test_rx_mpc
    test_rx_mpc.c
)
apply_strict_warnings(test_rx_mpc)
apply_test_includes(test_rx_mpc)
target_link_libraries(test_rx_mpc unity)

# Test: POEG Motor Fault Protection Driver
add_executable(test_rx_poeg
    test_rx_poeg.c
)
apply_strict_warnings(test_rx_poeg)
apply_test_includes(test_rx_poeg)
target_link_libraries(test_rx_poeg unity)

# Test: LED Status Task
add_executable(test_rx_led_status
    test_rx_led_status.c
)
apply_strict_warnings(test_rx_led_status)
apply_test_includes(test_rx_led_status)
target_link_libraries(test_rx_led_status unity)

# Test: Unified CRC API (all 5 polynomials x 3 backends)
# Uses mock_rx_dmaca to intercept DMA calls on host builds.
add_executable(test_rx_crc
    test_rx_crc.c
    ${RX_CRC_SRC}
    ${MOCK_DMACA_SRC}
)
apply_strict_warnings(test_rx_crc)
apply_test_includes(test_rx_crc)
target_link_libraries(test_rx_crc unity)

# Test: DMAC Polling Driver (rx_dmaca)
# Tests the real rx_dmaca.c against mock DMAC hardware registers.
# rx72n_dmac_regs.h (in mocks/) provides the mock DMAC register storage.
add_executable(test_rx_dmaca
    test_rx_dmaca.c
    ${RX_DMACA_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_dmaca)
apply_test_includes(test_rx_dmaca)
target_link_libraries(test_rx_dmaca unity)

# Test: TPU HAL Driver (Phase Counting Mode)
add_executable(test_rx_tpu
    test_rx_tpu.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_tpu.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_tpu)
apply_test_includes(test_rx_tpu)
target_link_libraries(test_rx_tpu unity)

# Test: TPU Encoder Backend (Overflow Detection + Velocity)
add_executable(test_rx_encoder_tpu
    test_rx_encoder_tpu.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_encoder/src/rx_encoder_tpu.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_tpu.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_encoder_tpu)
apply_test_includes(test_rx_encoder_tpu)
target_link_libraries(test_rx_encoder_tpu unity m)

# =============================================================================
# Multi-Task Firmware Test Sources
# =============================================================================

# Mock sources for multi-task testing
set(MOCK_TX_API_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_tx_api.c
)

set(MOCK_TASKS_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_tasks.c
)

set(MOCK_SHARED_DATA_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_shared_data.c
)

set(MOCK_PID_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_pid.c
)

set(MOCK_COMM_MANAGER_SRC_TASK
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_comm_manager.c
)

set(MOCK_NANOPB_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_nanopb.c
)

set(MOCK_OBSTACLE_DETECT_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_obstacle_detect.c
)

set(MOCK_DS18B20_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_ds18b20.c
)

# Task source files
set(TASK_MOTOR_CONTROL_SRC
    ${CMAKE_SOURCE_DIR}/../src/tasks/motor_control_task.c
)

set(TASK_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../src/tasks/comm_task.c
)

set(TASK_OBSTACLE_DETECT_SRC
    ${CMAKE_SOURCE_DIR}/../src/tasks/obstacle_detect_task.c
)

set(TASK_TEMP_SENSOR_SRC
    ${CMAKE_SOURCE_DIR}/../src/tasks/temp_sensor_task.c
)

set(TASK_TELEMETRY_SRC
    ${CMAKE_SOURCE_DIR}/../src/tasks/telemetry_task.c
)

# Test: Motor Control Task
add_executable(test_motor_control_task
    test_motor_control_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_PID_SRC}
    ${MOCK_MOTOR_SRC}
    ${MOCK_MTU_ENCODER_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_motor_control_task)
apply_test_includes(test_motor_control_task)
target_compile_definitions(test_motor_control_task PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_motor_control_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
target_link_libraries(test_motor_control_task unity m)

# Test: Communication Task
add_executable(test_communication_task
    test_communication_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_COMM_MANAGER_SRC_TASK}
    ${MOCK_NANOPB_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_communication_task)
apply_test_includes(test_communication_task)
target_compile_definitions(test_communication_task PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_communication_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
target_link_libraries(test_communication_task unity)

# Test: Obstacle Detection Task
add_executable(test_obstacle_detection_task
    test_obstacle_detection_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_OBSTACLE_DETECT_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_obstacle_detection_task)
apply_test_includes(test_obstacle_detection_task)
target_compile_definitions(test_obstacle_detection_task PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_obstacle_detection_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
target_link_libraries(test_obstacle_detection_task unity)

# Test: Temperature Sensing Task
add_executable(test_temperature_sensing_task
    test_temperature_sensing_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_DS18B20_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_temperature_sensing_task)
apply_test_includes(test_temperature_sensing_task)
target_compile_definitions(test_temperature_sensing_task PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_temperature_sensing_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_ds18b20/inc
)
target_link_libraries(test_temperature_sensing_task unity)

# Test: Telemetry Aggregation Task
add_executable(test_telemetry_aggregation_task
    test_telemetry_aggregation_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_COMM_MANAGER_SRC_TASK}
    ${MOCK_NANOPB_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_telemetry_aggregation_task)
apply_test_includes(test_telemetry_aggregation_task)
target_compile_definitions(test_telemetry_aggregation_task PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_telemetry_aggregation_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
target_link_libraries(test_telemetry_aggregation_task unity)

# Test: Stack Monitor (ThreadX stack overflow detection and high-water mark)
add_executable(test_rx_stack_monitor
    test_rx_stack_monitor.c
    ${CMAKE_SOURCE_DIR}/../src/rx_stack_monitor.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_stack_monitor)
apply_test_includes(test_rx_stack_monitor)
target_include_directories(test_rx_stack_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/inc
    ${CMAKE_SOURCE_DIR}/../libs/threadx
)
target_link_libraries(test_rx_stack_monitor unity)

# Test: HOST_IRQ GPIO Output Driver (self-contained)
add_executable(test_rx_host_irq
    test_rx_host_irq.c
)
apply_strict_warnings(test_rx_host_irq)
apply_test_includes(test_rx_host_irq)
target_link_libraries(test_rx_host_irq unity)

# Test: BNO055 9-DOF IMU Sensor
add_executable(test_rx_bno055
    test_rx_bno055.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bno055/src/rx_bno055.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_i2c.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bno055)
apply_test_includes(test_rx_bno055)
target_include_directories(test_rx_bno055 PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_bno055/inc
)
target_link_libraries(test_rx_bno055 PRIVATE unity m)
set_target_properties(test_rx_bno055 PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# Test: BMP280 Barometric Pressure Sensor
add_executable(test_rx_bmp280
    test_rx_bmp280.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bmp280/src/rx_bmp280.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_bus/src/rx_bus_i2c.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_bmp280)
# TESTING: enables rx_bmp280_test_reset_state() for test-order independence
target_compile_definitions(test_rx_bmp280 PRIVATE TESTING)
apply_test_includes(test_rx_bmp280)
target_include_directories(test_rx_bmp280 PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_bmp280/inc
)
target_link_libraries(test_rx_bmp280 PRIVATE unity m)
set_target_properties(test_rx_bmp280 PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# =============================================================================
# CTest Integration
# =============================================================================

enable_testing()

# List of all test targets (used by run_all_tests target)
set(ALL_TEST_TARGETS
    test_rx_crc32
    test_rx_crc8
    test_rx_crc
    test_rx_frame
    test_rx_fec
    test_rx_harq
    test_rx_usb
    test_rx_usb_multiport
    test_rx_usb_comm
    test_rx_time
    test_rx_hcsr04
    test_rx_obstacle_detect
    test_uart_hal
    test_gpio_hal
    test_rx_motor
    test_rx_bus_manager
    test_rx_bus_onewire
    test_rx_bus_gpio
    test_rx_bus_adc
    test_rx_bus_i2c
    test_rx_spi_comm
    test_rx_spi_link
    test_rx_pid
    test_rx_encoder
    test_rx_ds18b20
    test_rx_error_handler
    test_rx_pin_validator
    test_rx_register_guard
    test_rx_nanopb
    test_rx_gptw_staggered
    test_rx_drv8263
    test_rx_frame_ascii
    test_rx_session
    test_rx_comm_manager
    test_rx_iwdt
    test_rx_gpio
    test_rx_mpc
    test_rx_poeg
    test_rx_led_status
    test_rx_dmaca
    test_rx_tpu
    test_rx_encoder_tpu
    test_rx_host_irq
    test_rx_stack_monitor
    test_rx_bno055
    test_rx_bmp280
    # Multi-task firmware tests
    test_motor_control_task
    test_communication_task
    test_obstacle_detection_task
    test_temperature_sensing_task
    test_telemetry_aggregation_task
)

add_test(NAME test_rx_crc32 COMMAND test_rx_crc32)
add_test(NAME test_rx_crc8 COMMAND test_rx_crc8)
add_test(NAME test_rx_crc COMMAND test_rx_crc)
add_test(NAME test_rx_frame COMMAND test_rx_frame)
add_test(NAME test_rx_fec COMMAND test_rx_fec)
add_test(NAME test_rx_harq COMMAND test_rx_harq)
add_test(NAME test_rx_usb COMMAND test_rx_usb)
add_test(NAME test_rx_usb_multiport COMMAND test_rx_usb_multiport)
add_test(NAME test_rx_usb_comm COMMAND test_rx_usb_comm)
add_test(NAME test_rx_time COMMAND test_rx_time)
add_test(NAME test_rx_hcsr04 COMMAND test_rx_hcsr04)
add_test(NAME test_rx_obstacle_detect COMMAND test_rx_obstacle_detect)
add_test(NAME test_uart_hal COMMAND test_uart_hal)
add_test(NAME test_gpio_hal COMMAND test_gpio_hal)
add_test(NAME test_rx_motor COMMAND test_rx_motor)
add_test(NAME test_rx_bus_manager COMMAND test_rx_bus_manager)
add_test(NAME test_rx_bus_onewire COMMAND test_rx_bus_onewire)
add_test(NAME test_rx_bus_gpio COMMAND test_rx_bus_gpio)
add_test(NAME test_rx_bus_adc COMMAND test_rx_bus_adc)
add_test(NAME test_rx_bus_i2c COMMAND test_rx_bus_i2c)
add_test(NAME test_rx_spi_comm COMMAND test_rx_spi_comm)
add_test(NAME test_rx_spi_link COMMAND test_rx_spi_link)
add_test(NAME test_rx_pid COMMAND test_rx_pid)
add_test(NAME test_rx_encoder COMMAND test_rx_encoder)
add_test(NAME test_rx_ds18b20 COMMAND test_rx_ds18b20)
add_test(NAME test_rx_error_handler COMMAND test_rx_error_handler)
add_test(NAME test_rx_pin_validator COMMAND test_rx_pin_validator)
add_test(NAME test_rx_register_guard COMMAND test_rx_register_guard)
add_test(NAME test_rx_nanopb COMMAND test_rx_nanopb)
add_test(NAME test_rx_gptw_staggered COMMAND test_rx_gptw_staggered)
add_test(NAME test_rx_drv8263 COMMAND test_rx_drv8263)
add_test(NAME test_rx_frame_ascii COMMAND test_rx_frame_ascii)
add_test(NAME test_rx_session COMMAND test_rx_session)
add_test(NAME test_rx_comm_manager COMMAND test_rx_comm_manager)
add_test(NAME test_rx_iwdt COMMAND test_rx_iwdt)
add_test(NAME test_rx_gpio COMMAND test_rx_gpio)
add_test(NAME test_rx_mpc COMMAND test_rx_mpc)
add_test(NAME test_rx_poeg COMMAND test_rx_poeg)
add_test(NAME test_rx_led_status COMMAND test_rx_led_status)
add_test(NAME test_rx_dmaca COMMAND test_rx_dmaca)
add_test(NAME test_rx_tpu COMMAND test_rx_tpu)
add_test(NAME test_rx_encoder_tpu COMMAND test_rx_encoder_tpu)
add_test(NAME test_rx_host_irq COMMAND test_rx_host_irq)
add_test(NAME test_rx_stack_monitor COMMAND test_rx_stack_monitor)
add_test(NAME test_rx_bno055 COMMAND test_rx_bno055)
add_test(NAME test_rx_bmp280 COMMAND test_rx_bmp280)

# Multi-task firmware tests
add_test(NAME test_motor_control_task COMMAND test_motor_control_task)
add_test(NAME test_communication_task COMMAND test_communication_task)
add_test(NAME test_obstacle_detection_task COMMAND test_obstacle_detection_task)
add_test(NAME test_temperature_sensing_task COMMAND test_temperature_sensing_task)
add_test(NAME test_telemetry_aggregation_task COMMAND test_telemetry_aggregation_task)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${ALL_TEST_TARGETS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
