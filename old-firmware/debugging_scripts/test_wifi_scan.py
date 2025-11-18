#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_wifi_scan.py

"""
Test WiFi scan functionality on ESP32
"""

import serial
import sys
import time
import struct

SERIAL_PORT = '/dev/ttyUSB0'  # USB-UART adapter on TX2/RX2
BAUD_RATE = 115200

# Protocol constants
START_MARKER = 0xA5
CMD_WIFI_SCAN = 0x13
CMD_RESPONSE = 0xF0
STATUS_OK = 0x00

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

def parse_packet(data):
    """Parse a protocol packet"""
    if len(data) < 4:
        return None

    if data[0] != START_MARKER:
        return None

    cmd = data[1]
    payload_len = data[2] | (data[3] << 8)

    if len(data) < 4 + payload_len:
        return None

    payload = data[4:4+payload_len]

    return {
        'cmd': cmd,
        'payload_len': payload_len,
        'payload': payload
    }

def main():
    """Test WiFi scan command"""
    print(f"Opening {SERIAL_PORT} at {BAUD_RATE} baud...")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=15)
        time.sleep(0.5)

        # Clear buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print("\n=== Testing WiFi Scan ===")
        scan_packet = create_packet(CMD_WIFI_SCAN)
        print(f"Sending WIFI_SCAN packet: {scan_packet.hex()}")
        ser.write(scan_packet)

        # Wait for scan to complete (may take several seconds)
        print("Waiting for scan results...")
        time.sleep(12)  # Scan can take up to 10 seconds

        response = ser.read(4096)  # Large buffer for multiple APs

        if response:
            print(f"\nReceived {len(response)} bytes: {response.hex()}")
            packet = parse_packet(response)

            if packet:
                print(f"  Command: 0x{packet['cmd']:02X}")
                print(f"  Payload length: {packet['payload_len']}")

                if packet['cmd'] == CMD_RESPONSE and len(packet['payload']) > 0:
                    status = packet['payload'][0]
                    print(f"  Status: 0x{status:02X} {'(OK)' if status == STATUS_OK else '(ERROR)'}")

                    if status == STATUS_OK and len(packet['payload']) > 1:
                        # Parse scan results
                        # Each result: SSID (33 bytes) + RSSI (1 byte) + Channel (1 byte) + Auth (1 byte) = 36 bytes
                        scan_data = packet['payload'][1:]
                        result_size = 36
                        num_results = len(scan_data) // result_size

                        print(f"\nFound {num_results} WiFi networks:")
                        for i in range(num_results):
                            offset = i * result_size
                            ssid_bytes = scan_data[offset:offset+33]
                            # Find null terminator
                            ssid = ssid_bytes.split(b'\x00')[0].decode('utf-8', errors='ignore')
                            rssi = struct.unpack('b', scan_data[offset+33:offset+34])[0]  # signed byte
                            channel = scan_data[offset+34]
                            auth_mode = scan_data[offset+35]

                            auth_str = {
                                0: "OPEN",
                                1: "WEP",
                                2: "WPA_PSK",
                                3: "WPA2_PSK",
                                4: "WPA_WPA2_PSK",
                                5: "WPA2_ENTERPRISE",
                                6: "WPA3_PSK",
                                7: "WPA2_WPA3_PSK",
                            }.get(auth_mode, f"UNKNOWN({auth_mode})")

                            print(f"  {i+1}. SSID: {ssid:32s} | RSSI: {rssi:3d} dBm | Ch: {channel:2d} | Auth: {auth_str}")
            else:
                print("  Failed to parse response")
        else:
            print("No response received")

        ser.close()
        print("\nTest complete!")

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
