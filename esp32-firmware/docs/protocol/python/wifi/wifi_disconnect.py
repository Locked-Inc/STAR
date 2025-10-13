#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/wifi_disconnect.py

"""Disconnect ESP32 from WiFi"""

import serial
import time
import sys

def wifi_disconnect(uart):
    """
    Disconnect ESP32 from WiFi

    Args:
        uart: Serial connection

    Returns:
        bool: True if disconnect succeeded
    """
    # Create disconnect packet (no payload)
    packet = bytes([0xA5, 0x11, 0x00, 0x00])

    print("Disconnecting from WiFi...")
    uart.write(packet)

    # Wait for response
    response = uart.read(5)

    if len(response) < 5:
        print("ERROR: No response")
        return False

    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response")
        return False

    status = response[4]
    if status == 0x00:
        print("[OK] WiFi disconnected successfully")
        return True
    else:
        print(f"ERROR: Failed to disconnect (status: 0x{status:02X})")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python wifi_disconnect.py <serial_port>")
        print("\nExample:")
        print("  python wifi_disconnect.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Disconnect from WiFi
    success = wifi_disconnect(uart)

    uart.close()

    sys.exit(0 if success else 1)
