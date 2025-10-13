#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/system/get_version.py

"""Get ESP32 firmware version"""

import serial
import time
import sys

def get_version(uart):
    """
    Get firmware version from ESP32

    Args:
        uart: Serial connection

    Returns:
        tuple: (major, minor, patch) or None on failure
    """
    # Create version request packet
    packet = bytes([0xA5, 0x03, 0x00, 0x00])

    print("Requesting firmware version...")
    uart.write(packet)

    # Wait for response (header + status + data_len + 3 version bytes)
    response = uart.read(10)

    if len(response) < 10:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return None

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response header")
        return None

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Status = 0x{status:02X}")
        return None

    data_len = response[5] | (response[6] << 8)
    if data_len != 3:
        print(f"ERROR: Expected 3 version bytes, got {data_len}")
        return None

    # Extract version
    major = response[7]
    minor = response[8]
    patch = response[9]

    return (major, minor, patch)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python get_version.py <serial_port>")
        print("\nExample:")
        print("  python get_version.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Get version
    version = get_version(uart)

    if version:
        major, minor, patch = version
        print(f"[OK] ESP32 Firmware Version: {major}.{minor}.{patch}")
    else:
        print("[FAIL] Failed to get version")
        sys.exit(1)

    uart.close()
