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

# Compiler warnings (strict, but not -Werror for third-party code)
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-missing-braces  # nanopb generates these
    -Wno-comment  # ThreadX has nested comments in assembly
)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g3)
else()
    add_compile_options(-O2 -g)
endif()

# =============================================================================
# Source Files
# =============================================================================

set(SOURCES
    src/main.c
    src/hardware/startup_rx72n.S

    # Hardware initialization
    src/hardware/system_init.c
    src/hardware/gpio.c
    src/hardware/timer.c
    src/hardware/uart.c

    # ThreadX tasks (will be added)
    # src/tasks/motor_control_task.c
    # src/tasks/spi_task.c
    # src/tasks/telemetry_task.c

    # RX72N drivers (will be added)
    # src/drivers/mtu3a.c          # Motor PWM
    # src/drivers/s12adf.c         # ADC
    # src/drivers/rspi.c           # SPI
)

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # ThreadX will be added here
)

# =============================================================================
# ThreadX Integration
# =============================================================================

# Configure ThreadX for RXv3 with GNU toolchain
set(THREADX_ARCH "rxv3")
set(THREADX_TOOLCHAIN "gnu")

# Add ThreadX library
add_subdirectory(lib/threadx)

# =============================================================================
# Executable Target
# =============================================================================

add_executable(${PROJECT_NAME}.elf ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE
    threadx
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
