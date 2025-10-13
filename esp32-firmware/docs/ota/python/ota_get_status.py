#!/usr/bin/env python3
# esp32-firmware/docs/ota/python/ota_get_status.py

"""
Get OTA update status from ESP32

This script queries the ESP32 for the current OTA update status including
state, progress, and download information.
"""

import struct
import serial
from enum import IntEnum

class OTAState(IntEnum):
    """OTA state enumeration"""
    IDLE = 0         # No update in progress
    CHECKING = 1     # Checking for updates (version check)
    DOWNLOADING = 2  # Downloading firmware from server
    VERIFYING = 3    # Verifying SHA256 hash
    INSTALLING = 4   # Installing firmware to partition
    COMPLETE = 5     # Update complete, will reboot soon
    FAILED = 6       # Update failed (check logs)


def get_ota_status(esp32_uart):
    """
    Request current OTA status from ESP32

    Args:
        esp32_uart: Serial connection to ESP32

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

    # Parse ota_status_response_t
    # typedef struct __attribute__((packed)) {
    #   uint8_t  state;            /* OTA state (0-6) */
    #   uint8_t  progress;         /* Progress % (0-100) */
    #   uint32_t bytes_downloaded; /* Bytes downloaded */
    #   uint32_t total_bytes;      /* Total bytes */
    # } ota_status_response_t;

    state, progress, bytes_downloaded, total_bytes = struct.unpack('<BBII', response[4:14])

    return {
        'state': OTAState(state),
        'state_name': OTAState(state).name,
        'progress': progress,
        'bytes_downloaded': bytes_downloaded,
        'total_bytes': total_bytes
    }


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python ota_get_status.py <serial_port>")
        print("\nExample:")
        print("  python ota_get_status.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    # Open UART connection
    uart = serial.Serial(port, 115200, timeout=1)

    # Get status
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
