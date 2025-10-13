#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/http/test_http_commands.py

"""Complete HTTP commands test suite"""

import serial
import time
import sys
import struct

def test_http_commands(port):
    """Test all HTTP commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("="*60)
    print("ESP32 HTTP Commands Test Suite")
    print("="*60)

    # Test 1: HTTP GET with small response
    print("\n[1/4] Testing CMD_HTTP_GET (Small response)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Use httpbin.org for testing
    url = "http://httpbin.org/status/200"
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')

    payload_len = len(url_bytes)
    packet = bytes([0xA5, 0x20, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + url_bytes
    uart.write(packet)

    time.sleep(3)  # Wait for HTTP request
    response = uart.read(2048)

    if len(response) >= 5 and response[4] == 0x00:  # k_status_ok
        data_len = response[5] | (response[6] << 8)
        print(f"  [OK] HTTP_GET: PASSED (Received {data_len} bytes)")
        if data_len > 0:
            print(f"    Response preview: {response[7:min(7+50, len(response))]}")
    elif len(response) >= 5 and response[4] == 0x10:  # k_status_wifi_failed
        print("  [FAIL] HTTP_GET: FAILED (WiFi not connected)")
    elif len(response) >= 5 and response[4] == 0x20:  # k_status_http_failed
        print("  [FAIL] HTTP_GET: FAILED (HTTP request failed)")
    else:
        print("  [FAIL] HTTP_GET: FAILED (No response or invalid)")

    time.sleep(0.5)

    # Test 2: HTTP GET with JSON response
    print("\n[2/4] Testing CMD_HTTP_GET (JSON response)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    url = "http://httpbin.org/json"
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')

    payload_len = len(url_bytes)
    packet = bytes([0xA5, 0x20, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + url_bytes
    uart.write(packet)

    time.sleep(3)
    response = uart.read(2048)

    if len(response) >= 5 and response[4] == 0x00:
        data_len = response[5] | (response[6] << 8)
        print(f"  [OK] HTTP_GET (JSON): PASSED (Received {data_len} bytes)")
        # Check if response contains JSON
        if b'{' in response[7:]:
            print("    Response appears to be JSON [OK]")
    else:
        print("  [FAIL] HTTP_GET (JSON): FAILED")

    time.sleep(0.5)

    # Test 3: HTTP POST with data
    print("\n[3/4] Testing CMD_HTTP_POST...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    url = "http://httpbin.org/post"
    post_data = '{"test":"data","value":123}'

    url_bytes = url.encode('utf-8')
    data_bytes = post_data.encode('utf-8')

    # Create HTTP POST payload: url_len(2) + data_len(2) + url + data
    url_len = len(url_bytes)
    data_len = len(data_bytes)

    payload = struct.pack('<HH', url_len, data_len) + url_bytes + data_bytes
    payload_len = len(payload)

    packet = bytes([0xA5, 0x21, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(3)
    response = uart.read(2048)

    if len(response) >= 5 and response[4] == 0x00:
        resp_data_len = response[5] | (response[6] << 8)
        print(f"  [OK] HTTP_POST: PASSED (Received {resp_data_len} bytes)")
        if resp_data_len > 0:
            print(f"    Response preview: {response[7:min(7+50, len(response))]}")
    elif len(response) >= 5 and response[4] == 0x10:
        print("  [FAIL] HTTP_POST: FAILED (WiFi not connected)")
    elif len(response) >= 5 and response[4] == 0x20:
        print("  [FAIL] HTTP_POST: FAILED (HTTP request failed)")
    else:
        print("  [FAIL] HTTP_POST: FAILED")

    time.sleep(0.5)

    # Test 4: HTTP GET error handling (invalid URL)
    print("\n[4/4] Testing CMD_HTTP_GET (Error handling)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    url = "http://invalid.domain.that.does.not.exist.example"
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')

    payload_len = len(url_bytes)
    packet = bytes([0xA5, 0x20, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + url_bytes
    uart.write(packet)

    time.sleep(5)
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x20:  # k_status_http_failed
        print("  [OK] HTTP_GET (Error): PASSED (Correctly reported HTTP failure)")
    elif len(response) >= 5 and response[4] == 0x00:
        print("  [WARN] HTTP_GET (Error): Got OK status for invalid URL (unexpected)")
    else:
        print("  [FAIL] HTTP_GET (Error): FAILED (No proper error response)")

    uart.close()

    print("\n" + "="*60)
    print("HTTP test suite complete!")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_http_commands.py <serial_port>")
        print("Example: python test_http_commands.py /dev/ttyUSB0")
        print("\nNote: ESP32 must be connected to WiFi for HTTP tests to work")
        sys.exit(1)

    test_http_commands(sys.argv[1])
