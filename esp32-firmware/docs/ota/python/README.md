# OTA Python Scripts

This directory contains Python scripts for interacting with the ESP32 OTA update system via UART.

## Requirements

```bash
pip install pyserial
```

## Scripts

### ota_send_update.py

Send an OTA update command to ESP32.

**Usage:**
```bash
python ota_send_update.py <serial_port> <url> [sha256] [allow_downgrade]
```

**Examples:**
```bash
# Basic update (no SHA256 verification)
python ota_send_update.py /dev/ttyUSB0 https://example.com/firmware-v0.2.0.bin

# Secure update with SHA256 verification
python ota_send_update.py /dev/ttyUSB0 https://example.com/firmware-v0.2.0.bin a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Version downgrade (rollback)
python ota_send_update.py /dev/ttyUSB0 https://example.com/firmware-v0.1.0.bin b4c7d9e1f3a5b7c9d1e3f5a7b9c1d3e5f7a9b1c3d5e7f9a1b3c5d7e9f1a3b5c true
```

### ota_get_status.py

Query the current OTA update status.

**Usage:**
```bash
python ota_get_status.py <serial_port>
```

**Example:**
```bash
python ota_get_status.py /dev/ttyUSB0
```

**Output:**
```
OTA Status:
  State: DOWNLOADING (2)
  Progress: 45%
  Downloaded: 447,500 / 995,000 bytes
```

### ota_monitor.py

Continuously monitor OTA update progress with a real-time display.

**Usage:**
```bash
python ota_monitor.py <serial_port> [timeout_seconds]
```

**Example:**
```bash
python ota_monitor.py /dev/ttyUSB0
python ota_monitor.py /dev/ttyUSB0 600  # 10 minute timeout
```

**Output:**
```
============================================================
OTA UPDATE MONITOR
============================================================

[CHECK] Checking for updates...

[DOWNLOAD] Starting download (995,000 bytes)...
   [####################--------------------] 50% (497,500/995,000 bytes)
```

### ota_complete_workflow.py

Complete end-to-end OTA update workflow with error handling.

**Usage:**
```bash
python ota_complete_workflow.py <serial_port> <url> [sha256] [allow_downgrade]
```

**Example:**
```bash
python ota_complete_workflow.py /dev/ttyUSB0 https://example.com/fw-v0.2.0.bin a3f5e8b1...
```

**Workflow:**
1. Send OTA update command
2. Monitor progress with real-time display
3. Parse error logs on failure
4. Wait for reboot
5. Verify new firmware version

## Module Usage

These scripts can also be imported as modules:

```python
import serial
from ota_send_update import send_ota_update
from ota_get_status import get_ota_status
from ota_monitor import monitor_ota_update

# Open UART
uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Send update
send_ota_update(uart, "https://example.com/fw.bin", sha256="a3f5...", allow_downgrade=False)

# Get status
status = get_ota_status(uart)
print(f"State: {status['state_name']}, Progress: {status['progress']}%")

# Monitor to completion
success = monitor_ota_update(uart, timeout_seconds=300)

uart.close()
```

## OTA States

| State | Value | Description |
|-------|-------|-------------|
| IDLE | 0 | No update in progress |
| CHECKING | 1 | Checking for updates (version check) |
| DOWNLOADING | 2 | Downloading firmware from server |
| VERIFYING | 3 | Verifying SHA256 hash |
| INSTALLING | 4 | Installing firmware to partition |
| COMPLETE | 5 | Update complete, will reboot soon |
| FAILED | 6 | Update failed (check logs) |

## Troubleshooting

### "Failed to get status"
- Check serial port permissions: `sudo chmod 666 /dev/ttyUSB0`
- Verify correct baud rate (115200)
- Ensure ESP32 is powered and running

### "Timeout after X seconds"
- Check WiFi connection on ESP32
- Verify firmware URL is accessible
- Increase timeout parameter

### "SHA256 verification failed"
- Verify hash is exactly 64 hex characters
- Generate correct hash: `sha256sum firmware.bin`
- Ensure firmware wasn't corrupted during upload to server

## See Also

- [OTA Documentation Index](../README.md)
- [Making OTA Requests](../02-making-requests.md)
- [Status Monitoring](../04-status-monitoring.md)
