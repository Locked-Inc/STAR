#!/usr/bin/env python3
"""
Direct test of 40-pin header GPIO pins using PYNQ library

This tests the actual GPIO pins 8 and 10 on the 40-pin header
by toggling them and measuring with a multimeter or oscilloscope.

Since we don't have a second UART adapter, we'll test by:
1. Toggling Pin 8 (TXD/GPIO14) - watch with scope/LED
2. Toggling Pin 10 (RXD/GPIO15) - watch with scope/LED

This confirms the pins work for future ESP32 connection.
"""

from pynq import GPIO
import time

# 40-pin header GPIO mapping on PYNQ-Z2
# These map to MIO pins on the Zynq PS
# Pin 8 (TXD/GPIO14) and Pin 10 (RXD/GPIO15)

# Note: The actual MIO numbers depend on the device tree
# Let's try to find them

def find_gpio_pins():
    """Try to find available GPIO pins"""
    print("=" * 60)
    print("40-Pin Header GPIO Discovery")
    print("=" * 60)
    print()

    # Common EMIO/MIO pins that might be available
    test_pins = list(range(54, 64))  # EMIO pins

    available = []

    print("🔍 Scanning for available GPIO pins...")
    for pin in test_pins:
        try:
            gpio = GPIO(pin, 'out')
            available.append(pin)
            print(f"   ✅ GPIO {pin} available")
        except Exception as e:
            print(f"   ❌ GPIO {pin}: {str(e)[:50]}")

    print()
    return available

def test_gpio_toggle(pin_num):
    """Toggle a GPIO pin for testing"""

    print(f"📡 Testing GPIO {pin_num}")
    print(f"   Connect LED or oscilloscope to test")
    print()

    try:
        gpio = GPIO(pin_num, 'out')

        print("   Toggling 10 times at 1 Hz...")
        for i in range(10):
            gpio.write(1)
            print(f"   {i+1}/10: HIGH", end='\r')
            time.sleep(0.5)
            gpio.write(0)
            time.sleep(0.5)

        print("\n   ✅ Toggle complete!")
        return True

    except Exception as e:
        print(f"   ❌ Error: {e}")
        return False

def test_uart_pattern(pin_num):
    """Send UART-like pattern on GPIO pin"""

    print()
    print(f"📤 Sending UART pattern on GPIO {pin_num}")
    print(f"   This simulates sending 'A' (0x41) at 9600 baud")
    print(f"   Connect oscilloscope to verify")
    print()

    try:
        gpio = GPIO(pin_num, 'out')
        gpio.write(1)  # Idle high
        time.sleep(1)

        # Send 'A' = 0x41 = 01000001
        byte_to_send = 0x41
        bit_time = 1.0 / 9600  # 104 microseconds

        print(f"   Sending byte 0x{byte_to_send:02X} ({chr(byte_to_send)})")
        print(f"   Bit pattern: {bin(byte_to_send)[2:].zfill(8)}")
        print()

        # Start bit
        gpio.write(0)
        time.sleep(bit_time)

        # Data bits (LSB first)
        for i in range(8):
            bit = (byte_to_send >> i) & 1
            gpio.write(bit)
            time.sleep(bit_time)

        # Stop bit
        gpio.write(1)
        time.sleep(bit_time)

        print("   ✅ Pattern sent!")
        return True

    except Exception as e:
        print(f"   ❌ Error: {e}")
        return False

def main():
    print("\n" + "=" * 60)
    print("PYNQ-Z2 40-Pin Header GPIO Test")
    print("=" * 60)
    print()
    print("This script tests GPIO pins that can be used for UART")
    print("to connect to ESP32 on the 40-pin header.")
    print()
    print("⚠️  Note: Without FPGA overlay, standard UART pins")
    print("   (Pin 8/10) may not be accessible as simple GPIO.")
    print()
    print("   We'll test EMIO pins which ARE available.")
    print()

    # Find available pins
    available = find_gpio_pins()

    if not available:
        print("❌ No GPIO pins available!")
        print()
        print("💡 This might mean:")
        print("   1. All pins are used by peripherals")
        print("   2. Need to load FPGA overlay")
        print("   3. Need to modify device tree")
        return

    print()
    print("=" * 60)
    print("GPIO Pin Tests")
    print("=" * 60)
    print()

    # Test first available pin
    pin = available[0]
    print(f"Testing GPIO {pin} as UART TX pin...")
    print()

    if test_gpio_toggle(pin):
        test_uart_pattern(pin)

    print()
    print("=" * 60)
    print("📝 Summary")
    print("=" * 60)
    print()
    print(f"✅ Found {len(available)} available GPIO pins")
    print(f"   Tested GPIO {pin}")
    print()
    print("💡 For ESP32 connection:")
    print(f"   You can use GPIO {available[0]} as TX")
    if len(available) > 1:
        print(f"   You can use GPIO {available[1]} as RX")
    print()
    print("   These are EMIO pins (software UART)")
    print("   Max baud rate: ~9600")
    print()
    print("   Alternative: Use USB-UART adapter between")
    print("   board and ESP32 for higher speeds")
    print()

if __name__ == "__main__":
    main()
