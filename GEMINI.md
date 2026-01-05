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

## Backward Compatibility Policy

**IMPORTANT:** This project has not released any versions yet. There is **NO backward compatibility requirement**.

- Do NOT add function aliases or deprecation macros for renamed functions
- Do NOT keep old API signatures "for compatibility"
- When refactoring, update all call sites directly - no shims or compatibility layers
- Clean code now is better than technical debt for non-existent users

**Example - What NOT to do:**
```c
// WRONG - No backward compatibility needed!
#define old_function_name new_function_name  /* ❌ Delete old code instead */
```

**Example - What to do:**
```c
// CORRECT - Just update the function name and all call sites
rx_err_t new_function_name(...)  /* ✓ Clean break, no compatibility layer */
```

## Constants and Macros Policy

**Strict hierarchy for constants:**

1. **Enums** - ALWAYS use for ALL integer constants (mandatory)
2. **static const** - ONLY for floating-point values (enum can't hold floats)
3. **Macros** - ONLY for these 3 specific cases:
   - Reducing duplicated code (function-like macros: `RX_RETURN_ON_ERROR`)
   - Conditional compilation (optimization: `#if LOG_LEVEL >= k_log_error`)
   - Build configuration flags (`#ifdef __RX__`)

**Never use macros for:**
- Simple integer constants (use enum instead)
- Simple floating-point constants (use static const instead)
- Hardware register addresses (use inline accessor functions instead)
- Backward compatibility/function aliases (no releases = no compatibility)

**Hardware register access:**
Use inline accessor functions with enum addresses (never macros):
```c
// ✓ CORRECT - Inline accessor with enum address
typedef enum {
    k_cmt0_base_addr  = 0x00088000,
    k_port0_base_addr = 0x000C0000,
} hw_addresses_t;

static inline CMT_Type* cmt0(void) {
    return (CMT_Type*)k_cmt0_base_addr;
}

// Usage: Same syntax as macro approach
cmt0()->CMCR = 0x0042;
```

**Examples:**
```c
// ✓ CORRECT - Integer constants use enum
typedef enum {
    k_timeout_ms  = 1000,
    k_max_retries = 3
} limits_t;

// ✓ CORRECT - Floats use const (enum limitation)
static const float s_max_velocity_mps = 2.5f;

// ✓ CORRECT - Function-like macro reduces duplication
#define RX_RETURN_ON_ERROR(err, tag, msg) \
    do { \
        rx_err_t _err = (err); \
        if (_err != k_rx_ok) { \
            rx_log_error((tag), (msg)); \
            return _err; \
        } \
    } while (0)

// ❌ WRONG - Never use macro for simple constants
#define TIMEOUT_MS 1000        // Should be enum!
#define MAX_VELOCITY 2.5f      // Should be static const!
#define CMT0_BASE ((CMT_Type*)0x00088000)  // Should be inline accessor!
#define old_func new_func      // No compatibility needed!
```

## No Magic Numbers Policy

**ZERO TOLERANCE for magic numbers.** ALL numeric literals must be named enums, including:
- Array indices (`buf[0]` → `buf[k_idx_high_byte]`)
- Bit shift amounts (`val >> 8` → `val >> k_shift_byte`)
- Protocol offsets (`frame[4]` → `frame[k_offset_payload]`)
- Register bit positions (`1 << 7` → `1 << k_bit_enable`)
- Bit masks (`0xFF` → `k_mask_byte`)

**Examples:**
```c
// ✓ CORRECT: Named indices
typedef enum {
    k_idx_high_byte = 0,
    k_idx_low_byte  = 1
} be16_byte_idx_t;

buf[k_idx_high_byte] = (val >> k_shift_byte);

// ✓ CORRECT: Named shifts
typedef enum {
    k_shift_byte   = 8,
    k_shift_enable = 7
} bit_shifts_t;

// ✓ CORRECT: Named offsets
typedef enum {
    k_offset_sync    = 0,
    k_offset_payload = 4
} frame_offsets_t;

frame[k_offset_sync] = SYNC_MARKER;

// ✓ CORRECT: Named masks
typedef enum {
    k_mask_byte   = 0xFF,
    k_mask_enable = 0x80
} bit_masks_t;

val &= k_mask_byte;

// ❌ WRONG: Magic numbers
buf[0] = (val >> 8);         // What is 0? What is 8?
frame[4] = payload;          // What's at index 4?
REG = (1 << 7) | (3 << 3);  // Which bits? Why?
val &= 0xFF;                 // What does 0xFF represent?
```

**Benefits:**
- Self-documenting: `k_idx_high_byte` vs `0`
- Searchable: grep "high_byte" finds all uses
- Maintainable: change offset in one place
- Debugger-friendly: see names, not numbers
- Type-safe: compiler catches typos

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

## NASA Power of 10 Rules (STAR Implementation)

The STAR project follows NASA/JPL Power of 10 rules for safety-critical embedded code with one intentional deviation for testability.

### Rule 1: Simplify Control Flow ✓ COMPLIANT
- No `goto`, `setjmp`/`longjmp`, or recursion
- All control flow uses `if`/`while`/`for` only
- Example: `rx_pid_init()` uses sequential error checking, no goto cleanup

### Rule 2: Fixed Loop Upper-Bounds ✓ COMPLIANT
- All loops have statically provable bounds
- Exception: Main control loops use `while(1)` with watchdog
- Example: `for (uint8_t i = 0; i < k_max_retries; i++)` - enum provides bound

### Rule 3: No Dynamic Memory After Initialization ✓ COMPLIANT
- **Zero malloc/free in RX72N firmware** (safety-critical)
- All buffers statically allocated with enum-defined sizes
- ThreadX stacks are static arrays
- Example: `char items[k_max_items][k_max_desc_len]` - compile-time allocation

### Rule 4: Keep Functions Short (~60 lines) ✓ COMPLIANT
- Functions represent single verifiable units
- Example: `rx_pid_compute()` is 44 lines - complete PID algorithm in one screen

### Rule 5: Use Assertions/Validation ✓ COMPLIANT
- Minimum 2 validation checks per function
- **Pre-conditions**: `RX_CHECK_NULL_PTR`, state validation
- **Post-conditions**: Output bounds checking, invariant validation
- Example: `rx_pid_compute()` has 4 checks (NULL×2, initialized, dt > 0)

### Rule 6: Declare Data at Smallest Scope ✓ COMPLIANT
- Variables declared close to first use
- Loop counters in for-statement: `for (uint8_t i = 0; ...)`
- File-scope variables use `static` prefix (`s_tag`)

### Rule 7: Check All Return Values ✓ COMPLIANT
- All function returns validated or explicitly cast to `(void)`
- Use `RX_RETURN_ON_ERROR` macro for propagation
- Example: `rx_err_t ret = bus_init(config); if (ret != k_rx_ok) return ret;`

### Rule 8: Limit Preprocessor Use ✓ COMPLIANT
- Enums for ALL integer constants (mandatory)
- Macros ONLY for: duplicated code, conditional compilation, build flags
- Hardware register access: Use inline accessor functions (never macros)
- See "Constants and Macros Policy" section above for complete policy

### Rule 9: Restrict Pointer Use ⚠️ INTENTIONAL DEVIATION
- **Standard**: Maximum one level of dereferencing, no function pointers
- **STAR Deviation**: Function pointers ALLOWED for Dependency Inversion Principle (DIP)
- **Why**: Enables mock implementations for unit testing and hardware abstraction
- Example: `typedef struct { rx_err_t (*read)(void* ctx, ...); void* ctx; } bus_interface_t;`

### Rule 10: Compile with Maximum Warnings ✓ COMPLIANT
- CMake flags: `-Wall -Wextra -Werror`
- Build fails on ANY warning
- CI/CD enforces zero-warning builds

## SOLID Principles for C (STAR Implementation)

### Single Responsibility (S)
- **One module = one purpose**: `rx_pid` handles ONLY PID algorithm (no motor control, no hardware)
- **One function = one action**: `rx_pid_compute()` does PID math, `rx_pid_reset()` clears state
- **Separation of concerns**: Configuration (`rx_pid_config_t`) separate from runtime state (`rx_pid_handle_t`)

### Open/Closed (O)
- **Extensible without modification**: `rx_pid` configured via `rx_pid_config_t` struct
- **Runtime tuning**: `rx_pid_set_gains()` allows updates without recompilation
- **Avoid hardcoded values**: All limits defined in config (output_min/max, integral_min/max)

### Liskov Substitution (L)
- **Interface implementations interchangeable**: Bus manager accepts any bus type (I2C/SPI/1-Wire)
- **Mocks substitute real implementations**: Tests use `mock_rx_bus_onewire` in place of real hardware
- **Consistent error handling**: All drivers return `rx_err_t` with same semantics

### Interface Segregation (I)
- **Small, focused interfaces**: `rx_pid` API has 7 functions, each with clear purpose
- **No "fat" interfaces**: Bus interface split into `read()`, `write()`, `configure()` - use only what you need
- **Separate read/write**: Motor encoder read separate from motor driver write operations

### Dependency Inversion (D)
- **High-level modules don't depend on low-level details**: Motor control uses bus interface, not direct GPIO
- **Function pointer interfaces for abstraction**:
  ```c
  typedef struct {
      rx_err_t (*read)(void* ctx, uint8_t* data, uint32_t len);
      rx_err_t (*write)(void* ctx, const uint8_t* data, uint32_t len);
      void* ctx;
  } bus_interface_t;
  ```
- **Testable via mock injection**: `driver_init(driver, &mock_bus)` vs `driver_init(driver, &real_bus)`

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