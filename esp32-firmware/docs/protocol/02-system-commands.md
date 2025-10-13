# System Commands

System commands provide basic ESP32 functionality including connectivity testing, reset, and version information.

## Command Summary

| Command ID | Name | Description |
|------------|------|-------------|
| 0x01 | CMD_PING | Test connectivity and echo data |
| 0x02 | CMD_RESET | Reset ESP32 |
| 0x03 | CMD_GET_VERSION | Get firmware version |

## CMD_PING (0x01)

Test connectivity between PYNQ and ESP32. The ESP32 echoes back any payload data sent with the command.

### Request Packet

```
+-------+------+--------+--------+-----------+
| Start | CMD  | Len_Lo | Len_Hi | Payload   |
+-------+------+--------+--------+-----------+
| 0xA5  | 0x01 | N      | 0      | N bytes   |
+-------+------+--------+--------+-----------+
```

**Payload**: Optional data to echo (0-1024 bytes)

### Response Packet

```
+-------+------+--------+--------+--------+----------+-------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Data  |
+-------+------+--------+--------+--------+----------+-------+
| 0xA5  | 0xF0 | varies | varies | 0x00   | N        | Echo  |
+-------+------+--------+--------+--------+----------+-------+
```

**Status**: `0x00` (STATUS_OK)
**Data**: Echo of sent payload

### Use Cases

1. **Verify UART communication**
2. **Test round-trip latency**
3. **Validate packet integrity**

### Python Implementation

```python
#!/usr/bin/env python3
"""Test ESP32 connectivity with PING command"""

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

def ping_esp32(uart, test_data=b'Hello ESP32!'):
    """
    Send PING command and verify echo

    Args:
        uart: Serial connection
        test_data: Data to echo (default: "Hello ESP32!")

    Returns:
        bool: True if ping succeeded and echo matches
    """
    # Create ping packet
    packet = create_packet(0x01, test_data)

    # Send packet
    print(f"Sending PING with {len(test_data)} bytes: {test_data}")
    uart.write(packet)

    # Calculate expected response length
    # Header (4) + Status (1) + Data_len (2) + Echo data
    expected_len = 4 + 1 + 2 + len(test_data)

    # Read response
    response = uart.read(expected_len)

    if len(response) < 7:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return False

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        print(f"ERROR: Invalid response header")
        return False

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Status = 0x{status:02X}")
        return False

    data_len = response[5] | (response[6] << 8)
    echo_data = response[7:7+data_len]

    # Verify echo
    if echo_data == test_data:
        print(f"[OK] PING successful! Echo: {echo_data}")
        return True
    else:
        print(f"ERROR: Echo mismatch")
        print(f"  Sent: {test_data}")
        print(f"  Received: {echo_data}")
        return False

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python ping.py <serial_port> [test_data]")
        print("\nExample:")
        print("  python ping.py /dev/ttyUSB0")
        print("  python ping.py /dev/ttyUSB0 'Custom message'")
        sys.exit(1)

    port = sys.argv[1]
    test_data = sys.argv[2].encode() if len(sys.argv) > 2 else b'Hello ESP32!'

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Test ping
    success = ping_esp32(uart, test_data)

    uart.close()

    sys.exit(0 if success else 1)
```

### Example Usage

```bash
# Basic ping
python ping.py /dev/ttyUSB0

# Ping with custom message
python ping.py /dev/ttyUSB0 "Testing 1-2-3"

# Ping with maximum payload (1024 bytes)
python ping.py /dev/ttyUSB0 "$(python -c 'print("A"*1024)')"
```

## CMD_RESET (0x02)

Reset the ESP32. The ESP32 will reboot approximately 1 second after receiving this command.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x02 | 0x00   | 0x00   |
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

**Status**: `0x00` (STATUS_OK)
**Data**: None

**Note**: ESP32 resets ~1 second after sending response to allow UART transmission to complete.

### Use Cases

1. **Recovery from error states**
2. **Apply configuration changes**
3. **Clear WiFi connections**
4. **Restart services**

### Python Implementation

```python
#!/usr/bin/env python3
"""Reset ESP32"""

import serial
import struct
import time

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
    import sys

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
```

### Example Usage

```bash
# Reset ESP32
python reset.py /dev/ttyUSB0
```

## CMD_GET_VERSION (0x03)

Get the firmware version running on the ESP32.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x03 | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+----------+-----------------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Version (3B)    |
+-------+------+--------+--------+--------+----------+-----------------+
| 0xA5  | 0xF0 | 0x04   | 0x00   | 0x00   | 0x03,0x00| Major,Minor,Patch|
+-------+------+--------+--------+--------+----------+-----------------+
```

**Status**: `0x00` (STATUS_OK)
**Data**: 3 bytes representing version (major, minor, patch)

### Version Format

Version follows semantic versioning: `major.minor.patch`

- **Major** (byte 0): Incompatible API changes
- **Minor** (byte 1): New functionality, backward compatible
- **Patch** (byte 2): Bug fixes, backward compatible

Example: `0x00, 0x01, 0x00` = version 0.1.0

### Python Implementation

```python
#!/usr/bin/env python3
"""Get ESP32 firmware version"""

import serial
import struct
import time

def get_version(uart):
    """
    Get firmware version from ESP32

    Args:
        uart: Serial connection

    Returns:
        tuple: (major, minor, patch) or None on failure
    """
    # Create version request packet
    packet = bytes([0xA5, 0x03, 0x00, 0x00])

    print("Requesting firmware version...")
    uart.write(packet)

    # Wait for response (header + status + data_len + 3 version bytes)
    response = uart.read(10)

    if len(response) < 10:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return None

    # Parse response
    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response header")
        return None

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Status = 0x{status:02X}")
        return None

    data_len = response[5] | (response[6] << 8)
    if data_len != 3:
        print(f"ERROR: Expected 3 version bytes, got {data_len}")
        return None

    # Extract version
    major = response[7]
    minor = response[8]
    patch = response[9]

    return (major, minor, patch)

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python get_version.py <serial_port>")
        print("\nExample:")
        print("  python get_version.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    # Clear buffers
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Get version
    version = get_version(uart)

    if version:
        major, minor, patch = version
        print(f"[OK] ESP32 Firmware Version: {major}.{minor}.{patch}")
    else:
        print("[FAIL] Failed to get version")
        sys.exit(1)

    uart.close()
```

### Example Usage

```bash
# Get version
python get_version.py /dev/ttyUSB0

# Output:
# Requesting firmware version...
# [OK] ESP32 Firmware Version: 0.1.0
```

## Complete Test Suite

```python
#!/usr/bin/env python3
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
```

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| No response | UART not connected | Check cables and port |
| Invalid start marker | Wrong baud rate | Use 115200 baud |
| Timeout | ESP32 not running | Reset ESP32 |
| Wrong echo data | Data corruption | Check UART quality |

### Recovery Strategies

1. **Retry with backoff**: Wait increasing intervals between retries
2. **Reset connection**: Close and reopen UART
3. **Hard reset**: Use CMD_RESET to restart ESP32

## See Also

- [Protocol Overview](01-overview.md)
- [WiFi Commands](03-wifi-commands.md)
- [OTA Commands](05-ota-commands.md)
