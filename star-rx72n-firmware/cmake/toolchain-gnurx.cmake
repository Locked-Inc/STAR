# =============================================================================
# CMake Toolchain File for Renesas RX72N with GNURX (GCC for RX)
# =============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR rx)

# GNURX toolchain path - can be overridden via environment variable
if(DEFINED ENV{GNURX_ROOT})
    set(GNURX_ROOT $ENV{GNURX_ROOT})
else()
    set(GNURX_ROOT "/opt/gnurx")
endif()

message(STATUS "GNURX toolchain root: ${GNURX_ROOT}")

# Toolchain executables
set(CMAKE_C_COMPILER   "${GNURX_ROOT}/bin/rx-elf-gcc")
set(CMAKE_CXX_COMPILER "${GNURX_ROOT}/bin/rx-elf-g++")
set(CMAKE_ASM_COMPILER "${GNURX_ROOT}/bin/rx-elf-gcc")
set(CMAKE_AR           "${GNURX_ROOT}/bin/rx-elf-ar")
set(CMAKE_OBJCOPY      "${GNURX_ROOT}/bin/rx-elf-objcopy")
set(CMAKE_OBJDUMP      "${GNURX_ROOT}/bin/rx-elf-objdump")
set(CMAKE_SIZE         "${GNURX_ROOT}/bin/rx-elf-size")
set(CMAKE_RANLIB       "${GNURX_ROOT}/bin/rx-elf-ranlib")

# Target specifications for RX72N (RXv3 ISA via -mcpu=rxv3)
set(CPU_FLAGS "-mcpu=rxv3 -fpu -mlittle-endian-data")
set(ASM_CPU_FLAGS "-Wa,--mcpu=rxv3 -fpu -mlittle-endian-data")

# Compiler flags
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
set(CMAKE_ASM_FLAGS_INIT "${ASM_CPU_FLAGS}")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections -nostartfiles")

# Search for programs only in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers only in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Don't try to run test executables (we're cross-compiling)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

message(STATUS "Toolchain configured for RX72N with GNURX")
