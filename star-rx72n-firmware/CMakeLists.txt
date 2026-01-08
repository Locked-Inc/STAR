# =============================================================================
# STAR RX72N Firmware - CMake Configuration
# ThreadX-based motor control for Renesas RX72N
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# Project definition
project(star-rx72n-firmware
    VERSION 1.0.0
    LANGUAGES C ASM
    DESCRIPTION "STAR Robot RX72N Motor Control Firmware"
)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # GNU extensions for embedded

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# RX72N Configuration
# =============================================================================

set(MCU_PART_NUMBER "R5F572NNHGFP")
set(MCU_PACKAGE     "100-LFQFP")
set(MCU_FLASH_SIZE  "4M")
set(MCU_RAM_SIZE    "1M")
set(MCU_GPIO_COUNT  "182")

# RX72N specific compiler flags
add_compile_definitions(
    # MCU identification
    __RX72N__
    __RX_LITTLE_ENDIAN__=1

    # Memory configuration
    ROM_SIZE=0x400000    # 4MB Flash
    RAM_SIZE=0x100000    # 1MB RAM

    # STAR firmware version
    STAR_FIRMWARE_VERSION="1.0.0-rx72n"
)

# Compiler warnings (treat warnings as errors for clean code)
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror                # Treat warnings as errors
    -Wno-unused-parameter
    -Wno-missing-braces    # nanopb generates these
    -Wno-comment           # ThreadX has nested comments in assembly
    -fshort-enums          # Use smallest type for enums (embedded optimization)
)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g3)
else()
    add_compile_options(-O2 -g)
endif()

# =============================================================================
# ThreadX Integration
# =============================================================================

# Configure ThreadX for RXv3 with GNU toolchain
set(THREADX_ARCH "rxv3")
set(THREADX_TOOLCHAIN "gnu")

# Add ThreadX library
add_subdirectory(lib/threadx)

# =============================================================================
# Custom Libraries (following ESP32 structure)
# =============================================================================

# rx_core - Core infrastructure (DIP interfaces)
# Depends on ThreadX for RTOS primitives
add_library(rx_core STATIC
    lib/rx_core/src/rx_error_handler.c
    lib/rx_core/src/rx_pin_validator.c
    lib/rx_core/src/rx_infrastructure.c
    # Mock implementations available for testing (not compiled by default):
    # lib/rx_core/src/mock_error_handler.c
    # lib/rx_core/src/mock_pin_validator.c
)
target_include_directories(rx_core PUBLIC lib/rx_core/inc)
target_link_libraries(rx_core PUBLIC threadx)

# rx_hal - Hardware Abstraction Layer
# Depends on rx_core for error handling and logging, threadx for RTOS
add_library(rx_hal STATIC
    lib/rx_hal/src/startup_rx72n.S
    lib/rx_hal/src/system_init.c
    lib/rx_hal/src/gpio.c
    lib/rx_hal/src/uart.c
    lib/rx_hal/src/timer.c
    lib/rx_hal/src/adc.c
    lib/rx_hal/src/riic.c
    lib/rx_hal/src/rspi.c
    lib/rx_hal/src/rx_mtu3a.c
    lib/rx_hal/src/rx_gptw.c
    lib/rx_hal/src/rx_cmt.c
    lib/rx_hal/src/rx_mpc.c
)
target_include_directories(rx_hal PUBLIC
    lib/rx_hal/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # For tx_user.h
)
target_link_libraries(rx_hal PUBLIC rx_core threadx)

# rx_bus - Bus manager and peripherals
add_library(rx_bus STATIC
    lib/rx_bus/src/rx_bus_manager.c
    lib/rx_bus/src/rx_bus_config.c
    lib/rx_bus/src/rx_bus_gpio.c
    lib/rx_bus/src/rx_bus_adc.c
    lib/rx_bus/src/rx_bus_i2c.c
    lib/rx_bus/src/rx_bus_smbus.c
    lib/rx_bus/src/rx_bus_onewire.c
    lib/rx_bus/src/rx_bus_uart.c
)
target_include_directories(rx_bus PUBLIC lib/rx_bus/inc)
target_link_libraries(rx_bus PUBLIC rx_core rx_hal rx_crc)

# rx_encoder - MTU encoder driver
add_library(rx_encoder STATIC
    lib/rx_encoder/src/rx_mtu_encoder.c
)
target_include_directories(rx_encoder PUBLIC lib/rx_encoder/inc)
target_link_libraries(rx_encoder PUBLIC rx_hal rx_core)

# rx_pid - PID controller (pure algorithm, no dependencies)
add_library(rx_pid STATIC
    lib/rx_pid/src/rx_pid.c
)
target_include_directories(rx_pid PUBLIC lib/rx_pid/inc)
target_link_libraries(rx_pid PUBLIC rx_core)

# rx_fec - Forward Error Correction (convolutional encoder + Viterbi decoder)
add_library(rx_fec STATIC
    lib/rx_fec/src/rx_fec.c
)
target_include_directories(rx_fec PUBLIC lib/rx_fec/inc)
target_link_libraries(rx_fec PUBLIC rx_core)

# rx_harq - Hybrid Automatic Repeat Request (Chase Combining)
add_library(rx_harq STATIC
    lib/rx_harq/src/rx_harq.c
)
target_include_directories(rx_harq PUBLIC lib/rx_harq/inc)
target_link_libraries(rx_harq PUBLIC rx_fec rx_core)

# rx_crc - Shared CRC helpers
add_library(rx_crc STATIC
    lib/rx_crc/src/rx_crc32.c
    lib/rx_crc/src/rx_crc32_hw.c
    lib/rx_crc/src/rx_crc32_sw.c
    lib/rx_crc/src/rx_crc8.c
)
target_include_directories(rx_crc PUBLIC lib/rx_crc/inc)
target_link_libraries(rx_crc PUBLIC rx_core rx_hal)

# rx_frame - Frame Layer (SYNC + header + CRC usage)
add_library(rx_frame STATIC
    lib/rx_frame/src/rx_frame.c
)
target_include_directories(rx_frame PUBLIC lib/rx_frame/inc)
target_link_libraries(rx_frame PUBLIC rx_core rx_hal rx_crc)

# rx_nanopb - nanopb Protocol Buffers Integration
add_library(rx_nanopb STATIC
    lib/rx_nanopb/src/rx_nanopb.c
    lib/rx_nanopb/nanopb/pb_common.c
    lib/rx_nanopb/nanopb/pb_encode.c
    lib/rx_nanopb/nanopb/pb_decode.c
    lib/rx_nanopb/inc/gen/google/protobuf/timestamp.pb.c
    lib/rx_nanopb/inc/gen/google/protobuf/duration.pb.c
    lib/rx_nanopb/inc/gen/star/v1/common.pb.c
    lib/rx_nanopb/inc/gen/star/v1/motor_control.pb.c
    lib/rx_nanopb/inc/gen/star/v1/telemetry.pb.c
)
target_include_directories(rx_nanopb PUBLIC
    lib/rx_nanopb/inc
    lib/rx_nanopb/inc/gen
    lib/rx_nanopb/nanopb
)
# RX72N uses 4-byte double (same as float) - tell nanopb to handle conversion
target_compile_definitions(rx_nanopb PUBLIC PB_CONVERT_DOUBLE_FLOAT=1)
target_link_libraries(rx_nanopb PUBLIC rx_core)

# rx_spi_comm - High-Level SPI Communication Layer
add_library(rx_spi_comm STATIC
    lib/rx_spi_comm/src/rx_spi_comm.c
)
target_include_directories(rx_spi_comm PUBLIC lib/rx_spi_comm/inc)
target_link_libraries(rx_spi_comm PUBLIC rx_frame rx_hal rx_core)

# rx_usb - USB CDC-ACM Driver for RX72N
add_library(rx_usb STATIC
    lib/rx_usb/src/rx_usb.c
    lib/rx_usb/src/rx_usb_hw.c
    lib/rx_usb/src/rx_usb_cdc.c
    lib/rx_usb/src/rx_usb_isr.c
)
target_include_directories(rx_usb PUBLIC lib/rx_usb/inc)
target_link_libraries(rx_usb PUBLIC rx_core rx_hal)
target_compile_options(rx_usb PRIVATE -Wno-unused-function)

# rx_usb_comm - USB CDC Communication Layer
add_library(rx_usb_comm STATIC
    lib/rx_usb_comm/src/rx_usb_comm.c
)
target_include_directories(rx_usb_comm PUBLIC lib/rx_usb_comm/inc)
target_link_libraries(rx_usb_comm PUBLIC rx_core rx_frame rx_usb)

# rx_motor - MTU3a motor control
add_library(rx_motor STATIC
    lib/rx_motor/src/rx_motor.c
)
target_include_directories(rx_motor PUBLIC lib/rx_motor/inc)
target_link_libraries(rx_motor PUBLIC rx_hal rx_core)

# rx_drv8243 - DRV8243 driver integration
add_library(rx_drv8243 STATIC
    lib/rx_drv8243/src/rx_drv8243.c
)
target_include_directories(rx_drv8243 PUBLIC lib/rx_drv8243/inc)
target_link_libraries(rx_drv8243 PUBLIC rx_bus rx_motor rx_core)

# rx_bq4050 - BQ4050 battery fuel gauge (Smart Battery System)
add_library(rx_bq4050 STATIC
    lib/rx_bq4050/src/rx_bq4050.c
)
target_include_directories(rx_bq4050 PUBLIC lib/rx_bq4050/inc)
target_link_libraries(rx_bq4050 PUBLIC rx_bus rx_core)

# rx_ds18b20 - DS18B20 1-Wire temperature sensor
add_library(rx_ds18b20 STATIC
    lib/rx_ds18b20/src/rx_ds18b20.c
)
target_include_directories(rx_ds18b20 PUBLIC lib/rx_ds18b20/inc)
target_link_libraries(rx_ds18b20 PUBLIC rx_bus rx_crc rx_core)

# rx_hcsr04 - HC-SR04 ultrasonic distance sensor driver
add_library(rx_hcsr04 STATIC
    lib/rx_hcsr04/src/rx_hcsr04.c
    lib/rx_hcsr04/src/rx_hcsr04_hal_hw.c
)
target_include_directories(rx_hcsr04 PUBLIC lib/rx_hcsr04/inc)
target_link_libraries(rx_hcsr04 PUBLIC rx_hal rx_core)

# rx_obstacle_detect - Basic obstacle detection with emergency stop
add_library(rx_obstacle_detect STATIC
    lib/rx_obstacle_detect/src/rx_obstacle_detect.c
)
target_include_directories(rx_obstacle_detect PUBLIC lib/rx_obstacle_detect/inc)
target_link_libraries(rx_obstacle_detect PUBLIC rx_hcsr04 rx_motor rx_core)

# =============================================================================
# Application Source Files
# =============================================================================

set(APP_SOURCES
    src/main.c
    src/shared_state.c
    src/hardware_init.c
)

# =============================================================================
# Executable Target
# =============================================================================

add_executable(${PROJECT_NAME}.elf ${APP_SOURCES})

# Application include directories
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tasks
)

# Link all libraries
target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE
    threadx
    rx_hal
    rx_core
    rx_bus
    rx_motor
    rx_encoder
    rx_pid
    rx_fec
    rx_harq
    rx_frame
    rx_nanopb
    rx_spi_comm
    rx_usb
    rx_usb_comm
    rx_drv8243
    rx_bq4050
    rx_ds18b20
    rx_hcsr04
    rx_obstacle_detect
)

# Linker script
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_FLAGS "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/rx72n.ld -Wl,-Map=${PROJECT_NAME}.map,--cref,--no-warn-mismatch"
)

# =============================================================================
# Post-Build: Generate .hex and .bin files
# =============================================================================

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating HEX and BIN files, showing size..."
)

# =============================================================================
# Print Configuration Summary
# =============================================================================

message(STATUS "==============================================")
message(STATUS "STAR RX72N Firmware Configuration")
message(STATUS "==============================================")
message(STATUS "MCU:              ${MCU_PART_NUMBER}")
message(STATUS "Package:          ${MCU_PACKAGE}")
message(STATUS "GPIO Pins:        ${MCU_GPIO_COUNT}")
message(STATUS "Flash:            ${MCU_FLASH_SIZE}")
message(STATUS "RAM:              ${MCU_RAM_SIZE}")
message(STATUS "Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler:       ${CMAKE_C_COMPILER}")
message(STATUS "==============================================")
message(STATUS "Libraries:")
message(STATUS "  rx_hal         - Hardware abstraction (GPTW, MTU, CMT, GPIO, UART)")
message(STATUS "  rx_core        - Core infrastructure (error, logging, DIP)")
message(STATUS "  rx_bus         - Bus manager (GPIO, ADC, I2C, SMBUS)")
message(STATUS "  rx_motor       - Motor control (GPTW PWM)")
message(STATUS "  rx_encoder     - Encoder (MTU phase counting)")
message(STATUS "  rx_pid         - PID controller")
message(STATUS "  rx_fec         - FEC codec (K=7 convolutional + Viterbi)")
message(STATUS "  rx_harq        - HARQ protocol (Chase Combining)")
message(STATUS "  rx_frame       - Frame layer (SYNC, CRC-32, header)")
message(STATUS "  rx_nanopb      - Protocol Buffers (nanopb integration)")
message(STATUS "  rx_spi_comm    - SPI communication layer")
message(STATUS "  rx_usb         - USB CDC-ACM driver")
message(STATUS "  rx_usb_comm    - USB communication layer")
message(STATUS "  rx_drv8243     - DRV8243 H-bridge driver")
message(STATUS "  rx_bq4050      - BQ4050 battery fuel gauge (SBS 1.1)")
message(STATUS "  rx_ds18b20     - DS18B20 1-Wire temperature sensor")
message(STATUS "  rx_hcsr04      - HC-SR04 ultrasonic distance sensor")
message(STATUS "  rx_obstacle_detect - Obstacle detection with emergency stop")
message(STATUS "==============================================")

# =============================================================================
# Hardware Access (Flash/Debug)
# =============================================================================
#
# Professional embedded workflow:
# - BUILD in Docker (reproducible, cross-platform)
# - FLASH/DEBUG on host (requires USB hardware access)
#
# Use provided scripts instead of make targets:
#   ./flash.sh  - Flash firmware via E2 Lite (runs rfp-cli on host)
#   ./debug.sh  - Start GDB session (connects to J-Link/E2 on host)
#
# Why not 'make flash'?
# - Docker containers cannot access USB devices reliably
# - Host scripts provide better error messages and setup guidance
# - Consistent across Mac/Linux/Windows
#
# =============================================================================

message(STATUS "Flash/Debug:      Use ./flash.sh and ./debug.sh (host scripts)")
message(STATUS "                  Hardware access requires running on host, not in Docker")
