#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/read_serial.py

"""
Simple script to read and decode serial output from ESP32
"""

import serial
import sys

port = '/dev/ttyUSB1'
baudrate = 115200

try:
    ser = serial.Serial(port, baudrate, timeout=1)
    print(f"Connected to {port} at {baudrate} baud")
    print("=" * 80)

    while True:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8', errors='replace').rstrip()
                print(line)
            except Exception as e:
                print(f"Decode error: {e}")

except KeyboardInterrupt:
    print("\nExiting...")
except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
finally:
    if 'ser' in locals():
        ser.close()
