#!/usr/bin/env python3
"""
Test 40-Pin Raspberry Pi Header (J2) on Raspberry Pi 5

The 40-pin header provides:
- I2C (pins 3, 5)
- SPI (pins 19, 21, 23, 24, 26)
- UART (pins 8, 10)
- GPIO pins
- 3.3V power
- 5V power
- GND

⚠️  ALL GPIO PINS ARE 3.3V LOGIC! NOT 5V TOLERANT!

For UART to ESP32:
- Pin 8 (GPIO14/TXD)  -> ESP32 RX
- Pin 10 (GPIO15/RXD) -> ESP32 TX
- Pin 6 or 9 (GND)    -> ESP32 GND
- Pin 1 (3.3V)        -> ESP32 VCC (if powering ESP32)

Connect UART-to-USB adapter:
- Adapter RX  -> Pin 8 (TXD from board)
- Adapter TX  -> Pin 10 (RXD to board)
- Adapter GND -> Pin 6 or 9 (GND)
"""

import os
import sys
import time

# 40-pin header pinout (Raspberry Pi compatible)
PIN_MAP = {
    1:  '3.3V',
    2:  '5V',
    3:  'I2C1_SDA / GPIO2',
    4:  '5V',
    5:  'I2C1_SCL / GPIO3',
    6:  'GND',
    7:  'GPIO4',
    8:  'UART_TXD / GPIO14',  # <- UART TX
    9:  'GND',
    10: 'UART_RXD / GPIO15',  # <- UART RX
    11: 'GPIO17',
    12: 'GPIO18',
    13: 'GPIO27',
    14: 'GND',
    15: 'GPIO22',
    16: 'GPIO23',
    17: '3.3V',
    18: 'GPIO24',
    19: 'SPI_MOSI / GPIO10',
    20: 'GND',
    21: 'SPI_MISO / GPIO9',
    22: 'GPIO25',
    23: 'SPI_SCLK / GPIO11',
    24: 'SPI_CE0 / GPIO8',
    25: 'GND',
    26: 'SPI_CE1 / GPIO7',
    # Pins 27-40 are additional GPIO and power
}

def show_pinout():
    """Display 40-pin header pinout"""

    print("=" * 60)
    print("40-Pin Raspberry Pi Header Pinout")
    print("=" * 60)
    print()
    print("⚠️  ALL GPIO PINS: 3.3V LOGIC ONLY!")
    print("   NOT 5V tolerant - will damage Zynq!")
    print()
    print("   Left Side          Right Side")
    print("   ─────────          ──────────")
    print("   1  3.3V      ││    5V         2")
    print("   3  I2C_SDA   ││    5V         4")
    print("   5  I2C_SCL   ││    GND        6")
    print("   7  GPIO4     ││    TXD (TX)   8  <- UART!")
    print("   9  GND       ││    RXD (RX)  10  <- UART!")
    print("  11  GPIO17    ││    GPIO18    12")
    print("  13  GPIO27    ││    GND       14")
    print("  15  GPIO22    ││    GPIO23    16")
    print("  17  3.3V      ││    GPIO24    18")
    print("  19  SPI_MOSI  ││    GND       20")
    print("  21  SPI_MISO  ││    GPIO25    22")
    print("  23  SPI_SCLK  ││    SPI_CE0   24")
    print("  25  GND       ││    SPI_CE1   26")
    print()
    print("For ESP32 UART Connection:")
    print("  Board Pin 8  (TXD) -> ESP32 RX")
    print("  Board Pin 10 (RXD) -> ESP32 TX")
    print("  Board Pin 6  (GND) -> ESP32 GND")
    print()

def test_uart_40pin():
    """Test UART on 40-pin header (pins 8 and 10)"""

    print("=" * 60)
    print("40-Pin Header - UART Test (Pins 8/10)")
    print("=" * 60)
    print()
    print("🔌 Hardware Setup:")
    print("   UART-to-USB Adapter -> 40-Pin Header:")
    print("   - Adapter RX  -> Pin 8  (TXD)")
    print("   - Adapter TX  -> Pin 10 (RXD)")
    print("   - Adapter GND -> Pin 6 or 9 (GND)")
    print()
    print("⚠️  Use 3.3V logic levels only!")
    print("   Most FTDI/CP2102 adapters have 3.3V/5V jumper")
    print("   SET TO 3.3V MODE!")
    print()

    # The 40-pin UART typically maps to a PS UART
    # On Raspberry Pi 5, this is usually UART0 or UART1
    # Also check for USB-UART adapter for testing
    uart_devices = [
        '/dev/ttyUSB0',  # USB-UART adapter (for testing connections)
        '/dev/ttyUSB1',
        '/dev/ttyPS0',  # Usually console
        '/dev/ttyPS1',  # May be available for 40-pin
        '/dev/ttyS0',
        '/dev/ttyS1',
    ]

    print("🔍 Checking for UART devices...")
    print()

    found_device = None
    for device in uart_devices:
        if os.path.exists(device):
            # Check if it's the console
            try:
                with open('/proc/cmdline', 'r') as f:
                    cmdline = f.read()
                    if device.split('/')[-1] in cmdline:
                        print(f"   {device} - ⚠️  Console (skip)")
                        continue
            except:
                pass

            print(f"   {device} - ✅ Available")
            if not found_device:
                found_device = device
        else:
            print(f"   {device} - ❌ Not found")

    print()

    if found_device:
        print(f"📡 Testing {found_device}...")
        print()
        test_uart_device(found_device, "40-Pin Header")
    else:
        print("❌ No available UART devices found")
        print()
        print("💡 The 40-pin UART may be:")
        print("   1. Not enabled in device tree")
        print("   2. Already used by system")
        print("   3. Mapped to different device")
        print()
        print("   Try Arduino shield instead (usually has PS UART)")

def test_uart_device(device, label):
    """Test a UART device"""

    baud = 115200

    try:
        # Configure UART
        print(f"🔧 Configuring {device} at {baud} baud...")
        os.system(f'stty -F {device} {baud} cs8 -cstopb -parenb raw -echo 2>/dev/null')

        # Open device
        uart = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        print(f"✅ Opened {device}")
        print()
        print("📤 Sending test messages...")
        print("   Open serial terminal on your USB adapter at 115200 baud")
        print()

        messages = [
            b"Hello from 40-pin header!\n",
            b"Pin 8 (TXD) -> Your adapter RX\n",
            b"Pin 10 (RXD) <- Your adapter TX\n",
            b"Send me data back!\n",
            b"\n",
        ]

        for msg in messages:
            os.write(uart, msg)
            print(f"   Sent: {msg.decode().strip()}")
            time.sleep(0.3)

        print()
        print("📥 Listening for response...")
        print("   Type in your USB adapter terminal")
        print("   (Waiting 10 seconds...)")
        print()

        # Listen for data
        received_any = False
        start = time.time()
        while time.time() - start < 10:
            try:
                data = os.read(uart, 1024)
                if data:
                    print(f"   ✅ Received: {data.decode().strip()}")
                    received_any = True
            except BlockingIOError:
                time.sleep(0.1)

        if not received_any:
            print("   ⚠️  No data received")
            print("   Check:")
            print("   - Adapter TX connected to Pin 10?")
            print("   - Adapter set to 3.3V mode?")
            print("   - Correct baud rate (115200)?")

        os.close(uart)
        print("\n✅ UART test complete!")

    except PermissionError:
        print(f"❌ Permission denied!")
        print(f"   Try: sudo python3 {sys.argv[0]}")

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()

def test_i2c_40pin():
    """Test I2C on 40-pin header (pins 3 and 5)"""

    print("\n")
    print("=" * 60)
    print("40-Pin Header - I2C Test (Pins 3/5)")
    print("=" * 60)
    print()
    print("🔌 I2C Pins:")
    print("   Pin 3: I2C_SDA (Data)")
    print("   Pin 5: I2C_SCL (Clock)")
    print()

    # Check for I2C devices
    print("🔍 Checking for I2C buses...")
    i2c_devices = []
    for i in range(5):
        device = f'/dev/i2c-{i}'
        if os.path.exists(device):
            i2c_devices.append(device)
            print(f"   ✅ {device}")
        else:
            print(f"   ❌ {device} not found")

    if i2c_devices:
        print()
        print(f"💡 Found {len(i2c_devices)} I2C bus(es)")
        print()
        print("   To scan for devices:")
        print(f"   sudo apt install i2c-tools")
        print(f"   sudo i2cdetect -y 0")
    else:
        print()
        print("❌ No I2C devices found")
        print("   I2C may not be enabled in device tree")

def test_spi_40pin():
    """Test SPI on 40-pin header"""

    print("\n")
    print("=" * 60)
    print("40-Pin Header - SPI Test")
    print("=" * 60)
    print()
    print("🔌 SPI Pins:")
    print("   Pin 19: MOSI (Master Out Slave In)")
    print("   Pin 21: MISO (Master In Slave Out)")
    print("   Pin 23: SCLK (Clock)")
    print("   Pin 24: CE0 (Chip Enable 0)")
    print()

    # Check for SPI devices
    print("🔍 Checking for SPI devices...")
    spi_devices = []
    for i in range(5):
        for cs in range(2):
            device = f'/dev/spidev{i}.{cs}'
            if os.path.exists(device):
                spi_devices.append(device)
                print(f"   ✅ {device}")

    if not spi_devices:
        print("   ❌ No SPI devices found")
        print("   SPI may not be enabled")

if __name__ == "__main__":
    show_pinout()
    test_uart_40pin()
    test_i2c_40pin()
    test_spi_40pin()

    print("\n" + "=" * 60)
    print("⚠️  IMPORTANT: VOLTAGE LEVELS")
    print("=" * 60)
    print()
    print("ALL 40-pin GPIO: 3.3V ONLY!")
    print()
    print("DO NOT connect 5V signals to GPIO pins!")
    print("  ✅ Safe: 3.3V logic (ESP32, most sensors)")
    print("  ❌ DANGER: 5V Arduino, 5V USB-UART without level shifter")
    print()
    print("Power pins:")
    print("  Pin 1, 17: 3.3V output (limited current)")
    print("  Pin 2, 4:  5V output (from USB/power jack)")
    print()
    print("=" * 60)
