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

Note: Renesas RX and other external APIs may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Project Overview

**STAR (Simultaneous Tracking and Robotics)** - A distributed robotics platform with custom PCB hardware, embedded firmware, and high-level control software.

### Architecture

The STAR platform consists of multiple integrated components:

#### Hardware
- **Main Controller:** Raspberry Pi 5 (quad-core ARM Cortex-A76)
- **Real-Time Controller:** Renesas RX72N (RXv3 core @ 240 MHz, 4 MB Flash, 1 MB SRAM)
- **Motors:** 4× 6V brushed DC gearmotors (210 RPM, 1:34 gear ratio, 341 PPR Hall encoders)
- **Motor Drivers:** 4× DRV8243S H-bridge with current sensing
- **Lidar:** RPLiDAR C1 - DTOF LiDAR 360° Laser Range Scanner (12m range, IP54)
- **Battery Management:** BQ7850 (custom BMS PCB)
- **Communication:** 10 Mbps SPI (RPi5 ↔ RX72N), custom PCB with <15cm traces

#### Firmware (`star-rx72n-firmware/`)
- **Platform:** CMake with Renesas RX Toolchain
- **Architecture:** ThreadX-based modular framework with Dependency Inversion Principle (DIP)
- **Control:** 250 Hz closed-loop PID velocity control
- **Protocol:** nanopb (Protocol Buffers) with Stop-and-Wait ARQ and CRC-32
- **Key Libraries:** rx_motor, rx_drv8243, rx_encoder, rx_pid, rx_bus

#### Operating System (`star-rpi5-buildroot/`)
- **Platform:** Custom Buildroot Linux for Raspberry Pi 5
- **Purpose:** Lightweight embedded Linux optimized for SLAM and motor control coordination

#### Protocol (`star-proto/`)
- **Format:** Protocol Buffers (nanopb for RX72N, standard protobuf for RPi5)
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
├── star-rx72n-firmware/    # RX72N motor control firmware
├── star-rpi5-buildroot/    # Custom Linux image for Raspberry Pi 5
├── star-proto/             # Protocol Buffers schemas (SPI communication)
├── star-gateway/           # Gateway service (Go)
├── star-ui/                # User interface (TypeScript)
├── matlab/                 # Motor modeling and PID design
├── docs/                   # Technical documentation
├── test-scripts/           # Integration and hardware test scripts
└── archive/                # Archived/outdated content
```

## Hardware Specifications

### RX72N Motor Controller
- **MCU:** Renesas RX72N (R5F572NNHGFP#30)
- **RAM:** 1 MB SRAM
- **Flash:** 4 MB
- **Control Frequency:** 250 Hz (4 ms period)
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

### RX72N Firmware Development

**Build:**
```bash
cd star-rx72n-firmware
./build.sh
```

**Flash:**
```bash
./flash.sh
```

**Monitor:**
```bash
# Use serial monitor / terminal
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
buf generate
```

## Design Principles

### Embedded Firmware (RX72N)
- **Zero dynamic allocation:** No malloc/free (safety-critical)
- **Deterministic timing:** Fixed-priority tasks with ThreadX RTOS
- **Dependency injection:** DIP architecture for testability
- **Hardware abstraction:** Unified bus manager for I2C/SPI/UART/GPIO/ADC
- **Error handling:** Centralized error interface with retry logic

### Communication Protocol
- **Serialization:** nanopb (Protocol Buffers with zero malloc)
- **Framing:** SYNC marker + 8-byte header + payload + CRC-32
- **ARQ:** Stop-and-Wait (27× throughput margin)
- **Error Detection:** CRC-32 with RX hardware acceleration (99.9999% reliability)

### Motor Control
- **Frequency:** 250 Hz
- **Algorithm:** Discrete PID with anti-windup
- **Tuning:** MATLAB-based system identification and controller design
- **Safety:** Watchdog timeout 500ms, emergency stop <20ms

## Key Files

- `star-rx72n-firmware/CLAUDE.md` - RX72N firmware development guide
- `docs/star_documentation.pdf` - Complete system documentation
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
cd star-rx72n-firmware
# Run CMake tests
```

## Common Tasks

### Motor PID Tuning
1. Measure motor step response to estimate time constant
2. Run MATLAB scripts: `motor_model_1st_order.m` → `pid_design_velocity.m`
3. Update RX72N firmware with new gains
4. Test closed-loop performance

### Protocol Updates
1. Modify `.proto` schema in `star-proto/`
2. Regenerate code using buf
3. Update message handlers in firmware and gateway

### Hardware Debugging
- Use logic analyzer for SPI signals
- Check motor current with oscilloscope (IPROPI pin)
- Verify encoder signals (A/B quadrature)
- Monitor CRC-32 error rate in protocol logs

## Project Status

- ✅ RX72N firmware: Motor control, PID, encoders, bus abstraction
- ✅ MATLAB toolchain: Transfer function modeling, PID design
- ✅ Protocol design: nanopb + ARQ + CRC-32 specification
- ✅ ARQ Implementation in Go (star-gateway)
- 🚧 Protocol implementation: In progress (see star-gateway implementation)
- 🚧 RPi5 buildroot: Custom Linux image development
- 🚧 SLAM integration: RPLiDAR C1 integration with Nav2

## References

- RX72N Technical Reference Manual (Renesas)
- DRV8243 Datasheet (Texas Instruments)
- nanopb Documentation (https://jpa.kapsi.fi/nanopb/)
- RPLiDAR C1 User Manual (SLAMTEC)
- Protocol Buffers Language Guide (Google)