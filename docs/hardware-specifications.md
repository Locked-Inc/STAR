# STAR Robot Hardware Specifications

Complete hardware specification for the STAR autonomous robot platform.

---

## Core Computing Platform

### PYNQ-Z2 Development Board (STAR-Z2)

**Processor:**
- **SoC**: Xilinx Zynq-7020
- **ARM CPU**: Dual-core ARM Cortex-A9 @ 650 MHz
- **FPGA**: Artix-7 FPGA (85K logic cells)
- **RAM**: 512 MB DDR3
- **Storage**: MicroSD card (28.9 GB tested, expandable)

**Key Features:**
- PS-PL communication via AXI bus
- Programmable logic for hardware acceleration
- Low-power embedded Linux platform

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

**Connection to PYNQ-Z2:**
```
TiM561 Ethernet Port <-> Ethernet Switch/Router <-> PYNQ-Z2 Ethernet
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

**Connection to PYNQ-Z2:**
```
USB Camera <-> PYNQ-Z2 USB Host Port
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
- **I/O Voltage**: 3.3V ⚠️ (compatible with PYNQ-Z2)

**Connection to PYNQ-Z2:**
```
PYNQ-Z2 40-Pin Header          ESP32
────────────────────────       ─────────
Pin 8  (UART1 TX)          ->  RX (GPIO3)
Pin 10 (UART1 RX)          <-  TX (GPIO1)
Pin 6  (GND)               --  GND
Pin 1  (3.3V) [optional]   ->  3.3V
```

**Communication:**
- **Primary**: UART at 115200 baud (after UART1 enabled)
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

**Interface**: 10/100 Mbps Ethernet (on PYNQ-Z2)
**Connector**: RJ45

**Use Cases:**
1. **Development**: SSH access (192.168.2.99)
2. **SICK TiM561**: Lidar data via Ethernet
3. **ROS 2 DDS**: Multi-machine ROS communication
4. **Internet**: Package installation, updates

**Topology:**
```
PYNQ-Z2 ─┬─> Router/Switch ─> Development Laptop (RViz)
         │
         └─> SICK TiM561 Lidar
```

### WiFi (via ESP32)

**Use Cases:**
- Remote control interface
- Telemetry streaming
- Remote monitoring
- Emergency stop commands

---

## Power Requirements

### PYNQ-Z2 Board
- **Input**: 5V DC via micro USB or barrel jack
- **Current**: ~2A typical, 2.5A max
- **Power**: 10-12.5W

### SICK TiM561 Lidar
- **Input**: 24V DC (external power supply required)
- **Current**: ~150 mA typical
- **Power**: ~3.6W

### USB Camera
- **Power**: USB bus-powered (500 mA @ 5V max)
- **Typical**: 200-300 mA

### ESP32
- **Power**: 3.3V from PYNQ-Z2 or separate supply
- **Current**: 80 mA typical, 300 mA peak (WiFi active)
- **Recommendation**: Separate 3.3V regulator for reliable WiFi

### Total System Power
- **Minimum**: ~20W (board + lidar + peripherals)
- **Recommended**: 25W power budget with margin

---

## Physical Interfaces Summary

| Interface | Purpose | Device | Connection |
|-----------|---------|--------|------------|
| **Ethernet** | Lidar data | SICK TiM561 | RJ45 |
| **Ethernet** | SSH/Development | Laptop | RJ45 |
| **USB** | Camera | USB Webcam | USB-A port |
| **USB** | Debug UART | FTDI adapter (temp) | USB-A port |
| **40-pin GPIO** | ESP32 control | ESP32 module | UART1 pins 8/10 |
| **MicroSD** | Boot/Storage | SD card (28.9 GB) | MicroSD slot |

---

## I/O Voltage Levels ⚠️

**CRITICAL SAFETY INFORMATION:**

| Component | Logic Level | Safe? |
|-----------|-------------|-------|
| PYNQ-Z2 GPIO | 3.3V | ⚠️ NOT 5V tolerant! |
| ESP32 GPIO | 3.3V | ✅ Compatible |
| SICK TiM561 | Ethernet (isolated) | ✅ Safe |
| USB Camera | USB (isolated) | ✅ Safe |
| FTDI Adapter | 3.3V mode required | ⚠️ Check jumper! |

**All GPIO pins on PYNQ-Z2 are 3.3V only - connecting 5V will damage the chip!**

See `voltage-levels-safety.md` for complete safety guide.

---

## Expansion Options

### Arduino Shield Connector
- **Compatibility**: 3.3V Arduino shields only (not 5V!)
- **Pins**: Digital 0-13, Analog 0-5
- **Current Use**: Reserved for future expansion

### PMOD Connectors (A & B)
- **Voltage**: 3.3V
- **Pins**: 8 per connector (signal + power)
- **Protocol**: GPIO, I2C, SPI (requires FPGA bitstream)
- **Current Use**: Available for sensors/peripherals

### I2C Buses
- **Available**: 2 buses (`/dev/i2c-0`, `/dev/i2c-1`)
- **Voltage**: 3.3V
- **Use**: Additional sensors (IMU, compass, etc.)

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

### FPGA
- Vivado (for bitstream generation)
- PYNQ Python library
- Custom AXI IP cores for acceleration

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

### FPGA Acceleration
- **Target**: 5-10x speedup for SLAM algorithms
- **Latency**: <5ms for critical operations

---

## Current Hardware Status

### ✅ Tested & Working:
- PYNQ-Z2 board (booting, SSH, peripherals)
- USB ports (FTDI adapter detected)
- Ethernet connectivity
- I2C buses available
- SD card storage

### 📋 Ready to Connect:
- SICK TiM561 Lidar (Ethernet - just plug in)
- USB Camera (USB - just plug in)
- ESP32 (needs UART1 enabled in software)

### ⚠️ Needs Configuration:
- UART1 for ESP32 (device tree modification)
- WiFi network setup (via ESP32)
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

### PYNQ-Z2
- Board Info: https://www.tulembedded.com/FPGA/ProductsPYNQ-Z2.html
- Documentation: https://pynq.readthedocs.io/

### ESP32
- Manufacturer: https://www.espressif.com/en/products/socs/esp32
- ESP-IDF: https://docs.espressif.com/projects/esp-idf/

---

## Bill of Materials (BOM)

| Item | Quantity | Notes |
|------|----------|-------|
| PYNQ-Z2 Board | 1 | Core platform |
| SICK TiM561 Lidar | 1 | 2D laser scanner |
| USB Webcam | 1 | Logitech C270 or compatible |
| ESP32 Module | 1 | WROOM-32 or compatible |
| MicroSD Card | 1 | 32 GB minimum |
| Ethernet Cable | 2 | For lidar and development |
| FTDI USB-UART | 1 | FT232RL (testing/debug) |
| 24V Power Supply | 1 | For SICK TiM561 |
| 5V Power Supply | 1 | For PYNQ-Z2 (2.5A min) |
| Jumper Wires | Set | For ESP32 connection |
| Motor Drivers | TBD | Specification needed |
| Chassis/Frame | TBD | Mechanical design |

**Total Estimated Cost**: ~$500-800 (excluding motors/chassis)
