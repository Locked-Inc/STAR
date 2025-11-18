#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/udp/udp_send.py

"""UDP send via ESP32

This script demonstrates sending UDP datagrams through the ESP32.
Perfect for:
- Sensor data streaming
- Telemetry in noisy environments
- Low-latency communication
- Fire-and-forget messaging
"""

import serial
import time
import sys
import struct

def udp_send(port, host, udp_port, data):
    """Send UDP datagram via ESP32"""
    uart = serial.Serial(port, 115200, timeout=10)
    time.sleep(0.1)

    print(f"UDP Send to {host}:{udp_port}")
    print("=" * 60)

    # Step 1: Create UDP socket
    print("\n[1/3] Creating UDP socket...")
    local_port = 0  # Auto-assign
    payload = struct.pack('<H', local_port)

    packet = bytes([0xA5, 0x25, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) < 8 or response[4] != 0x00:
        print("[FAIL] Failed to create UDP socket")
        if len(response) >= 5 and response[4] == 0x10:
            print("  WiFi not connected")
        uart.close()
        return

    socket_id = response[7]
    assigned_port = struct.unpack('<H', response[8:10])[0]
    print(f"[OK] Socket created! Socket ID: {socket_id}, Local port: {assigned_port}")

    # Step 2: Send UDP datagram
    print(f"\n[2/3] Sending UDP datagram ({len(data)} bytes)...")
    print(f"  Data: {data}")

    # Build payload: [socket_id(1)] [host(128)] [port(2)] [data_len(2)] [data]
    host_bytes = host.encode('utf-8')[:127] + b'\x00'
    host_bytes = host_bytes.ljust(128, b'\x00')

    data_bytes = data.encode('utf-8')
    data_len = len(data_bytes)

    payload = struct.pack('<B', socket_id) + host_bytes + struct.pack('<HH', udp_port, data_len) + data_bytes

    packet = bytes([0xA5, 0x26, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print(f"[OK] Datagram sent successfully!")
    else:
        print("[FAIL] Failed to send datagram")

    # Step 3: Close UDP socket
    print(f"\n[3/3] Closing UDP socket...")
    payload = struct.pack('<B', socket_id)
    packet = bytes([0xA5, 0x27, 0x01, 0x00]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(0.5)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print("[OK] Socket closed")
    else:
        print("[FAIL] Failed to close socket")

    uart.close()

    print("\n" + "=" * 60)
    print("UDP send complete!")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python udp_send.py <serial_port> <host> <port> [data]")
        print("Example: python udp_send.py /dev/ttyUSB0 192.168.1.100 8080 'sensor:temp=25.5'")
        print("\nIdeal for:")
        print("  - Sensor data streaming")
        print("  - Telemetry in noisy environments")
        print("  - Real-time updates")
        sys.exit(1)

    data = sys.argv[4] if len(sys.argv) > 4 else "UDP test data"
    udp_send(sys.argv[1], sys.argv[2], int(sys.argv[3]), data)
