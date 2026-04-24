cmake_minimum_required(VERSION 3.20)

# Project configuration
project(star-rx72n-firmware
    LANGUAGES C ASM
    VERSION 1.0.0
    DESCRIPTION "STAR RX72N Motor Controller Firmware"
)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")

# C Standard - C23 (GNU2x)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # Enable GNU extensions

# Export compile commands for clangd/tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#############################################################################
# Compiler Flags
#############################################################################

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "-O0 -g2")
set(CMAKE_ASM_FLAGS_DEBUG "-g2")

# Release flags
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0")
set(CMAKE_ASM_FLAGS_RELEASE "-g0")

#############################################################################
# Source Files
#############################################################################

# Main application sources (all custom boot code, no auto-generated BSP)
file(GLOB APP_SOURCES_ROOT CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.c
)
file(GLOB_RECURSE APP_BOOT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/boot/*.c
    ${CMAKE_SOURCE_DIR}/src/boot/*.S
)
file(GLOB_RECURSE APP_TASKS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/tasks/*.c)
# usb_task.c re-registered for feat/usb0 -- main.c instantiates it after
# the inline USB0 bring-up sequence and rx_usb_init() call.
file(GLOB_RECURSE APP_SHARED_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/shared/*.c)
file(GLOB_RECURSE APP_RTOS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/rtos_config/*.c)

# BSP intrinsic functions (low-level GNUC implementations)
set(BSP_INTRINSIC_SOURCES
    ${CMAKE_SOURCE_DIR}/src/boot/r_bsp/r_rx_intrinsic_functions.c
)

# Suppress specific warnings in BSP intrinsic functions (Renesas-provided code with inline assembly)
# -Wno-unused-parameter: Inline assembly macros may not use all declared parameters
set_source_files_properties(${BSP_INTRINSIC_SOURCES} PROPERTIES
    COMPILE_FLAGS "-Wno-unused-parameter -Wno-error"
)

#------------------------------------------------------------------------------
# Include directories (global)
#------------------------------------------------------------------------------

include_directories(
    # Main source includes
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/inc/custom
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks/inc

    # SMC Generated code
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen/r_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen/r_bsp/mcu/all
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen/r_bsp/mcu/rx72n
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen/r_config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smc_gen/r_pincfg

    # ThreadX
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/threadx
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/threadx/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/threadx/ports/rxv3/gnu/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/threadx/utility/low_power

    # nanopb
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_nanopb/nanopb
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_nanopb/inc/gen
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_nanopb/inc

    # Custom libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_bus/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_comm_manager/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_crc/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_dmaca/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_ds18b20/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_encoder/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_fec/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_frame/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_frame_ascii/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_hal/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_harq/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_hcsr04/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_motor/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_obstacle_detect/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_pid/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_sync/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_spi_comm/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_usb/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_usb/src
)

# Combine app sources (custom boot + intrinsic functions)
set(APP_SOURCES
    ${APP_SOURCES_ROOT}
    ${APP_BOOT_SOURCES}
    ${APP_TASKS_SOURCES}
    ${APP_SHARED_SOURCES}
    ${APP_RTOS_SOURCES}
    ${BSP_INTRINSIC_SOURCES}
)

# ThreadX RTOS
file(GLOB THREADX_COMMON_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/threadx/common/src/*.c)
file(GLOB THREADX_PORT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/threadx/ports/rxv3/gnu/src/*.c)
file(GLOB THREADX_PORT_ASM_RAW CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/threadx/ports/rxv3/gnu/src/*.S)

# Strip C-style /* ... */ block comments from upstream ThreadX port .S
# files before they reach rx-elf-gcc.  The upstream Eclipse ThreadX
# source (byte-identical to our vendored copy) uses CC-RX / GNU hybrid
# syntax: /* ... */ blocks are embedded INSIDE ;-prefixed assembler
# comment lines AND span multiple real executable instructions.  Under
# CC-RX the ; makes each line a comment; under GCC's cpp, ; is just a
# character and cpp eats everything from /* to */ -- deleting the
# system_state load + CMP + BEQ + ADD + MOV that implements the
# increment in __tx_thread_context_save.  Result: every CMT0 ISR
# unbalanced, tx_thread_sleep returns TX_CALLER_ERROR forever, tasks
# never run.  See memory project_threadx_rxv3_cpp_block_comment_bug.md
# and the matching pattern rule in motors_rtos/Makefile.
set(THREADX_PORT_ASM "")
foreach(RAW_ASM IN LISTS THREADX_PORT_ASM_RAW)
    get_filename_component(ASM_NAME ${RAW_ASM} NAME_WE)
    set(SED_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/threadx_asm/${ASM_NAME}.sed.S)
    add_custom_command(
        OUTPUT ${SED_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/threadx_asm
        COMMAND sed -e "s|/\\*||g" -e "s|\\*/||g" ${RAW_ASM} > ${SED_OUTPUT}
        DEPENDS ${RAW_ASM}
        COMMENT "Stripping /* */ from ThreadX ${ASM_NAME}.S for GCC cpp compatibility"
        VERBATIM
    )
    list(APPEND THREADX_PORT_ASM ${SED_OUTPUT})
endforeach()
file(GLOB THREADX_LOWPOWER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/threadx/utility/low_power/*.c)

# Custom libraries (using GLOB_RECURSE for src subdirectories)
file(GLOB_RECURSE LIB_RX_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_core/src/*.c)
# rx_log_usb.c is orphaned after dc6f3201b (includes rx_usb.h which was purged).
# rx_log_uart.c is the active log backend.
list(REMOVE_ITEM LIB_RX_CORE_SOURCES ${CMAKE_SOURCE_DIR}/libs/rx_core/src/rx_log_usb.c)
file(GLOB_RECURSE LIB_RX_HAL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libs/rx_hal/src/*.c
    ${CMAKE_SOURCE_DIR}/libs/rx_hal/src/*.S
)
file(GLOB_RECURSE LIB_RX_BUS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_bus/src/*.c)
file(GLOB_RECURSE LIB_RX_PID_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_pid/src/*.c)
file(GLOB_RECURSE LIB_RX_SYNC_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_sync/src/*.c)
file(GLOB_RECURSE LIB_RX_MOTOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_motor/src/*.c)
file(GLOB_RECURSE LIB_RX_ENCODER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_encoder/src/*.c)
file(GLOB_RECURSE LIB_RX_CRC_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_crc/src/*.c)
file(GLOB_RECURSE LIB_RX_DMACA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_dmaca/src/*.c)
file(GLOB_RECURSE LIB_RX_FRAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_frame/src/*.c)
file(GLOB_RECURSE LIB_RX_FRAME_ASCII_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_frame_ascii/src/*.c)
file(GLOB_RECURSE LIB_RX_FEC_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_fec/src/*.c)
file(GLOB_RECURSE LIB_RX_HARQ_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_harq/src/*.c)

file(GLOB_RECURSE LIB_RX_BMP280_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_bmp280/src/*.c)
file(GLOB_RECURSE LIB_RX_BNO055_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_bno055/src/*.c)
file(GLOB_RECURSE LIB_RX_DRV8263_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_drv8263/src/*.c)
file(GLOB_RECURSE LIB_RX_DS18B20_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_ds18b20/src/*.c)
file(GLOB_RECURSE LIB_RX_HCSR04_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_hcsr04/src/*.c)
file(GLOB_RECURSE LIB_RX_OBSTACLE_DETECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_obstacle_detect/src/*.c)
file(GLOB_RECURSE LIB_RX_SESSION_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_session/src/*.c)
file(GLOB_RECURSE LIB_RX_I2C_COMM_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_i2c_comm/src/*.c)
file(GLOB_RECURSE LIB_RX_SPI_COMM_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_spi_comm/src/*.c)
file(GLOB_RECURSE LIB_RX_SPI_LINK_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_spi_link/src/*.c)
file(GLOB_RECURSE LIB_RX_UART_COMM_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_uart_comm/src/*.c)
file(GLOB_RECURSE LIB_RX_COMM_MANAGER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_comm_manager/src/*.c)
file(GLOB_RECURSE LIB_RX_USB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_usb/src/*.c)

# nanopb (only core files, not tests/examples)
set(NANOPB_SOURCES
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/nanopb/pb_common.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/nanopb/pb_encode.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/nanopb/pb_decode.c
)

# nanopb wrapper
file(GLOB_RECURSE LIB_RX_NANOPB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/src/*.c)

# Generated protobuf files - only include messages used by firmware
set(PROTOBUF_SOURCES
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/common.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/configuration.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/gateway_service.pb.c  # KEEP: provides star_v1_SetPIDGainsRequest_fields (and related nanopb symbol registrations) used by rx_nanopb.c; removing this file breaks the link
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/diagnostics.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/motor_control.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/telemetry.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/star/v1/wire.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/google/protobuf/duration.pb.c
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen/google/protobuf/timestamp.pb.c
)

# Combine all sources
set(ALL_SOURCES
    ${APP_SOURCES}
    ${THREADX_COMMON_SOURCES}
    ${THREADX_PORT_SOURCES}
    ${THREADX_PORT_ASM}
    ${THREADX_LOWPOWER_SOURCES}
    ${LIB_RX_CORE_SOURCES}
    ${LIB_RX_HAL_SOURCES}
    ${LIB_RX_BUS_SOURCES}
    ${LIB_RX_PID_SOURCES}
    ${LIB_RX_SYNC_SOURCES}
    ${LIB_RX_MOTOR_SOURCES}
    ${LIB_RX_ENCODER_SOURCES}
    ${LIB_RX_CRC_SOURCES}
    ${LIB_RX_DMACA_SOURCES}
    ${LIB_RX_FRAME_SOURCES}
    ${LIB_RX_FRAME_ASCII_SOURCES}
    ${LIB_RX_FEC_SOURCES}
    ${LIB_RX_HARQ_SOURCES}
    ${LIB_RX_BMP280_SOURCES}
    ${LIB_RX_BNO055_SOURCES}
    ${LIB_RX_DRV8263_SOURCES}
    ${LIB_RX_DS18B20_SOURCES}
    ${LIB_RX_HCSR04_SOURCES}
    ${LIB_RX_OBSTACLE_DETECT_SOURCES}
    ${LIB_RX_SESSION_SOURCES}
    ${LIB_RX_I2C_COMM_SOURCES}
    ${LIB_RX_SPI_COMM_SOURCES}
    ${LIB_RX_SPI_LINK_SOURCES}
    ${LIB_RX_UART_COMM_SOURCES}
    ${LIB_RX_COMM_MANAGER_SOURCES}
    ${LIB_RX_USB_SOURCES}
    ${NANOPB_SOURCES}
    ${LIB_RX_NANOPB_SOURCES}
    ${PROTOBUF_SOURCES}
)

#############################################################################
# Executable Target
#############################################################################

# Find linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/linker_script.ld)
if(NOT EXISTS ${LINKER_SCRIPT})
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()

add_executable(${PROJECT_NAME}.elf ${ALL_SOURCES})

# Target-specific compile options (previously global add_compile_options)
# Note: -Wpedantic intentionally omitted - firmware uses GNU extensions (__attribute__,
# __asm__, __builtin_rx_*) required for BSP/boot code and interrupt handlers
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall
    -Wextra
    -Werror
    -fshort-enums
    $<$<COMPILE_LANGUAGE:C>:-Wstack-usage=2200>
    $<$<COMPILE_LANGUAGE:ASM>:-DCPPAPP>
)

#############################################################################
# Board variant selection
#
# PROD = STAR production PCB (24 MHz external crystal, MOSC -> PLL)
# TOM  = Tom's bench PCB (no crystal, HOCO -> PLL)
#
# Configure the build:
#   cmake -DSTAR_BOARD=PROD ..    # default
#   cmake -DSTAR_BOARD=TOM  ..    # for Tom's bench board
#
# The board variant is forwarded as a -DSTAR_BOARD_PROD or -DSTAR_BOARD_TOM
# preprocessor macro which r_bsp_config.h consumes to override clock
# sources.  Default is PROD so existing bench workflows keep building
# unchanged.
#############################################################################
set(STAR_BOARD "PROD" CACHE STRING "Target board variant: PROD (24 MHz crystal) or TOM (HOCO-only)")
set_property(CACHE STAR_BOARD PROPERTY STRINGS "PROD" "TOM")
if(NOT STAR_BOARD STREQUAL "PROD" AND NOT STAR_BOARD STREQUAL "TOM")
    message(FATAL_ERROR "STAR_BOARD must be PROD or TOM, got: ${STAR_BOARD}")
endif()
message(STATUS "Board variant: ${STAR_BOARD}")

# Target-specific compile definitions (previously global add_compile_definitions)
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    TX_INCLUDE_USER_DEFINE_FILE
    CPPAPP  # Enables C++ support in reset_program.S
    STAR_BOARD_${STAR_BOARD}=1   # STAR_BOARD_PROD=1 or STAR_BOARD_TOM=1
)

# Target-specific include directories (previously global include_directories)
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    # Project headers
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/inc
    ${CMAKE_SOURCE_DIR}/src/boot
    ${CMAKE_SOURCE_DIR}/src/boot/inc
    ${CMAKE_SOURCE_DIR}/src/boot/inc/custom
    ${CMAKE_SOURCE_DIR}/src/boot/inc/smc
    ${CMAKE_SOURCE_DIR}/src/tasks
    ${CMAKE_SOURCE_DIR}/src/tasks/inc
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/shared/inc
    ${CMAKE_SOURCE_DIR}/src/rtos_config

    # ThreadX RTOS
    ${CMAKE_SOURCE_DIR}/libs/threadx
    ${CMAKE_SOURCE_DIR}/libs/threadx/common/inc
    ${CMAKE_SOURCE_DIR}/libs/threadx/ports/rxv3/gnu/inc
    ${CMAKE_SOURCE_DIR}/libs/threadx/utility/low_power

    # Custom libraries
    ${CMAKE_SOURCE_DIR}/libs/rx_core/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_hal/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_bus/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_pid/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_sync/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_motor/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_encoder/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_crc/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_dmaca/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_frame/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_frame_ascii/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_fec/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_harq/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/inc/gen
    ${CMAKE_SOURCE_DIR}/libs/rx_nanopb/nanopb
    ${CMAKE_SOURCE_DIR}/libs/rx_bmp280/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_bno055/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_drv8263/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_ds18b20/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_hcsr04/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_obstacle_detect/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_session/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_i2c_comm/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_spi_comm/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_spi_link/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_uart_comm/inc
    ${CMAKE_SOURCE_DIR}/libs/rx_comm_manager/inc
)

# Set linker script.  `--undefined` forces the linker to treat the
# named symbols as unresolved references, which pulls their containing
# .text sub-sections in past --gc-sections.  Without this, the ThreadX
# CMT0 tick ISR in src/boot/cmt0_vector.S is dropped (the linker
# script's `DEFINED($tableentry$28$.rvectors)` check does not count as
# a GC root) and vector 28 ends up as 0 instead of the ISR address.
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,--Map=${PROJECT_NAME}.map
    "-Wl,--undefined=$tableentry$28$.rvectors"
    # Force-keep the SCI9_RXI9 interrupt vector. libs/rx_hal/src/sci9_rxi9_vector.S
    # defines $tableentry$102$.rvectors as an alias for _INT_SCI9_RXI9, but the
    # DEFINED() check in linker_script_rvectors.inc:108 isn't a GC root, so the
    # --gc-sections pass drops the alias and vector 102 falls through to
    # $tableentry$default$.rvectors (0xFFFFFFFF). Mirrors the CMT0 pattern above.
    "-Wl,--undefined=$tableentry$102$.rvectors"
)

#############################################################################
# Testing Infrastructure
#############################################################################

# Enable CTest for unit/integration testing
enable_testing()

# TODO: Add test targets
# Example: add_test(NAME unit_tests COMMAND simulator_test_runner)
# Tests can use simulator builds (RX_SIMULATOR_MODE) for logic validation
# See CLAUDE.md "Simulator Support" for details

#############################################################################
# Post-build: Generate Motorola S-Record (.mot) file
#############################################################################

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O srec -I elf32-rx-be-ns $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.mot
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.mot and showing size"
)

#############################################################################
# Print Summary
#############################################################################

message(STATUS "====================================")
message(STATUS "STAR RX72N Firmware Build Configuration")
message(STATUS "====================================")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Standard: C${CMAKE_C_STANDARD} (GNU extensions enabled)")
message(STATUS "Linker Script: ${LINKER_SCRIPT}")
message(STATUS "Source Files: ${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "====================================")
