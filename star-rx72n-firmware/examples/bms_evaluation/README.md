# BMS Evaluation Tool - RX72N Firmware Example

Comprehensive BQ78350-R1A battery management system evaluation firmware for use with desktop BMS tool (Wails application).

## Overview

This example firmware turns the RX72N into a **USB-to-SMBus bridge** for the BQ78350 fuel gauge, enabling:

- Low-level SMBus register access (read/write any register)
- High-level battery monitoring (telemetry, cell voltages, status)
- Manufacturer access commands
- Block read/write for configuration
- Real-time data streaming

This firmware is designed to be used with the **star-bms-tool** desktop application (Go + Wails), which provides a graphical interface for battery evaluation, configuration, and monitoring.

## Architecture

```
Desktop Tool (Go + Wails)
    ↓ USB CDC (Virtual COM Port)
[Protocol Buffers over Framed Protocol]
    ↓
RX72N Firmware
    ├── USB Task (rx_usb_comm)
    ├── BMS Task (command handler)
    └── SMBus Driver (rx_bq78350)
        ↓
BQ78350-R1A Fuel Gauge
    ↓
Battery Pack (4S-16S Li-ion)
```

### Communication Protocol

**Transport Layer:** USB CDC with frame-based protocol (`rx_usb_comm`)
- Frame format: `[SYNC(0x55AA) | SEQ | LEN | TYPE | FLAGS | PAYLOAD | CRC-32]`
- Automatic retransmission, sequence numbering, error detection

**Application Layer:** Protocol Buffers (nanopb)
- Request: `BmsCommandRequest` (defined in `star-proto/proto/star/v1/bms.proto`)
- Response: `BmsCommandResponse`
- Compact binary serialization optimized for embedded systems

## Hardware Requirements

### RX72N MCU
- Renesas RX72N microcontroller (R5F572NNHGFP#30)
- USB connection to PC (for CDC Virtual COM Port)
- Debug UART optional (for logging)

### BQ78350-R1A Fuel Gauge
- TI BQ78350-R1A battery management IC (automotive-grade, 4S-16S)
- Connected via SMBus (I2C compatible)
- Battery pack (4S to 16S Li-ion/LiPo)

### Connections

| Signal      | RX72N Pin      | BQ78350 Pin | Notes                          |
|-------------|----------------|-------------|--------------------------------|
| SMBC0 (SCL) | P12 (pin 34)   | SCL         | SMBus clock (RIIC0)            |
| SMBD0 (SDA) | P13 (pin 33)   | SDA         | SMBus data (RIIC0)             |
| GND         | GND            | GND         | Common ground                  |

**SMBus Configuration:**
- Bus speed: 100 kHz (SMBus standard mode)
- Device address: 0x0B (7-bit, default for BQ78350)
- PEC enabled: Yes (CRC-8 error checking)
- Uses dedicated SMBus pins (SMBC0/SMBD0) with Fast Mode Plus support

## Software Requirements

- RX72N firmware build environment (Docker + GNURX toolchain)
- PC with USB CDC driver (usually automatic on Windows/Linux/macOS)
- **star-bms-tool** desktop application (see `star-bms-tool/` directory)

## Building

```bash
# From star-rx72n-firmware directory
./build.sh
```

The example can be built standalone or integrated into main firmware.

### Standalone Build

To build this example as the main application:

```bash
# Option 1: Symlink (for development)
cd star-rx72n-firmware/src
mv main.c main.c.bak
ln -s ../examples/bms_evaluation/main.c main.c

# Option 2: Modify CMakeLists.txt
# Edit star-rx72n-firmware/CMakeLists.txt:
set(SOURCES
    examples/bms_evaluation/main.c
    # ... other sources
)
```

## Flashing

```bash
# Flash to RX72N via E2 Lite or J-Link
./flash.sh
```

## Usage with Desktop Tool

### 1. Flash Firmware
```bash
cd star-rx72n-firmware
./build.sh
./flash.sh
```

### 2. Connect Hardware
- Connect RX72N to PC via USB (will enumerate as Virtual COM Port)
- Connect BQ78350-R1A to RX72N SMBus pins (P12/SMBC0, P13/SMBD0)
- Connect battery pack to BQ78350

### 3. Launch Desktop Tool
```bash
cd star-bms-tool
./star-bms-tool  # Or double-click the application icon
```

### 4. Connect to Device
- Desktop tool will auto-detect USB serial port
- Select port and click "Connect"
- Tool will display live telemetry and allow register access

## Supported Commands

The firmware implements all commands from `bms.proto`:

### Low-Level SMBus Access (EV2300 Replacement)
- **ReadRegisterCommand**: Read 1-2 bytes from any register
- **WriteRegisterCommand**: Write 1-2 bytes to any register
- **ReadBlockCommand**: Read block data (strings, data flash)
- **WriteBlockCommand**: Write block data (configuration)
- **ManufacturerAccessCommand**: Execute manufacturer access commands

### High-Level Convenience Commands
- **ReadTelemetryCommand**: Get voltage, current, SOC, temperature, capacity, cycles
- **ReadCellVoltagesCommand**: Get individual cell voltages (up to 16 cells)
- **ReadDeviceInfoCommand**: Get manufacturer, model, serial, versions
- **ReadProtectionStatusCommand**: Get overvoltage, undervoltage, overcurrent, temperature flags
- **ResetDeviceCommand**: Send soft reset to BMS

## Code Structure

```
examples/bms_evaluation/
├── README.md           # This file
├── main.c              # Application entry point
└── CMakeLists.txt      # Build configuration (optional)
```

**Tasks:**
- **USB Task (Priority 5):** Receives BMS commands, sends responses
- **BMS Task (Priority 5):** Executes BMS commands, interfaces with BQ78350

## Protocol Buffer Integration

### Request Message (Desktop → Firmware)
```protobuf
message BmsCommandRequest {
  RequestHeader header = 1;
  oneof command {
    ReadRegisterCommand read_register = 2;
    WriteRegisterCommand write_register = 3;
    // ... other commands
  }
}
```

### Response Message (Firmware → Desktop)
```protobuf
message BmsCommandResponse {
  ResponseHeader header = 1;
  oneof response {
    BmsRegisterData register_data = 2;
    BmsTelemetryData telemetry_data = 5;
    // ... other responses
  }
}
```

### nanopb Code Generation

Protocol Buffer code is generated by `buf` from `star-proto/` repository:

```bash
cd star-proto
buf generate proto/ --template buf.gen.yaml
# Generates gen/nanopb/ with .c/.h files
```

Generated files are copied to firmware during build.

## Configuration

### Adjust Number of Cells

Edit `main.c`:
```c
typedef enum {
  k_num_cells = 16,  // Change to match your battery pack (4-16)
} app_constants_t;
```

### Change SMBus Address

If your BQ78350 is configured for a different address, edit `main.c`:
```c
rx_bus_config_t bms_bus_config = {
  .proto = {
    .smbus = {
      .i2c_config = {
        .device_addr = 0x0B,  // Change to your device address
      },
    },
  },
};
```

## Troubleshooting

### No USB Device Detected
- Check USB cable connection
- Verify USB driver installation (should be automatic)
- Check Device Manager (Windows) or `ls /dev/tty*` (macOS/Linux)
- Try different USB port or cable

### Error Reading BMS Data
- Verify SMBus connections (SCL, SDA, GND)
- Check SMBus pull-up resistors (typically 4.7kΩ to 10kΩ)
- Confirm BQ78350 power and battery connection
- Try different SMBus address if non-default
- Use oscilloscope to check signal integrity

### USB Communication Errors
- Ensure firmware and desktop tool are using same protocol version
- Check USB cable quality (some cables are power-only)
- Try lower SMBus speed if seeing CRC errors:
  ```c
  .frequency_hz = 50000,  // Reduce to 50 kHz
  ```

### Desktop Tool Can't Connect
- Verify correct COM port selected
- Check that no other application is using the port
- Restart desktop tool
- Reflash firmware
- Check USB enumeration in Device Manager/System Info

## API Reference

See full documentation:
- `lib/rx_bms/inc/rx_bq78350.h` - BQ78350 driver API
- `lib/rx_bus/inc/rx_bus_smbus.h` - SMBus protocol API
- `lib/rx_usb/inc/rx_usb_comm.h` - USB communication API
- `star-proto/proto/star/v1/bms.proto` - Protocol Buffer definitions

## Related Projects

- **star-bms-tool**: Desktop GUI application (Go + Wails + Svelte)
- **star-proto**: Protocol Buffer definitions for STAR system
- **star-rx72n-firmware**: Main RX72N motor control firmware

## License

Copyright (c) 2026 STAR Project
