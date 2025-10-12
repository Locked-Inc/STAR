#!/usr/bin/env python3
# esp32-firmware/debugging_scripts/test_error_handler.py

"""
Test error handler by attempting to connect to non-existent WiFi network.
This will trigger connection failures and show the error handler's retry logic
with exponential backoff.
"""

import serial
import sys
import time
import threading

SERIAL_PORT = '/dev/ttyUSB0'  # USB-UART adapter on TX2/RX2
BAUD_RATE = 115200

# Protocol constants
START_MARKER = 0xA5
CMD_WIFI_CONNECT = 0x11
CMD_RESPONSE = 0xF0
STATUS_OK = 0x00

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

def monitor_serial_logs(ser, stop_event):
    """Monitor and print any debug output from ESP32"""
    print("\n=== Monitoring ESP32 Debug Output ===")
    print("(Look for error handler logs with retry attempts and backoff delays)\n")

    while not stop_event.is_set():
        if ser.in_waiting > 0:
            try:
                # Try to read any debug output
                data = ser.read(ser.in_waiting)
                # Print as text if possible
                try:
                    text = data.decode('utf-8', errors='ignore')
                    if text.strip():
                        print(text, end='')
                except:
                    pass
            except:
                pass
        time.sleep(0.1)

def main():
    """Test error handler with invalid WiFi credentials"""
    print("=== Error Handler Test ===")
    print("This test will attempt to connect to a non-existent WiFi network.")
    print("Watch for error handler logs showing:")
    print("  - Error recording")
    print("  - Retry attempts (0/5, 1/5, 2/5, etc.)")
    print("  - Exponential backoff delays (1s -> 2s -> 4s -> 8s -> 10s)")
    print("  - Max retries reached message")
    print("\nStarting test...\n")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(0.5)

        # Clear buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # Create payload with fake WiFi credentials
        # Format: SSID (null-terminated) + Password (null-terminated)
        fake_ssid = b'NONEXISTENT_NETWORK_12345\x00'
        fake_password = b'wrong_password_123\x00'
        payload = fake_ssid + fake_password

        print(f"Attempting to connect to fake network: {fake_ssid.decode('utf-8', errors='ignore').strip(chr(0))}")
        print(f"This should trigger 5 retry attempts with exponential backoff.\n")

        # Send WIFI_CONNECT command
        connect_packet = create_packet(CMD_WIFI_CONNECT, payload)
        print(f"Sending WIFI_CONNECT packet ({len(connect_packet)} bytes)")
        ser.write(connect_packet)

        print("\nWaiting for response and monitoring retry attempts...")
        print("This will take about 30-40 seconds as retries happen with delays.\n")

        # Start monitoring thread to capture ESP32 logs
        stop_event = threading.Event()
        monitor_thread = threading.Thread(target=monitor_serial_logs, args=(ser, stop_event))
        monitor_thread.daemon = True
        monitor_thread.start()

        # Wait for response (with long timeout for retries)
        response_data = b''
        start_time = time.time()
        timeout = 60  # 60 seconds to allow for all retries

        while time.time() - start_time < timeout:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting)
                response_data += chunk

                # Check if we have a complete response packet
                if len(response_data) >= 4:
                    if response_data[0] == START_MARKER and response_data[1] == CMD_RESPONSE:
                        payload_len = response_data[2] | (response_data[3] << 8)
                        if len(response_data) >= 4 + payload_len:
                            # Got complete response
                            print("\n=== Received Response ===")
                            print(f"Total: {len(response_data)} bytes")
                            print(f"Hex: {response_data.hex()}")

                            if payload_len > 0:
                                status = response_data[4]
                                print(f"Status: 0x{status:02X} {'(OK)' if status == STATUS_OK else '(FAILED)'}")

                                if status != STATUS_OK:
                                    print("\n✓ Test successful! Connection failed as expected.")
                                    print("  Check the logs above to see the error handler in action.")
                            break

            time.sleep(0.1)

        stop_event.set()
        monitor_thread.join(timeout=1)

        if not response_data:
            print("\nNo response received - this is normal if connection failed after all retries.")

        ser.close()
        print("\nTest complete!")
        print("\nExpected behavior:")
        print("  1. First connection attempt fails immediately")
        print("  2. Error handler records error: 'WiFi connection failed' (0/5)")
        print("  3. Retry after 1000ms delay")
        print("  4. Error recorded again (1/5), retry after 2000ms")
        print("  5. Error recorded (2/5), retry after 4000ms")
        print("  6. Error recorded (3/5), retry after 8000ms")
        print("  7. Error recorded (4/5), retry after 10000ms (capped)")
        print("  8. Error recorded (5/5) - max retries exceeded")
        print("  9. Reset function called (if configured)")
        print("  10. Connection marked as failed")

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(0)

if __name__ == '__main__':
    main()
