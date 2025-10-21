# Raspberry Pi 5 Hardware Testing Guide

## Quick Hardware Test Procedure

After successfully booting your Raspberry Pi 5, follow these tests to verify all hardware interfaces work correctly.

---

## Test 1: USB Webcam

### Setup
1. Connect USB webcam to any USB port on Raspberry Pi 5
2. SSH into board: `ssh pi@star-robot.local` (or use the IP address)

### Run Test Script

```bash
# Copy test script to board
exit  # Exit SSH first
scp ~/Documents/git/STAR/test-scripts/test_webcam.py pi@star-robot.local:~/
ssh pi@star-robot.local

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
   ✅ Frame 1/5: /home/pi/webcam_test/test_frame_000.jpg
   ...

✅ Camera test PASSED!
```

### Verify Images
```bash
# Check captured images
ls -lh ~/webcam_test/

# Copy to your laptop to view
exit
scp pi@star-robot.local:~/webcam_test/*.jpg ~/Downloads/
```

### Troubleshooting
- **No /dev/video0**: Camera not detected, check USB connection
- **Permission denied**: Run `sudo chmod 666 /dev/video0`
- **Failed to open**: Try different USB port or camera

---

## Test 2: UART (Hardware Serial)

### Recommended Method: Hardware UART
Uses GPIO header pins 8 (TX) and 10 (RX).

### Physical Connections

**40-Pin GPIO Header:**
- **Pin 8 (GPIO 14 / UART TX)**: UART transmit
- **Pin 10 (GPIO 15 / UART RX)**: UART receive
- **Pin 6 (GND)**: Ground

**For loopback test:** Connect Pin 8 to Pin 10 with jumper wire

**For ESP32 connection:**
- Pin 8 (TX) → ESP32 RX (GPIO3)
- Pin 10 (RX) → ESP32 TX (GPIO1)
- Pin 6 (GND) → GND

### Enable Serial Port

First, enable the serial port in `raspi-config`:
```bash
sudo raspi-config
# Navigate to: Interface Options -> Serial Port
# Serial login shell: No
# Serial hardware: Yes
sudo reboot
```

### Run Test

```bash
# Copy test script
scp ~/Documents/git/STAR/test-scripts/test_uart_hardware.py pi@star-robot.local:~/
ssh pi@star-robot.local

# Install pyserial
sudo apt install -y python3-serial

# Check available UART devices
ls -l /dev/ttyAMA*
# Should show: /dev/ttyAMA0 (primary UART)

# Run test (modify script to use /dev/ttyAMA0)
python3 ~/test_uart_hardware.py
```

### Expected Output (with loopback)
```
✅ Opened /dev/ttyAMA0 at 115200 baud

📤 Sending test messages...
   Sent: Hello UART!
   ✅ Received: Hello UART!

   Sent: Testing 123
   ✅ Received: Testing 123
```

### If /dev/ttyAMA0 doesn't exist

UART might not be enabled. Options:

**Option A: Enable via raspi-config** (recommended)
- Run `sudo raspi-config`
- Navigate to Interface Options -> Serial Port
- Disable serial login shell, enable serial hardware

**Option B: Use USB-to-Serial adapter**
- Connect USB-to-Serial adapter to USB port
- Will show up as `/dev/ttyUSB0`
- Modify script to use `/dev/ttyUSB0`

---

## Test 3: GPIO (General Purpose)

Raspberry Pi 5 has many GPIO pins available for various uses.

### Simple LED Blink Test

Connect an LED (with 220Ω resistor) between GPIO 17 and GND to test.

### Run Test

```bash
ssh pi@star-robot.local

# Install GPIO library
sudo apt install -y python3-lgpio python3-rpi-lgpio

# Test GPIO
python3
```

```python
from gpiozero import LED
from time import sleep

# Use GPIO 17 (Pin 11)
led = LED(17)

# Blink 10 times
for i in range(10):
    led.on()
    print(f"LED ON {i+1}/10")
    sleep(0.5)
    led.off()
    sleep(0.5)

print("Test complete!")
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

### Test with Python (if devices available)

```python
import smbus
import time

# Example: Read from I2C device at address 0x50
bus = smbus.SMBus(1)
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

## What's Installed on Raspberry Pi 5

### ✅ Default on Raspberry Pi OS:
- Raspberry Pi OS (64-bit) Bookworm
- Linux kernel 6.1+ (Raspberry Pi optimized)
- Python 3.11+
- Standard Linux utilities
- SSH, networking
- Built-in WiFi and Bluetooth drivers

### ❌ To Be Installed for STAR:
- Java 17 / Gradle / Kotlin (for Robot Gateway)
- OpenCV (cv2 for Python)
- ROS 2 Humble (for SLAM)
- TensorFlow Lite (for vision AI)
- SICK lidar drivers (sick_scan_xd)
- ESP-IDF toolchain (for ESP32 development)

---

## Next Steps After Hardware Testing

Once all hardware tests pass:

1. **Document any issues** found with specific pins/peripherals
2. **Plan ESP32 integration** (connect to GPIO UART pins)
3. **Install required software** for robot operation
4. **Configure ROS 2** for SLAM and sensor integration
5. **Test SICK TiM561** lidar connection

---

## Quick Reference: Pin Mappings

### 40-Pin GPIO Header (Raspberry Pi 5)
- **Pin 1**: 3.3V power
- **Pin 2, 4**: 5V power
- **Pin 6, 9, 14, 20, 25, 30, 34, 39**: Ground
- **Pin 8 (GPIO 14)**: UART TX
- **Pin 10 (GPIO 15)**: UART RX
- **Pin 3 (GPIO 2), Pin 5 (GPIO 3)**: I2C SDA/SCL
- **Pin 19, 21, 23**: SPI MOSI, MISO, SCLK
- **Remaining pins**: General GPIO (see pinout.xyz for full details)

### For ESP32 Connection
- Pin 8 (GPIO 14 / UART TX) → ESP32 RX
- Pin 10 (GPIO 15 / UART RX) → ESP32 TX
- Pin 6 (GND) → ESP32 GND
- Pin 1 (3.3V) → ESP32 VCC (optional, can use separate power)

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
