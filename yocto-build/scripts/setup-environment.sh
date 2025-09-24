#!/bin/bash

# PYNQ-Z2 Robot System - Environment Setup Script
# Sets up the complete build environment for the robot system

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
BUILD_ROOT="$PROJECT_ROOT"

echo "=========================================="
echo "PYNQ-Z2 Robot System Environment Setup"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Build root: $BUILD_ROOT"
echo ""

# Check system requirements
check_system_requirements() {
    echo "Checking system requirements..."
    
    # Check available disk space (need at least 50GB)
    AVAILABLE_SPACE=$(df "$BUILD_ROOT" | tail -1 | awk '{print $4}')
    REQUIRED_SPACE=52428800  # 50GB in KB
    
    if [ "$AVAILABLE_SPACE" -lt "$REQUIRED_SPACE" ]; then
        echo "❌ Insufficient disk space. Need at least 50GB, available: $((AVAILABLE_SPACE/1024/1024))GB"
        exit 1
    else
        echo "✅ Sufficient disk space: $((AVAILABLE_SPACE/1024/1024))GB available"
    fi
    
    # Check Docker installation
    if command -v docker >/dev/null 2>&1; then
        echo "✅ Docker is installed"
        if docker info >/dev/null 2>&1; then
            echo "✅ Docker is running"
        else
            echo "❌ Docker is not running. Please start Docker service."
            exit 1
        fi
    else
        echo "❌ Docker is not installed. Please install Docker first."
        exit 1
    fi
    
    # Check for Xilinx tools (optional warning)
    if [ ! -d "/opt/Xilinx" ] && [ ! -d "/tools/Xilinx" ]; then
        echo "⚠️  Xilinx tools not found in standard locations"
        echo "   Please ensure Vivado/Vitis and PetaLinux are installed"
        echo "   You'll need to mount them when running Docker"
    else
        echo "✅ Xilinx tools directory found"
    fi
    
    echo ""
}

# Setup directory structure
setup_directories() {
    echo "Setting up directory structure..."
    
    # Ensure all required directories exist
    mkdir -p "$BUILD_ROOT"/{build,downloads,sstate-cache,deploy}
    mkdir -p "$BUILD_ROOT"/layers/{downloaded,meta-robot-slam,meta-vitis-ai}
    mkdir -p "$BUILD_ROOT"/logs
    mkdir -p "$BUILD_ROOT"/workspace
    
    echo "✅ Directory structure created"
    echo ""
}

# Download and configure meta-layers
setup_meta_layers() {
    echo "Setting up meta-layers..."
    
    # Check if setup-layers.sh exists and is executable
    SETUP_LAYERS_SCRIPT="$PROJECT_ROOT/configs/setup-layers.sh"
    if [ -f "$SETUP_LAYERS_SCRIPT" ]; then
        echo "Running layer setup script..."
        bash "$SETUP_LAYERS_SCRIPT"
    else
        echo "⚠️  Layer setup script not found at $SETUP_LAYERS_SCRIPT"
        echo "   You may need to run it manually later"
    fi
    
    echo ""
}

# Build Docker environment
build_docker_environment() {
    echo "Building Docker environment..."
    
    cd "$PROJECT_ROOT"
    
    # Get current user info for Docker build
    USER_ID=$(id -u)
    GROUP_ID=$(id -g)
    USER_NAME=$(whoami)
    
    # Build Docker image
    echo "Building Docker image (this may take several minutes)..."
    docker build \
        --build-arg USERNAME="$USER_NAME" \
        --build-arg USER_UID="$USER_ID" \
        --build-arg USER_GID="$GROUP_ID" \
        -t pynq-z2-builder:latest \
        -f docker/Dockerfile \
        .
    
    if [ $? -eq 0 ]; then
        echo "✅ Docker image built successfully"
    else
        echo "❌ Failed to build Docker image"
        exit 1
    fi
    
    echo ""
}

# Create convenience scripts
create_convenience_scripts() {
    echo "Creating convenience scripts..."
    
    # Create docker-run script
    cat > "$PROJECT_ROOT/docker-run.sh" << EOF
#!/bin/bash
# Convenience script to run the Docker build environment

SCRIPT_DIR=\$(dirname "\$(realpath "\$0")")

# Default Xilinx tools path - modify as needed
XILINX_PATH="/opt/Xilinx"

# Check if custom Xilinx path is provided
if [ "\$1" = "--xilinx-path" ] && [ -n "\$2" ]; then
    XILINX_PATH="\$2"
    shift 2
fi

# Check if Xilinx tools exist
if [ ! -d "\$XILINX_PATH" ]; then
    echo "⚠️  Xilinx tools not found at \$XILINX_PATH"
    echo "   Usage: \$0 [--xilinx-path /path/to/Xilinx] [additional docker args]"
    echo "   Container will run without Xilinx tools mounted"
    XILINX_MOUNT=""
else
    echo "Using Xilinx tools from: \$XILINX_PATH"
    XILINX_MOUNT="-v \$XILINX_PATH:/opt/Xilinx:ro"
fi

# Run Docker container
docker run -it --rm \\
    --hostname pynq-builder \\
    --network host \\
    -v "\$SCRIPT_DIR:/workspace/yocto-build:rw" \\
    \$XILINX_MOUNT \\
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \\
    -e DISPLAY=\${DISPLAY:-:0} \\
    -e USER_ID=\$(id -u) \\
    -e GROUP_ID=\$(id -g) \\
    pynq-z2-builder:latest \\
    "\$@"
EOF

    chmod +x "$PROJECT_ROOT/docker-run.sh"
    
    # Create build script
    cat > "$PROJECT_ROOT/build-image.sh" << EOF
#!/bin/bash
# Convenience script to build robot image

set -e

echo "Building PYNQ-Z2 Robot System Image..."
echo "This may take several hours on first build"
echo ""

# Run build in Docker container
./docker-run.sh bash -c "
    source /workspace/setup-build-env.sh
    cd /workspace/yocto-build
    source ../poky/oe-init-build-env build
    cp ../configs/local.conf conf/
    cp ../configs/bblayers.conf conf/
    bitbake pynq-robot-image
"

echo ""
echo "Build complete! Image should be in deploy/images/"
EOF

    chmod +x "$PROJECT_ROOT/build-image.sh"
    
    # Create flash script
    cat > "$PROJECT_ROOT/flash-sdcard.sh" << EOF
#!/bin/bash
# Flash SD card with robot image

if [ -z "\$1" ] || [ -z "\$2" ]; then
    echo "Usage: \$0 <image-file> <device>"
    echo "Example: \$0 deploy/pynq-robot-image.wic /dev/sdX"
    exit 1
fi

IMAGE="\$1"
DEVICE="\$2"

if [ ! -f "\$IMAGE" ]; then
    echo "Image file not found: \$IMAGE"
    exit 1
fi

if [ ! -b "\$DEVICE" ]; then
    echo "Device not found: \$DEVICE"
    exit 1
fi

echo "About to flash \$IMAGE to \$DEVICE"
echo "This will DESTROY all data on \$DEVICE"
read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! \$REPLY =~ ^[Yy]\$ ]]; then
    echo "Cancelled"
    exit 1
fi

echo "Flashing image..."
sudo dd if="\$IMAGE" of="\$DEVICE" bs=4M status=progress
sync

echo "Flash complete!"
EOF

    chmod +x "$PROJECT_ROOT/flash-sdcard.sh"
    
    echo "✅ Convenience scripts created:"
    echo "   - docker-run.sh: Run Docker build environment"
    echo "   - build-image.sh: Build robot system image"
    echo "   - flash-sdcard.sh: Flash SD card with image"
    echo ""
}

# Generate build status
generate_build_status() {
    echo "Generating build status..."
    
    cat > "$PROJECT_ROOT/build-status.txt" << EOF
PYNQ-Z2 Robot System - Build Environment Status
Generated: $(date)

Environment Setup: ✅ Complete
Docker Image: ✅ Built
Meta-layers: $([ -d "$PROJECT_ROOT/layers/downloaded" ] && echo "✅ Downloaded" || echo "❌ Not downloaded")

System Information:
- Host OS: $(uname -s) $(uname -r)
- Available Space: $(($(df "$BUILD_ROOT" | tail -1 | awk '{print $4}')/1024/1024))GB
- Docker Version: $(docker --version 2>/dev/null || echo "Not available")

Next Steps:
1. Download meta-layers: ./configs/setup-layers.sh
2. Start Docker environment: ./docker-run.sh
3. Build robot image: ./build-image.sh
4. Flash to SD card: ./flash-sdcard.sh <image> <device>

For detailed instructions, see:
- README.md
- docs/setup-guide.md
EOF

    echo "✅ Build status saved to build-status.txt"
    echo ""
}

# Main setup function
main() {
    check_system_requirements
    setup_directories
    setup_meta_layers
    build_docker_environment
    create_convenience_scripts
    generate_build_status
    
    echo "=========================================="
    echo "Environment Setup Complete!"
    echo "=========================================="
    echo ""
    echo "Next steps:"
    echo "1. Review build-status.txt for system information"
    echo "2. Run './docker-run.sh' to start the build environment"
    echo "3. Run './build-image.sh' to build the robot system image"
    echo ""
    echo "For help:"
    echo "- Check README.md for detailed instructions"
    echo "- Review logs in logs/ directory for any issues"
    echo ""
}

# Run setup if script is executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi