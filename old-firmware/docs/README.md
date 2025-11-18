# ESP32 Firmware Documentation

Complete documentation for the STAR ESP32 firmware project, including communication protocols, OTA updates, WiFi management, and more.

## [DOCS] Documentation Structure

### [Protocol Documentation](protocol/README.md)
**PYNQ-ESP32 Communication Protocol**

Complete guide to the binary UART protocol for PYNQ-ESP32 communication:
- System commands (PING, RESET, GET_VERSION)
- WiFi commands (CONNECT, DISCONNECT, STATUS, SCAN)
- Network commands (HTTP GET/POST, TCP operations)
- OTA update commands
- UART transport layer
- Python implementations and examples
- Troubleshooting guides

### [OTA Documentation](ota/README.md)
**Over-The-Air Firmware Updates**

Comprehensive OTA update system documentation:
- Complete OTA implementation overview
- Making OTA requests from PYNQ
- SHA256 verification deep dive
- Status monitoring and failure detection
- Auto-update system
- Python scripts for OTA operations
- Security considerations

## [QUICKSTART] Quick Start

### Test ESP32 Connectivity

```bash
# Test UART communication
python docs/protocol/python/system/ping.py /dev/ttyUSB0

# Get firmware version
python docs/protocol/python/system/get_version.py /dev/ttyUSB0
```

**Expected Output:**
```
Sending PING with 12 bytes: b'Hello ESP32!'
[OK] PING successful! Echo: b'Hello ESP32!'

Requesting firmware version...
[OK] ESP32 Firmware Version: 0.1.0
```

### Connect to WiFi

```bash
# Scan for networks
python docs/protocol/python/wifi/wifi_scan.py /dev/ttyUSB0

# Connect to your network
python docs/protocol/python/wifi/wifi_connect.py /dev/ttyUSB0 "YourSSID" "YourPassword"

# Check connection status
python docs/protocol/python/wifi/wifi_status.py /dev/ttyUSB0
```

### Update Firmware

```bash
# Start OTA update with SHA256 verification
python docs/ota/python/ota_send_update.py /dev/ttyUSB0 \
  https://example.com/firmware-v0.2.0.bin \
  a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Monitor progress
python docs/ota/python/ota_monitor.py /dev/ttyUSB0
```

## [GUIDE] Documentation by Topic

### Communication Protocol

| Topic | Description | Link |
|-------|-------------|------|
| Protocol Overview | Packet format, command structure, response handling | [01-overview.md](protocol/01-overview.md) |
| System Commands | PING, RESET, GET_VERSION | [02-system-commands.md](protocol/02-system-commands.md) |
| WiFi Commands | CONNECT, DISCONNECT, STATUS, SCAN | [03-wifi-commands.md](protocol/03-wifi-commands.md) |
| Network Commands | HTTP GET/POST, TCP operations | [04-network-commands.md](protocol/04-network-commands.md) |
| OTA Commands | CHECK_UPDATE, START_UPDATE, GET_STATUS | [05-ota-commands.md](protocol/05-ota-commands.md) |
| UART Transport | Physical layer, setup, troubleshooting | [06-uart-transport.md](protocol/06-uart-transport.md) |

### OTA Updates

| Topic | Description | Link |
|-------|-------------|------|
| OTA Overview | Architecture, update process, safety features | [ota/01-overview.md](ota/01-overview.md) |
| Making Requests | Protocol structure, Python implementations | [ota/02-making-requests.md](ota/02-making-requests.md) |
| SHA256 Verification | Hash verification, flash architecture, memory usage | [ota/03-sha256-verification.md](ota/03-sha256-verification.md) |
| Status Monitoring | Progress tracking, failure detection, error handling | [ota/04-status-monitoring.md](ota/04-status-monitoring.md) |
| Auto-Update | Automatic update system, version API, security | [ota/05-auto-update.md](ota/05-auto-update.md) |

### Python Scripts

| Category | Scripts | Location |
|----------|---------|----------|
| System | ping, reset, get_version | `protocol/python/system/` |
| WiFi | connect, disconnect, status, scan | `protocol/python/wifi/` |
| OTA | send_update, get_status, monitor, complete_workflow | `ota/python/` |

## [TASKS] Common Tasks

### Task 1: Initial ESP32 Setup

```bash
# 1. Test connectivity
python docs/protocol/python/system/ping.py /dev/ttyUSB0

# 2. Check firmware version
python docs/protocol/python/system/get_version.py /dev/ttyUSB0

# 3. Connect to WiFi
python docs/protocol/python/wifi/wifi_connect.py /dev/ttyUSB0 "MyWiFi" "password"

# 4. Verify WiFi connection
python docs/protocol/python/wifi/wifi_status.py /dev/ttyUSB0
```

### Task 2: Firmware Update

```bash
# Complete OTA workflow with monitoring
python docs/ota/python/ota_complete_workflow.py /dev/ttyUSB0 \
  https://example.com/firmware-v0.2.0.bin \
  a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d
```

### Task 3: Rollback to Previous Version

```bash
# Downgrade firmware (requires allow_downgrade=true)
python docs/ota/python/ota_send_update.py /dev/ttyUSB0 \
  https://example.com/firmware-v0.1.0.bin \
  b4c7d9e1f3a5b7c9d1e3f5a7b9c1d3e5f7a9b1c3d5e7f9a1b3c5d7e9f1a3b5c \
  true
```

### Task 4: Network Scanning and Connection

```bash
# Scan for available networks
python docs/protocol/python/wifi/wifi_scan.py /dev/ttyUSB0

# Connect to strongest network
python docs/protocol/python/wifi/wifi_connect.py /dev/ttyUSB0 "BestNetwork" "password"
```

### Task 5: System Health Check

```python
#!/usr/bin/env python3
"""ESP32 health check script"""

import serial
import time

def health_check(port):
    uart = serial.Serial(port, 115200, timeout=2)
    results = {}

    # Test 1: Connectivity
    print("Testing connectivity...")
    packet = bytes([0xA5, 0x01, 0x00, 0x00])
    uart.write(packet)
    response = uart.read(5)
    results['connectivity'] = len(response) >= 5 and response[0] == 0xA5

    # Test 2: Firmware version
    print("Checking firmware version...")
    packet = bytes([0xA5, 0x03, 0x00, 0x00])
    uart.write(packet)
    response = uart.read(10)
    if len(response) >= 10:
        results['version'] = f"{response[7]}.{response[8]}.{response[9]}"
    else:
        results['version'] = "Unknown"

    # Test 3: WiFi status
    print("Checking WiFi status...")
    packet = bytes([0xA5, 0x12, 0x00, 0x00])
    uart.write(packet)
    response = uart.read(20)
    if len(response) >= 8:
        status = response[7]
        results['wifi'] = ['Disconnected', 'Connecting', 'Connected', 'Failed'][status]
    else:
        results['wifi'] = "Unknown"

    uart.close()

    # Print results
    print("\n" + "="*50)
    print("ESP32 HEALTH CHECK RESULTS")
    print("="*50)
    print(f"Connectivity: {'[OK] OK' if results['connectivity'] else '[FAIL] FAILED'}")
    print(f"Firmware: {results.get('version', 'Unknown')}")
    print(f"WiFi: {results.get('wifi', 'Unknown')}")
    print("="*50)

    return all([results['connectivity'], results.get('version') != 'Unknown'])

if __name__ == "__main__":
    import sys
    success = health_check(sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0')
    sys.exit(0 if success else 1)
```

## [CONFIG] Configuration

### UART Settings

| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | None |

### ESP32 Pins

#### ESP32-WROOM-32
- **TX**: GPIO1 (U0TXD)
- **RX**: GPIO3 (U0RXD)

#### ESP32-S3
- **TX**: GPIO43 (U0TXD)
- **RX**: GPIO44 (U0RXD)

### Kconfig Options

Configure via `idf.py menuconfig`:

```
STAR WiFi Bridge Configuration
+-> OTA Update Configuration
|   +-> OTA Update URL
|   +-> OTA Version Check URL
|   +-> [*] Enable Automatic OTA Updates
|   +-> OTA Update Check Interval (ms)
|   +-> [*] Automatically Reboot After Update
|   +-> [ ] Enable HTTPS Certificate Verification
+-> WiFi Configuration
    +-> Default SSID
    +-> Default Password
```

## [DEBUG] Troubleshooting

### Problem: Can't connect to ESP32

**Symptoms**: `No such file or directory: '/dev/ttyUSB0'`

**Solutions**:
```bash
# Find the correct device
ls /dev/ttyUSB* /dev/ttyACM*

# Fix permissions
sudo chmod 666 /dev/ttyUSB0

# Add user to dialout group (permanent fix)
sudo usermod -a -G dialout $USER
# Log out and back in for this to take effect
```

### Problem: Commands not working

**Symptoms**: No response or error responses

**Solutions**:
1. Verify baud rate is 115200
2. Clear UART buffers before sending
3. Check ESP32 is running (look for LED activity)
4. Reset ESP32: `python docs/protocol/python/system/reset.py /dev/ttyUSB0`
5. Monitor ESP32 logs: `python -m serial.tools.miniterm /dev/ttyUSB0 115200`

### Problem: WiFi won't connect

**Symptoms**: Connection fails or times out

**Solutions**:
1. Verify SSID and password are correct
2. Check WiFi is in range: `python docs/protocol/python/wifi/wifi_scan.py /dev/ttyUSB0`
3. Ensure network uses WPA2/WPA3 (not WEP or enterprise)
4. Check for special characters in password
5. Try 2.4GHz network (ESP32 doesn't support 5GHz)

### Problem: OTA update fails

**Symptoms**: Update fails with STATE_FAILED

**Solutions**:
1. Ensure WiFi is connected before starting update
2. Verify firmware URL is accessible
3. Check SHA256 hash matches firmware
4. Increase timeout for large files
5. Check server supports HTTP/HTTPS properly
6. Review ESP32 logs for specific error

### Problem: Corrupted data

**Symptoms**: Invalid packets, wrong data

**Solutions**:
```python
# Clear buffers before critical operations
uart.reset_input_buffer()
uart.reset_output_buffer()

# Add delay after opening port
import time
uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(0.1)

# Verify packet integrity
if data[0] != 0xA5:
    print("Invalid start marker - resync needed")
```

## [STATS] Protocol Command Reference

### Quick Reference Table

| Command | ID | Payload | Response | Status |
|---------|----|----|----------|--------|
| PING | 0x01 | Optional data | Echo data | [x] |
| RESET | 0x02 | None | OK | [x] |
| GET_VERSION | 0x03 | None | Version (3B) | [x] |
| WIFI_CONNECT | 0x10 | SSID, Password | OK/Error | [x] |
| WIFI_DISCONNECT | 0x11 | None | OK | [x] |
| WIFI_STATUS | 0x12 | None | Status, IP, RSSI | [x] |
| WIFI_SCAN | 0x13 | None | Network list | [x] |
| HTTP_GET | 0x20 | URL | Response data | [WARN] Not implemented |
| HTTP_POST | 0x21 | URL, Data | Response data | [WARN] Not implemented |
| TCP_CONNECT | 0x22 | Host, Port | OK/Error | [WARN] Not implemented |
| TCP_SEND | 0x23 | Data | OK/Error | [WARN] Not implemented |
| TCP_CLOSE | 0x24 | None | OK | [WARN] Not implemented |
| OTA_CHECK_UPDATE | 0x28 | None | Available?, Versions | [x] |
| OTA_START_UPDATE | 0x29 | URL, SHA256, Flags | OK/Error | [x] |
| OTA_GET_STATUS | 0x2A | None | State, Progress | [x] |

## [SECURITY] Security Best Practices

### Production Deployment

1. **Always use SHA256 verification for OTA updates**
   ```bash
   # Generate hash
   sha256sum firmware.bin

   # Use in update command
   python docs/ota/python/ota_send_update.py /dev/ttyUSB0 \
     https://secure-server.com/firmware.bin <sha256_hash>
   ```

2. **Use HTTPS for firmware downloads**
   - Enable certificate verification in Kconfig
   - Embed CA certificate in firmware

3. **Secure WiFi credentials**
   - Use WPA2/WPA3
   - Don't log passwords
   - Clear credentials from memory after use

4. **Implement authentication**
   - Add authentication layer if exposing over network
   - Use token-based auth for API access

5. **Rate limiting**
   - Limit command frequency to prevent abuse
   - Implement exponential backoff for retries

## [METRICS] Performance Metrics

### UART Throughput
- **Theoretical Maximum**: 14,400 bytes/sec @ 115200 baud
- **Effective Throughput**: ~10,000 bytes/sec (with protocol overhead)

### Command Latencies
- **PING**: 5-10 ms
- **GET_VERSION**: 10-20 ms
- **WIFI_STATUS**: 20-50 ms
- **WIFI_SCAN**: 5-10 seconds
- **WIFI_CONNECT**: 2-10 seconds
- **OTA_START_UPDATE**: 50-100 ms (to start background task)

### OTA Update Times
| Firmware Size | Download | Verify | Install | Total |
|---------------|----------|--------|---------|-------|
| 500 KB | 30-60s | 5s | 10s | ~1 min |
| 1 MB | 60-120s | 10s | 15s | ~2 min |
| 2 MB | 120-240s | 20s | 20s | ~4 min |

## [DEV] Development

### Building Firmware

```bash
cd esp32-firmware

# Configure
idf.py menuconfig

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor
```

### Running Tests

```bash
# Run all tests
idf.py test

# Run specific test component
cd components/star_wifi_bridge
idf.py test

# Run Python integration tests
python docs/protocol/python/system/test_system_commands.py /dev/ttyUSB0
python docs/protocol/python/wifi/test_wifi_commands.py /dev/ttyUSB0
```

### Adding New Commands

See [Protocol Overview - Protocol Extension](protocol/01-overview.md#protocol-extension)

## [NOTES] Additional Resources

### Documentation Files
- [OTA_IMPLEMENTATION.md](../OTA_IMPLEMENTATION.md) - Original OTA implementation notes
- [OTA_IMPROVEMENTS.md](../OTA_IMPROVEMENTS.md) - OTA enhancements summary
- [PARTITIONS.md](../PARTITIONS.md) - Flash partition layout

### Source Code
- **Protocol**: `components/star_wifi_bridge/include/pynq_wifi_protocol.h`
- **Handlers**: `components/star_wifi_bridge/pynq_wifi_handler.c`
- **OTA Manager**: `components/star_wifi_bridge/pynq_ota_manager.c`
- **WiFi Manager**: `components/star_wifi_bridge/pynq_wifi_manager.c`
- **Tests**: `components/star_wifi_bridge/test/`

### External Resources
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [PySerial Documentation](https://pyserial.readthedocs.io/)

## [INFO] Version Information

- **Firmware Version**: 0.1.0
- **Protocol Version**: 1.0
- **Documentation Date**: 2025-01-15

## [FILE] License

This documentation and associated code are part of the STAR ESP32 firmware project.

---

**Need Help?**
1. Check the troubleshooting sections
2. Review ESP32 UART logs
3. Test with provided Python scripts
4. Check source code examples
5. Review test files for usage patterns
