# GEMINI.md

This file provides guidance to Google Gemini when working with code in this repository.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA (Open Source Hardware Association) standards:

- **Controller/Peripheral** - NOT master/slave
  - I2C: Controller device initiates transactions, Peripheral device responds
  - SPI: Controller provides clock, Peripheral responds to chip select
  - 1-Wire: Controller initiates communication, Peripheral responds
- **COPI/CIPO** - NOT MOSI/MISO
  - COPI = Controller Out, Peripheral In (data from controller to peripheral)
  - CIPO = Controller In, Peripheral Out (data from peripheral to controller)
- **Primary/Main** - NOT master (for configuration structures, etc.)

When writing or modifying code, documentation, or comments:
1. Never use "master" or "slave" terminology
2. Never use MOSI/MISO - always use COPI/CIPO
3. Use "primary" or "main" for configuration structures instead of "master"

Note: ESP-IDF and other external APIs may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Project Overview

**STAR (Sensor and Actuator Abstraction Runtime)** - A distributed robotics platform with custom PCB hardware, embedded firmware, and high-level control software.

### Architecture

The STAR platform consists of multiple integrated components:

#### Hardware
- **Main Controller:** Raspberry Pi 5 (quad-core Arm Cortex-A76)
- **Motor Controller:** ESP32-S3-WROOM-1-N16 (dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM, 16 MB Flash)
- **Motors:** 4× 6V brushed DC gearmotors (210 RPM, 1:34 gear ratio, 341 PPR Hall encoders)
- **Motor Drivers:** 4× DRV8243S H-bridge with current sensing
- **Lidar:** RPLiDAR C1 - DTOF LiDAR 360° Laser Range Scanner (12m range, IP54)
- **Battery Management:** BQ7850 (custom BMS PCB)
- **Communication:** 10 Mbps SPI (RPi5 ↔ ESP32), custom PCB with <15cm traces

#### Firmware (`star-esp32-firmware/`)
- **Platform:** ESP32-IDF with PlatformIO
- **Architecture:** Modular framework with Dependency Inversion Principle (DIP)
- **Control:** 100 Hz closed-loop PID velocity control
- **Protocol:** nanopb (Protocol Buffers) with Stop-and-Wait ARQ and CRC-32
- **Key Libraries:** star_motor, star_drv8243, star_encoder, star_pid, star_sensor_ds18b20, star_bus

#### Operating System (`star-rpi5-buildroot/`)
- **Platform:** Custom Buildroot Linux for Raspberry Pi 5
- **Purpose:** Lightweight embedded Linux optimized for SLAM and motor control coordination

#### Protocol (`star-proto/`)
- **Format:** Protocol Buffers (nanopb for ESP32, standard protobuf for RPi5)
- **Transport:** SPI peripheral mode (10 Mbps)
- **Reliability:** Stop-and-Wait ARQ with hardware-accelerated CRC-32
- **Messages:** Motor commands, telemetry, PID tuning, OTA updates

#### MATLAB Toolchain (`matlab/`)
- **Purpose:** Motor system identification and PID controller design
- **Workflow:** Transfer function modeling → PID tuning → discretization → C code generation
- **Motor Model:** G(s) = 3.665 / (0.075s + 1) with τ = 75ms

### Directory Structure

```
STAR/
├── schematic/              # KiCad hardware designs (PCBs, schematics)
├── star-esp32-firmware/    # ESP32-S3 motor control firmware
├── star-rpi5-buildroot/    # Custom Linux image for Raspberry Pi 5
├── star-proto/             # Protocol Buffers schemas (SPI communication)
├── star-gateway/           # Gateway service (if applicable)
├── star-ui/                # User interface (if applicable)
├── matlab/                 # Motor modeling and PID design
├── docs/                   # Technical documentation
├── test-scripts/           # Integration and hardware test scripts
└── archive/                # Archived/outdated content
```

## Hardware Specifications

### ESP32-S3 Motor Controller
- **MCU:** ESP32-S3-WROOM-1-N16
- **RAM:** 512 KB SRAM (NO PSRAM)
- **Flash:** 16 MB
- **Control Frequency:** 100 Hz (10 ms period)
- **Protocol:** SPI peripheral mode @ 10 Mbps

### Motor System
- **Motors:** 4× 6V brushed DC (210 RPM no-load, 1:34 gear ratio)
- **Encoders:** 341 PPR Hall sensors (1365 CPR quadrature)
- **Drivers:** DRV8243S with IPROPI current sensing (3.16 kΩ sense resistor)
- **Control:** Closed-loop PID (Kp=0.286, Ki=8.01, designed with MATLAB)

### Lidar Sensor
- **Model:** RPLiDAR C1 - DTOF LiDAR 360° Laser Range Scanner
- **Range:** 12 meters
- **Rating:** IP54 (dust and splash resistant)
- **Use Case:** SLAM (Simultaneous Localization and Mapping)

## Development Workflow

### ESP32 Firmware Development

**Build:**
```bash
cd star-esp32-firmware
pio run -e esp32s3
```

**Upload:**
```bash
pio run -e esp32s3 --target upload
```

**Monitor:**
```bash
pio device monitor
```

### Buildroot Development

**Configure:**
```bash
cd star-rpi5-buildroot
make raspberrypi5_defconfig
```

**Build:**
```bash
make
```

**Flash to SD card:**
```bash
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M status=progress
```

### Protocol Development

**Generate protobuf code:**
```bash
cd star-proto
protoc --nanopb_out=. messages.proto  # For ESP32
protoc --python_out=. messages.proto  # For RPi5
```

## Design Principles

### Embedded Firmware (ESP32)
- **Zero dynamic allocation:** No malloc/free (safety-critical)
- **Deterministic timing:** Fixed-priority tasks with FreeRTOS
- **Dependency injection:** DIP architecture for testability
- **Hardware abstraction:** Unified bus manager for I2C/SPI/UART/GPIO/ADC
- **Error handling:** Centralized error interface with retry logic

### Communication Protocol
- **Serialization:** nanopb (Protocol Buffers with zero malloc)
- **Framing:** SYNC marker + 8-byte header + payload + CRC-32
- **ARQ:** Stop-and-Wait (27× throughput margin)
- **Error Detection:** CRC-32 with ESP32 hardware acceleration (99.9999% reliability)

### Motor Control
- **Frequency:** 100 Hz (4.7× motor time constant)
- **Algorithm:** Discrete PID with anti-windup
- **Tuning:** MATLAB-based system identification and controller design
- **Safety:** Watchdog timeout 500ms, emergency stop <20ms

## Key Files

- `star-esp32-firmware/CLAUDE.md` - ESP32 firmware development guide
- `docs/Protobuf_Protocol_Design_Analysis.pdf` - Communication protocol specification
- `docs/Protocol_Implementation_Guide.pdf` - Protocol implementation guide
- `matlab/README.md` - Motor control design workflow
- `schematic/STAR_MCU.kicad_sch` - Main MCU schematic
- `schematic/STAR_MOTOR_DRIVER.kicad_sch` - Motor driver schematic

## Testing

### Hardware Testing
```bash
cd test-scripts
./test_motor_control.py      # Motor control validation
./test_spi_communication.py  # SPI protocol test
./test_lidar_integration.py  # RPLiDAR C1 integration
```

### Firmware Unit Tests
```bash
cd star-esp32-firmware
pio test -e esp32s3
```

## Common Tasks

### Motor PID Tuning
1. Measure motor step response to estimate time constant
2. Run MATLAB scripts: `motor_model_1st_order.m` → `pid_design_velocity.m`
3. Update ESP32 firmware with new gains
4. Test closed-loop performance

### Protocol Updates
1. Modify `.proto` schema in `star-proto/`
2. Regenerate nanopb code for ESP32
3. Regenerate Python code for RPi5
4. Update message handlers in firmware and gateway

### Hardware Debugging
- Use logic analyzer for SPI signals
- Check motor current with oscilloscope (IPROPI pin)
- Verify encoder signals (A/B quadrature)
- Monitor CRC-32 error rate in protocol logs

## Project Status

- ✅ ESP32-S3 firmware: Motor control, PID, encoders, bus abstraction
- ✅ MATLAB toolchain: Transfer function modeling, PID design
- ✅ Protocol design: nanopb + ARQ + CRC-32 specification
- 🚧 Protocol implementation: In progress (see Protocol_Implementation_Guide.pdf)
- 🚧 RPi5 buildroot: Custom Linux image development
- 🚧 SLAM integration: RPLiDAR C1 integration with Nav2

## References

- ESP32-S3 Technical Reference Manual (Espressif Systems)
- DRV8243 Datasheet (Texas Instruments)
- nanopb Documentation (https://jpa.kapsi.fi/nanopb/)
- RPLiDAR C1 User Manual (SLAMTEC)
- Protocol Buffers Language Guide (Google)
