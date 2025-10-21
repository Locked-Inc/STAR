# STAR Robot

STAR (Simultaneous Tracking And Robotics) is a LiDAR SLAM robot with remote control capabilities.

## Hardware

- **Main Board**: Raspberry Pi 5
- **LiDAR**: For mapping and navigation
- **Cameras**: Stereo vision for depth perception
- **Controller**: Handheld remote (Retroid Pocket 2S)

## Software Architecture

```
Handheld Controller (Retroid Pocket 2S)
    ↓ [WiFi]
Robot Gateway (Java/Spring Boot)
    ↓ [Commands & Telemetry]
Raspberry Pi 5 Linux System
    ↓ [SLAM & Computer Vision]
LiDAR + Stereo Cameras
```

## Modules

- **`android-app/`**: Controller app for Retroid Pocket 2S
- **`robot-gateway/`**: Java gateway running on robot
- **`server-backend/`**: Data collection server
- **`esp32-firmware/`**: ESP32 WiFi bridge firmware (optional)
- **`Schematic/`**: Hardware design files (KiCad)
- **`docs/`**: Documentation and guides

## Features

- **LiDAR SLAM**: Real-time mapping and localization
- **Computer Vision**: Object detection with stereo cameras
- **Remote Control**: Handheld wireless controller
- **Raspberry Pi OS**: Standard Raspberry Pi OS on Raspberry Pi 5

## Development Environment

This project was developed and tested on:
- **OS**: Ubuntu 20.04.6 LTS (Focal Fossa)
- **Kernel**: 5.15.0-139-generic
- **CPU**: 12th Gen Intel(R) Core(TM) i7-1280P (20 cores)
- **RAM**: 32GB
- **Machine**: ThinkPad X13 Gen 3

## Quick Start

### Setup Raspberry Pi 5
```bash
# Flash Raspberry Pi OS to SD card using Raspberry Pi Imager
# See raspberry-pi-5-setup-guide.md for complete steps
# Boot Raspberry Pi 5 and install required software
```

### Controller Setup
```bash
cd android-app
./gradlew assembleDebug
# Install APK on Retroid Pocket 2S
```

### Robot Operation
1. Boot Raspberry Pi 5 with configured SD card
2. Connect LiDAR and cameras
3. Start robot gateway software
4. Use handheld controller for manual operation
5. Run SLAM for autonomous mapping