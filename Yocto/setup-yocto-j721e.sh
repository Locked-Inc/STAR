#!/bin/bash

# TI J721E (TDA4VM) Yocto Build Environment Setup Script
# This script sets up everything needed to build Yocto images for the J721EXSKG01EVM

set -e

echo "=== TI J721E Yocto Build Environment Setup ==="

# Update system
echo "Updating system packages..."
sudo apt-get update

# Install required dependencies
echo "Installing build dependencies..."
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    git build-essential diffstat texinfo gawk chrpath socat doxygen \
    dos2unix python3 bison flex libssl-dev u-boot-tools mono-devel \
    python3-distutils python3-pexpect xz-utils debianutils iputils-ping \
    python3-git python3-jinja2 python3-pip python3-setuptools \
    python3-subunit mesa-common-dev zstd liblz4-tool file locales \
    cpio

# Configure bash as default shell
echo "Configuring bash as default shell..."
echo "dash dash/sh boolean false" | sudo debconf-set-selections
sudo DEBIAN_FRONTEND=noninteractive dpkg-reconfigure dash

# Set up locale
echo "Setting up locale..."
sudo locale-gen en_US.UTF-8
export LANG=en_US.UTF-8

# Clone TI's layer setup tool
echo "Cloning TI oe-layersetup repository..."
cd /home/yoctouser
git clone https://git.ti.com/git/arago-project/oe-layersetup.git yocto-build
cd yocto-build

# Find the latest processor-sdk config
echo "Finding latest processor-sdk configuration..."
LATEST_CONFIG=$(ls configs/processor-sdk-analytics/ | grep "processor-sdk-analytics.*config.txt" | sort -V | tail -1)
echo "Using configuration: $LATEST_CONFIG"

# Setup layers
echo "Setting up Yocto layers..."
./oe-layertool-setup.sh -f "configs/processor-sdk-analytics/$LATEST_CONFIG"

# Enter build environment
cd build
source conf/setenv

# Configure for ADAS/EdgeAI build
echo "Configuring build for J721E ADAS..."
echo 'ARAGO_BRAND = "adas"' >> conf/local.conf
echo 'MACHINE = "j721e-evm"' >> conf/local.conf

# Optimize build settings
echo "Optimizing build settings..."
echo 'BB_NUMBER_THREADS = "4"' >> conf/local.conf
echo 'PARALLEL_MAKE = "-j 4"' >> conf/local.conf

# Add custom layer support (for future use)
echo 'ARAGO_SYSROOT_NATIVE = "/usr"' >> conf/local.conf

echo ""
echo "=== Setup Complete! ==="
echo ""
echo "To build an image, run:"
echo "cd /home/yoctouser/yocto-build/build"
echo "source conf/setenv"
echo "MACHINE=j721e-evm bitbake tisdk-adas-image"
echo ""
echo "Available image targets:"
echo "- tisdk-adas-image: Full ADAS filesystem"
echo "- tisdk-edgeai-image: EdgeAI optimized filesystem"
echo "- tisdk-default-image: Default filesystem"
echo "- tisdk-base-image: Minimal filesystem"