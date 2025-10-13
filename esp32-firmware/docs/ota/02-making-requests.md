# Making OTA Update Requests from PYNQ

## Question: How do I make a request to update/downgrade the firmware version?

The ESP32 expects a binary protocol packet. Here's the complete implementation.

## Protocol Structure

From `pynq_wifi_protocol.h`, the packet format is:

```
+-------+--------+--------+--------+-----------+
| Start |  CMD   | Len_Lo | Len_Hi |  Payload  |
+-------+--------+--------+--------+-----------+
| 0xA5  | 0x29   | 2 bytes (LE)    | 322 bytes |
+-------+--------+--------+--------+-----------+
```

## Payload Structure (322 bytes total)

```c
typedef struct __attribute__((packed)) {
  char    url[256];        // Firmware URL (null-terminated)
  char    sha256[65];      // SHA256 hash (64 hex + null), or empty to skip
  uint8_t allow_downgrade; // 1 = allow downgrade, 0 = upgrade only
} ota_start_payload_t;
```

## Python Implementation

### Basic OTA Update Function

```python
import struct
import time

def send_ota_update(esp32_uart, url, sha256_hash=None, allow_downgrade=False):
    """
    Send OTA update command to ESP32

    Args:
        esp32_uart: UART connection to ESP32
        url: Firmware download URL (string, max 255 chars)
        sha256_hash: Optional SHA256 hash (64 hex characters), None to skip verification
        allow_downgrade: True to allow version downgrade (for rollbacks)

    Returns:
        bool: True if command was sent successfully
    """
    # Prepare URL field (256 bytes, null-terminated)
    url_bytes = url.encode('utf-8')[:255] + b'\x00'  # Null-terminate
    url_bytes = url_bytes.ljust(256, b'\x00')        # Pad to exactly 256 bytes

    # Prepare SHA256 field (65 bytes, null-terminated)
    if sha256_hash and len(sha256_hash) == 64:
        # Validate hex characters
        try:
            int(sha256_hash, 16)  # Verify it's valid hex
            sha256_bytes = sha256_hash.lower().encode('utf-8') + b'\x00'
            sha256_bytes = sha256_bytes.ljust(65, b'\x00')
        except ValueError:
            print(f"ERROR: Invalid SHA256 hash format: {sha256_hash}")
            return False
    else:
        sha256_bytes = b'\x00' * 65  # Empty = skip verification

    # Prepare downgrade flag (1 byte)
    downgrade_byte = struct.pack('B', 1 if allow_downgrade else 0)

    # Combine payload (total: 322 bytes)
    payload = url_bytes + sha256_bytes + downgrade_byte
    payload_len = len(payload)

    # Build packet
    packet = bytearray()
    packet.append(0xA5)  # Start marker
    packet.append(0x29)  # CMD_OTA_START_UPDATE
    packet.extend(struct.pack('<H', payload_len))  # Little-endian uint16
    packet.extend(payload)

    # Send to ESP32
    bytes_sent = esp32_uart.write(packet)

    # Log what was sent
    print(f"[OK] Sent OTA update command ({bytes_sent} bytes)")
    print(f"  URL: {url}")
    print(f"  SHA256: {sha256_hash if sha256_hash else 'SKIPPED (no verification)'}")
    print(f"  Allow downgrade: {allow_downgrade}")

    return bytes_sent == len(packet)
```

### Usage Examples

#### Example 1: Basic Update (No Hash Verification)

```python
import serial

# Open UART connection to ESP32
uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Send update command (no hash = no verification)
send_ota_update(
    uart,
    url="https://example.com/firmware-v0.2.0.bin",
    sha256_hash=None,          # No verification
    allow_downgrade=False      # Reject downgrades
)

print("Update initiated!")
```

#### Example 2: Secure Update (With SHA256 Verification)

```python
# Generate SHA256 on your server:
# $ sha256sum firmware-v0.2.0.bin
# a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d  firmware-v0.2.0.bin

send_ota_update(
    uart,
    url="https://example.com/firmware-v0.2.0.bin",
    sha256_hash="a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d",
    allow_downgrade=False
)

print("Secure update initiated with SHA256 verification!")
```

#### Example 3: Version Downgrade (Rollback)

```python
# Downgrade to previous version (e.g., after detecting bugs)
send_ota_update(
    uart,
    url="https://example.com/firmware-v0.1.0.bin",
    sha256_hash="b4c7d9e1f3a5b7c9d1e3f5a7b9c1d3e5f7a9b1c3d5e7f9a1b3c5d7e9f1a3b5c",
    allow_downgrade=True  # REQUIRED for older versions
)

print("Rollback initiated to v0.1.0")
```

## Complete OTA Update Workflow

### Full Implementation with Progress Monitoring

```python
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
    """Get current OTA status from ESP32"""
    # Send status request
    packet = bytearray([0xA5, 0x2A])  # Start + CMD_OTA_GET_STATUS
    packet.extend(struct.pack('<H', 0))  # No payload
    uart.write(packet)

    # Read response (Header + 10 bytes)
    response = uart.read(4 + 10)
    if len(response) < 14:
        return None

    # Parse response
    _, _, _, _ = struct.unpack('BBBB', response[:4])
    state, progress, bytes_downloaded, total_bytes = struct.unpack('<BBII', response[4:14])

    return {
        'state': OTAState(state),
        'progress': progress,
        'bytes_downloaded': bytes_downloaded,
        'total_bytes': total_bytes
    }

def monitor_ota_update(uart, timeout_seconds=300):
    """
    Monitor OTA update progress

    Args:
        uart: Serial connection to ESP32
        timeout_seconds: Maximum time to wait for update

    Returns:
        bool: True if update succeeded, False if failed
    """
    start_time = time.time()
    last_progress = -1

    print("\nMonitoring OTA update...")
    print("-" * 60)

    while True:
        # Check timeout
        if time.time() - start_time > timeout_seconds:
            print("[ERROR] Update timed out!")
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

        # Print progress updates
        if progress != last_progress or state != last_state if 'last_state' in locals() else None:
            if state == OTAState.DOWNLOADING:
                print(f"[DOWNLOAD] Downloading: {progress}% ({downloaded:,}/{total:,} bytes)")
            elif state == OTAState.VERIFYING:
                print(f"[CHECK] Verifying firmware integrity (SHA256)...")
            elif state == OTAState.INSTALLING:
                print(f"[INSTALL]  Installing firmware to flash...")
            last_progress = progress
            last_state = state

        # Check for completion
        if state == OTAState.COMPLETE:
            print("-" * 60)
            print("[SUCCESS] Update successful!")
            print("   ESP32 will reboot in 3 seconds...")
            return True

        # Check for failure
        elif state == OTAState.FAILED:
            print("-" * 60)
            print("[ERROR] Update failed!")
            print("   Check ESP32 logs for details")
            return False

        time.sleep(1)  # Poll every second

def perform_ota_update(uart_port, url, sha256=None, allow_downgrade=False):
    """
    Complete OTA update workflow

    Args:
        uart_port: Serial port device (e.g., '/dev/ttyUSB0')
        url: Firmware URL
        sha256: Optional SHA256 hash
        allow_downgrade: Allow version downgrade

    Returns:
        bool: True if update succeeded
    """
    # Open UART
    uart = serial.Serial(uart_port, 115200, timeout=1)

    try:
        # Send OTA command
        print(f"Initiating OTA update...")
        if not send_ota_update(uart, url, sha256, allow_downgrade):
            return False

        # Monitor progress
        success = monitor_ota_update(uart)

        if success:
            print("\n[WAIT] Waiting for ESP32 to reboot...")
            time.sleep(5)
            print("[OK] ESP32 should now be running new firmware!")

        return success

    finally:
        uart.close()

# Usage:
if __name__ == "__main__":
    success = perform_ota_update(
        uart_port='/dev/ttyUSB0',
        url='https://example.com/firmware-v0.2.0.bin',
        sha256='a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d',
        allow_downgrade=False
    )

    if success:
        print("[SUCCESS] OTA update completed successfully!")
    else:
        print("[ALERT] OTA update failed!")
```

## Testing the Implementation

```python
# Test 1: Check if command is properly formatted
def test_packet_format():
    import io
    uart = io.BytesIO()  # Mock UART

    send_ota_update(
        uart,
        url="https://test.com/fw.bin",
        sha256="a" * 64,
        allow_downgrade=True
    )

    packet = uart.getvalue()

    assert packet[0] == 0xA5, "Start marker incorrect"
    assert packet[1] == 0x29, "Command ID incorrect"
    assert len(packet) == 4 + 322, "Packet size incorrect"

    print("[OK] Packet format test passed!")

test_packet_format()
```

## Common Errors

### Error 1: Invalid SHA256 Hash
```python
# [ERROR] Wrong - not 64 characters
send_ota_update(uart, url="...", sha256="abc123")

# [SUCCESS] Correct - exactly 64 hex characters
send_ota_update(uart, url="...", sha256="a3f5..." * 8)  # 64 chars total
```

### Error 2: URL Too Long
```python
# [ERROR] Wrong - URL longer than 255 characters
url = "https://example.com/" + "a" * 300
send_ota_update(uart, url=url)  # Will be truncated!

# [SUCCESS] Correct - Keep URL under 255 characters
url = "https://example.com/firmware.bin"  # Much better
send_ota_update(uart, url=url)
```

### Error 3: Forgetting Downgrade Flag
```python
# [ERROR] Wrong - Trying to install v0.1.0 when running v0.2.0
send_ota_update(uart, url="https://.../v0.1.0.bin", allow_downgrade=False)
# ESP32 will reject this!

# [SUCCESS] Correct - Enable downgrade for rollbacks
send_ota_update(uart, url="https://.../v0.1.0.bin", allow_downgrade=True)
```

## Summary

- **Command ID**: 0x29 (CMD_OTA_START_UPDATE)
- **Payload Size**: 322 bytes fixed
- **URL**: Max 255 chars, null-terminated
- **SHA256**: Optional, exactly 64 hex chars or empty
- **Downgrade**: 0 = reject downgrades, 1 = allow downgrades
- **Response**: Poll with CMD_OTA_GET_STATUS (0x2A) to monitor progress
