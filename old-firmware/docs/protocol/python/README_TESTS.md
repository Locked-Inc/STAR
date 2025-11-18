# ESP32 Firmware Protocol Test Suite

Comprehensive test suite for ESP32 firmware communication protocol.

## Overview

This directory contains Python test scripts for all ESP32 protocol commands:
- **System Commands** - Basic system operations
- **WiFi Commands** - WiFi connectivity and management
- **HTTP Commands** - HTTP client functionality
- **TCP Commands** - TCP socket operations
- **OTA Commands** - Over-the-air firmware updates

## Prerequisites

- ESP32 connected via USB/UART
- Python 3.6+ with `pyserial` package:
  ```bash
  pip3 install pyserial
  ```

## Quick Start

### Run All Tests (Integration Test)
```bash
python test_all_commands.py /dev/ttyUSB0
```

### Run Individual Test Suites
```bash
# System commands
python system/test_system_commands.py /dev/ttyUSB0

# WiFi commands
python wifi/test_wifi_commands.py /dev/ttyUSB0

# HTTP commands (requires WiFi connection)
python http/test_http_commands.py /dev/ttyUSB0

# TCP commands (requires WiFi connection)
python tcp/test_tcp_commands.py /dev/ttyUSB0

# OTA commands (requires WiFi connection)
python ota/test_ota_commands.py /dev/ttyUSB0
```

## Test Suites

### 1. System Commands (`system/`)
Tests basic system operations:
- **CMD_PING** - Connectivity test with echo
- **CMD_GET_VERSION** - Firmware version query
- **CMD_RESET** - System reset (optional)

**Scripts:**
- `test_system_commands.py` - Complete test suite
- `ping.py` - Send ping with custom data
- `get_version.py` - Query firmware version
- `reset.py` - Reset ESP32

### 2. WiFi Commands (`wifi/`)
Tests WiFi functionality:
- **CMD_WIFI_STATUS** - Get connection status
- **CMD_WIFI_SCAN** - Scan for networks
- **CMD_WIFI_CONNECT** - Connect to WiFi
- **CMD_WIFI_DISCONNECT** - Disconnect from WiFi

**Scripts:**
- `test_wifi_commands.py` - Complete test suite
- `wifi_status.py` - Get WiFi status (with monitor mode)
- `wifi_scan.py` - Scan networks
- `wifi_connect.py` - Connect to WiFi
- `wifi_disconnect.py` - Disconnect
- `wifi_manager.py` - High-level WiFi manager class

### 3. HTTP Commands (`http/`)
Tests HTTP client operations:
- **CMD_HTTP_GET** - HTTP GET requests
- **CMD_HTTP_POST** - HTTP POST requests

**Tests include:**
- Small response handling
- JSON response parsing
- POST with data
- Error handling for invalid URLs

**Scripts:**
- `test_http_commands.py` - Complete test suite
- `http_get.py` - Simple HTTP GET
- `http_post.py` - HTTP POST with data

**Example:**
```bash
# HTTP GET
python http/http_get.py /dev/ttyUSB0 http://httpbin.org/json

# HTTP POST
python http/http_post.py /dev/ttyUSB0 http://httpbin.org/post '{"test":"data"}'
```

### 4. TCP Commands (`tcp/`)
Tests TCP socket operations:
- **CMD_TCP_CONNECT** - Open TCP connection
- **CMD_TCP_SEND** - Send/receive data
- **CMD_TCP_CLOSE** - Close connection

**Tests include:**
- Single connection
- Send/receive data
- Multiple concurrent connections (up to 4)
- Connection cleanup

**Scripts:**
- `test_tcp_commands.py` - Complete test suite
- `tcp_client.py` - TCP client with send/receive

**Example:**
```bash
python tcp/tcp_client.py /dev/ttyUSB0 httpbin.org 80 \
  "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n"
```

### 5. OTA Commands (`ota/`)
Tests over-the-air update functionality:
- **CMD_OTA_CHECK_UPDATE** - Check for updates
- **CMD_OTA_GET_STATUS** - Get update status
- **CMD_OTA_START_UPDATE** - Start update (with SHA256 verification)

**Tests include:**
- Version checking
- Update status monitoring
- SHA256 verification
- Error code validation
- Downgrade protection

**Scripts:**
- `test_ota_commands.py` - Complete test suite
- `ota_check.py` - Check for updates
- `ota_status.py` - Get/monitor update status

**Example:**
```bash
# Check for updates
python ota/ota_check.py /dev/ttyUSB0

# Monitor update progress
python ota/ota_status.py /dev/ttyUSB0 --monitor
```

## Integration Test

The comprehensive integration test (`test_all_commands.py`) runs all protocol commands in sequence:

```bash
python test_all_commands.py /dev/ttyUSB0
```

**Features:**
- Automatic test result tracking
- Success/failure summary
- WiFi connection detection
- Conditional test execution (skips network tests if WiFi not connected)

**Output:**
```
ESP32 COMPREHENSIVE INTEGRATION TEST SUITE
==========================================================

SYSTEM COMMANDS
==========================================================
[1] CMD_PING
  [OK] PASSED: PING with echo data
...

TEST SUMMARY
==========================================================
Total tests: 15
Passed: 14 [OK]
Failed: 1 [FAIL]
Success rate: 93%
```

## Test Coverage

### Protocol Command Coverage
- [SUCCESS] System: 3/3 commands (100%)
- [SUCCESS] WiFi: 4/4 commands (100%)
- [SUCCESS] HTTP: 2/2 commands (100%)
- [SUCCESS] TCP: 3/3 commands (100%)
- [SUCCESS] OTA: 3/3 commands (100%)

**Total: 15/15 commands implemented and tested (100%)**

### Error Code Testing
All error codes are tested:
- [SUCCESS] `k_status_ok` (0x00)
- [SUCCESS] `k_status_error` (0x01)
- [SUCCESS] `k_status_wifi_failed` (0x10)
- [SUCCESS] `k_status_http_failed` (0x20)
- [SUCCESS] `k_status_ota_failed` (0x30)
- [SUCCESS] `k_status_ota_wifi_disconnected` (0x32)
- [SUCCESS] `k_status_ota_sha256_mismatch` (0x33)
- [SUCCESS] `k_status_ota_version_downgrade` (0x34)
- [SUCCESS] `k_status_ota_partition_error` (0x35)
- [SUCCESS] `k_status_ota_http_error` (0x37)

## Test Scenarios

### Positive Tests
- All commands with valid inputs
- Expected response validation
- Data integrity checks
- Multiple concurrent operations

### Negative Tests
- Invalid URLs (HTTP)
- Network errors
- Connection failures
- SHA256 mismatch
- Version downgrade attempts

### Edge Cases
- Empty payloads
- Maximum payload sizes
- Concurrent connections
- Timeout handling

## Troubleshooting

### Common Issues

**1. "Permission denied" on serial port**
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add user to dialout group:
sudo usermod -a -G dialout $USER
```

**2. "WiFi not connected" errors**
First connect to WiFi:
```bash
python wifi/wifi_connect.py /dev/ttyUSB0 <SSID> <password>
```

**3. HTTP/TCP tests fail**
- Ensure ESP32 is connected to WiFi
- Check internet connectivity
- Verify firewall settings

**4. OTA tests fail**
- Ensure update server is accessible
- Verify firmware URL is valid
- Check SHA256 hash if using verification

## Serial Port Names

- **Linux:** `/dev/ttyUSB0`, `/dev/ttyACM0`
- **macOS:** `/dev/cu.usbserial-*`, `/dev/tty.usbserial-*`
- **Windows:** `COM3`, `COM4`, etc.

## Protocol Details

### Packet Structure
```
+-------+--------+--------+--------+-----------+
| Start |  CMD   | Len_Lo | Len_Hi |  Payload  |
+-------+--------+--------+--------+-----------+
| 0xA5  | 1 byte | 1 byte | 1 byte |  N bytes  |
+-------+--------+--------+--------+-----------+
```

### Response Structure
```
+-------+--------+--------+--------+--------+--------+-----------+
| Start |  CMD   | Len_Lo | Len_Hi | Status | DLen_L | DLen_H | Data |
+-------+--------+--------+--------+--------+--------+-----------+
| 0xA5  | 0xF0   | ...    | ...    | 1 byte | 2 bytes (optional) | ... |
+-------+--------+--------+--------+--------+--------+-----------+
```

## Contributing

When adding new tests:
1. Follow existing test structure
2. Include both positive and negative test cases
3. Test error handling
4. Update this README
5. Ensure all tests are executable (`chmod +x`)

## License

Part of the STAR ESP32 firmware project.
