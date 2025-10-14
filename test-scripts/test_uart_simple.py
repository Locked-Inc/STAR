#!/usr/bin/env python3
"""
Simple UART test using only built-in Python (no pyserial needed)
Tests using raw /dev/ttyPS1 device access

For ESP32 connection:
- AR0 (TX) on Arduino header -> ESP32 RX
- AR1 (RX) on Arduino header -> ESP32 TX
- GND -> ESP32 GND

For loopback test: Connect AR0 to AR1 with jumper wire
"""

import os
import sys
import time

UART_DEVICE = '/dev/ttyPS1'
BAUD_RATE = 115200

def configure_uart(device, baud):
    """Configure UART using stty command"""
    import subprocess

    cmd = [
        'stty', '-F', device,
        str(baud),
        'cs8',           # 8 data bits
        '-cstopb',       # 1 stop bit
        '-parenb',       # No parity
        'raw',           # Raw mode
        '-echo'          # No echo
    ]

    try:
        subprocess.run(cmd, check=True)
        return True
    except Exception as e:
        print(f"❌ Failed to configure UART: {e}")
        return False

def test_uart_raw():
    """Test UART using raw file I/O (no pyserial needed)"""

    print("=" * 60)
    print("Simple UART Test (No Extra Packages Needed)")
    print("=" * 60)
    print()

    # Check if device exists
    if not os.path.exists(UART_DEVICE):
        print(f"❌ {UART_DEVICE} not found!")
        print()
        print("📋 Available serial devices:")
        os.system('ls -l /dev/ttyPS* /dev/ttyUSB* 2>/dev/null')
        print()
        print("💡 UART1 might not be enabled in device tree.")
        print("   Try connecting a USB-to-Serial adapter (will show as /dev/ttyUSB0)")
        return False

    print(f"✅ Found {UART_DEVICE}")
    print()

    # Configure UART
    print(f"🔧 Configuring {UART_DEVICE} at {BAUD_RATE} baud...")
    if not configure_uart(UART_DEVICE, BAUD_RATE):
        return False

    print("✅ UART configured")
    print()

    try:
        # Open UART device
        uart = os.open(UART_DEVICE, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        print(f"✅ Opened {UART_DEVICE}")
        print()

        # Test messages
        test_messages = [
            b"Hello UART!\n",
            b"Testing 123\n",
            b"STAR Robot ESP32\n"
        ]

        print("📤 Sending test messages...")
        print("   (Connect AR0 to AR1 for loopback test)")
        print()

        for msg in test_messages:
            # Send message
            written = os.write(uart, msg)
            print(f"   Sent ({written} bytes): {msg.decode().strip()}")

            # Wait a bit
            time.sleep(0.2)

            # Try to read (only works if loopback connected)
            try:
                received = os.read(uart, 1024)
                if received:
                    print(f"   ✅ Received: {received.decode().strip()}")
                else:
                    print(f"   ⚠️  No data received (loopback not connected)")
            except BlockingIOError:
                print(f"   ⚠️  No data received (loopback not connected)")

            print()

        # Interactive mode
        print("📡 Interactive Mode")
        print("   Type messages to send (or 'quit' to exit)")
        print()

        while True:
            msg = input("Send: ")
            if msg.lower() == 'quit':
                break

            # Send message
            os.write(uart, (msg + '\n').encode())

            # Wait for response
            time.sleep(0.1)

            # Try to read
            try:
                received = os.read(uart, 1024)
                if received:
                    print(f"Received: {received.decode().strip()}\n")
            except BlockingIOError:
                pass

        os.close(uart)
        print("\n✅ Test complete!")
        return True

    except PermissionError:
        print(f"❌ Permission denied!")
        print(f"   Try: sudo python3 {sys.argv[0]}")
        return False

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_uart_send_only():
    """Just send data (for testing with logic analyzer)"""

    print("=" * 60)
    print("UART Send-Only Test")
    print("=" * 60)
    print()

    if not os.path.exists(UART_DEVICE):
        print(f"❌ {UART_DEVICE} not found!")
        return False

    # Configure
    configure_uart(UART_DEVICE, BAUD_RATE)

    try:
        uart = os.open(UART_DEVICE, os.O_WRONLY | os.O_NOCTTY)
        print(f"✅ Opened {UART_DEVICE} for writing")
        print()
        print("📤 Sending continuous data...")
        print("   Connect oscilloscope or logic analyzer to AR0 pin")
        print()

        for i in range(10):
            msg = f"Test message {i}\n".encode()
            os.write(uart, msg)
            print(f"   Sent: Test message {i}")
            time.sleep(0.5)

        os.close(uart)
        print("\n✅ Send test complete!")

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    print("\n")

    # Show hardware info
    print("🔌 Hardware Setup:")
    print("   Arduino Shield Connector (J1):")
    print("   - AR0 (TX): Pin 0 on digital header")
    print("   - AR1 (RX): Pin 1 on digital header")
    print("   - GND: Any ground pin")
    print()
    print("   For loopback: Connect AR0 to AR1")
    print("   For ESP32: AR0->ESP32 RX, AR1->ESP32 TX")
    print()

    if len(sys.argv) > 1 and sys.argv[1] == 'send':
        test_uart_send_only()
    else:
        test_uart_raw()
