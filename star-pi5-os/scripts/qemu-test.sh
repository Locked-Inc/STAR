#!/bin/bash
#
# QEMU Testing Script for STAR Robot OS
# Uses generic ARM64 'virt' machine since QEMU doesn't support Pi5 (BCM2712) yet
#
# This allows smoke testing of:
# - Systemd boot and services
# - Userspace applications
# - Network connectivity
# - Basic functionality
#
# NOT tested (Pi5-specific):
# - GPIO, SPI, I2C
# - Camera (CSI)
# - Pi5 firmware/bootloader
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILDROOT_DIR="${PROJECT_DIR}/buildroot-2025.02"
OUTPUT_DIR="${BUILDROOT_DIR}/output"
IMAGES_DIR="${OUTPUT_DIR}/images"

# QEMU settings
QEMU_MEMORY="${QEMU_MEMORY:-2048}"
QEMU_CPUS="${QEMU_CPUS:-4}"
QEMU_SSH_PORT="${QEMU_SSH_PORT:-2222}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

check_dependencies() {
    info "Checking dependencies..."

    if ! command -v qemu-system-aarch64 &> /dev/null; then
        error "qemu-system-aarch64 not found. Install with: sudo apt install qemu-system-arm"
    fi

    if ! command -v qemu-img &> /dev/null; then
        error "qemu-img not found. Install with: sudo apt install qemu-utils"
    fi

    info "All dependencies found"
}

check_build() {
    if [ ! -f "${IMAGES_DIR}/sdcard.img" ]; then
        error "sdcard.img not found. Run 'make build' first."
    fi

    if [ ! -f "${IMAGES_DIR}/Image" ]; then
        error "Kernel Image not found. Run 'make build' first."
    fi

    info "Build artifacts found"
}

prepare_qemu_image() {
    info "Preparing QEMU disk image..."

    QEMU_DIR="${PROJECT_DIR}/qemu"
    mkdir -p "${QEMU_DIR}"

    # Create a copy of rootfs for QEMU (we'll expand it)
    QEMU_ROOTFS="${QEMU_DIR}/rootfs-qemu.ext4"

    if [ ! -f "${QEMU_ROOTFS}" ] || [ "${IMAGES_DIR}/sdcard.img" -nt "${QEMU_ROOTFS}" ]; then
        info "Extracting rootfs for QEMU..."
        # Use the rootfs.ext4 directly instead of extracting from sdcard.img
        # This avoids size calculation issues with larger filesystems
        cp "${IMAGES_DIR}/rootfs.ext4" "${QEMU_ROOTFS}"

        # Apply nofail mount option for QEMU compatibility (mmcblk devices don't exist)
        if command -v e2fsck &> /dev/null; then
            e2fsck -f -y "${QEMU_ROOTFS}" || true

            # Mount and patch fstab and shadow if needed
            MOUNT_DIR=$(mktemp -d)
            if sudo mount -o loop "${QEMU_ROOTFS}" "${MOUNT_DIR}" 2>/dev/null; then
                if grep -q "mmcblk" "${MOUNT_DIR}/etc/fstab" && ! grep -q "nofail" "${MOUNT_DIR}/etc/fstab"; then
                    info "Patching fstab for QEMU compatibility..."
                    sudo sed -i 's/defaults\s*0/defaults,nofail 0/g' "${MOUNT_DIR}/etc/fstab"
                fi
                # Fix shadow file to allow passwordless root login (must have 9 fields)
                if grep -q "^root::::::::" "${MOUNT_DIR}/etc/shadow" || ! grep -q "^root::" "${MOUNT_DIR}/etc/shadow"; then
                    info "Fixing root login in shadow file..."
                    sudo sed -i 's/^root:[^:]*:[^:]*:[^:]*:[^:]*:[^:]*:[^:]*:[^:]*:[^:]*$/root::0:0:99999:7:::/' "${MOUNT_DIR}/etc/shadow"
                    sudo sed -i 's/^root::::::::$/root::0:0:99999:7:::/' "${MOUNT_DIR}/etc/shadow"
                fi
                sudo umount "${MOUNT_DIR}"
            fi
            rmdir "${MOUNT_DIR}"
        fi

        # No need to resize - rootfs.ext4 is already the correct size
    else
        info "Using existing QEMU rootfs"
    fi
}

run_qemu() {
    info "Starting QEMU..."
    info "SSH will be available at: localhost:${QEMU_SSH_PORT}"
    info "Login: root (no password by default)"
    info "Press Ctrl+A X to exit QEMU"
    echo ""

    QEMU_DIR="${PROJECT_DIR}/qemu"
    QEMU_ROOTFS="${QEMU_DIR}/rootfs-qemu.ext4"

    qemu-system-aarch64 \
        -M virt \
        -cpu max \
        -m "${QEMU_MEMORY}" \
        -smp "${QEMU_CPUS}" \
        -kernel "${IMAGES_DIR}/Image" \
        -drive file="${QEMU_ROOTFS}",format=raw,if=virtio \
        -append "root=/dev/vda rw console=ttyAMA0 systemd.unified_cgroup_hierarchy=1" \
        -netdev user,id=net0,hostfwd=tcp::${QEMU_SSH_PORT}-:22 \
        -device virtio-net-pci,netdev=net0 \
        -nographic \
        -no-reboot \
        "$@"
}

run_qemu_graphical() {
    info "Starting QEMU with graphical output..."
    info "SSH will be available at: localhost:${QEMU_SSH_PORT}"
    echo ""

    QEMU_DIR="${PROJECT_DIR}/qemu"
    QEMU_ROOTFS="${QEMU_DIR}/rootfs-qemu.ext4"

    qemu-system-aarch64 \
        -M virt \
        -cpu max \
        -m "${QEMU_MEMORY}" \
        -smp "${QEMU_CPUS}" \
        -kernel "${IMAGES_DIR}/Image" \
        -drive file="${QEMU_ROOTFS}",format=raw,if=virtio \
        -append "root=/dev/vda rw console=ttyAMA0 systemd.unified_cgroup_hierarchy=1" \
        -netdev user,id=net0,hostfwd=tcp::${QEMU_SSH_PORT}-:22 \
        -device virtio-net-pci,netdev=net0 \
        -device virtio-gpu-pci \
        -display gtk \
        -no-reboot \
        "$@"
}

show_help() {
    cat << EOF
STAR Robot OS QEMU Testing Script

Usage: $0 [command] [options]

Commands:
    run         Start QEMU in serial console mode (default)
    graphical   Start QEMU with graphical display
    ssh         Connect to running QEMU instance via SSH
    clean       Remove QEMU working files
    help        Show this help message

Environment Variables:
    QEMU_MEMORY     RAM size in MB (default: 2048)
    QEMU_CPUS       Number of CPUs (default: 4)
    QEMU_SSH_PORT   SSH port forwarding (default: 2222)

Examples:
    $0 run                          # Start QEMU with serial console
    $0 graphical                    # Start with graphical display
    QEMU_MEMORY=4096 $0 run         # Start with 4GB RAM
    $0 ssh                          # SSH into running instance

Notes:
    - QEMU uses the generic 'virt' machine (Pi5/BCM2712 not supported)
    - Good for testing: systemd, services, userspace applications
    - NOT tested: GPIO, cameras, Pi5-specific hardware

EOF
}

connect_ssh() {
    info "Connecting to QEMU via SSH..."
    ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -p "${QEMU_SSH_PORT}" root@localhost
}

clean_qemu() {
    info "Cleaning QEMU files..."
    rm -rf "${PROJECT_DIR}/qemu"
    info "QEMU files removed"
}

# Main
case "${1:-run}" in
    run)
        check_dependencies
        check_build
        prepare_qemu_image
        shift 2>/dev/null || true
        run_qemu "$@"
        ;;
    graphical)
        check_dependencies
        check_build
        prepare_qemu_image
        shift
        run_qemu_graphical "$@"
        ;;
    ssh)
        connect_ssh
        ;;
    clean)
        clean_qemu
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        error "Unknown command: $1. Use '$0 help' for usage."
        ;;
esac
