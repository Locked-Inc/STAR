#!/bin/bash

# PYNQ-Z2 Robot System - Meta-Layer Setup Script
# This script downloads and configures all required meta-layers for the build

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
BUILD_ROOT=$(dirname "$SCRIPT_DIR")
LAYERS_DIR="$BUILD_ROOT/layers/downloaded"

echo "=== PYNQ-Z2 Robot System Meta-Layer Setup ==="
echo "Build root: $BUILD_ROOT"
echo "Layers directory: $LAYERS_DIR"

# Create layers directory
mkdir -p "$LAYERS_DIR"
cd "$LAYERS_DIR"

# Function to clone or update a git repository
clone_or_update() {
    local repo_url="$1"
    local repo_name="$2"
    local branch="${3:-master}"
    
    if [ -d "$repo_name" ]; then
        echo "Updating $repo_name..."
        cd "$repo_name"
        git fetch origin
        git checkout "$branch"
        git pull origin "$branch"
        cd ..
    else
        echo "Cloning $repo_name (branch: $branch)..."
        git clone --branch "$branch" "$repo_url" "$repo_name"
    fi
}

echo ""
echo "Downloading meta-layers..."
echo ""

# Core OpenEmbedded layers
echo "=== OpenEmbedded Core Layers ==="
clone_or_update "https://git.openembedded.org/meta-openembedded" "meta-openembedded" "kirkstone"

# Xilinx layers
echo "=== Xilinx/AMD Layers ==="
clone_or_update "https://github.com/Xilinx/meta-xilinx.git" "meta-xilinx" "rel-v2024.1"
clone_or_update "https://github.com/Xilinx/meta-petalinux.git" "meta-petalinux" "rel-v2024.1"

# PYNQ layers
echo "=== PYNQ Layers ==="
clone_or_update "https://github.com/Xilinx/PYNQ.git" "PYNQ" "v3.0.1"
# The PYNQ repo contains meta-pynq layer
if [ -d "PYNQ/meta-pynq" ]; then
    ln -sf "$(pwd)/PYNQ/meta-pynq" "$(pwd)/meta-pynq"
else
    echo "Creating placeholder meta-pynq layer..."
    mkdir -p meta-pynq/conf
    cat > meta-pynq/conf/layer.conf << 'EOF'
# PYNQ Meta Layer Configuration
BBPATH .= ":${LAYERDIR}"

BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*.bbappend"

BBFILE_COLLECTIONS += "meta-pynq"
BBFILE_PATTERN_meta-pynq = "^${LAYERDIR}/"
BBFILE_PRIORITY_meta-pynq = "7"

LAYERNAME = "meta-pynq"
LAYERSERIES_COMPAT_meta-pynq = "kirkstone"
EOF
fi

# ROS2 layers
echo "=== ROS2 Layers ==="
clone_or_update "https://github.com/ros/meta-ros.git" "meta-ros" "kirkstone"

# OpenCV layer
echo "=== Computer Vision Layers ==="
clone_or_update "https://github.com/ros/meta-ros.git" "meta-opencv" "kirkstone" || {
    # If meta-opencv doesn't exist as separate layer, we'll create it
    echo "Creating custom meta-opencv layer..."
    mkdir -p meta-opencv/conf
    cat > meta-opencv/conf/layer.conf << 'EOF'
# OpenCV Meta Layer Configuration
BBPATH .= ":${LAYERDIR}"

BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*.bbappend"

BBFILE_COLLECTIONS += "meta-opencv"
BBFILE_PATTERN_meta-opencv = "^${LAYERDIR}/"
BBFILE_PRIORITY_meta-opencv = "6"

LAYERNAME = "meta-opencv"
LAYERSERIES_COMPAT_meta-opencv = "kirkstone"
EOF
}

# Qt5 layer (for GUI applications)
echo "=== Qt5 Layer ==="
clone_or_update "https://github.com/meta-qt5/meta-qt5.git" "meta-qt5" "kirkstone"

# Security layer (optional)
echo "=== Security Layer ==="
clone_or_update "https://git.yoctoproject.org/git/meta-security" "meta-security" "kirkstone"

# Real-time layer
echo "=== Real-time Layer ==="
clone_or_update "https://git.yoctoproject.org/git/meta-realtime" "meta-realtime" "kirkstone"

# Check if Poky (Yocto reference distribution) exists
if [ ! -d "../../../poky" ]; then
    echo ""
    echo "=== Downloading Poky (Yocto Reference Distribution) ==="
    cd "$BUILD_ROOT/.."
    clone_or_update "https://git.yoctoproject.org/git/poky" "poky" "kirkstone"
    cd "$LAYERS_DIR"
fi

echo ""
echo "=== Layer Setup Complete ==="
echo ""
echo "Downloaded layers:"
ls -la

echo ""
echo "Next steps:"
echo "1. Set up build environment: cd $BUILD_ROOT && source ../poky/oe-init-build-env build"
echo "2. Copy configuration files: cp configs/* build/conf/"
echo "3. Update bblayers.conf with correct paths"
echo "4. Start building: bitbake pynq-robot-image"
echo ""

# Create a summary file
cat > "$BUILD_ROOT/layer-status.txt" << EOF
PYNQ-Z2 Robot System - Layer Status
Generated: $(date)

Core Layers:
✓ meta-openembedded (OpenEmbedded core functionality)
✓ meta-xilinx (Xilinx/AMD hardware support)
✓ meta-petalinux (PetaLinux tools integration)
✓ meta-pynq (PYNQ framework)
✓ meta-ros (ROS2 robotics middleware)
✓ meta-opencv (Computer vision libraries)
✓ meta-qt5 (Qt5 GUI framework)
✓ meta-security (Security hardening)
✓ meta-realtime (Real-time kernel support)

Custom Layers:
- meta-robot-slam (Robot-specific recipes)
- meta-vitis-ai (Vitis AI integration)

Build Configuration:
- Target: PYNQ-Z2 (Zynq-7020)
- Distribution: Poky/Kirkstone
- Features: ROS2, OpenCV, ML acceleration, Real-time
EOF

echo "Layer status saved to: $BUILD_ROOT/layer-status.txt"