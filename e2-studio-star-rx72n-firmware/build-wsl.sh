#!/bin/bash
# WSL Build Wrapper for e² studio generated Makefiles
# Converts Windows paths to WSL paths and builds the firmware

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/HardwareDebug"
MAKEFILE="${BUILD_DIR}/makefile"
MAKEFILE_BACKUP="${BUILD_DIR}/makefile.windows"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== STAR RX72N Firmware - WSL Build Wrapper ===${NC}"

# Check if GNURX toolchain is in PATH
if ! command -v rx-elf-gcc &> /dev/null; then
    echo -e "${RED}Error: rx-elf-gcc not found in PATH${NC}"
    echo "Adding GNURX to PATH..."
    export PATH="/home/sikar/gnurx/bin:$PATH"

    if ! command -v rx-elf-gcc &> /dev/null; then
        echo -e "${RED}Error: GNURX toolchain not found!${NC}"
        echo "Please install GNURX to ~/gnurx or update PATH"
        exit 1
    fi
fi

echo -e "${GREEN}✓ GNURX toolchain found:${NC} $(rx-elf-gcc --version | head -1)"

# Check if Makefile exists
if [ ! -f "$MAKEFILE" ]; then
    echo -e "${RED}Error: Makefile not found at $MAKEFILE${NC}"
    echo "Please build the project once in e² studio to generate the Makefile"
    exit 1
fi

# Backup original Makefile if not already backed up
if [ ! -f "$MAKEFILE_BACKUP" ]; then
    echo -e "${YELLOW}Backing up original Makefile...${NC}"
    cp "$MAKEFILE" "$MAKEFILE_BACKUP"
fi

echo -e "${YELLOW}Converting Windows paths to WSL paths...${NC}"

# Create a temporary modified Makefile
TEMP_MAKEFILE="${BUILD_DIR}/makefile.wsl"
cp "$MAKEFILE_BACKUP" "$TEMP_MAKEFILE"

# Convert ALL Windows paths to WSL paths
# Step 1: Convert C:/ to /mnt/c/ (forward slash variant)
sed -i 's|C:/|/mnt/c/|g' "$TEMP_MAKEFILE"

# Step 2: Convert C:\ to /mnt/c/ (backslash variant - must come before general backslash replacement)
sed -i 's|C:\\\\|/mnt/c/|g' "$TEMP_MAKEFILE"

# Step 3: Fix remaining escaped backslashes in paths (\\)
sed -i 's|\\\\|/|g' "$TEMP_MAKEFILE"

# Step 4: Replace Windows GNURX library paths with WSL GNURX paths
sed -i 's|-L"/mnt/c/ProgramData/GCC for Renesas RX [^"]*"|-L"${HOME}/gnurx/rx-elf/lib/gcc/rx-elf/14.2.0.202511-GNURX" -L"${HOME}/gnurx/rx-elf/rx-elf/lib"|g' "$TEMP_MAKEFILE"

# Replace the active Makefile with the WSL version
mv "$TEMP_MAKEFILE" "$MAKEFILE"

echo -e "${GREEN}✓ Paths converted (linker, includes, and source paths)${NC}"

# Change to build directory
cd "$BUILD_DIR"

# Parse command line arguments
MAKE_TARGET="all"
MAKE_FLAGS="-j$(nproc)"
CLEAN_FIRST="no"

while [[ $# -gt 0 ]]; do
    case $1 in
        clean)
            CLEAN_FIRST="yes"
            shift
            ;;
        -j*)
            MAKE_FLAGS="$1"
            shift
            ;;
        *)
            MAKE_TARGET="$1"
            shift
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN_FIRST" = "yes" ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    make clean
    echo -e "${GREEN}✓ Clean complete${NC}"
fi

# Build
echo -e "${YELLOW}Building firmware (target: $MAKE_TARGET, flags: $MAKE_FLAGS)...${NC}"
if make $MAKE_FLAGS $MAKE_TARGET; then
    echo -e "${GREEN}✓ Build successful!${NC}"

    # Show build artifacts
    if [ -f "e2-studio-star-rx72n-firmware.elf" ]; then
        echo ""
        echo -e "${GREEN}Build artifacts:${NC}"
        ls -lh e2-studio-star-rx72n-firmware.elf e2-studio-star-rx72n-firmware.mot 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
        echo ""
        echo -e "${GREEN}✓ Firmware ready:${NC} e2-studio-star-rx72n-firmware.elf"
    fi
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
