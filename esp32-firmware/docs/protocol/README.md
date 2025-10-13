# PYNQ-ESP32 Communication Protocol Documentation

Comprehensive documentation for the binary protocol used to communicate between PYNQ and ESP32 via UART.

## Quick Start

```python
import serial

# Open UART connection
uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Send PING command
packet = bytes([0xA5, 0x01, 0x00, 0x00])  # Start, CMD_PING, Length=0
uart.write(packet)

# Read response
response = uart.read(5)
print(f"Response: {response.hex()}")

uart.close()
```

## Documentation Index

### [01. Protocol Overview](01-overview.md)
**Start here!** Complete protocol specification including:
- Packet structure and format
- Command categories and ranges
- Response packet format
- Communication flow patterns
- Basic Python implementation
- Error handling strategies
- Best practices

### [02. System Commands](02-system-commands.md)
**Commands**: PING (0x01), RESET (0x02), GET_VERSION (0x03)

Core system functionality:
- Connectivity testing with echo
- ESP32 reset/reboot
- Firmware version query
- Complete Python implementations
- Usage examples for each command
- Test suite

### [03. WiFi Commands](03-wifi-commands.md)
**Commands**: WIFI_CONNECT (0x10), WIFI_DISCONNECT (0x11), WIFI_STATUS (0x12), WIFI_SCAN (0x13)

WiFi network management:
- Connect to WiFi with SSID/password
- Disconnect from networks
- Get connection status, IP address, RSSI
- Scan for available networks
- WiFi authentication modes
- Signal quality interpretation
- Complete WiFi management library
- Test suite

### [04. Network Commands](04-network-commands.md)
**Commands**: HTTP_GET (0x20), HTTP_POST (0x21), TCP_CONNECT (0x22), TCP_SEND (0x23), TCP_CLOSE (0x24)

Network and HTTP operations:
- **Current Status**: HTTP commands return NOT_IMPLEMENTED
- Protocol specifications for all commands
- Python examples (for future use)
- Implementation guide for contributors
- Testing tools to check implementation status

### [05. OTA Commands](05-ota-commands.md)
**Commands**: OTA_CHECK_UPDATE (0x28), OTA_START_UPDATE (0x29), OTA_GET_STATUS (0x2A)

Over-the-air firmware updates:
- Check for available updates
- Start firmware download with SHA256 verification
- Monitor update progress
- Version downgrade/rollback support
- Complete OTA workflow examples
- Security considerations
- Performance metrics

### [06. UART Transport Layer](06-uart-transport.md)

Physical communication layer:
- UART hardware configuration (115200 baud, 8N1)
- Platform-specific setup (Linux, macOS, Windows)
- Buffer management and flow control
- Packet synchronization and framing
- Handling partial reads and corruption
- PySerial configuration
- Troubleshooting guide
- Performance considerations

## Command Quick Reference

### System Commands (0x01-0x0F)

| ID | Command | Description | Status |
|----|---------|-------------|--------|
| 0x01 | PING | Test connectivity, echo data | [SUCCESS] Implemented |
| 0x02 | RESET | Reset ESP32 | [SUCCESS] Implemented |
| 0x03 | GET_VERSION | Get firmware version | [SUCCESS] Implemented |

### WiFi Commands (0x10-0x1F)

| ID | Command | Description | Status |
|----|---------|-------------|--------|
| 0x10 | WIFI_CONNECT | Connect to WiFi network | [SUCCESS] Implemented |
| 0x11 | WIFI_DISCONNECT | Disconnect from WiFi | [SUCCESS] Implemented |
| 0x12 | WIFI_STATUS | Get WiFi status | [SUCCESS] Implemented |
| 0x13 | WIFI_SCAN | Scan for networks | [SUCCESS] Implemented |

### Network Commands (0x20-0x27)

| ID | Command | Description | Status |
|----|---------|-------------|--------|
| 0x20 | HTTP_GET | HTTP GET request | [WARN] Not implemented |
| 0x21 | HTTP_POST | HTTP POST request | [WARN] Not implemented |
| 0x22 | TCP_CONNECT | Open TCP connection | [WARN] Not implemented |
| 0x23 | TCP_SEND | Send TCP data | [WARN] Not implemented |
| 0x24 | TCP_CLOSE | Close TCP connection | [WARN] Not implemented |

### OTA Commands (0x28-0x2F)

| ID | Command | Description | Status |
|----|---------|-------------|--------|
| 0x28 | OTA_CHECK_UPDATE | Check for firmware update | [SUCCESS] Implemented |
| 0x29 | OTA_START_UPDATE | Start OTA update | [SUCCESS] Implemented |
| 0x2A | OTA_GET_STATUS | Get OTA status | [SUCCESS] Implemented |

### Response Commands (0xF0-0xFF)

| ID | Command | Description |
|----|---------|-------------|
| 0xF0 | RESPONSE | Generic response |
| 0xFF | ERROR | Error response |

## Python Script Library

### Location
All Python scripts are organized by functionality:

- **System**: `docs/protocol/python/system/`
- **WiFi**: `docs/protocol/python/wifi/`
- **OTA**: `docs/ota/python/`

### Available Scripts

#### System Commands
- `ping.py` - Test connectivity
- `reset.py` - Reset ESP32
- `get_version.py` - Get firmware version
- `test_system_commands.py` - Complete test suite

#### WiFi Commands
- `wifi_connect.py` - Connect to WiFi
- `wifi_disconnect.py` - Disconnect from WiFi
- `wifi_status.py` - Get connection status
- `wifi_scan.py` - Scan for networks
- `wifi_manager.py` - Complete WiFi management library
- `test_wifi_commands.py` - Complete test suite

#### OTA Commands
- `ota_send_update.py` - Start firmware update
- `ota_get_status.py` - Get update status
- `ota_monitor.py` - Monitor update progress
- `ota_complete_workflow.py` - End-to-end update workflow

## Common Use Cases

### Use Case 1: Connect ESP32 to WiFi

```bash
# Scan for networks
python docs/protocol/python/wifi/wifi_scan.py /dev/ttyUSB0

# Connect to network
python docs/protocol/python/wifi/wifi_connect.py /dev/ttyUSB0 "MyWiFi" "password123"

# Verify connection
python docs/protocol/python/wifi/wifi_status.py /dev/ttyUSB0
```

### Use Case 2: Update ESP32 Firmware

```bash
# Check for updates
python docs/ota/python/ota_send_update.py /dev/ttyUSB0 \
  https://example.com/firmware-v0.2.0.bin \
  a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Monitor progress
python docs/ota/python/ota_monitor.py /dev/ttyUSB0
```

### Use Case 3: System Health Check

```bash
# Test connectivity
python docs/protocol/python/system/ping.py /dev/ttyUSB0

# Get firmware version
python docs/protocol/python/system/get_version.py /dev/ttyUSB0

# Check WiFi status
python docs/protocol/python/wifi/wifi_status.py /dev/ttyUSB0
```

### Use Case 4: Automated Testing

```python
#!/usr/bin/env python3
"""Automated ESP32 system test"""

import serial
import time
import sys

def run_system_tests(port):
    """Run complete system test suite"""
    uart = serial.Serial(port, 115200, timeout=5)

    tests_passed = 0
    tests_failed = 0

    # Test 1: Ping
    print("Testing PING...")
    packet = bytes([0xA5, 0x01, 0x05, 0x00]) + b'hello'
    uart.write(packet)
    response = uart.read(100)
    if b'hello' in response:
        print("  [OK] PING passed")
        tests_passed += 1
    else:
        print("  [FAIL] PING failed")
        tests_failed += 1

    # Test 2: Get Version
    print("Testing GET_VERSION...")
    packet = bytes([0xA5, 0x03, 0x00, 0x00])
    uart.write(packet)
    response = uart.read(10)
    if len(response) >= 10:
        version = f"{response[7]}.{response[8]}.{response[9]}"
        print(f"  [OK] GET_VERSION passed (v{version})")
        tests_passed += 1
    else:
        print("  [FAIL] GET_VERSION failed")
        tests_failed += 1

    # Test 3: WiFi Status
    print("Testing WIFI_STATUS...")
    packet = bytes([0xA5, 0x12, 0x00, 0x00])
    uart.write(packet)
    response = uart.read(20)
    if len(response) >= 7:
        print("  [OK] WIFI_STATUS passed")
        tests_passed += 1
    else:
        print("  [FAIL] WIFI_STATUS failed")
        tests_failed += 1

    uart.close()

    print(f"\nResults: {tests_passed} passed, {tests_failed} failed")
    return tests_failed == 0

if __name__ == "__main__":
    success = run_system_tests('/dev/ttyUSB0')
    sys.exit(0 if success else 1)
```

## Protocol Implementation

### ESP32 C Implementation

Key files in the firmware:
- **Protocol Definition**: `components/pynq_wifi_bridge/include/pynq_wifi_protocol.h`
- **Command Handlers**: `components/pynq_wifi_bridge/pynq_wifi_handler.c`
- **Protocol Functions**: `components/pynq_wifi_bridge/pynq_wifi_protocol.c`
- **UART Transport**: `components/pynq_wifi_bridge/pynq_wifi_transport.c`

### Adding New Commands

1. **Define command ID** in `pynq_wifi_protocol.h`
2. **Define payload structure** (if needed)
3. **Implement handler** in `pynq_wifi_handler.c`
4. **Add to switch statement** in `command_handler_process()`
5. **Write tests** in `components/pynq_wifi_bridge/test/`
6. **Document command** in this documentation

Example:
```c
// 1. Define command (pynq_wifi_protocol.h)
#define CMD_CUSTOM 0x50

typedef struct __attribute__((packed)) {
    uint8_t param1;
    uint16_t param2;
} custom_payload_t;

// 2. Implement handler (pynq_wifi_handler.c)
static void handle_cmd_custom(protocol_packet_t* packet) {
    custom_payload_t* payload = (custom_payload_t*)packet->payload;

    // Process command
    uint8_t result = process_custom_command(payload);

    protocol_send_response(k_status_ok, &result, sizeof(result));
}

// 3. Add to dispatcher
case CMD_CUSTOM:
    handle_cmd_custom(packet);
    break;
```

## Troubleshooting

### Common Issues

#### No Response from ESP32

**Symptoms**: Commands sent but no response received

**Solutions**:
1. Check UART connection: `ls /dev/ttyUSB*`
2. Verify baud rate: Must be 115200
3. Check permissions: `sudo chmod 666 /dev/ttyUSB0`
4. Test with PING command
5. Check ESP32 power and reset

#### Corrupted Data

**Symptoms**: Invalid start marker, wrong packet length

**Solutions**:
1. Clear UART buffers before sending
2. Add delay after opening UART (100ms)
3. Check for electrical interference
4. Verify cable quality
5. Reduce baud rate if persistent

#### Command Not Working

**Symptoms**: STATUS_INVALID_CMD or STATUS_ERROR

**Solutions**:
1. Check command ID is correct
2. Verify payload length matches specification
3. Check if command is implemented (see table above)
4. Review ESP32 UART logs for errors
5. Verify firmware version supports command

#### Update Failures

**Symptoms**: OTA update fails or times out

**Solutions**:
1. Ensure WiFi is connected first
2. Verify firmware URL is accessible
3. Check SHA256 hash is correct
4. Increase timeout for large files
5. Check server response and logs

### Debug Mode

Enable verbose logging:

```python
import serial
import logging

# Enable pyserial debug logging
logging.basicConfig(level=logging.DEBUG)

uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
uart.set_debug(True)
```

### ESP32 Logs

Monitor ESP32 UART output for detailed logs:

```bash
# On Linux
screen /dev/ttyUSB0 115200

# Or use minicom
minicom -D /dev/ttyUSB0 -b 115200

# Or Python
python -m serial.tools.miniterm /dev/ttyUSB0 115200
```

## Performance

### Throughput

Theoretical maximum at 115200 baud:
- **Raw**: 14,400 bytes/second
- **Effective**: ~10,000 bytes/second (with protocol overhead)

### Latency

Typical command latencies:
- **PING**: 5-10 ms
- **WiFi commands**: 100-2000 ms (depends on operation)
- **OTA update**: 1-5 minutes (depends on file size)

### Optimization

1. **Batch commands** when possible
2. **Use larger payloads** to reduce overhead
3. **Clear buffers** before critical operations
4. **Monitor timeouts** to detect issues early
5. **Reuse connections** instead of reopening

## Security Considerations

### Secure Connections

- Use **HTTPS** for OTA updates
- Always verify **SHA256 hashes** for firmware
- Use **WPA2/WPA3** for WiFi connections
- Avoid sending **passwords in logs**

### Production Recommendations

1. **Enable SHA256 verification** for all OTA updates
2. **Use HTTPS** for firmware downloads
3. **Implement authentication** if exposing commands over network
4. **Rate limit** commands to prevent abuse
5. **Monitor** for suspicious activity

## Additional Resources

### Related Documentation

- [Complete OTA Documentation](../ota/README.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [PySerial Documentation](https://pyserial.readthedocs.io/)

### Firmware Source Code

- **GitHub**: `/home/bsikar/Documents/git/STAR/esp32-firmware/`
- **Components**: `components/pynq_wifi_bridge/`
- **Tests**: `components/pynq_wifi_bridge/test/`

### Getting Help

1. Check this documentation first
2. Review ESP32 UART logs for errors
3. Test with provided Python scripts
4. Check GitHub issues
5. Review test files for examples

## Version Information

- **Protocol Version**: 1.0
- **Firmware Version**: 0.1.0
- **Documentation Version**: 2025-01-15

## License

This documentation and associated code are part of the STAR ESP32 firmware project.
