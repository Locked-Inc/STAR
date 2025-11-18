#!/usr/bin/env python3
# esp32-firmware/docs/ota/python/ota_send_update.py

"""
Send OTA update command to ESP32

This script sends an OTA update command via UART to initiate a firmware update.
Supports SHA256 verification and version downgrade options.
"""

import struct
import serial

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


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 3:
        print("Usage: python ota_send_update.py <serial_port> <url> [sha256] [allow_downgrade]")
        print("\nExamples:")
        print("  python ota_send_update.py /dev/ttyUSB0 https://example.com/fw.bin")
        print("  python ota_send_update.py /dev/ttyUSB0 https://example.com/fw.bin a3f5e8b1...")
        print("  python ota_send_update.py /dev/ttyUSB0 https://example.com/fw.bin a3f5e8b1... true")
        sys.exit(1)

    port = sys.argv[1]
    url = sys.argv[2]
    sha256 = sys.argv[3] if len(sys.argv) > 3 else None
    allow_downgrade = (len(sys.argv) > 4 and sys.argv[4].lower() == 'true')

    # Open UART connection
    uart = serial.Serial(port, 115200, timeout=1)

    # Send update command
    success = send_ota_update(uart, url, sha256, allow_downgrade)

    uart.close()

    sys.exit(0 if success else 1)
