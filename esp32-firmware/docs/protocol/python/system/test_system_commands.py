#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/system/test_system_commands.py

"""Complete system commands test suite"""

import serial
import time
import sys

def test_system_commands(port):
    """Test all system commands"""
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    print("="*60)
    print("ESP32 System Commands Test Suite")
    print("="*60)

    # Test 1: PING
    print("\n[1/3] Testing CMD_PING...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    test_data = b"PING TEST DATA"
    packet = bytes([0xA5, 0x01, len(test_data), 0x00]) + test_data
    uart.write(packet)

    response = uart.read(100)
    if len(response) >= 7 and response[7:7+len(test_data)] == test_data:
        print("  [OK] PING: PASSED")
    else:
        print("  [FAIL] PING: FAILED")

    time.sleep(0.5)

    # Test 2: GET_VERSION
    print("\n[2/3] Testing CMD_GET_VERSION...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    packet = bytes([0xA5, 0x03, 0x00, 0x00])
    uart.write(packet)

    response = uart.read(10)
    if len(response) >= 10:
        version = f"{response[7]}.{response[8]}.{response[9]}"
        print(f"  [OK] GET_VERSION: PASSED (v{version})")
    else:
        print("  [FAIL] GET_VERSION: FAILED")

    time.sleep(0.5)

    # Test 3: RESET (optional - ask user)
    print("\n[3/3] Testing CMD_RESET...")
    response = input("  Reset ESP32? This will reboot the device. (y/N): ")

    if response.lower() == 'y':
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        packet = bytes([0xA5, 0x02, 0x00, 0x00])
        uart.write(packet)

        response = uart.read(5)
        if len(response) >= 5 and response[4] == 0x00:
            print("  [OK] RESET: PASSED (ESP32 rebooting...)")
        else:
            print("  [FAIL] RESET: FAILED")
    else:
        print("  [SKIP] RESET: SKIPPED")

    uart.close()

    print("\n" + "="*60)
    print("Test suite complete!")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_system_commands.py <serial_port>")
        sys.exit(1)

    test_system_commands(sys.argv[1])
