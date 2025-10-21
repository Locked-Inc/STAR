#!/usr/bin/env python3
"""
Test Software UART (bit-banging) using PYNQ GPIO
Uses any available GPIO pins

Recommended pins on PMOD connectors:
- TX: Any GPIO pin (we'll use one from available list)
- RX: Any GPIO pin (we'll use another from available list)

This is slower than hardware UART but works with any GPIO pins.
"""

import RPi.GPIO as GPIO
import time

# We'll discover available pins first
def find_available_gpios():
    """Find available GPIO pins on the system"""
    import os
    available = []

    # Check /sys/class/gpio for exported pins
    gpio_base = "/sys/class/gpio"

    # Common GPIO numbers on Raspberry Pi 5 (MIO and EMIO)
    # MIO: 0-53 (PS pins, some used by peripherals)
    # EMIO: 54-117 (Extended MIO)

    # Let's try some known-available EMIO pins
    test_pins = [54, 55, 56, 57, 58, 59, 60, 61]  # EMIO pins usually available

    print("🔍 Scanning for available GPIO pins...")
    for pin in test_pins:
        try:
            # Try to create GPIO object
            test = GPIO(pin, 'out')
            available.append(pin)
            print(f"   ✅ GPIO {pin} is available")
            # Don't clean up yet, we'll use these
        except Exception as e:
            print(f"   ❌ GPIO {pin} not available: {e}")

    return available

def software_uart_send(gpio_pin, data, baud_rate=9600):
    """
    Send data via software UART (bit-banging)

    Args:
        gpio_pin: GPIO object configured as output
        data: bytes to send
        baud_rate: bits per second
    """
    bit_time = 1.0 / baud_rate  # Time per bit in seconds

    for byte in data:
        # Start bit (low)
        gpio_pin.write(0)
        time.sleep(bit_time)

        # Data bits (LSB first)
        for i in range(8):
            bit = (byte >> i) & 1
            gpio_pin.write(bit)
            time.sleep(bit_time)

        # Stop bit (high)
        gpio_pin.write(1)
        time.sleep(bit_time)

def test_software_uart():
    """Test software UART implementation"""
    print("=" * 60)
    print("Raspberry Pi 5 Software UART Test (GPIO Bit-Banging)")
    print("=" * 60)
    print()

    # Find available GPIOs
    available = find_available_gpios()

    if len(available) < 2:
        print(f"\n❌ Need at least 2 GPIO pins, only found {len(available)}")
        print("   Trying specific PMOD pins...")

        # Try specific PMOD A pins (usually available)
        tx_pin_num = 54  # EMIO[0]
        rx_pin_num = 55  # EMIO[1]
    else:
        tx_pin_num = available[0]
        rx_pin_num = available[1]

    print(f"\n📌 Using pins:")
    print(f"   TX: GPIO {tx_pin_num}")
    print(f"   RX: GPIO {rx_pin_num} (not implemented in this test)")
    print()

    try:
        # Create TX pin
        tx_pin = GPIO(tx_pin_num, 'out')
        tx_pin.write(1)  # Idle state is high

        print("✅ GPIO initialized")
        print()

        # Test parameters
        baud_rate = 9600  # Slower for software UART
        test_message = b"Hello UART!"

        print(f"📤 Sending via software UART at {baud_rate} baud...")
        print(f"   Message: {test_message.decode()}")
        print(f"   Connect logic analyzer or oscilloscope to GPIO {tx_pin_num}")
        print()
        print("   Sending in 3 seconds...")
        time.sleep(3)

        # Send the message
        start_time = time.time()
        software_uart_send(tx_pin, test_message, baud_rate)
        elapsed = time.time() - start_time

        print(f"✅ Sent {len(test_message)} bytes in {elapsed:.3f}s")
        print(f"   Expected time: {len(test_message) * 10 / baud_rate:.3f}s")
        print()

        # Send continuous pattern for testing
        print("📡 Sending continuous 0xAA pattern (10 bytes)...")
        print("   0xAA = 10101010b (good for scope triggering)")
        time.sleep(1)

        pattern = b'\xAA' * 10
        software_uart_send(tx_pin, pattern, baud_rate)

        print("✅ Pattern sent!")
        print()

        # Cleanup
        tx_pin.write(1)  # Return to idle

        print("✅ Test complete!")
        print()
        print("💡 To verify:")
        print("   - Connect oscilloscope to GPIO pin")
        print("   - Set scope to trigger on falling edge")
        print("   - You should see UART waveform at 9600 baud")

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()

def test_simple_gpio_toggle():
    """Simple GPIO toggle test to verify GPIO works at all"""
    print("=" * 60)
    print("Simple GPIO Toggle Test")
    print("=" * 60)
    print()

    # Try GPIO 54 (EMIO[0])
    pin_num = 54

    try:
        print(f"🔧 Testing GPIO {pin_num}...")
        gpio = GPIO(pin_num, 'out')

        print(f"✅ GPIO {pin_num} initialized as output")
        print("📡 Toggling pin 10 times at 1 Hz...")
        print(f"   Connect LED (with resistor) or oscilloscope to GPIO {pin_num}")
        print()

        for i in range(10):
            gpio.write(1)
            print(f"   {i+1}/10: HIGH", end='\r')
            time.sleep(0.5)
            gpio.write(0)
            time.sleep(0.5)

        print()
        print("✅ Toggle test complete!")

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == 'simple':
        test_simple_gpio_toggle()
    else:
        test_software_uart()
