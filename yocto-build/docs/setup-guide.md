# PYNQ-Z2 Robot System Setup Guide

Complete guide for setting up the PYNQ-Z2 LiDAR SLAM and Computer Vision robot system development environment.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Host System Setup](#host-system-setup)
3. [Xilinx Tools Installation](#xilinx-tools-installation)
4. [Build Environment Setup](#build-environment-setup)
5. [Building the Image](#building-the-image)
6. [Flashing and First Boot](#flashing-and-first-boot)
7. [Troubleshooting](#troubleshooting)

## Prerequisites

### Hardware Requirements

- **Development Machine:**
  - Ubuntu 22.04 LTS (recommended) or Ubuntu 20.04 LTS
  - Minimum 16GB RAM (32GB recommended for parallel builds)
  - Minimum 100GB free disk space (SSD recommended)
  - Multi-core CPU (8+ cores recommended)

- **Target Hardware:**
  - PYNQ-Z2 development board
  - 8GB+ microSD card (Class 10 or better)
  - LiDAR sensor (RPLiDAR A2/A3 or compatible)
  - Stereo camera setup or dual USB cameras
  - IMU sensor (MPU6050 or compatible)
  - Robot chassis with motor drivers

### Software Requirements

- Docker installed and configured
- Git version control
- Basic knowledge of Linux, Yocto, and ROS2

## Host System Setup

### 1. Install Docker

```bash
# Update package index
sudo apt update

# Install Docker dependencies
sudo apt install apt-transport-https ca-certificates curl gnupg lsb-release

# Add Docker's official GPG key
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

# Add Docker repository
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker
sudo apt update
sudo apt install docker-ce docker-ce-cli containerd.io docker-compose-plugin

# Add user to docker group
sudo usermod -aG docker $USER
```

Log out and back in for group changes to take effect.

### 2. Verify Docker Installation

```bash
# Test Docker installation
docker --version
docker run hello-world

# Test Docker Compose
docker compose version
```

### 3. Install Additional Tools

```bash
# Install development tools
sudo apt install git vim tree curl wget htop

# Install SD card tools
sudo apt install gparted

# Install cross-compilation tools (optional)
sudo apt install gcc-arm-linux-gnueabihf
```

## Xilinx Tools Installation

### 1. Download Xilinx Tools

Download the following from AMD/Xilinx website:
- **Vivado/Vitis 2024.1** or 2023.2 (WebPACK edition sufficient)
- **PetaLinux 2024.1** or 2023.2

### 2. Install Vivado/Vitis

```bash
# Make installer executable
chmod +x Xilinx_Unified_2024.1_0522_2023_Lin64.bin

# Run installer
./Xilinx_Unified_2024.1_0522_2023_Lin64.bin

# Follow GUI installer:
# - Install to /opt/Xilinx (recommended)
# - Select Vitis (includes Vivado)
# - Include Zynq-7000 device support
# - Install cable drivers when prompted
```

### 3. Install PetaLinux

```bash
# Create installation directory
sudo mkdir -p /opt/Xilinx/petalinux-2024.1
sudo chown $USER:$USER /opt/Xilinx/petalinux-2024.1

# Make installer executable
chmod +x petalinux-v2024.1-final-installer.run

# Install PetaLinux
./petalinux-v2024.1-final-installer.run -d /opt/Xilinx/petalinux-2024.1
```

### 4. Verify Installation

```bash
# Source tools
source /opt/Xilinx/Vitis/2024.1/settings64.sh
source /opt/Xilinx/petalinux-2024.1/settings.sh

# Verify
vivado -version
petalinux-util --webtalk off
```

## Build Environment Setup

### 1. Clone Project Repository

```bash
# Clone the project
git clone <your-robot-repo-url>
cd robot-project/yocto-build

# Or if you're setting this up in the existing STAR repository
cd /path/to/STAR/yocto-build
```

### 2. Run Environment Setup

```bash
# Make setup script executable (if not already)
chmod +x scripts/setup-environment.sh

# Run complete environment setup
./scripts/setup-environment.sh
```

This script will:
- Check system requirements
- Create directory structure
- Download required meta-layers
- Build Docker environment
- Create convenience scripts

### 3. Manual Meta-Layer Setup (Alternative)

If the automatic setup fails:

```bash
# Run layer setup manually
./configs/setup-layers.sh

# Or download layers individually:
cd layers/downloaded

# Core layers
git clone -b kirkstone https://git.openembedded.org/meta-openembedded
git clone -b rel-v2024.1 https://github.com/Xilinx/meta-xilinx.git
git clone -b kirkstone https://github.com/ros/meta-ros.git

# Base Yocto
cd ../../..
git clone -b kirkstone https://git.yoctoproject.org/git/poky
```

## Building the Image

### 1. Start Build Environment

```bash
# Method 1: Use convenience script
./docker-run.sh

# Method 2: Manual Docker run
docker run -it --rm \
  -v $(pwd):/workspace/yocto-build:rw \
  -v /opt/Xilinx:/opt/Xilinx:ro \
  --network host \
  pynq-z2-builder:latest
```

### 2. Initialize Yocto Build

Inside Docker container:

```bash
# Source build environment
source /workspace/setup-build-env.sh

# Initialize Yocto
cd /workspace/yocto-build
source ../poky/oe-init-build-env build

# Copy configuration files
cp ../configs/local.conf conf/
cp ../configs/bblayers.conf conf/
```

### 3. Customize Configuration

Edit `conf/local.conf` for your needs:

```bash
# For development builds (larger, includes debug tools)
echo 'EXTRA_IMAGE_FEATURES += "debug-tweaks tools-debug"' >> conf/local.conf

# For production builds (smaller, optimized)
echo 'EXTRA_IMAGE_FEATURES:remove = "debug-tweaks"' >> conf/local.conf
echo 'IMAGE_INSTALL:remove = "gdb strace"' >> conf/local.conf

# Adjust build threads (match your CPU cores)
sed -i 's/BB_NUMBER_THREADS = "8"/BB_NUMBER_THREADS = "16"/' conf/local.conf
sed -i 's/PARALLEL_MAKE = "-j 8"/PARALLEL_MAKE = "-j 16"/' conf/local.conf
```

### 4. Build the Image

```bash
# Build robot image (this takes 2-4 hours first time)
bitbake pynq-robot-image

# Or use convenience script
exit  # Exit Docker container
./build-image.sh
```

### 5. Build Progress Monitoring

```bash
# Monitor build progress
tail -f logs/build-*.log

# Check build status
bitbake pynq-robot-image -g -u taskexp

# Clean and rebuild specific packages if needed
bitbake -c cleanall package-name
bitbake package-name
```

## Flashing and First Boot

### 1. Prepare SD Card

```bash
# Find SD card device
lsblk

# Example output: /dev/sdb (adjust accordingly)
```

**⚠️ Warning: Double-check device name! Wrong device will destroy data!**

### 2. Flash Image

```bash
# Use convenience script
./flash-sdcard.sh deploy/latest/pynq-robot-image-*.wic /dev/sdX

# Or manual flash
sudo dd if=deploy/latest/pynq-robot-image-*.wic of=/dev/sdX bs=4M status=progress
sync
```

### 3. First Boot Setup

1. Insert SD card into PYNQ-Z2
2. Connect Ethernet cable
3. Connect USB serial cable (optional, for console)
4. Power on the board

### 4. Initial Configuration

```bash
# Connect via SSH (find IP with router/DHCP logs)
ssh root@<robot-ip>

# Or connect via serial console
minicom -b 115200 -D /dev/ttyUSB0
```

First boot configuration:

```bash
# Set up networking (if needed)
systemctl enable NetworkManager
systemctl start NetworkManager

# Check services
systemctl status robot-gateway-bridge
systemctl status ros2-bridge-node

# Test robot bridge
curl http://localhost:9090/health

# Access Jupyter notebooks
# Open browser: http://<robot-ip>:8888
```

### 5. Hardware Integration

Connect your robot hardware:

1. **LiDAR**: Connect to UART or USB as configured in device tree
2. **Cameras**: Connect USB stereo cameras or CSI cameras
3. **IMU**: Connect via I2C (default address 0x68)
4. **Motors**: Connect to PWM and GPIO pins as defined in device tree

Test hardware:

```bash
# Test camera
v4l2-ctl --list-devices
gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink

# Test I2C devices
i2cdetect -y 0

# Test GPIO
echo 60 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio60/direction
echo 1 > /sys/class/gpio/gpio60/value
```

## Troubleshooting

### Build Issues

**Problem**: Out of disk space during build
```bash
# Solution: Clean up temporary files
bitbake -c cleanall pynq-robot-image
rm -rf build/tmp/work/*
```

**Problem**: Layer configuration errors
```bash
# Solution: Check layer paths in bblayers.conf
bitbake-layers show-layers
bitbake-layers add-layer ../layers/downloaded/meta-xyz
```

**Problem**: Missing dependencies
```bash
# Solution: Check package dependencies
bitbake -e pynq-robot-image | grep ^DEPENDS
bitbake package-name -c listtasks
```

### Runtime Issues

**Problem**: Services not starting
```bash
# Check service status
systemctl status robot-gateway-bridge
journalctl -u robot-gateway-bridge -f

# Restart services
systemctl restart robot-gateway-bridge
```

**Problem**: Hardware not detected
```bash
# Check device tree
ls /sys/firmware/devicetree/base/
cat /proc/device-tree/model

# Check kernel modules
lsmod | grep -i sensor
dmesg | grep -i error
```

**Problem**: Network connectivity issues
```bash
# Check network configuration
ip addr show
ping google.com

# Reset network
systemctl restart NetworkManager
```

### Development Tips

1. **Incremental Builds**: Use `bitbake -c compile package-name` for faster rebuilds
2. **SDK Development**: Generate SDK with `bitbake pynq-robot-image -c populate_sdk`
3. **Remote Debugging**: Enable SSH and use remote development tools
4. **Log Analysis**: Use `journalctl` and `/var/log/` for debugging
5. **Performance Tuning**: Monitor with `htop`, `iotop`, and custom performance scripts

### Getting Help

1. **Documentation**: Check `docs/` directory for detailed guides
2. **Logs**: Review build logs in `logs/` directory
3. **Community**: Yocto Project and ROS2 communities
4. **Xilinx Support**: AMD/Xilinx developer forums and documentation

### Next Steps

After successful setup:
1. **Hardware Integration**: Connect and test all robot sensors
2. **ROS2 Configuration**: Set up robot-specific ROS2 packages
3. **SLAM Tuning**: Configure and tune SLAM algorithms
4. **Vision Pipeline**: Set up stereo vision processing
5. **Robot Control**: Implement and test robot control algorithms