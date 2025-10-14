# PYNQ-Z2 Hardware Testing Guide

## Quick Hardware Test Procedure

After successfully booting your STAR-Z2 board, follow these tests to verify all hardware interfaces work correctly.

---

## Test 1: USB Webcam

### Setup
1. Connect USB webcam to any USB port on PYNQ-Z2
2. SSH into board: `ssh xilinx@192.168.2.99`

### Run Test Script

```bash
# Copy test script to board
exit  # Exit SSH first
scp ~/Documents/git/STAR/test_webcam.py xilinx@192.168.2.99:~/
ssh xilinx@192.168.2.99

# Install OpenCV if needed
sudo apt update
sudo apt install -y python3-opencv

# Run webcam test
python3 ~/test_webcam.py
```

### Expected Output
```
🔌 USB Devices:
Bus 001 Device 002: ID 046d:0825 Logitech, Inc. Webcam C270
...

🔍 Scanning for video devices...
✅ Found 1 video device(s):
   - /dev/video0

📹 Opening camera: /dev/video0
✅ Camera opened successfully!

📊 Camera Properties:
   Resolution: 640x480
   FPS: 30.0

📸 Capturing test frames...
   ✅ Frame 1/5: /home/xilinx/webcam_test/test_frame_000.jpg
   ...

✅ Camera test PASSED!
```

### Verify Images
```bash
# Check captured images
ls -lh ~/webcam_test/

# Copy to your laptop to view
exit
scp xilinx@192.168.2.99:~/webcam_test/*.jpg ~/Downloads/
```

### Troubleshooting
- **No /dev/video0**: Camera not detected, check USB connection
- **Permission denied**: Run `sudo chmod 666 /dev/video0`
- **Failed to open**: Try different USB port or camera

---

## Test 2: UART (Hardware - PS UART1)

### Recommended Method: Hardware UART
Uses Arduino shield pins AR0 (TX) and AR1 (RX).

### Physical Connections

**Arduino Shield Connector (J1):**
- **AR0 (TX)**: Pin 0 on digital header
- **AR1 (RX)**: Pin 1 on digital header
- **GND**: Any ground pin

**For loopback test:** Connect AR0 to AR1 with jumper wire

**For ESP32 connection:**
- AR0 (TX) → ESP32 RX
- AR1 (RX) → ESP32 TX
- GND → GND

### Run Test

```bash
# Copy test script
scp ~/Documents/git/STAR/test_uart_hardware.py xilinx@192.168.2.99:~/
ssh xilinx@192.168.2.99

# Install pyserial
sudo apt install -y python3-serial

# Check available UART devices
ls -l /dev/ttyPS*
# Should show: /dev/ttyPS0 (console) and /dev/ttyPS1 (if available)

# Run test
python3 ~/test_uart_hardware.py
```

### Expected Output (with loopback)
```
✅ Opened /dev/ttyPS1 at 115200 baud

📤 Sending test messages...
   Sent: Hello UART!
   ✅ Received: Hello UART!

   Sent: Testing 123
   ✅ Received: Testing 123
```

### If /dev/ttyPS1 doesn't exist

UART1 might not be enabled in the device tree. Two options:

**Option A: Use USB-to-Serial adapter**
- Connect USB-to-Serial adapter to USB port
- Will show up as `/dev/ttyUSB0`
- Modify script to use `/dev/ttyUSB0`

**Option B: Use GPIO Software UART** (see below)

---

## Test 3: UART (Software - GPIO Bit-Banging)

If hardware UART isn't available, use software UART with any GPIO pins.

### Recommended GPIO Pins
- **TX**: GPIO 54 (EMIO[0])
- **RX**: GPIO 55 (EMIO[1])

These are extended MIO pins not used by other peripherals.

### Run Test

```bash
scp ~/Documents/git/STAR/test_uart_gpio.py xilinx@192.168.2.99:~/
ssh xilinx@192.168.2.99

# Simple GPIO toggle test first
python3 ~/test_uart_gpio.py simple

# Full software UART test
python3 ~/test_uart_gpio.py
```

### Expected Output
```
🔍 Scanning for available GPIO pins...
   ✅ GPIO 54 is available
   ✅ GPIO 55 is available

📌 Using pins:
   TX: GPIO 54
   RX: GPIO 55

✅ GPIO initialized

📤 Sending via software UART at 9600 baud...
   Message: Hello UART!

✅ Sent 11 bytes in 0.011s
```

### Verify with Oscilloscope
- Connect oscilloscope to GPIO 54
- Trigger on falling edge
- Should see UART waveform at 9600 baud

**Note:** Software UART is limited to ~9600 baud max on ARM Cortex-A9 due to timing.

---

## Test 4: I2C

### Check Available I2C Buses

```bash
ssh xilinx@192.168.2.99

# Install i2c-tools
sudo apt install -y i2c-tools

# List I2C buses
ls -l /dev/i2c-*

# Scan for devices on I2C bus 0
sudo i2cdetect -y 0
```

### Expected Output
```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
...
```

If you have I2C devices connected, they'll show up as hex addresses.

### Test with PYNQ (if devices available)

```python
from pynq import MMIO
import time

# Example: Read from I2C device at address 0x50
# Implement based on your specific I2C hardware
```

---

## Test 5: GPIO (General Purpose)

### Simple LED Blink Test

```bash
ssh xilinx@192.168.2.99
python3
```

```python
from pynq import GPIO
import time

# Use GPIO 54 (EMIO[0])
led = GPIO(54, 'out')

# Blink 10 times
for i in range(10):
    led.write(1)
    print(f"LED ON {i+1}/10")
    time.sleep(0.5)
    led.write(0)
    time.sleep(0.5)

print("Test complete!")
```

Connect LED (with 220Ω resistor) between GPIO 54 and GND to see it blink.

---

## Test 6: Ethernet

Already tested! You're using it for SSH at `192.168.2.99`.

### Additional Network Tests

```bash
# Check interface
ip addr show eth0

# Ping gateway (your laptop)
ping -c 4 192.168.2.1

# Test internet (if connected to router)
ping -c 4 8.8.8.8

# DNS test
ping -c 4 google.com
```

---

## Test 7: Check Installed Software

### Current Java Version
```bash
java -version
# Expected: openjdk version "1.8.0_xxx" (will upgrade to 17)
```

### Check for Jupyter
```bash
jupyter --version
# Expected: Shows version (will remove in customization)
```

### Check for ROS
```bash
which ros2
# Expected: Not found (will install in customization)
```

### Python Packages
```bash
pip3 list | grep -E 'numpy|opencv|pynq'
```

---

## Test Summary Checklist

After running all tests, you should have verified:

- [ ] USB webcam works (`/dev/video0` accessible, images captured)
- [ ] UART communication possible (hardware or software)
- [ ] I2C buses available (`/dev/i2c-*` exists)
- [ ] GPIO pins controllable (can toggle pins)
- [ ] Ethernet working (SSH access, ping)
- [ ] Current software versions identified

---

## What's Installed Currently

### ✅ Working:
- Ubuntu 22.04 Jammy ARM
- Linux kernel 5.15.19-xilinx-v2022.1
- PYNQ 3.0.1 library
- Python 3.10
- NumPy, SciPy, OpenCV
- Jupyter Notebook (to be removed)
- Java 8 (to be upgraded to 17)
- SSH, networking

### ❌ Not Installed (To Add):
- Java 17 / Gradle / Kotlin
- ESP-IDF toolchain
- ROS 2 Humble
- TensorFlow Lite
- SICK lidar drivers

---

## Next Steps After Hardware Testing

Once all hardware tests pass:

1. **Document any issues** found with specific pins/peripherals
2. **Plan ESP32 integration** (which pins to use for UART)
3. **Proceed with image customization** following `/home/bsikar/Documents/git/STAR/docs/customization-game-plan.md`

---

## Quick Reference: Pin Mappings

### Arduino Shield (J1)
- AR0 (Digital 0): UART1 TX / GPIO
- AR1 (Digital 1): UART1 RX / GPIO
- AR2-AR13: Additional digital I/O

### 40-Pin Raspberry Pi Header (J2)
- For ESP32 connection
- I2C, SPI, UART, GPIO available
- Check STAR-Z2 schematic for exact pinout

### PMOD Connectors
- PMOD A/B: 8 GPIO pins each
- Good for custom peripherals

---

## Troubleshooting

### Permission Errors
```bash
# Add user to dialout group (for UART)
sudo usermod -a -G dialout xilinx

# Add to i2c group
sudo usermod -a -G i2c xilinx

# Reboot or re-login for changes
```

### Module Not Found
```bash
# Update package list
sudo apt update

# Install Python package
sudo apt install python3-<package>
# or
pip3 install <package>
```

### Device Not Found
- Check physical connections
- Run `lsusb` to see USB devices
- Check kernel messages: `dmesg | tail -20`
