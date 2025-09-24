# PYNQ-Z2 Yocto/PetaLinux Build Environment

This directory contains the complete Yocto/PetaLinux build environment for the PYNQ-Z2 based LiDAR SLAM and Computer Vision robot system.

## System Requirements

### Host System
- Ubuntu 22.04 LTS (recommended) or Ubuntu 20.04 LTS
- Minimum 50GB free disk space
- 16GB+ RAM (32GB recommended for parallel builds)
- Docker installed and configured

### Xilinx Tools (Required)
- Vivado/Vitis 2024.1 or 2023.2
- PetaLinux 2024.1 or 2023.2
- Vitis AI 3.0+

## Directory Structure

```
yocto-build/
├── layers/                 # Meta-layers for Yocto
│   ├── meta-robot-slam/    # Custom robot SLAM layer
│   ├── meta-vitis-ai/      # Vitis AI integration layer
│   └── downloaded/         # Downloaded meta-layers
├── build/                  # Build workspace
├── downloads/              # Downloaded sources cache
├── sstate-cache/          # Shared state cache
├── deploy/                # Final images and packages
├── scripts/               # Build automation scripts
├── configs/               # Configuration files
└── docker/                # Docker build environment
```

## Key Features

### Hardware Acceleration
- **OpenCV**: PYNQ hardware acceleration overlays for computer vision
- **Vitis AI DPU**: Hardware-accelerated ML inference
- **FPGA Integration**: Custom IP cores for sensor processing

### Software Stack
- **ROS2 Humble**: Complete navigation and SLAM stack
- **OpenCV 4.x**: Computer vision with hardware acceleration
- **Python 3.x**: PYNQ development environment with Jupyter
- **TensorFlow Lite/ONNX**: ML model inference

### Robot Capabilities
- **LiDAR SLAM**: Real-time mapping and localization
- **Stereo Vision**: Depth perception and obstacle detection
- **Sensor Fusion**: IMU, cameras, and LiDAR integration
- **ROS2 Integration**: Standard robotics middleware

## Quick Start

1. **Environment Setup**
   ```bash
   cd yocto-build
   ./scripts/setup-environment.sh
   ```

2. **Build Image**
   ```bash
   ./scripts/build-image.sh pynq-z2-robot
   ```

3. **Flash SD Card**
   ```bash
   ./scripts/flash-sdcard.sh deploy/pynq-z2-robot-image.wic /dev/sdX
   ```

## Integration with Existing Robot Gateway

The system includes bridge components to integrate with the existing Java-based robot gateway:
- REST API bridge between ROS2 nodes and Spring Boot gateway
- Shared memory interfaces for high-frequency sensor data
- Configuration management for seamless operation

## Development Workflow

1. **Hardware Design**: Create/modify Vivado projects in `hardware/`
2. **Custom Recipes**: Add software components in `layers/meta-robot-slam/`
3. **Build & Test**: Use automated build scripts for consistent results
4. **Deploy**: Flash images and test on hardware

## Documentation

- `docs/setup-guide.md` - Detailed setup instructions
- `docs/development.md` - Development workflow and customization
- `docs/hardware-integration.md` - Camera and LiDAR integration
- `docs/performance-tuning.md` - Optimization guidelines

## Support

For build issues or questions:
1. Check the troubleshooting section in documentation
2. Review build logs in `build/tmp/log/`
3. Verify Xilinx tool installation and licensing