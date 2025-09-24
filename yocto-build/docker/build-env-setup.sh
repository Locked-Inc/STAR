#!/bin/bash

# PYNQ-Z2 Yocto Build Environment Setup Script
# This script sets up the build environment inside the Docker container

set -e

echo "=== PYNQ-Z2 Yocto Build Environment Setup ==="

# Check if Xilinx tools are available (mounted from host)
XILINX_TOOLS_PATH="/opt/Xilinx"
PETALINUX_PATH=""
VIVADO_PATH=""

if [ -d "$XILINX_TOOLS_PATH" ]; then
    echo "✓ Xilinx tools directory found: $XILINX_TOOLS_PATH"
    
    # Find PetaLinux installation
    PETALINUX_CANDIDATES=$(find $XILINX_TOOLS_PATH -name "petalinux*" -type d 2>/dev/null | head -1)
    if [ -n "$PETALINUX_CANDIDATES" ]; then
        PETALINUX_PATH="$PETALINUX_CANDIDATES"
        echo "✓ PetaLinux found: $PETALINUX_PATH"
    else
        echo "⚠ PetaLinux not found in $XILINX_TOOLS_PATH"
    fi
    
    # Find Vivado/Vitis installation
    VIVADO_CANDIDATES=$(find $XILINX_TOOLS_PATH -name "Vitis" -type d 2>/dev/null | head -1)
    if [ -n "$VIVADO_CANDIDATES" ]; then
        VIVADO_PATH="$VIVADO_CANDIDATES"
        echo "✓ Vitis/Vivado found: $VIVADO_PATH"
    else
        echo "⚠ Vitis/Vivado not found in $XILINX_TOOLS_PATH"
    fi
else
    echo "⚠ Xilinx tools not found. Please mount Xilinx installation to $XILINX_TOOLS_PATH"
    echo "   Docker run example:"
    echo "   docker run -v /path/to/Xilinx:/opt/Xilinx:ro ..."
fi

# Set up environment variables
echo "Setting up build environment..."

# Yocto build configuration
export BB_ENV_EXTRAWHITE="MACHINE DL_DIR SSTATE_DIR TMPDIR BB_HASHSERVER_UPSTREAM"
export MACHINE="zynq-generic"
export DL_DIR="/workspace/yocto-build/downloads"
export SSTATE_DIR="/workspace/yocto-build/sstate-cache"

# PetaLinux settings (if available)
if [ -n "$PETALINUX_PATH" ] && [ -f "$PETALINUX_PATH/settings.sh" ]; then
    echo "Sourcing PetaLinux settings..."
    source "$PETALINUX_PATH/settings.sh"
fi

# Vivado/Vitis settings (if available)
if [ -n "$VIVADO_PATH" ]; then
    VIVADO_SETTINGS="$VIVADO_PATH/settings64.sh"
    if [ -f "$VIVADO_SETTINGS" ]; then
        echo "Sourcing Vivado/Vitis settings..."
        source "$VIVADO_SETTINGS"
    fi
fi

# Create build environment script
cat > /workspace/setup-build-env.sh << 'EOF'
#!/bin/bash
# Source this script to set up the build environment

# Xilinx tools
export XILINX_TOOLS_PATH="/opt/Xilinx"
if [ -d "$XILINX_TOOLS_PATH" ]; then
    # Find and source PetaLinux
    PETALINUX_SETTINGS=$(find $XILINX_TOOLS_PATH -name "settings.sh" -path "*/petalinux*" | head -1)
    if [ -f "$PETALINUX_SETTINGS" ]; then
        echo "Sourcing PetaLinux: $PETALINUX_SETTINGS"
        source "$PETALINUX_SETTINGS"
    fi
    
    # Find and source Vitis/Vivado
    VITIS_SETTINGS=$(find $XILINX_TOOLS_PATH -name "settings64.sh" -path "*/Vitis*" | head -1)
    if [ -f "$VITIS_SETTINGS" ]; then
        echo "Sourcing Vitis: $VITIS_SETTINGS"
        source "$VITIS_SETTINGS"
    fi
fi

# Yocto environment
export BB_ENV_EXTRAWHITE="MACHINE DL_DIR SSTATE_DIR TMPDIR"
export MACHINE="zynq-generic"
export DL_DIR="/workspace/yocto-build/downloads"
export SSTATE_DIR="/workspace/yocto-build/sstate-cache"
export TMPDIR="/workspace/yocto-build/build/tmp"

# Build optimization
export BB_NUMBER_THREADS="$(nproc)"
export PARALLEL_MAKE="-j $(nproc)"

echo "Build environment configured:"
echo "  MACHINE: $MACHINE"
echo "  DL_DIR: $DL_DIR"
echo "  SSTATE_DIR: $SSTATE_DIR"
echo "  Build threads: $BB_NUMBER_THREADS"
EOF

chmod +x /workspace/setup-build-env.sh

echo ""
echo "=== Setup Complete ==="
echo "To use this environment:"
echo "1. Mount your Xilinx tools: -v /path/to/Xilinx:/opt/Xilinx:ro"
echo "2. Source the build environment: source /workspace/setup-build-env.sh"
echo "3. Navigate to your build directory and start building"
echo ""