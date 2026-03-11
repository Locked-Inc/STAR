#############################################################################################################################
# file:  CMakeLists.txt
# brief: Multi-chip STM32F7 firmware build configuration.
#
# usage: Select target chip with -DSTM32_TARGET=<chip>:
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -DSTM32_TARGET=STM32F767xx -S . -B build-f767 -G Ninja
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -DSTM32_TARGET=STM32F746xx -S . -B build-f746 -G Ninja
#          cmake --build build-f767 -j$(nproc)
#          cmake --build build-f746 -j$(nproc)
#############################################################################################################################
cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Toolchain version gate -- C23 typed enums require arm-none-eabi-gcc >= 13
# ---------------------------------------------------------------------------
if(CMAKE_CROSSCOMPILING AND CMAKE_C_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "arm-none-eabi-gcc >= 13.0 required for C23 typed enums (found ${CMAKE_C_COMPILER_VERSION})")
endif()

###################### CHIP SELECTION ######################################
set(STM32_TARGET "STM32F767xx" CACHE STRING "Target STM32 device")
set_property(CACHE STM32_TARGET PROPERTY STRINGS STM32F767xx STM32F746xx)

message(STATUS "Building for: ${STM32_TARGET}")

###################### PER-CHIP CONFIGURATION ######################################
if(STM32_TARGET STREQUAL "STM32F767xx")
    set(CPU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ld/STM32F767ZITx_FLASH.ld")
    set(STARTUP_FILE "Startup/startup_stm32f767zitx.s")
elseif(STM32_TARGET STREQUAL "STM32F746xx")
    set(CPU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ld/STM32F746NGHx_FLASH.ld")
    set(STARTUP_FILE "Startup/startup_stm32f746ngxx.s")
else()
    message(FATAL_ERROR "Unknown STM32_TARGET: ${STM32_TARGET}. Valid values: STM32F767xx STM32F746xx")
endif()

###################### CONSTANTS ######################################
set(RUNTIME_LIBRARY_REDUCED_C        "--specs=nano.specs")
set(RUNTIME_LIBRARY_SYSCALLS_MINIMAL "--specs=nosys.specs")
#######################################################################

###################### PROJECT SETUP ######################################
project(star-stm32-firmware)
enable_language(ASM)

set(PROJECT_SOURCES
    ${STARTUP_FILE}
    Sources/main.c
    Sources/syscalls.c
    Sources/sysmem.c
    Sources/system_stm32f7xx.c
    Sources/usb_descriptors.c

    # Phase 1 library sources:
    libs/stm32_usb_cdc/src/stm32_usb_cdc.c

    # TinyUSB device stack (core + CDC class + DWC2 driver for STM32F7 OTG_HS)
    libs/tinyusb/src/tusb.c
    libs/tinyusb/src/common/tusb_fifo.c
    libs/tinyusb/src/device/usbd.c
    libs/tinyusb/src/device/usbd_control.c
    libs/tinyusb/src/class/cdc/cdc_device.c
    libs/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c

    # FreeRTOS kernel (Cortex-M7 port + heap_4 best-fit allocator)
    libs/freertos/tasks.c
    libs/freertos/queue.c
    libs/freertos/list.c
    libs/freertos/event_groups.c
    libs/freertos/stream_buffer.c
    libs/freertos/portable/GCC/ARM_CM7/r0p1/port.c
    libs/freertos/portable/MemMang/heap_4.c

    # STM32F7 HAL (required when USE_HAL_DRIVER is defined; stm32f7xx.h pulls it in)
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_cortex.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_rcc.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_rcc_ex.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_dma.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_gpio.c
    libs/stm32f7xx_hal_driver/Src/stm32f7xx_hal_pwr_ex.c
)

add_executable(firmware ${PROJECT_SOURCES})

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

string(JOIN " " CPU_FLAGS_STR ${CPU_FLAGS})
set(CMAKE_C_FLAGS   "${CPU_FLAGS_STR} -std=gnu11 ${RUNTIME_LIBRARY_REDUCED_C} -Wall -Werror")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS
    "${CPU_FLAGS_STR} -T${LINKER_SCRIPT} ${RUNTIME_LIBRARY_SYSCALLS_MINIMAL} -Wl,-Map=firmware.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group"
)

target_include_directories(firmware PRIVATE
    Sources
    libs/stm32_err/inc
    libs/stm32_usb_cdc/inc
    libs/tinyusb/src
    libs/tinyusb/src/class/cdc
    libs/tinyusb/src/device
    libs/tinyusb/src/portable/synopsys/dwc2
    libs/freertos/include
    libs/freertos/portable/GCC/ARM_CM7/r0p1
    # ARM CMSIS-Core (core_cm7.h, cmsis_gcc.h, NVIC_SetPriority, etc.)
    libs/cmsis_5/CMSIS/Core/Include
    # ST CMSIS device headers (stm32f7xx.h -> stm32f767xx.h / stm32f746xx.h)
    libs/cmsis_device_f7/Include
    # ST HAL driver headers (pulled in by stm32f7xx.h when USE_HAL_DRIVER is defined)
    libs/stm32f7xx_hal_driver/Inc
    # TODO (Phase 2): libs/stm32_crc32/inc
    # TODO (Phase 2): libs/stm32_frame/inc
    # TODO (Phase 3): libs/nanopb, libs/stm32_nanopb/inc
    # TODO (Phase 4): libs/stm32_comm_manager/inc, libs/stm32_shared_data/inc
)

target_compile_definitions(firmware PRIVATE
    USE_HAL_DRIVER          # Makes stm32f7xx.h include stm32f7xx_hal.h
    ${STM32_TARGET}         # STM32F767xx or STM32F746xx -- selects device header in stm32f7xx.h
    STM32_USE_HSE=1         # Use 8 MHz HSE crystal for PLL (Nucleo-F767ZI); better USB accuracy
)

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
)

###################### TESTS ######################################
# Tests validate the cross-compiled ELF using host ARM binutils.
# Run AFTER building: ctest --test-dir <build_dir>
enable_testing()

# Sanity: ELF header is readable and is an ARM binary.
add_test(
    NAME firmware_elf_valid
    COMMAND arm-none-eabi-readelf -h firmware.elf
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Sanity: text section exists and is non-empty.
add_test(
    NAME firmware_text_nonzero
    COMMAND sh -c "arm-none-eabi-size --format=berkeley firmware.elf | awk 'NR==2 { exit ($1 > 0) ? 0 : 1 }'"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Sanity: Reset_Handler symbol is present (startup code linked in).
add_test(
    NAME firmware_has_reset_handler
    COMMAND sh -c "arm-none-eabi-nm firmware.elf | grep -q Reset_Handler"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Sanity: main symbol is present.
add_test(
    NAME firmware_has_main
    COMMAND sh -c "arm-none-eabi-nm firmware.elf | grep -q ' T main'"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
