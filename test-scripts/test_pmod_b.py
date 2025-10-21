#!/usr/bin/env python3
"""
Test PMOD B Connector on Raspberry Pi 5

PMOD B has same pinout as PMOD A:
  Pin 1-4 (top row)
  Pin 5-8 (bottom row)

Standard PMOD UART uses:
- Pin 2: TXD (output from board)
- Pin 3: RXD (input to board)
- Pin 5: GND
- Pin 6: VCC (3.3V)

Connect UART-to-USB adapter:
- Adapter RX  -> PMOD B Pin 2 (TXD)
- Adapter TX  -> PMOD B Pin 3 (RXD)
- Adapter GND -> PMOD B Pin 5 (GND)
"""

import os
import sys
import time

def show_pmod_pinout():
    """Display PMOD B pinout"""

    print("=" * 60)
    print("PMOD B Connector Pinout")
    print("=" * 60)
    print()
    print("   Top Row:    Bottom Row:")
    print("   ┌─────────┐")
    print("   │ 1  2  3  4 │ 5  6  7  8 │")
    print("   └─────────┘")
    print()
    print("   Pin 1: IO_L16P (GPIO)")
    print("   Pin 2: IO_L16N (GPIO) <- Use for UART TX")
    print("   Pin 3: IO_L17P (GPIO) <- Use for UART RX")
    print("   Pin 4: IO_L17N (GPIO)")
    print("   Pin 5: GND")
    print("   Pin 6: VCC (3.3V)")
    print("   Pin 7: IO_L18P (GPIO)")
    print("   Pin 8: IO_L18N (GPIO)")
    print()

def test_pmod_uart():
    """Test UART on PMOD B pins 2 and 3"""

    print("=" * 60)
    print("PMOD B - UART Test (Pins 2/3)")
    print("=" * 60)
    print()
    print("🔌 Hardware Setup:")
    print("   UART-to-USB Adapter -> PMOD B:")
    print("   - Adapter RX  -> Pin 2 (TXD from board)")
    print("   - Adapter TX  -> Pin 3 (RXD to board)")
    print("   - Adapter GND -> Pin 5 (GND)")
    print()

    print("⚠️  PMOD pins require FPGA bitstream configuration!")
    print()
    print("💡 Same as PMOD A - requires custom FPGA overlay")
    print("   PMOD B is identical to PMOD A, just different FPGA pins")
    print()

    # Check for AXI UART
    print("   Checking for AXI UART devices...")
    axi_devices = []
    for i in range(10):
        device = f'/dev/ttyUL{i}'
        if os.path.exists(device):
            axi_devices.append(device)

    if axi_devices:
        print(f"✅ Found: {axi_devices}")
        print("   (These may or may not be PMOD B)")
    else:
        print("❌ No AXI UART found")
        print("   Need custom bitstream with UART IP for PMOD B")

def test_pmod_gpio():
    """Test GPIO on PMOD B"""

    print("\n")
    print("=" * 60)
    print("PMOD B - GPIO Test")
    print("=" * 60)
    print()
    print("⚠️  Same as PMOD A - requires FPGA bitstream")
    print()
    print("   PMOD B pins connect to FPGA fabric (PL)")
    print("   Need overlay to expose these as GPIO")
    print()

if __name__ == "__main__":
    show_pmod_pinout()
    test_pmod_uart()
    test_pmod_gpio()

    print("\n" + "=" * 60)
    print("📝 Note: PMOD A and B are identical")
    print("=" * 60)
    print()
    print("Both are FPGA (PL) pins, not ARM (PS) pins")
    print("They require custom bitstream to use")
    print()
    print("For ESP32 connection, use:")
    print("  • 40-pin header (PS UART available)")
    print("  • Arduino shield (PS UART available)")
    print()
    print("=" * 60)
