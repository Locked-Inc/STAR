# STAR Robot Hardware Specifications

Complete hardware specification for the STAR autonomous robot platform.

---

## Core Computing Platform

### Raspberry Pi 5 (STAR Main Board)

**Processor:**
- **SoC**: Broadcom BCM2712
- **ARM CPU**: Quad-core ARM Cortex-A76 @ 2.4 GHz
- **GPU**: VideoCore VII
- **RAM**: 4GB or 8GB LPDDR4X (8GB recommended)
- **Storage**: MicroSD card (32GB+ recommended)

**Key Features:**
- Dual 4Kp60 HDMI output
- 2 × USB 3.0, 2 × USB 2.0
- Gigabit Ethernet with PoE+ support (via HAT)
- Dual-band 802.11ac WiFi, Bluetooth 5.0/BLE
- 40-pin GPIO header (Raspberry Pi compatible)
- PCIe 2.0 x1 interface
- Significantly faster than Raspberry Pi 4 (~2-3x performance)

---

## Sensors

### 1. SICK TiM561 2D Lidar Scanner

**Manufacturer**: SICK AG
**Model**: TiM561-2050101
**Type**: 2D Laser Scanner (LIDAR)

**Specifications:**
- **Scanning Range**: 0.05m to 10m
- **Angular Range**: 270° (configurable)
- **Angular Resolution**: 0.33° (1/3 degree)
- **Scanning Frequency**: 15 Hz
- **Interface**: Ethernet (10/100 Mbps)
- **Protocol**: SICK COLA-B protocol
- **Data Output**: Polar coordinates (distance, angle)
- **Power**: 24V DC (typically via external power supply)
- **Protection**: IP65 rated

**Connection to Raspberry Pi 5:**
```
TiM561 Ethernet Port <-> Ethernet Switch/Router <-> Raspberry Pi 5 Ethernet
```

**ROS 2 Driver:**
- Package: `sick_scan_xd` or `sick_scan2`
- Topics Published:
  - `/scan` (sensor_msgs/LaserScan) - 2D scan data
  - `/cloud` (sensor_msgs/PointCloud2) - Point cloud format
- Configuration: Ethernet IP-based communication

**Use Case:**
- 2D SLAM (Simultaneous Localization and Mapping)
- Obstacle detection
- Navigation and path planning
- Environment mapping

**Why This Lidar:**
- Industrial-grade reliability
- Wide field of view (270°)
- Ethernet interface (easy integration, no USB/serial needed)
- Excellent ROS 2 support
- Precise measurements for indoor navigation

---

### 2. USB Camera (Person Detection)

**Type**: Standard USB webcam
**Interface**: USB 2.0
**Tested**: Logitech C270 compatible

**Specifications:**
- **Resolution**: 640x480 minimum (configurable)
- **Frame Rate**: 30 fps
- **Interface**: USB Video Class (UVC) - Linux native support
- **Power**: USB bus-powered
- **Device**: `/dev/video0` on Linux

**Connection to Raspberry Pi 5:**
```
USB Camera <-> Raspberry Pi 5 USB 3.0 or USB 2.0 Port
```

**Software Stack:**
- **Driver**: v4l2 (Video4Linux2) - kernel built-in
- **Capture**: OpenCV (cv2.VideoCapture)
- **Processing**: TensorFlow Lite for person detection
- **Models**: MobileNet SSD or similar lightweight models

**Use Case:**
- Person detection and tracking
- Object recognition
- Visual servoing
- Safety monitoring

---

## Actuators & Control

### ESP32 Microcontroller (WiFi & Motor Control)

**Module**: ESP32-WROOM-32 or compatible
**Manufacturer**: Espressif Systems

**Specifications:**
- **CPU**: Dual-core Xtensa LX6 @ 240 MHz
- **RAM**: 520 KB SRAM
- **Flash**: 4 MB (typical)
- **WiFi**: 802.11 b/g/n (2.4 GHz)
- **Bluetooth**: BLE 4.2 (not used in this project)
- **GPIO**: 34 programmable pins
- **UART**: 3 hardware UARTs
- **I/O Voltage**: 3.3V ⚠️ (compatible with Raspberry Pi 5)

**Connection to Raspberry Pi 5:**
```
Raspberry Pi 5 40-Pin Header   ESP32
────────────────────────       ─────────
Pin 8  (GPIO 14 / UART TX) ->  RX (GPIO3)
Pin 10 (GPIO 15 / UART RX) <-  TX (GPIO1)
Pin 6  (GND)               --  GND
Pin 1  (3.3V) [optional]   ->  3.3V
```

**Communication:**
- **Primary**: UART at 115200 baud (GPIO 14/15 on Raspberry Pi)
- **Protocol**: Custom serial protocol or ROS serial
- **Backup**: External USB-UART adapter (temporary)

**Firmware:**
- **Framework**: ESP-IDF (Espressif IoT Development Framework)
- **Language**: C/C++
- **Features**:
  - WiFi AP/Station mode
  - Motor control PWM
  - Sensor interfacing
  - OTA updates

**Use Case:**
- WiFi connectivity for remote control
- Motor driver communication
- Real-time motor speed/position control
- Emergency stop relay
- Status LED control

**Motor Interface:**
From ESP32 to motor drivers (not specified yet):
- PWM signals for speed control
- Direction control pins
- Encoder feedback (optional)

---

## Network & Communication

### Ethernet

**Interface**: Gigabit Ethernet (on Raspberry Pi 5)
**Connector**: RJ45

**Use Cases:**
1. **Development**: SSH access
2. **SICK TiM561**: Lidar data via Ethernet
3. **ROS 2 DDS**: Multi-machine ROS communication
4. **Internet**: Package installation, updates

**Topology:**
```
Raspberry Pi 5 ─┬─> Router/Switch ─> Development Laptop (RViz)
                │
                └─> SICK TiM561 Lidar
```

### WiFi (Built-in + ESP32)

**Built-in WiFi (Raspberry Pi 5):**
- Dual-band 802.11ac WiFi
- Can be used for network connectivity
- Bluetooth 5.0/BLE available

**ESP32 WiFi Bridge (Optional):**
- For additional wireless features
- Motor control relay
- Backup communication channel

**Use Cases:**
- Remote control interface
- Telemetry streaming
- Remote monitoring
- Emergency stop commands

---

## Power Requirements

### Raspberry Pi 5
- **Input**: 5V DC via USB-C (official power supply recommended)
- **Current**: 5A max (27W official power supply)
- **Power**: 12-27W (depending on peripherals and load)
- **Note**: Raspberry Pi 5 has higher power requirements than previous models

### SICK TiM561 Lidar
- **Input**: 24V DC (external power supply required)
- **Current**: ~150 mA typical
- **Power**: ~3.6W

### USB Camera
- **Power**: USB bus-powered (500 mA @ 5V max)
- **Typical**: 200-300 mA

### ESP32 (if used)
- **Power**: 3.3V from Raspberry Pi 5 or separate supply
- **Current**: 80 mA typical, 300 mA peak (WiFi active)
- **Recommendation**: Separate 3.3V regulator for reliable WiFi operation

### Total System Power
- **Minimum**: ~30W (board + lidar + peripherals)
- **Recommended**: 40W power budget with margin for peaks and accessories

---

## Physical Interfaces Summary

| Interface | Purpose | Device | Connection |
|-----------|---------|--------|------------|
| **Ethernet** | Lidar data | SICK TiM561 | RJ45 |
| **Ethernet** | SSH/Development | Laptop | RJ45 |
| **USB 3.0** | Camera | USB Webcam | USB-A port |
| **USB 2.0** | Debug UART | FTDI adapter (temp) | USB-A port |
| **40-pin GPIO** | ESP32 control | ESP32 module | UART pins 8/10 |
| **MicroSD** | Boot/Storage | SD card (32GB+) | MicroSD slot |

---

## I/O Voltage Levels ⚠️

**CRITICAL SAFETY INFORMATION:**

| Component | Logic Level | Safe? |
|-----------|-------------|-------|
| Raspberry Pi 5 GPIO | 3.3V | ⚠️ NOT 5V tolerant! |
| ESP32 GPIO | 3.3V | ✅ Compatible |
| SICK TiM561 | Ethernet (isolated) | ✅ Safe |
| USB Camera | USB (isolated) | ✅ Safe |
| FTDI Adapter | 3.3V mode required | ⚠️ Check jumper! |

**All GPIO pins on Raspberry Pi 5 are 3.3V only - connecting 5V will damage the chip!**

See `voltage-levels-safety.md` for complete safety guide.

---

## Expansion Options

### 40-Pin GPIO Header
- **Compatibility**: Standard Raspberry Pi HAT compatible
- **Voltage**: 3.3V I/O (NOT 5V tolerant!)
- **Pins**: 28 GPIO pins available
- **Power**: 5V and 3.3V power rails available
- **Protocols**: UART, I2C, SPI, PWM
- **Current Use**: ESP32 connection, future HAT expansion

### I2C Buses
- **Available**: Multiple I2C buses (`/dev/i2c-1`, `/dev/i2c-11`, etc.)
- **Voltage**: 3.3V
- **Speed**: Standard (100kHz), Fast (400kHz), Fast-plus (1MHz)
- **Use**: Additional sensors (IMU, compass, etc.)

### SPI Interface
- **Available**: SPI0, SPI1 (via GPIO header)
- **Voltage**: 3.3V
- **Speed**: Up to 125 MHz
- **Use**: High-speed peripherals

### PCIe Interface
- **Version**: PCIe 2.0 x1
- **Speed**: Up to 5 GT/s
- **Use**: NVMe SSDs, AI accelerators (via adapter)

---

## Software Requirements for Hardware

### Lidar (SICK TiM561)
- ROS 2 Humble
- `sick_scan_xd` or `sick_scan2` package
- Ethernet network configuration
- SLAM toolbox (slam_toolbox or cartographer)

### Camera (USB Webcam)
- v4l-utils (Video4Linux utilities)
- OpenCV (cv2 - Python or C++)
- TensorFlow Lite (person detection models)

### ESP32
- ESP-IDF toolchain
- Serial communication libraries
- Custom firmware (in `esp-firmware/` directory)

### System Software
- Raspberry Pi OS (64-bit)
- Python 3.11+
- OpenCV for computer vision
- Standard Linux tools and libraries

---

## Performance Targets

### Lidar Processing
- **Scan Rate**: 15 Hz (SICK TiM561 native)
- **SLAM Update**: 10 Hz minimum
- **Latency**: <100ms scan-to-map

### Vision Processing
- **Frame Rate**: 15-30 fps
- **Detection Latency**: <200ms
- **Model**: MobileNet SSD or similar

### ESP32 Communication
- **Baud Rate**: 115200 (UART1 enabled)
- **Command Latency**: <10ms
- **Update Rate**: 50 Hz motor control

### CPU Performance
- **ARM Cortex-A76**: Significantly faster than Cortex-A9 (Raspberry Pi 5)
- **Multi-threading**: Quad-core for parallel processing
- **Optional**: PCIe AI accelerator for neural network inference

---

## Current Hardware Status

### ✅ Tested & Working (with Raspberry Pi 5):
- Raspberry Pi 5 (booting, SSH, peripherals)
- USB 3.0 and USB 2.0 ports
- Ethernet connectivity (Gigabit)
- Built-in WiFi and Bluetooth
- I2C buses available
- SD card storage

### 📋 Ready to Connect:
- SICK TiM561 Lidar (Ethernet - just plug in)
- USB Camera (USB - just plug in)
- ESP32 (connect to GPIO header UART pins)

### ⚠️ Needs Configuration:
- Serial UART for ESP32 (enable via raspi-config)
- WiFi network setup (built-in or via ESP32)
- Motor drivers (specification TBD)

---

## Related Documentation

- **Voltage Safety**: `voltage-levels-safety.md`
- **Hardware Testing**: `hardware-testing-guide.md`
- **ROS Integration**: `ros-on-zynq-guide.md`
- **Lidar Visualization**: `lidar-visualization-options.md`
- **Customization Plan**: `customization-game-plan.md`

---

## Vendor Links

### SICK TiM561
- Product Page: https://www.sick.com/tim5xx
- ROS Driver: https://github.com/SICKAG/sick_scan_xd
- Manual: Available from SICK AG

### Raspberry Pi 5
- Board Info: https://www.raspberrypi.com/products/raspberry-pi-5/
- Documentation: https://www.raspberrypi.com/documentation/
- GPIO Pinout: https://pinout.xyz/

### ESP32
- Manufacturer: https://www.espressif.com/en/products/socs/esp32
- ESP-IDF: https://docs.espressif.com/projects/esp-idf/

---

## Bill of Materials (BOM)

| Item | Quantity | Notes |
|------|----------|-------|
| Raspberry Pi 5 (8GB) | 1 | Core platform |
| Official RPi 5 Power Supply | 1 | 5V 5A USB-C (27W) |
| Active Cooler (RPi 5) | 1 | Recommended for cooling |
| SICK TiM561 Lidar | 1 | 2D laser scanner |
| USB Webcam | 1 | Logitech C270 or compatible |
| ESP32 Module | 1 | WROOM-32 or compatible (optional) |
| MicroSD Card (64GB) | 1 | Class 10 or UHS-I |
| Ethernet Cable | 2 | For lidar and development |
| FTDI USB-UART | 1 | FT232RL (testing/debug) |
| 24V Power Supply | 1 | For SICK TiM561 |
| Jumper Wires | Set | For ESP32 connection |
| Motor Drivers | TBD | Specification needed |
| Chassis/Frame | TBD | Mechanical design |

**Total Estimated Cost**: ~$500-850 (excluding motors/chassis)
