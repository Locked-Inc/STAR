#!/usr/bin/env python3
"""
Test UART using USB-UART adapter (FTDI FT232RL)

Automatically detects /dev/ttyUSB0 and tests communication.
This script tests if you can send/receive data through the USB-UART adapter.

For loopback test: Connect adapter TX to RX with jumper wire
For external device: Connect as normal (RX->TX, TX->RX)
"""

import os
import sys
import time
import glob

def find_usb_uart():
    """Find USB-UART adapter device"""

    # Check common USB-UART devices
    devices = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')

    if devices:
        return devices[0]  # Return first found

    return None

def test_uart_usb(device, baud=115200):
    """Test USB-UART adapter"""

    print("=" * 60)
    print("USB-UART Adapter Test")
    print("=" * 60)
    print()

    if not device:
        print("❌ No USB-UART adapter found!")
        print()
        print("💡 Make sure FTDI adapter is plugged in")
        print("   Run: lsusb")
        print("   Should see: 'Future Technology Devices International'")
        return False

    if not os.path.exists(device):
        print(f"❌ Device {device} not found!")
        return False

    print(f"✅ Found USB-UART: {device}")
    print(f"   Baud rate: {baud}")
    print()

    # Get device info
    print("📋 Device Info:")
    try:
        result = os.popen(f'udevadm info {device} 2>/dev/null | grep -E "ID_MODEL|ID_VENDOR"').read()
        if result:
            print(result)
        else:
            print("   (udevadm info not available)")
    except:
        pass

    print()
    print("🔧 Test Options:")
    print("   1. Loopback: Connect TX to RX on adapter")
    print("   2. External: Connect to another device")
    print("   3. Monitor: Just receive data")
    print()

    choice = input("Select test (1/2/3) or Enter for loopback: ").strip()
    if not choice:
        choice = '1'

    print()

    try:
        # Configure UART
        os.system(f'stty -F {device} {baud} cs8 -cstopb -parenb raw -echo 2>/dev/null')

        # Open device
        uart = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        print(f"✅ Opened {device}")
        print()

        if choice == '1':
            test_loopback(uart, device)
        elif choice == '2':
            test_external(uart, device)
        elif choice == '3':
            test_monitor(uart, device)

        os.close(uart)
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

def test_loopback(uart, device):
    """Test with loopback (TX connected to RX)"""

    print("=" * 60)
    print("Loopback Test")
    print("=" * 60)
    print()
    print("🔌 Connect TX to RX on your USB-UART adapter")
    print("   (Use a jumper wire)")
    print()
    input("Press Enter when ready...")
    print()

    test_messages = [
        b"Hello UART!\n",
        b"Testing 123\n",
        b"Loopback test\n",
        b"STAR Robot\n",
        b"0123456789\n",
    ]

    print("📤 Sending test messages...")
    print()

    passed = 0
    failed = 0

    for msg in test_messages:
        # Clear input buffer
        try:
            os.read(uart, 1024)
        except BlockingIOError:
            pass

        # Send message
        os.write(uart, msg)
        print(f"   Sent: {msg.decode().strip()}")

        # Wait for loopback
        time.sleep(0.1)

        # Read response
        try:
            received = os.read(uart, len(msg))
            if received == msg:
                print(f"   ✅ Received: {received.decode().strip()}")
                passed += 1
            else:
                print(f"   ❌ Mismatch: {received}")
                failed += 1
        except BlockingIOError:
            print(f"   ❌ No data received")
            failed += 1

        print()
        time.sleep(0.2)

    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 60)

    if passed == len(test_messages):
        print("✅ Loopback test PASSED!")
        print("   USB-UART adapter is working correctly!")
    else:
        print("⚠️  Some tests failed")
        print("   Check TX-RX connection")

def test_external(uart, device):
    """Test with external device"""

    print("=" * 60)
    print("External Device Test")
    print("=" * 60)
    print()
    print("🔌 Connect to external device:")
    print("   Adapter TX -> Device RX")
    print("   Adapter RX <- Device TX")
    print("   Adapter GND - Device GND")
    print()

    print("📤 Sending test messages...")
    print("   (Monitor on external device)")
    print()

    messages = [
        b"Hello from Raspberry Pi 5!\n",
        b"USB-UART test\n",
        b"Can you read this?\n",
        b"Send data back!\n",
    ]

    for msg in messages:
        os.write(uart, msg)
        print(f"   Sent: {msg.decode().strip()}")
        time.sleep(0.5)

    print()
    print("📥 Listening for response (10 seconds)...")
    print("   Type on external device...")
    print()

    received_any = False
    start = time.time()
    while time.time() - start < 10:
        try:
            data = os.read(uart, 1024)
            if data:
                print(f"   ✅ Received: {data.decode('utf-8', errors='replace').strip()}")
                received_any = True
        except BlockingIOError:
            time.sleep(0.1)

    if not received_any:
        print("   ⚠️  No data received")
        print()
        print("   Check:")
        print("   - External device connected?")
        print("   - Correct baud rate?")
        print("   - TX/RX crossed over?")

def test_monitor(uart, device):
    """Just monitor incoming data"""

    print("=" * 60)
    print("Monitor Mode")
    print("=" * 60)
    print()
    print("📥 Listening for incoming data...")
    print("   Press Ctrl+C to exit")
    print()

    try:
        while True:
            try:
                data = os.read(uart, 1024)
                if data:
                    # Try to decode as text
                    try:
                        text = data.decode('utf-8')
                        print(f"RX: {text}", end='')
                    except:
                        # Show as hex if not text
                        print(f"RX (hex): {data.hex()}")
            except BlockingIOError:
                time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n\n✅ Monitor stopped")

def interactive_mode(uart, device):
    """Interactive send/receive"""

    print("=" * 60)
    print("Interactive Mode")
    print("=" * 60)
    print()
    print("Type messages to send (or 'quit' to exit)")
    print()

    try:
        while True:
            msg = input("Send: ")
            if msg.lower() == 'quit':
                break

            # Send
            os.write(uart, (msg + '\n').encode())

            # Wait for response
            time.sleep(0.1)

            # Read
            try:
                data = os.read(uart, 1024)
                if data:
                    print(f"Received: {data.decode('utf-8', errors='replace').strip()}")
                else:
                    print("(no response)")
            except BlockingIOError:
                print("(no response)")

            print()

    except KeyboardInterrupt:
        print("\n")

if __name__ == "__main__":
    print()

    # Find USB-UART device
    device = find_usb_uart()

    if device:
        print(f"🔍 Detected: {device}")
        print()
    else:
        print("🔍 Searching for USB-UART adapter...")
        print()
        print("Available serial devices:")
        os.system('ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  None found"')
        print()

    # Run test
    baud = 115200
    if len(sys.argv) > 1:
        try:
            baud = int(sys.argv[1])
        except:
            pass

    success = test_uart_usb(device, baud)

    if success:
        print()
        print("=" * 60)
        print("💡 Next Steps:")
        print("=" * 60)
        print()
        print("Now you can use this adapter to test:")
        print("  1. 40-pin header UART (pins 8, 10)")
        print("  2. Arduino shield UART (AR0, AR1)")
        print("  3. Connect to ESP32 for development")
        print()
        print("Connection diagram:")
        print("  Board TX -> Adapter RX (receive from board)")
        print("  Board RX <- Adapter TX (send to board)")
        print("  Board GND - Adapter GND")
        print()
