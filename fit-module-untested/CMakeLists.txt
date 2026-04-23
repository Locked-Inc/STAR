# fit-module-untested -- standalone RX72N sandbox using Renesas FIT modules.
#
# This tree intentionally does NOT depend on ../star-rx72n-firmware/ or
# ../star-proto/. Everything it needs is either in this directory or under
# vendor/FITModules/ (populated by scripts/fetch-fit-modules.sh).
#
# Build:
#   ./scripts/fetch-fit-modules.sh
#   PATH=/opt/gnurx/bin:$PATH cmake -B build -S . \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rx72n.cmake
#   cmake --build build -j

cmake_minimum_required(VERSION 3.20)
project(fit-module-untested C ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(VENDOR_FIT ${CMAKE_SOURCE_DIR}/vendor/FITModules)

if(NOT EXISTS ${VENDOR_FIT}/r_bsp)
    message(FATAL_ERROR
        "FIT modules not found at ${VENDOR_FIT}. "
        "Run ./scripts/fetch-fit-modules.sh first.")
endif()

# ----------------------------------------------------------------------------
# FIT module sources. Layout after fetch is two-deep -- each zip extracts to
#   vendor/FITModules/r_bsp/r_bsp/...
#   vendor/FITModules/r_bsp/r_config/...
# Source globs cover the per-MCU and shared bits we actually need.
# ----------------------------------------------------------------------------
set(FIT_BSP_SOURCES
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/dbsct.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/lowlvl.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/lowsrc.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/mcu_locks.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_common.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_cpu.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_interrupts.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_locking.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_mcu_startup.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_bsp_software_interrupt.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/r_rx_intrinsic_functions.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/reset_program.S
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/resetprg.c
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all/sbrk.c
    ${VENDOR_FIT}/r_bsp/r_bsp/board/generic_rx72n/hwsetup.c
)

# Per-MCU BSP files for RX72N
file(GLOB FIT_BSP_RX72N_SOURCES ${VENDOR_FIT}/r_bsp/r_bsp/mcu/rx72n/*.c)

file(GLOB FIT_SCI_SOURCES   ${VENDOR_FIT}/r_sci_rx/r_sci_rx/src/*.c)
file(GLOB FIT_SCI_TARGET    ${VENDOR_FIT}/r_sci_rx/r_sci_rx/src/targets/rx72n/*.c)

file(GLOB FIT_RIIC_SOURCES  ${VENDOR_FIT}/r_riic_rx/r_riic_rx/src/*.c)
file(GLOB FIT_RIIC_TARGET   ${VENDOR_FIT}/r_riic_rx/r_riic_rx/src/targets/rx72n/*.c)

file(GLOB FIT_BYTEQ_SOURCES ${VENDOR_FIT}/r_byteq/r_byteq/src/*.c)

# ----------------------------------------------------------------------------
# Application sources.
# motors.c is intentionally NOT here in v0 -- Renesas does not ship a FIT
# module for PWM/motor control on RX72N (no r_gpt_rx, no r_mtu3_rx).
# Direct-register GPTW glue lands once we've trusted FIT's BSP+SCI+RIIC path.
# ----------------------------------------------------------------------------
set(APP_SOURCES
    src/main.c
    src/hw_init.c
    src/bno055.c
    src/serial_proto.c
    src/crc8.c
    src/sci9_polled.c
)

add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${FIT_BSP_SOURCES}
    ${FIT_BSP_RX72N_SOURCES}
    ${FIT_SCI_SOURCES}
    ${FIT_SCI_TARGET}
    ${FIT_RIIC_SOURCES}
    ${FIT_RIIC_TARGET}
    ${FIT_BYTEQ_SOURCES}
)

# Include order matters: config/ FIRST so our overrides win over the vendor
# defaults. Then the FIT module include paths.
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_SOURCE_DIR}/src

    # r_bsp -- needs both the top-level include path and the per-board/per-MCU
    # paths because platform.h chains through both.
    ${VENDOR_FIT}/r_bsp/r_bsp
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/all
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/rx72n
    ${VENDOR_FIT}/r_bsp/r_bsp/mcu/rx72n/register_access/gnuc
    ${VENDOR_FIT}/r_bsp/r_bsp/board/generic_rx72n

    # SCI / RIIC / ByteQ public headers
    ${VENDOR_FIT}/r_sci_rx/r_sci_rx
    ${VENDOR_FIT}/r_sci_rx/r_sci_rx/src
    ${VENDOR_FIT}/r_riic_rx/r_riic_rx
    ${VENDOR_FIT}/r_riic_rx/r_riic_rx/src
    ${VENDOR_FIT}/r_byteq/r_byteq
    ${VENDOR_FIT}/r_byteq/r_byteq/src
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    # Tells r_bsp to pick the correct platform.h include path.
    BSP_BOARD_GENERIC_RX72N
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -Wall -Wextra
    # NOT -Werror -- vendor sources will trip warnings we don't own.
    -Os
    -g3
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/src/linker_script.ld
    -Wl,-Map=${PROJECT_NAME}.map
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O srec --srec-forceS3 --srec-len=32
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.mot
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.mot and reporting size"
)
