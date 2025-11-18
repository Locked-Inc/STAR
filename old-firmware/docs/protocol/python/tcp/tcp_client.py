#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/tcp/tcp_client.py

"""TCP client via ESP32"""

import serial
import time
import sys
import struct

def tcp_client(port, host, tcp_port, data):
    """Connect via TCP and send data"""
    uart = serial.Serial(port, 115200, timeout=10)
    time.sleep(0.1)

    print(f"Connecting to {host}:{tcp_port}...")

    # Step 1: TCP Connect
    host_bytes = host.encode('utf-8')[:127] + b'\x00'
    host_bytes = host_bytes.ljust(128, b'\x00')
    payload = host_bytes + struct.pack('<H', tcp_port)
    payload_len = len(payload)

    packet = bytes([0xA5, 0x22, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(3)
    response = uart.read(100)

    if len(response) < 8 or response[4] != 0x00:
        print("[FAIL] Failed to connect")
        if len(response) >= 5 and response[4] == 0x10:
            print("  WiFi not connected")
        uart.close()
        return

    socket_id = response[7]
    print(f"[OK] Connected! Socket ID: {socket_id}")

    # Step 2: TCP Send
    if data:
        print(f"\nSending data ({len(data)} bytes): {data}")

        data_bytes = data.encode('utf-8')
        payload = struct.pack('<BH', socket_id, len(data_bytes)) + data_bytes
        payload_len = len(payload)

        packet = bytes([0xA5, 0x23, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload

        uart.reset_input_buffer()
        uart.reset_output_buffer()
        uart.write(packet)

        time.sleep(2)
        response = uart.read(4096)

        if len(response) >= 5 and response[4] == 0x00:
            resp_data_len = response[5] | (response[6] << 8)
            print(f"[OK] Data sent successfully")
            if resp_data_len > 0:
                print(f"\nReceived {resp_data_len} bytes:")
                print("-" * 60)
                print(response[7:7+resp_data_len].decode('utf-8', errors='replace'))
                print("-" * 60)
            else:
                print("  (No immediate response from server)")
        else:
            print("[FAIL] Failed to send data")

    # Step 3: TCP Close
    print(f"\nClosing connection...")
    payload = struct.pack('<B', socket_id)
    packet = bytes([0xA5, 0x24, 0x01, 0x00]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(0.5)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        print("[OK] Connection closed")
    else:
        print("[FAIL] Failed to close connection")

    uart.close()

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python tcp_client.py <serial_port> <host> <port> [data]")
        print("Example: python tcp_client.py /dev/ttyUSB0 httpbin.org 80 'GET / HTTP/1.1\\r\\nHost: httpbin.org\\r\\n\\r\\n'")
        sys.exit(1)

    data = sys.argv[4] if len(sys.argv) > 4 else ""
    tcp_client(sys.argv[1], sys.argv[2], int(sys.argv[3]), data)
