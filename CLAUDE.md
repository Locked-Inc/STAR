# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA standards:

- **Controller/Peripheral** - NOT master/slave (I2C, SPI, 1-Wire)
- **COPI/CIPO** - NOT MOSI/MISO (Controller Out Peripheral In / Controller In Peripheral Out)
- **Primary/Main** - NOT master (for configuration structures)

Note: External APIs may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Backward Compatibility Policy

**IMPORTANT:** This project has not released any versions yet. There is **NO backward compatibility requirement**.

- Do NOT add function aliases or deprecation macros for renamed functions
- Do NOT keep old API signatures "for compatibility"
- When refactoring, update all call sites directly - no shims
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

## Project Overview

**STAR (Simultaneous Tracking and Robotics)** - A distributed robotics platform with custom PCB hardware, Renesas RX72N motor control firmware, Raspberry Pi 5 control system, and Protocol Buffers communication.

### Architecture

| Component | Description |
|-----------|-------------|
| `star-rx72n-firmware/` | Renesas RX72N motor controller (CMake + GNURX + ThreadX) |
| `star-proto/` | Protocol Buffers schemas with multi-language code generation |
| `star-rpi5-buildroot/` | Custom Buildroot Linux for Raspberry Pi 5 |
| `star-gateway/` | Go gateway service (UI ↔ ROS2 bridge) running on RPi5 |
| `star-ui/` | User interface (TypeScript) |
| `matlab/` | Motor system identification and PID controller design |
| `schematic/` | KiCad PCB designs |

### System Communication Flow

```
User → UI (TypeScript)
     → Gateway (Go on RPi5)
     → ROS2 (C++ on RPi5)
     → [SPI Bridge - TBD: ROS2 node or custom C]
     → RX72N (C firmware with ThreadX + nanopb)
```

**Key Design Notes:**
- **Gateway (Go):** Handles WebSocket/HTTP with UI, bridges to ROS2, runs on RPi5
- **ROS2 (C++):** Robot control framework, runs on RPi5
- **SPI Bridge:** Not yet implemented - ROS2 node with SPI support
- **RX72N:** Real-time motor control, communicates via Protocol Buffers over SPI

### Hardware

- **Main Controller:** Raspberry Pi 5
- **Motor Controller:** Renesas RX72N (4MB Flash, 512KB SRAM)
- **Motors:** 4x 6V brushed DC gearmotors (210 RPM, 341 PPR Hall encoders)
- **Motor Drivers:** DRV8243S H-bridge with current sensing
- **Lidar:** RPLiDAR C1 (12m range, IP54)
- **Communication:** 10 Mbps SPI (RPi5 ↔ RX72N) with nanopb + CRC-32

## Build Commands

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
pid_discretize          # Generate discrete coefficients for RX72N
```

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
| C (RX72N) | nanopb_generator | `gen/nanopb/` |

### nanopb Considerations

Configure field sizes in `.options` files for RX72N (no dynamic allocation):
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

**Strict preference hierarchy:**

1. **Enums** - ALWAYS use for ALL integer constants
   ```c
   // CORRECT: Type-safe enums with debugger support
   typedef enum {
     k_motor_state_idle    = 0,
     k_motor_state_running = 1,
     k_motor_state_error   = 2,
     k_timeout_ms          = 1000,    // Integer constant → enum
     k_max_retries         = 3        // Integer constant → enum
   } motor_config_t;

   // WRONG: Never use macros for integer constants
   #define TIMEOUT_MS (1000)  // ❌ Should be enum!
   ```

2. **const variables** - ONLY for floating-point (enum limitation)
   ```c
   // CORRECT: Floating-point must use const (can't use enum)
   static const float s_max_velocity_mps = 2.5f;
   static const float s_pid_kp = 1.0f;

   // WRONG: Never use macros for floats
   #define MAX_VELOCITY_MPS (2.5f)  // ❌ Should be const!
   ```

3. **Macros** - ONLY for these 3 specific cases:
   ```c
   // ✓ ALLOWED: Reducing duplicated code
   #define RX_RETURN_ON_ERROR(err, tag, msg) \
       do { \
           rx_err_t _err = (err); \
           if (_err != k_rx_ok) { \
               rx_log_error((tag), (msg)); \
               return _err; \
           } \
       } while (0)

   // ✓ ALLOWED: Conditional compilation (optimization)
   #if LOG_LEVEL >= k_log_error
   #define rx_log_error(tag, msg) internal_rx_log_error((tag), (msg))
   #else
   #define rx_log_error(tag, msg) ((void)0)
   #endif

   // ✓ ALLOWED: Build configuration flags
   #ifdef __RX__
   #define RX_CRC32_USE_HARDWARE
   #endif

   // ❌ FORBIDDEN: Hardware register addresses (use inline accessors)
   #define CMT0_BASE ((rx_cmt_channel_regs_t*)0x00088000)  // Wrong!
   #define CMT0      (*CMT0_BASE)                          // Wrong!

   // ❌ FORBIDDEN: Backward compatibility (no releases = no compatibility)
   #define old_function new_function  // Wrong! Update call sites instead
   ```

4. **Hardware Register Access** - Use inline accessor functions:
   ```c
   // ✓ CORRECT: Inline accessor with enum address
   typedef enum {
       k_cmt0_base_addr  = 0x00088000,
       k_port0_base_addr = 0x000C0000,
   } hw_addresses_t;

   static inline CMT_Type* cmt0(void) {
       return (CMT_Type*)k_cmt0_base_addr;
   }

   static inline PORT_Type* port0(void) {
       return (PORT_Type*)k_port0_base_addr;
   }

   // Usage: Same syntax as macro approach
   cmt0()->CMCR = 0x0042;
   port0()->PDR |= (1 << k_bit_led);
   ```

### No Magic Numbers

**ZERO TOLERANCE for magic numbers.** ALL numeric literals must be named enums, including:

```c
// ✓ CORRECT: Array indices as enums
typedef enum {
    k_idx_high_byte = 0,
    k_idx_low_byte  = 1
} be16_byte_idx_t;

buf[k_idx_high_byte] = (val >> k_shift_byte);

// ✓ CORRECT: Bit shifts as enums
typedef enum {
    k_shift_byte   = 8,
    k_shift_enable = 7
} bit_shifts_t;

// ✓ CORRECT: Protocol offsets as enums
typedef enum {
    k_offset_sync    = 0,
    k_offset_payload = 4
} frame_offsets_t;

// ✓ CORRECT: Bit masks as enums
typedef enum {
    k_mask_byte   = 0xFF,
    k_mask_enable = 0x80
} bit_masks_t;

// ❌ WRONG: Magic numbers
buf[0] = (val >> 8);              // What is 0? What is 8?
frame[4] = payload;               // What's at index 4?
REG = (1 << 7) | (3 << 3);       // Which bits? Why?
```

**Why this matters:**
- Self-documenting code (k_idx_high_byte vs 0)
- Searchable (grep for "high_byte" finds all uses)
- Maintainable (change offset in one place)
- Debugger-friendly (see names, not numbers)
- Compile-time checked (typos caught)
- Enums provide type safety
- Macros lack type safety and can cause subtle bugs

### Critical Rules

- Always use braces for control statements
- Use `assert()` for programming errors only, not runtime errors
- Avoid inline ASM; if required, use `volatile` and document why
- Zero dynamic allocation in RX72N firmware (safety-critical)

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
- See "Constants and Macros" section above for complete policy

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

## Motor Control

### PID Tuning Workflow

1. Measure motor step response → estimate time constant (τ = 75ms)
2. Run MATLAB: `motor_model_1st_order.m` → `pid_design_velocity.m` → `pid_discretize.m`
3. Update RX72N firmware with new gains (Kp=0.286, Ki=8.01)
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

**IMPORTANT:** Always reference the LaTeX source files (`.tex`) in `docs/sections/` for accurate technical information, NOT the compiled PDF.

- `star-rx72n-firmware/CLAUDE.md` - Detailed RX72N firmware guide
- `star-gateway/CLAUDE.md` - Gateway service architecture and build guide
- `docs/sections/*.tex` - System documentation source files (hardware pinout, protocols, style guides)
  - `03_hardware_pinout.tex` - Complete GPIO pin assignments and peripheral connections
  - `01_nanopb_protocol.tex` - SPI communication protocol specification
  - `02_protobuf_schemas.tex` - Protocol Buffer message definitions
  - `04_style_guide.tex` - Protocol Buffer coding standards
  - `06_nasa_power_of_10.tex` - Safety-critical coding rules
  - `07_gateway_architecture.tex` - Gateway service design
- `docs/star_documentation.pdf` - Compiled documentation (reference `.tex` files for latest changes)
