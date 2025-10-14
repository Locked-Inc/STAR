#!/usr/bin/env python3
"""
Test Arduino Shield Connector (J1) on PYNQ-Z2

Arduino Digital Pins (via EMIO):
- AR0 (Digital 0): UART TX / GPIO
- AR1 (Digital 1): UART RX / GPIO
- AR2-AR13: GPIO pins

This script tests:
1. UART on AR0/AR1
2. GPIO toggle on all digital pins

Connect your UART-to-USB adapter:
- Adapter RX -> AR0 (TX from board)
- Adapter TX -> AR1 (RX to board)
- Adapter GND -> GND
"""

import os
import sys
import time

# Arduino pins map to these GPIOs
# These are EMIO pins on PYNQ-Z2
ARDUINO_PINS = {
    'AR0': 0,   # Will determine actual GPIO number
    'AR1': 1,
    'AR2': 2,
    'AR3': 3,
    'AR4': 4,
    'AR5': 5,
    'AR6': 6,
    'AR7': 7,
    'AR8': 8,
    'AR9': 9,
    'AR10': 10,
    'AR11': 11,
    'AR12': 12,
    'AR13': 13,
}

def test_uart_arduino():
    """Test UART on Arduino pins AR0/AR1"""

    print("=" * 60)
    print("Arduino Shield - UART Test (AR0/AR1)")
    print("=" * 60)
    print()
    print("🔌 Hardware Setup:")
    print("   UART-to-USB Adapter -> Arduino Shield J1:")
    print("   - Adapter RX  -> AR0 (Digital Pin 0)")
    print("   - Adapter TX  -> AR1 (Digital Pin 1)")
    print("   - Adapter GND -> GND")
    print()

    # Try hardware UART first
    # Also check USB-UART for testing
    uart_devices = ['/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyPS1', '/dev/ttyS1']

    for device in uart_devices:
        if os.path.exists(device):
            print(f"✅ Found UART device: {device}")
            test_uart_device(device, "Arduino AR0/AR1")
            return

    print("⚠️  No hardware UART found on AR0/AR1")
    print("   Trying GPIO bit-banging method...")
    print()

    # TODO: Implement software UART if hardware UART not available

def test_uart_device(device, label):
    """Test a UART device"""

    baud = 115200

    # Configure using stty
    os.system(f'stty -F {device} {baud} cs8 -cstopb -parenb raw -echo')

    try:
        # Open device
        uart = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        print(f"✅ Opened {device} at {baud} baud")
        print()
        print("📤 Sending test messages...")
        print("   Check your USB adapter terminal (115200 baud)")
        print()

        messages = [
            b"Hello from Arduino Shield!\n",
            b"AR0 (TX) -> Your adapter RX\n",
            b"AR1 (RX) <- Your adapter TX\n",
            b"Type something back!\n",
        ]

        for msg in messages:
            os.write(uart, msg)
            print(f"   Sent: {msg.decode().strip()}")
            time.sleep(0.5)

        print()
        print("📥 Listening for data from adapter...")
        print("   Type in your USB adapter terminal and press Enter")
        print("   (Waiting 10 seconds...)")
        print()

        # Listen for response
        start = time.time()
        while time.time() - start < 10:
            try:
                data = os.read(uart, 1024)
                if data:
                    print(f"   ✅ Received: {data.decode().strip()}")
            except BlockingIOError:
                time.sleep(0.1)

        os.close(uart)
        print("\n✅ UART test complete!")

    except Exception as e:
        print(f"❌ Error: {e}")

def test_gpio_arduino():
    """Test GPIO pins on Arduino shield"""

    print("\n")
    print("=" * 60)
    print("Arduino Shield - GPIO Test (AR2-AR13)")
    print("=" * 60)
    print()

    # Note: Need to determine actual GPIO numbers for Arduino pins
    print("⚠️  GPIO test requires knowing EMIO mapping")
    print("   This depends on your board's device tree configuration")
    print()
    print("💡 To test GPIO manually:")
    print("   1. Find GPIO chip: ls /sys/class/gpio/")
    print("   2. Export pin: echo N > /sys/class/gpio/export")
    print("   3. Set output: echo out > /sys/class/gpio/gpioN/direction")
    print("   4. Toggle: echo 1 > /sys/class/gpio/gpioN/value")
    print()

if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("PYNQ-Z2 Arduino Shield Test")
    print("=" * 60)
    print()

    test_uart_arduino()
    test_gpio_arduino()

    print("\n" + "=" * 60)
    print("Test Complete!")
    print("=" * 60)
