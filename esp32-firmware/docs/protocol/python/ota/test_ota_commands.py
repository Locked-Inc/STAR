#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/ota/test_ota_commands.py

"""Complete OTA commands test suite"""

import serial
import time
import sys
import struct

def test_ota_commands(port):
    """Test all OTA commands"""
    uart = serial.Serial(port, 115200, timeout=15)
    time.sleep(0.1)

    print("="*60)
    print("ESP32 OTA Commands Test Suite")
    print("="*60)

    # Test 1: OTA Check Update
    print("\n[1/4] Testing CMD_OTA_CHECK_UPDATE...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    packet = bytes([0xA5, 0x28, 0x00, 0x00])
    uart.write(packet)

    time.sleep(5)  # Version check may take time
    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:  # k_status_ok
        if len(response) >= 14:
            update_available = response[7]
            current_version = f"{response[8]}.{response[9]}.{response[10]}"
            new_version = f"{response[11]}.{response[12]}.{response[13]}"

            print(f"  [OK] OTA_CHECK_UPDATE: PASSED")
            print(f"    Current version: {current_version}")
            print(f"    New version: {new_version}")
            print(f"    Update available: {'Yes' if update_available else 'No'}")
        else:
            print("  [FAIL] OTA_CHECK_UPDATE: Response too short")
    elif len(response) >= 5 and response[4] == 0x10:  # k_status_wifi_failed
        print("  [FAIL] OTA_CHECK_UPDATE: FAILED (WiFi not connected)")
    else:
        print("  [FAIL] OTA_CHECK_UPDATE: FAILED")

    time.sleep(0.5)

    # Test 2: OTA Get Status
    print("\n[2/4] Testing CMD_OTA_GET_STATUS...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    packet = bytes([0xA5, 0x2A, 0x00, 0x00])
    uart.write(packet)

    response = uart.read(100)

    if len(response) >= 5 and response[4] == 0x00:
        if len(response) >= 17:
            state = response[7]
            progress = response[8]
            bytes_downloaded = struct.unpack('<I', response[9:13])[0]
            total_bytes = struct.unpack('<I', response[13:17])[0]
            error_code = response[17] if len(response) > 17 else 0

            state_names = {
                0: 'IDLE',
                1: 'CHECKING',
                2: 'DOWNLOADING',
                3: 'VERIFYING',
                4: 'INSTALLING',
                5: 'COMPLETE',
                6: 'FAILED'
            }

            error_names = {
                0x00: 'OK',
                0x30: 'OTA_FAILED',
                0x32: 'WIFI_DISCONNECTED',
                0x33: 'SHA256_MISMATCH',
                0x34: 'VERSION_DOWNGRADE',
                0x35: 'PARTITION_ERROR',
                0x36: 'WRITE_ERROR',
                0x37: 'HTTP_ERROR',
                0x38: 'NO_UPDATE'
            }

            print(f"  [OK] OTA_GET_STATUS: PASSED")
            print(f"    State: {state_names.get(state, f'UNKNOWN({state})')}")
            print(f"    Progress: {progress}%")
            print(f"    Downloaded: {bytes_downloaded} / {total_bytes} bytes")
            if error_code != 0:
                print(f"    Error code: {error_names.get(error_code, f'0x{error_code:02X}')}")
        else:
            print("  [FAIL] OTA_GET_STATUS: Response too short")
    else:
        print("  [FAIL] OTA_GET_STATUS: FAILED")

    time.sleep(0.5)

    # Test 3: OTA Start Update (optional - dangerous!)
    print("\n[3/4] Testing CMD_OTA_START_UPDATE...")
    print("  WARNING: This will attempt to download and install firmware!")
    do_update = input("  Start OTA update? (y/N): ")

    if do_update.lower() == 'y':
        url = input("  Enter firmware URL: ")
        sha256 = input("  Enter SHA256 hash (or press Enter to skip): ")
        allow_downgrade = input("  Allow downgrade? (y/N): ")

        uart.reset_input_buffer()
        uart.reset_output_buffer()

        # Prepare payload
        url_bytes = url.encode('utf-8')[:255] + b'\x00'
        url_bytes = url_bytes.ljust(256, b'\x00')

        sha256_bytes = sha256.encode('utf-8')[:64] + b'\x00' if sha256 else b'\x00'
        sha256_bytes = sha256_bytes.ljust(65, b'\x00')

        downgrade_byte = b'\x01' if allow_downgrade.lower() == 'y' else b'\x00'

        payload = url_bytes + sha256_bytes + downgrade_byte
        payload_len = len(payload)

        packet = bytes([0xA5, 0x29, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
        uart.write(packet)

        time.sleep(1)
        response = uart.read(100)

        if len(response) >= 5 and response[4] == 0x00:
            print("  [OK] OTA_START_UPDATE: Command accepted, update starting...")
            print("    Monitor progress with CMD_OTA_GET_STATUS")

            # Poll status for a few seconds
            for i in range(10):
                time.sleep(2)
                uart.reset_input_buffer()
                uart.reset_output_buffer()

                status_packet = bytes([0xA5, 0x2A, 0x00, 0x00])
                uart.write(status_packet)
                status_response = uart.read(100)

                if len(status_response) >= 9:
                    state = status_response[7]
                    progress = status_response[8]

                    state_names = {2: 'DOWNLOADING', 3: 'VERIFYING', 4: 'INSTALLING', 5: 'COMPLETE', 6: 'FAILED'}
                    print(f"    [{i+1}] State: {state_names.get(state, 'IDLE')}, Progress: {progress}%")

                    if state == 5:  # COMPLETE
                        print("  [OK] OTA update completed successfully!")
                        break
                    elif state == 6:  # FAILED
                        if len(status_response) > 17:
                            error_code = status_response[17]
                            print(f"  [FAIL] OTA update failed with error code: 0x{error_code:02X}")
                        break
        elif len(response) >= 5 and response[4] == 0x10:
            print("  [FAIL] OTA_START_UPDATE: FAILED (WiFi not connected)")
        elif len(response) >= 5 and response[4] == 0x30:
            print("  [FAIL] OTA_START_UPDATE: FAILED (OTA failed)")
        elif len(response) >= 5 and response[4] == 0x33:
            print("  [FAIL] OTA_START_UPDATE: FAILED (SHA256 mismatch)")
        elif len(response) >= 5 and response[4] == 0x37:
            print("  [FAIL] OTA_START_UPDATE: FAILED (HTTP error)")
        else:
            print("  [FAIL] OTA_START_UPDATE: FAILED")
    else:
        print("  [SKIP] OTA_START_UPDATE: SKIPPED")

    time.sleep(0.5)

    # Test 4: OTA Error Code Testing
    print("\n[4/4] Testing OTA error handling...")
    uart.reset_input_buffer()
    uart.reset_output_buffer()

    # Try to start update with invalid URL (should fail)
    url = "http://invalid.url.that.does.not.exist"
    url_bytes = url.encode('utf-8')[:255] + b'\x00'
    url_bytes = url_bytes.ljust(256, b'\x00')
    sha256_bytes = b'\x00' * 65
    downgrade_byte = b'\x00'

    payload = url_bytes + sha256_bytes + downgrade_byte
    payload_len = len(payload)

    packet = bytes([0xA5, 0x29, payload_len & 0xFF, (payload_len >> 8) & 0xFF]) + payload
    uart.write(packet)

    time.sleep(3)
    response = uart.read(100)

    # Should get some kind of error
    if len(response) >= 5:
        status_code = response[4]
        if status_code == 0x00:
            # Command accepted, check status for error
            time.sleep(5)
            uart.reset_input_buffer()
            uart.reset_output_buffer()

            status_packet = bytes([0xA5, 0x2A, 0x00, 0x00])
            uart.write(status_packet)
            status_response = uart.read(100)

            if len(status_response) >= 18 and status_response[7] == 6:  # FAILED state
                error_code = status_response[17]
                print(f"  [OK] OTA error handling: PASSED (Error code: 0x{error_code:02X})")
            else:
                print("  [WARN] OTA error handling: Update didn't fail as expected")
        elif status_code in [0x30, 0x37]:  # OTA_FAILED or HTTP_ERROR
            print(f"  [OK] OTA error handling: PASSED (Immediate error: 0x{status_code:02X})")
        else:
            print(f"  [WARN] OTA error handling: Unexpected status: 0x{status_code:02X}")
    else:
        print("  [FAIL] OTA error handling: No response")

    uart.close()

    print("\n" + "="*60)
    print("OTA test suite complete!")
    print("="*60)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_ota_commands.py <serial_port>")
        print("Example: python test_ota_commands.py /dev/ttyUSB0")
        print("\nNote: ESP32 must be connected to WiFi for OTA tests to work")
        print("      OTA update tests are optional and potentially destructive")
        sys.exit(1)

    test_ota_commands(sys.argv[1])
