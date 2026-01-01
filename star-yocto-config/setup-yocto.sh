#!/bin/bash
# Setup script for STAR Yocto build environment
# This clones required repos and sets up the build directory

set -e

YOCTO_DIR="${1:-/home/brighton/STAR/star-yocto}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Setting up Yocto in: $YOCTO_DIR"

mkdir -p "$YOCTO_DIR"
cd "$YOCTO_DIR"

# Clone Poky (Yocto base) - scarthgap branch
if [ ! -d "poky" ]; then
    git clone -b scarthgap https://git.yoctoproject.org/poky
fi

# Clone meta-raspberrypi
if [ ! -d "meta-raspberrypi" ]; then
    git clone -b scarthgap https://github.com/agherzan/meta-raspberrypi.git
fi

# Clone meta-openembedded
if [ ! -d "meta-openembedded" ]; then
    git clone -b scarthgap https://github.com/openembedded/meta-openembedded.git
fi

# Clone meta-ros
if [ ! -d "meta-ros" ]; then
    git clone -b scarthgap https://github.com/ros/meta-ros.git
fi

# Initialize build environment
source poky/oe-init-build-env build-rpi5

# Copy our config files
cp "$SCRIPT_DIR/conf/local.conf" conf/
cp "$SCRIPT_DIR/conf/bblayers.conf" conf/

echo ""
echo "Yocto environment ready!"
echo "To build: bitbake core-image-minimal"
