# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA standards:

- **Controller/Peripheral** - NOT master/slave (I2C, SPI, 1-Wire)
- **COPI/CIPO** - NOT MOSI/MISO (Controller Out Peripheral In / Controller In Peripheral Out)
- **Primary/Main** - NOT master (for configuration structures)

Note: External APIs may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Backward Compatibility Policy

**IMPORTANT:** This is an in-house project with **ZERO backward compatibility requirements**. There will never be public releases or versioned APIs.

### Core Principles

1. **Breaking changes are ENCOURAGED** - If it improves code quality, refactor immediately
2. **No compatibility layers** - Delete old code, update all call sites in the same change
3. **Main branch must work** - The only requirement is that main remains in a working state
4. **No gradual transitions** - No deprecation warnings, no compatibility shims, no aliases

### What This Means in Practice

**FORBIDDEN (will be rejected in code review):**
- Function aliases: `#define old_name new_name`
- Deprecation macros: `__attribute__((deprecated))`
- Wrapper functions for "compatibility"
- Comments like `// TODO: Remove old API after migration`
- Keeping unused code "just in case"

**REQUIRED:**
- Update ALL call sites in the same PR when changing APIs
- Delete old code immediately - no staged rollouts
- Rename types, functions, fields freely to improve clarity
- Ensure all tests pass and main branch builds successfully

### Examples

**❌ WRONG - Don't do this:**
```c
// WRONG - No backward compatibility shims!
#define motor_set_speed motor_set_velocity_mps  // ❌ Just update call sites
rx_err_t motor_set_speed(float speed) __attribute__((deprecated));  // ❌ Delete it

// WRONG - Don't keep old implementations
rx_err_t old_pid_compute(pid_t* pid) {  // ❌ Delete and update callers
    return new_pid_compute(pid);
}
```

**✅ CORRECT - Do this:**
```c
// CORRECT - Just rename the function and update all call sites
rx_err_t motor_set_velocity_mps(float velocity_mps);

// CORRECT - If renaming a type, update all references
typedef struct {
    float velocity_mps;  // Renamed from 'speed'
} motor_config_t;
```

### Protobuf Breaking Changes

Even Protocol Buffers can have breaking changes:
- Field numbers CAN change if all consumers are updated in the same PR
- Field types CAN change (e.g., int32 → uint32) if needed
- Messages CAN be deleted if no longer used
- Enum values CAN be reordered or removed

**Requirement:** Update firmware, gateway, and ROS2 code in the same PR.

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
# Lint and format (run from star-proto/)
cd star-proto
buf lint proto/
buf format --diff proto/

# Generate code for all targets (run from workspace root)
buf generate star-proto/proto

# Alternative: Use Makefile
make proto-gen

# Run Go tests
cd star-proto/tests/go && go test ./...
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

### ROS2 Code Quality (`star-ros2/`)

```bash
# Format all ROS2 C++ files
./scripts/format-ros2.sh

# Check formatting (CI mode)
./scripts/format-ros2.sh --check

# Run automated code review
./scripts/review-ros2.sh

# Generate review report to file
./scripts/review-ros2.sh --report review.txt

# Install pre-commit hook (recommended)
cp scripts/pre-commit-ros2 .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

### Code Review (`coderabbit`)

CodeRabbit is an AI-powered code review tool that provides detailed feedback and fix suggestions for CI/dev workflows. It helps maintain code quality by catching issues early and offering improvement insights.

```bash
# AI-powered code review with detailed feedback and fix suggestions
coderabbit review --plain

# Token-efficient mode (minimal output for AI agents)
coderabbit review --prompt-only

# Review specific files
coderabbit review --plain path/to/file.go

# Shorthand alias
cr review --plain
```

**When to use:**
- Before commits to catch issues early
- After implementing features to get improvement suggestions
- During refactoring to ensure code quality
- Use `--plain` for human-readable analysis
- Use `--prompt-only` when working with AI agents to save tokens

**IMPORTANT - Automated Review Workflow:**
When completing plan mode implementations or any significant code changes:
1. Consider running `coderabbit review --prompt-only` after implementing changes
2. Optionally review feedback and apply suggested changes as appropriate
3. Iteratively improve your code based on the feedback
4. You may skip for trivial changes (typos, docs)

**Note:** Must run from repository root directory.

## Simulator Support (e² Studio)

### Purpose

The Renesas e² studio simulator allows testing firmware **logic** without real hardware:
- ✅ Algorithm validation (PID controllers, state machines)
- ✅ Protocol parsing and encoding
- ✅ Error handling paths
- ✅ Interactive debugging (breakpoints, step-through, variable inspection)
- ❌ Timing behavior (not cycle-accurate)
- ❌ Hardware peripheral specifics (clocks, USB, SPI fully functional)

**WARNING**: Simulator builds are FOR LOGIC TESTING ONLY. Always validate critical paths on real hardware before deployment.

### Building for Simulator

#### Option 1: e² studio (Interactive Debugging)

**Creating Simulator Build Configuration**:
1. In e² studio, right-click project "e2-studio-star-rx72n-firmware" → Properties
2. Navigate to: C/C++ Build → Manage Configurations
3. Click "New..." button
4. Name: **"Simulator Debug"**
5. Select "Copy settings from": **"Debug"**
6. Click OK

**Adding RX_SIMULATOR_MODE Define**:
1. Still in Properties, select configuration: **"Simulator Debug"** (top dropdown)
2. Navigate to: C/C++ Build → Settings
3. Expand: **"Compiler"** → click **"Preprocessor"**
4. In "Defined symbols (-D)" section, click "Add" (green + icon)
5. Enter: **`RX_SIMULATOR_MODE`** (no value needed)
6. Click OK → Apply → Close

**Building**:
1. Project → Build Configurations → Set Active → **"Simulator Debug"**
2. Project → Build Project (Ctrl+B)
3. Verify build succeeds with warning: "RX_SIMULATOR_MODE: This build is FOR SIMULATOR ONLY"

**Launching Simulator**:
1. Run → Debug As → Renesas GDB Hardware Debugging
2. Ensure "Simulator" is selected as target device (not hardware emulator)
3. Set breakpoints, step through code, inspect variables
4. Logs appear in Console view (Window → Show View → Console)

#### Option 2: CMake (Automated Testing)

```bash
cd e2-studio-star-rx72n-firmware/tests
cmake .. -DCMAKE_BUILD_TYPE=Debug  # RX_SIMULATOR_MODE auto-enabled
make -j$(nproc)
ctest --output-on-failure
```

### What Works in Simulator

- **Control flow**: All branching, loops, function calls
- **Algorithms**: PID calculations, filtering, state machines
- **Protocol logic**: Parsing, encoding, CRC validation
- **Error paths**: Timeout handling, validation failures
- **Data structures**: Struct manipulation, array operations
- **Logging**: Output to console via stdout

### What Doesn't Work in Simulator

**Clock/Oscillator**:
- External 24 MHz crystal oscillation
- PLL/PPLL lock timing (flags never set → our fix skips polling)
- Precise frequency generation

**Peripherals**:
- **USB**: Enumeration, bulk transfers (limited or no support)
- **SPI**: External device communication (no physical devices)
- **UART**: Serial transmission (our fix redirects to console)
- **ADC**: Real sensor readings (could mock with constants)
- **Timers**: Precise timing, interrupt latency
- **DMA**: Transfer behavior, timing

**Timing**:
- Not cycle-accurate (instruction-level timing)
- Interrupt latency unpredictable
- DMA timing not modeled

### Use Hardware For

- Clock tree validation (actual 240 MHz operation)
- PLL lock timing measurements
- USB enumeration and bulk transfers
- SPI communication with real devices (DRV8243, sensors)
- UART communication (actual baud rates)
- Interrupt latency verification
- DMA transfer validation
- Real-time performance analysis
- Final integration testing

### Troubleshooting

**Problem**: Simulator still hangs in clock init
- **Solution**: Verify `RX_SIMULATOR_MODE` is defined
  - Check: Project Properties → C/C++ Build → Settings → Preprocessor
  - Should see: `RX_SIMULATOR_MODE` in defined symbols list
- **Alternative**: Build from wrong configuration (use "Simulator Debug", not "Debug")

**Problem**: No log output in simulator
- **Solution**: Open Console view (Window → Show View → Console)
- Logs use stdout, not UART hardware

**Problem**: Error "undefined reference to putchar"
- **Solution**: Ensure standard library is linked (should be automatic with GNURX)
- Check linker settings if issue persists

**Problem**: Simulator runs but functions don't behave as expected
- **Cause**: Simulator limitation (peripheral not modeled, timing issue)
- **Solution**: Test on hardware - simulator is for logic, not hardware behavior

### Implementation Details

**Simulator support added in 3 files**:
1. **`rx_simulator_config.h`**: Central configuration header with `RX_IS_SIMULATOR` macro
2. **`rx_clock_power_init.c`**: Skips PLL/PPLL polling loops (prevents 0x203 timeout)
3. **`rx_log.h`**: Redirects logging to stdout (console output)

**Hardware builds unaffected**: Conditional compilation (`#if RX_IS_SIMULATOR`) eliminates simulator code at compile time. Hardware builds produce identical binaries.

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

## Doxygen Documentation Requirements

**CRITICAL:** ALL code in the STAR project MUST be documented with comprehensive Doxygen comments using ALL applicable tags.

### Documentation Policy

This project enforces **MAXIMUM documentation coverage** with zero tolerance for undocumented code:

1. **Every file** must have complete file-level documentation
2. **Every function** must use ALL applicable Doxygen tags
3. **Every struct/enum** must document ALL members
4. **Every variable** (global, static, member) must be documented
5. **Every typedef** must have full documentation
6. **Every macro** must be documented with usage examples

**Rule:** If a Doxygen tag is applicable to a code element, it MUST be used. Do not omit tags.

### Complete Documentation Reference

See **[DOXYGEN_ROADMAP.md](DOXYGEN_ROADMAP.md)** for:
- Complete Doxygen tag reference (25 subsections, 100+ tags)
- Exhaustive templates for every code element type
- State machine documentation with PlantUML/Graphviz/MSC
- Documentation enforcement checklist (28 items)
- File-by-file documentation tracking (238 files)

### Required Tags by Code Element

**Functions - Minimum Required Tags:**
- `@brief` - One-line summary
- `@details` - Multi-paragraph explanation with algorithm description
- `@param[in/out/in,out]` - ALL parameters with direction, valid range, units, constraints
- `@return` - Return value description
- `@retval` - EVERY possible return value documented individually
- `@pre` - Preconditions (minimum 2 per NASA Rule 5)
- `@post` - Postconditions (minimum 2 per NASA Rule 5)
- `@note` - Thread safety statement
- `@code` - Usage example (if non-trivial)
- `@see` - Cross-references to related functions
- `@since` - Version introduced

**Additional Function Tags (when applicable):**
- `@par` - Special sections (Thread Safety, Performance, Re-entrancy, State Machine)
- `@warning` - Critical usage constraints
- `@attention` - Important information
- `@invariant` - Invariant conditions
- `@todo` - Future improvements (with issue tracker reference)
- `@bug` - Known issues (with issue tracker reference)
- `@deprecated` - Deprecation notice
- `@test` - Test reference
- `@startuml/@dot/@msc` - Diagrams (state machines, flows, sequences)
- `@callgraph/@callergraph` - Call graphs

**Structs/Enums - Minimum Required Tags:**
- `@struct/@enum` - Structure/enumeration tag
- `@brief` - One-line summary
- `@details` - Detailed explanation
- `/**<` - Inline comment for EVERY member/value with full explanation
- `@invariant` - Constraints on fields
- `@code` - Usage example
- `@see` - Related types

**Variables - Minimum Required Tags:**
- `@var/@def` - Variable/macro tag
- `@brief` - One-line summary
- `@details` - Purpose and usage
- `@note` - Access restrictions
- `@warning` - Direct modification warnings (for static variables)
- `@since` - Version introduced

**State Machines - Required Documentation:**
- `@startuml` state diagram showing all transitions
- Each state documented with entry/exit actions
- Transition conditions and guards
- State transition table in `@par` section

### Example: Complete Function Documentation

```c
/**
 * @brief Compute PID control output for one control cycle
 *
 * @details
 * Implements discrete-time PID algorithm with backward Euler integration,
 * anti-windup clamping, and derivative low-pass filtering. Updates internal
 * integral and derivative state for the next iteration.
 *
 * Algorithm steps:
 * 1. Compute error = setpoint - measured
 * 2. Update integral with anti-windup clamping
 * 3. Compute derivative with low-pass filtering
 * 4. Calculate output = Kp*error + Ki*integral + Kd*derivative
 * 5. Clamp output to [output_min, output_max]
 *
 * @param[in] pid PID controller handle (must be initialized)
 * @param[in] setpoint Desired value in engineering units
 * @param[in] measured Current measured value (same units as setpoint)
 * @param[in] dt_sec Time step in seconds (must be > 0, typically 0.01 for 100Hz)
 * @param[out] output Computed control output (clamped to configured limits)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, output written
 * @retval k_rx_err_null_ptr NULL pointer in pid or output
 * @retval k_rx_err_invalid_arg dt_sec <= 0
 * @retval k_rx_err_not_initialized PID not initialized via rx_pid_init()
 *
 * @pre pid must be initialized via rx_pid_init()
 * @pre dt_sec must match actual control loop period
 * @post Internal integral state updated
 * @post Internal derivative state updated
 * @post output clamped to [config.output_min, config.output_max]
 *
 * @invariant pid->integral in [config.integral_min, config.integral_max]
 *
 * @note Not thread-safe, caller must provide synchronization
 * @warning Do not call with varying dt_sec (breaks integral/derivative math)
 *
 * @par Performance:
 * Execution time: ~2 µs @ 240 MHz with -O2 optimization
 *
 * @par Example:
 * @code
 * rx_pid_handle_t pid;
 * rx_pid_init(&pid, &config);
 *
 * float output;
 * rx_err_t err = rx_pid_compute(&pid, 100.0f, 95.0f, 0.01f, &output);
 * if (err == k_rx_ok) {
 *     motor_set_pwm(output);
 * }
 * @endcode
 *
 * @see rx_pid_init() Initialize controller first
 * @see rx_pid_reset() Clear integral/derivative state
 * @see rx_pid_set_gains() Update gains at runtime
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 4 preconditions, 3 postconditions
 */
rx_err_t rx_pid_compute(rx_pid_handle_t* pid, float setpoint,
                        float measured, float dt_sec, float* output);
```

### Example: Complete Enum Documentation

```c
/**
 * @enum motor_state_t
 * @brief Motor operational states for finite state machine
 *
 * @details
 * Defines all possible states in motor control state machine.
 * Transitions managed by motor_handle_event() with strict safety rules.
 *
 * @startuml
 * [*] --> IDLE
 * IDLE --> RUNNING : motor_start()
 * RUNNING --> IDLE : motor_stop()
 * RUNNING --> ERROR : fault
 * ERROR --> IDLE : motor_reset()
 * @enduml
 *
 * @see motor_get_state() Query current state
 * @see motor_handle_event() State transition handler
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    /**
     * @brief Motor stopped, safe to reconfigure
     * @details
     * All outputs disabled, no power to motor.
     * @par Entry Actions: Disable PWM, clear faults
     * @par Valid Transitions: IDLE → RUNNING
     */
    k_motor_state_idle = 0,

    /**
     * @brief Motor running with closed-loop control
     * @details
     * PID active, outputs enabled, fault monitoring active.
     * @par Entry Actions: Enable PWM, start PID loop
     * @par Do Actions: Update PID at 100 Hz
     * @par Valid Transitions: RUNNING → IDLE, RUNNING → ERROR
     */
    k_motor_state_running = 1,

    /**
     * @brief Fault detected, motor stopped
     * @details
     * Recoverable fault (overcurrent, encoder error).
     * @par Entry Actions: Emergency disable outputs, log fault
     * @par Valid Transitions: ERROR → IDLE (after reset)
     */
    k_motor_state_error = 2,

} motor_state_t;
```

### Documentation Enforcement

Before marking any file as documented:

1. **Verify ALL tags present** - Use checklist in DOXYGEN_ROADMAP.md
2. **Run Doxygen** - Check for warnings: `doxygen Doxyfile 2>&1 | grep warning`
3. **Review generated docs** - Ensure diagrams render, cross-references work
4. **CodeRabbit review** - Run `coderabbit review --plain` to check documentation quality

**CI/CD:** Doxygen warnings will cause build failures. All public APIs must be fully documented.

### Generating Documentation

```bash
# Generate RX72N firmware docs
cd star-rx72n-firmware
doxygen Doxyfile
# Output: docs/doxygen/html/index.html

# Generate ROS2 docs
cd star-ros2
doxygen Doxyfile
# Output: docs/doxygen/html/index.html

# Check for warnings
grep -i "warning" doxygen_warnings.log
```

### Documentation Templates

See [DOXYGEN_ROADMAP.md](DOXYGEN_ROADMAP.md) for complete templates:
- Template 1: Functions (~40+ tags)
- Template 2: Structs (exhaustive member docs)
- Template 3: Enums (with state diagrams)
- Template 4: Variables (global, static, #defines)
- Template 5: Typedefs (especially callbacks)
- Template 6: Macros (with expansion details)

## Code Style

### Naming Conventions

- Functions/variables: `snake_case`
- Macros/constants: `SCREAMING_SNAKE_CASE`
- Types: `snake_case_t`
- Static functions: `internal_` prefix
- Private functions: `priv_` prefix
- Static variables: `s_` prefix
- Global variables: `g_` prefix (avoid)

### Constants and Macros (RX72N C Firmware)

**Strict preference hierarchy:**

1. **Enums** - ALWAYS use for ALL integer constants
   ```c
   // CORRECT: C23 typed enums with explicit underlying type (MANDATORY)
   typedef enum : uint8_t {
     k_motor_state_idle    = 0,
     k_motor_state_running = 1,
     k_motor_state_error   = 2,
   } motor_state_t;

   typedef enum : uint16_t {
     k_timeout_ms  = 1000,    // Integer constant → enum
     k_max_retries = 3,       // Integer constant → enum
   } motor_config_t;

   // WRONG: Untyped enum (no underlying type specified)
   typedef enum {
     k_motor_state_idle = 0,  // ❌ Missing `: uint8_t`
   } motor_state_t;

   // WRONG: Never use macros for integer constants
   #define TIMEOUT_MS (1000)  // ❌ Should be enum!
   ```

   **C23 Typed Enum Requirements (MANDATORY for RX72N firmware):**
   - ALL enums MUST specify an explicit underlying type using C23 syntax
   - Syntax: `typedef enum : <type> { ... } name_t;`
   - Choose the smallest type that fits all values:
     - `uint8_t` - Values 0-255 (most common: states, indices, small constants)
     - `uint16_t` - Values 256-65535 (timeouts in ms, medium constants)
     - `uint32_t` - Values > 65535 (addresses, large constants, bit masks)
     - `int8_t`, `int16_t`, `int32_t` - For signed values
   - This ensures predictable size, ABI stability, and debugger compatibility

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
   // ✓ CORRECT: Inline accessor with typed enum address
   typedef enum : uint32_t {
       k_cmt0_base_addr  = 0x00088000,
       k_port0_base_addr = 0x000C0000,
   } hw_addresses_t;

   typedef enum : uint8_t {
       k_bit_led = 5,
   } gpio_bits_t;

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

### No Magic Numbers (RX72N C Firmware)

**ZERO TOLERANCE for magic numbers.** ALL numeric literals must be named typed enums, including:

```c
// ✓ CORRECT: Array indices as typed enums
typedef enum : uint8_t {
    k_idx_high_byte = 0,
    k_idx_low_byte  = 1,
} be16_byte_idx_t;

buf[k_idx_high_byte] = (val >> k_shift_byte);

// ✓ CORRECT: Bit shifts as typed enums
typedef enum : uint8_t {
    k_shift_byte   = 8,
    k_shift_enable = 7,
} bit_shifts_t;

// ✓ CORRECT: Protocol offsets as typed enums
typedef enum : uint8_t {
    k_offset_sync    = 0,
    k_offset_payload = 4,
} frame_offsets_t;

// ✓ CORRECT: Bit masks as typed enums (use uint32_t for masks)
typedef enum : uint32_t {
    k_mask_byte   = 0xFF,
    k_mask_enable = 0x80,
} bit_masks_t;

// ❌ WRONG: Magic numbers
buf[0] = (val >> 8);              // What is 0? What is 8?
frame[4] = payload;               // What's at index 4?
REG = (1 << 7) | (3 << 3);       // Which bits? Why?

// ❌ WRONG: Untyped enums
typedef enum {                    // Missing `: uint8_t`
    k_idx_high_byte = 0,
} be16_byte_idx_t;
```

**Why this matters:**
- Self-documenting code (k_idx_high_byte vs 0)
- Searchable (grep for "high_byte" finds all uses)
- Maintainable (change offset in one place)
- Debugger-friendly (see names, not numbers)
- Compile-time checked (typos caught)
- **Typed enums guarantee size** (uint8_t is always 1 byte, uint32_t is always 4 bytes)
- **ABI stability** (enum size won't change between compiler versions)
- **Predictable memory layout** (critical for embedded systems and protocol structs)
- Macros lack type safety and can cause subtle bugs

### Critical Rules

- Always use braces for control statements
- Use `assert()` for programming errors only, not runtime errors
- Avoid inline ASM; if required, use `volatile` and document why
- Zero dynamic allocation in RX72N firmware (safety-critical)

## ROS2 C++ Style Guide

**Philosophy:** Maintain consistency with C firmware wherever possible. Only differ for C++-specific features (classes, namespaces, exceptions).

### When to Use This Guide

- **ROS2 packages** (`star-ros2/src/*/`): Use ROS2 C++ style
- **RX72N firmware** (`star-rx72n-firmware/`): Use C firmware style (above)
- **Gateway** (`star-gateway/`): Follow Go conventions (see star-gateway/CLAUDE.md)

### Naming Conventions

**Classes and Types**:
```cpp
// CamelCase for classes (ROS2 convention)
class StarGatewayBridgeNode : public rclcpp::Node {
  // ...
};

// CamelCase for structs used as types
struct TelemetryData {
  double battery_voltage_;
  double current_ma_;
};

// Type aliases use CamelCase
using TelemetryPtr = std::shared_ptr<TelemetryData>;
```

**Methods and Functions**:
```cpp
// snake_case for methods (same as C firmware)
void publish_telemetry(const TelemetryData & data);
bool is_connected() const;
void on_battery_state_received(const sensor_msgs::msg::BatteryState::SharedPtr msg);

// Use verb-based names that clarify actions
void check_for_errors();      // ✓ Clear intent
void error_check();           // ✗ Noun-first is confusing
```

**Variables**:
```cpp
// under_scored for variables
int loop_counter = 0;
std::string node_name = "gateway_bridge";

// Member variables with trailing underscore
class MyNode : public rclcpp::Node {
private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr telemetry_pub_;
  std::shared_ptr<grpc::Channel> grpc_channel_;
  bool is_connected_;
};

// Constants: ALL_CAPITALS
const int MAX_RETRIES = 3;
const double DEFAULT_TIMEOUT_S = 5.0;
```

**Namespaces**:
```cpp
// Package-based namespaces (under_scored)
namespace star {
namespace spi_bridge {

class SpiDriverNode : public rclcpp::Node {
  // ...
};

}  // namespace spi_bridge
}  // namespace star
```

### File Naming and Organization

**Headers**:
```cpp
// Use .hpp extension for C++ headers (not .h)
star_gateway_bridge_node.hpp
message_converter.hpp
grpc_client.hpp

// Include guards: PACKAGE_FILE_NAME_HPP_
#ifndef STAR_GATEWAY_BRIDGE_STAR_GATEWAY_BRIDGE_NODE_HPP_
#define STAR_GATEWAY_BRIDGE_STAR_GATEWAY_BRIDGE_NODE_HPP_

// ... code ...

#endif  // STAR_GATEWAY_BRIDGE_STAR_GATEWAY_BRIDGE_NODE_HPP_
```

**Source files**:
```cpp
// .cpp extension
star_gateway_bridge_node.cpp
message_converter.cpp
grpc_client.cpp
```

**Naming pattern**: `under_scored` for all filenames (matches package names)

### Header Organization

**Standard order** (enforced by .clang-format):

```cpp
// 1. License and copyright
// Copyright (c) 2026 STAR Project
// Licensed under MIT

// 2. Include guard
#ifndef STAR_SPI_BRIDGE_SPI_BRIDGE_NODE_HPP_
#define STAR_SPI_BRIDGE_SPI_BRIDGE_NODE_HPP_

// 3. ROS2 core includes
#include <rclcpp/rclcpp.hpp>

// 4. ROS2 message includes
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

// 5. Project includes
#include "star/v1/motor_control.pb.h"

// 6. System C++ includes
#include <memory>
#include <string>

// 7. System C includes
#include <cstdint>

// 8. Namespace declaration
namespace star {
namespace spi_bridge {

// 9. Class/function declarations

}  // namespace spi_bridge
}  // namespace star

#endif  // STAR_SPI_BRIDGE_SPI_BRIDGE_NODE_HPP_
```

### Formatting

**Line Length**:
- ROS2 C++: **120 characters** (star-ros2/.clang-format)
- C firmware: **100 characters** (star-rx72n-firmware/.clang-format)

**Indentation**:
```cpp
// 2 spaces (same as C firmware)
class MyNode : public rclcpp::Node {
public:
  MyNode() : Node("my_node") {
    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(100),
      std::bind(&MyNode::timerCallback, this));
  }

private:
  void timerCallback() {
    // ...
  }

  rclcpp::TimerBase::SharedPtr timer_;
};
```

**Braces**:
```cpp
// Functions: Braces on new line (same as C firmware)
void myFunction()
{
  // ...
}

// Control statements: Cuddled braces
if (condition) {
  // ...
} else {
  // ...
}

// Namespaces: No indentation inside
namespace star {
namespace spi_bridge {

// Content at zero indent
class MyClass {
};

}  // namespace spi_bridge
}  // namespace star
```

### ROS2-Specific Patterns

**Node Inheritance**:
```cpp
// Inherit from rclcpp::Node for basic nodes
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  StarGatewayBridgeNode();  // Constructor

private:
  void telemetry_callback();  // Timer callback

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr telemetry_pub_;
  rclcpp::TimerBase::SharedPtr telemetry_timer_;
};

// Use rclcpp_lifecycle::LifecycleNode for safety-critical nodes
class StarSpiDriverNode : public rclcpp_lifecycle::LifecycleNode {
public:
  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  StarSpiDriverNode();

  // Lifecycle transitions
  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override;

private:
  // ...
};
```

**Publishers and Subscribers**:
```cpp
class MyNode : public rclcpp::Node {
public:
  MyNode() : Node("my_node") {
    // Publisher: use SharedPtr
    telemetry_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/telemetry",
      10  // QoS depth
    );

    // Subscriber: use std::bind
    cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel",
      10,
      std::bind(&MyNode::cmd_vel_callback, this, std::placeholders::_1)
    );
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    // Handle message
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr telemetry_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
};
```

**Timers**:
```cpp
// Use create_wall_timer for periodic operations
timer_ = this->create_wall_timer(
  std::chrono::milliseconds(100),  // 10 Hz
  std::bind(&MyNode::timer_callback, this)
);
```

### Error Handling

**ROS2 uses exceptions** (different from C firmware):

```cpp
// Throw exceptions for errors
void publish_telemetry()
{
  if (!grpc_channel_) {
    throw std::runtime_error("gRPC channel not initialized");
  }

  // ...
}

// Catch exceptions in callbacks (avoid crashing node)
void timer_callback()
{
  try {
    publish_telemetry();
  } catch (const std::exception & e) {
    RCLCPP_ERROR(this->get_logger(), "Failed to publish telemetry: %s", e.what());
  }
}
```

**Logging** (use rosconsole, not printf):
```cpp
// ROS2 logging macros (preferred)
RCLCPP_INFO(this->get_logger(), "Node started");
RCLCPP_WARN(this->get_logger(), "Connection lost, retrying...");
RCLCPP_ERROR(this->get_logger(), "Failed to initialize: %s", error_msg.c_str());
RCLCPP_DEBUG(this->get_logger(), "Processing message %d", count);

// Throttled logging (max once per 5 seconds)
RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
  "Telemetry command stale (%ldms > %dms)", cmd_age_ms, timeout_ms_);

// Never use printf/cout in ROS2 nodes
printf("Debug message");        // ✗ Don't use
std::cout << "Debug" << std::endl;  // ✗ Don't use
```

### Documentation

**Doxygen comments** (use /** and /**< - same as C firmware):

```cpp
/**
 * @brief Brief description of class
 *
 * Detailed description can span multiple lines. Explain purpose,
 * usage, and any important details.
 */
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  /**
   * @brief Constructor for gateway bridge node
   *
   * @param options Node options for configuration
   */
  explicit StarGatewayBridgeNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  /**
   * @brief Callback for telemetry publishing timer
   *
   * Forwards latest telemetry to Gateway via gRPC. Called at 10 Hz.
   */
  void telemetry_timer_callback();

  rclcpp::TimerBase::SharedPtr telemetry_timer_;  /**< Timer for telemetry publishing */
};
```

### Constants

**Prefer enums and const** (same philosophy as C firmware):

```cpp
// Enum for related constants
enum class MotorState {
  kIdle = 0,
  kRunning = 1,
  kError = 2
};

// const for single values
const int DEFAULT_QOS_DEPTH = 10;
const double MAX_LINEAR_VELOCITY_MPS = 1.0;

// static const for class-specific constants
class MyNode : public rclcpp::Node {
private:
  static constexpr int kMaxRetries = 3;
  static constexpr double kTimeoutS = 5.0;
};
```

### Differences from C Firmware Style

| Feature | C Firmware (RX72N) | ROS2 C++ |
|---------|-------------------|----------|
| **Headers** | `.h` | `.hpp` |
| **Classes** | N/A | CamelCase |
| **Methods** | snake_case | snake_case (same) |
| **Variables** | snake_case | snake_case (same) |
| **Member vars** | `s_` prefix | trailing `_` |
| **Line limit** | 100 chars | 120 chars |
| **Namespaces** | Not used | star::package_name:: |
| **Error handling** | Return codes | Exceptions |
| **Logging** | uart_puts() | RCLCPP_INFO/WARN/ERROR |
| **Include guards** | `STAR_RX72N_FILE_H` | `PACKAGE_FILE_HPP_` |
| **Doxygen** | `/**` and `/**<` | `/**` and `/**<` (same) |

### Formatting Enforcement

```bash
# Format ROS2 C++ code
cd star-ros2
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Check formatting (CI/CD)
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
```

**CI enforcement**: `.github/workflows/ros2.yml` runs clang-format check on all PRs.

### Additional Resources

- **ROS2 C++ Style Guide**: https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html
- **ROS C++ Best Practices**: https://wiki.ros.org/CppStyleGuide
- **Google C++ Style Guide**: https://google.github.io/styleguide/cppguide.html (ROS2 loosely based on this)
- **This project's C style**: See "Code Style" section above

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
- **C23 typed enums** for ALL integer constants (mandatory): `typedef enum : uint8_t { ... } name_t;`
- Macros ONLY for: duplicated code, conditional compilation, build flags
- Hardware register access: Use inline accessor functions (never macros)
- See "Constants and Macros (RX72N C Firmware)" section above for complete policy

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

## Summary Documents

**Do not create summary documents, integration summaries, or completion reports unless explicitly requested by the user.** This includes files like `INTEGRATION_SUMMARY.md`, `COMPLETION_REPORT.md`, test scripts, or similar documentation. Only create these if the user specifically asks for them.

## RX72N Peripheral Implementation Tracking

**CRITICAL:** The STAR project tracks complete and correct implementation of all RX72N peripherals against the official Renesas User's Manual. These documents ensure zero mistakes in register addresses, initialization sequences, and peripheral configuration.

### 📚 START HERE: [RX72N_VERIFICATION_SUMMARY.md](RX72N_VERIFICATION_SUMMARY.md)

**Quick reference guide** for the entire RX72N verification system. Read this first to understand how all the documents work together.

### Implementation Documentation

| Document | Purpose | When to Use |
|----------|---------|-------------|
| **[RX72N_VERIFICATION_SUMMARY.md](RX72N_VERIFICATION_SUMMARY.md)** | **Quick reference** - Overview, workflows, automation | **START HERE** - explains entire system |
| **[RX72N_ROADMAP.md](RX72N_ROADMAP.md)** | Status tracking for all 63 manual chapters | Check implementation status, priorities |
| **[RX72N_FEATURE_CHECKLIST.md](star-rx72n-firmware/RX72N_FEATURE_CHECKLIST.md)** | Detailed feature verification per chapter | **Verify every feature** the manual says MCU can do |
| **[RX72N_IMPLEMENTATION_PLAN.md](RX72N_IMPLEMENTATION_PLAN.md)** | Implementation guide with templates | Step-by-step instructions for peripherals |
| **[RX72N_IMPLEMENTATION_PROMPT.md](RX72N_IMPLEMENTATION_PROMPT.md)** | Ready-to-use prompts for Claude | Copy-paste prompts to guide Claude |
| **`star-rx72n-firmware/RX72N_Manual_Chapters/`** | Official Renesas Manual (63 chapters, 3240 pages) | **AUTHORITATIVE SOURCE** for all specs |

### Peripheral Implementation Workflow

When working on RX72N peripherals:

1. **Check Status**: Look up peripheral in [RX72N_ROADMAP.md](RX72N_ROADMAP.md)
2. **Read Manual**: Review corresponding chapter in `RX72N_Manual_Chapters/ChXX_*.txt`
3. **Review Features**: Check [RX72N_FEATURE_CHECKLIST.md](star-rx72n-firmware/RX72N_FEATURE_CHECKLIST.md) for detailed feature list
4. **Follow Plan**: Use implementation steps from [RX72N_IMPLEMENTATION_PLAN.md](RX72N_IMPLEMENTATION_PLAN.md)
5. **Use Prompts**: Reference [RX72N_IMPLEMENTATION_PROMPT.md](RX72N_IMPLEMENTATION_PROMPT.md) for specific tasks
6. **Verify**: Complete all verification checklist items from FEATURE_CHECKLIST.md
7. **Update Status**: Mark complete in [RX72N_ROADMAP.md](RX72N_ROADMAP.md)

### Status Legend

| Status | Symbol | Meaning |
|--------|--------|---------|
| VERIFIED | ✅ | Fully implemented, tested, verified correct against manual |
| COMPLETE | 🟢 | Implemented but needs verification |
| PARTIAL | 🟡 | Some functionality done, needs completion |
| IN PROGRESS | 🔵 | Currently being worked on |
| NOT STARTED | ⚪ | Not yet implemented |
| NOT NEEDED | ⚫ | Not required for STAR project |

### Critical Priority Items

Before working on new features, fix these critical peripheral issues:

1. **USB CDC (Ch40)** - Fix bulk transfer reliability (blocks debugging)
2. **Clock Generation (Ch09)** - Verify all register addresses and frequencies
3. **Hardware CRC (Ch46)** - Implement hardware CRC (performance critical)

See [RX72N_ROADMAP.md](RX72N_ROADMAP.md) "Next Steps" section for full priority list.

### Verification Requirements

Before marking any peripheral as ✅ VERIFIED:

- [ ] All register addresses match Ch04 memory map **EXACTLY**
- [ ] All register offsets verified against manual tables
- [ ] All bit field definitions correct
- [ ] Module initialization tested on hardware
- [ ] Unit tests pass (error cases + success cases)
- [ ] Complete Doxygen documentation (see [DOXYGEN_ROADMAP.md](DOXYGEN_ROADMAP.md))
- [ ] NASA Power of 10 compliance verified
- [ ] No magic numbers (all constants in typed enums)
- [ ] Code review passed (`coderabbit review --plain`)

**NEVER mark a peripheral as VERIFIED unless ALL checklist items pass.**

---

## Key Documentation

**IMPORTANT:** Always reference the LaTeX source files (`.tex`) in `docs/sections/` for accurate technical information, NOT the compiled PDF.

- `star-rx72n-firmware/CLAUDE.md` - Detailed RX72N firmware guide
- `star-gateway/CLAUDE.md` - Gateway service architecture and build guide
- **[RX72N_ROADMAP.md](RX72N_ROADMAP.md)** - RX72N peripheral implementation status (63 chapters tracked)
- **[RX72N_IMPLEMENTATION_PLAN.md](RX72N_IMPLEMENTATION_PLAN.md)** - Peripheral implementation guide with templates
- **[RX72N_IMPLEMENTATION_PROMPT.md](RX72N_IMPLEMENTATION_PROMPT.md)** - Main prompt for peripheral work
- `docs/sections/*.tex` - System documentation source files (hardware pinout, protocols, style guides)
  - `03_hardware_pinout.tex` - Complete GPIO pin assignments and peripheral connections
  - `01_nanopb_protocol.tex` - SPI communication protocol specification
  - `02_protobuf_schemas.tex` - Protocol Buffer message definitions
  - `04_style_guide.tex` - Protocol Buffer coding standards
  - `06_nasa_power_of_10.tex` - Safety-critical coding rules
  - `07_gateway_architecture.tex` - Gateway service design
- `docs/star_documentation.pdf` - Compiled documentation (reference `.tex` files for latest changes)
