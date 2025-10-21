#!/bin/bash
#
# Build STAR minimal image
# Can be run from anywhere - it will find the correct paths
#

set -e

# Find the yocto root directory (where this script's parent is)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${YOCTO_ROOT}/build"

echo "========================================="
echo "Building STAR Minimal Image"
echo "========================================="
echo ""

# Check if environment is set up
if [ ! -d "${YOCTO_ROOT}/poky" ]; then
    echo "ERROR: Yocto layers not found!"
    echo "Please run './scripts/init-build.sh' first"
    exit 1
fi

# Always start from yocto root and source environment
cd "${YOCTO_ROOT}"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Setting up build environment..."
fi

# Source the environment (this changes directory to BUILD_DIR)
source poky/oe-init-build-env "${BUILD_DIR}" > /dev/null

# Now we're in BUILD_DIR, which is where bitbake should run

echo "Build configuration:"
echo "  Machine: raspberrypi5"
echo "  Image: star-minimal-image"
echo "  Build directory: ${BUILD_DIR}"
echo ""
echo "Starting build..."
echo "This will take several hours on first build."
echo "Go grab a coffee (or several)!"
echo ""

# Build the image
bitbake star-minimal-image

echo ""
echo "========================================="
echo "✓ Build Complete!"
echo "========================================="
echo ""
echo "Image location:"
echo "  ${BUILD_DIR}/tmp/deploy/images/raspberrypi5/"
echo ""
echo "Files created:"
ls -lh "${BUILD_DIR}/tmp/deploy/images/raspberrypi5/star-minimal-image"*.wic* 2>/dev/null || echo "  (check build/tmp/deploy/images/raspberrypi5/)"
echo ""
echo "To flash to SD card:"
echo "  sudo ${YOCTO_ROOT}/scripts/flash-sd.sh /dev/sdX"
echo ""
echo "========================================="
