#!/usr/bin/env python3
"""
Minimal USB webcam test - no external dependencies
Just checks if device exists and basic properties
"""

import os
import sys

def check_video_devices():
    """Find all video devices"""
    print("=" * 60)
    print("USB Webcam Detection Test (No Dependencies)")
    print("=" * 60)
    print()

    # Check for video devices
    print("🔍 Checking for video devices...")
    devices = []

    for i in range(10):
        device = f'/dev/video{i}'
        if os.path.exists(device):
            devices.append(device)

            # Get device info
            stat_info = os.stat(device)
            print(f"✅ Found: {device}")
            print(f"   Permissions: {oct(stat_info.st_mode)[-3:]}")
            print(f"   Owner: {stat_info.st_uid}:{stat_info.st_gid}")
            print()

    if not devices:
        print("❌ No video devices found")
        print()
        print("💡 Troubleshooting:")
        print("   1. Is USB camera plugged in?")
        print("   2. Check USB devices with: lsusb")
        print("   3. Check kernel messages: dmesg | grep -i video")
        return False

    return True

def check_usb_devices():
    """Check USB devices (if lsusb available)"""
    print("🔌 Checking USB devices...")

    try:
        import subprocess
        result = subprocess.run(['lsusb'], capture_output=True, text=True)

        if result.returncode == 0:
            print(result.stdout)
        else:
            print("   lsusb command failed")
    except:
        print("   lsusb not available")

    print()

def check_v4l_info():
    """Try to get video4linux info"""
    print("📹 Checking video device capabilities...")

    # Check if v4l2-ctl is available
    try:
        import subprocess
        result = subprocess.run(['which', 'v4l2-ctl'], capture_output=True)

        if result.returncode == 0:
            # Get device info
            result = subprocess.run(['v4l2-ctl', '--list-devices'],
                                  capture_output=True, text=True)
            print(result.stdout)
        else:
            print("   v4l2-ctl not installed (optional)")
    except:
        print("   v4l-utils not available")

    print()

if __name__ == "__main__":
    check_usb_devices()

    if check_video_devices():
        check_v4l_info()

        print("=" * 60)
        print("✅ Webcam hardware detected!")
        print()
        print("📝 Next steps:")
        print("   To test camera capture, you need opencv:")
        print("   1. Connect board to internet (router via Ethernet)")
        print("   2. sudo apt update")
        print("   3. sudo apt install python3-opencv")
        print("   4. Then run full test_webcam.py")
        print()
        print("   Or use the full test script on laptop to transfer images.")
    else:
        print("=" * 60)
        print("❌ No webcam detected")
        print()
