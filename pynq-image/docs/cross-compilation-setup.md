# Cross-Compilation Setup Guide

Complete guide for setting up C++ cross-compilation from x86_64 Ubuntu 22.04 to ARM Cortex-A9 (PYNQ-Z2).

## Toolchain Installation

### Install ARM Cross-Compiler

```bash
# Add armhf architecture support
sudo dpkg --add-architecture armhf
sudo apt update

# Install cross-compilation toolchain
sudo apt install -y \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    binutils-arm-linux-gnueabihf \
    libc6-dev:armhf \
    libstdc++6:armhf

# Install build tools
sudo apt install -y cmake make pkg-config

# Verify installation
arm-linux-gnueabihf-gcc --version
arm-linux-gnueabihf-g++ --version
```

**Expected output:**
```
arm-linux-gnueabihf-gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
```

**Important:** Ubuntu 22.04 ships with GCC 11.x for ARM. Since your PYNQ-Z2 also runs Ubuntu 22.04 Jammy, the glibc versions match (2.35), avoiding compatibility issues.

## CMake Toolchain File

Create `toolchain/arm-cortex-a9-toolchain.cmake`:

```cmake
# Target system specification
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Cross-compiler paths
set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)

# ARM Cortex-A9 specific flags (PYNQ-Z2)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb")

# Release build optimization
set(CMAKE_C_FLAGS_RELEASE "-O2 -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

### Compiler Flag Explanations

| Flag | Purpose | Value for PYNQ-Z2 |
|------|---------|-------------------|
| `-march` | Target architecture | `armv7-a` (ARMv7 instruction set) |
| `-mcpu` | Specific CPU model | `cortex-a9` (optimizes for A9 pipeline) |
| `-mfpu` | Floating-point unit | `neon` (SIMD acceleration) |
| `-mfloat-abi` | Float ABI convention | `hard` (FP in registers, faster) |
| `-mthumb` | Use Thumb-2 instructions | Reduces code size by ~30% |
| `-O2` | Optimization level | Balanced speed/size |
| `-ffunction-sections` | Separate functions | Enables dead code elimination |
| `-fdata-sections` | Separate data | Enables unused data removal |
| `-Wl,--gc-sections` | Linker flag | Remove unused sections (20-40% size reduction) |

## Project Directory Structure

```
star-robot/
├── CMakeLists.txt                    # Main build configuration
├── CMakePresets.json                 # Build presets (optional but recommended)
├── toolchain/
│   └── arm-cortex-a9-toolchain.cmake # ARM toolchain file
├── src/
│   ├── main.cpp                      # Main application
│   ├── lidar_interface.cpp           # SICK LiDAR communication
│   └── cola_protocol.cpp             # COLA protocol parser
├── include/
│   ├── lidar_interface.h
│   └── cola_protocol.h
├── build/                            # Host build (x86_64 for testing)
└── build-arm/                        # ARM cross-compiled build
```

## CMakeLists.txt Example

```cmake
cmake_minimum_required(VERSION 3.16)
project(star_robot CXX)

# C++14 standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/lidar_interface.cpp
    src/cola_protocol.cpp
)

# Executable
add_executable(star_robot ${SOURCES})

# Link pthread for std::thread
target_link_libraries(star_robot pthread)

# Installation
install(TARGETS star_robot DESTINATION bin)
```

## CMakePresets.json (Optional)

Create `CMakePresets.json` for easier build management:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "host-debug",
      "displayName": "Host Debug Build (x86_64)",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "arm-release",
      "displayName": "ARM Cross-Compile Release (Cortex-A9)",
      "binaryDir": "${sourceDir}/build-arm",
      "toolchainFile": "${sourceDir}/toolchain/arm-cortex-a9-toolchain.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "host-debug",
      "configurePreset": "host-debug"
    },
    {
      "name": "arm-release",
      "configurePreset": "arm-release"
    }
  ]
}
```

**Usage with presets:**
```bash
# Configure
cmake --preset=arm-release

# Build
cmake --build --preset=arm-release
```

## Build Workflows

### Host Build (for Development/Testing on x86_64)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./star_robot --test
```

### ARM Cross-Compilation

```bash
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-cortex-a9-toolchain.cmake ..
make -j$(nproc)
```

**Alternative with presets:**
```bash
cmake --preset=arm-release
cmake --build --preset=arm-release -j$(nproc)
```

### Verify ARM Binary

```bash
# Check architecture
file build-arm/star_robot
# Output: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked...

# Check dependencies
readelf -d build-arm/star_robot | grep NEEDED
# Output:
#  0x00000001 (NEEDED)             Shared library: [libpthread.so.0]
#  0x00000001 (NEEDED)             Shared library: [libstdc++.so.6]
#  0x00000001 (NEEDED)             Shared library: [libm.so.6]
#  0x00000001 (NEEDED)             Shared library: [libgcc_s.so.1]
#  0x00000001 (NEEDED)             Shared library: [libc.so.6]
```

## Handling Library Dependencies

### Dynamic Linking (Recommended)

Your PYNQ image already has all necessary runtime libraries. Use dynamic linking for:
- Smaller binary size (~50-200 KB vs 2-10 MB static)
- System library updates benefit your application
- Faster compilation and linking

**No additional configuration needed** - CMake defaults to dynamic linking.

### Static Linking (If Needed)

To create a fully static binary:

```cmake
# In CMakeLists.txt
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
```

**Trade-offs:**
- ✅ Single self-contained executable
- ✅ No runtime dependencies
- ❌ Much larger binary size (5-10x)
- ❌ Cannot benefit from system library updates

## Troubleshooting

### Error: "cannot find -lstdc++"

**Solution:** Install armhf C++ libraries:
```bash
sudo apt install libstdc++6:armhf
```

### Error: "No CMAKE_C_COMPILER could be found"

**Solution:** Verify toolchain file path:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=$(pwd)/toolchain/arm-cortex-a9-toolchain.cmake ..
```

### Error: Binary runs on host but not on target

**Solution:** Verify you're using the ARM binary, not the host binary:
```bash
file build-arm/star_robot  # Should show "ARM"
```

### glibc Version Mismatch

If your target has older glibc than host compiler:
```bash
# Check target glibc version
ssh star@<pynq-ip> ldd --version

# Use older compiler if needed (not required for Ubuntu 22.04 → Ubuntu 22.04)
```

## Best Practices

1. **Separate Build Directories**: Always use `build/` for host and `build-arm/` for ARM
2. **Clean Builds**: Run `rm -rf build-arm/*` when changing toolchain files
3. **Parallel Builds**: Use `-j$(nproc)` to leverage all CPU cores
4. **Verify Before Deploy**: Always check with `file` and `readelf` before transferring
5. **Version Control**: Add `build/` and `build-arm/` to `.gitignore`

## Next Steps

- [SICK TIM561 LiDAR Communication](./lidar-communication.md) - Implement sensor communication
- [Build & Deploy Workflow](./build-deploy-workflow.md) - Complete deployment pipeline
- [Embedded C++ Best Practices](./embedded-cpp-best-practices.md) - Optimization techniques
