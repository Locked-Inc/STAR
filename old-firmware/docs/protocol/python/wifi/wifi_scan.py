#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/wifi_scan.py

"""Scan for WiFi networks"""

import serial
import struct
import time
import sys

def wifi_scan(uart):
    """
    Scan for WiFi networks

    Args:
        uart: Serial connection

    Returns:
        list: List of dicts with 'ssid', 'rssi', 'channel', 'auth_mode'
    """
    # Create scan request packet
    packet = bytes([0xA5, 0x13, 0x00, 0x00])

    print("Scanning for WiFi networks...")
    uart.write(packet)

    # Wait for scan to complete (may take 5-10 seconds)
    time.sleep(10)

    # Read response (may be large - up to 4KB)
    response = uart.read(4096)

    if len(response) < 7:
        print("ERROR: No response or response too short")
        return []

    # Parse header
    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response header")
        return []

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Scan failed (status: 0x{status:02X})")
        return []

    data_len = response[5] | (response[6] << 8)
    scan_data = response[7:7+data_len]

    # Parse scan results (each result is 36 bytes: SSID[33] + RSSI[1] + Channel[1] + Auth[1])
    result_size = 36
    num_results = len(scan_data) // result_size

    networks = []
    for i in range(num_results):
        offset = i * result_size

        # Extract SSID (null-terminated)
        ssid_bytes = scan_data[offset:offset+33]
        ssid = ssid_bytes.split(b'\x00')[0].decode('utf-8', errors='ignore')

        # Extract RSSI (signed byte)
        rssi = struct.unpack('b', scan_data[offset+33:offset+34])[0]

        # Extract channel and auth mode
        channel = scan_data[offset+34]
        auth_mode = scan_data[offset+35]

        networks.append({
            'ssid': ssid,
            'rssi': rssi,
            'channel': channel,
            'auth_mode': auth_mode
        })

    return networks

def get_auth_mode_name(auth_mode):
    """Convert auth mode number to name"""
    auth_modes = {
        0: "OPEN",
        1: "WEP",
        2: "WPA_PSK",
        3: "WPA2_PSK",
        4: "WPA_WPA2_PSK",
        5: "WPA2_ENTERPRISE",
        6: "WPA3_PSK",
        7: "WPA2_WPA3_PSK",
    }
    return auth_modes.get(auth_mode, f"UNKNOWN({auth_mode})")

def get_signal_quality(rssi):
    """Convert RSSI to signal quality"""
    if rssi >= -50:
        return "*****"
    elif rssi >= -60:
        return "*****"
    elif rssi >= -70:
        return "*****"
    elif rssi >= -80:
        return "*****"
    else:
        return "*****"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python wifi_scan.py <serial_port>")
        print("\nExample:")
        print("  python wifi_scan.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Scan for networks
    networks = wifi_scan(uart)

    if networks:
        print(f"\n[OK] Found {len(networks)} WiFi networks:\n")
        print(f"{'#':<3} {'SSID':<32} {'Signal':<8} {'RSSI':<6} {'Ch':<4} {'Security':<20}")
        print("-" * 80)

        # Sort by signal strength (strongest first)
        networks.sort(key=lambda x: x['rssi'], reverse=True)

        for i, network in enumerate(networks, 1):
            quality = get_signal_quality(network['rssi'])
            auth = get_auth_mode_name(network['auth_mode'])

            print(f"{i:<3} {network['ssid']:<32} {quality:<8} "
                  f"{network['rssi']:>3d} dBm {network['channel']:<4} {auth:<20}")
    else:
        print("[FAIL] No networks found or scan failed")
        sys.exit(1)

    uart.close()
