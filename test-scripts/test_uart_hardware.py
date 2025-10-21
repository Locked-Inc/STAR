#!/usr/bin/env python3
"""
Test hardware UART on Raspberry Pi 5
Uses PS UART1 on Arduino pins AR0 (TX) and AR1 (RX)

Hardware connections:
- AR0 (TX) -> Connect to RX of external device
- AR1 (RX) -> Connect to TX of external device
- GND -> Connect to GND of external device

For loopback test: Connect AR0 to AR1 directly
"""

import serial
import time

# UART1 device
UART_DEVICE = '/dev/ttyPS1'
BAUD_RATE = 115200

def test_uart_loopback():
    """Test UART by sending and receiving data (requires TX->RX connection)"""
    try:
        # Open UART
        ser = serial.Serial(
            port=UART_DEVICE,
            baudrate=BAUD_RATE,
            timeout=1,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )

        print(f"✅ Opened {UART_DEVICE} at {BAUD_RATE} baud")
        print(f"   Settings: {ser}")
        print()

        # Test data
        test_messages = [
            b"Hello UART!\n",
            b"Testing 123\n",
            b"STAR Robot\n"
        ]

        print("📤 Sending test messages...")
        print("   (Connect TX to RX for loopback test)")
        print()

        for msg in test_messages:
            # Clear input buffer
            ser.reset_input_buffer()

            # Send message
            ser.write(msg)
            print(f"   Sent: {msg.decode().strip()}")

            # Wait a bit for loopback
            time.sleep(0.1)

            # Try to receive (will only work if TX->RX connected)
            if ser.in_waiting > 0:
                received = ser.read(ser.in_waiting)
                print(f"   ✅ Received: {received.decode().strip()}")
            else:
                print(f"   ⚠️  No data received (connect TX to RX for loopback)")

            print()
            time.sleep(0.5)

        # Interactive mode
        print("\n📡 Entering interactive mode...")
        print("   Type messages to send (Ctrl+C to exit)")
        print()

        while True:
            # Send user input
            msg = input("Send: ")
            ser.write((msg + '\n').encode())

            # Wait for response
            time.sleep(0.1)
            if ser.in_waiting > 0:
                received = ser.read(ser.in_waiting)
                print(f"Received: {received.decode().strip()}\n")

    except serial.SerialException as e:
        print(f"❌ Error opening {UART_DEVICE}: {e}")
        print(f"\n💡 Possible causes:")
        print(f"   1. UART1 not enabled in device tree")
        print(f"   2. Device {UART_DEVICE} doesn't exist")
        print(f"   3. Permission denied (try: sudo)")
        print(f"\n   Run: ls -l /dev/ttyPS* to see available UARTs")

    except KeyboardInterrupt:
        print("\n\n👋 Exiting...")

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("   UART closed")

def test_uart_send_only():
    """Just send data without checking for received data"""
    try:
        ser = serial.Serial(UART_DEVICE, BAUD_RATE, timeout=1)
        print(f"✅ Opened {UART_DEVICE} at {BAUD_RATE} baud")
        print("📤 Sending test pattern...")

        for i in range(10):
            msg = f"Test message {i}\n"
            ser.write(msg.encode())
            print(f"   Sent: {msg.strip()}")
            time.sleep(0.5)

        print("\n✅ Send test complete!")
        print("   Connect logic analyzer to AR0 to see output")

        ser.close()

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    print("=" * 60)
    print("Raspberry Pi 5 Hardware UART Test (PS UART1)")
    print("=" * 60)
    print()
    print("Hardware Setup:")
    print("  - TX: Arduino pin AR0 (Digital Pin 0)")
    print("  - RX: Arduino pin AR1 (Digital Pin 1)")
    print()
    print("For loopback test: Connect AR0 to AR1 with jumper wire")
    print()
    print("=" * 60)
    print()

    # Check if device exists
    import os
    if not os.path.exists(UART_DEVICE):
        print(f"❌ {UART_DEVICE} not found!")
        print(f"\n📋 Available UART devices:")
        import subprocess
        result = subprocess.run(['ls', '-l', '/dev/ttyPS*'],
                                capture_output=True, text=True)
        print(result.stdout)

        print("\n💡 UART1 might not be enabled in device tree.")
        print("   Trying alternate method with GPIO bit-banging...\n")
    else:
        test_uart_loopback()
