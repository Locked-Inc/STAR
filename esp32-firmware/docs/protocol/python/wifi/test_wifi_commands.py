#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/test_wifi_commands.py

"""Complete WiFi commands test suite"""

import serial
import time
import sys

def test_wifi_commands(port):
    """Test all WiFi commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("="*60)
    print("ESP32 WiFi Commands Test Suite")
    print("="*60)

    # Test 1: WiFi Status
    print("\n[1/4] Testing CMD_WIFI_STATUS...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    packet = bytes([0xA5, 0x12, 0x00, 0x00])
    uart.write(packet)

    response = uart.read(20)
    if len(response) >= 13:
        status = response[7]
        status_names = {0: 'DISCONNECTED', 1: 'CONNECTING', 2: 'CONNECTED', 3: 'FAILED'}
        print(f"  [OK] WIFI_STATUS: PASSED (Status: {status_names.get(status, 'UNKNOWN')})")
    else:
        print("  [FAIL] WIFI_STATUS: FAILED")

    time.sleep(0.5)

    # Test 2: WiFi Scan
    print("\n[2/4] Testing CMD_WIFI_SCAN...")
    print("  (This may take 10-15 seconds...)")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    packet = bytes([0xA5, 0x13, 0x00, 0x00])
    uart.write(packet)

    time.sleep(12)  # Wait for scan
    response = uart.read(4096)

    if len(response) >= 7 and response[4] == 0x00:
        data_len = response[5] | (response[6] << 8)
        num_networks = data_len // 36
        print(f"  [OK] WIFI_SCAN: PASSED (Found {num_networks} networks)")
    else:
        print("  [FAIL] WIFI_SCAN: FAILED")

    time.sleep(0.5)

    # Test 3: WiFi Connect (optional)
    print("\n[3/4] Testing CMD_WIFI_CONNECT...")
    do_connect = input("  Test WiFi connection? Enter SSID (or press Enter to skip): ")

    if do_connect:
        password = input("  Enter password: ")

        uart.reset_input_buffer()
        uart.reset_output_buffer()

        # Prepare payload
        ssid_bytes = do_connect.encode('utf-8')[:31] + b'\x00'
        ssid_bytes = ssid_bytes.ljust(32, b'\x00')

        password_bytes = password.encode('utf-8')[:63] + b'\x00'
        password_bytes = password_bytes.ljust(64, b'\x00')

        payload = ssid_bytes + password_bytes

        packet = bytes([0xA5, 0x10, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
        uart.write(packet)

        response = uart.read(5)
        if len(response) >= 5 and response[4] == 0x00:
            print("  [OK] WIFI_CONNECT: Command accepted, waiting for connection...")

            # Poll status
            for i in range(20):  # 10 seconds
                time.sleep(0.5)
                uart.reset_input_buffer()
                uart.reset_output_buffer()

                status_packet = bytes([0xA5, 0x12, 0x00, 0x00])
                uart.write(status_packet)
                status_response = uart.read(20)

                if len(status_response) >= 8:
                    wifi_status = status_response[7]
                    if wifi_status == 2:  # CONNECTED
                        print("  [OK] WIFI_CONNECT: PASSED (Connected)")
                        break
                    elif wifi_status == 3:  # FAILED
                        print("  [FAIL] WIFI_CONNECT: FAILED (Connection failed)")
                        break
            else:
                print("  [WARN] WIFI_CONNECT: TIMEOUT (Still connecting)")
        else:
            print("  [FAIL] WIFI_CONNECT: FAILED")
    else:
        print("  [SKIP] WIFI_CONNECT: SKIPPED")

    time.sleep(0.5)

    # Test 4: WiFi Disconnect (optional)
    print("\n[4/4] Testing CMD_WIFI_DISCONNECT...")
    do_disconnect = input("  Disconnect from WiFi? (y/N): ")

    if do_disconnect.lower() == 'y':
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        packet = bytes([0xA5, 0x11, 0x00, 0x00])
        uart.write(packet)

        response = uart.read(5)
        if len(response) >= 5 and response[4] == 0x00:
            print("  [OK] WIFI_DISCONNECT: PASSED")
        else:
            print("  [FAIL] WIFI_DISCONNECT: FAILED")
    else:
        print("  [SKIP] WIFI_DISCONNECT: SKIPPED")

    uart.close()

    print("\n" + "="*60)
    print("Test suite complete!")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_wifi_commands.py <serial_port>")
        sys.exit(1)

    test_wifi_commands(sys.argv[1])
