# OTA Update Commands

OTA (Over-The-Air) update commands provide firmware update capabilities for the ESP32. These commands allow the PYNQ board to check for updates, initiate firmware downloads, and monitor update progress.

## Command Summary

| Command ID | Name | Description |
|------------|------|-------------|
| 0x28 | CMD_OTA_CHECK_UPDATE | Check if firmware update is available |
| 0x29 | CMD_OTA_START_UPDATE | Start OTA update process |
| 0x2A | CMD_OTA_GET_STATUS | Get OTA update status and progress |

## OTA State Enumeration

| State | Value | Description |
|-------|-------|-------------|
| IDLE | 0 | No update in progress |
| CHECKING | 1 | Checking for updates (version check) |
| DOWNLOADING | 2 | Downloading firmware from server |
| VERIFYING | 3 | Verifying SHA256 hash |
| INSTALLING | 4 | Installing firmware to partition |
| COMPLETE | 5 | Update complete, will reboot soon |
| FAILED | 6 | Update failed (check logs) |

## CMD_OTA_CHECK_UPDATE (0x28)

Check if a firmware update is available by querying the configured version check URL.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x28 | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+----------+---------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Data    |
+-------+------+--------+--------+--------+----------+---------+
| 0xA5  | 0xF0 | 0x08   | 0x00   | 0x00   | 0x07,0x00| 7 bytes |
+-------+------+--------+--------+--------+----------+---------+
```

**Status**: `0x00` (STATUS_OK)
**Data**: `ota_check_response_t` structure (7 bytes)

#### Response Data Structure

```c
typedef struct __attribute__((packed)) {
  uint8_t update_available; // 1 if update available, 0 otherwise
  uint8_t current_version[3]; // major.minor.patch
  uint8_t new_version[3];     // major.minor.patch
} ota_check_response_t;
```

### Use Cases

1. **Periodic update checks** - Check for new firmware periodically
2. **Manual update initiation** - User-triggered update check
3. **Version verification** - Confirm current firmware version

### Python Implementation

```python
#!/usr/bin/env python3
"""Check for available firmware updates"""

import serial
import struct
import time

def check_for_update(uart):
    """
    Check if firmware update is available

    Args:
        uart: Serial connection to ESP32

    Returns:
        dict: {
            'update_available': bool,
            'current_version': (major, minor, patch),
            'new_version': (major, minor, patch)
        } or None on failure
    """
    # Create check update packet
    packet = bytes([0xA5, 0x28, 0x00, 0x00])

    print("Checking for firmware updates...")
    uart.write(packet)

    # Read response (header + status + data_len + 7 bytes)
    response = uart.read(14)

    if len(response) < 14:
        print(f"ERROR: Response too short ({len(response)} bytes)")
        return None

    # Parse header
    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response header")
        return None

    status = response[4]
    if status != 0x00:
        print(f"ERROR: Status = 0x{status:02X}")
        return None

    # Parse response data
    update_available = response[7] != 0
    current_version = (response[8], response[9], response[10])
    new_version = (response[11], response[12], response[13])

    return {
        'update_available': update_available,
        'current_version': current_version,
        'new_version': new_version
    }

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python check_update.py <serial_port>")
        print("\nExample:")
        print("  python check_update.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART
    uart = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.1)

    # Check for updates
    result = check_for_update(uart)

    if result:
        curr_ver = '.'.join(map(str, result['current_version']))
        new_ver = '.'.join(map(str, result['new_version']))

        print(f"\nCurrent Version: {curr_ver}")

        if result['update_available']:
            print(f"[OK] Update Available: {new_ver}")
            print("\nYou can update using:")
            print(f"  python ota_start_update.py {port} <firmware_url> [sha256]")
        else:
            print("[OK] Firmware is up to date")
    else:
        print("[FAIL] Failed to check for updates")
        sys.exit(1)

    uart.close()
```

### Example Usage

```bash
# Check for updates
python check_update.py /dev/ttyUSB0

# Output when update is available:
# Checking for firmware updates...
#
# Current Version: 0.1.0
# [OK] Update Available: 0.2.0
#
# You can update using:
#   python ota_start_update.py /dev/ttyUSB0 <firmware_url> [sha256]

# Output when up to date:
# Checking for firmware updates...
#
# Current Version: 0.2.0
# [OK] Firmware is up to date
```

## CMD_OTA_START_UPDATE (0x29)

Start the OTA update process. This command initiates a background firmware download with optional SHA256 verification and version downgrade support.

### Request Packet

```
+-------+------+--------+--------+-----------+
| Start | CMD  | Len_Lo | Len_Hi | Payload   |
+-------+------+--------+--------+-----------+
| 0xA5  | 0x29 | 0x42   | 0x01   | 322 bytes |
+-------+------+--------+--------+-----------+
```

**Payload**: `ota_start_payload_t` structure (322 bytes)

#### Payload Structure

```c
typedef struct __attribute__((packed)) {
  char    url[256];        // Update URL (null-terminated)
  char    sha256[65];      // Expected SHA256 hash (64 hex + null), or empty to skip
  uint8_t allow_downgrade; // 1 to allow version downgrade, 0 to enforce upgrade only
} ota_start_payload_t;
```

### Response Packet

```
+-------+------+--------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi | Status |
+-------+------+--------+--------+--------+
| 0xA5  | 0xF0 | 0x01   | 0x00   | 0x00   |
+-------+------+--------+--------+--------+
```

**Status**:
- `0x00` (STATUS_OK) - Update started successfully
- `0x30` (STATUS_OTA_FAILED) - Failed to start update
- `0x31` (STATUS_OTA_PROGRESS) - Update already in progress

**Note**: This command starts a background task. Use CMD_OTA_GET_STATUS to monitor progress.

### SHA256 Verification

If SHA256 hash is provided (65 bytes with 64 hex characters + null terminator):
- Firmware is downloaded to flash partition
- Hash is calculated from downloaded firmware
- Update proceeds only if hash matches
- Update is aborted if hash mismatches

If SHA256 is empty (all zeros or empty string):
- Firmware is downloaded without verification
- **Not recommended for production use**

### Version Downgrade

The `allow_downgrade` flag controls version policy:
- `0` (false): Only allow upgrades (new version > current version)
- `1` (true): Allow downgrades (any version accepted, enables rollback)

### Python Implementation

```python
#!/usr/bin/env python3
"""Start OTA firmware update"""

import serial
import struct
import time

def start_ota_update(uart, url, sha256_hash=None, allow_downgrade=False):
    """
    Start OTA firmware update

    Args:
        uart: Serial connection to ESP32
        url: Firmware download URL (max 255 chars)
        sha256_hash: Optional SHA256 hash (64 hex chars), None to skip verification
        allow_downgrade: True to allow version downgrade

    Returns:
        bool: True if update started successfully
    """
    # Prepare URL field (256 bytes)
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')

    # Prepare SHA256 field (65 bytes)
    if sha256_hash and len(sha256_hash) == 64:
        try:
            int(sha256_hash, 16)  # Validate hex
            sha256_bytes = sha256_hash.lower().encode('utf-8') + b'\x00'
            sha256_bytes = sha256_bytes.ljust(65, b'\x00')
        except ValueError:
            print(f"ERROR: Invalid SHA256 hash format")
            return False
    else:
        sha256_bytes = b'\x00' * 65

    # Prepare downgrade flag (1 byte)
    downgrade_byte = struct.pack('B', 1 if allow_downgrade else 0)

    # Build payload (322 bytes total)
    payload = url_bytes + sha256_bytes + downgrade_byte

    # Build packet
    packet = bytes([0xA5, 0x29])
    packet += struct.pack('<H', len(payload))
    packet += payload

    # Send command
    print(f"Starting OTA update...")
    print(f"  URL: {url}")
    if sha256_hash:
        print(f"  SHA256: {sha256_hash}")
        print(f"  Verification: ENABLED")
    else:
        print(f"  Verification: DISABLED (not recommended)")
    print(f"  Allow downgrade: {allow_downgrade}")

    uart.write(packet)

    # Read response
    response = uart.read(5)

    if len(response) < 5:
        print("ERROR: No response")
        return False

    if response[0] != 0xA5 or response[1] != 0xF0:
        print("ERROR: Invalid response")
        return False

    status = response[4]

    if status == 0x00:
        print("[OK] OTA update started successfully")
        print("\nMonitor progress with:")
        print("  python ota_get_status.py <serial_port>")
        return True
    elif status == 0x31:
        print("[WARN]  Update already in progress")
        return False
    else:
        print(f"[FAIL] Failed to start update (status: 0x{status:02X})")
        return False

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python start_update.py <serial_port> <url> [sha256] [allow_downgrade]")
        print("\nExamples:")
        print("  # Basic update (no verification)")
        print("  python start_update.py /dev/ttyUSB0 https://example.com/fw.bin")
        print()
        print("  # Secure update with SHA256")
        print("  python start_update.py /dev/ttyUSB0 https://example.com/fw.bin a3f5e8b1...")
        print()
        print("  # Version downgrade (rollback)")
        print("  python start_update.py /dev/ttyUSB0 https://example.com/old-fw.bin a3f5... true")
        sys.exit(1)

    port = sys.argv[1]
    url = sys.argv[2]
    sha256 = sys.argv[3] if len(sys.argv) > 3 else None
    allow_downgrade = (len(sys.argv) > 4 and sys.argv[4].lower() == 'true')

    # Open UART
    uart = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.1)

    # Start update
    success = start_ota_update(uart, url, sha256, allow_downgrade)

    uart.close()

    sys.exit(0 if success else 1)
```

### Example Usage

```bash
# Basic update without verification
python start_update.py /dev/ttyUSB0 https://example.com/firmware-v0.2.0.bin

# Secure update with SHA256 verification
python start_update.py /dev/ttyUSB0 https://example.com/firmware-v0.2.0.bin \
  a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Version downgrade (rollback to previous version)
python start_update.py /dev/ttyUSB0 https://example.com/firmware-v0.1.0.bin \
  b4c7d9e1f3a5b7c9d1e3f5a7b9c1d3e5f7a9b1c3d5e7f9a1b3c5d7e9f1a3b5c true
```

## CMD_OTA_GET_STATUS (0x2A)

Get the current OTA update status including state, progress percentage, and download statistics.

### Request Packet

```
+-------+------+--------+--------+
| Start | CMD  | Len_Lo | Len_Hi |
+-------+------+--------+--------+
| 0xA5  | 0x2A | 0x00   | 0x00   |
+-------+------+--------+--------+
```

**Payload**: None

### Response Packet

```
+-------+------+--------+--------+--------+----------+----------+
| Start | CMD  | Len_Lo | Len_Hi | Status | Data_Len | Data     |
+-------+------+--------+--------+--------+----------+----------+
| 0xA5  | 0xF0 | 0x0D   | 0x00   | 0x00   | 0x0A,0x00| 10 bytes |
+-------+------+--------+--------+--------+----------+----------+
```

**Status**: `0x00` (STATUS_OK)
**Data**: `ota_status_response_t` structure (10 bytes)

#### Response Data Structure

```c
typedef struct __attribute__((packed)) {
  uint8_t  state;            // OTA state (0-6)
  uint8_t  progress;         // Progress % (0-100)
  uint32_t bytes_downloaded; // Bytes downloaded so far
  uint32_t total_bytes;      // Total bytes to download
} ota_status_response_t;
```

### Use Cases

1. **Progress monitoring** - Display download progress to user
2. **Completion detection** - Wait for update to complete
3. **Failure detection** - Detect and handle update failures
4. **Dashboard integration** - Real-time status in web UI

### Python Implementation

```python
#!/usr/bin/env python3
"""Get OTA update status"""

import serial
import struct
import time
from enum import IntEnum

class OTAState(IntEnum):
    IDLE = 0
    CHECKING = 1
    DOWNLOADING = 2
    VERIFYING = 3
    INSTALLING = 4
    COMPLETE = 5
    FAILED = 6

def get_ota_status(uart):
    """
    Get current OTA status

    Args:
        uart: Serial connection to ESP32

    Returns:
        dict: {
            'state': OTAState,
            'state_name': str,
            'progress': int (0-100),
            'bytes_downloaded': int,
            'total_bytes': int
        } or None on failure
    """
    # Create status request packet
    packet = bytes([0xA5, 0x2A, 0x00, 0x00])

    uart.write(packet)

    # Read response (header + status + data_len + 10 bytes)
    response = uart.read(17)

    if len(response) < 17:
        return None

    # Parse header
    if response[0] != 0xA5 or response[1] != 0xF0:
        return None

    status = response[4]
    if status != 0x00:
        return None

    # Parse status data
    state, progress, bytes_downloaded, total_bytes = struct.unpack(
        '<BBII', response[7:17]
    )

    return {
        'state': OTAState(state),
        'state_name': OTAState(state).name,
        'progress': progress,
        'bytes_downloaded': bytes_downloaded,
        'total_bytes': total_bytes
    }

def monitor_ota_progress(uart, timeout=300):
    """
    Monitor OTA progress until completion

    Args:
        uart: Serial connection
        timeout: Maximum time to wait (seconds)

    Returns:
        bool: True if update completed successfully
    """
    start_time = time.time()
    last_progress = -1
    last_state = None

    print("\n" + "="*60)
    print("OTA UPDATE MONITOR")
    print("="*60)

    while True:
        # Check timeout
        if time.time() - start_time > timeout:
            print(f"\n[FAIL] Timeout after {timeout} seconds")
            return False

        # Get status
        status = get_ota_status(uart)
        if not status:
            time.sleep(0.5)
            continue

        state = status['state']
        progress = status['progress']
        downloaded = status['bytes_downloaded']
        total = status['total_bytes']

        # Detect state changes
        if state != last_state:
            print()
            if state == OTAState.CHECKING:
                print("[CHECK] Checking for updates...")
            elif state == OTAState.DOWNLOADING:
                print(f"[DOWNLOAD] Downloading ({total:,} bytes)...")
            elif state == OTAState.VERIFYING:
                print("[VERIFY] Verifying SHA256 hash...")
            elif state == OTAState.INSTALLING:
                print("[INSTALL]  Installing firmware...")
            last_state = state

        # Show download progress
        if state == OTAState.DOWNLOADING and progress != last_progress:
            bar_length = 40
            filled = int(bar_length * progress / 100)
            bar = '#' * filled + '-' * (bar_length - filled)
            print(f"\r   [{bar}] {progress}% ({downloaded:,}/{total:,})",
                  end='', flush=True)
            last_progress = progress

        # Check for completion
        if state == OTAState.COMPLETE:
            print("\n\n" + "="*60)
            print("[SUCCESS] UPDATE SUCCESSFUL!")
            print("="*60)
            print("ESP32 will reboot in 3 seconds...")
            return True

        # Check for failure
        if state == OTAState.FAILED:
            print("\n\n" + "="*60)
            print("[ERROR] UPDATE FAILED!")
            print("="*60)
            print("Check ESP32 UART logs for error details")
            return False

        time.sleep(1)

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python get_status.py <serial_port> [--monitor]")
        print("\nExamples:")
        print("  # Get current status once")
        print("  python get_status.py /dev/ttyUSB0")
        print()
        print("  # Monitor progress until completion")
        print("  python get_status.py /dev/ttyUSB0 --monitor")
        sys.exit(1)

    port = sys.argv[1]
    monitor = '--monitor' in sys.argv

    # Open UART
    uart = serial.Serial(port, 115200, timeout=2)
    time.sleep(0.1)

    if monitor:
        # Monitor until completion
        success = monitor_ota_progress(uart)
        sys.exit(0 if success else 1)
    else:
        # Get status once
        status = get_ota_status(uart)

        if status:
            print(f"OTA Status:")
            print(f"  State: {status['state_name']} ({status['state']})")
            print(f"  Progress: {status['progress']}%")
            print(f"  Downloaded: {status['bytes_downloaded']:,} / {status['total_bytes']:,} bytes")
        else:
            print("Failed to get OTA status")
            sys.exit(1)

    uart.close()
```

### Example Usage

```bash
# Get current status (one-time)
python get_status.py /dev/ttyUSB0

# Output:
# OTA Status:
#   State: DOWNLOADING (2)
#   Progress: 45%
#   Downloaded: 447,500 / 995,000 bytes

# Monitor progress until completion
python get_status.py /dev/ttyUSB0 --monitor

# Output:
# ============================================================
# OTA UPDATE MONITOR
# ============================================================
#
# [DOWNLOAD] Downloading (995,000 bytes)...
#    [####################--------------------] 50% (497,500/995,000)
```

## Complete OTA Workflow

For a complete end-to-end OTA update workflow, see:
- [OTA Documentation](../ota/README.md)
- [Python OTA Scripts](../ota/python/)

### Quick Workflow Example

```bash
# Step 1: Check for updates
python check_update.py /dev/ttyUSB0

# Step 2: Start update with SHA256 verification
python start_update.py /dev/ttyUSB0 \
  https://example.com/firmware-v0.2.0.bin \
  a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d

# Step 3: Monitor progress
python get_status.py /dev/ttyUSB0 --monitor

# Result: ESP32 reboots with new firmware
```

## Error Handling

### Common Errors

| Error | Status | Cause | Solution |
|-------|--------|-------|----------|
| Update already in progress | 0x31 | Another update is running | Wait for current update to finish |
| Failed to start | 0x30 | WiFi not connected | Check WiFi connection first |
| Download timeout | State: FAILED | Slow network or large file | Retry with better connection |
| SHA256 mismatch | State: FAILED | Corrupted firmware or wrong hash | Verify hash and retry |
| Version rejection | State: FAILED | Downgrade not allowed | Use allow_downgrade=true |

### Recovery Strategies

1. **Check WiFi status before update**:
   ```python
   # Verify WiFi is connected
   status = wifi_get_status(uart)
   if status['status'] != 2:  # CONNECTED
       print("ERROR: WiFi not connected")
       wifi_connect(uart, ssid, password)
   ```

2. **Retry on failure**:
   ```python
   for attempt in range(3):
       if start_ota_update(uart, url, sha256):
           break
       print(f"Retry {attempt+1}/3...")
       time.sleep(5)
   ```

3. **Automatic rollback on failure**:
   - ESP32 bootloader automatically reverts after 3 failed boot attempts
   - Old firmware remains intact until new firmware boots successfully
   - No risk of "bricking" the device

## Security Considerations

### SHA256 Verification (Recommended)

Always use SHA256 verification in production:

```bash
# Generate hash on server
sha256sum firmware-v0.2.0.bin

# Use hash in update command
python start_update.py /dev/ttyUSB0 https://example.com/fw.bin <hash>
```

### Why SHA256 is Critical

- **Prevents corrupted updates**: Detects transmission errors
- **Prevents malicious updates**: Attacker can't inject fake firmware
- **Ensures authenticity**: Only authorized firmware can be installed

### Without SHA256

- Firmware is installed without verification
- Corrupted downloads may brick the device
- Man-in-the-middle attacks possible
- **Only acceptable for testing/development**

## Performance

### Typical Update Times

| Firmware Size | Download Time (WiFi) | Verification | Total Time |
|---------------|---------------------|--------------|------------|
| 500 KB | 30-60 seconds | 5 seconds | ~1 minute |
| 1 MB | 60-120 seconds | 10 seconds | ~2 minutes |
| 2 MB | 120-240 seconds | 20 seconds | ~4 minutes |

**Note**: Times vary based on WiFi signal strength and server speed.

### Optimization Tips

1. **Use local server** for faster downloads
2. **Pre-calculate SHA256** to avoid delays
3. **Monitor progress** to detect stalls
4. **Use auto-reboot** to minimize downtime

## See Also

- [Protocol Overview](01-overview.md)
- [Complete OTA Documentation](../ota/README.md)
- [OTA Python Scripts](../ota/python/)
- [OTA Implementation Details](../ota/01-overview.md)
- [SHA256 Verification Deep Dive](../ota/03-sha256-verification.md)
- [Status Monitoring Guide](../ota/04-status-monitoring.md)
