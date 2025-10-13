#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/udp/test_udp_commands.py

"""Complete UDP commands test suite

Tests all UDP operations:
- Socket creation with port binding
- Datagram transmission
- Socket cleanup
"""

import serial
import time
import sys
import struct

def test_udp_commands(port):
    """Test all UDP commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("=" * 60)
    print("ESP32 UDP Commands Test Suite")
    print("=" * 60)

    # Test 1: Create UDP socket with auto-assigned port
    print("\n[1/6] Testing CMD_UDP_CREATE (auto port)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    local_port = 0  # Auto-assign
    payload = struct.pack('<H', local_port)
    packet = bytes([0xA5, 0x25, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) >= 10 and response[4] == 0x00:
        socket_id = response[7]
        assigned_port = struct.unpack('<H', response[8:10])[0]
        print(f"  [OK] UDP_CREATE: PASSED (Socket ID: {socket_id}, Port: {assigned_port})")
    else:
        print("  [FAIL] UDP_CREATE: FAILED")
        uart.close()
        return

    time.sleep(0.5)

    # Test 2: Create UDP socket with specific port
    print("\n[2/6] Testing CMD_UDP_CREATE (specific port)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    specific_port = 9999
    payload = struct.pack('<H', specific_port)
    packet = bytes([0xA5, 0x25, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) >= 10 and response[4] == 0x00:
        socket_id_2 = response[7]
        bound_port = struct.unpack('<H', response[8:10])[0]
        print(f"  [OK] UDP_CREATE: PASSED (Socket ID: {socket_id_2}, Port: {bound_port})")
    else:
        print("  [FAIL] UDP_CREATE: FAILED")

    time.sleep(0.5)

    # Test 3: Send UDP datagram to IP address
    print("\n[3/6] Testing CMD_UDP_SEND (to IP)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    host = "8.8.8.8"  # Google DNS
    dest_port = 53
    data = b"UDP_TEST_DATA"

    host_bytes = host.encode('utf-8')[:127] + b'\x00'
    host_bytes = host_bytes.ljust(128, b'\x00')

    payload = struct.pack('<B', socket_id) + host_bytes + struct.pack('<HH', dest_port, len(data)) + data
    packet = bytes([0xA5, 0x26, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print(f"  [OK] UDP_SEND: PASSED (Sent {len(data)} bytes to {host}:{dest_port})")
    else:
        print("  [FAIL] UDP_SEND: FAILED")

    time.sleep(0.5)

    # Test 4: Send UDP broadcast
    print("\n[4/6] Testing CMD_UDP_SEND (broadcast)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    broadcast = "255.255.255.255"
    bcast_port = 12345
    bcast_data = b"BROADCAST_MSG"

    host_bytes = broadcast.encode('utf-8')[:127] + b'\x00'
    host_bytes = host_bytes.ljust(128, b'\x00')

    payload = struct.pack('<B', socket_id) + host_bytes + struct.pack('<HH', bcast_port, len(bcast_data)) + bcast_data
    packet = bytes([0xA5, 0x26, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(1)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print(f"  [OK] UDP_SEND: PASSED (Broadcast sent)")
    else:
        print("  [FAIL] UDP_SEND: FAILED")

    time.sleep(0.5)

    # Test 5: Close first UDP socket
    print("\n[5/6] Testing CMD_UDP_CLOSE...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    payload = struct.pack('<B', socket_id)
    packet = bytes([0xA5, 0x27, 0x01, 0x00]) + payload
    uart.write(packet)

    time.sleep(0.5)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print(f"  [OK] UDP_CLOSE: PASSED (Socket {socket_id} closed)")
    else:
        print("  [FAIL] UDP_CLOSE: FAILED")

    time.sleep(0.5)

    # Test 6: Close second UDP socket
    print("\n[6/6] Testing CMD_UDP_CLOSE (second socket)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    payload = struct.pack('<B', socket_id_2)
    packet = bytes([0xA5, 0x27, 0x01, 0x00]) + payload
    uart.write(packet)

    time.sleep(0.5)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print(f"  [OK] UDP_CLOSE: PASSED (Socket {socket_id_2} closed)")
    else:
        print("  [FAIL] UDP_CLOSE: FAILED")

    uart.close()

    print("\n" + "=" * 60)
    print("Test suite complete!")
    print("=" * 60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_udp_commands.py <serial_port>")
        print("Example: python test_udp_commands.py /dev/ttyUSB0")
        print("\nThis tests all UDP protocol commands.")
        print("Note: ESP32 must be connected to WiFi for these tests.")
        sys.exit(1)

    test_udp_commands(sys.argv[1])
