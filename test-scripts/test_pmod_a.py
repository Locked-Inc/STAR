#!/usr/bin/env python3
"""
Test PMOD A Connector on PYNQ-Z2

PMOD A has 8 pins (2 rows of 4):
  Pin 1-4 (top row)
  Pin 5-8 (bottom row)

Standard PMOD UART uses:
- Pin 2: TXD (output from board)
- Pin 3: RXD (input to board)
- Pin 5: GND
- Pin 6: VCC (3.3V)

Connect UART-to-USB adapter:
- Adapter RX  -> PMOD A Pin 2 (TXD)
- Adapter TX  -> PMOD A Pin 3 (RXD)
- Adapter GND -> PMOD A Pin 5 (GND)
"""

import os
import sys
import time

# PMOD A pin mappings on PYNQ-Z2
# These map to specific FPGA pins that route to GPIO
PMOD_A_PINS = {
    'PIN1': None,  # Actual GPIO numbers depend on bitstream
    'PIN2_TX': None,
    'PIN3_RX': None,
    'PIN4': None,
    'PIN5_GND': 'GND',
    'PIN6_VCC': '3.3V',
    'PIN7': None,
    'PIN8': None,
}

def show_pmod_pinout():
    """Display PMOD A pinout"""

    print("=" * 60)
    print("PMOD A Connector Pinout")
    print("=" * 60)
    print()
    print("   Top Row:    Bottom Row:")
    print("   ┌─────────┐")
    print("   │ 1  2  3  4 │ 5  6  7  8 │")
    print("   └─────────┘")
    print()
    print("   Pin 1: IO_L13P (GPIO)")
    print("   Pin 2: IO_L13N (GPIO) <- Use for UART TX")
    print("   Pin 3: IO_L14P (GPIO) <- Use for UART RX")
    print("   Pin 4: IO_L14N (GPIO)")
    print("   Pin 5: GND")
    print("   Pin 6: VCC (3.3V)")
    print("   Pin 7: IO_L15P (GPIO)")
    print("   Pin 8: IO_L15N (GPIO)")
    print()

def test_pmod_uart():
    """Test UART on PMOD A pins 2 and 3"""

    print("=" * 60)
    print("PMOD A - UART Test (Pins 2/3)")
    print("=" * 60)
    print()
    print("🔌 Hardware Setup:")
    print("   UART-to-USB Adapter -> PMOD A:")
    print("   - Adapter RX  -> Pin 2 (TXD from board)")
    print("   - Adapter TX  -> Pin 3 (RXD to board)")
    print("   - Adapter GND -> Pin 5 (GND)")
    print()

    print("⚠️  PMOD pins require FPGA bitstream configuration!")
    print()
    print("💡 PMOD UART requires:")
    print("   1. FPGA bitstream with UART IP on PMOD pins")
    print("   2. Or software UART using PL GPIO")
    print()
    print("   For now, testing if any UART device is mapped...")
    print()

    # Check for any AXI UART devices
    axi_uart_devices = []
    for i in range(10):
        device = f'/dev/ttyUL{i}'
        if os.path.exists(device):
            axi_uart_devices.append(device)

    if axi_uart_devices:
        print(f"✅ Found AXI UART devices: {axi_uart_devices}")
        for device in axi_uart_devices:
            print(f"\n   Testing {device}...")
            test_uart_device(device, "PMOD A")
    else:
        print("❌ No AXI UART devices found")
        print()
        print("   PMOD UART requires custom FPGA overlay")
        print("   Use Arduino shield or 40-pin header for UART instead")

def test_uart_device(device, label):
    """Test a UART device"""

    baud = 115200

    try:
        # Configure
        os.system(f'stty -F {device} {baud} cs8 -cstopb -parenb raw -echo 2>/dev/null')

        # Open
        uart = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        print(f"✅ Opened {device} at {baud} baud")

        # Send test data
        messages = [
            b"Hello from PMOD A!\n",
            b"Testing UART on PMOD connector\n",
        ]

        for msg in messages:
            os.write(uart, msg)
            print(f"   Sent: {msg.decode().strip()}")
            time.sleep(0.3)

        # Listen briefly
        print("\n   Listening for response (3 seconds)...")
        start = time.time()
        while time.time() - start < 3:
            try:
                data = os.read(uart, 1024)
                if data:
                    print(f"   Received: {data.decode().strip()}")
            except BlockingIOError:
                time.sleep(0.1)

        os.close(uart)
        print("✅ Test complete!")

    except Exception as e:
        print(f"❌ Error: {e}")

def test_pmod_gpio():
    """Test GPIO on PMOD A"""

    print("\n")
    print("=" * 60)
    print("PMOD A - GPIO Test")
    print("=" * 60)
    print()
    print("⚠️  PMOD GPIO requires FPGA bitstream!")
    print()
    print("💡 PMOD pins are connected to FPGA fabric (PL)")
    print("   They require a bitstream overlay to be loaded")
    print()
    print("   Default PYNQ base overlay may not expose PMOD pins")
    print("   You'll need to create custom overlay for PMOD access")
    print()

if __name__ == "__main__":
    show_pmod_pinout()
    test_pmod_uart()
    test_pmod_gpio()

    print("\n" + "=" * 60)
    print("💡 Recommendation for UART Testing:")
    print("=" * 60)
    print()
    print("PMOD connectors are FPGA pins and need bitstream config.")
    print()
    print("For easier UART testing, use:")
    print("  1. Arduino Shield (AR0/AR1) - Usually has PS UART")
    print("  2. 40-Pin Header - Has multiple PS UART options")
    print()
    print("=" * 60)
