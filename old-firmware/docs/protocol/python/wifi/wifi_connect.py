#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/wifi_connect.py

"""Connect ESP32 to WiFi network"""

import serial
import struct
import time
import sys

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        0xA5,  # Start marker
        cmd,   # Command ID
        payload_len & 0xFF,          # Len_Lo
        (payload_len >> 8) & 0xFF    # Len_Hi
    ])
    packet += payload
    return packet

def wifi_connect(uart, ssid, password, timeout=10):
    """
    Connect ESP32 to WiFi network

    Args:
        uart: Serial connection
        ssid: WiFi SSID (max 31 chars)
        password: WiFi password (max 63 chars)
        timeout: Connection timeout in seconds

    Returns:
        bool: True if connection succeeded
    """
    # Prepare payload (SSID: 32 bytes, Password: 64 bytes)
    ssid_bytes = ssid.encode('utf-8')[:31] + b'\x00'
    ssid_bytes = ssid_bytes.ljust(32, b'\x00')

    password_bytes = password.encode('utf-8')[:63] + b'\x00'
    password_bytes = password_bytes.ljust(64, b'\x00')

    payload = ssid_bytes + password_bytes

    # Create and send packet
    packet = create_packet(0x10, payload)

    print(f"Connecting to WiFi: {ssid}")
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
    if status != 0x00:
        print(f"ERROR: Failed to start connection (status: 0x{status:02X})")
        return False

    print("[OK] Connection started, waiting for WiFi to connect...")

    # Poll WiFi status until connected or timeout
    start_time = time.time()
    while time.time() - start_time < timeout:
        # Get WiFi status
        status_packet = create_packet(0x12, b'')
        uart.write(status_packet)

        status_response = uart.read(20)
        if len(status_response) >= 8:
            wifi_status = status_response[7]

            if wifi_status == 2:  # CONNECTED
                print("[OK] WiFi connected successfully!")
                return True
            elif wifi_status == 3:  # FAILED
                print("[FAIL] WiFi connection failed")
                return False

        time.sleep(0.5)

    print("[FAIL] Connection timeout")
    return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python wifi_connect.py <serial_port> <ssid> <password> [timeout]")
        print("\nExample:")
        print("  python wifi_connect.py /dev/ttyUSB0 'MyWiFi' 'password123'")
        print("  python wifi_connect.py /dev/ttyUSB0 'MyWiFi' 'password123' 15")
        sys.exit(1)

    port = sys.argv[1]
    ssid = sys.argv[2]
    password = sys.argv[3]
    timeout = int(sys.argv[4]) if len(sys.argv) > 4 else 10

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Connect to WiFi
    success = wifi_connect(uart, ssid, password, timeout)

    uart.close()

    sys.exit(0 if success else 1)
