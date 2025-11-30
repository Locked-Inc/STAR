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

- **`ESP32-Firmware/`**: ESP32 sensor and actuator framework (STAR Firmware)
- **`star-pi5-os/`**: Custom Buildroot-based OS for Raspberry Pi 5
- **`Schematic/`**: Hardware design files (KiCad) and PCB layouts
- **`docs/`**: Documentation and setup guides
- **`test-scripts/`**: Hardware testing scripts for Raspberry Pi 5

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
cd star-pi5-os
make build
# Flash custom OS image to SD card
# See star-pi5-os/README.md for complete build and setup instructions
```

### ESP32 Firmware Development
```bash
cd ESP32-Firmware
pio run -e esp32_wroom  # or esp32s3
pio run -e esp32_wroom --target upload
# See ESP32-Firmware/README.md for complete development guide
```

### Robot Operation
1. Boot Raspberry Pi 5 with STAR custom OS
2. Connect TiM561 LiDAR via Ethernet
3. Connect dual CSI cameras for stereo vision
4. Deploy robot gateway Java application
5. Use ROS2 for SLAM and computer vision
6. Access robot via SSH at star-robot.local