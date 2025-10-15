#!/bin/bash
# STAR Robot - Build Cache Cleanup Script
# This script cleans the cached build artifacts to force a fresh rebuild

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STAR Build Cache Cleanup${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Ensure PYNQ submodule is on the correct branch
echo -e "${YELLOW}Checking PYNQ submodule branch...${NC}"
cd ~/Documents/git/STAR/pynq-image/PYNQ
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$CURRENT_BRANCH" != "star-customizations" ]; then
    echo -e "${YELLOW}Switching to star-customizations branch...${NC}"
    git checkout star-customizations
    if [ $? -ne 0 ]; then
        echo -e "${RED}ERROR: Failed to checkout star-customizations branch${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓${NC} PYNQ on branch: star-customizations"
echo ""

cd ~/Documents/git/STAR/pynq-image/PYNQ/sdbuild

# Step 1: Kill any chroot processes
echo -e "${YELLOW}Step 1: Killing chroot processes...${NC}"
if [ -d "build/jammy.arm" ]; then
    sudo scripts/kill_chroot_processes.sh build/jammy.arm || true
    echo -e "${GREEN}✓${NC} Processes killed"
else
    echo -e "${YELLOW}ℹ${NC} No jammy.arm directory found"
fi
echo ""

# Step 2: Unmount all filesystems (try multiple times)
echo -e "${YELLOW}Step 2: Unmounting filesystems...${NC}"
for attempt in {1..3}; do
    echo -e "  Attempt $attempt/3..."

    # Get all mounts related to jammy.arm in reverse order
    mounts=$(cat /proc/mounts | grep "jammy.arm" | cut -d' ' -f2 | sort -r || true)

    if [ -z "$mounts" ]; then
        echo -e "${GREEN}✓${NC} No filesystems mounted"
        break
    fi

    # Unmount each one WITH sudo (this was the missing piece!)
    for mount in $mounts; do
        echo "    Unmounting: $mount"
        sudo umount "$mount" || sudo umount -l "$mount" || sudo umount -f "$mount" || true
    done

    sleep 1
done
echo ""

# Step 3: Verify no mounts remain
echo -e "${YELLOW}Step 3: Verifying unmount...${NC}"
remaining_mounts=$(cat /proc/mounts | grep "jammy.arm" || true)
if [ -n "$remaining_mounts" ]; then
    echo -e "${RED}✗${NC} Some filesystems still mounted:"
    echo "$remaining_mounts"
    echo ""
    echo -e "${YELLOW}Attempting forced unmount...${NC}"
    echo "$remaining_mounts" | awk '{print $2}' | while read mount; do
        sudo umount -lf "$mount" || true
    done
else
    echo -e "${GREEN}✓${NC} All filesystems unmounted"
fi
echo ""

# Step 4: Remove the cached directories
echo -e "${YELLOW}Step 4: Removing cached directories...${NC}"
sudo rm -rf build/jammy.arm build/jammy.STAR-Z2 build/*.tar.gz || {
    echo -e "${RED}✗${NC} Failed to remove some directories"
    echo ""
    echo -e "${YELLOW}Showing what's preventing deletion:${NC}"
    lsof +D build/jammy.arm || true
    echo ""
    echo -e "${YELLOW}Still mounted:${NC}"
    mount | grep jammy.arm || echo "  (none)"
    exit 1
}
echo -e "${GREEN}✓${NC} Cache directories removed"
echo ""

# Step 5: Verify cleanup
echo -e "${YELLOW}Step 5: Verifying cleanup...${NC}"
if [ -d "build/jammy.arm" ]; then
    echo -e "${RED}✗${NC} jammy.arm still exists!"
    ls -la build/jammy.arm | head -20
    exit 1
else
    echo -e "${GREEN}✓${NC} Cache successfully cleaned"
fi
echo ""

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓ Cleanup Complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${GREEN}Next steps:${NC}"
echo "  1. Run the build to apply all customizations:"
echo "     ./build.sh"
echo ""
echo "  2. Or build directly:"
echo "     cd PYNQ/sdbuild"
echo "     make BOARDS=STAR-Z2 BOARDDIR=\$(realpath ~/Documents/git/STAR/pynq-image/board-config) REBUILD_PYNQ_ROOTFS=1"
echo ""
echo -e "${YELLOW}Expected changes in new build:${NC}"
echo "  • Java 17 (not Java 8)"
echo "  • Only en_US.UTF-8 locale"
echo "  • No Samba packages"
echo "  • No Python 2.7 packages"
echo "  • No Perl modules"
echo "  • No X11/Desktop packages"
echo "  • ~1.89 GB total savings"
echo ""
