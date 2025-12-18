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
| `star-gateway/` | Go gateway service (UI ↔ ROS2 bridge) running on RPi5 |
| `star-ui/` | User interface (TypeScript) |
| `matlab/` | Motor system identification and PID controller design |
| `Schematic/` | KiCad PCB designs |

### System Communication Flow

```
User → UI (TypeScript)
     → Gateway (Go on RPi5)
     → ROS2 (C++ on RPi5)
     → [SPI Bridge - TBD: ROS2 node or custom C using DIP libs]
     → ESP32 (C firmware with nanopb)
```

**Key Design Notes:**
- **Gateway (Go):** Handles WebSocket/HTTP with UI, bridges to ROS2, runs on RPi5
- **ROS2 (C++):** Robot control framework, runs on RPi5
- **SPI Bridge:** Not yet implemented - options include:
  - ROS2 node with SPI support
  - Custom C implementation using existing DIP libraries from `star-esp32-firmware/lib/star_bus/`
  - Go implementation (less likely due to existing C libraries)
- **ESP32:** Real-time motor control at 250Hz, communicates via Protocol Buffers over SPI

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

# Run Go tests
cd tests/go && go test ./...
```

### Gateway Service (`star-gateway/`)

```bash
# Build
cd star-gateway
go build ./cmd/star-gateway

# Test
go test ./...

# Run (on RPi5)
./star-gateway
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
| Go | buf.build/protocolbuffers/go, buf.build/grpc/go | `gen/go/` |
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

### Constants and Macros

Prefer enums over const variables over macros for defining constant values.

1. **Enums** - Use for related integer constants
   ```c
   // PREFER: Type-safe enums with debugger support
   typedef enum {
     k_motor_state_idle    = 0,
     k_motor_state_running = 1,
     k_motor_state_error   = 2,
   } motor_state_t;

   // AVOID: Macros for related constants
   #define MOTOR_STATE_IDLE    (0)
   #define MOTOR_STATE_RUNNING (1)
   #define MOTOR_STATE_ERROR   (2)
   ```

2. **const variables** - Use for single typed values
   ```c
   // PREFER: Type-safe const with scope
   static const float    s_max_velocity_mps = 2.5f;
   static const uint32_t s_timeout_ms       = 1000;

   // AVOID: Macros for simple constants
   #define MAX_VELOCITY_MPS (2.5f)
   #define TIMEOUT_MS       (1000)
   ```

3. **Macros** - Only when compile-time evaluation or token pasting is required
   ```c
   // ACCEPTABLE: Macro required for token pasting
   #define CONCAT(a, b) a##b

   // ACCEPTABLE: Macro required for compile-time size
   #define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
   ```

**Why this matters:**
- Enums provide type safety and debugger support
- const variables have type information and scope
- Macros lack type safety and can cause subtle bugs

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
3. **Generate:** Go, TypeScript, nanopb code
4. **Test:** Serialization tests for all three targets

## Git Commits and Pull Requests

**Do not add AI attribution to commits or PRs.** Write natural commit messages and PR descriptions without any "Generated by Claude Code", "Co-Authored-By: Claude", or similar footers. Keep messages clean and professional as if written by a human developer.

## Key Documentation

- `star-esp32-firmware/CLAUDE.md` - Detailed ESP32 firmware guide
- `star-gateway/CLAUDE.md` - Gateway service architecture and build guide
- `star-proto/STYLE_GUIDE.md` - Protobuf conventions
- `docs/star_documentation.pdf` - System documentation
