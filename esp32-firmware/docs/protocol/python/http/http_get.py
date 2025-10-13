#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/http/http_get.py

"""Send HTTP GET request via ESP32"""

import serial
import time
import sys

def http_get(port, url):
    """Send HTTP GET request"""
    uart = serial.Serial(port, 115200, timeout=10)
    time.sleep(0.1)

    # Prepare URL payload (256 bytes, null-terminated)
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')

    # Send CMD_HTTP_GET (0x20)
    payload_len = len(url_bytes)
    packet = bytes([0xA5, 0x20, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + url_bytes

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    print(f"Sending HTTP GET request to: {url}")
    print("Waiting for response...")

    # Wait for response
    time.sleep(5)
    response = uart.read(4096)

    if len(response) >= 5:
        cmd = response[1]
        status = response[4]

        if status == 0x00:  # SUCCESS
            data_len = response[5] | (response[6] << 8)
            print(f"\n[OK] Success! Received {data_len} bytes")
            print("\nResponse data:")
            print("-" * 60)
            print(response[7:7+data_len].decode('utf-8', errors='replace'))
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
    if len(sys.argv) < 3:
        print("Usage: python http_get.py <serial_port> <url>")
        print("Example: python http_get.py /dev/ttyUSB0 http://httpbin.org/get")
        sys.exit(1)

    http_get(sys.argv[1], sys.argv[2])
