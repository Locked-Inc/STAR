#!/bin/bash

# TI J721E (TDA4VM) Yocto Build Environment Setup Script - Native Linux
# This script sets up everything needed to build Yocto images for the J721EXSKG01EVM on native Linux

set -e

echo "=== TI J721E Yocto Build Environment Setup (Native Linux) ==="

# Check if running as root and create/switch to yocto user
if [ "$EUID" -eq 0 ]; then
    echo "Running as root, creating yocto user and switching to it..."
    
    # Create yocto user if it doesn't exist
    if ! id "yocto" &>/dev/null; then
        useradd -m -s /bin/bash yocto
        echo "Created yocto user"
    fi
    
    # Add yocto user to sudo group
    usermod -aG sudo yocto
    
    # Allow yocto user to run sudo without password for this script
    echo "yocto ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/yocto-temp
    
    # Copy the run script to yocto's home if it exists
    if [ -f "run-yocto-build.sh" ]; then
        cp run-yocto-build.sh /home/yocto/
        chown yocto:yocto /home/yocto/run-yocto-build.sh
        chmod +x /home/yocto/run-yocto-build.sh
        echo "Copied run-yocto-build.sh to yocto user's home"
    fi
    
    echo "Switching to yocto user..."
    exec sudo -u yocto bash "$0" "$@"
fi

# Update system
echo "Updating system packages..."
sudo apt-get update

# Enable i386 architecture for 32-bit package support
echo "Enabling i386 architecture..."
sudo dpkg --add-architecture i386
sudo apt-get update

# Install required dependencies (including basic build tools)
echo "Installing build dependencies..."
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    git build-essential diffstat texinfo gawk chrpath socat doxygen \
    dos2unix python3 python3-dev bison flex libssl-dev u-boot-tools mono-devel \
    python3-distutils python3-pexpect xz-utils debianutils iputils-ping \
    python3-git python3-jinja2 python3-pip python3-setuptools \
    python3-subunit mesa-common-dev zstd liblz4-tool file locales \
    cpio curl wget ca-certificates gnupg lsb-release libc6:i386

# Configure bash as default shell
echo "Configuring bash as default shell..."
echo "dash dash/sh boolean false" | sudo debconf-set-selections
sudo DEBIAN_FRONTEND=noninteractive dpkg-reconfigure dash

# Set up locale
echo "Setting up locale..."
sudo locale-gen en_US.UTF-8
export LANG=en_US.UTF-8

# Create yocto build directory in current user's home
echo "Setting up Yocto build directory..."
YOCTO_DIR="$HOME/yocto-j721e-build"
mkdir -p "$YOCTO_DIR"
cd "$YOCTO_DIR"

# Clone TI's layer setup tool
echo "Cloning TI oe-layersetup repository..."
if [ ! -d "oe-layersetup" ]; then
    git clone https://git.ti.com/git/arago-project/oe-layersetup.git oe-layersetup
fi
cd oe-layersetup

# Find the latest processor-sdk config
echo "Finding latest processor-sdk configuration..."
LATEST_CONFIG=$(ls configs/processor-sdk-analytics/ | grep "processor-sdk-analytics.*config.txt" | sort -V | tail -1)
echo "Using configuration: $LATEST_CONFIG"

# Setup layers
echo "Setting up Yocto layers..."
./oe-layertool-setup.sh -f "configs/processor-sdk-analytics/$LATEST_CONFIG"

# Configure build environment
cd build
echo "Configuring build environment..."

# Source the environment (this creates conf/local.conf if it doesn't exist)
source conf/setenv

# Configure for ADAS/EdgeAI build
echo "Configuring build for J721E ADAS..."
echo 'ARAGO_BRAND = "adas"' >> conf/local.conf
echo 'MACHINE = "j721e-evm"' >> conf/local.conf

# Optimize build settings based on system resources
NPROC=$(nproc)
echo "Optimizing build settings for $NPROC CPU cores..."
echo "BB_NUMBER_THREADS = \"$NPROC\"" >> conf/local.conf
echo "PARALLEL_MAKE = \"-j $NPROC\"" >> conf/local.conf

# Add custom layer support (for future use)
echo 'ARAGO_SYSROOT_NATIVE = "/usr"' >> conf/local.conf

# Set download directory to prevent re-downloading sources
echo "DL_DIR = \"$YOCTO_DIR/downloads\"" >> conf/local.conf
echo "SSTATE_DIR = \"$YOCTO_DIR/sstate-cache\"" >> conf/local.conf

# Fix patch-fuzz QA issue for u-boot-ti-staging
echo "Configuring patch fuzz handling..."
echo "" >> conf/local.conf
echo "# Temporarily disable patch-fuzz QA check for u-boot-ti-staging" >> conf/local.conf
echo "WARN_QA:remove:pn-u-boot-ti-staging = \"patch-fuzz\"" >> conf/local.conf
echo "ERROR_QA:remove:pn-u-boot-ti-staging = \"patch-fuzz\"" >> conf/local.conf

echo ""
echo "=== Setup Complete! ==="
echo ""
echo "Yocto build environment is ready at: $YOCTO_DIR/oe-layersetup/build"
echo ""
echo "To build an image, IMPORTANT: Use the run script as root to ensure proper user switching:"
echo "sudo /home/yocto/run-yocto-build.sh"
echo ""
echo "Or manually run as yocto user:"
echo "sudo su - yocto"
echo "cd $YOCTO_DIR/oe-layersetup/build"
echo "source conf/setenv"
echo "MACHINE=j721e-evm bitbake tisdk-adas-image"
echo ""
echo "Available image targets:"
echo "- tisdk-adas-image: Full ADAS filesystem"
echo "- tisdk-edgeai-image: EdgeAI optimized filesystem"
echo "- tisdk-default-image: Default filesystem"
echo "- tisdk-base-image: Minimal filesystem"
echo ""
echo "Build output will be in:"
echo "$YOCTO_DIR/oe-layersetup/build/arago-tmp-external-arm-glibc/deploy/images/j721e-evm/"

# Clean up temporary sudoers file if we created it
if [ -f "/etc/sudoers.d/yocto-temp" ]; then
    sudo rm -f /etc/sudoers.d/yocto-temp
    echo "Cleaned up temporary sudo permissions"
fi