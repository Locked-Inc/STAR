# Toolchain file for Renesas RX72N with GNURX cross-compiler.
# Copied verbatim from star-rx72n-firmware/cmake/toolchain-rx72n.cmake so this
# sandbox builds without depending on the production tree.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rx72n.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR rx)

set(TOOLCHAIN_PREFIX rx-elf-)

find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++ REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc REQUIRED)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size REQUIRED)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# RX72N CPU/ABI flags. NOTE: removed -mtfu-version=v2 vs the production
# toolchain -- r_bsp/mcu/rx72n/mcu_info.h declares BSP_MCU_TFU_VERSION=1, which
# expects GCC to expose __init_tfu() (TFU v1 intrinsic). Passing -mtfu-version=v2
# silently activates the v2 intrinsic ABI and __init_tfu disappears, breaking
# r_bsp's resetprg.c. Default (v1) matches r_bsp's expectation on RX72N.
set(RX72N_CPU_FLAGS "-mcpu=rx72t -misa=v3 -mlittle-endian-data -m64bit-doubles -mtfu=intrinsic,mathlib -mdfpu")

set(CMAKE_C_FLAGS_INIT "${RX72N_CPU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_INIT "${RX72N_CPU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS_INIT "${RX72N_CPU_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${RX72N_CPU_FLAGS} -nostartfiles -Wl,--gc-sections -Wl,-e_PowerON_Reset")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
