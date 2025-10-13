# OTA Status Monitoring and Failure Detection

## Question: How will the PYNQ know if the ESP has been updated or if the update failed? And if the update failed, how would the PYNQ know what type of failure it was?

PYNQ monitors the update by polling the ESP32's status using the **CMD_OTA_GET_STATUS (0x2A)** command.

## Status Request/Response Protocol

### Sending Status Request

```python
import struct

def get_ota_status(esp32_uart):
    """
    Request current OTA status from ESP32

    Returns:
        dict with state, progress, bytes_downloaded, total_bytes
        or None if request failed
    """
    # Build status request packet
    packet = bytearray()
    packet.append(0xA5)  # Start marker
    packet.append(0x2A)  # CMD_OTA_GET_STATUS
    packet.extend(struct.pack('<H', 0))  # Payload length = 0

    # Send request
    esp32_uart.write(packet)

    # Read response (Header: 4 bytes + Payload: 10 bytes)
    response = esp32_uart.read(14)
    if len(response) < 14:
        return None

    # Parse header
    start, cmd, payload_len = struct.unpack('<BBH', response[:4])

    if start != 0xA5 or cmd != 0xF0:  # 0xF0 = CMD_RESPONSE
        return None

    # Parse ota_status_response_t (from pynq_wifi_protocol.h:160-165)
    # typedef struct __attribute__((packed)) {
    #   uint8_t  state;            /* OTA state (0-6) */
    #   uint8_t  progress;         /* Progress % (0-100) */
    #   uint32_t bytes_downloaded; /* Bytes downloaded */
    #   uint32_t total_bytes;      /* Total bytes */
    # } ota_status_response_t;

    state, progress, bytes_downloaded, total_bytes = struct.unpack('<BBII', response[4:14])

    return {
        'state': state,
        'progress': progress,
        'bytes_downloaded': bytes_downloaded,
        'total_bytes': total_bytes
    }
```

## OTA States

From `pynq_ota_manager.h:16-24`:

```python
from enum import IntEnum

class OTAState(IntEnum):
    IDLE = 0         # No update in progress
    CHECKING = 1     # Checking for updates (version check)
    DOWNLOADING = 2  # Downloading firmware from server
    VERIFYING = 3    # Verifying SHA256 hash
    INSTALLING = 4   # Installing firmware to partition
    COMPLETE = 5     # Update complete, will reboot soon
    FAILED = 6       # Update failed (check logs)
```

## Complete Monitoring Implementation

### Basic Progress Monitor

```python
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

def monitor_ota_update(esp32_uart, timeout_seconds=300):
    """
    Monitor OTA update progress with detailed status reporting

    Args:
        esp32_uart: Serial connection to ESP32
        timeout_seconds: Maximum time to wait (default: 5 minutes)

    Returns:
        bool: True if update succeeded, False if failed/timeout
    """
    start_time = time.time()
    last_progress = -1
    last_state = None
    stall_counter = 0

    print("\n" + "="*60)
    print("OTA UPDATE MONITOR")
    print("="*60)

    while True:
        # Check timeout
        elapsed = time.time() - start_time
        if elapsed > timeout_seconds:
            print(f"\n[ERROR] Timeout after {elapsed:.1f} seconds")
            return False

        # Get status
        status = get_ota_status(esp32_uart)
        if not status:
            print("[WARN]  Failed to get status, retrying...")
            time.sleep(0.5)
            continue

        state = OTAState(status['state'])
        progress = status['progress']
        downloaded = status['bytes_downloaded']
        total = status['total_bytes']

        # Detect state changes
        if state != last_state:
            print()  # New line for state change

            if state == OTAState.CHECKING:
                print("[CHECK] Checking for updates...")
            elif state == OTAState.DOWNLOADING:
                print(f"[DOWNLOAD] Starting download ({total:,} bytes)...")
            elif state == OTAState.VERIFYING:
                print("[VERIFY] Verifying firmware integrity (SHA256)...")
            elif state == OTAState.INSTALLING:
                print("[INSTALL]  Installing firmware to flash...")

            last_state = state

        # Show download progress
        if state == OTAState.DOWNLOADING:
            if progress != last_progress:
                bar_length = 40
                filled = int(bar_length * progress / 100)
                bar = '#' * filled + '-' * (bar_length - filled)

                print(f"\r   [{bar}] {progress}% ({downloaded:,}/{total:,} bytes)", end='', flush=True)
                last_progress = progress
                stall_counter = 0
            else:
                stall_counter += 1
                if stall_counter > 30:  # 30 seconds without progress
                    print(f"\n[WARN]  Download stalled at {progress}%")

        # Check for completion
        if state == OTAState.COMPLETE:
            print("\n")
            print("="*60)
            print("[SUCCESS] UPDATE SUCCESSFUL!")
            print("="*60)
            print("ESP32 will reboot in 3 seconds...")
            print("New firmware will be active after reboot.")
            return True

        # Check for failure
        elif state == OTAState.FAILED:
            print("\n")
            print("="*60)
            print("[ERROR] UPDATE FAILED!")
            print("="*60)
            print("ESP32 is still running old firmware.")
            print("Check ESP32 UART logs for detailed error.")
            return False

        time.sleep(1)  # Poll every second

# Usage example
if __name__ == "__main__":
    import serial

    uart = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

    # Start OTA update
    send_ota_update(uart, url="https://example.com/fw.bin", sha256="a3f5...")

    # Monitor progress
    success = monitor_ota_update(uart, timeout_seconds=300)

    if success:
        print("\n[WAIT] Waiting for reboot...")
        time.sleep(5)
        print("[OK] ESP32 should be running new firmware now!")
    else:
        print("\n[WARN]  Update unsuccessful. ESP32 still on old firmware.")
```

## Failure Detection Methods

### Method 1: Current Implementation (State-Based)

Currently, when `state == OTA_FAILED`, you know there was a failure, but not the specific reason.

```python
status = get_ota_status(uart)

if status['state'] == OTAState.FAILED:
    print("Update failed!")
    # But WHY did it fail? Need to check logs...
```

### Method 2: Parse ESP32 Logs (Recommended for Now)

The ESP32 outputs detailed error messages via UART:

```python
import re

def parse_ota_error_from_logs(uart):
    """
    Read ESP32 logs and detect OTA error messages

    Returns:
        dict with 'category' and 'message', or None
    """
    error_patterns = {
        'network': [
            (r'WiFi not connected', 'WiFi connection lost'),
            (r'HTTP GET request failed', 'Failed to connect to server'),
            (r'OTA begin failed', 'Could not start OTA download'),
        ],
        'download': [
            (r'OTA download failed', 'Download interrupted'),
            (r'OTA data incomplete', 'Downloaded data is incomplete'),
            (r'connection timeout', 'Server connection timed out'),
        ],
        'verification': [
            (r'SHA256 verification failed', 'Firmware hash mismatch'),
            (r'Expected: ([a-f0-9]+)', 'SHA256 mismatch (see logs for hashes)'),
            (r'Invalid firmware image', 'Downloaded file is not valid ESP32 firmware'),
        ],
        'installation': [
            (r'OTA finish failed', 'Could not finalize installation'),
            (r'Failed to mark partition bootable', 'Flash partition error'),
        ],
        'storage': [
            (r'No free OTA partition', 'Both OTA partitions in use'),
            (r'Flash write error', 'Flash memory hardware error'),
        ]
    }

    timeout = time.time() + 5  # Read logs for 5 seconds

    while time.time() < timeout:
        try:
            line = uart.readline().decode('utf-8', errors='ignore').strip()
            if not line or 'ota_manager' not in line.lower():
                continue

            # Check each category
            for category, patterns in error_patterns.items():
                for pattern, description in patterns:
                    if re.search(pattern, line, re.IGNORECASE):
                        return {
                            'category': category,
                            'description': description,
                            'raw_message': line
                        }
        except:
            pass

    return None

# Usage
status = get_ota_status(uart)
if status['state'] == OTAState.FAILED:
    error = parse_ota_error_from_logs(uart)
    if error:
        print(f"Failure category: {error['category']}")
        print(f"Description: {error['description']}")
        print(f"Raw log: {error['raw_message']}")
```

### Method 3: Enhanced Protocol (Future Enhancement)

**Recommendation**: Add an `error_code` field to the status response.

Modify `pynq_wifi_protocol.h`:

```c
typedef struct __attribute__((packed)) {
  uint8_t  state;
  uint8_t  progress;
  uint32_t bytes_downloaded;
  uint32_t total_bytes;
  uint8_t  error_code;  // NEW: Specific error when state == FAILED
} ota_status_response_t;

// Error codes
typedef enum {
  OTA_ERROR_NONE = 0,
  OTA_ERROR_WIFI_DISCONNECTED = 1,
  OTA_ERROR_HTTP_FAILED = 2,
  OTA_ERROR_DOWNLOAD_TIMEOUT = 3,
  OTA_ERROR_DOWNLOAD_INCOMPLETE = 4,
  OTA_ERROR_SHA256_MISMATCH = 5,
  OTA_ERROR_INVALID_IMAGE = 6,
  OTA_ERROR_FLASH_WRITE = 7,
  OTA_ERROR_NO_FREE_PARTITION = 8,
  OTA_ERROR_OUT_OF_MEMORY = 9,
} ota_error_code_t;
```

Then in Python:

```python
ERROR_MESSAGES = {
    0: "No error",
    1: "WiFi disconnected during update",
    2: "HTTP request failed (check URL and network)",
    3: "Download timeout (slow connection or large file)",
    4: "Download incomplete (connection lost)",
    5: "SHA256 hash mismatch (corrupted or wrong hash)",
    6: "Invalid firmware image (not valid ESP32 binary)",
    7: "Flash write error (hardware issue)",
    8: "No free OTA partition (internal error)",
    9: "Out of memory (firmware too large)",
}

RECOVERY_SUGGESTIONS = {
    1: "Check WiFi connection stability",
    2: "Verify server URL is correct and accessible",
    3: "Try again with better network connection",
    4: "Check server stability and retry",
    5: "Verify SHA256 hash is correct; firmware may be corrupted",
    6: "Ensure firmware binary is built for this ESP32 variant",
    7: "Hardware issue - may need to replace ESP32",
    8: "Contact developer - this should not happen",
    9: "Firmware is too large for partition (max 1024KB)",
}

def get_ota_status_enhanced(uart):
    """Get OTA status with error code support"""
    # ... send request ...

    # Parse with extra byte for error_code
    state, progress, bytes_downloaded, total_bytes, error_code = \
        struct.unpack('<BBIIB', response[4:15])

    return {
        'state': OTAState(state),
        'progress': progress,
        'bytes_downloaded': bytes_downloaded,
        'total_bytes': total_bytes,
        'error_code': error_code,
        'error_message': ERROR_MESSAGES.get(error_code, "Unknown error"),
        'recovery_suggestion': RECOVERY_SUGGESTIONS.get(error_code, "")
    }

# Usage
status = get_ota_status_enhanced(uart)
if status['state'] == OTAState.FAILED:
    print(f"[ERROR] Update failed: {status['error_message']}")
    print(f"[INFO] Suggestion: {status['recovery_suggestion']}")

    if status['error_code'] == 5:  # SHA256 mismatch
        print("\nPossible causes:")
        print("  1. Firmware was corrupted during download")
        print("  2. Wrong SHA256 hash was provided")
        print("  3. Server sent wrong file")
        print("  4. Network corruption (rare)")
```

## Complete Monitoring Workflow

```python
def complete_ota_workflow(uart_port, url, sha256=None):
    """
    Complete OTA update with comprehensive monitoring and error handling
    """
    import serial

    # Open connection
    uart = serial.Serial(uart_port, 115200, timeout=1)

    try:
        # Step 1: Send OTA command
        print("Step 1: Initiating OTA update...")
        if not send_ota_update(uart, url, sha256, allow_downgrade=False):
            print("[ERROR] Failed to send OTA command")
            return False

        # Step 2: Monitor progress
        print("\nStep 2: Monitoring update progress...")
        success = monitor_ota_update(uart, timeout_seconds=300)

        if not success:
            # Step 3: Parse error logs
            print("\nStep 3: Analyzing failure...")
            error = parse_ota_error_from_logs(uart)
            if error:
                print(f"\n[INFO] Error Details:")
                print(f"   Category: {error['category']}")
                print(f"   Description: {error['description']}")
                print(f"   Raw message: {error['raw_message']}")
            return False

        # Step 4: Wait for reboot
        print("\nStep 4: Waiting for ESP32 to reboot...")
        time.sleep(5)

        # Step 5: Verify new version (optional)
        print("\nStep 5: Verifying new firmware...")
        # Could send CMD_GET_VERSION here to confirm

        print("\n[SUCCESS] OTA update completed successfully!")
        return True

    finally:
        uart.close()

# Run it
success = complete_ota_workflow(
    uart_port='/dev/ttyUSB0',
    url='https://example.com/firmware-v0.2.0.bin',
    sha256='a3f5e8b1c2d4f6a89b3e5c7d9f1a3b5c7e9f1a3c5d7e9b1a3c5d7f9a1b3c5e7d'
)
```

## Summary

### How PYNQ Knows Update Status

1. **Poll with CMD_OTA_GET_STATUS (0x2A)** every 1 second
2. **Check state field**:
   - `DOWNLOADING` (2) = In progress
   - `VERIFYING` (3) = Checking hash
   - `INSTALLING` (4) = Writing to flash
   - `COMPLETE` (5) = Success!
   - `FAILED` (6) = Failed!

### How PYNQ Detects Failure Type

**Current method:**
- State becomes `FAILED`
- Parse ESP32 UART logs for error messages
- Match against known error patterns

**Recommended enhancement:**
- Add `error_code` field to status response
- Get specific failure reason directly
- No need to parse logs

### Common Failure Scenarios

| Error | Cause | Detection | Recovery |
|-------|-------|-----------|----------|
| Network | WiFi disconnected | State=FAILED + "WiFi not connected" in logs | Check WiFi, retry |
| Download | Server timeout | State=FAILED + "HTTP GET failed" in logs | Verify URL, retry |
| Hash | SHA256 mismatch | State=FAILED + "SHA256 verification failed" | Check hash, retry |
| Flash | Write error | State=FAILED + "Flash write error" | Hardware issue |

The ESP32 never becomes "bricked" - if update fails, it continues running the old firmware from ota_0!
