# GNURX Toolchain File for Renesas RX72N
#
# This toolchain file configures CMake to use the GNURX compiler (GCC for Renesas RX).
# It supports both Windows and WSL environments.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/gnurx-toolchain.cmake ..
#
# For CLion:
#   File → Settings → Build, Execution, Deployment → CMake
#   CMake options: -DCMAKE_TOOLCHAIN_FILE=cmake/gnurx-toolchain.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR rx)

# GNURX Compiler Location
# Update this path if your installation differs
if(WIN32)
    # Native Windows build
    set(GNURX_TOOLCHAIN_PATH "C:/ProgramData/GCC for Renesas RX 14.2.0.202511-GNURX-ELF/rx-elf/rx-elf/bin")
elseif(UNIX AND NOT APPLE)
    # WSL build - detect if we can access Windows filesystem
    if(EXISTS "/mnt/c/ProgramData/GCC for Renesas RX 14.2.0.202511-GNURX-ELF")
        set(GNURX_TOOLCHAIN_PATH "/mnt/c/ProgramData/GCC for Renesas RX 14.2.0.202511-GNURX-ELF/rx-elf/rx-elf/bin")
    else()
        message(FATAL_ERROR "GNURX compiler not found. Please install GNURX or update GNURX_TOOLCHAIN_PATH in cmake/gnurx-toolchain.cmake")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform for GNURX cross-compilation")
endif()

# Set compilers
set(CMAKE_C_COMPILER "${GNURX_TOOLCHAIN_PATH}/rx-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${GNURX_TOOLCHAIN_PATH}/rx-elf-g++.exe")
set(CMAKE_ASM_COMPILER "${GNURX_TOOLCHAIN_PATH}/rx-elf-gcc.exe")
set(CMAKE_AR "${GNURX_TOOLCHAIN_PATH}/rx-elf-ar.exe")
set(CMAKE_RANLIB "${GNURX_TOOLCHAIN_PATH}/rx-elf-ranlib.exe")
set(CMAKE_OBJCOPY "${GNURX_TOOLCHAIN_PATH}/rx-elf-objcopy.exe")
set(CMAKE_OBJDUMP "${GNURX_TOOLCHAIN_PATH}/rx-elf-objdump.exe")
set(CMAKE_SIZE "${GNURX_TOOLCHAIN_PATH}/rx-elf-size.exe")

# Prevent CMake from testing the compilers (requires hardware to run)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

# Disable compiler checks that require running the compiled binary
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set CPU and architecture flags for RX72N
# Note: RX72N uses rx72t architecture with ISA v3
set(CPU_FLAGS "-mcpu=rx72t -misa=v3 -mlittle-endian-data -m64bit-doubles -mtfu=intrinsic,mathlib -mtfu-version=v2")

# Compiler flags (matching e² studio configuration)
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-strict-aliasing -fdiagnostics-parseable-fixits")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-strict-aliasing")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=rx72t -misa=v3 -mlittle-endian-data")

# Debug flags (matching e² studio)
set(CMAKE_C_FLAGS_DEBUG_INIT "-g2 -Og -Wstack-usage=100")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g2 -Og")

# Release flags
set(CMAKE_C_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcpu=rx72t -misa=v3 -mlittle-endian-data -m64bit-doubles -Wl,--gc-sections -nostartfiles")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Extensions
set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
