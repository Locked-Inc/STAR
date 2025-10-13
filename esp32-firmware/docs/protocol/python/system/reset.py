#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/system/reset.py

"""Reset ESP32"""

import serial
import time
import sys

def reset_esp32(uart):
    """
    Send RESET command to ESP32

    Args:
        uart: Serial connection

    Returns:
        bool: True if reset command was acknowledged
    """
    # Create reset packet (no payload)
    packet = bytes([0xA5, 0x02, 0x00, 0x00])

    print("Sending RESET command...")
    uart.write(packet)

    # Wait for response
    response = uart.read(5)

    if len(response) < 5:
        print("ERROR: No response")
        return False

    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response")
        return False

    status = response[4]
    if status == 0x00:
        print("[OK] Reset command acknowledged")
        print("  ESP32 will reboot in 1 second...")
        return True
    else:
        print(f"ERROR: Status = 0x{status:02X}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python reset.py <serial_port>")
        print("\nExample:")
        print("  python reset.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Reset ESP32
    success = reset_esp32(uart)

    if success:
        print("\nWaiting for reboot...")
        time.sleep(3)
        print("[OK] ESP32 should be running now")

    uart.close()

    sys.exit(0 if success else 1)
