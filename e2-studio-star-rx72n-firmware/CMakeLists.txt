cmake_minimum_required(VERSION 3.20)

# Project configuration
project(STAR_RX72N_Firmware
    VERSION 1.0.0
    LANGUAGES C ASM
    DESCRIPTION "STAR Robot RX72N Motor Controller Firmware"
)

# C standard (C23 as specified in e² studio configuration)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Toolchain: GNURX ${CMAKE_C_COMPILER_VERSION}")

#------------------------------------------------------------------------------
# Project-wide definitions
#------------------------------------------------------------------------------

# Add defines (matching e² studio configuration)
add_compile_definitions(
    # RX72N device configuration
    __RX72N__
    __RXV3__
    R5F572NNHxFB_DUAL

    # ThreadX configuration
    TX_ENABLE
    TX_INCLUDE_USER_DEFINE_FILE

    # Build configuration
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/inc/smc_generated
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
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_bq4050/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_bus/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_comm_manager/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_crc/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_drv8243/inc
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
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_spi_comm/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_usb/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/rx_usb_comm/inc
)

#------------------------------------------------------------------------------
# Compiler warnings (matching e² studio - currently warnings disabled)
#------------------------------------------------------------------------------

# Note: e² studio has most warnings disabled. Enable as needed:
# add_compile_options(-Wall -Wextra -Wconversion -Wshadow)

#------------------------------------------------------------------------------
# Source files
#------------------------------------------------------------------------------

# Main application sources
set(APP_SOURCES
    src/main.c
    src/hardware_init.c
    src/rx_clock_power_init.c
    src/shared/shared_data.c
)

# Boot and startup code
set(BOOT_SOURCES
    src/boot/default_interrupt_handlers.c
    src/boot/smc_generated/lowlvl.c
    src/boot/smc_generated/lowsrc.c
    src/boot/smc_generated/resetprg.c
    src/boot/smc_generated/vecttbl.c
    # Note: dbsct.c is CCRX-specific, not compiled for GNURX
)

# SMC Generated code
set(SMC_SOURCES
    src/smc_gen/r_pincfg/Pin.c
)

# Task sources
set(TASK_SOURCES
    src/tasks/app_main_task.c
    src/tasks/bms_monitor_task.c
    src/tasks/comm_task.c
    src/tasks/led_status_task.c
    src/tasks/motor_control_task.c
    src/tasks/obstacle_detect_task.c
    src/tasks/telemetry_task.c
    src/tasks/temp_sensor_task.c
)

# ThreadX RTOS sources
file(GLOB THREADX_COMMON_SOURCES "libs/threadx/common/src/*.c")
set(THREADX_PORT_SOURCES
    libs/threadx/ports/rxv3/gnu/src/tx_initialize_low_level.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_context_restore.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_context_save.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_interrupt_control.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_schedule.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_stack_build.S
    libs/threadx/ports/rxv3/gnu/src/tx_thread_system_return.S
    libs/threadx/ports/rxv3/gnu/src/tx_timer_interrupt.S
)

# nanopb Protocol Buffers
set(NANOPB_SOURCES
    libs/rx_nanopb/nanopb/pb_common.c
    libs/rx_nanopb/nanopb/pb_decode.c
    libs/rx_nanopb/nanopb/pb_encode.c
    libs/rx_nanopb/src/rx_nanopb.c
)

# Generated protobuf sources
file(GLOB PROTOBUF_GEN_SOURCES "libs/rx_nanopb/inc/gen/**/*.pb.c")

# Custom library sources
set(LIB_SOURCES
    # BQ4050 Battery Monitor
    libs/rx_bq4050/src/rx_bq4050.c

    # Bus Manager and Drivers
    libs/rx_bus/src/rx_bus_adc.c
    libs/rx_bus/src/rx_bus_config.c
    libs/rx_bus/src/rx_bus_gpio.c
    libs/rx_bus/src/rx_bus_i2c.c
    libs/rx_bus/src/rx_bus_manager.c
    libs/rx_bus/src/rx_bus_onewire.c
    libs/rx_bus/src/rx_bus_smbus.c
    libs/rx_bus/src/rx_bus_uart.c

    # Communication Manager
    libs/rx_comm_manager/src/rx_comm_manager.c

    # Core utilities
    libs/rx_core/src/rx_error_handler.c
    libs/rx_core/src/rx_exception.c
    libs/rx_core/src/rx_infrastructure.c
    libs/rx_core/src/rx_iwdt.c
    libs/rx_core/src/rx_log_usb.c
    libs/rx_core/src/rx_pin_validator.c
    libs/rx_core/src/rx_register_guard.c
    libs/rx_core/src/rx_time_threadx.c
    libs/rx_core/src/rx_wdt.c

    # CRC
    libs/rx_crc/src/rx_crc32.c
    libs/rx_crc/src/rx_crc32_hw.c
    libs/rx_crc/src/rx_crc32_sw.c
    libs/rx_crc/src/rx_crc8.c

    # DRV8243 Motor Driver
    libs/rx_drv8243/src/rx_drv8243.c

    # DS18B20 Temperature Sensor
    libs/rx_ds18b20/src/rx_ds18b20.c

    # Encoder
    libs/rx_encoder/src/rx_encoder_tpu.c
    libs/rx_encoder/src/rx_mtu_encoder.c

    # Forward Error Correction
    libs/rx_fec/src/rx_fec.c

    # Framing
    libs/rx_frame/src/rx_frame.c
    libs/rx_frame_ascii/src/rx_frame_ascii.c

    # Hardware Abstraction Layer
    libs/rx_hal/src/adc.c
    libs/rx_hal/src/gpio.c
    libs/rx_hal/src/riic.c
    libs/rx_hal/src/rspi.c
    libs/rx_hal/src/rx_bms_alert.c
    libs/rx_hal/src/rx_cmt.c
    libs/rx_hal/src/rx_gptw.c
    libs/rx_hal/src/rx_host_irq.c
    libs/rx_hal/src/rx_irq_filter.c
    libs/rx_hal/src/rx_iwdt.c
    libs/rx_hal/src/rx_mpc.c
    libs/rx_hal/src/rx_mtu.c
    libs/rx_hal/src/rx_poeg.c
    libs/rx_hal/src/rx_tpu.c
    libs/rx_hal/src/timer.c
    libs/rx_hal/src/uart.c

    # HARQ
    libs/rx_harq/src/rx_harq.c

    # HC-SR04 Ultrasonic Sensor
    libs/rx_hcsr04/src/rx_hcsr04.c
    libs/rx_hcsr04/src/rx_hcsr04_hal_hw.c

    # Motor Control
    libs/rx_motor/src/rx_motor.c

    # Obstacle Detection
    libs/rx_obstacle_detect/src/rx_obstacle_detect.c

    # PID Controller
    libs/rx_pid/src/rx_pid.c

    # SPI Communication
    libs/rx_spi_comm/src/rx_spi_comm.c

    # USB
    libs/rx_usb/src/rx_usb.c
    libs/rx_usb/src/rx_usb_cdc.c
    libs/rx_usb/src/rx_usb_hw.c
    libs/rx_usb/src/rx_usb_isr.c

    # USB Communication
    libs/rx_usb_comm/src/rx_usb_comm.c
)

#------------------------------------------------------------------------------
# Executable target
#------------------------------------------------------------------------------

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${BOOT_SOURCES}
    ${SMC_SOURCES}
    ${TASK_SOURCES}
    ${THREADX_COMMON_SOURCES}
    ${THREADX_PORT_SOURCES}
    ${NANOPB_SOURCES}
    ${PROTOBUF_GEN_SOURCES}
    ${LIB_SOURCES}
)

# Linker script
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/linker_script.ld")
target_link_options(${PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map
)

# Make linker script a dependency
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

#------------------------------------------------------------------------------
# Post-build commands
#------------------------------------------------------------------------------

# Generate binary file
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMENT "Generating binary file: ${PROJECT_NAME}.bin"
)

# Generate hex file
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMENT "Generating hex file: ${PROJECT_NAME}.hex"
)

# Print size information
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Firmware size:"
)

#------------------------------------------------------------------------------
# Summary
#------------------------------------------------------------------------------

message(STATUS "==============================================")
message(STATUS "STAR RX72N Firmware Configuration Summary")
message(STATUS "==============================================")
message(STATUS "Processor: Renesas RX72N (RXv3)")
message(STATUS "Toolchain: GNURX (GCC for Renesas RX)")
message(STATUS "C Standard: C23")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Linker Script: ${LINKER_SCRIPT}")
message(STATUS "Output: ${PROJECT_NAME}.elf/.bin/.hex")
message(STATUS "==============================================")
