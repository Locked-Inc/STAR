#!/bin/bash

# PYNQ-Z2 Robot Image Builder for macOS
# Handles case-sensitive filesystem requirements automatically

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_ROOT="$SCRIPT_DIR"
DISK_IMAGE="$HOME/yocto-build.dmg"
MOUNT_POINT="/Volumes/YoctoBuild"

echo "=========================================="
echo "PYNQ-Z2 Robot Image Builder"
echo "=========================================="
echo "Project: $PROJECT_ROOT"
echo "Disk image: $DISK_IMAGE"
echo "Mount point: $MOUNT_POINT"
echo ""

# Function to create case-sensitive disk image
create_case_sensitive_volume() {
    echo "=== Setting up case-sensitive filesystem ==="
    
    if [ ! -f "$DISK_IMAGE" ]; then
        echo "Creating 50GB case-sensitive disk image..."
        hdiutil create -size 50g -fs "Case-sensitive HFS+" -volname "YoctoBuild" "$DISK_IMAGE"
    else
        echo "Disk image already exists: $DISK_IMAGE"
    fi
    
    # Mount if not already mounted
    if [ ! -d "$MOUNT_POINT" ]; then
        echo "Mounting disk image..."
        hdiutil attach "$DISK_IMAGE"
    else
        echo "Volume already mounted: $MOUNT_POINT"
    fi
    
    # Verify case sensitivity
    echo "Verifying case sensitivity..."
    touch "$MOUNT_POINT/test_Case" "$MOUNT_POINT/test_case" 2>/dev/null || true
    if [ $(ls "$MOUNT_POINT"/test_* 2>/dev/null | wc -l) -eq 2 ]; then
        echo "✅ Case-sensitive filesystem confirmed"
        rm -f "$MOUNT_POINT"/test_*
    else
        echo "❌ Filesystem is not case-sensitive!"
        exit 1
    fi
    
    echo ""
}

# Function to prepare build environment
prepare_build_environment() {
    echo "=== Preparing build environment ==="
    
    # Copy source to case-sensitive filesystem
    echo "Copying source files to case-sensitive volume..."
    if [ ! -d "$MOUNT_POINT/yocto-build" ]; then
        cp -r "$PROJECT_ROOT" "$MOUNT_POINT/"
        echo "✅ Source files copied"
    else
        echo "✅ Source files already present"
    fi
    
    # Update configuration for proper machine
    echo "Updating machine configuration..."
    cat > "$MOUNT_POINT/yocto-build/configs/local.conf" << 'EOF'
#
# PYNQ-Z2 Robot System Local Configuration
# This file contains local configuration for the PYNQ-Z2 LiDAR SLAM and CV robot system
#

# Machine Selection - Use qemuarm for initial testing
MACHINE = "qemuarm"

# Target Distribution - Use latest stable
DISTRO = "poky"

# Package Management
PACKAGE_CLASSES = "package_rpm"

# SDK and Cross-toolchain
SDKMACHINE = "x86_64"

# Build optimization - adjust based on your build machine
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j 4"

# Disable sanity checks for development
SANITY_TESTED_DISTROS = ""
SKIP_SANITY_BBAPPEND_CHECK = "1"

# Disk space monitoring
BB_DISKMON_DIRS = "\
    STOPTASKS,${TMPDIR},1G,100K \
    STOPTASKS,${DL_DIR},1G,100K \
    STOPTASKS,${SSTATE_DIR},1G,100K \
    STOPTASKS,/tmp,100M,100K \
    ABORT,${TMPDIR},100M,1K \
    ABORT,${DL_DIR},100M,1K \
    ABORT,${SSTATE_DIR},100M,1K \
    ABORT,/tmp,10M,1K"

# Additional image features for robot system
EXTRA_IMAGE_FEATURES = "\
    debug-tweaks \
    tools-debug \
    tools-profile \
    ssh-server-openssh \
    package-management \
    splash \
"

# Core image features
IMAGE_FEATURES += "\
    hwcodecs \
    package-management \
    ssh-server-openssh \
    tools-debug \
"

# Enable systemd
INIT_MANAGER = "systemd"

# Root filesystem
IMAGE_ROOTFS_EXTRA_SPACE = "2097152"

# Development settings
INHERIT += "rm_work"
EOF

    # Create basic bblayers.conf
    cat > "$MOUNT_POINT/yocto-build/configs/bblayers.conf" << 'EOF'
# PYNQ-Z2 Robot System Layer Configuration - Working Version

POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "${TOPDIR}"
BBFILES ?= ""

# Core Yocto layers
BBLAYERS ?= " \
  ${TOPDIR}/../../poky/meta \
  ${TOPDIR}/../../poky/meta-poky \
  ${TOPDIR}/../../poky/meta-yocto-bsp \
"

# OpenEmbedded layers for additional functionality
BBLAYERS += " \
  ${TOPDIR}/../layers/downloaded/meta-openembedded/meta-oe \
  ${TOPDIR}/../layers/downloaded/meta-openembedded/meta-python \
"

# Custom robot SLAM layer
BBLAYERS += " \
  ${TOPDIR}/../layers/meta-robot-slam \
"
EOF

    echo "✅ Build environment prepared"
    echo ""
}

# Function to run the build
run_build() {
    echo "=== Starting BitBake build in Docker ==="
    
    # Clean up any existing containers
    docker rm -f pynq-robot-build 2>/dev/null || true
    
    # Start the build
    echo "Starting Docker container for BitBake build..."
    docker run -d --name pynq-robot-build \
        -v "$MOUNT_POINT:/workspace" \
        -v "/tmp:/tmp" \
        --workdir /workspace \
        pynq-yocto-build bash -c "
            echo '=== PYNQ-Z2 Robot Build Started at \$(date) ==='
            
            # Setup Yocto environment
            source poky/oe-init-build-env yocto-build/build
            
            # Copy configuration
            cp yocto-build/configs/local.conf conf/local.conf
            cp yocto-build/configs/bblayers.conf conf/bblayers.conf
            
            echo 'Configuration:'
            grep '^MACHINE' conf/local.conf
            echo ''
            echo 'Available layers:'
            bitbake-layers show-layers
            echo ''
            
            echo '=== Building core-image-minimal ==='
            bitbake core-image-minimal
            
            echo '=== Build completed at \$(date) ==='
            echo 'Generated artifacts:'
            find tmp/deploy/images -name '*.wic' -o -name '*.img' 2>/dev/null || echo 'No image files found'
            
            # Keep container running
            sleep 7200
        "
    
    echo "✅ Build started in background"
    echo "Container name: pynq-robot-build"
    echo ""
}

# Function to monitor build
monitor_build() {
    echo "=== Build Monitoring ==="
    echo "Use these commands to monitor progress:"
    echo ""
    echo "# Check build logs:"
    echo "docker logs -f pynq-robot-build"
    echo ""
    echo "# Check build progress:"
    echo "docker exec pynq-robot-build bash -c 'cd /workspace/yocto-build/build && find tmp/work -name \"*.log\" | tail -5'"
    echo ""
    echo "# Check generated images:"
    echo "docker exec pynq-robot-build bash -c 'ls -la /workspace/yocto-build/build/tmp/deploy/images/'"
    echo ""
    echo "# Stop build:"
    echo "docker kill pynq-robot-build && docker rm pynq-robot-build"
    echo ""
}

# Function to extract results
extract_results() {
    echo "=== Extracting Build Results ==="
    
    if [ ! -d "$PROJECT_ROOT/deploy/images" ]; then
        mkdir -p "$PROJECT_ROOT/deploy/images"
    fi
    
    # Copy images from case-sensitive volume
    if [ -d "$MOUNT_POINT/yocto-build/build/tmp/deploy/images" ]; then
        cp -r "$MOUNT_POINT/yocto-build/build/tmp/deploy/images"/* "$PROJECT_ROOT/deploy/images/" 2>/dev/null || true
        echo "✅ Images copied to $PROJECT_ROOT/deploy/images/"
        
        echo "Generated files:"
        ls -la "$PROJECT_ROOT/deploy/images"/*/ 2>/dev/null | grep -E '\.(wic|img|tar\.gz)$' | head -10
    else
        echo "⚠️  No images found yet"
    fi
}

# Function to clean up
cleanup() {
    echo "=== Cleanup ==="
    
    # Stop Docker container
    docker kill pynq-robot-build 2>/dev/null || true
    docker rm pynq-robot-build 2>/dev/null || true
    
    # Optionally unmount disk image
    read -p "Unmount case-sensitive volume? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        hdiutil unmount "$MOUNT_POINT" 2>/dev/null || true
        echo "✅ Volume unmounted"
    fi
}

# Main execution
main() {
    case "${1:-all}" in
        "setup")
            create_case_sensitive_volume
            prepare_build_environment
            ;;
        "build")
            run_build
            monitor_build
            ;;
        "monitor")
            monitor_build
            ;;
        "extract")
            extract_results
            ;;
        "cleanup")
            cleanup
            ;;
        "all")
            create_case_sensitive_volume
            prepare_build_environment
            run_build
            monitor_build
            ;;
        *)
            echo "Usage: $0 {setup|build|monitor|extract|cleanup|all}"
            echo ""
            echo "Commands:"
            echo "  setup   - Create case-sensitive volume and prepare environment"
            echo "  build   - Start the BitBake build process"
            echo "  monitor - Show build monitoring commands"
            echo "  extract - Copy build results to project directory"
            echo "  cleanup - Stop build and cleanup resources"
            echo "  all     - Run setup and start build (default)"
            exit 1
            ;;
    esac
}

# Handle script interruption
trap cleanup INT TERM

# Run main function
main "$@"