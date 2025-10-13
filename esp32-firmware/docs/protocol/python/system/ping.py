#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/system/ping.py

"""Test ESP32 connectivity with PING command"""

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

def ping_esp32(uart, test_data=b'Hello ESP32!'):
    """
    Send PING command and verify echo

    Args:
        uart: Serial connection
        test_data: Data to echo (default: "Hello ESP32!")

    Returns:
        bool: True if ping succeeded and echo matches
    """
    # Create ping packet
    packet = create_packet(0x01, test_data)

    # Send packet
    print(f"Sending PING with {len(test_data)} bytes: {test_data}")
    uart.write(packet)

    # Calculate expected response length
    # Header (4) + Status (1) + Data_len (2) + Echo data
    expected_len = 4 + 1 + 2 + len(test_data)

    # Read response
    response = uart.read(expected_len)

    if len(response) < 7:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return False

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        print(f"ERROR: Invalid response header")
        return False

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Status = 0x{status:02X}")
        return False

    data_len = response[5] | (response[6] << 8)
    echo_data = response[7:7+data_len]

    # Verify echo
    if echo_data == test_data:
        print(f"[OK] PING successful! Echo: {echo_data}")
        return True
    else:
        print(f"ERROR: Echo mismatch")
        print(f"  Sent: {test_data}")
        print(f"  Received: {echo_data}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ping.py <serial_port> [test_data]")
        print("\nExample:")
        print("  python ping.py /dev/ttyUSB0")
        print("  python ping.py /dev/ttyUSB0 'Custom message'")
        sys.exit(1)

    port = sys.argv[1]
    test_data = sys.argv[2].encode() if len(sys.argv) > 2 else b'Hello ESP32!'

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Test ping
    success = ping_esp32(uart, test_data)

    uart.close()

    sys.exit(0 if success else 1)
