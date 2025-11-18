#!/usr/bin/env python3
# esp32-firmware/docs/protocol/python/ota/ota_status.py

"""Get OTA update status"""

import serial
import time
import sys
import struct

def ota_status(port, monitor=False):
    """Get OTA update status"""
    uart = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.1)

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

    def get_status():
        # Send CMD_OTA_GET_STATUS (0x2A)
        packet = bytes([0xA5, 0x2A, 0x00, 0x00])

        uart.reset_input_buffer()
        uart.reset_output_buffer()
        uart.write(packet)

        time.sleep(0.5)
        response = uart.read(100)

        if len(response) >= 17 and response[4] == 0x00:
            state = response[7]
            progress = response[8]
            bytes_downloaded = struct.unpack('<I', response[9:13])[0]
            total_bytes = struct.unpack('<I', response[13:17])[0]
            error_code = response[17] if len(response) > 17 else 0

            return {
                'state': state,
                'state_name': state_names.get(state, f'UNKNOWN({state})'),
                'progress': progress,
                'bytes_downloaded': bytes_downloaded,
                'total_bytes': total_bytes,
                'error_code': error_code,
                'error_name': error_names.get(error_code, f'0x{error_code:02X}')
            }
        return None

    if monitor:
        print("Monitoring OTA status (Ctrl+C to stop)...")
        print("-" * 60)
        try:
            while True:
                status = get_status()
                if status:
                    print(f"\rState: {status['state_name']:12} | Progress: {status['progress']:3}% | "
                          f"Downloaded: {status['bytes_downloaded']:8}/{status['total_bytes']:8} bytes",
                          end='', flush=True)

                    if status['error_code'] != 0:
                        print(f"\n[FAIL] Error: {status['error_name']}")
                        break

                    if status['state'] == 5:  # COMPLETE
                        print("\n[OK] OTA update completed successfully!")
                        break
                    elif status['state'] == 6:  # FAILED
                        print(f"\n[FAIL] OTA update failed: {status['error_name']}")
                        break
                else:
                    print("\r[FAIL] Failed to get status", end='', flush=True)

                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\nMonitoring stopped.")
    else:
        print("Getting OTA status...")
        status = get_status()

        if status:
            print("\n" + "="*60)
            print("OTA STATUS")
            print("="*60)
            print(f"State:      {status['state_name']}")
            print(f"Progress:   {status['progress']}%")
            print(f"Downloaded: {status['bytes_downloaded']} / {status['total_bytes']} bytes")
            if status['error_code'] != 0:
                print(f"Error:      {status['error_name']}")
            print("="*60)
        else:
            print("\n[FAIL] Failed to get OTA status")

    uart.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ota_status.py <serial_port> [--monitor]")
        print("Example: python ota_status.py /dev/ttyUSB0")
        print("         python ota_status.py /dev/ttyUSB0 --monitor")
        sys.exit(1)

    monitor_mode = '--monitor' in sys.argv or '-m' in sys.argv
    ota_status(sys.argv[1], monitor=monitor_mode)
