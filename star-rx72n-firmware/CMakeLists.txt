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
)
target_include_directories(rx_bus PUBLIC lib/rx_bus/inc)
target_link_libraries(rx_bus PUBLIC rx_core rx_hal)

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

# rx_fuel_gauge - Battery management
add_library(rx_fuel_gauge STATIC
    lib/rx_fuel_gauge/src/rx_fuel_gauge.c
    lib/rx_fuel_gauge/src/rx_bq25798.c
)
target_include_directories(rx_fuel_gauge PUBLIC lib/rx_fuel_gauge/inc)
target_link_libraries(rx_fuel_gauge PUBLIC rx_bus rx_core)

# =============================================================================
# Application Source Files
# =============================================================================

set(APP_SOURCES
    src/main.c
    src/motor_shared_state.c
    src/tasks/motor_control_task.c
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
    rx_drv8243
    rx_fuel_gauge
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
message(STATUS "  rx_hal         - Hardware abstraction (MTU, CMT, GPIO, UART)")
message(STATUS "  rx_core        - Core infrastructure (error, logging, DIP)")
message(STATUS "  rx_bus         - Bus manager (GPIO, ADC, I2C, SMBUS)")
message(STATUS "  rx_motor       - Motor control (MTU3a PWM)")
message(STATUS "  rx_encoder     - Encoder (MTU phase counting)")
message(STATUS "  rx_pid         - PID controller")
message(STATUS "  rx_drv8243     - DRV8243 H-bridge driver")
message(STATUS "  rx_fuel_gauge  - Battery management (BQ25798)")
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
