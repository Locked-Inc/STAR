#!/bin/bash

# PYNQ-Z2 Robot System Image Build Script
# Automated build script for the complete robot system image

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
BUILD_DIR="$PROJECT_ROOT/build"
DEPLOY_DIR="$PROJECT_ROOT/deploy"

echo "=========================================="
echo "PYNQ-Z2 Robot System Image Build"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

# Configuration
IMAGE_NAME="${1:-pynq-robot-image}"
MACHINE="${2:-zynq-generic}"
BUILD_TYPE="${3:-release}"  # release or debug

echo "Build configuration:"
echo "  Image: $IMAGE_NAME"
echo "  Machine: $MACHINE"
echo "  Type: $BUILD_TYPE"
echo ""

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."
    
    # Check if we're running in Docker
    if [ -f /.dockerenv ]; then
        echo "✅ Running inside Docker container"
    else
        echo "⚠️  Not running in Docker container"
        echo "   It's recommended to run this inside the Docker build environment"
        echo "   Use: ./docker-run.sh ./scripts/build-robot-image.sh"
    fi
    
    # Check if Yocto environment is available
    if [ ! -d "$PROJECT_ROOT/../poky" ]; then
        echo "❌ Poky (Yocto) not found. Please run setup-layers.sh first"
        exit 1
    fi
    
    # Check if meta-layers are available
    if [ ! -d "$PROJECT_ROOT/layers/downloaded" ]; then
        echo "❌ Meta-layers not found. Please run setup-layers.sh first"
        exit 1
    fi
    
    echo "✅ Prerequisites check passed"
    echo ""
}

# Setup build environment
setup_build_environment() {
    echo "Setting up build environment..."
    
    cd "$PROJECT_ROOT"
    
    # Source Xilinx tools if available
    if [ -d "/opt/Xilinx" ]; then
        echo "Looking for Xilinx tools..."
        
        # Find and source PetaLinux
        PETALINUX_SETTINGS=$(find /opt/Xilinx -name "settings.sh" -path "*/petalinux*" 2>/dev/null | head -1)
        if [ -f "$PETALINUX_SETTINGS" ]; then
            echo "Sourcing PetaLinux: $PETALINUX_SETTINGS"
            source "$PETALINUX_SETTINGS"
        fi
        
        # Find and source Vitis/Vivado
        VITIS_SETTINGS=$(find /opt/Xilinx -name "settings64.sh" -path "*/Vitis*" 2>/dev/null | head -1)
        if [ -f "$VITIS_SETTINGS" ]; then
            echo "Sourcing Vitis: $VITIS_SETTINGS"
            source "$VITIS_SETTINGS"
        fi
    fi
    
    # Initialize Yocto build environment
    echo "Initializing Yocto build environment..."
    source ../poky/oe-init-build-env build
    
    # Copy configuration files
    echo "Copying configuration files..."
    cp ../configs/local.conf conf/local.conf
    cp ../configs/bblayers.conf conf/bblayers.conf
    
    # Customize configuration based on build type
    if [ "$BUILD_TYPE" = "debug" ]; then
        echo 'EXTRA_IMAGE_FEATURES += "debug-tweaks tools-debug"' >> conf/local.conf
        echo 'IMAGE_INSTALL:append = " gdb strace ltrace"' >> conf/local.conf
    fi
    
    # Set machine if different from default
    if [ "$MACHINE" != "zynq-generic" ]; then
        sed -i "s/MACHINE = \"zynq-generic\"/MACHINE = \"$MACHINE\"/" conf/local.conf
    fi
    
    echo "✅ Build environment configured"
    echo ""
}

# Validate layer configuration
validate_layers() {
    echo "Validating layer configuration..."
    
    # Check that all required layers are available
    REQUIRED_LAYERS=(
        "meta"
        "meta-poky"
        "meta-yocto-bsp"
        "meta-openembedded/meta-oe"
        "meta-xilinx/meta-xilinx-core"
        "meta-robot-slam"
    )
    
    for layer in "${REQUIRED_LAYERS[@]}"; do
        if bitbake-layers show-layers | grep -q "$layer"; then
            echo "✅ Layer found: $layer"
        else
            echo "❌ Layer missing: $layer"
            echo "Please check bblayers.conf configuration"
            exit 1
        fi
    done
    
    echo "✅ All required layers found"
    echo ""
}

# Build the image
build_image() {
    echo "Starting image build..."
    echo "This may take several hours depending on your system"
    echo ""
    
    # Create log directory
    mkdir -p "$PROJECT_ROOT/logs"
    LOG_FILE="$PROJECT_ROOT/logs/build-$(date +%Y%m%d-%H%M%S).log"
    
    echo "Build log will be saved to: $LOG_FILE"
    echo ""
    
    # Start build with logging
    echo "Building $IMAGE_NAME..."
    echo "Progress will be shown here, detailed logs in $LOG_FILE"
    
    bitbake $IMAGE_NAME 2>&1 | tee "$LOG_FILE"
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo ""
        echo "✅ Build completed successfully!"
    else
        echo ""
        echo "❌ Build failed! Check log file: $LOG_FILE"
        exit 1
    fi
}

# Package build artifacts
package_artifacts() {
    echo ""
    echo "Packaging build artifacts..."
    
    DEPLOY_SOURCE="$BUILD_DIR/tmp/deploy/images/$MACHINE"
    TIMESTAMP=$(date +%Y%m%d-%H%M%S)
    PACKAGE_DIR="$DEPLOY_DIR/pynq-robot-$TIMESTAMP"
    
    if [ -d "$DEPLOY_SOURCE" ]; then
        mkdir -p "$PACKAGE_DIR"
        
        # Copy image files
        cp "$DEPLOY_SOURCE"/*.wic* "$PACKAGE_DIR/" 2>/dev/null || true
        cp "$DEPLOY_SOURCE"/*.ext4 "$PACKAGE_DIR/" 2>/dev/null || true
        cp "$DEPLOY_SOURCE"/*.tar.gz "$PACKAGE_DIR/" 2>/dev/null || true
        cp "$DEPLOY_SOURCE"/uImage "$PACKAGE_DIR/" 2>/dev/null || true
        cp "$DEPLOY_SOURCE"/*.dtb "$PACKAGE_DIR/" 2>/dev/null || true
        cp "$DEPLOY_SOURCE"/u-boot.* "$PACKAGE_DIR/" 2>/dev/null || true
        
        # Create manifest
        cat > "$PACKAGE_DIR/manifest.txt" << EOF
PYNQ-Z2 Robot System Build
Generated: $(date)
Build Type: $BUILD_TYPE
Machine: $MACHINE
Image: $IMAGE_NAME

Files included:
$(ls -la "$PACKAGE_DIR")

Build Information:
Host: $(hostname)
User: $(whoami)
Yocto Branch: $(cd ../poky && git branch --show-current 2>/dev/null || echo "unknown")
Build Time: $(grep "Build completed" "$LOG_FILE" | tail -1 || echo "unknown")

Flash Instructions:
1. Insert SD card (8GB or larger)
2. Identify device: lsblk
3. Flash image: sudo dd if=pynq-robot-image-*.wic of=/dev/sdX bs=4M status=progress
4. Sync: sync
5. Safely remove SD card

First Boot:
- Default login: root (no password in debug builds)
- Network: DHCP enabled on eth0
- Services: ROS2, Jupyter, robot-gateway-bridge auto-start
- Web interface: http://<robot-ip>:8888 (Jupyter)
                http://<robot-ip>:9090 (Robot Bridge)
EOF

        # Create symbolic link to latest
        ln -sf "$(basename "$PACKAGE_DIR")" "$DEPLOY_DIR/latest"
        
        echo "✅ Build artifacts packaged in: $PACKAGE_DIR"
        echo "   Symbolic link created: $DEPLOY_DIR/latest"
        
        # Show package contents
        echo ""
        echo "Package contents:"
        ls -la "$PACKAGE_DIR"
        
    else
        echo "❌ Deploy directory not found: $DEPLOY_SOURCE"
        exit 1
    fi
}

# Generate build report
generate_build_report() {
    echo ""
    echo "Generating build report..."
    
    REPORT_FILE="$PROJECT_ROOT/logs/build-report-$(date +%Y%m%d-%H%M%S).txt"
    
    cat > "$REPORT_FILE" << EOF
PYNQ-Z2 Robot System Build Report
Generated: $(date)

Build Configuration:
  Image: $IMAGE_NAME
  Machine: $MACHINE
  Build Type: $BUILD_TYPE
  
Build Environment:
  Host OS: $(uname -s -r)
  Docker: $(docker --version 2>/dev/null || echo "Not available")
  Yocto Version: $(bitbake --version 2>/dev/null || echo "Not available")
  
Build Statistics:
$(grep -E "(Build completed|ERROR|WARNING)" "$LOG_FILE" | tail -20)

Layer Information:
$(bitbake-layers show-layers 2>/dev/null || echo "Not available")

Disk Usage:
  Build directory: $(du -sh "$BUILD_DIR" 2>/dev/null || echo "Not available")
  Downloads: $(du -sh "$PROJECT_ROOT/downloads" 2>/dev/null || echo "Not available")
  Shared state: $(du -sh "$PROJECT_ROOT/sstate-cache" 2>/dev/null || echo "Not available")

Artifacts Location: $(realpath "$DEPLOY_DIR/latest" 2>/dev/null || echo "Not available")

Next Steps:
1. Test image in QEMU (optional): runqemu $MACHINE $IMAGE_NAME
2. Flash to SD card: ./flash-sdcard.sh <image> <device>
3. Boot PYNQ-Z2 and test robot functionality
4. Check services: systemctl status robot-gateway-bridge
EOF

    echo "✅ Build report saved to: $REPORT_FILE"
}

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up temporary files..."
    
    # Clean up BitBake cache if requested
    if [ "$CLEAN_CACHE" = "yes" ]; then
        echo "Cleaning BitBake cache..."
        bitbake -c cleanall $IMAGE_NAME || true
    fi
    
    echo "✅ Cleanup completed"
}

# Main build function
main() {
    local start_time=$(date +%s)
    
    check_prerequisites
    setup_build_environment
    validate_layers
    build_image
    package_artifacts
    generate_build_report
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    echo ""
    echo "=========================================="
    echo "Build Complete!"
    echo "=========================================="
    echo "Total build time: $((duration / 60)) minutes $((duration % 60)) seconds"
    echo ""
    echo "Artifacts available at: $DEPLOY_DIR/latest"
    echo ""
    echo "To flash to SD card:"
    echo "  ./flash-sdcard.sh $DEPLOY_DIR/latest/*.wic /dev/sdX"
    echo ""
}

# Handle script arguments
case "${1:-}" in
    --help|-h)
        echo "Usage: $0 [IMAGE_NAME] [MACHINE] [BUILD_TYPE]"
        echo ""
        echo "Arguments:"
        echo "  IMAGE_NAME: Image to build (default: pynq-robot-image)"
        echo "  MACHINE: Target machine (default: zynq-generic)"
        echo "  BUILD_TYPE: release or debug (default: release)"
        echo ""
        echo "Environment variables:"
        echo "  CLEAN_CACHE: Set to 'yes' to clean cache after build"
        echo ""
        echo "Examples:"
        echo "  $0                                    # Build with defaults"
        echo "  $0 pynq-robot-image zynq-generic debug  # Debug build"
        echo "  CLEAN_CACHE=yes $0                   # Clean cache after build"
        exit 0
        ;;
esac

# Trap cleanup on exit
trap cleanup EXIT

# Run main function
main "$@"