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
)

add_executable(firmware ${PROJECT_SOURCES})

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

string(JOIN " " CPU_FLAGS_STR ${CPU_FLAGS})
set(CMAKE_C_FLAGS   "${CPU_FLAGS_STR} -std=gnu11 ${RUNTIME_LIBRARY_REDUCED_C} -Wall -Werror")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS
    "${CPU_FLAGS_STR} -T${LINKER_SCRIPT} ${RUNTIME_LIBRARY_SYSCALLS_MINIMAL} -Wl,-Map=firmware.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group"
)

target_compile_definitions(firmware PRIVATE
    USE_HAL_DRIVER
    ${STM32_TARGET}
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
