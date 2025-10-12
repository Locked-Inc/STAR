#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_wifi_scan_simple.py

"""
Simplified WiFi scan test with better debugging
"""

import serial
import sys
import time

SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

START_MARKER = 0xA5
CMD_WIFI_SCAN = 0x13

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        START_MARKER,
        cmd,
        payload_len & 0xFF,
        (payload_len >> 8) & 0xFF
    ])
    packet += payload
    return packet

def main():
    print(f"Opening {SERIAL_PORT}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(0.5)

    # Clear buffers
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    print("Sending WIFI_SCAN command...")
    scan_packet = create_packet(CMD_WIFI_SCAN)
    print(f"Packet: {scan_packet.hex()}")
    ser.write(scan_packet)

    print("Waiting for response (this may take up to 10 seconds)...")

    # Read data as it arrives
    start_time = time.time()
    all_data = b''

    while time.time() - start_time < 15:
        if ser.in_waiting > 0:
            chunk = ser.read(ser.in_waiting)
            all_data += chunk
            print(f"  Received {len(chunk)} bytes (total: {len(all_data)} bytes)")
        time.sleep(0.1)

    ser.close()

    if all_data:
        print(f"\n=== Received Data ===")
        print(f"Total: {len(all_data)} bytes")
        print(f"Hex: {all_data.hex()}")

        # Try to parse
        if len(all_data) >= 4 and all_data[0] == START_MARKER:
            cmd = all_data[1]
            payload_len = all_data[2] | (all_data[3] << 8)
            print(f"\nParsed:")
            print(f"  Start: 0x{all_data[0]:02x}")
            print(f"  Command: 0x{cmd:02x}")
            print(f"  Payload length: {payload_len}")
            if len(all_data) >= 4 + payload_len:
                print(f"  Complete packet received")
            else:
                print(f"  Incomplete: need {4+payload_len} bytes, got {len(all_data)}")
    else:
        print("\nNo data received!")

if __name__ == '__main__':
    main()
