#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/ota/ota_check.py

"""Check for OTA updates"""

import serial
import time
import sys

def ota_check(port):
    """Check if OTA update is available"""
    uart = serial.Serial(port, 115200, timeout=10)
    time.sleep(0.1)

    print("Checking for OTA updates...")

    # Send CMD_OTA_CHECK_UPDATE (0x28)
    packet = bytes([0xA5, 0x28, 0x00, 0x00])

    uart.reset_input_buffer()
    uart.reset_output_buffer()
    uart.write(packet)

    time.sleep(5)  # Version check may take time
    response = uart.read(100)

    if len(response) >= 5:
        status = response[4]

        if status == 0x00 and len(response) >= 14:  # SUCCESS
            update_available = response[7]
            current_version = f"{response[8]}.{response[9]}.{response[10]}"
            new_version = f"{response[11]}.{response[12]}.{response[13]}"

            print("\n" + "="*60)
            print("OTA UPDATE CHECK RESULTS")
            print("="*60)
            print(f"Current firmware version: {current_version}")
            print(f"Latest firmware version:  {new_version}")
            print(f"Update available:         {'Yes [OK]' if update_available else 'No [FAIL]'}")
            print("="*60)

            if update_available:
                print("\nA firmware update is available!")
                print("Use ota_update.py to perform the update.")
        elif status == 0x10:
            print("\n[FAIL] Failed: WiFi not connected")
        else:
            print(f"\n[FAIL] Failed: Status 0x{status:02X}")
    else:
        print("\n[FAIL] Failed: No response from ESP32")

    uart.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ota_check.py <serial_port>")
        print("Example: python ota_check.py /dev/ttyUSB0")
        sys.exit(1)

    ota_check(sys.argv[1])
