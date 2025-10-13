#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_error_handler_with_logs.py

"""
Comprehensive error handler test with automatic log capture.
Monitors ESP32 logs and sends invalid WiFi command to trigger error handler.
"""

import serial
import subprocess
import sys
import time
import threading
import re

SERIAL_UART = '/dev/ttyUSB0'  # USB-UART adapter for commands
SERIAL_LOG = '/dev/ttyUSB1'   # ESP32 USB port for logs
BAUD_RATE = 115200

START_MARKER = 0xA5
CMD_WIFI_CONNECT = 0x11

# Patterns to look for in logs
ERROR_PATTERNS = [
    r'STAR ERROR HANDLER.*Error Recorded',
    r'Retry:.*\d+/\d+',
    r'Backoff:.*delay:.*\d+\s*ms',
    r'Max Retries',
    r'Reset Attempt',
    r'wifi_manager.*Retrying',
    r'wifi_manager.*disconnected',
]

def create_packet(cmd, payload=b''):
    """Create a protocol packet"""
    payload_len = len(payload)
    packet = bytes([
        START_MARKER,
        cmd,
        payload_len & 0xFF,
        (payload_len >> 8) & 0xFF
    ])
    packet += payload
    return packet

def monitor_logs(log_port, stop_event, error_logs):
    """Monitor ESP32 serial logs and capture error handler messages"""
    try:
        ser = serial.Serial(log_port, BAUD_RATE, timeout=0.5)
        print(f"[OK] Log monitor started on {log_port}\n")

        buffer = ""
        while not stop_event.is_set():
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting)
                try:
                    text = chunk.decode('utf-8', errors='ignore')
                    buffer += text

                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)

                        # Check if line contains error handler patterns
                        for pattern in ERROR_PATTERNS:
                            if re.search(pattern, line, re.IGNORECASE):
                                error_logs.append(line)
                                # Print important lines in real-time
                                if 'Error Recorded' in line or 'Backoff' in line or 'Max Retries' in line:
                                    print(f"  [WARN]  {line.strip()}")
                                break
                except:
                    pass

            time.sleep(0.05)

        ser.close()
    except Exception as e:
        print(f"Log monitor error: {e}")

def main():
    print("=" * 70)
    print("ERROR HANDLER TEST WITH AUTOMATIC LOG CAPTURE")
    print("=" * 70)
    print("\nThis test will:")
    print("  1. Monitor ESP32 debug logs on /dev/ttyUSB1")
    print("  2. Send invalid WiFi connect command on /dev/ttyUSB0")
    print("  3. Capture and display error handler retry logic")
    print("  4. Show exponential backoff in action\n")

    error_logs = []
    stop_event = threading.Event()

    try:
        # Start log monitoring in background thread
        print("Starting log monitor...")
        monitor_thread = threading.Thread(target=monitor_logs, args=(SERIAL_LOG, stop_event, error_logs))
        monitor_thread.daemon = True
        monitor_thread.start()
        time.sleep(1)  # Give monitor time to start

        # Open command serial port
        print(f"Opening command port {SERIAL_UART}...")
        cmd_ser = serial.Serial(SERIAL_UART, BAUD_RATE, timeout=1)
        time.sleep(0.5)

        # Clear buffers
        cmd_ser.reset_input_buffer()
        cmd_ser.reset_output_buffer()

        # Create payload with fake WiFi credentials
        fake_ssid = b'TEST_NONEXISTENT_AP\x00'
        fake_password = b'invalid_password\x00'
        payload = fake_ssid + fake_password

        print(f"\nSending invalid WiFi connect command...")
        print(f"  SSID: {fake_ssid.decode('utf-8').strip(chr(0))}")
        print(f"  (This network doesn't exist - will trigger retries)\n")

        # Send WIFI_CONNECT command
        connect_packet = create_packet(CMD_WIFI_CONNECT, payload)
        cmd_ser.write(connect_packet)
        print("[OK] Command sent!\n")

        print("Monitoring error handler behavior...")
        print("Expected sequence:")
        print("  - Attempt 1: Fail, retry after ~1000ms  (Retry: 0/5)")
        print("  - Attempt 2: Fail, retry after ~2000ms  (Retry: 1/5)")
        print("  - Attempt 3: Fail, retry after ~4000ms  (Retry: 2/5)")
        print("  - Attempt 4: Fail, retry after ~8000ms  (Retry: 3/5)")
        print("  - Attempt 5: Fail, retry after ~10000ms (Retry: 4/5)")
        print("  - Attempt 6: Fail, max retries reached  (Retry: 5/5)")
        print("\nWatching logs (this takes ~30 seconds)...\n")
        print("-" * 70)

        # Wait for retries to complete
        start_time = time.time()
        timeout = 45  # Give plenty of time for all retries

        while time.time() - start_time < timeout:
            time.sleep(0.5)

            # Check if we've seen max retries
            if any('Max Retries' in log for log in error_logs):
                print("\n" + "-" * 70)
                print("[OK] Detected max retries reached!")
                time.sleep(2)  # Wait a bit more for final logs
                break

        # Stop monitoring
        stop_event.set()
        monitor_thread.join(timeout=2)
        cmd_ser.close()

        # Display summary
        print("\n" + "=" * 70)
        print("TEST SUMMARY")
        print("=" * 70)

        if error_logs:
            print(f"\n[OK] Captured {len(error_logs)} error handler log entries:")
            print("\nKey error handler events:")
            for log in error_logs:
                # Clean up the log line
                log_clean = log.strip()
                if log_clean:
                    # Highlight important parts
                    if 'Retry:' in log_clean:
                        retry_match = re.search(r'Retry:\s*(\d+)/(\d+)', log_clean)
                        if retry_match:
                            print(f"  -> Retry {retry_match.group(1)}/{retry_match.group(2)}")
                    elif 'Backoff' in log_clean:
                        delay_match = re.search(r'delay:\s*(\d+)', log_clean)
                        if delay_match:
                            print(f"     +- Backoff delay: {delay_match.group(1)}ms")
                    elif 'Max Retries' in log_clean:
                        print(f"  [FAIL] Max retries exceeded")
                    elif 'Reset' in log_clean:
                        print(f"  [RESET] Reset attempt")

            # Count retries
            retry_count = sum(1 for log in error_logs if 'Retry:' in log)
            print(f"\n[STATS] Statistics:")
            print(f"  - Total retry attempts detected: {retry_count}")
            print(f"  - Expected: 5 retry attempts")
            print(f"  - Status: {'[OK] PASS' if retry_count >= 5 else '[WARN]  CHECK LOGS'}")

        else:
            print("\n[WARN]  No error handler logs captured.")
            print("This might mean:")
            print("  - The WiFi connected successfully (unlikely with fake SSID)")
            print("  - Logs weren't captured (check /dev/ttyUSB1 connection)")
            print("  - Error handler didn't trigger (check integration)")

        print("\n" + "=" * 70)
        print("Test complete!")
        print("=" * 70)

    except serial.SerialException as e:
        print(f"\n[ERROR] Serial error: {e}")
        print("Make sure both /dev/ttyUSB0 and /dev/ttyUSB1 are available.")
        stop_event.set()
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        stop_event.set()
        sys.exit(0)
    except Exception as e:
        print(f"\n[ERROR] Unexpected error: {e}")
        stop_event.set()
        sys.exit(1)

if __name__ == '__main__':
    main()
