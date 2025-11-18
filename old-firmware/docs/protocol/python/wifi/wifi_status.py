#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/wifi_status.py

"""Get WiFi connection status"""

import serial
import struct
import time
import sys

def wifi_get_status(uart):
    """
    Get WiFi connection status

    Args:
        uart: Serial connection

    Returns:
        dict: {
            'status': int (0=disconnected, 1=connecting, 2=connected, 3=failed),
            'status_name': str,
            'ip_address': str,
            'rssi': int (dBm)
        } or None on failure
    """
    # Create status request packet
    packet = bytes([0xA5, 0x12, 0x00, 0x00])

    uart.write(packet)

    # Read response (header + status + data_len + 6 bytes)
    response = uart.read(13)

    if len(response) < 13:
        return None

    # Parse header
    if response[0] != 0xA5 or response[1] != 0xF0:
        return None

    if response[4] != 0x00:  # Check status OK
        return None

    # Parse WiFi status data
    wifi_status = response[7]
    ip_addr = response[8:12]
    rssi = struct.unpack('b', response[12:13])[0]  # Signed byte

    status_names = {
        0: 'DISCONNECTED',
        1: 'CONNECTING',
        2: 'CONNECTED',
        3: 'FAILED'
    }

    ip_address = f"{ip_addr[0]}.{ip_addr[1]}.{ip_addr[2]}.{ip_addr[3]}"

    return {
        'status': wifi_status,
        'status_name': status_names.get(wifi_status, 'UNKNOWN'),
        'ip_address': ip_address,
        'rssi': rssi
    }

def get_signal_quality(rssi):
    """Convert RSSI to signal quality description"""
    if rssi >= -50:
        return "Excellent"
    elif rssi >= -60:
        return "Good"
    elif rssi >= -70:
        return "Fair"
    else:
        return "Weak"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python wifi_status.py <serial_port> [--monitor]")
        print("\nExamples:")
        print("  # Get status once")
        print("  python wifi_status.py /dev/ttyUSB0")
        print()
        print("  # Monitor status continuously")
        print("  python wifi_status.py /dev/ttyUSB0 --monitor")
        sys.exit(1)

    port = sys.argv[1]
    monitor = '--monitor' in sys.argv

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    try:
        if monitor:
            print("Monitoring WiFi status (Ctrl+C to exit)...")
            print("="*60)
            while True:
                uart.reset_input_buffer()
                uart.reset_output_buffer()

                status = wifi_get_status(uart)

                if status:
                    print(f"\r[{time.strftime('%H:%M:%S')}] "
                          f"Status: {status['status_name']:12s} | "
                          f"IP: {status['ip_address']:15s} | "
                          f"RSSI: {status['rssi']:3d} dBm ({get_signal_quality(status['rssi'])})",
                          end='', flush=True)
                else:
                    print(f"\r[{time.strftime('%H:%M:%S')}] Failed to get status", end='', flush=True)

                time.sleep(1)
        else:
            # Get status once
            uart.reset_input_buffer()
            uart.reset_output_buffer()

            status = wifi_get_status(uart)

            if status:
                print(f"WiFi Status:")
                print(f"  Status: {status['status_name']}")
                print(f"  IP Address: {status['ip_address']}")
                print(f"  Signal Strength: {status['rssi']} dBm ({get_signal_quality(status['rssi'])})")
            else:
                print("Failed to get WiFi status")
                sys.exit(1)

    except KeyboardInterrupt:
        print("\n\nMonitoring stopped")

    uart.close()
