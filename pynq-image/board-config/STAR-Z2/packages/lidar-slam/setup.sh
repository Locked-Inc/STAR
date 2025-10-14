#!/bin/bash
# LiDAR SLAM Package Installation Script
# Installs necessary packages for LiDAR-based SLAM

set -e

echo "Installing LiDAR SLAM dependencies..."

# Update package lists
apt-get update

# Install ROS dependencies (if using ROS-based SLAM)
# apt-get install -y ros-noetic-slam-toolbox ros-noetic-hector-slam

# Install Python SLAM libraries
pip3 install --no-cache-dir \
    transforms3d \
    scikit-learn \
    matplotlib

# Install LiDAR driver dependencies
apt-get install -y \
    python3-serial \
    python3-numpy

echo "LiDAR SLAM packages installed successfully"
