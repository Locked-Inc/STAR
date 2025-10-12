#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_error_handler_detailed.py

"""
Detailed error handler test with extended monitoring.
Sends WiFi connect command and monitors ESP32 logs for error handler activity.
"""

import serial
import time
import threading
import re

SERIAL_UART = '/dev/ttyUSB0'
SERIAL_LOG = '/dev/ttyUSB1'
BAUD_RATE = 115200

class LogMonitor:
    def __init__(self):
        self.logs = []
        self.error_handler_events = []
        self.wifi_events = []
        self.running = False

    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._monitor)
        self.thread.daemon = True
        self.thread.start()

    def stop(self):
        self.running = False
        if hasattr(self, 'thread'):
            self.thread.join(timeout=2)

    def _monitor(self):
        try:
            ser = serial.Serial(SERIAL_LOG, BAUD_RATE, timeout=0.5)
            buffer = ""

            while self.running:
                if ser.in_waiting > 0:
                    chunk = ser.read(ser.in_waiting)
                    text = chunk.decode('utf-8', errors='ignore')
                    buffer += text

                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        self.logs.append(line)

                        # Categorize events
                        if 'ERROR HANDLER' in line:
                            self.error_handler_events.append(line)
                            print(f"[ERROR_HANDLER] {line.strip()}")
                        elif any(kw in line for kw in ['wifi_manager', 'wifi:', 'WIFI']):
                            self.wifi_events.append(line)
                            if any(kw in line for kw in ['connect', 'disconnect', 'Retry', 'fail']):
                                print(f"[WIFI] {line.strip()}")
                        elif 'Retry:' in line or 'Backoff' in line:
                            self.error_handler_events.append(line)
                            print(f"[RETRY] {line.strip()}")

                time.sleep(0.05)

            ser.close()
        except Exception as e:
            print(f"Monitor error: {e}")

def send_wifi_connect(ssid, password):
    """Send WIFI_CONNECT command"""
    try:
        ser = serial.Serial(SERIAL_UART, BAUD_RATE, timeout=2)
        time.sleep(0.3)

        # Create payload
        payload = ssid.encode() + b'\x00' + password.encode() + b'\x00'
        packet = bytes([0xA5, 0x11, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload

        ser.write(packet)

        # Wait for response
        time.sleep(0.5)
        if ser.in_waiting > 0:
            response = ser.read(ser.in_waiting)
            print(f"Response: {response.hex()}")
            if len(response) >= 5:
                status = response[4]
                print(f"Status: {'OK' if status == 0 else 'ERROR'}")

        ser.close()
        return True
    except Exception as e:
        print(f"Send error: {e}")
        return False

def main():
    print("=" * 80)
    print("DETAILED ERROR HANDLER TEST")
    print("=" * 80)
    print("\nThis test will:")
    print("  1. Start monitoring ESP32 logs")
    print("  2. Send a WiFi connect command to a non-existent network")
    print("  3. Watch for connection attempts and retry logic")
    print("  4. Display error handler exponential backoff in action\n")

    # Start log monitor
    monitor = LogMonitor()
    print("Starting log monitor...")
    monitor.start()
    time.sleep(1)

    # Send WiFi connect command
    print("\n" + "-" * 80)
    print("Sending WIFI_CONNECT command...")
    print("  SSID: NONEXISTENT_TEST_NETWORK")
    print("  Password: wrongpassword")
    print("-" * 80 + "\n")

    if not send_wifi_connect("NONEXISTENT_TEST_NETWORK", "wrongpassword"):
        print("Failed to send command")
        monitor.stop()
        return

    print("Command sent. Monitoring for 45 seconds...")
    print("(Connection attempts happen asynchronously)\n")
    print("=" * 80)
    print("LIVE LOG FEED (filtered for relevant events)")
    print("=" * 80 + "\n")

    # Monitor for a while
    time.sleep(45)

    # Stop monitoring
    print("\n" + "=" * 80)
    print("Stopping monitor...")
    monitor.stop()
    time.sleep(1)

    # Display summary
    print("\n" + "=" * 80)
    print("TEST SUMMARY")
    print("=" * 80)

    print(f"\nTotal logs captured: {len(monitor.logs)}")
    print(f"Error handler events: {len(monitor.error_handler_events)}")
    print(f"WiFi events: {len(monitor.wifi_events)}")

    if monitor.error_handler_events:
        print("\n--- Error Handler Events ---")
        for event in monitor.error_handler_events:
            print(f"  {event.strip()}")

    if monitor.wifi_events:
        print("\n--- Key WiFi Events (last 20) ---")
        for event in monitor.wifi_events[-20:]:
            if any(kw in event for kw in ['connect', 'disconnect', 'fail', 'Retry']):
                print(f"  {event.strip()}")

    # Analysis
    print("\n--- Analysis ---")
    retry_matches = [e for e in monitor.logs if 'Retry:' in e or 'retry' in e.lower()]
    backoff_matches = [e for e in monitor.logs if 'Backoff' in e or 'delay' in e.lower()]

    print(f"Retry mentions: {len(retry_matches)}")
    print(f"Backoff mentions: {len(backoff_matches)}")

    if len(retry_matches) >= 5:
        print("\n✓ SUCCESS: Error handler retry logic appears to be working!")
        print("  Multiple retry attempts detected with exponential backoff.")
    elif len(retry_matches) > 0:
        print("\n⚠️  PARTIAL: Some retry activity detected, but less than expected.")
    else:
        print("\n⚠️  NOTE: No explicit retry messages detected in logs.")
        print("  This might mean:")
        print("    - Error handler logs are at a different log level")
        print("    - Connection didn't reach the retry stage")
        print("    - Network was rejected before connection attempt")

    print("\n" + "=" * 80)
    print("Test complete!")
    print("=" * 80)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")

