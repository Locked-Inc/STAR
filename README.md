# STAR Robot

STAR (Simultaneous Tracking And Robotics) is a LiDAR SLAM robot with remote control capabilities.

## Hardware

- **Main Board**: PYNQ-Z2 (Xilinx Zynq-7020 SoC) 
- **LiDAR**: For mapping and navigation
- **Cameras**: Stereo vision for depth perception
- **Controller**: Handheld remote (Retroid Pocket 2S)

## Software Architecture

```
Handheld Controller (Retroid Pocket 2S)
    ↓ [WiFi]
Robot Gateway (Java/Spring Boot) 
    ↓ [Commands & Telemetry]
PYNQ-Z2 Linux System
    ↓ [SLAM & Computer Vision]
LiDAR + Stereo Cameras
```

## Modules

- **`android-app/`**: Controller app for Retroid Pocket 2S
- **`robot-gateway/`**: Java gateway running on robot
- **`server-backend/`**: Data collection server
- **`yocto-build/`**: PYNQ-Z2 Linux image build system
- **`Schematic/`**: Hardware design files (KiCad)

## Features

- **LiDAR SLAM**: Real-time mapping and localization
- **Computer Vision**: Object detection with stereo cameras  
- **Remote Control**: Handheld wireless controller
- **Custom Linux**: Optimized embedded system for PYNQ-Z2

## Quick Start

### Build Robot Linux Image
```bash
cd yocto-build
# See BUILD_INSTRUCTIONS.md for complete steps
./flash-pynq-z2-macos.sh /dev/diskX
```

### Controller Setup
```bash
cd android-app
./gradlew assembleDebug
# Install APK on Retroid Pocket 2S
```

### Robot Operation
1. Flash SD card and boot PYNQ-Z2
2. Connect LiDAR and cameras
3. Start robot gateway software
4. Use handheld controller for manual operation
5. Run SLAM for autonomous mapping