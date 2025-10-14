#!/usr/bin/env python3
"""
Test 40-Pin Header UART with USB-UART Adapter

Physical Setup:
  Board 40-Pin Header    ->    FTDI USB-UART Adapter
  ─────────────────────        ──────────────────────
  Pin 8  (TXD)           ->    RX (receive from board)
  Pin 10 (RXD)           <-    TX (send to board)
  Pin 6  (GND)           --    GND

  FTDI adapter is plugged into board's USB port -> appears as /dev/ttyUSB0

Connection via SSH (Ethernet) to run this script.

This script will:
1. Find the device for 40-pin header UART (ttyPS1 or similar)
2. Open /dev/ttyUSB0 (your FTDI adapter)
3. Send data FROM 40-pin header TO FTDI adapter (test TX)
4. Send data FROM FTDI adapter TO 40-pin header (test RX)
"""

import os
import sys
import time
import select

def find_uart_devices():
    """Find all available UART devices"""

    devices = {
        'usb_adapter': None,
        'ps_uart': None,
        'all': []
    }

    # Check for USB-UART adapter
    for i in range(5):
        dev = f'/dev/ttyUSB{i}'
        if os.path.exists(dev):
            devices['usb_adapter'] = dev
            devices['all'].append(dev)
            break

    # Check for PS UART (40-pin header)
    # ttyPS1 would be the second UART, ttyPS0 is console
    ps_candidates = ['/dev/ttyPS1', '/dev/ttyS1', '/dev/ttyAMA1']

    for dev in ps_candidates:
        if os.path.exists(dev):
            # Make sure it's not the console
            try:
                with open('/proc/cmdline', 'r') as f:
                    if dev.split('/')[-1] not in f.read():
                        devices['ps_uart'] = dev
                        devices['all'].append(dev)
                        break
            except:
                devices['ps_uart'] = dev
                devices['all'].append(dev)
                break

    return devices

def configure_uart(device, baud):
    """Configure UART device"""
    cmd = f'stty -F {device} {baud} cs8 -cstopb -parenb raw -echo'
    os.system(cmd + ' 2>/dev/null')

def test_40pin_to_adapter(uart_40pin, uart_adapter):
    """Test TX from 40-pin header to USB adapter (Board transmits)"""

    print("=" * 60)
    print("Test 1: 40-Pin TX -> USB Adapter RX")
    print("=" * 60)
    print()
    print("🔌 Wiring check:")
    print(f"   40-pin Pin 8 (TXD) -> FTDI RX")
    print()

    print(f"📤 Sending from {uart_40pin} (40-pin header)...")
    print(f"📥 Receiving on {uart_adapter} (USB adapter)...")
    print()

    try:
        # Open both devices
        fd_40pin = os.open(uart_40pin, os.O_WRONLY | os.O_NOCTTY)
        fd_adapter = os.open(uart_adapter, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)

        test_messages = [
            b"Hello from 40-pin header!\n",
            b"Pin 8 TX test\n",
            b"STAR Robot ESP32 connection\n",
            b"0123456789ABCDEF\n",
        ]

        passed = 0
        failed = 0

        for msg in test_messages:
            # Clear adapter buffer
            try:
                os.read(fd_adapter, 1024)
            except:
                pass

            # Send from 40-pin
            os.write(fd_40pin, msg)
            print(f"   Sent: {msg.decode().strip()}")

            # Wait for data
            time.sleep(0.2)

            # Receive on adapter
            try:
                received = os.read(fd_adapter, len(msg))
                if received == msg:
                    print(f"   ✅ Received correctly!")
                    passed += 1
                else:
                    print(f"   ❌ Mismatch: {received}")
                    failed += 1
            except BlockingIOError:
                print(f"   ❌ No data received")
                failed += 1

            print()

        os.close(fd_40pin)
        os.close(fd_adapter)

        print("-" * 60)
        print(f"Results: {passed} passed, {failed} failed")

        if passed == len(test_messages):
            print("✅ 40-pin TX (Pin 8) is WORKING!")
        else:
            print("⚠️  Check wiring: Pin 8 -> FTDI RX")

        return passed == len(test_messages)

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_adapter_to_40pin(uart_40pin, uart_adapter):
    """Test RX from USB adapter to 40-pin header (Board receives)"""

    print()
    print("=" * 60)
    print("Test 2: USB Adapter TX -> 40-Pin RX")
    print("=" * 60)
    print()
    print("🔌 Wiring check:")
    print(f"   FTDI TX -> 40-pin Pin 10 (RXD)")
    print()

    print(f"📤 Sending from {uart_adapter} (USB adapter)...")
    print(f"📥 Receiving on {uart_40pin} (40-pin header)...")
    print()

    try:
        # Open both devices
        fd_40pin = os.open(uart_40pin, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        fd_adapter = os.open(uart_adapter, os.O_WRONLY | os.O_NOCTTY)

        test_messages = [
            b"Hello to 40-pin header!\n",
            b"Pin 10 RX test\n",
            b"ESP32 will send like this\n",
            b"FEDCBA9876543210\n",
        ]

        passed = 0
        failed = 0

        for msg in test_messages:
            # Clear 40-pin buffer
            try:
                os.read(fd_40pin, 1024)
            except:
                pass

            # Send from adapter
            os.write(fd_adapter, msg)
            print(f"   Sent: {msg.decode().strip()}")

            # Wait for data
            time.sleep(0.2)

            # Receive on 40-pin
            try:
                received = os.read(fd_40pin, len(msg))
                if received == msg:
                    print(f"   ✅ Received correctly!")
                    passed += 1
                else:
                    print(f"   ❌ Mismatch: {received}")
                    failed += 1
            except BlockingIOError:
                print(f"   ❌ No data received")
                failed += 1

            print()

        os.close(fd_40pin)
        os.close(fd_adapter)

        print("-" * 60)
        print(f"Results: {passed} passed, {failed} failed")

        if passed == len(test_messages):
            print("✅ 40-pin RX (Pin 10) is WORKING!")
        else:
            print("⚠️  Check wiring: FTDI TX -> Pin 10")

        return passed == len(test_messages)

    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    print("\n" + "=" * 60)
    print("40-Pin Header UART Test with USB Adapter")
    print("=" * 60)
    print()

    # Find devices
    print("🔍 Detecting UART devices...")
    devices = find_uart_devices()

    print()
    print("Found devices:")
    if devices['usb_adapter']:
        print(f"  ✅ USB Adapter: {devices['usb_adapter']}")
    else:
        print(f"  ❌ USB Adapter: Not found")

    if devices['ps_uart']:
        print(f"  ✅ 40-Pin UART: {devices['ps_uart']}")
    else:
        print(f"  ❌ 40-Pin UART: Not found")

    print()

    # Check if we have what we need
    if not devices['usb_adapter']:
        print("❌ No USB-UART adapter detected!")
        print("   Is FTDI adapter plugged into USB port?")
        return 1

    if not devices['ps_uart']:
        print("⚠️  No PS UART available for 40-pin header!")
        print()
        print("💡 This means:")
        print("   UART1 is not enabled in device tree")
        print("   The 40-pin pins 8 and 10 are not configured as UART")
        print()
        print("   Options:")
        print("   1. Use GPIO software UART (slower, ~9600 baud)")
        print("   2. Enable UART1 in device tree (requires rebuild)")
        print("   3. Use external USB-UART adapter for ESP32")
        print()
        print("   Run: bash ~/check_uart_devices.sh for more info")
        return 1

    # Configure both UARTs
    baud = 115200
    print(f"🔧 Configuring UARTs at {baud} baud...")
    configure_uart(devices['ps_uart'], baud)
    configure_uart(devices['usb_adapter'], baud)
    print()

    # Run tests
    tx_works = test_40pin_to_adapter(devices['ps_uart'], devices['usb_adapter'])
    rx_works = test_adapter_to_40pin(devices['ps_uart'], devices['usb_adapter'])

    # Summary
    print()
    print("=" * 60)
    print("📊 Final Summary")
    print("=" * 60)
    print()

    if tx_works and rx_works:
        print("✅ BOTH TX and RX working on 40-pin header!")
        print()
        print("🎉 You can now connect ESP32:")
        print(f"   ESP32 RX -> Pin 8  (40-pin header)")
        print(f"   ESP32 TX -> Pin 10 (40-pin header)")
        print(f"   ESP32 GND -> Pin 6 (GND)")
        print()
        print(f"   Communication speed: {baud} baud")
    elif tx_works:
        print("⚠️  Only TX working (Pin 8)")
        print("   Check wiring for RX (Pin 10)")
    elif rx_works:
        print("⚠️  Only RX working (Pin 10)")
        print("   Check wiring for TX (Pin 8)")
    else:
        print("❌ Neither TX nor RX working")
        print("   Check all wiring and connections")

    print()

if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted")
        sys.exit(1)
