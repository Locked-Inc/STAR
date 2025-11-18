# WiFi Commands

WiFi commands provide network connectivity management including connecting to WiFi networks, scanning for available networks, and monitoring connection status.

## Command Summary

| Command ID | Name | Description |
|------------|------|-------------|
| 0x10 | CMD_WIFI_CONNECT | Connect to WiFi network |
| 0x11 | CMD_WIFI_DISCONNECT | Disconnect from WiFi |
| 0x12 | CMD_WIFI_STATUS | Get WiFi connection status |
| 0x13 | CMD_WIFI_SCAN | Scan for WiFi networks |

## WiFi Status Codes

WiFi connection status is represented by the following values:

| Code | Name | Description |
|------|------|-------------|
| 0x00 | WIFI_DISCONNECTED | Not connected to any network |
| 0x01 | WIFI_CONNECTING | Connection in progress |
| 0x02 | WIFI_CONNECTED | Successfully connected |
| 0x03 | WIFI_FAILED | Connection failed |

## WiFi Authentication Modes

WiFi networks use various authentication modes for security:

| Code | Name | Description |
|------|------|-------------|
| 0 | OPEN | Open network (no security) |
| 1 | WEP | WEP security (deprecated, insecure) |
| 2 | WPA_PSK | WPA with Pre-Shared Key |
| 3 | WPA2_PSK | WPA2 with Pre-Shared Key |
| 4 | WPA_WPA2_PSK | WPA/WPA2 mixed mode |
| 5 | WPA2_ENTERPRISE | WPA2 Enterprise (802.1X) |
| 6 | WPA3_PSK | WPA3 with Pre-Shared Key |
| 7 | WPA2_WPA3_PSK | WPA2/WPA3 mixed mode |
| 8+ | UNKNOWN | Unknown or future auth modes |

## CMD_WIFI_CONNECT (0x10)

Connect to a WiFi network using SSID and password.

### Request Packet

```
+-------+------+--------+--------+------------+--------------+
| Start | CMD  | Len_Lo | Len_Hi | SSID (32B) | Password(64B)|
+-------+------+--------+--------+------------+--------------+
| 0xA5  | 0x10 | 0x60   | 0x00   | SSID       | Password     |
+-------+------+--------+--------+------------+--------------+
```

**Payload**: 96 bytes total
- **SSID** (32 bytes): WiFi network name, null-terminated string
- **Password** (64 bytes): WiFi password, null-terminated string

**Note**: Both SSID and password must be null-terminated. If shorter than the buffer size, pad with zeros or simply null-terminate.

### Response Packet

```
+-------+------+--------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi | Status |
+-------+------+--------+--------+--------+
| 0xA5  | 0xF0 | 0x01   | 0x00   | 0x00   |
+-------+------+--------+--------+--------+
```

**Status**:
- `0x00` (STATUS_OK): Connection initiated successfully
- `0x01` (STATUS_ERROR): Failed to start connection

**Important**: A successful response means the connection attempt has started, not that the connection is complete. Use CMD_WIFI_STATUS to poll for connection status.

### Connection Process

The ESP32 uses automatic retry with exponential backoff for connection attempts:

1. **Initial attempt**: Immediate connection attempt
2. **Retry on failure**: Up to 5 retries with increasing delays (1s, 2s, 4s, 8s, 10s max)
3. **Success**: Status changes to WIFI_CONNECTED when IP address is obtained
4. **Failure**: Status changes to WIFI_FAILED after all retries exhausted

### Python Implementation

```python
#!/usr/bin/env python3
"""Connect to WiFi network"""

import serial
import struct
import time

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        0xA5,  # Start marker
        cmd,   # Command ID
        payload_len & 0xFF,          # Len_Lo
        (payload_len >> 8) & 0xFF    # Len_Hi
    ])
    packet += payload
    return packet

def wifi_connect(uart, ssid, password):
    """
    Connect to WiFi network

    Args:
        uart: Serial connection
        ssid: WiFi network name (max 31 chars)
        password: WiFi password (max 63 chars)

    Returns:
        bool: True if connection initiated, False on error
    """
    if len(ssid) > 31:
        print(f"ERROR: SSID too long ({len(ssid)} > 31)")
        return False

    if len(password) > 63:
        print(f"ERROR: Password too long ({len(password)} > 63)")
        return False

    # Create payload: SSID (32 bytes) + Password (64 bytes)
    payload = bytearray(96)

    # Copy SSID
    ssid_bytes = ssid.encode('utf-8')
    payload[0:len(ssid_bytes)] = ssid_bytes

    # Copy password
    password_bytes = password.encode('utf-8')
    payload[32:32+len(password_bytes)] = password_bytes

    # Create and send packet
    packet = create_packet(0x10, bytes(payload))

    print(f"Connecting to WiFi: '{ssid}'...")
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
        print("Connection initiated successfully")
        print("Note: Use wifi_status() to check connection progress")
        return True
    else:
        print(f"ERROR: Failed to initiate connection (status=0x{status:02X})")
        return False

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python wifi_connect.py <serial_port> <ssid> [password]")
        print("\nExamples:")
        print("  python wifi_connect.py /dev/ttyUSB0 MyNetwork mypassword")
        print("  python wifi_connect.py /dev/ttyUSB0 OpenNetwork")
        sys.exit(1)

    port = sys.argv[1]
    ssid = sys.argv[2]
    password = sys.argv[3] if len(sys.argv) > 3 else ""

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Connect to WiFi
    success = wifi_connect(uart, ssid, password)

    if success:
        print("\nPolling connection status...")
        # Poll status for up to 30 seconds
        for i in range(15):
            time.sleep(2)

            # Send status request
            uart.reset_input_buffer()
            status_packet = create_packet(0x12)
            uart.write(status_packet)

            response = uart.read(11)
            if len(response) >= 11 and response[0] == 0xA5:
                wifi_status = response[7]

                if wifi_status == 0x02:  # CONNECTED
                    ip = response[8:12]
                    rssi = struct.unpack('b', response[12:13])[0]
                    print(f"\nConnected successfully!")
                    print(f"  IP: {ip[0]}.{ip[1]}.{ip[2]}.{ip[3]}")
                    print(f"  RSSI: {rssi} dBm")
                    break
                elif wifi_status == 0x03:  # FAILED
                    print(f"\nConnection failed!")
                    break
                elif wifi_status == 0x01:  # CONNECTING
                    print(f"  Still connecting... ({i+1}/15)")
                else:
                    print(f"  Status: 0x{wifi_status:02X}")

    uart.close()
    sys.exit(0 if success else 1)
```

### Example Usage

```bash
# Connect to WPA2 network
python wifi_connect.py /dev/ttyUSB0 MyHomeWiFi mypassword123

# Connect to open network
python wifi_connect.py /dev/ttyUSB0 OpenNetwork

# Connect with spaces in SSID
python wifi_connect.py /dev/ttyUSB0 "Coffee Shop WiFi" password
```

## CMD_WIFI_DISCONNECT (0x11)

Disconnect from the current WiFi network.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x11 | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi | Status |
+-------+------+--------+--------+--------+
| 0xA5  | 0xF0 | 0x01   | 0x00   | 0x00   |
+-------+------+--------+--------+--------+
```

**Status**:
- `0x00` (STATUS_OK): Disconnected successfully
- `0x01` (STATUS_ERROR): Disconnection failed

### Python Implementation

```python
#!/usr/bin/env python3
"""Disconnect from WiFi network"""

import serial
import time

def wifi_disconnect(uart):
    """
    Disconnect from WiFi network

    Args:
        uart: Serial connection

    Returns:
        bool: True if disconnected successfully
    """
    # Create disconnect packet (no payload)
    packet = bytes([0xA5, 0x11, 0x00, 0x00])

    print("Disconnecting from WiFi...")
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
        print("Disconnected successfully")
        return True
    else:
        print(f"ERROR: Failed to disconnect (status=0x{status:02X})")
        return False

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python wifi_disconnect.py <serial_port>")
        print("\nExample:")
        print("  python wifi_disconnect.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Disconnect
    success = wifi_disconnect(uart)

    uart.close()
    sys.exit(0 if success else 1)
```

### Example Usage

```bash
# Disconnect from current network
python wifi_disconnect.py /dev/ttyUSB0
```

## CMD_WIFI_STATUS (0x12)

Get the current WiFi connection status, including IP address and signal strength.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x12 | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+----------+--------+--------+--------+--------+------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | WStatus| IP[0-3]          | RSSI |
+-------+------+--------+--------+--------+----------+--------+--------+--------+--------+------+
| 0xA5  | 0xF0 | 0x07   | 0x00   | 0x00   | 0x06,0x00| 1 byte | 4 bytes          | 1 B  |
+-------+------+--------+--------+--------+----------+--------+--------+--------+--------+------+
```

**Status**: `0x00` (STATUS_OK)

**Data** (6 bytes):
- **WiFi Status** (1 byte): Connection status (see WiFi Status Codes)
- **IP Address** (4 bytes): IP address in network byte order (e.g., 192.168.1.100)
- **RSSI** (1 byte): Signal strength in dBm (signed, typically -90 to -30)

### Python Implementation

```python
#!/usr/bin/env python3
"""Get WiFi connection status"""

import serial
import struct
import time

def wifi_status(uart, verbose=True):
    """
    Get WiFi connection status

    Args:
        uart: Serial connection
        verbose: Print status information

    Returns:
        dict: Status information or None on failure
            {
                'wifi_status': int,      # 0=disconnected, 1=connecting, 2=connected, 3=failed
                'ip_address': str,       # IP address (e.g., "192.168.1.100")
                'rssi': int              # Signal strength in dBm
            }
    """
    # Create status request packet
    packet = bytes([0xA5, 0x12, 0x00, 0x00])

    if verbose:
        print("Requesting WiFi status...")
    uart.write(packet)

    # Wait for response (header + status + data_len + 6 data bytes)
    response = uart.read(13)

    if len(response) < 13:
        if verbose:
            print(f"ERROR: Response too short ({len(response)} bytes)")
        return None

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        if verbose:
            print("ERROR: Invalid response header")
        return None

    status = response[4]
    if status != 0x00:
        if verbose:
            print(f"ERROR: Status = 0x{status:02X}")
        return None

    data_len = response[5] | (response[6] << 8)
    if data_len != 6:
        if verbose:
            print(f"ERROR: Expected 6 data bytes, got {data_len}")
        return None

    # Extract WiFi status data
    wifi_status_code = response[7]
    ip_bytes = response[8:12]
    rssi = struct.unpack('b', response[12:13])[0]  # signed byte

    # Format IP address
    ip_address = f"{ip_bytes[0]}.{ip_bytes[1]}.{ip_bytes[2]}.{ip_bytes[3]}"

    status_names = {
        0x00: "DISCONNECTED",
        0x01: "CONNECTING",
        0x02: "CONNECTED",
        0x03: "FAILED"
    }

    result = {
        'wifi_status': wifi_status_code,
        'wifi_status_name': status_names.get(wifi_status_code, f"UNKNOWN(0x{wifi_status_code:02X})"),
        'ip_address': ip_address,
        'rssi': rssi
    }

    if verbose:
        print(f"WiFi Status: {result['wifi_status_name']}")
        if wifi_status_code == 0x02:  # CONNECTED
            print(f"  IP Address: {ip_address}")
            print(f"  Signal Strength: {rssi} dBm")

            # Signal quality interpretation
            if rssi >= -50:
                quality = "Excellent"
            elif rssi >= -60:
                quality = "Good"
            elif rssi >= -70:
                quality = "Fair"
            else:
                quality = "Weak"
            print(f"  Signal Quality: {quality}")
        elif wifi_status_code == 0x01:  # CONNECTING
            print(f"  Connection in progress...")
        elif wifi_status_code == 0x03:  # FAILED
            print(f"  Connection failed")

    return result

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python wifi_status.py <serial_port> [--monitor]")
        print("\nExamples:")
        print("  python wifi_status.py /dev/ttyUSB0")
        print("  python wifi_status.py /dev/ttyUSB0 --monitor  # Continuous monitoring")
        sys.exit(1)

    port = sys.argv[1]
    monitor = "--monitor" in sys.argv

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    if monitor:
        print("WiFi Status Monitor (Ctrl+C to stop)")
        print("=" * 50)
        try:
            while True:
                uart.reset_input_buffer()
                uart.reset_output_buffer()

                status = wifi_status(uart, verbose=True)
                print("-" * 50)

                time.sleep(2)
        except KeyboardInterrupt:
            print("\nMonitoring stopped")
    else:
        # Single status check
        uart.reset_input_buffer()
        uart.reset_output_buffer()

        status = wifi_status(uart, verbose=True)

        if status:
            sys.exit(0)
        else:
            sys.exit(1)

    uart.close()
```

### Example Usage

```bash
# Get current WiFi status
python wifi_status.py /dev/ttyUSB0

# Monitor WiFi status continuously (update every 2 seconds)
python wifi_status.py /dev/ttyUSB0 --monitor

# Output:
# WiFi Status: CONNECTED
#   IP Address: 192.168.1.100
#   Signal Strength: -45 dBm
#   Signal Quality: Excellent
```

## CMD_WIFI_SCAN (0x13)

Scan for available WiFi networks and return a list of access points with their details.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x13 | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+----------+-------------------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Scan Results      |
+-------+------+--------+--------+--------+----------+-------------------+
| 0xA5  | 0xF0 | varies | varies | 0x00   | varies   | N * scan_result_t |
+-------+------+--------+--------+--------+----------+-------------------+
```

**Status**:
- `0x00` (STATUS_OK): Scan completed successfully
- `0x01` (STATUS_ERROR): Scan failed

**Data**: Array of scan results, each 36 bytes:

```c
typedef struct {
    char    ssid[33];   // SSID (max 32 chars + null terminator)
    int8_t  rssi;       // Signal strength in dBm (signed)
    uint8_t channel;    // WiFi channel (1-14)
    uint8_t auth_mode;  // Authentication mode (see auth mode table)
} wifi_scan_result_t;  // Total: 36 bytes
```

### Scan Result Format

Each scan result is exactly 36 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0-32 | 33 bytes | SSID | Network name (null-terminated) |
| 33 | 1 byte | RSSI | Signal strength (signed, dBm) |
| 34 | 1 byte | Channel | WiFi channel number |
| 35 | 1 byte | Auth Mode | Security type (see auth modes) |

### Scan Timing

- **Scan duration**: Typically 5-10 seconds
- **Maximum results**: 20 networks
- **Timeout**: 15 seconds (recommended client timeout)

### Python Implementation

```python
#!/usr/bin/env python3
"""Scan for WiFi networks"""

import serial
import struct
import time

def wifi_scan(uart):
    """
    Scan for available WiFi networks

    Args:
        uart: Serial connection

    Returns:
        list: List of network dictionaries, each containing:
            {
                'ssid': str,        # Network name
                'rssi': int,        # Signal strength (dBm)
                'channel': int,     # WiFi channel
                'auth_mode': int,   # Authentication mode code
                'auth_name': str    # Authentication mode name
            }
    """
    # Auth mode mapping
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

    # Create scan request packet
    packet = bytes([0xA5, 0x13, 0x00, 0x00])

    print("Starting WiFi scan...")
    print("This may take 5-10 seconds...")
    uart.write(packet)

    # Wait for scan to complete (can take up to 10 seconds)
    # Read response in chunks with timeout
    start_time = time.time()
    response = b''

    while time.time() - start_time < 15:  # 15 second timeout
        if uart.in_waiting > 0:
            chunk = uart.read(uart.in_waiting)
            response += chunk

            # Check if we have at least the header
            if len(response) >= 7:
                # Check if we have complete packet
                if response[0] == 0xA5 and response[1] == 0xF0:
                    data_len = response[5] | (response[6] << 8)
                    expected_len = 7 + data_len

                    if len(response) >= expected_len:
                        break  # Complete packet received

        time.sleep(0.1)

    if len(response) < 7:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return None

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response header")
        return None

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Scan failed (status=0x{status:02X})")
        return None

    data_len = response[5] | (response[6] << 8)
    scan_data = response[7:7+data_len]

    # Parse scan results (each result is 36 bytes)
    result_size = 36
    num_results = len(scan_data) // result_size

    if len(scan_data) % result_size != 0:
        print(f"WARNING: Partial scan result (data_len={len(scan_data)}, expected multiple of 36)")

    networks = []
    for i in range(num_results):
        offset = i * result_size

        # Extract SSID (33 bytes, null-terminated)
        ssid_bytes = scan_data[offset:offset+33]
        ssid = ssid_bytes.split(b'\x00')[0].decode('utf-8', errors='ignore')

        # Extract RSSI (signed byte)
        rssi = struct.unpack('b', scan_data[offset+33:offset+34])[0]

        # Extract channel
        channel = scan_data[offset+34]

        # Extract auth mode
        auth_mode = scan_data[offset+35]
        auth_name = auth_modes.get(auth_mode, f"UNKNOWN({auth_mode})")

        networks.append({
            'ssid': ssid,
            'rssi': rssi,
            'channel': channel,
            'auth_mode': auth_mode,
            'auth_name': auth_name
        })

    return networks

def print_scan_results(networks):
    """Pretty print scan results"""
    if not networks:
        print("No networks found")
        return

    print(f"\nFound {len(networks)} WiFi networks:\n")

    # Header
    print(f"{'#':<3} {'SSID':<32} {'RSSI':>5} {'Signal':<10} {'Ch':>3} {'Security':<16}")
    print("-" * 80)

    # Sort by RSSI (strongest first)
    sorted_networks = sorted(networks, key=lambda x: x['rssi'], reverse=True)

    for i, network in enumerate(sorted_networks, 1):
        # Signal quality
        rssi = network['rssi']
        if rssi >= -50:
            signal = "Excellent"
        elif rssi >= -60:
            signal = "Good"
        elif rssi >= -70:
            signal = "Fair"
        elif rssi >= -80:
            signal = "Weak"
        else:
            signal = "Very Weak"

        # Truncate long SSIDs
        ssid = network['ssid']
        if len(ssid) > 32:
            ssid = ssid[:29] + "..."

        print(f"{i:<3} {ssid:<32} {rssi:>4} dBm {signal:<10} {network['channel']:>3} {network['auth_name']:<16}")

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python wifi_scan.py <serial_port> [--json]")
        print("\nExamples:")
        print("  python wifi_scan.py /dev/ttyUSB0")
        print("  python wifi_scan.py /dev/ttyUSB0 --json  # Output as JSON")
        sys.exit(1)

    port = sys.argv[1]
    json_output = "--json" in sys.argv

    # Open UART
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Perform scan
    networks = wifi_scan(uart)

    if networks is not None:
        if json_output:
            import json
            print(json.dumps(networks, indent=2))
        else:
            print_scan_results(networks)

        uart.close()
        sys.exit(0)
    else:
        print("Scan failed!")
        uart.close()
        sys.exit(1)
```

### Example Usage

```bash
# Scan for networks
python wifi_scan.py /dev/ttyUSB0

# Output as JSON
python wifi_scan.py /dev/ttyUSB0 --json

# Example output:
# Starting WiFi scan...
# This may take 5-10 seconds...
#
# Found 8 WiFi networks:
#
# #   SSID                             RSSI Signal     Ch  Security
# --------------------------------------------------------------------------------
# 1   MyHomeWiFi                       -35 dBm Excellent   6 WPA2_PSK
# 2   NeighborNetwork                  -52 dBm Good       11 WPA2_WPA3_PSK
# 3   Coffee_Shop_Free                 -58 dBm Good        1 OPEN
# 4   Office_5G                        -64 dBm Fair        36 WPA2_ENTERPRISE
# 5   Guest_Network                    -71 dBm Fair        6 WPA2_PSK
# 6   IoT_Devices                      -75 dBm Weak        11 WPA_PSK
# 7   OldRouter                        -82 dBm Weak        3 WEP
# 8   FarAway_AP                       -88 dBm Very Weak   1 WPA2_PSK
```

## Complete WiFi Management Library

Here's a complete Python library that combines all WiFi commands:

```python
#!/usr/bin/env python3
"""
Complete WiFi management library for ESP32

Provides high-level functions for WiFi operations.
"""

import serial
import struct
import time

class ESP32WiFi:
    """ESP32 WiFi management class"""

    # Command IDs
    CMD_WIFI_CONNECT = 0x10
    CMD_WIFI_DISCONNECT = 0x11
    CMD_WIFI_STATUS = 0x12
    CMD_WIFI_SCAN = 0x13
    CMD_RESPONSE = 0xF0

    # Status codes
    STATUS_OK = 0x00
    STATUS_ERROR = 0x01

    # WiFi status
    WIFI_DISCONNECTED = 0x00
    WIFI_CONNECTING = 0x01
    WIFI_CONNECTED = 0x02
    WIFI_FAILED = 0x03

    # Auth modes
    AUTH_MODES = {
        0: "OPEN",
        1: "WEP",
        2: "WPA_PSK",
        3: "WPA2_PSK",
        4: "WPA_WPA2_PSK",
        5: "WPA2_ENTERPRISE",
        6: "WPA3_PSK",
        7: "WPA2_WPA3_PSK",
    }

    def __init__(self, port, baud=115200, timeout=2):
        """
        Initialize WiFi manager

        Args:
            port: Serial port (e.g., '/dev/ttyUSB0')
            baud: Baud rate (default: 115200)
            timeout: Serial timeout in seconds
        """
        self.uart = serial.Serial(port, baud, timeout=timeout)
        time.sleep(0.1)

    def close(self):
        """Close serial connection"""
        if self.uart:
            self.uart.close()

    def _create_packet(self, cmd, payload=b''):
        """Create protocol packet"""
        payload_len = len(payload)
        packet = bytes([
            0xA5,  # Start marker
            cmd,
            payload_len & 0xFF,
            (payload_len >> 8) & 0xFF
        ])
        return packet + payload

    def _clear_buffers(self):
        """Clear UART buffers"""
        self.uart.reset_input_buffer()
        self.uart.reset_output_buffer()

    def connect(self, ssid, password="", wait=False, timeout=30):
        """
        Connect to WiFi network

        Args:
            ssid: Network name
            password: Network password (empty for open networks)
            wait: Wait for connection to complete
            timeout: Maximum wait time in seconds

        Returns:
            bool: True if connection initiated (or completed if wait=True)
        """
        if len(ssid) > 31 or len(password) > 63:
            return False

        # Create payload
        payload = bytearray(96)
        payload[0:len(ssid)] = ssid.encode('utf-8')
        payload[32:32+len(password)] = password.encode('utf-8')

        # Send command
        self._clear_buffers()
        packet = self._create_packet(self.CMD_WIFI_CONNECT, bytes(payload))
        self.uart.write(packet)

        # Read response
        response = self.uart.read(5)
        if len(response) < 5 or response[4] != self.STATUS_OK:
            return False

        # Wait for connection if requested
        if wait:
            start_time = time.time()
            while time.time() - start_time < timeout:
                status = self.get_status()
                if status and status['wifi_status'] == self.WIFI_CONNECTED:
                    return True
                elif status and status['wifi_status'] == self.WIFI_FAILED:
                    return False
                time.sleep(1)
            return False

        return True

    def disconnect(self):
        """
        Disconnect from WiFi

        Returns:
            bool: True if disconnected successfully
        """
        self._clear_buffers()
        packet = self._create_packet(self.CMD_WIFI_DISCONNECT)
        self.uart.write(packet)

        response = self.uart.read(5)
        return len(response) >= 5 and response[4] == self.STATUS_OK

    def get_status(self):
        """
        Get WiFi status

        Returns:
            dict: Status information or None on failure
        """
        self._clear_buffers()
        packet = self._create_packet(self.CMD_WIFI_STATUS)
        self.uart.write(packet)

        response = self.uart.read(13)
        if len(response) < 13 or response[4] != self.STATUS_OK:
            return None

        wifi_status = response[7]
        ip = response[8:12]
        rssi = struct.unpack('b', response[12:13])[0]

        return {
            'wifi_status': wifi_status,
            'ip_address': f"{ip[0]}.{ip[1]}.{ip[2]}.{ip[3]}",
            'rssi': rssi
        }

    def scan(self):
        """
        Scan for WiFi networks

        Returns:
            list: List of network dictionaries or None on failure
        """
        self._clear_buffers()
        packet = self._create_packet(self.CMD_WIFI_SCAN)
        self.uart.write(packet)

        # Wait for scan (up to 15 seconds)
        start_time = time.time()
        response = b''

        while time.time() - start_time < 15:
            if self.uart.in_waiting > 0:
                response += self.uart.read(self.uart.in_waiting)

                if len(response) >= 7:
                    data_len = response[5] | (response[6] << 8)
                    if len(response) >= 7 + data_len:
                        break
            time.sleep(0.1)

        if len(response) < 7 or response[4] != self.STATUS_OK:
            return None

        # Parse scan results
        data_len = response[5] | (response[6] << 8)
        scan_data = response[7:7+data_len]

        networks = []
        for i in range(len(scan_data) // 36):
            offset = i * 36
            ssid = scan_data[offset:offset+33].split(b'\x00')[0].decode('utf-8', errors='ignore')
            rssi = struct.unpack('b', scan_data[offset+33:offset+34])[0]
            channel = scan_data[offset+34]
            auth_mode = scan_data[offset+35]

            networks.append({
                'ssid': ssid,
                'rssi': rssi,
                'channel': channel,
                'auth_mode': auth_mode,
                'auth_name': self.AUTH_MODES.get(auth_mode, f"UNKNOWN({auth_mode})")
            })

        return networks

    def is_connected(self):
        """
        Check if connected to WiFi

        Returns:
            bool: True if connected
        """
        status = self.get_status()
        return status and status['wifi_status'] == self.WIFI_CONNECTED


# Example usage
if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python esp32_wifi.py <serial_port>")
        sys.exit(1)

    # Create WiFi manager
    wifi = ESP32WiFi(sys.argv[1])

    try:
        # Scan for networks
        print("Scanning for networks...")
        networks = wifi.scan()

        if networks:
            print(f"\nFound {len(networks)} networks:")
            for i, net in enumerate(sorted(networks, key=lambda x: x['rssi'], reverse=True), 1):
                print(f"  {i}. {net['ssid']:32s} {net['rssi']:>4} dBm  Ch:{net['channel']:>3}  {net['auth_name']}")

        # Connect to network (example)
        # wifi.connect("MyNetwork", "mypassword", wait=True)

        # Check status
        print("\nCurrent status:")
        status = wifi.get_status()
        if status:
            if status['wifi_status'] == wifi.WIFI_CONNECTED:
                print(f"  Connected to network")
                print(f"  IP: {status['ip_address']}")
                print(f"  RSSI: {status['rssi']} dBm")
            else:
                print(f"  Not connected")

    finally:
        wifi.close()
```

## Complete Test Suite

```python
#!/usr/bin/env python3
"""Complete WiFi commands test suite"""

import serial
import time
import sys

def test_wifi_commands(port):
    """Test all WiFi commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("="*70)
    print("ESP32 WiFi Commands Test Suite")
    print("="*70)

    results = {}

    # Test 1: WiFi Scan
    print("\n[1/4] Testing CMD_WIFI_SCAN...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    scan_packet = bytes([0xA5, 0x13, 0x00, 0x00])
    uart.write(scan_packet)
    print("  Scanning (this takes 5-10 seconds)...")

    start_time = time.time()
    response = b''
    while time.time() - start_time < 15:
        if uart.in_waiting > 0:
            response += uart.read(uart.in_waiting)
            if len(response) >= 7:
                data_len = response[5] | (response[6] << 8)
                if len(response) >= 7 + data_len:
                    break
        time.sleep(0.1)

    if len(response) >= 7 and response[4] == 0x00:
        num_networks = (response[5] | (response[6] << 8)) // 36
        print(f"  WIFI_SCAN: PASSED ({num_networks} networks found)")
        results['scan'] = True
    else:
        print("  WIFI_SCAN: FAILED")
        results['scan'] = False

    time.sleep(1)

    # Test 2: WiFi Status (while disconnected)
    print("\n[2/4] Testing CMD_WIFI_STATUS (disconnected)...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    status_packet = bytes([0xA5, 0x12, 0x00, 0x00])
    uart.write(status_packet)

    response = uart.read(13)
    if len(response) >= 13 and response[4] == 0x00:
        wifi_status = response[7]
        print(f"  WIFI_STATUS: PASSED (status=0x{wifi_status:02X})")
        results['status'] = True
    else:
        print("  WIFI_STATUS: FAILED")
        results['status'] = False

    time.sleep(1)

    # Test 3: WiFi Connect (user interactive)
    print("\n[3/4] Testing CMD_WIFI_CONNECT...")
    test_connect = input("  Connect to a network? This requires SSID and password. (y/N): ")

    if test_connect.lower() == 'y':
        ssid = input("  Enter SSID: ")
        password = input("  Enter password: ")

        uart.reset_input_buffer()
        uart.reset_output_buffer()

        payload = bytearray(96)
        payload[0:len(ssid)] = ssid.encode('utf-8')
        payload[32:32+len(password)] = password.encode('utf-8')

        connect_packet = bytes([0xA5, 0x10, 0x60, 0x00]) + bytes(payload)
        uart.write(connect_packet)

        response = uart.read(5)
        if len(response) >= 5 and response[4] == 0x00:
            print("  WIFI_CONNECT: Command accepted, waiting for connection...")

            # Poll status
            connected = False
            for i in range(15):
                time.sleep(2)
                uart.reset_input_buffer()
                uart.write(status_packet)
                response = uart.read(13)

                if len(response) >= 13:
                    wifi_status = response[7]
                    if wifi_status == 0x02:  # CONNECTED
                        ip = response[8:12]
                        print(f"  WIFI_CONNECT: PASSED (IP: {ip[0]}.{ip[1]}.{ip[2]}.{ip[3]})")
                        connected = True
                        results['connect'] = True
                        break
                    elif wifi_status == 0x03:  # FAILED
                        print("  WIFI_CONNECT: FAILED (connection failed)")
                        results['connect'] = False
                        break
                    elif wifi_status == 0x01:  # CONNECTING
                        print(f"    Still connecting... ({i+1}/15)")

            if not connected and 'connect' not in results:
                print("  WIFI_CONNECT: TIMEOUT")
                results['connect'] = False
        else:
            print("  WIFI_CONNECT: FAILED (command rejected)")
            results['connect'] = False

        # Test 4: WiFi Disconnect (if connected)
        if results.get('connect', False):
            time.sleep(1)
            print("\n[4/4] Testing CMD_WIFI_DISCONNECT...")
            uart.reset_input_buffer()
            uart.reset_output_buffer()

            disconnect_packet = bytes([0xA5, 0x11, 0x00, 0x00])
            uart.write(disconnect_packet)

            response = uart.read(5)
            if len(response) >= 5 and response[4] == 0x00:
                print("  WIFI_DISCONNECT: PASSED")
                results['disconnect'] = True
            else:
                print("  WIFI_DISCONNECT: FAILED")
                results['disconnect'] = False
        else:
            print("\n[4/4] WIFI_DISCONNECT: SKIPPED (not connected)")
            results['disconnect'] = None
    else:
        print("  WIFI_CONNECT: SKIPPED")
        print("\n[4/4] WIFI_DISCONNECT: SKIPPED")
        results['connect'] = None
        results['disconnect'] = None

    uart.close()

    # Summary
    print("\n" + "="*70)
    print("Test Summary:")
    print("="*70)

    for test, result in results.items():
        if result is True:
            status = "PASSED"
        elif result is False:
            status = "FAILED"
        else:
            status = "SKIPPED"
        print(f"  {test.upper():<15} {status}")

    print("="*70)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_wifi_commands.py <serial_port>")
        sys.exit(1)

    test_wifi_commands(sys.argv[1])
```

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| No response | ESP32 not ready | Wait longer, check connection |
| Scan timeout | WiFi interference | Retry scan, check antenna |
| Connection failed | Wrong password | Verify credentials |
| Weak signal | Too far from AP | Move closer or use different network |
| No networks found | WiFi disabled | Check ESP32 WiFi is initialized |

### Recovery Strategies

1. **Scan Timeout**
   - Retry scan up to 3 times
   - Increase timeout to 20 seconds
   - Check WiFi is not in connecting state

2. **Connection Failures**
   - Verify SSID and password are correct
   - Check signal strength (RSSI > -70 dBm recommended)
   - Try disconnecting first, then reconnecting
   - Reset ESP32 if repeated failures

3. **Signal Issues**
   - Move closer to access point
   - Check for interference (2.4GHz devices)
   - Try different WiFi channel
   - Use external antenna if available

## Troubleshooting Guide

### WiFi Not Connecting

**Symptoms**: Connection stays in CONNECTING state or fails

**Checks**:
1. Verify SSID and password are correct
2. Check signal strength (use scan to find RSSI)
3. Ensure network is 2.4GHz (ESP32 doesn't support 5GHz)
4. Check authentication mode is supported
5. Verify MAC filtering not blocking ESP32

**Solutions**:
- Use scan to verify network is visible
- Try connecting to open network first (test setup)
- Check ESP32 logs for detailed error messages
- Reset ESP32 and retry

### Scan Returns No Networks

**Symptoms**: Scan completes but returns 0 networks

**Checks**:
1. Verify antenna is connected
2. Check WiFi is enabled on ESP32
3. Ensure not in connecting/connected state
4. Check for RF shielding issues

**Solutions**:
- Disconnect from any network first
- Reset ESP32
- Move to area with known WiFi networks
- Check antenna connection

### Intermittent Connection Loss

**Symptoms**: Connection drops randomly

**Checks**:
1. Monitor RSSI (should be > -70 dBm)
2. Check for interference sources
3. Verify router stability
4. Check power supply voltage

**Solutions**:
- Move closer to access point
- Use WiFi status monitoring to track RSSI
- Implement automatic reconnection
- Check ESP32 power supply quality

### Response Parsing Errors

**Symptoms**: Invalid or corrupted responses

**Checks**:
1. Verify baud rate is 115200
2. Check UART connections
3. Look for electrical noise
4. Verify timeout is adequate

**Solutions**:
- Clear UART buffers before commands
- Add retry logic with exponential backoff
- Verify start marker (0xA5) before parsing
- Check cable quality and length

## Best Practices

### 1. Connection Management

```python
# Always check status before connecting
status = wifi.get_status()
if status['wifi_status'] != wifi.WIFI_DISCONNECTED:
    wifi.disconnect()
    time.sleep(1)

# Connect with wait
success = wifi.connect("MyNetwork", "password", wait=True, timeout=30)
```

### 2. Signal Monitoring

```python
# Monitor signal quality during operation
status = wifi.get_status()
if status and status['rssi'] < -75:
    print("WARNING: Weak signal!")
```

### 3. Scan Before Connect

```python
# Verify network availability before connecting
networks = wifi.scan()
target_network = next((n for n in networks if n['ssid'] == "MyNetwork"), None)

if target_network:
    if target_network['rssi'] > -70:
        wifi.connect("MyNetwork", "password")
    else:
        print("Signal too weak!")
else:
    print("Network not found!")
```

### 4. Automatic Reconnection

```python
# Implement reconnection logic
def ensure_connected(wifi, ssid, password, max_retries=3):
    for attempt in range(max_retries):
        if wifi.is_connected():
            return True

        print(f"Connection attempt {attempt+1}/{max_retries}")
        wifi.connect(ssid, password, wait=True, timeout=30)

        if wifi.is_connected():
            return True

        time.sleep(5)

    return False
```

### 5. Proper Resource Cleanup

```python
# Always close connection in finally block
wifi = ESP32WiFi('/dev/ttyUSB0')
try:
    # WiFi operations
    wifi.connect(...)
finally:
    wifi.close()
```

## See Also

- [Protocol Overview](01-overview.md)
- [System Commands](02-system-commands.md)
- [Network Commands](04-network-commands.md)
- [OTA Commands](05-ota-commands.md)
