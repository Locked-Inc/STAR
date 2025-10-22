#!/bin/bash
#
# Initialize Yocto build environment by cloning required layers
# Supports both git submodules and direct cloning
# Run this script once before first build
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_ROOT="$(dirname "${SCRIPT_DIR}")"

# Yocto version to use (Scarthgap LTS - has Raspberry Pi 5 support)
YOCTO_BRANCH="scarthgap"

# Optional: Pin to specific commits for reproducible builds
# Leave empty to use latest from branch
POKY_COMMIT=""
META_OE_COMMIT=""
META_RPI_COMMIT=""
META_TEMURIN_COMMIT=""

echo "========================================="
echo "STAR Yocto Build Initialization"
echo "========================================="
echo "Branch: ${YOCTO_BRANCH}"
echo "Target: ${YOCTO_ROOT}"
echo ""

cd "${YOCTO_ROOT}"

# Check if we're in a git repository and have submodules configured
if [ -d ".git" ] && [ -f ".gitmodules" ]; then
    echo "📦 Git submodules detected!"
    echo "Initializing and updating submodules..."
    echo ""

    git submodule update --init --recursive

    echo ""
    echo "✓ Submodules initialized"

    # Verify submodules are present
    if [ ! -d "poky/.git" ] || [ ! -d "meta-openembedded/.git" ] || [ ! -d "meta-raspberrypi/.git" ] || [ ! -d "meta-openjdk-temurin/.git" ]; then
        echo "⚠️  Warning: Some submodules failed to initialize"
        echo "Falling back to direct cloning..."
        USE_SUBMODULES=false
    else
        USE_SUBMODULES=true
    fi
else
    echo "📥 No git submodules configured"
    echo "Cloning layers directly..."
    echo ""
    USE_SUBMODULES=false
fi

# Clone or update layers if not using submodules
if [ "$USE_SUBMODULES" = false ]; then
    # Clone Poky (Yocto reference distribution)
    if [ ! -d "poky" ]; then
        echo "Cloning Poky..."
        git clone -b ${YOCTO_BRANCH} https://git.yoctoproject.org/git/poky
        if [ -n "$POKY_COMMIT" ]; then
            cd poky && git checkout $POKY_COMMIT && cd ..
            echo "✓ Poky cloned and pinned to ${POKY_COMMIT}"
        else
            echo "✓ Poky cloned"
        fi
    else
        echo "✓ Poky already exists"
    fi

    # Clone meta-openembedded
    if [ ! -d "meta-openembedded" ]; then
        echo "Cloning meta-openembedded..."
        git clone -b ${YOCTO_BRANCH} https://github.com/openembedded/meta-openembedded.git
        if [ -n "$META_OE_COMMIT" ]; then
            cd meta-openembedded && git checkout $META_OE_COMMIT && cd ..
            echo "✓ meta-openembedded cloned and pinned to ${META_OE_COMMIT}"
        else
            echo "✓ meta-openembedded cloned"
        fi
    else
        echo "✓ meta-openembedded already exists"
    fi

    # Clone meta-raspberrypi
    if [ ! -d "meta-raspberrypi" ]; then
        echo "Cloning meta-raspberrypi..."
        git clone -b ${YOCTO_BRANCH} https://github.com/agherzan/meta-raspberrypi.git
        if [ -n "$META_RPI_COMMIT" ]; then
            cd meta-raspberrypi && git checkout $META_RPI_COMMIT && cd ..
            echo "✓ meta-raspberrypi cloned and pinned to ${META_RPI_COMMIT}"
        else
            echo "✓ meta-raspberrypi cloned"
        fi
    else
        echo "✓ meta-raspberrypi already exists"
    fi

    # Clone meta-openjdk-temurin (for modern OpenJDK support via Eclipse Temurin binaries)
    if [ ! -d "meta-openjdk-temurin" ]; then
        echo "Cloning meta-openjdk-temurin..."
        git clone -b ${YOCTO_BRANCH} https://github.com/lucimber/meta-openjdk-temurin.git
        if [ -n "$META_TEMURIN_COMMIT" ]; then
            cd meta-openjdk-temurin && git checkout $META_TEMURIN_COMMIT && cd ..
            echo "✓ meta-openjdk-temurin cloned and pinned to ${META_TEMURIN_COMMIT}"
        else
            echo "✓ meta-openjdk-temurin cloned"
        fi
    else
        echo "✓ meta-openjdk-temurin already exists"
    fi
fi

# Note: For runit, we'll use recipes directly in meta-star
# instead of a separate layer to keep it simple

echo ""
echo "========================================="
echo "✓ Initialization Complete!"
echo "========================================="
echo ""
echo "Layers ready:"
echo "  - poky (Yocto reference)"
echo "  - meta-openembedded"
echo "  - meta-raspberrypi"
echo "  - meta-openjdk-temurin (Modern Java JRE 8/11/17/21)"

if [ "$USE_SUBMODULES" = true ]; then
    echo ""
    echo "Using git submodules ✓"
    echo "Versions are tracked in .gitmodules"
fi

echo ""
echo "Next steps:"
echo "  1. source setup-environment.sh"
echo "  2. ./scripts/build-image.sh"
echo ""
echo "Note: First build will take 4-8 hours and download ~10GB"
echo "      Subsequent builds are much faster due to caching"
echo ""
echo "Git submodules are configured in .gitmodules"
echo "Versions are pinned for reproducible builds"
echo "========================================="
