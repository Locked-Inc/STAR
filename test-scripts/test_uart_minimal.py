#!/usr/bin/env python3
"""
Minimal UART/GPIO test using only PYNQ library (already installed)
No additional dependencies needed
"""

from pynq import GPIO
import time

def test_gpio_simple():
    """Test GPIO with simple toggle - verifies GPIO subsystem works"""
    print("=" * 60)
    print("GPIO Test - No Dependencies (PYNQ only)")
    print("=" * 60)
    print()

    # Try different GPIO pins to find one that works
    test_pins = [54, 55, 56, 57]  # EMIO pins

    working_pins = []

    print("🔍 Testing GPIO pins...")
    for pin_num in test_pins:
        try:
            gpio = GPIO(pin_num, 'out')
            working_pins.append(pin_num)
            print(f"   ✅ GPIO {pin_num}: Available")

            # Test write
            gpio.write(1)
            time.sleep(0.01)
            gpio.write(0)

        except Exception as e:
            print(f"   ❌ GPIO {pin_num}: {e}")

    print()

    if not working_pins:
        print("❌ No GPIO pins available!")
        return False

    # Use first working pin for demo
    pin_num = working_pins[0]
    print(f"📌 Using GPIO {pin_num} for toggle test")
    print(f"   Connect LED or oscilloscope to verify")
    print()

    gpio = GPIO(pin_num, 'out')

    print("🔄 Toggling GPIO 10 times at 1 Hz...")
    for i in range(10):
        gpio.write(1)
        print(f"   {i+1:2d}/10: HIGH", end='\r')
        time.sleep(0.5)
        gpio.write(0)
        time.sleep(0.5)

    print("\n✅ GPIO toggle complete!")
    print()

    # Test multiple pins for UART
    if len(working_pins) >= 2:
        tx_pin = working_pins[0]
        rx_pin = working_pins[1]

        print(f"📡 Recommended UART pins:")
        print(f"   TX: GPIO {tx_pin}")
        print(f"   RX: GPIO {rx_pin}")
        print()
        print("   These can be used for software UART to ESP32")

    return True

def check_uart_devices():
    """Check what UART devices exist"""
    import os

    print("=" * 60)
    print("UART Device Check")
    print("=" * 60)
    print()

    print("🔍 Available UART devices:")

    uart_devices = [
        '/dev/ttyPS0',
        '/dev/ttyPS1',
        '/dev/ttyUSB0',
        '/dev/ttyUSB1',
        '/dev/ttyACM0',
    ]

    found = []
    for device in uart_devices:
        if os.path.exists(device):
            stat_info = os.stat(device)
            print(f"   ✅ {device}")
            print(f"      Permissions: {oct(stat_info.st_mode)[-3:]}")
            found.append(device)

    if not found:
        print("   ❌ No UART devices found")
        print()
        print("   /dev/ttyPS0 is console (should exist)")
        print("   /dev/ttyPS1 would be hardware UART (not enabled)")
        print()
        print("💡 Solution: Use GPIO software UART or USB-Serial adapter")

    print()
    return found

def software_uart_demo(tx_pin_num=54):
    """Demonstrate software UART transmission"""
    print("=" * 60)
    print("Software UART Demo (Bit-Banging)")
    print("=" * 60)
    print()

    try:
        tx_pin = GPIO(tx_pin_num, 'out')
        tx_pin.write(1)  # Idle high

        print(f"📡 TX Pin: GPIO {tx_pin_num}")
        print(f"   Baud Rate: 9600")
        print(f"   Message: 'A' (0x41)")
        print()

        # Send single character 'A' = 0x41 = 01000001
        bit_time = 1.0 / 9600  # 104 microseconds

        byte_to_send = ord('A')

        print("🔧 Bit pattern for 'A':")
        print(f"   Binary: {bin(byte_to_send)[2:].zfill(8)}")
        print(f"   Sending: START + 01000001 + STOP")
        print()

        input("   Press Enter to transmit (connect oscilloscope to GPIO {})...".format(tx_pin_num))

        # Start bit
        tx_pin.write(0)
        time.sleep(bit_time)

        # Data bits (LSB first)
        for i in range(8):
            bit = (byte_to_send >> i) & 1
            tx_pin.write(bit)
            time.sleep(bit_time)

        # Stop bit
        tx_pin.write(1)
        time.sleep(bit_time)

        print("✅ Character transmitted!")
        print()
        print("📊 If you had oscilloscope connected:")
        print("   - You'd see start bit (low)")
        print("   - 8 data bits")
        print("   - Stop bit (high)")

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    import sys

    # Check UART devices first
    uart_devs = check_uart_devices()

    # Test GPIO
    print()
    if test_gpio_simple():
        print()
        print("=" * 60)
        print("📝 Summary")
        print("=" * 60)
        print()

        if uart_devs:
            print("✅ Hardware UART devices found:")
            for dev in uart_devs:
                print(f"   - {dev}")
            print()
            if '/dev/ttyPS0' in uart_devs and len(uart_devs) == 1:
                print("   ⚠️  Only console UART available")
                print("   For ESP32: Use GPIO software UART (GPIOs 54, 55)")
        else:
            print("⚠️  No hardware UART (except console)")
            print("   For ESP32: Use GPIO software UART (GPIOs 54, 55)")

        print()
        print("🔧 GPIO software UART recommendation:")
        print("   - TX: GPIO 54 (to ESP32 RX)")
        print("   - RX: GPIO 55 (from ESP32 TX)")
        print("   - Max baud: ~9600 (software limitation)")
        print()

        # Demo software UART
        if '--demo' in sys.argv:
            print()
            software_uart_demo(54)
