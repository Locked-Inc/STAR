# Toolchain file for Renesas RX72N with GNURX cross-compiler
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rx72n.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR rx)

# Cross-compiler paths
set(TOOLCHAIN_PREFIX rx-elf-)

# Find the toolchain
find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++ REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc REQUIRED)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size REQUIRED)

# Don't try to test the compiler (requires running on target hardware)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# RX72N specific flags (as single string, not list)
set(RX72N_CPU_FLAGS "-mcpu=rx72t -misa=v3 -mlittle-endian-data -m64bit-doubles -mtfu=intrinsic,mathlib -mtfu-version=v2 -mdfpu")

# Compiler flags
set(CMAKE_C_FLAGS_INIT "${RX72N_CPU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_INIT "${RX72N_CPU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS_INIT "${RX72N_CPU_FLAGS}")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${RX72N_CPU_FLAGS} -nostartfiles -Wl,--gc-sections -Wl,-e_PowerON_Reset")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
