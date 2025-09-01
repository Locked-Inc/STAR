#!/bin/bash

# Script to resize the VM disk image for Yocto builds
# Yocto builds require 100GB+ of space

set -e

echo "=== Resizing VM Disk for Yocto Builds ==="

# Stop any running QEMU instances
echo "Stopping any running QEMU instances..."
pkill qemu-system-x86_64 || true
sleep 5

# Backup original image
echo "Creating backup of original image..."
cp jammy-server-cloudimg-amd64.img jammy-server-cloudimg-amd64.img.backup

# Resize the disk image to 120GB
echo "Resizing disk image to 120GB..."
qemu-img resize jammy-server-cloudimg-amd64.img 120G

echo "Disk resized successfully!"
echo "Note: The filesystem will auto-expand on first boot."
echo ""
echo "You can now start the VM with the usual command:"
echo "qemu-system-x86_64 -m 8192 -smp 4 -cpu qemu64 -machine type=q35 \\"
echo "  -drive file=jammy-server-cloudimg-amd64.img,if=virtio \\"
echo "  -drive file=cloud-init.iso,media=cdrom \\"
echo "  -netdev user,id=net0,hostfwd=tcp::2222-:22 \\"
echo "  -device virtio-net-pci,netdev=net0 -nographic"