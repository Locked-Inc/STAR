# STAR Robot - PYNQ-Z2 Build System

STAR (Simultaneous Tracking And Robotics) is a LiDAR SLAM robot built on the PYNQ-Z2 development board.

## Hardware

- **Main Board**: PYNQ-Z2 (Xilinx Zynq-7020 SoC)
- **LiDAR**: For mapping and navigation
- **Controller**: Handheld wireless controller for manual operation
- **Cameras**: Stereo vision for depth perception

## Features

- **SLAM**: Real-time mapping and localization using LiDAR
- **Computer Vision**: Object detection and stereo depth
- **Remote Control**: Wireless handheld controller
- **Linux**: Custom embedded Linux image optimized for robotics

## Quick Build

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for complete build steps.

## Flash to SD Card (macOS)

```bash
diskutil list
./flash-pynq-z2-macos.sh /dev/diskX
```

## Robot Operation

1. Insert flashed SD card into PYNQ-Z2
2. Connect power and Ethernet
3. SSH to robot: `ssh root@<robot-ip>`
4. Use handheld controller for manual control
5. Run SLAM algorithms for autonomous mapping