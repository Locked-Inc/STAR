#!/usr/bin/env python3
"""
Test USB webcam on Raspberry Pi 5

This script:
1. Detects available video devices
2. Opens the camera
3. Captures frames
4. Saves test images
5. Shows camera properties

No display needed - saves images to files for verification.
"""

import cv2
import sys
import os

def find_video_devices():
    """Find all available video devices"""
    devices = []
    for i in range(10):  # Check /dev/video0 through /dev/video9
        device = f'/dev/video{i}'
        if os.path.exists(device):
            devices.append(device)
    return devices

def test_camera(device_path='/dev/video0'):
    """Test camera capture and save images"""

    print("=" * 60)
    print("Raspberry Pi 5 USB Webcam Test")
    print("=" * 60)
    print()

    # Find available devices
    print("🔍 Scanning for video devices...")
    devices = find_video_devices()

    if not devices:
        print("❌ No video devices found!")
        print("\n💡 Troubleshooting:")
        print("   1. Is USB camera plugged in?")
        print("   2. Check with: ls -l /dev/video*")
        print("   3. Check USB devices: lsusb")
        return False

    print(f"✅ Found {len(devices)} video device(s):")
    for dev in devices:
        print(f"   - {dev}")
    print()

    # Try to open camera
    print(f"📹 Opening camera: {device_path}")

    # Try different backend priorities
    # CAP_V4L2 is best for Linux
    cap = cv2.VideoCapture(device_path, cv2.CAP_V4L2)

    if not cap.isOpened():
        print(f"❌ Failed to open {device_path}")
        print("\n   Trying without backend specification...")
        cap = cv2.VideoCapture(device_path)

        if not cap.isOpened():
            print("❌ Still failed to open camera")
            print("\n💡 Try:")
            print("   sudo chmod 666 /dev/video0")
            return False

    print("✅ Camera opened successfully!")
    print()

    # Get camera properties
    print("📊 Camera Properties:")
    width = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    height = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    fps = cap.get(cv2.CAP_PROP_FPS)

    print(f"   Resolution: {int(width)}x{int(height)}")
    print(f"   FPS: {fps}")
    print()

    # Try to set resolution (optional)
    target_width = 640
    target_height = 480
    print(f"🔧 Setting resolution to {target_width}x{target_height}...")

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, target_width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, target_height)

    actual_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"   Actual resolution: {actual_width}x{actual_height}")
    print()

    # Capture test frames
    print("📸 Capturing test frames...")

    # Skip first few frames (camera initialization)
    for i in range(5):
        ret, frame = cap.read()

    # Capture and save test images
    test_dir = "/home/pi/webcam_test"
    os.makedirs(test_dir, exist_ok=True)

    num_test_frames = 5
    successful_captures = 0

    for i in range(num_test_frames):
        ret, frame = cap.read()

        if ret:
            filename = f"{test_dir}/test_frame_{i:03d}.jpg"
            cv2.imwrite(filename, frame)
            print(f"   ✅ Frame {i+1}/{num_test_frames}: {filename}")
            print(f"      Shape: {frame.shape}, Dtype: {frame.dtype}")
            successful_captures += 1
        else:
            print(f"   ❌ Frame {i+1}/{num_test_frames}: Failed to capture")

        # Small delay between captures
        import time
        time.sleep(0.2)

    print()

    # Release camera
    cap.release()

    # Summary
    print("=" * 60)
    print("📊 Test Summary")
    print("=" * 60)
    print(f"   Captured: {successful_captures}/{num_test_frames} frames")
    print(f"   Saved to: {test_dir}/")
    print()

    if successful_captures == num_test_frames:
        print("✅ Camera test PASSED!")
        print()
        print("💡 Next steps:")
        print("   - View images: ls -lh " + test_dir)
        print("   - Transfer to laptop: scp pi@star-robot.local:" + test_dir + "/*.jpg .")
        print("   - Use for person detection (TensorFlow Lite)")
        return True
    else:
        print("⚠️  Camera test PARTIAL - some frames failed")
        return False

def continuous_capture(device_path='/dev/video0', duration=10):
    """Capture frames continuously for testing"""

    print(f"📹 Continuous capture for {duration} seconds...")

    cap = cv2.VideoCapture(device_path, cv2.CAP_V4L2)

    if not cap.isOpened():
        print("❌ Failed to open camera")
        return

    import time
    start_time = time.time()
    frame_count = 0

    while time.time() - start_time < duration:
        ret, frame = cap.read()
        if ret:
            frame_count += 1
        else:
            print("⚠️  Dropped frame")

    elapsed = time.time() - start_time
    cap.release()

    print(f"\n📊 Captured {frame_count} frames in {elapsed:.2f}s")
    print(f"   Average FPS: {frame_count/elapsed:.2f}")

def check_usb_devices():
    """Check what USB devices are connected"""
    print("🔌 USB Devices:")
    import subprocess

    try:
        result = subprocess.run(['lsusb'], capture_output=True, text=True)
        print(result.stdout)
    except Exception as e:
        print(f"   lsusb not available: {e}")

    print()

if __name__ == "__main__":
    # Check USB devices first
    check_usb_devices()

    # Test camera
    device = '/dev/video0'

    if len(sys.argv) > 1:
        if sys.argv[1] == 'continuous':
            continuous_capture(device)
        else:
            device = sys.argv[1]
            test_camera(device)
    else:
        test_camera(device)
