# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA standards:

- **Controller/Peripheral** - NOT master/slave (I2C, SPI, 1-Wire)
- **COPI/CIPO** - NOT MOSI/MISO (Controller Out Peripheral In / Controller In Peripheral Out)
- **Primary/Main** - NOT master (for configuration structures)

Note: ESP-IDF and other external APIs may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Project Overview

**STAR (Sensor and Actuator Abstraction Runtime)** - A distributed robotics platform with custom PCB hardware, ESP32 motor control firmware, Raspberry Pi 5 control system, and Protocol Buffers communication.

### Architecture

| Component | Description |
|-----------|-------------|
| `star-esp32-firmware/` | ESP32-S3 motor controller (PlatformIO + ESP-IDF) |
| `star-proto/` | Protocol Buffers schemas with multi-language code generation |
| `star-rpi5-buildroot/` | Custom Buildroot Linux for Raspberry Pi 5 |
| `star-gateway/` | Gateway service (RPi5 to ESP32 communication) |
| `star-ui/` | User interface |
| `matlab/` | Motor system identification and PID controller design |
| `Schematic/` | KiCad PCB designs |

### Hardware

- **Main Controller:** Raspberry Pi 5
- **Motor Controller:** ESP32-S3-WROOM-1-N16 (16MB Flash, 512KB SRAM, NO PSRAM)
- **Motors:** 4x 6V brushed DC gearmotors (210 RPM, 341 PPR Hall encoders)
- **Motor Drivers:** DRV8243S H-bridge with current sensing
- **Lidar:** RPLiDAR C1 (12m range, IP54)
- **Communication:** 10 Mbps SPI (RPi5 ↔ ESP32) with nanopb + CRC-32

## Build Commands

### ESP32 Firmware (`star-esp32-firmware/`)

```bash
# Build
pio run -e esp32_wroom          # ESP32-WROOM (4MB)
pio run -e esp32s3              # ESP32-S3 (16MB)

# Upload and monitor
pio run -e esp32s3 --target upload
pio device monitor

# Generate call graphs (requires cflow, doxygen, graphviz)
./scripts/generate_callgraph.sh
```

### Protocol Buffers (`star-proto/`)

```bash
# Lint and format
buf lint proto/
buf format --diff proto/

# Generate code for all targets
buf generate proto/ --template buf.gen.yaml --include-imports

# Run tests
cd tests/kotlin && ./gradlew test
cd tests/typescript && npm test
cd tests/nanopb && mkdir -p build && cd build && cmake .. && make && ctest
```

### MATLAB (`matlab/`)

```bash
# Run in MATLAB
motor_model_1st_order   # Estimate transfer function
pid_design_velocity     # Design PID controller
pid_discretize          # Generate discrete coefficients for ESP32
```

## ESP32 Firmware Architecture

The firmware follows **Dependency Inversion Principle (DIP)** with these key patterns:

### Core Libraries

| Library | Purpose | Dependencies |
|---------|---------|--------------|
| `star_core` | Abstract interfaces only (error_interface, pin_interface) | None |
| `star_bus` | Unified bus abstraction (I2C, SPI, UART, GPIO, ADC, OneWire) | star_core |
| `star_motor` | MCPWM-based motor control | None |
| `star_encoder` | PCNT-based quadrature encoder | None |
| `star_pid` | Stateless PID algorithm | None |
| `star_drv8243` | DRV8243 driver integration | star_bus |

### DIP Pattern

```c
// 1. Initialize concrete implementation
error_handler_t error_handler;
error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

// 2. Get interface from implementation
star_error_interface_t error_iface;
error_handler_get_interface(&error_iface, &error_handler);

// 3. Inject interface into dependent component
star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
```

### Thread Safety

Use `star_bus_manager_with_bus()` instead of `star_bus_manager_find_bus()` to avoid race conditions. Default mutex timeout: 1000ms.

## Protocol Buffers

### Style Guide (Boston Dynamics-based)

- **Proto3 only**, 100 char line limit, 4-space indent
- **Naming:** Messages `PascalCase`, fields `snake_case`, enums `SCREAMING_SNAKE`
- **Enum zero value:** Must end with `_UNKNOWN`
- **Units:** MKS system with suffixes (`_mps`, `_rad`, `_ma`, `_celsius`)
- **Headers:** Include `RequestHeader`/`ResponseHeader` in all RPC messages

### Code Generation Targets

| Target | Plugin | Output |
|--------|--------|--------|
| Kotlin/Java | buf.build/protocolbuffers/java, grpc/java, grpc/kotlin | `gen/kotlin/` |
| TypeScript | timostamm-protobuf-ts | `gen/typescript/` |
| C (ESP32) | nanopb_generator | `gen/nanopb/` |

### nanopb Considerations

Configure field sizes in `.options` files for ESP32 (no dynamic allocation):
```
star.v1.RequestHeader.request_id max_size:64
```

## Code Style

### Naming Conventions

- Functions/variables: `snake_case`
- Macros/constants: `SCREAMING_SNAKE_CASE`
- Types: `snake_case_t`
- Static functions: `internal_` prefix
- Private functions: `priv_` prefix
- Static variables: `s_` prefix
- Global variables: `g_` prefix (avoid)

### Critical Rules

- Always use braces for control statements
- Use `assert()` for programming errors only, not runtime errors
- Avoid inline ASM; if required, use `volatile` and document why
- Zero dynamic allocation in ESP32 firmware (safety-critical)

## Motor Control

### PID Tuning Workflow

1. Measure motor step response → estimate time constant (τ = 75ms)
2. Run MATLAB: `motor_model_1st_order.m` → `pid_design_velocity.m` → `pid_discretize.m`
3. Update ESP32 firmware with new gains (Kp=0.286, Ki=8.01)
4. Test closed-loop at 100 Hz control rate

### Motor Model

G(s) = 3.665 / (0.075s + 1)

## CI/CD

The `proto.yml` workflow runs on pushes to `star-proto/`:
1. **Lint:** `buf format`, `buf lint`, `buf build`
2. **Breaking:** `buf breaking` against main (PRs only)
3. **Generate:** Kotlin, TypeScript, nanopb code
4. **Test:** Serialization tests for all three targets

## Git Commits and Pull Requests

**Do not add AI attribution to commits or PRs.** Write natural commit messages and PR descriptions without any "Generated by Claude Code", "Co-Authored-By: Claude", or similar footers. Keep messages clean and professional as if written by a human developer.

## Key Documentation

- `star-esp32-firmware/CLAUDE.md` - Detailed ESP32 firmware guide
- `star-proto/STYLE_GUIDE.md` - Protobuf conventions
- `star-proto/PROTO_GUIDE.pdf` - Protocol implementation guide
- `docs/star_documentation.pdf` - System documentation
