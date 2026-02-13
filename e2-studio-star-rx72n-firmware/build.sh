#!/bin/bash
# Portable CMake build script for STAR RX72N Firmware
# Works on: WSL, Linux, macOS, Dev Containers

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== STAR RX72N Firmware - CMake Build ===${NC}"

# Check for rx-elf-gcc and auto-add common locations to PATH
if ! command -v rx-elf-gcc &> /dev/null; then
    # Try common GNURX installation locations
    COMMON_PATHS=(
        "$HOME/gnurx/bin"
        "/opt/gnurx/bin"
        "/usr/local/gnurx/bin"
    )

    for GNURX_PATH in "${COMMON_PATHS[@]}"; do
        if [ -f "$GNURX_PATH/rx-elf-gcc" ]; then
            export PATH="$GNURX_PATH:$PATH"
            echo -e "${YELLOW}Added $GNURX_PATH to PATH${NC}"
            break
        fi
    done

    # Check again after trying to add to PATH
    if ! command -v rx-elf-gcc &> /dev/null; then
        echo -e "${RED}Error: rx-elf-gcc not found in PATH${NC}"
        echo "Please install GNURX toolchain or add it to PATH"
        echo ""
        echo "Example:"
        echo "  export PATH=\"\$HOME/gnurx/bin:\$PATH\""
        exit 1
    fi
fi

echo -e "${GREEN}✓ GNURX toolchain found:${NC} $(rx-elf-gcc --version | head -1)"

# Parse arguments
BUILD_TYPE="Debug"
CLEAN=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        clean)
            CLEAN=true
            shift
            ;;
        release|Release)
            BUILD_TYPE="Release"
            shift
            ;;
        debug|Debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  clean           Clean build directory before building"
            echo "  debug           Build in Debug mode (default)"
            echo "  release         Build in Release mode"
            echo "  -v, --verbose   Verbose build output"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h for help"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ Clean complete${NC}"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring CMake (${BUILD_TYPE} mode)...${NC}"
cd "$BUILD_DIR"

cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/toolchain-rx72n.cmake" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ..

echo -e "${GREEN}✓ Configuration complete${NC}"

# Build
echo -e "${YELLOW}Building firmware...${NC}"

# Portable CPU count (nproc on Linux, sysctl on macOS)
if command -v nproc &> /dev/null; then
    JOBS="$(nproc)"
else
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 1)"
fi

if [ "$VERBOSE" = true ]; then
    cmake --build . --verbose
else
    cmake --build . -- -j"${JOBS}"
fi

echo ""
echo -e "${GREEN}✓ Build successful!${NC}"
echo ""
echo -e "${GREEN}Build artifacts:${NC}"
find . -maxdepth 1 \( -name '*.elf' -o -name '*.mot' -o -name '*.map' \) -exec ls -lh {} + 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo -e "${GREEN}Firmware ready:${NC} star-rx72n-firmware.elf"
echo -e "${GREEN}Flash file:${NC} star-rx72n-firmware.mot"
