#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_uart.py

"""
Test script for ESP32 PYNQ WiFi Bridge UART communication

WIRING GUIDE:
=============
ESP32 is configured to use UART1 on GPIO 17 (TX) and GPIO 16 (RX)
On most ESP32 dev boards these pins are labeled TX2/RX2

Connect USB-UART adapter to ESP32:
  - Adapter RX  → ESP32 TX2 (GPIO 17)
  - Adapter TX  → ESP32 RX2 (GPIO 16)
  - Adapter GND → ESP32 GND

FINDING THE CORRECT /dev/ttyUSB* DEVICE:
========================================
Method 1: Check before and after plugging in adapter
  1. Run: ls /dev/ttyUSB*
  2. Plug in USB-UART adapter
  3. Run: ls /dev/ttyUSB* again
  4. The new device is your adapter

Method 2: Check kernel messages (requires sudo)
  1. Run: sudo dmesg | tail
  2. Look for messages like "ch341-uart converter now attached to ttyUSB0"

Method 3: Use udevadm (shows device info)
  1. Run: udevadm info /dev/ttyUSB0 | grep ID_SERIAL

Note: The ESP32 programming port (USB on the ESP32 board itself) is NOT
the same as the UART1 port we're testing. We need the external adapter
connected to TX2/RX2.

PROTOCOL OVERVIEW:
==================
The ESP32 uses a simple packet-based protocol:
  - Start marker: 0xA5
  - Command byte: identifies the command type
  - Length: 16-bit little-endian payload length
  - Payload: variable length data (0-1024 bytes)

This script sends PING and GET_VERSION commands to verify the ESP32 firmware
is receiving commands, processing them, and sending responses correctly.
"""

import serial
import sys
import time

# Change this to match your USB-UART adapter device
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

# Protocol constants - must match ESP32 firmware definitions
START_MARKER = 0xA5       # Unique byte that marks the start of every packet
CMD_PING = 0x01           # Command to test echo/connectivity
CMD_GET_VERSION = 0x03    # Command to get firmware version
CMD_RESPONSE = 0xF0       # All responses from ESP32 use this command byte

STATUS_OK = 0x00          # Status code indicating success

def create_packet(cmd, payload=b''):
    """
    Create a protocol packet to send to ESP32

    Packet format:
      Byte 0:    Start marker (0xA5)
      Byte 1:    Command byte
      Byte 2-3:  Payload length (16-bit little-endian)
      Byte 4+:   Payload data (if any)

    Args:
        cmd: Command byte (e.g., CMD_PING, CMD_GET_VERSION)
        payload: Optional payload data as bytes

    Returns:
        Complete packet as bytes ready to send over UART
    """
    payload_len = len(payload)
    # Build 4-byte header
    packet = bytes([
        START_MARKER,              # Start marker
        cmd,                       # Command
        payload_len & 0xFF,        # Low byte of length
        (payload_len >> 8) & 0xFF  # High byte of length
    ])
    # Append payload if present
    packet += payload
    return packet

def parse_packet(data):
    """
    Parse a protocol packet received from ESP32

    Validates the packet structure and extracts fields.

    Args:
        data: Raw bytes received from UART

    Returns:
        Dictionary with 'cmd', 'payload_len', 'payload' keys, or None if invalid
    """
    # Check minimum packet size (4-byte header)
    if len(data) < 4:
        return None

    # Verify start marker
    if data[0] != START_MARKER:
        return None

    # Extract command byte
    cmd = data[1]

    # Extract payload length (16-bit little-endian)
    payload_len = data[2] | (data[3] << 8)

    # Verify we have the complete packet
    if len(data) < 4 + payload_len:
        return None

    # Extract payload
    payload = data[4:4+payload_len]

    return {
        'cmd': cmd,
        'payload_len': payload_len,
        'payload': payload
    }

def main():
    """
    Main test function - sends PING and GET_VERSION commands to ESP32

    Test 1 (PING): Verifies basic communication and echo functionality
    Test 2 (GET_VERSION): Verifies firmware version reporting
    """
    print(f"Opening {SERIAL_PORT} at {BAUD_RATE} baud...")

    try:
        # Open serial port
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
        time.sleep(0.5)  # Give ESP32 time to boot if just powered on

        # Clear any existing data in buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # ========== Test 1: PING Command ==========
        print("\n=== Test 1: PING Command ===")
        # Create PING packet with test payload "Hello ESP32"
        ping_packet = create_packet(CMD_PING, b'Hello ESP32')
        print(f"Sending PING packet: {ping_packet.hex()}")
        ser.write(ping_packet)

        # Wait for ESP32 to process and respond
        time.sleep(0.1)
        response = ser.read(256)

        if response:
            print(f"Received {len(response)} bytes: {response.hex()}")
            packet = parse_packet(response)
            if packet:
                print(f"  Command: 0x{packet['cmd']:02X}")
                print(f"  Payload length: {packet['payload_len']}")
                if packet['payload']:
                    print(f"  Payload: {packet['payload']}")
                    # Response format: [status_byte, original_payload...]
                    if packet['cmd'] == CMD_RESPONSE and len(packet['payload']) > 0:
                        status = packet['payload'][0]
                        print(f"  Status: 0x{status:02X} {'(OK)' if status == STATUS_OK else '(ERROR)'}")
                        # PING echoes back the original data after status byte
                        if len(packet['payload']) > 1:
                            print(f"  Echo data: {packet['payload'][1:]}")
            else:
                print("  Failed to parse response")
        else:
            print("No response received")

        # ========== Test 2: GET_VERSION Command ==========
        print("\n=== Test 2: GET_VERSION Command ===")
        # Create GET_VERSION packet (no payload needed)
        version_packet = create_packet(CMD_GET_VERSION)
        print(f"Sending GET_VERSION packet: {version_packet.hex()}")
        ser.reset_input_buffer()  # Clear any leftover data
        ser.write(version_packet)

        # Wait for ESP32 to process and respond
        time.sleep(0.1)
        response = ser.read(256)

        if response:
            print(f"Received {len(response)} bytes: {response.hex()}")
            packet = parse_packet(response)
            if packet:
                print(f"  Command: 0x{packet['cmd']:02X}")
                print(f"  Payload length: {packet['payload_len']}")
                # Response format: [status_byte, major, minor, patch]
                if packet['payload'] and len(packet['payload']) >= 4:
                    status = packet['payload'][0]
                    print(f"  Status: 0x{status:02X} {'(OK)' if status == STATUS_OK else '(ERROR)'}")
                    if len(packet['payload']) >= 4:
                        major = packet['payload'][1]
                        minor = packet['payload'][2]
                        patch = packet['payload'][3]
                        print(f"  Firmware version: {major}.{minor}.{patch}")
            else:
                print("  Failed to parse response")
        else:
            print("No response received")

        # Clean up
        ser.close()
        print("\nTest complete!")

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
