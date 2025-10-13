#!/usr/bin/env python3
# esp32-firmware/docs/ota/python/ota_complete_workflow.py

"""
Complete OTA update workflow

This script provides a complete end-to-end OTA update workflow including:
- Sending the update command
- Monitoring progress
- Parsing errors on failure
- Waiting for reboot
"""

import serial
import time
import sys
import re
from ota_send_update import send_ota_update
from ota_monitor import monitor_ota_update


def parse_ota_error_from_logs(uart):
    """
    Read ESP32 logs and detect OTA error messages

    Args:
        uart: Serial connection to ESP32

    Returns:
        dict with 'category' and 'message', or None
    """
    error_patterns = {
        'network': [
            (r'WiFi not connected', 'WiFi connection lost'),
            (r'HTTP GET request failed', 'Failed to connect to server'),
            (r'OTA begin failed', 'Could not start OTA download'),
        ],
        'download': [
            (r'OTA download failed', 'Download interrupted'),
            (r'OTA data incomplete', 'Downloaded data is incomplete'),
            (r'connection timeout', 'Server connection timed out'),
        ],
        'verification': [
            (r'SHA256 verification failed', 'Firmware hash mismatch'),
            (r'Expected: ([a-f0-9]+)', 'SHA256 mismatch (see logs for hashes)'),
            (r'Invalid firmware image', 'Downloaded file is not valid ESP32 firmware'),
        ],
        'installation': [
            (r'OTA finish failed', 'Could not finalize installation'),
            (r'Failed to mark partition bootable', 'Flash partition error'),
        ],
        'storage': [
            (r'No free OTA partition', 'Both OTA partitions in use'),
            (r'Flash write error', 'Flash memory hardware error'),
        ]
    }

    timeout = time.time() + 5  # Read logs for 5 seconds

    while time.time() < timeout:
        try:
            line = uart.readline().decode('utf-8', errors='ignore').strip()
            if not line or 'ota_manager' not in line.lower():
                continue

            # Check each category
            for category, patterns in error_patterns.items():
                for pattern, description in patterns:
                    if re.search(pattern, line, re.IGNORECASE):
                        return {
                            'category': category,
                            'description': description,
                            'raw_message': line
                        }
        except:
            pass

    return None


def complete_ota_workflow(uart_port, url, sha256=None, allow_downgrade=False):
    """
    Complete OTA update with comprehensive monitoring and error handling

    Args:
        uart_port: Serial port device (e.g., '/dev/ttyUSB0')
        url: Firmware URL
        sha256: Optional SHA256 hash
        allow_downgrade: Allow version downgrade

    Returns:
        bool: True if update succeeded
    """
    # Open connection
    uart = serial.Serial(uart_port, 115200, timeout=1)

    try:
        # Step 1: Send OTA command
        print("Step 1: Initiating OTA update...")
        if not send_ota_update(uart, url, sha256, allow_downgrade):
            print("[ERROR] Failed to send OTA command")
            return False

        # Wait a moment for ESP32 to start the update
        time.sleep(1)

        # Step 2: Monitor progress
        print("\nStep 2: Monitoring update progress...")
        success = monitor_ota_update(uart, timeout_seconds=300)

        if not success:
            # Step 3: Parse error logs
            print("\nStep 3: Analyzing failure...")
            error = parse_ota_error_from_logs(uart)
            if error:
                print(f"\n[ERROR] Error Details:")
                print(f"   Category: {error['category']}")
                print(f"   Description: {error['description']}")
                print(f"   Raw message: {error['raw_message']}")
            return False

        # Step 4: Wait for reboot
        print("\nStep 4: Waiting for ESP32 to reboot...")
        time.sleep(5)

        # Step 5: Verify new version (optional)
        print("\nStep 5: Verifying new firmware...")
        # Could send CMD_GET_VERSION here to confirm

        print("\n[SUCCESS] OTA update completed successfully!")
        return True

    finally:
        uart.close()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python ota_complete_workflow.py <serial_port> <url> [sha256] [allow_downgrade]")
        print("\nExamples:")
        print("  python ota_complete_workflow.py /dev/ttyUSB0 https://example.com/fw-v0.2.0.bin")
        print("  python ota_complete_workflow.py /dev/ttyUSB0 https://example.com/fw-v0.2.0.bin a3f5e8b1...")
        print("  python ota_complete_workflow.py /dev/ttyUSB0 https://example.com/fw-v0.1.0.bin a3f5e8b1... true")
        sys.exit(1)

    port = sys.argv[1]
    url = sys.argv[2]
    sha256 = sys.argv[3] if len(sys.argv) > 3 else None
    allow_downgrade = (len(sys.argv) > 4 and sys.argv[4].lower() == 'true')

    success = complete_ota_workflow(port, url, sha256, allow_downgrade)

    if success:
        print("\n[WAIT] Waiting for reboot...")
        time.sleep(5)
        print("[OK] ESP32 should be running new firmware now!")
    else:
        print("\n[WARN] Update unsuccessful. ESP32 still on old firmware.")

    sys.exit(0 if success else 1)
