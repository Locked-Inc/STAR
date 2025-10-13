#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/wifi/wifi_manager.py

"""
Complete WiFi management library for ESP32

Provides high-level API for all WiFi operations.
"""

import serial
import struct
import time

class ESP32WiFi:
    """ESP32 WiFi Manager"""

    # WiFi status constants
    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2
    FAILED = 3

    def __init__(self, port, baudrate=115200, timeout=2):
        """
        Initialize WiFi manager

        Args:
            port: Serial port (e.g., '/dev/ttyUSB0')
            baudrate: Baud rate (default: 115200)
            timeout: Serial timeout in seconds
        """
        self.uart = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.1)

    def close(self):
        """Close serial connection"""
        if self.uart and self.uart.is_open:
            self.uart.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def _create_packet(self, cmd, payload=b''):
        """Create protocol packet"""
        payload_len = len(payload)
        packet = bytes([
            0xA5,  # Start marker
            cmd,   # Command ID
            payload_len & 0xFF,          # Len_Lo
            (payload_len >> 8) & 0xFF    # Len_Hi
        ])
        packet += payload
        return packet

    def connect(self, ssid, password, timeout=10, wait=True):
        """
        Connect to WiFi network

        Args:
            ssid: WiFi SSID
            password: WiFi password
            timeout: Connection timeout in seconds
            wait: Wait for connection to complete

        Returns:
            bool: True if connected successfully (or connection started if wait=False)
        """
        # Prepare payload
        ssid_bytes = ssid.encode('utf-8')[:31] + b'\x00'
        ssid_bytes = ssid_bytes.ljust(32, b'\x00')

        password_bytes = password.encode('utf-8')[:63] + b'\x00'
        password_bytes = password_bytes.ljust(64, b'\x00')

        payload = ssid_bytes + password_bytes

        # Send command
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()

        packet = self._create_packet(0x10, payload)
        self.uart.write(packet)

        # Wait for response
        response = self.uart.read(5)
        if len(response) < 5 or response[0] != 0xA5 or response[1] != 0xF0:
            return False

        if response[4] != 0x00:  # Check status
            return False

        if not wait:
            return True

        # Poll status until connected or timeout
        start_time = time.time()
        while time.time() - start_time < timeout:
            status = self.get_status()
            if status and status['status'] == self.CONNECTED:
                return True
            elif status and status['status'] == self.FAILED:
                return False
            time.sleep(0.5)

        return False

    def disconnect(self):
        """
        Disconnect from WiFi

        Returns:
            bool: True if disconnected successfully
        """
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()

        packet = self._create_packet(0x11, b'')
        self.uart.write(packet)

        response = self.uart.read(5)
        if len(response) < 5:
            return False

        return response[0] == 0xA5 and response[1] == 0xF0 and response[4] == 0x00

    def get_status(self):
        """
        Get WiFi connection status

        Returns:
            dict: Status information or None
        """
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()

        packet = self._create_packet(0x12, b'')
        self.uart.write(packet)

        response = self.uart.read(13)
        if len(response) < 13:
            return None

        if response[0] != 0xA5 or response[1] != 0xF0:
            return None

        wifi_status = response[7]
        ip_addr = response[8:12]
        rssi = struct.unpack('b', response[12:13])[0]

        status_names = {
            0: 'DISCONNECTED',
            1: 'CONNECTING',
            2: 'CONNECTED',
            3: 'FAILED'
        }

        return {
            'status': wifi_status,
            'status_name': status_names.get(wifi_status, 'UNKNOWN'),
            'ip_address': f"{ip_addr[0]}.{ip_addr[1]}.{ip_addr[2]}.{ip_addr[3]}",
            'rssi': rssi
        }

    def scan(self, timeout=15):
        """
        Scan for WiFi networks

        Args:
            timeout: Scan timeout in seconds

        Returns:
            list: List of network dicts
        """
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()

        packet = self._create_packet(0x13, b'')
        self.uart.write(packet)

        # Wait for scan to complete
        time.sleep(10)

        response = self.uart.read(4096)
        if len(response) < 7:
            return []

        if response[0] != 0xA5 or response[1] != 0xF0 or response[4] != 0x00:
            return []

        data_len = response[5] | (response[6] << 8)
        scan_data = response[7:7+data_len]

        # Parse results
        result_size = 36
        num_results = len(scan_data) // result_size

        networks = []
        for i in range(num_results):
            offset = i * result_size

            ssid_bytes = scan_data[offset:offset+33]
            ssid = ssid_bytes.split(b'\x00')[0].decode('utf-8', errors='ignore')

            rssi = struct.unpack('b', scan_data[offset+33:offset+34])[0]
            channel = scan_data[offset+34]
            auth_mode = scan_data[offset+35]

            networks.append({
                'ssid': ssid,
                'rssi': rssi,
                'channel': channel,
                'auth_mode': auth_mode
            })

        return networks

    def is_connected(self):
        """
        Check if WiFi is connected

        Returns:
            bool: True if connected
        """
        status = self.get_status()
        return status and status['status'] == self.CONNECTED


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python wifi_manager.py <serial_port>")
        print("\nThis demonstrates the ESP32WiFi class")
        sys.exit(1)

    port = sys.argv[1]

    # Example usage
    with ESP32WiFi(port) as wifi:
        print("Scanning for networks...")
        networks = wifi.scan()
        print(f"Found {len(networks)} networks\n")

        for net in networks[:5]:  # Show top 5
            print(f"  {net['ssid']:30s} {net['rssi']:3d} dBm")

        print("\nChecking current status...")
        status = wifi.get_status()
        if status:
            print(f"  Status: {status['status_name']}")
            print(f"  IP: {status['ip_address']}")
            print(f"  RSSI: {status['rssi']} dBm")
