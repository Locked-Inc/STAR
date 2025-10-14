#!/bin/bash
# Computer Vision Package Installation Script
# Installs packages for stereo camera processing and computer vision

set -e

echo "Installing Computer Vision dependencies..."

# Update package lists
apt-get update

# Install OpenCV and dependencies
apt-get install -y \
    python3-opencv \
    libopencv-dev \
    v4l-utils

# Install PYNQ ComputerVision (FPGA-accelerated)
pip3 install --no-cache-dir \
    git+https://github.com/Xilinx/PYNQ-ComputerVision.git || \
    echo "PYNQ-ComputerVision installation skipped (requires hardware)"

# Install additional vision libraries
pip3 install --no-cache-dir \
    pillow \
    scikit-image

echo "Computer Vision packages installed successfully"
