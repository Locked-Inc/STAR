# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA standards:

- **Controller/Peripheral** - NOT master/slave (I2C, SPI, 1-Wire)
- **COPI/CIPO** - NOT MOSI/MISO (Controller Out Peripheral In / Controller In Peripheral Out)
- **CS (Chip Select)** - NOT SS (Slave Select). Use "CS" for SPI chip select signals and "Chip Select" in documentation.
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

**[FAIL] WRONG - Don't do this:**
```c
// WRONG - No backward compatibility shims!
#define motor_set_speed motor_set_velocity_mps  // [FAIL] Just update call sites
rx_err_t motor_set_speed(float speed) __attribute__((deprecated));  // [FAIL] Delete it

// WRONG - Don't keep old implementations
rx_err_t old_pid_compute(pid_t* pid) {  // [FAIL] Delete and update callers
    return new_pid_compute(pid);
}
```

**[PASS] CORRECT - Do this:**
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
- Field types CAN change (e.g., int32 -> uint32) if needed
- Messages CAN be deleted if no longer used
- Enum values CAN be reordered or removed

**Requirement:** Update firmware, gateway, and ROS2 code in the same PR.

## Character Encoding Policy

**MANDATORY:** ALL source files in the STAR project must use **pure 7-bit ASCII only**
(Unicode code points U+0000-U+007F). This applies to every `.c`, `.h`, `.go`, `.cpp`,
`.hpp`, `.proto`, `.ts`, and `.md` file -- including comments, documentation, and string
literals.

**Rationale:** Multi-byte UTF-8 characters break downstream toolchains (static analyzers,
MISRA checkers, code coverage tools, Windows IDEs) used by other teams integrating with
the STAR project.

### Enforcement

A pre-commit hook at `scripts/git/pre-commit` rejects any commit containing non-ASCII
characters in source files. CI/CD will also run the check.

To auto-fix non-ASCII in a file:

```
python3 scripts/utils/fix-encoding.py path/to/file
```

To check a directory without modifying files:

```
python3 scripts/utils/fix-encoding.py --check path/to/dir
```

## Engineering Discipline: No Guessing on Embedded Bring-Up

**MANDATORY** for all RX72N / BeagleBone Blue / STAR PCB work. This section
overrides any instinct to "try a value and re-flash".

### P0 Rules (never violate)

1. **Read before you write.** Before touching any peripheral register,
   clock divisor, pin-mux, baud rate, DMA channel, or ISR priority, open
   the authoritative source and read it. Editing first is not allowed.
2. **Cite the source of every hardware-facing change.** In your response
   to the user, name the file and line (or datasheet section) that
   justified the value. "I set BRR=31 because `libs/rx_hal/inc/rx72n_clock.h:42`
   defines `PCLKB = 60 MHz` and the RX72N HW manual section 36.2.9 gives
   `N = PCLKB/(64 * 2^(2n-1) * baud) - 1`." No uncited hardware values.
3. **Cap empirical tries at ONE.** If a first guided attempt (grounded in
   the docs) fails, stop editing. Return to the authoritative source,
   enumerate candidate causes (firmware / host tty / wiring / clock tree
   / module-stop / pin-mux), and rule them out by *reading*, not by
   reflashing. No "try X, reflash, try Y, reflash" loops.
4. **Root cause before symptoms.** When output is wrong, identify the
   mechanism (FPU context save, clock tree, MSTPCR, PFS/PWPR, MPC PSEL,
   ISR priority) -- not the surface value. Fix once at the right layer.
5. **Trust headers over comments.** When a stripped-down test file's
   comment and the `libs/rx_hal/inc/*.h` header disagree (e.g. encoder_test
   claims "PCLKB=120 MHz" but `rx72n_clock.h` says 60 MHz), the header wins.
6. **Trust known-good bench tests over fresh code.** If `motor_spin_test/`,
   `encoder_test/`, or `imu_test/` already init the peripheral successfully
   on the bench, diff your new code against theirs. Do not freelance an
   init sequence when a proven one exists.

### Authoritative Sources (in precedence order)

| Rank | Source | When to open |
|------|--------|--------------|
| 1 | `star-rx72n-firmware/libs/rx_hal/inc/rx72n_*.h` | Register field layouts, clock constants, bit shifts -- ground truth for this codebase |
| 2 | Renesas RX72N HW manual `R01UH0824` | Peripheral init sequences, timing, module-stop tables, MPC PSEL values |
| 3 | DRV8263H datasheet | Motor-driver PWM mode, tDEAD, IPROPI gain, nSLEEP/DRVOFF power-up ordering |
| 4 | STAR PCB schematic + `docs/sections/03_hardware_pinout.tex` | Pin assignments, pull-ups, current-sense wiring, LED polarity |
| 5 | Known-good test dir init code (`motor_spin_test/`, `encoder_test/`, `imu_test/`, `pwm_test_hal/`, `uart_test/`) | Authoritative bench-proven init sequences |
| 6 | Existing lib module (`libs/rx_motor/`, `libs/rx_encoder/`, `libs/rx_drv8263/`, `libs/rx_hal/`) | Production init paths already under review |

Comments in stripped-down bring-up code (`motor_spin_test/sci9.c` etc.)
are **not** authoritative. They have been wrong before. Trust the header
or the manual.

### Embedded Gotchas to Pattern-Match Instantly

Before proposing any RX72N peripheral fix, check each of these:

- **Module-stop not released** -- MSTPCRA/B/C/D bit for that peripheral
  still set. Silent "peripheral ignores all writes" failure mode.
- **PFS locked** -- writes to PmnPFS require `PWPR.B0WI=0` then `PWPR.PFSWE=1`.
  Missing unlock = pin-mux never changes = no signal at pad.
- **MPC PSEL wrong** -- PSEL value differs per pin (e.g. 0x02 vs 0x03 for
  MTCLK vs TCLK). Check the MPC chapter's pin-function table, not a sibling pin.
- **Clock tree** -- PCLKA vs PCLKB vs PCLKD vs ICLK vs FCLK. SCI uses PCLKB.
  GPTW uses PCLKA. ADC12 uses PCLKD. Mixing these silently miscomputes dividers.
- **FPU context save** -- ThreadX RXv3 port needs `TX_THREAD_EXTENSION_2` and
  `TX_ENABLE_FPU_SUPPORT` for any task that uses floats. Silent task death /
  corruption otherwise.
- **GPTW vs MTU vs TPU** -- different register maps, different PWM capabilities.
  Don't copy MTU init into GPTW.
- **PSW.I / ISR re-entrancy** -- interrupts disabled across peripheral init?
  CMT0 tick won't fire until PSW.I=1 again.
- **DRV8263H power-up** -- nSLEEP -> tWAKE -> DRVOFF low -> PWM. Skipping
  tWAKE produces OUT1/OUT2 stuck at Vm/2.
- **Encoder quadrature mode** -- MTU `TMDR=0x04` for phase-counting. TMDR=0
  gives free-running timer, not encoder.
- **Volatile on MMIO + cache coherency** -- register accessors in `libs/rx_hal/`
  use `volatile`; new accessor code must too.
- **Host-side stty state** -- before blaming BRR, `stty -F /dev/ttyACM0 -a`
  to rule out stale host baud from a previous session.

### Workflow Discipline

- **Always read before you write.** This is both a rule and a forcing
  function -- the `Edit` tool will refuse to edit a file you haven't read,
  but that is the floor, not the ceiling. Read the *authoritative* source
  (header/manual/schematic/known-good test), not only the file being edited.
- **Cite file:line in every hardware-facing change** in your response
  to the user. "Justified by `libs/rx_hal/inc/rx72n_gptw_regs.h:118`" is
  acceptable. "Looks right" is not.
- **Compact before context drift.** At ~50% context use `/compact` rather
  than letting context grow past 60-70%, where instruction-following
  degrades and basic register errors resurface.
- **One empirical test max.** If re-reading the relevant header/manual
  section doesn't resolve the failure mode within one guided bench attempt,
  escalate by reading a wider-scope source (MSTPCR table, clock-tree
  diagram, pin-function table) rather than trying another value.
- **Diff against known-good tests.** When writing new peripheral init, open
  the corresponding `*_test/` directory's `main.c` and diff line-by-line.
  Absent or changed init ordering is almost always the root cause.

## Project Overview

**STAR (Spatial Topography Accessibility Robot)** - A distributed robotics platform for autonomous indoor ADA-compliance auditing, with custom PCB hardware, Renesas RX72N motor control firmware, a Raspberry Pi 5 control system, and Protocol Buffers communication.

### Architecture

| Component | Description |
|-----------|-------------|
| `star-rx72n-firmware/` | Renesas RX72N motor controller (CMake + GNURX + ThreadX) |
| `star-proto/` | Protocol Buffers schemas with multi-language code generation |
| `star-gateway/` | Go gateway service (UI <-> ROS2 bridge) running on RPi5 |
| `star-ui/` | User interface (TypeScript) |
| `matlab/` | Motor system identification and PID controller design |
| `schematic/` | KiCad PCB designs |

### System Communication Flow

```
User -> UI (TypeScript)
     -> Gateway (Go on RPi5)
     -> ROS2 (C++ on RPi5)
     -> [SPI Bridge - TBD: ROS2 node or custom C]
     -> RX72N (C firmware with ThreadX + nanopb)
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
- **Motor Drivers:** DRV8263H H-bridge with current sensing
- **Lidar:** RPLiDAR C1 (12m range, IP54)
- **Communication:** 10 Mbps SPI (RPi5 <-> RX72N) with nanopb + CRC-32

## Available Skills

Skills provide workflow-specific instructions that load automatically when relevant. You can also invoke any skill directly with `/skill-name`.

**How skills work:**
- When you request a task (e.g., "commit these changes"), Claude automatically loads the relevant skill
- Skill descriptions are always available so Claude knows which skill to use
- You can also invoke skills directly: `/commit`, `/pr`, `/build firmware`, etc.

**Available skills:**

- **`/build [subsystem]`** - Build commands for proto, gateway, firmware, ROS2, UI, MATLAB, docs
- **`/commit`** - Create git commits with safety gates, proper message formatting, and STAR conventions
- **`/pr`** - Create pull requests with comprehensive description, test plan, and change analysis
- **`/code-review [directory]`** - Review code for NASA Power of 10, SOLID, and STAR standards
- **`/pid-tune [motor-id]`** - Motor PID controller tuning workflow with MATLAB system identification
- **`/ros2-style`** - ROS2 C++ style guide and formatting reference for star-ros2 packages
- **`/simulator-setup`** - Configure e^2 Studio simulator for RX72N firmware logic testing

**Note:** Skills replace workflow instructions that were previously embedded in this file, reducing CLAUDE.md size while maintaining full context availability on demand.

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
 * Execution time: ~2 us @ 240 MHz with -O2 optimization
 *
 * @par Example:
 * @code
 * rx_pid_handle_t pid;
 * rx_pid_init(&pid, &config);
 *
 * float output;
 * rx_err_t err = rx_pid_compute(&pid, 100.0F, 95.0F, 0.01F, &output);
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
 * - Rule 5: [PASS] 4 preconditions, 3 postconditions
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
     * @par Valid Transitions: IDLE -> RUNNING
     */
    k_motor_state_idle = 0,

    /**
     * @brief Motor running with closed-loop control
     * @details
     * PID active, outputs enabled, fault monitoring active.
     * @par Entry Actions: Enable PWM, start PID loop
     * @par Do Actions: Update PID at 100 Hz
     * @par Valid Transitions: RUNNING -> IDLE, RUNNING -> ERROR
     */
    k_motor_state_running = 1,

    /**
     * @brief Fault detected, motor stopped
     * @details
     * Recoverable fault (overcurrent, encoder error).
     * @par Entry Actions: Emergency disable outputs, log fault
     * @par Valid Transitions: ERROR -> IDLE (after reset)
     */
    k_motor_state_error = 2,

} motor_state_t;
```

### Documentation Enforcement

Before marking any file as documented:

1. **Verify ALL tags present** - Use the tag lists above
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

See the documentation examples above for complete templates covering functions, structs,
enums, variables, typedefs, and macros.

## Code Style

### Naming Conventions

- Functions/variables: `snake_case`
- Macros/constants: `SCREAMING_SNAKE_CASE`
- Types: `snake_case_t`
- Static functions: `internal_` prefix
- Private functions: `priv_` prefix
- Static variables: `s_` prefix
- Global variables: `g_` prefix (avoid)

### Header Guards

**MANDATORY:** Use `#pragma once` for all C/C++ header files.

**Rationale:**
- Simpler syntax (one line vs three)
- Eliminates naming conflicts and typos
- Faster compilation (compiler can skip file entirely on second include)
- Widely supported compiler extension (GCC, Clang, MSVC, GNURX, CC-RX, IAR)
- De-facto industry standard and convention

**Note:** While `#pragma once` is not formally part of ISO C23, it is universally supported by all compilers used in this project and provides superior ergonomics and safety compared to traditional include guards.

**Example:**

```c
/***********************************************************************************************************************
 * File header comment, copyright, license
 ***********************************************************************************************************************/

#pragma once

/***********************************************************************************************************************
 * Includes, types, functions
 ***********************************************************************************************************************/
```

**DO NOT use traditional include guards:**

```c
// [FAIL] WRONG - Don't use traditional guards
#ifndef STAR_RX72N_FILENAME_H
#define STAR_RX72N_FILENAME_H
// ...
#endif /* STAR_RX72N_FILENAME_H */
```

**Placement:** Place `#pragma once` immediately after the file header comment (copyright/license), before any includes or code.

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
     k_timeout_ms  = 1000,    // Integer constant -> enum
     k_max_retries = 3,       // Integer constant -> enum
   } motor_config_t;

   // WRONG: Untyped enum (no underlying type specified)
   typedef enum {
     k_motor_state_idle = 0,  // [FAIL] Missing `: uint8_t`
   } motor_state_t;

   // WRONG: Never use macros for integer constants
   #define TIMEOUT_MS (1000)  // [FAIL] Should be enum!
   ```

   **C23 Typed Enum Requirements (MANDATORY for RX72N firmware):**
   - ALL enums MUST specify an explicit underlying type using C23 syntax
   - Syntax: `typedef enum : <type> { ... } name_t;`
   - Choose the smallest type that fits all values:
     - `uint8_t` - Values 0-255 (most common: states, indices, small constants)
     - `uint16_t` - Values 256-65535 (timeouts in ms, medium constants)
     - `uint32_t` - Values > 65535 (large constants, bit masks -- NOT addresses)
     - `uintptr_t` - Hardware register base addresses (MANDATORY for all address enums)
     - `int8_t`, `int16_t`, `int32_t` - For signed values
   - Use `uintptr_t` for any enum whose values are hardware memory-mapped addresses.
     On the 32-bit RX72N target `uintptr_t` == `uint32_t`, but on the 64-bit x86_64
     unit-test host `uintptr_t` == `uint64_t`. Using `uint32_t` for addresses silently
     truncates on the test host and produces wrong pointer casts.
   - This ensures predictable size, ABI stability, and debugger compatibility

2. **const variables** - ONLY for floating-point (enum limitation)
   ```c
   // CORRECT: Floating-point must use const (can't use enum)
   static const float s_max_velocity_mps = 2.5F;
   static const float s_pid_kp = 1.0F;

   // WRONG: Never use macros for floats
   #define MAX_VELOCITY_MPS (2.5F)  // [FAIL] Should be const!
   ```

3. **Macros** - ONLY for these 3 specific cases:
   ```c
   // [PASS] ALLOWED: Reducing duplicated code
   #define RX_RETURN_ON_ERROR(err, tag, msg) \
       do { \
           rx_err_t _err = (err); \
           if (_err != k_rx_ok) { \
               rx_log_error((tag), (msg)); \
               return _err; \
           } \
       } while (0)

   // [PASS] ALLOWED: Conditional compilation (optimization)
   #if LOG_LEVEL >= k_log_error
   #define rx_log_error(tag, msg) internal_rx_log_error((tag), (msg))
   #else
   #define rx_log_error(tag, msg) ((void)0)
   #endif

   // [PASS] ALLOWED: Build configuration flags
   #ifdef __RX__
   #define RX_CRC32_USE_HARDWARE
   #endif

   // [FAIL] FORBIDDEN: Hardware register addresses (use inline accessors)
   #define CMT0_BASE ((rx_cmt_channel_regs_t*)0x00088000)  // Wrong!
   #define CMT0      (*CMT0_BASE)                          // Wrong!

   // [FAIL] FORBIDDEN: Backward compatibility (no releases = no compatibility)
   #define old_function new_function  // Wrong! Update call sites instead
   ```

4. **Hardware Register Access** - Use inline accessor functions:
   ```c
   // [PASS] CORRECT: Inline accessor with typed enum address
   typedef enum : uintptr_t {
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
// [PASS] CORRECT: Array indices as typed enums
typedef enum : uint8_t {
    k_idx_high_byte = 0,
    k_idx_low_byte  = 1,
} be16_byte_idx_t;

buf[k_idx_high_byte] = (val >> k_shift_byte);

// [PASS] CORRECT: Bit shifts as typed enums
typedef enum : uint8_t {
    k_shift_byte   = 8,
    k_shift_enable = 7,
} bit_shifts_t;

// [PASS] CORRECT: Protocol offsets as typed enums
typedef enum : uint8_t {
    k_offset_sync    = 0,
    k_offset_payload = 4,
} frame_offsets_t;

// [PASS] CORRECT: Bit masks as typed enums (use uint32_t for masks)
typedef enum : uint32_t {
    k_mask_byte   = 0xFF,
    k_mask_enable = 0x80,
} bit_masks_t;

// [FAIL] WRONG: Magic numbers
buf[0] = (val >> 8);              // What is 0? What is 8?
frame[4] = payload;               // What's at index 4?
REG = (1 << 7) | (3 << 3);       // Which bits? Why?

// [FAIL] WRONG: Untyped enums
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

## ROS2 C++ Style

**Quick reference** (applies to `star-ros2/` packages only):

- 120-char line limit, 2-space indent, mandatory `clang-format` enforcement
- Classes: `CamelCase`; methods/variables: `snake_case`; member vars: trailing underscore (`velocity_mps_`)
- Headers: `.hpp` extension with `#pragma once` (not traditional include guards)
- Error handling: exceptions (not return codes)
- Logging: `RCLCPP_INFO/WARN/ERROR` macros (never `printf`/`cout`)
- Node types: inherit `rclcpp::Node` or `rclcpp_lifecycle::LifecycleNode` for safety-critical

**For comprehensive guide** (file organization, ROS2 patterns, publishers/subscribers, timers, documentation), use the **`/ros2-style`** skill.

## NASA Power of 10 Rules (STAR Implementation)

The STAR project follows NASA/JPL Power of 10 rules for safety-critical embedded code with one intentional deviation for testability.

### Rule 1: Simplify Control Flow [PASS] COMPLIANT
- No `goto`, `setjmp`/`longjmp`, or recursion
- All control flow uses `if`/`while`/`for` only
- Example: `rx_pid_init()` uses sequential error checking, no goto cleanup

### Rule 2: Fixed Loop Upper-Bounds [PASS] COMPLIANT
- All loops have statically provable bounds
- Exception: Main control loops use `while(1)` with watchdog
- Example: `for (uint8_t i = 0; i < k_max_retries; i++)` - enum provides bound

### Rule 3: No Dynamic Memory After Initialization [PASS] COMPLIANT
- **Zero malloc/free in RX72N firmware** (safety-critical)
- All buffers statically allocated with enum-defined sizes
- ThreadX stacks are static arrays
- Example: `char items[k_max_items][k_max_desc_len]` - compile-time allocation

### Rule 4: Keep Functions Short (~60 lines) [PASS] COMPLIANT
- Functions represent single verifiable units
- Example: `rx_pid_compute()` is 44 lines - complete PID algorithm in one screen

### Rule 5: Use Assertions/Validation [PASS] COMPLIANT
- Minimum 2 validation checks per function
- **Pre-conditions**: `RX_CHECK_NULL_PTR`, state validation
- **Post-conditions**: Output bounds checking, invariant validation
- Example: `rx_pid_compute()` has 4 checks (NULLx2, initialized, dt > 0)

### Rule 6: Declare Data at Smallest Scope [PASS] COMPLIANT
- Variables declared close to first use
- Loop counters in for-statement: `for (uint8_t i = 0; ...)`
- File-scope variables use `static` prefix (`s_tag`)

### Rule 7: Check All Return Values [PASS] COMPLIANT
- All function returns validated or explicitly cast to `(void)`
- Use `RX_RETURN_ON_ERROR` macro for propagation
- Example: `rx_err_t ret = bus_init(config); if (ret != k_rx_ok) return ret;`

### Rule 8: Limit Preprocessor Use [PASS] COMPLIANT
- **C23 typed enums** for ALL integer constants (mandatory): `typedef enum : uint8_t { ... } name_t;`
- Macros ONLY for: duplicated code, conditional compilation, build flags
- Hardware register access: Use inline accessor functions (never macros)
- See "Constants and Macros (RX72N C Firmware)" section above for complete policy

### Rule 9: Restrict Pointer Use [WARN] INTENTIONAL DEVIATION
- **Standard**: Maximum one level of dereferencing, no function pointers
- **STAR Deviation**: Function pointers ALLOWED for Dependency Inversion Principle (DIP)
- **Why**: Enables mock implementations for unit testing and hardware abstraction
- Example: `typedef struct { rx_err_t (*read)(void* ctx, ...); void* ctx; } bus_interface_t;`

### Rule 10: Compile with Maximum Warnings [PASS] COMPLIANT
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

For motor PID tuning workflow (MATLAB system identification, controller design, firmware integration), use the **`/pid-tune`** skill.

**Motor Model**: G(s) = 3.665 / (0.075s + 1) - DC motor first-order model with gain = 3.665 rad/s/V and time constant tau = 0.075 s (75 ms)

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
