#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/http/http_post.py

"""Send HTTP POST request via ESP32"""

import serial
import time
import sys
import struct

def http_post(port, url, data):
    """Send HTTP POST request"""
    uart = serial.Serial(port, 115200, timeout=10)
    time.sleep(0.1)

    # Prepare payload: url_len(2) + data_len(2) + url + data
    url_bytes = url.encode('utf-8')
    data_bytes = data.encode('utf-8')

    url_len = len(url_bytes)
    data_len = len(data_bytes)

    payload = struct.pack('<HH', url_len, data_len) + url_bytes + data_bytes
    payload_len = len(payload)

    # Send CMD_HTTP_POST (0x21)
    packet = bytes([0xA5, 0x21, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    print(f"Sending HTTP POST request to: {url}")
    print(f"POST data ({data_len} bytes): {data}")
    print("Waiting for response...")

    # Wait for response
    time.sleep(5)
    response = uart.read(4096)

    if len(response) >= 5:
        status = response[4]

        if status == 0x00:  # SUCCESS
            resp_data_len = response[5] | (response[6] << 8)
            print(f"\n[OK] Success! Received {resp_data_len} bytes")
            print("\nResponse data:")
            print("-" * 60)
            print(response[7:7+resp_data_len].decode('utf-8', errors='replace'))
            print("-" * 60)
        elif status == 0x10:
            print("\n[FAIL] Failed: WiFi not connected")
        elif status == 0x20:
            print("\n[FAIL] Failed: HTTP request error")
        else:
            print(f"\n[FAIL] Failed: Unknown status 0x{status:02X}")
    else:
        print("\n[FAIL] Failed: No response from ESP32")

    uart.close()

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python http_post.py <serial_port> <url> <data>")
        print('Example: python http_post.py /dev/ttyUSB0 http://httpbin.org/post \'{"key":"value"}\'')
        sys.exit(1)

    http_post(sys.argv[1], sys.argv[2], sys.argv[3])
