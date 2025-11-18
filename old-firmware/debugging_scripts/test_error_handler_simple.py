#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_error_handler_simple.py

"""
Simple error handler test - sends invalid WiFi connect command.
Run this while monitoring ESP32 logs on /dev/ttyUSB1 to see error handler in action.
"""

import serial
import sys
import time

SERIAL_PORT = '/dev/ttyUSB0'  # USB-UART adapter on TX2/RX2
BAUD_RATE = 115200

START_MARKER = 0xA5
CMD_WIFI_CONNECT = 0x11

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        START_MARKER,
        cmd,
        payload_len & 0xFF,
        (payload_len >> 8) & 0xFF
    ])
    packet += payload
    return packet

def main():
    print("=== Error Handler Test (Simple) ===")
    print("\nIMPORTANT: Before running this test, open another terminal and run:")
    print("  cd /home/bsikar/Documents/git/STAR/esp32-firmware")
    print("  . ~/esp/esp-idf/export.sh && idf.py -p /dev/ttyUSB1 monitor")
    print("\nThis will let you see the error handler logs with retry attempts.\n")

    input("Press Enter when you have the monitor running in another terminal...")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(0.5)

        # Clear buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # Create payload with fake WiFi credentials
        fake_ssid = b'FAKE_NETWORK_TESTING\x00'
        fake_password = b'wrong_password\x00'
        payload = fake_ssid + fake_password

        print(f"\nSending WIFI_CONNECT command with fake credentials:")
        print(f"  SSID: {fake_ssid.decode('utf-8', errors='ignore').strip(chr(0))}")
        print(f"  Password: {fake_password.decode('utf-8', errors='ignore').strip(chr(0))}")

        # Send WIFI_CONNECT command
        connect_packet = create_packet(CMD_WIFI_CONNECT, payload)
        ser.write(connect_packet)

        print("\n[OK] Command sent!")
        print("\nNow watch the other terminal for error handler logs.")
        print("You should see:")
        print("  - 'Error Recorded' messages with retry counts (0/5, 1/5, 2/5...)")
        print("  - 'Backoff: Next retry delay' messages (1000ms, 2000ms, 4000ms...)")
        print("  - 'Max Retries' message when retries are exhausted")
        print("\nThe full retry sequence will take about 25-30 seconds.")
        print("Waiting 40 seconds for retries to complete...\n")

        # Wait for retries to complete
        for i in range(40):
            print(f"\rTime elapsed: {i+1}s", end='', flush=True)
            time.sleep(1)

        print("\n\n[OK] Test complete!")
        print("Check the monitor terminal to verify error handler behavior.")

        ser.close()

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nTest interrupted")
        sys.exit(0)

if __name__ == '__main__':
    main()
