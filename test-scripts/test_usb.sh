#!/bin/bash
# Test USB ports on Raspberry Pi 5
# No internet connection needed - just checks what's connected

echo "=========================================="
echo "Raspberry Pi 5 USB Port Test"
echo "=========================================="
echo ""

echo "🔌 USB Devices (lsusb):"
lsusb
echo ""

echo "📦 Block Devices (USB drives):"
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL,MOUNTPOINTS
echo ""

echo "🖱️  Input Devices (mouse/keyboard):"
ls -l /dev/input/mouse* 2>/dev/null || echo "   No mouse detected"
ls -l /dev/input/event* 2>/dev/null | head -5
echo ""

echo "💾 USB Storage Devices:"
ls -l /dev/sd* 2>/dev/null || echo "   No USB storage detected"
echo ""

echo "📹 Video Devices (cameras):"
ls -l /dev/video* 2>/dev/null || echo "   No video devices detected"
echo ""

echo "🔍 Kernel Messages (last 20 lines):"
dmesg | tail -20
echo ""

echo "=========================================="
echo "Test Complete!"
echo "=========================================="
echo ""
echo "💡 How to test:"
echo "   1. Run this script: bash ~/test_usb.sh"
echo "   2. Plug in USB device (flash drive, mouse, etc.)"
echo "   3. Wait 2 seconds"
echo "   4. Run script again: bash ~/test_usb.sh"
echo "   5. Compare outputs - new device should appear!"
echo ""
echo "For flash drive:"
echo "   - Should appear as /dev/sda1 or /dev/sdb1"
echo "   - Mount: sudo mkdir /mnt/usb && sudo mount /dev/sda1 /mnt/usb"
echo "   - List: ls /mnt/usb"
echo ""
echo "For mouse:"
echo "   - Should appear as /dev/input/mouse0"
echo "   - Test: sudo cat /dev/input/mouse0 (wiggle mouse, see data)"
