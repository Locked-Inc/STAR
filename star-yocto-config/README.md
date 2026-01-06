# STAR Yocto Configuration

Yocto/OpenEmbedded configuration for building Raspberry Pi 5 images with ROS2 Jazzy.

## Quick Start

```bash
./setup-yocto.sh /path/to/yocto/workspace
cd /path/to/yocto/workspace
source poky/oe-init-build-env build-rpi5
bitbake core-image-minimal
```

## Image Contents

- **ROS2 Jazzy** - Full ROS2 installation with auto-source on login
- **Python 3.12** with NumPy, pip
- **OpenCV 4.9**
- **v4l-utils** for camera support
- **systemd** init system

## ROS2 Packages Included

- ros-core (base runtime)
- rclcpp, rclpy (C++ and Python client libraries)
- std-msgs, sensor-msgs, geometry-msgs, nav-msgs

## Layers Used

- poky (scarthgap) - Yocto base
- meta-raspberrypi (scarthgap) - RPi5 support
- meta-openembedded (scarthgap) - Additional packages
- meta-ros (scarthgap) - ROS2 Jazzy

## Files

- `conf/local.conf` - Build configuration
- `conf/bblayers.conf` - Layer configuration  
- `overlays/` - Files to install on target
- `setup-yocto.sh` - Environment setup script
