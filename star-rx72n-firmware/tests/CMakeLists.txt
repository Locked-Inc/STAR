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

# =============================================================================
# clang-tidy integration (optional, default OFF)
#
# Enable with: cmake -B build/tidy -S . -DENABLE_CLANG_TIDY=ON
# Requires clang-tidy >= 16 for C23 typed enum syntax support.
# Runs against this host build's compile_commands.json so that all firmware
# source files are analyzed without cross-compiler flag conflicts.
# =============================================================================
option(ENABLE_CLANG_TIDY "Run clang-tidy on all compiled sources" OFF)

if(ENABLE_CLANG_TIDY)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Prefer versioned clang-tidy binaries (require >= 16 for C23 typed enums)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy
        DOC "Path to clang-tidy executable"
    )

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY=ON but clang-tidy not found.\n"
            "Install with: sudo apt-get install clang-tidy"
        )
    endif()

    # Verify the found clang-tidy is >= 16 (required for C23 typed enum syntax).
    execute_process(
        COMMAND "${CLANG_TIDY_EXE}" --version
        OUTPUT_VARIABLE CLANG_TIDY_VERSION_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "version ([0-9]+)" _ "${CLANG_TIDY_VERSION_OUTPUT}")
    set(CLANG_TIDY_VERSION_MAJOR "${CMAKE_MATCH_1}")
    if(NOT CLANG_TIDY_VERSION_MAJOR OR CLANG_TIDY_VERSION_MAJOR LESS 16)
        message(FATAL_ERROR
            "clang-tidy >= 16 required for C23 typed enum support.\n"
            "Found: ${CLANG_TIDY_EXE} (version ${CLANG_TIDY_VERSION_MAJOR}).\n"
            "Install with: sudo apt-get install clang-tidy-18"
        )
    endif()

    # Verify .clang-tidy config exists before configuring CMAKE_C_CLANG_TIDY
    # so that a misconfigured build fails early with a clear message.
    set(_clang_tidy_config "${CMAKE_SOURCE_DIR}/../.clang-tidy")
    if(NOT EXISTS "${_clang_tidy_config}")
        message(FATAL_ERROR
            ".clang-tidy config not found at ${_clang_tidy_config}.\n"
            "Ensure star-rx72n-firmware/.clang-tidy exists in the repository."
        )
    endif()

    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE} (version ${CLANG_TIDY_VERSION_MAJOR})")

    # Do NOT use --config-file here.  Omitting it lets clang-tidy walk up the
    # directory tree from each source file, picking up tests/.clang-tidy (which
    # relaxes function-size limits for Unity test runners) before falling through
    # to star-rx72n-firmware/.clang-tidy for production code.
    #
    # -std=c2x: pre-ratification name for C23; supported by clang-16+ and required
    # for typed enum syntax (typedef enum : uint8_t {}).  Use c2x rather than c23
    # for maximum clang version compatibility.
    # -DUSE_MOCK_SCI_REGS: matches apply_strict_warnings() compile definitions so
    # static analysis sees the same conditional flags as the test build.
    set(CMAKE_C_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
        "--extra-arg=-std=c2x"
        "--extra-arg=-DUNIT_TEST"
        "--extra-arg=-DRX_SIMULATOR_MODE"
        "--extra-arg=-DUSE_MOCK_SCI_REGS"
    )
endif()

# Helper function to apply strict warnings/defines to our test targets only
function(apply_strict_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic --coverage)
    target_link_options(${target} PRIVATE --coverage)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fshort-enums)
    endif()
    target_compile_definitions(${target} PRIVATE USE_MOCK_SCI_REGS UNIT_TEST RX_SIMULATOR_MODE)
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
        ${CMAKE_SOURCE_DIR}/../libs/rx_i2c_comm/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_spi_comm/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_spi_link/inc
        ${CMAKE_SOURCE_DIR}/../libs/rx_uart_comm/inc
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

# Disable clang-tidy for nanopb and Unity vendor sources -- they don't follow
# STAR coding standards (magic numbers, cognitive complexity, insecure APIs).
if(ENABLE_CLANG_TIDY)
    set_target_properties(protobuf-nanopb-static PROPERTIES C_CLANG_TIDY "")
    set_target_properties(unity PROPERTIES C_CLANG_TIDY "")
endif()

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

# rx_log backend: the real UART ring-buffer log backend compiles on the host.
# (A mock is provided for tests that need to inspect buffered bytes.)
set(RX_LOG_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_log_uart.c
)

# Log mock source: alias to the real UART log backend; no separate UART/USB
# stub is needed because only one backend exists now.
set(MOCK_LOG_SRC
    ${RX_LOG_SRC}
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

# rx_hcsr04 source
set(RX_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_hcsr04/src/rx_hcsr04.c
)

# HC-SR04 mock source (includes IRQ mode mocks)
set(MOCK_HCSR04_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_hcsr04_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_hcsr04_hal.c
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

# Mock RIIC HAL source for I2C peripheral mode testing.
# NOTE: mock_riic_hal.c also appears in MOCK_HAL_SRC (used by GPIO/ADC/bus
# tests that need a combined HAL mock).  Targets must use EITHER MOCK_HAL_SRC
# OR MOCK_RIIC_HAL_SRC - never both, as that would produce duplicate symbols.
set(MOCK_RIIC_HAL_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_riic_hal.c
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

# rx_i2c_comm source
set(RX_I2C_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_i2c_comm/src/rx_i2c_comm.c
)

# rx_uart_comm source
set(RX_UART_COMM_SRC
    ${CMAKE_SOURCE_DIR}/../libs/rx_uart_comm/src/rx_uart_comm.c
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
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_frame)
apply_test_includes(test_rx_frame)
target_link_libraries(test_rx_frame unity)

# Test: Frame Go Cross-Language Compatibility
add_executable(test_frame_go_compat
    test_frame_go_compat.c
    ${RX_FRAME_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_frame_go_compat)
apply_test_includes(test_frame_go_compat)
target_link_libraries(test_frame_go_compat unity)

# Test: FEC Codec
add_executable(test_rx_fec
    test_rx_fec.c
    ${RX_FEC_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_fec)
apply_test_includes(test_rx_fec)
target_link_libraries(test_rx_fec unity)

# Test: HARQ Protocol
add_executable(test_rx_harq
    test_rx_harq.c
    ${RX_HARQ_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_harq)
apply_test_includes(test_rx_harq)
target_link_libraries(test_rx_harq unity)

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
    ${MOCK_LOG_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_tx_api.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tasks.c
)
apply_strict_warnings(test_rx_hcsr04)
apply_test_includes(test_rx_hcsr04)
target_link_libraries(test_rx_hcsr04 unity)
target_compile_definitions(test_rx_hcsr04 PRIVATE
    MOCK_TX_THREAD_CREATE
    MOCK_TX_THREAD_DELETE
    MOCK_TX_EVENT_FLAGS_CREATE
    MOCK_TX_EVENT_FLAGS_GET
)

# Test: Obstacle Detection
add_executable(test_rx_obstacle_detect
    test_rx_obstacle_detect.c
    ${RX_OBSTACLE_DETECT_SRC}
    ${RX_HCSR04_SRC}
    ${MOCK_HCSR04_SRC}
    ${MOCK_MOTOR_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_LOG_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_tx_api.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tasks.c
)
apply_strict_warnings(test_rx_obstacle_detect)
apply_test_includes(test_rx_obstacle_detect)
target_link_libraries(test_rx_obstacle_detect unity)
target_compile_definitions(test_rx_obstacle_detect PRIVATE
    MOCK_TX_THREAD_CREATE
    MOCK_TX_THREAD_DELETE
    MOCK_TX_EVENT_FLAGS_CREATE
    MOCK_TX_EVENT_FLAGS_GET
    MOCK_TX_THREAD_SLEEP
)

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
target_compile_definitions(test_rx_drv8263 PRIVATE UNIT_TEST)
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
    ${MOCK_LOG_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
)

# Test: Bus Manager (uses mock implementation for testing)
add_executable(test_rx_bus_manager
    test_rx_bus_manager.c
    ${MOCK_BUS_MANAGER_SRC}
    ${MOCK_LOG_SRC}
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
    ${CMAKE_SOURCE_DIR}/../libs/rx_encoder/src/rx_mtu_encoder.c
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
# Disable clang-tidy for nanopb vendor sources compiled into this target.
# set_target_properties(protobuf-nanopb-static) only covers the library target;
# these .c files are compiled directly into test_rx_nanopb and need separate treatment.
if(ENABLE_CLANG_TIDY)
    set_source_files_properties(
        ${nanopb_SOURCE_DIR}/pb_common.c
        ${nanopb_SOURCE_DIR}/pb_encode.c
        ${nanopb_SOURCE_DIR}/pb_decode.c
        PROPERTIES SKIP_LINTING ON
    )
endif()

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
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_frame_ascii)
apply_test_includes(test_rx_frame_ascii)
target_include_directories(test_rx_frame_ascii PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_frame_ascii/inc
)
target_link_libraries(test_rx_frame_ascii unity)

# test_rx_comm_manager was removed along with the USB0 libraries it tested.
# Individual transport correctness is covered by test_rx_spi_comm,
# test_rx_i2c_comm, and test_rx_uart_comm. If integration-level tests are
# needed again, re-add a dedicated suite targeting UART + SPI + I2C routing.

# Test: I2C Comm Layer (RIIC0 peripheral frame protocol)
add_executable(test_rx_i2c_comm
    test_rx_i2c_comm.c
    ${RX_I2C_COMM_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
    ${MOCK_RIIC_HAL_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_i2c_comm)
apply_test_includes(test_rx_i2c_comm)
target_include_directories(test_rx_i2c_comm PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_i2c_comm/inc
)
target_link_libraries(test_rx_i2c_comm unity)

# Test: UART Comm Layer (SCI9 frame protocol)
add_executable(test_rx_uart_comm
    test_rx_uart_comm.c
    ${RX_UART_COMM_SRC}
    ${RX_FRAME_SRC}
    ${RX_SESSION_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_uart_comm)
apply_test_includes(test_rx_uart_comm)
target_include_directories(test_rx_uart_comm PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_uart_comm/inc
)
target_link_libraries(test_rx_uart_comm unity)

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

# Test: Clock Frequency Accuracy Measurement Circuit (CAC) Driver
add_executable(test_rx_cac
    test_rx_cac.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_cac.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_cac.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_cac)
apply_test_includes(test_rx_cac)
target_link_libraries(test_rx_cac unity)

# Test: Data Operation Circuit (DOC) Driver
add_executable(test_rx_doc
    test_rx_doc.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_doc.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_doc.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_doc)
apply_test_includes(test_rx_doc)
target_link_libraries(test_rx_doc unity)

# Test: ECC-Protected RAM (ECCRAM) Driver
add_executable(test_rx_eccram
    test_rx_eccram.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_eccram.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_eccram)
apply_test_includes(test_rx_eccram)
target_link_libraries(test_rx_eccram unity)

# Test: Low Power Consumption (LPC) Driver
add_executable(test_rx_lpc
    test_rx_lpc.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_lpc.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_lpc.c
)
apply_strict_warnings(test_rx_lpc)
apply_test_includes(test_rx_lpc)
target_link_libraries(test_rx_lpc unity)

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

# Test: TMR HAL Driver (8-bit + 16-bit cascade)
add_executable(test_rx_tmr
    test_rx_tmr.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_hal/src/rx_tmr.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_onewire_hw.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_system_regs.c
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_rx_tmr)
apply_test_includes(test_rx_tmr)
target_link_libraries(test_rx_tmr unity)

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

# Mock comm transport sources: stubs for transport inits, IWDT heartbeat,
# and comm_manager/nanopb helpers absent from the standard mocks.
# Required when linking the real comm_task.c in test_rx_system_config.
set(MOCK_COMM_TRANSPORTS_SRC
    ${CMAKE_SOURCE_DIR}/mocks/mock_comm_transports.c
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

# Test: IMU Task (interrupt-driven BNO055 + BMP280)
add_executable(test_imu_task
    test_imu_task.c
    ${MOCK_TX_API_SRC}
    ${MOCK_TASKS_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_LOG_SRC}
)
apply_strict_warnings(test_imu_task)
apply_test_includes(test_imu_task)
target_compile_definitions(test_imu_task PRIVATE MOCK_TX_THREAD_CREATE MOCK_TX_EVENT_FLAGS_CREATE)
target_include_directories(test_imu_task PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
target_link_libraries(test_imu_task unity)
set_target_properties(test_imu_task PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)

# Test: rx_log_uart ring buffer backend
add_executable(test_rx_log_uart
    test_rx_log_uart.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_log_uart.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tx_api.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tasks.c
)
apply_strict_warnings(test_rx_log_uart)
apply_test_includes(test_rx_log_uart)
target_include_directories(test_rx_log_uart PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/inc
)
target_link_libraries(test_rx_log_uart unity)

# Test: rx_log.h inline coverage (every rx_log_* macro variant + simulator uart_debug_*)
add_executable(test_rx_log
    test_rx_log.c
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_log_uart.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tx_api.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_tasks.c
)
apply_strict_warnings(test_rx_log)
apply_test_includes(test_rx_log)
target_include_directories(test_rx_log PRIVATE
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/inc
)
# Force LOG_LEVEL=verbose so every level-gated macro expands to a real call.
target_compile_definitions(test_rx_log PRIVATE LOG_LEVEL=k_log_verbose)
target_link_libraries(test_rx_log unity)

# Test: comm_task_apply_system_config() unified config
add_executable(test_rx_system_config
    test_rx_system_config.c
    ${TASK_COMM_SRC}
    ${MOCK_TX_API_SRC}
    ${MOCK_TIME_SRC}
    ${MOCK_SHARED_DATA_SRC}
    ${MOCK_COMM_MANAGER_SRC_TASK}
    ${MOCK_NANOPB_SRC}
    ${MOCK_LOG_SRC}
    ${MOCK_COMM_TRANSPORTS_SRC}
)
apply_strict_warnings(test_rx_system_config)
apply_test_includes(test_rx_system_config)
target_compile_definitions(test_rx_system_config PRIVATE MOCK_TX_THREAD_CREATE)
target_include_directories(test_rx_system_config PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
)
set_target_properties(test_rx_system_config PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)
target_link_libraries(test_rx_system_config unity)

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
    test_frame_go_compat
    test_rx_fec
    test_rx_harq
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
    test_rx_i2c_comm
    test_rx_uart_comm
    test_rx_iwdt
    test_rx_cac
    test_rx_doc
    test_rx_eccram
    test_rx_lpc
    test_rx_gpio
    test_rx_mpc
    test_rx_poeg
    test_rx_led_status
    test_rx_dmaca
    test_rx_tpu
    test_rx_tmr
    test_rx_encoder_tpu
    test_rx_host_irq
    test_rx_stack_monitor
    test_rx_bno055
    test_rx_bmp280
    test_rx_log_uart
    test_rx_log
    # System config test
    test_rx_system_config
    # Multi-task firmware tests
    test_motor_control_task
    test_communication_task
    test_obstacle_detection_task
    test_temperature_sensing_task
    test_telemetry_aggregation_task
    test_imu_task
)

add_test(NAME test_rx_crc32 COMMAND test_rx_crc32)
add_test(NAME test_rx_crc8 COMMAND test_rx_crc8)
add_test(NAME test_rx_crc COMMAND test_rx_crc)
add_test(NAME test_rx_frame COMMAND test_rx_frame)
add_test(NAME test_frame_go_compat COMMAND test_frame_go_compat)
add_test(NAME test_rx_fec COMMAND test_rx_fec)
add_test(NAME test_rx_harq COMMAND test_rx_harq)
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
add_test(NAME test_rx_i2c_comm COMMAND test_rx_i2c_comm)
add_test(NAME test_rx_uart_comm COMMAND test_rx_uart_comm)
add_test(NAME test_rx_iwdt COMMAND test_rx_iwdt)
add_test(NAME test_rx_cac COMMAND test_rx_cac)
add_test(NAME test_rx_doc COMMAND test_rx_doc)
add_test(NAME test_rx_eccram COMMAND test_rx_eccram)
add_test(NAME test_rx_lpc COMMAND test_rx_lpc)
add_test(NAME test_rx_gpio COMMAND test_rx_gpio)
add_test(NAME test_rx_mpc COMMAND test_rx_mpc)
add_test(NAME test_rx_poeg COMMAND test_rx_poeg)
add_test(NAME test_rx_led_status COMMAND test_rx_led_status)
add_test(NAME test_rx_dmaca COMMAND test_rx_dmaca)
add_test(NAME test_rx_tpu COMMAND test_rx_tpu)
add_test(NAME test_rx_tmr COMMAND test_rx_tmr)
add_test(NAME test_rx_encoder_tpu COMMAND test_rx_encoder_tpu)
add_test(NAME test_rx_host_irq COMMAND test_rx_host_irq)
add_test(NAME test_rx_stack_monitor COMMAND test_rx_stack_monitor)
add_test(NAME test_rx_bno055 COMMAND test_rx_bno055)
add_test(NAME test_rx_bmp280 COMMAND test_rx_bmp280)
add_test(NAME test_rx_log_uart COMMAND test_rx_log_uart)
add_test(NAME test_rx_log COMMAND test_rx_log)
add_test(NAME test_rx_system_config COMMAND test_rx_system_config)

# Multi-task firmware tests
add_test(NAME test_motor_control_task COMMAND test_motor_control_task)
add_test(NAME test_communication_task COMMAND test_communication_task)
add_test(NAME test_obstacle_detection_task COMMAND test_obstacle_detection_task)
add_test(NAME test_temperature_sensing_task COMMAND test_temperature_sensing_task)
add_test(NAME test_telemetry_aggregation_task COMMAND test_telemetry_aggregation_task)
add_test(NAME test_imu_task COMMAND test_imu_task)

# =============================================================================
# Coverage Baseline: compile all firmware source files
#
# Every source file in libs/ and src/ that is not already compiled by another
# test is compiled here with the same flags (--coverage, UNIT_TEST,
# RX_SIMULATOR_MODE).  The resulting .gcno instrumentation files allow
# lcov --initial to report 0% coverage for untested code paths, making all
# firmware files visible in the HTML coverage report.
#
# Files guarded entirely by #ifdef __RX__ will compile but contribute 0
# instrumented lines -- they appear in the report as "N/A" (no executable
# lines on the host), which is correct: those paths require the hardware build.
#
# Boot files (smc_generated, r_bsp, resetprg, vecttbl, etc.) depend on
# GNURX-specific startup ABI and are excluded; they are tested by the
# cross-compile CI job instead.
# =============================================================================

# Task source files not linked into any other test executable
set(COVERAGE_EXTRA_TASK_SRCS
    ${CMAKE_SOURCE_DIR}/../src/tasks/motor_control_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/obstacle_detect_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/temp_sensor_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/telemetry_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/imu_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/led_status_task.c
    ${CMAKE_SOURCE_DIR}/../src/tasks/watchdog_monitor_task.c
)

# Core runtime files not linked into any other test executable.
# rx_wdt.c, rx_time_threadx.c, rx_exception.c, rx_infrastructure.c are
# hardware/RTOS-specific (depend on __RX__ registers or ThreadX internals)
# and cannot be compiled on the host test build; they are tested by the
# cross-compile CI job.  The UART log backend (rx_log_uart.c) is the only
# host-compilable log source now.
set(COVERAGE_EXTRA_CORE_SRCS
    ${CMAKE_SOURCE_DIR}/../libs/rx_core/src/rx_log_uart.c
)

# HAL and USB ISR source files are replaced by mocks in all other tests and
# depend on hardware-specific register types that are not available in the
# host build (guarded by #ifdef __RX__ or __attribute__((interrupt))).
# They are excluded from host coverage; coverage for HAL is achieved through
# the mock-based tests that exercise the HAL interface contract.
# (No COVERAGE_EXTRA_HAL_SRCS or COVERAGE_EXTRA_USB_SRCS intentionally.)

# Remaining source files compilable on host
set(COVERAGE_EXTRA_MISC_SRCS
    ${CMAKE_SOURCE_DIR}/../src/shared/shared_data.c
)

# OBJECT library: compiles all coverage-extra files with host flags + --coverage.
# Not linked into a runnable test; .gcno files it produces are sufficient for
# lcov --initial to register each source file in the coverage database.
add_library(firmware_coverage_obj OBJECT
    ${COVERAGE_EXTRA_TASK_SRCS}
    ${COVERAGE_EXTRA_CORE_SRCS}
    ${COVERAGE_EXTRA_MISC_SRCS}
)

apply_strict_warnings(firmware_coverage_obj)
apply_test_includes(firmware_coverage_obj)

# Additional includes required by task sources
target_include_directories(firmware_coverage_obj PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
    ${CMAKE_SOURCE_DIR}/../src/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_ds18b20/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_bno055/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_bmp280/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_drv8263/inc
    ${CMAKE_SOURCE_DIR}/../libs/threadx
    ${CMAKE_SOURCE_DIR}/mocks
)

# firmware_coverage_obj compiles real task/shared sources on the host.
# All root-cause warnings have been fixed; no blanket suppression needed.
# (See shared_data.c tx_inherit_option cast, tx_api.h declarations, rx_simulator_config.h pragma.)

target_compile_definitions(firmware_coverage_obj PRIVATE
    MOCK_TX_THREAD_CREATE
    MOCK_TX_EVENT_FLAGS_CREATE
    USE_MOCK_SCI_REGS
    UNIT_TEST
    RX_SIMULATOR_MODE
)

# Dummy test that links firmware_coverage_obj so CMake actually compiles it.
# The test body is empty; its sole purpose is to ensure .gcno files exist for
# all extra firmware sources so lcov --initial can account for them.
#
# NOTE: firmware_coverage_obj compiles the REAL implementations of:
#   - All task source files (motor_control_task.c, etc.)
#   - shared_data.c
#   - rx_log_usb.c
# Therefore we must NOT include their mock equivalents here (MOCK_TASKS_SRC,
# MOCK_SHARED_DATA_SRC, mock_rx_log.c), which would produce duplicate symbols.
# We use ${RX_LOG_SRC} (rx_log.c only) instead of the full MOCK_LOG_SRC set.
add_executable(test_coverage_compile_all
    test_coverage_compile_all.c
    $<TARGET_OBJECTS:firmware_coverage_obj>
    ${MOCK_TX_API_SRC}
    ${MOCK_COMM_MANAGER_SRC_TASK}
    ${MOCK_NANOPB_SRC}
    ${MOCK_UART_SRC}
    ${MOCK_HAL_SRC}
    ${MOCK_MOTOR_SRC}
    ${MOCK_RSPI_SRC}
    ${MOCK_GPTW_SRC}
    ${MOCK_DS18B20_SRC}
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_bmp280.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_rx_bno055.c
    ${CMAKE_SOURCE_DIR}/mocks/mock_coverage_stubs.c
)
apply_strict_warnings(test_coverage_compile_all)
apply_test_includes(test_coverage_compile_all)
target_include_directories(test_coverage_compile_all PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/shared
    ${CMAKE_SOURCE_DIR}/../src/tasks/inc
    ${CMAKE_SOURCE_DIR}/../src/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_ds18b20/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_bno055/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_bmp280/inc
    ${CMAKE_SOURCE_DIR}/../libs/rx_drv8263/inc
    ${CMAKE_SOURCE_DIR}/../libs/threadx
)
target_compile_definitions(test_coverage_compile_all PRIVATE
    MOCK_TX_THREAD_CREATE
    MOCK_TX_EVENT_FLAGS_CREATE
    USE_MOCK_SCI_REGS
    UNIT_TEST
    RX_SIMULATOR_MODE
)
target_link_libraries(test_coverage_compile_all unity)

list(APPEND ALL_TEST_TARGETS test_coverage_compile_all)
add_test(NAME test_coverage_compile_all COMMAND test_coverage_compile_all)

# =============================================================================
# Convenience Targets
# =============================================================================

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${ALL_TEST_TARGETS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)
