# STAR Robot C++ Cross-Compilation Development Guide

Complete documentation for developing C++ applications on x86_64 Ubuntu 22.04 and cross-compiling for deployment on the PYNQ-Z2 (ARM Cortex-A9) board.

## Table of Contents

1. [Cross-Compilation Setup](./cross-compilation-setup.md) - Toolchain installation and CMake configuration
2. [SICK TIM561 LiDAR Communication](./lidar-communication.md) - COLA protocol and TCP/IP implementation
3. [Build & Deploy Workflow](./build-deploy-workflow.md) - Complete development to deployment pipeline
4. [Embedded C++ Best Practices](./embedded-cpp-best-practices.md) - Optimization and memory management
5. [Remote Debugging](./remote-debugging.md) - GDB debugging on ARM target
6. [PYNQ Image Integration](./pynq-image-integration.md) - Runtime dependencies and deployment

## Quick Start

### 1. Install Cross-Compilation Toolchain (One-Time Setup)

```bash
# On your x86_64 Ubuntu 22.04 development machine
sudo dpkg --add-architecture armhf
sudo apt update
sudo apt install -y \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    binutils-arm-linux-gnueabihf \
    cmake make pkg-config

# Verify installation
arm-linux-gnueabihf-gcc --version
arm-linux-gnueabihf-g++ --version
```

### 2. Create Project Structure

```bash
mkdir -p star-robot/{src,include,build,build-arm,toolchain}
cd star-robot
```

### 3. Configure CMake Toolchain

See [Cross-Compilation Setup](./cross-compilation-setup.md) for the complete CMake toolchain file.

### 4. Build for ARM

```bash
cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-cortex-a9-toolchain.cmake ..
make -j$(nproc)
```

### 5. Deploy to PYNQ-Z2

```bash
# Replace with your PYNQ-Z2 IP address
rsync -avz ./star-robot star@<pynq-ip>:/home/star/bin/
```

### 6. Run on Target

```bash
ssh star@<pynq-ip>
cd /home/star/bin
./star-robot
```

## Hardware Setup

- **Development Machine**: x86_64 Ubuntu 22.04 Jammy
- **Target Device**: PYNQ-Z2 (ARM Cortex-A9, 32-bit armhf)
- **Target OS**: Minimal Ubuntu 22.04 Jammy armhf
- **LiDAR Sensor**: SICK TIM561-2050101 (Ethernet connection)

## Key Features

- ✅ No build tools needed on PYNQ-Z2 (runtime-only deployment)
- ✅ Fast iteration: Compile on x86_64, deploy binaries
- ✅ Optimized for ARM Cortex-A9 with NEON SIMD
- ✅ Dynamic linking (uses system libraries already in minimal image)
- ✅ Complete SICK LiDAR communication without ROS2
- ✅ C++14 standard for embedded development
- ✅ Systemd service integration for auto-start

## Runtime Dependencies

Your minimal PYNQ image already includes all necessary C++ runtime libraries:

- ✅ `libc6` (GNU C Library)
- ✅ `libstdc++6` (C++ Standard Library)
- ✅ `libgcc-s1` (GCC support library)
- ✅ `libm.so.6` (Math library)
- ✅ `libpthread.so.0` (POSIX threads)

**No additional packages need to be added to multistrap.config!**

## Development Workflow

```
┌─────────────────────────────────────┐
│   Development Machine (x86_64)      │
│   ┌─────────────────────────────┐   │
│   │ 1. Edit C++ source code     │   │
│   │ 2. Cross-compile to ARM     │   │
│   │ 3. Verify with readelf      │   │
│   └─────────────────────────────┘   │
└────────────────┬────────────────────┘
                 │ rsync/scp
                 ▼
┌─────────────────────────────────────┐
│   PYNQ-Z2 Board (ARM Cortex-A9)     │
│   ┌─────────────────────────────┐   │
│   │ 4. Verify with ldd          │   │
│   │ 5. Run executable           │   │
│   │ 6. Debug with gdbserver     │   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

## Project Goals

- **Minimal Image**: No ROS2 overhead, runtime-only deployment
- **Fast Development**: Leverage x86_64 compilation speed
- **Real-Time Performance**: Optimized for ARM with NEON
- **Maintainability**: Modern C++14 with embedded best practices
- **Reliability**: Systemd service management, remote debugging

## Next Steps

1. Follow [Cross-Compilation Setup](./cross-compilation-setup.md) to configure your development environment
2. Read [SICK TIM561 LiDAR Communication](./lidar-communication.md) to implement sensor communication
3. Review [Embedded C++ Best Practices](./embedded-cpp-best-practices.md) for optimization strategies

## Contributing

For questions or issues with the PYNQ image build system, see the main repository README.

For C++ development questions, refer to the individual documentation files in this directory.
