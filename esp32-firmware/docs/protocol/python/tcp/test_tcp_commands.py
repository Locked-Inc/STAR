#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/tcp/test_tcp_commands.py

"""Complete TCP commands test suite"""

import serial
import time
import sys
import struct

def test_tcp_commands(port):
    """Test all TCP commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("="*60)
    print("ESP32 TCP Commands Test Suite")
    print("="*60)

    socket_id = None

    # Test 1: TCP Connect
    print("\n[1/5] Testing CMD_TCP_CONNECT...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Connect to httpbin.org on port 80
    host = "httpbin.org"
    port_num = 80

    host_bytes = host.encode('utf-8')[:127] + b'\x00'
    host_bytes = host_bytes.ljust(128, b'\x00')

    payload = host_bytes + struct.pack('<H', port_num)
    payload_len = len(payload)

    packet = bytes([0xA5, 0x22, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(3)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:  # k_status_ok
        if len(response) >= 8:
            socket_id = response[7]
            print(f"  [OK] TCP_CONNECT: PASSED (Socket ID: {socket_id})")
        else:
            print("  [FAIL] TCP_CONNECT: Response too short")
    elif len(response) >= 5 and response[4] == 0x10:  # k_status_wifi_failed
        print("  [FAIL] TCP_CONNECT: FAILED (WiFi not connected)")
    else:
        print("  [FAIL] TCP_CONNECT: FAILED")

    time.sleep(0.5)

    # Test 2: TCP Send and Receive (HTTP request)
    if socket_id is not None:
        print(f"\n[2/5] Testing CMD_TCP_SEND (Socket {socket_id})...")
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        # Send a simple HTTP GET request
        http_request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n"
        request_bytes = http_request.encode('utf-8')

        # Create TCP send payload: socket_id(1) + data_len(2) + data
        data_len = len(request_bytes)
        payload = struct.pack('<BH', socket_id, data_len) + request_bytes
        payload_len = len(payload)

        packet = bytes([0xA5, 0x23, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        uart.write(packet)

        time.sleep(2)
        response = uart.read(2048)

        if len(response) >= 5 and response[4] == 0x00:
            resp_data_len = response[5] | (response[6] << 8)
            print(f"  [OK] TCP_SEND: PASSED (Sent, received {resp_data_len} bytes)")
            if resp_data_len > 0:
                # Check if we got HTTP response
                resp_data = response[7:7+resp_data_len]
                if b'HTTP/' in resp_data:
                    print("    Response contains HTTP header [OK]")
                print(f"    Response preview: {resp_data[:min(80, len(resp_data))]}")
        else:
            print("  [FAIL] TCP_SEND: FAILED")
    else:
        print("\n[2/5] TCP_SEND: SKIPPED (no connection)")

    time.sleep(0.5)

    # Test 3: TCP Send without immediate response
    if socket_id is not None:
        print(f"\n[3/5] Testing CMD_TCP_SEND (No immediate data)...")
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        # Send partial request (server won't respond until complete)
        partial_request = "GET /delay/"
        request_bytes = partial_request.encode('utf-8')

        data_len = len(request_bytes)
        payload = struct.pack('<BH', socket_id, data_len) + request_bytes
        payload_len = len(payload)

        packet = bytes([0xA5, 0x23, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        uart.write(packet)

        time.sleep(1)
        response = uart.read(100)

        if len(response) >= 5 and response[4] == 0x00:
            resp_data_len = response[5] | (response[6] << 8)
            if resp_data_len == 0:
                print("  [OK] TCP_SEND (No data): PASSED (Send acknowledged, no response)")
            else:
                print(f"  [WARN] TCP_SEND (No data): Got {resp_data_len} bytes (unexpected)")
        else:
            print("  [FAIL] TCP_SEND (No data): FAILED")
    else:
        print("\n[3/5] TCP_SEND (No data): SKIPPED (no connection)")

    time.sleep(0.5)

    # Test 4: TCP Close
    if socket_id is not None:
        print(f"\n[4/5] Testing CMD_TCP_CLOSE (Socket {socket_id})...")
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        payload = struct.pack('<B', socket_id)
        payload_len = len(payload)

        packet = bytes([0xA5, 0x24, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        uart.write(packet)

        time.sleep(0.5)
        response = uart.read(100)

        if len(response) >= 5 and response[4] == 0x00:
            print(f"  [OK] TCP_CLOSE: PASSED (Socket {socket_id} closed)")
            socket_id = None  # Mark as closed
        else:
            print("  [FAIL] TCP_CLOSE: FAILED")
    else:
        print("\n[4/5] TCP_CLOSE: SKIPPED (no connection)")

    time.sleep(0.5)

    # Test 5: Multiple concurrent connections
    print("\n[5/5] Testing multiple TCP connections...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    sockets = []
    test_hosts = [
        ("httpbin.org", 80),
        ("example.com", 80),
    ]

    for i, (host, port_num) in enumerate(test_hosts):
        host_bytes = host.encode('utf-8')[:127] + b'\x00'
        host_bytes = host_bytes.ljust(128, b'\x00')

        payload = host_bytes + struct.pack('<H', port_num)
        payload_len = len(payload)

        packet = bytes([0xA5, 0x22, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        uart.write(packet)

        time.sleep(2)
        response = uart.read(100)

        if len(response) >= 8 and response[4] == 0x00:
            sid = response[7]
            sockets.append(sid)
            print(f"  Connection {i+1} to {host}: Socket ID {sid} [OK]")
        else:
            print(f"  Connection {i+1} to {host}: FAILED [FAIL]")

        time.sleep(0.5)

    if len(sockets) >= 2:
        print(f"  [OK] Multiple connections: PASSED (Opened {len(sockets)} sockets)")
    elif len(sockets) == 1:
        print(f"  [WARN] Multiple connections: Partial (Only 1 socket opened)")
    else:
        print(f"  [FAIL] Multiple connections: FAILED")

    # Clean up - close all sockets
    for sid in sockets:
        uart.reset_input_buffer()
        uart.reset_output_buffer()
        payload = struct.pack('<B', sid)
        packet = bytes([0xA5, 0x24, 0x01, 0x00]) + payload
        uart.write(packet)
        time.sleep(0.3)

    uart.close()

    print("\n" + "="*60)
    print("TCP test suite complete!")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_tcp_commands.py <serial_port>")
        print("Example: python test_tcp_commands.py /dev/ttyUSB0")
        print("\nNote: ESP32 must be connected to WiFi for TCP tests to work")
        sys.exit(1)

    test_tcp_commands(sys.argv[1])
