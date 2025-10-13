#!/usr/bin/env python3
# esp32-firmware/docs/ota/python/ota_monitor.py

"""
Monitor OTA update progress

This script continuously polls the ESP32 for OTA status and displays
a real-time progress indicator until the update completes or fails.
"""

import time
import sys
from ota_get_status import get_ota_status, OTAState


def monitor_ota_update(uart, timeout_seconds=300):
    """
    Monitor OTA update progress with detailed status reporting

    Args:
        uart: Serial connection to ESP32
        timeout_seconds: Maximum time to wait (default: 5 minutes)

    Returns:
        bool: True if update succeeded, False if failed/timeout
    """
    start_time = time.time()
    last_progress = -1
    last_state = None
    stall_counter = 0

    print("\n" + "="*60)
    print("OTA UPDATE MONITOR")
    print("="*60)

    while True:
        # Check timeout
        elapsed = time.time() - start_time
        if elapsed > timeout_seconds:
            print(f"\n[ERROR] Timeout after {elapsed:.1f} seconds")
            return False

        # Get status
        status = get_ota_status(uart)
        if not status:
            print("[WARN] Failed to get status, retrying...")
            time.sleep(0.5)
            continue

        state = status['state']
        progress = status['progress']
        downloaded = status['bytes_downloaded']
        total = status['total_bytes']

        # Detect state changes
        if state != last_state:
            print()  # New line for state change

            if state == OTAState.CHECKING:
                print("[CHECK] Checking for updates...")
            elif state == OTAState.DOWNLOADING:
                print(f"[DOWNLOAD] Starting download ({total:,} bytes)...")
            elif state == OTAState.VERIFYING:
                print("[VERIFY] Verifying firmware integrity (SHA256)...")
            elif state == OTAState.INSTALLING:
                print("[INSTALL] Installing firmware to flash...")

            last_state = state

        # Show download progress
        if state == OTAState.DOWNLOADING:
            if progress != last_progress:
                bar_length = 40
                filled = int(bar_length * progress / 100)
                bar = '#' * filled + '-' * (bar_length - filled)

                print(f"\r   [{bar}] {progress}% ({downloaded:,}/{total:,} bytes)", end='', flush=True)
                last_progress = progress
                stall_counter = 0
            else:
                stall_counter += 1
                if stall_counter > 30:  # 30 seconds without progress
                    print(f"\n[WARN] Download stalled at {progress}%")

        # Check for completion
        if state == OTAState.COMPLETE:
            print("\n")
            print("="*60)
            print("[SUCCESS] UPDATE SUCCESSFUL!")
            print("="*60)
            print("ESP32 will reboot in 3 seconds...")
            print("New firmware will be active after reboot.")
            return True

        # Check for failure
        elif state == OTAState.FAILED:
            print("\n")
            print("="*60)
            print("[FAILED] UPDATE FAILED!")
            print("="*60)
            print("ESP32 is still running old firmware.")
            print("Check ESP32 UART logs for detailed error.")
            return False

        time.sleep(1)  # Poll every second


if __name__ == "__main__":
    import serial

    if len(sys.argv) < 2:
        print("Usage: python ota_monitor.py <serial_port> [timeout_seconds]")
        print("\nExample:")
        print("  python ota_monitor.py /dev/ttyUSB0")
        print("  python ota_monitor.py /dev/ttyUSB0 600")
        sys.exit(1)

    port = sys.argv[1]
    timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 300

    # Open UART connection
    uart = serial.Serial(port, 115200, timeout=1)

    # Monitor progress
    success = monitor_ota_update(uart, timeout_seconds=timeout)

    uart.close()

    sys.exit(0 if success else 1)
