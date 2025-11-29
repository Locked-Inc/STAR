# Raspberry Pi 5 Hardware Test Scripts

This directory contains test scripts for verifying hardware interfaces on the Raspberry Pi 5 board.

## Quick Start

### Copy all scripts to board:
```bash
cd ~/Documents/git/STAR/test-scripts
scp *.py *.sh root@star-robot.local:~/test-scripts/
```

### Or copy individually:
```bash
scp test_uart_usb.py root@star-robot.local:~/
scp test_40pin_header.py root@star-robot.local:~/
# etc.
```

---

## Test Scripts Overview

### USB Testing
| Script | Purpose | Dependencies |
|--------|---------|--------------|
| `test_usb.sh` | Detect USB devices (flash drive, mouse, UART adapter) | None |

**Usage:**
```bash
bash ~/test_usb.sh
```

---

### UART Testing

#### Primary UART Scripts (Recommended)

| Script | Purpose | Hardware Needed |
|--------|---------|-----------------|
| `test_uart_usb.py` | Test FTDI USB-UART adapter | FTDI FT232RL or similar |
| `test_40pin_header.py` | Test 40-pin header UART (best for ESP32) | USB-UART adapter |
| `test_arduino_shield.py` | Test Arduino shield UART | USB-UART adapter |

**Usage:**
```bash
# Test USB-UART adapter (loopback test)
python3 ~/test_uart_usb.py

# Test 40-pin header (for ESP32 connection)
sudo python3 ~/test_40pin_header.py

# Test Arduino shield
sudo python3 ~/test_arduino_shield.py
```

#### Alternative UART Scripts (Fallback)

| Script | Purpose | Notes |
|--------|---------|-------|
| `test_uart_simple.py` | Raw UART using stty | No external packages |
| `test_uart_minimal.py` | Minimal UART test | Uses Raspberry Pi library only |
| `test_uart_hardware.py` | PS UART test | Requires pyserial |
| `test_uart_gpio.py` | Software UART (bit-bang) | Slow, ~9600 baud max |

---

### PMOD Testing

| Script | Purpose | Notes |
|--------|---------|-------|
| `test_pmod_a.py` | Test PMOD A connector | Requires FPGA bitstream |
| `test_pmod_b.py` | Test PMOD B connector | Requires FPGA bitstream |

**Note:** PMOD connectors need custom FPGA overlay to use. Use 40-pin header or Arduino shield for easier UART testing.

---

### Webcam Testing

| Script | Purpose | Dependencies |
|--------|---------|--------------|
| `test_webcam_minimal.py` | Detect webcam device | None |
| `test_webcam.py` | Capture images from webcam | opencv (requires internet) |

**Usage:**
```bash
# Just detect webcam
python3 ~/test_webcam_minimal.py

# Capture images (needs opencv)
# sudo apt install python3-opencv
python3 ~/test_webcam.py
```

---

## Hardware Connection Guide

### 40-Pin Header UART (Recommended for ESP32)

**⚠️ ALL PINS ARE 3.3V - NOT 5V TOLERANT!**

```
FTDI Adapter      40-Pin Header
────────────      ─────────────
RX (yellow)   <-  Pin 8  (TXD)
TX (orange)   ->  Pin 10 (RXD)
GND (black)   ->  Pin 6 or 9 (GND)

Set adapter to 3.3V mode!
```

### Arduino Shield UART

```
FTDI Adapter      Arduino Shield
────────────      ──────────────
RX            <-  AR0 (Digital Pin 0)
TX            ->  AR1 (Digital Pin 1)
GND           ->  GND
```

### ESP32 Connection (Final Setup)

```
40-Pin Header     ESP32
─────────────     ─────────────
Pin 8  (TXD)  ->  RX (GPIO3)
Pin 10 (RXD)  <-  TX (GPIO1)
Pin 6  (GND)  --  GND
Pin 1  (3.3V) ->  3.3V (optional)
```

---

## Testing Workflow

### Step 1: Test USB Port
```bash
bash ~/test_usb.sh
```
Should detect FTDI adapter as `/dev/ttyUSB0`

### Step 2: Test USB-UART Adapter (Loopback)
```bash
python3 ~/test_uart_usb.py
# Select option 1 (loopback)
# Connect TX to RX on adapter
```

### Step 3: Test Board UART
```bash
# Try 40-pin header first
sudo python3 ~/test_40pin_header.py

# Or try Arduino shield
sudo python3 ~/test_arduino_shield.py
```

### Step 4: Connect ESP32
Once you find working UART, connect ESP32 and test communication.

---

## Common Issues

### Permission Denied
```bash
# Add user to dialout group (if using non-root user)
sudo usermod -a -G dialout pi

# Or run with sudo
sudo python3 ~/test_uart_usb.py
```

### Device Not Found
```bash
# Check what's connected
lsusb
ls -l /dev/ttyUSB* /dev/ttyPS*

# Check kernel messages
dmesg | tail -20
```

### No Data Received
- Check TX/RX are crossed (TX -> RX, RX -> TX)
- Verify baud rate matches (default: 115200)
- Check GND is connected
- Verify 3.3V logic levels (not 5V!)

---

## Voltage Safety

**⚠️ CRITICAL: All GPIO pins are 3.3V ONLY!**

### Safe Connections:
- ✅ ESP32 (3.3V)
- ✅ FTDI adapter in 3.3V mode
- ✅ Most modern sensors (3.3V)

### Dangerous:
- ❌ 5V Arduino signals (will damage chip!)
- ❌ 5V UART adapters without level shifter
- ❌ Standard Arduino shields (5V)

See `../docs/voltage-levels-safety.md` for complete safety guide.

---

## Related Documentation

- `../docs/hardware-testing-guide.md` - Complete testing procedures
- `../docs/voltage-levels-safety.md` - Voltage safety reference
- `../docs/README.md` - Documentation overview
- `../star-pi5-os/README.md` - Custom OS setup guide

---

## Quick Reference

| Task | Command |
|------|---------|
| Copy all scripts | `scp *.py *.sh root@star-robot.local:~/` |
| List USB devices | `lsusb` |
| List serial ports | `ls -l /dev/ttyUSB* /dev/ttyPS*` |
| Check kernel messages | `dmesg \| tail` |
| Test USB-UART | `python3 ~/test_uart_usb.py` |
| Test 40-pin | `sudo python3 ~/test_40pin_header.py` |

---

## Script Status

| Script | Status | Notes |
|--------|--------|-------|
| test_usb.sh | ✅ Tested | Works, detected FTDI FT232RL |
| test_uart_usb.py | 🧪 Ready | Needs loopback test |
| test_40pin_header.py | 🧪 Ready | Needs hardware connection |
| test_arduino_shield.py | 🧪 Ready | Needs hardware connection |
| test_uart_minimal.py | ✅ Working | No dependencies |
| test_webcam_minimal.py | 🧪 Ready | Needs webcam |
| test_pmod_*.py | ⚠️ Needs bitstream | Requires FPGA config |
