##############################################################
# file:  toolchain-arm-linux-gnueabihf.cmake
# brief: CMake toolchain file for cross-compiling to
#        BeagleBone Blue (ARM Cortex-A8, hard-float ABI).
#
# Install toolchain on Ubuntu/Debian:
#   sudo apt install gcc-arm-linux-gnueabihf
#
# Install toolchain on macOS (Homebrew):
#   brew install arm-linux-gnueabihf-binutils
#   # Use Crosstool-NG or Docker for full sysroot
##############################################################

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
set(CROSS_COMPILE arm-linux-gnueabihf-)

find_program(CMAKE_C_COMPILER   NAMES ${CROSS_COMPILE}gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${CROSS_COMPILE}g++   REQUIRED)
find_program(CMAKE_AR           NAMES ${CROSS_COMPILE}ar    REQUIRED)
find_program(CMAKE_STRIP        NAMES ${CROSS_COMPILE}strip REQUIRED)

# Cortex-A8 with VFPv3-d16 hard-float (matches BBB OSD3358 / AM335x)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard")

# Sysroot (optional -- set to BBB sysroot if available for full cross-link)
# set(CMAKE_SYSROOT /path/to/beaglebone-sysroot)
# set(CMAKE_FIND_ROOT_PATH /path/to/beaglebone-sysroot)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
