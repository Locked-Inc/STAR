# STAR Project Coding Standards Reference

This document provides detailed reference material for the code review agent.

## NASA Power of 10 Rules (Complete Reference)

### Rule 1: Simplify Control Flow

**Rule:** Restrict all code to very simple control flow constructs - no `goto`, `setjmp`/`longjmp`, or recursion (direct or indirect).

**Rationale:** Creates predictable program structure for human understanding and static analysis. Recursion causes unbounded stack usage - dangerous in embedded systems.

**Detection Patterns:**
```regex
\bgoto\s+\w+
\bsetjmp\s*\(
\blongjmp\s*\(
```

**Compliant Example:**
```c
rx_err_t motor_init(motor_handle_t* handle, const motor_config_t* config)
{
    if (handle == NULL || config == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    rx_err_t ret = configure_timer(handle, config);
    if (ret != RX_OK) {
        return ret;
    }

    return configure_gpio(handle, config);
}
```

### Rule 2: Fixed Loop Upper-Bounds

**Rule:** All loops must have a fixed upper bound that static analysis tools can trivially prove.

**Rationale:** Prevents runaway code, guarantees termination, enables real-time deadline verification.

**Detection Patterns:**
```regex
while\s*\(\s*1\s*\)          # while(1) - check for bounded exit
while\s*\(\s*true\s*\)       # while(true)
for\s*\(\s*;\s*;\s*\)        # for(;;)
```

**Compliant Example:**
```c
/* PREFER: Enum for integer constant */
typedef enum {
    k_max_retries = 3
} retry_limits_t;

for (uint8_t i = 0; i < k_max_retries; i++) {
    if (sensor_read() == RX_OK) {
        break;
    }
    tx_thread_sleep(10);
}
```

**Exception:** Main control loops may use `while(1)` with watchdog feeding.

### Rule 3: No Dynamic Memory After Initialization

**Rule:** Prohibit `malloc`/`free` after initialization phase.

**Rationale:** Dynamic allocation causes unpredictable performance, memory leaks, fragmentation, and corruption.

**Detection Patterns:**
```regex
\bmalloc\s*\(
\bcalloc\s*\(
\brealloc\s*\(
\bfree\s*\(
```

**Compliant Example:**
```c
/* PREFER: Enum for compile-time array sizes */
typedef enum {
    k_max_items    = 8,
    k_max_desc_len = 64
} pool_limits_t;

typedef struct {
    char items[k_max_items][k_max_desc_len];
    uint8_t item_count;
} static_pool_t;
```

### Rule 4: Keep Functions Short

**Rule:** Functions should not exceed ~60 lines (one printed page, one statement per line).

**Rationale:** Represents single verifiable logical units; improves comprehension, review, and testability.

**Detection:** Count lines between function open brace `{` and close brace `}`, excluding:
- Blank lines
- Comment-only lines

**Threshold:** Functions exceeding 60 lines should be refactored.

### Rule 5: Use Assertions/Validation

**Rule:** Minimum two assertions per function; assertions must be side-effect-free.

**Rationale:** Acts as executable documentation verifying pre-conditions, post-conditions, and invariants.

**Detection Patterns:**
```regex
RX_CHECK_NULL_PTR\s*\(
RX_RETURN_ON_ERROR\s*\(
if\s*\([^)]*==\s*NULL
assert\s*\(
```

**Compliant Example:**
```c
rx_err_t pid_compute(pid_handle_t* handle, float setpoint,
                     float measurement, float dt, float* output)
{
    /* Pre-condition checks */
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }
    if (!handle->initialized) {
        return RX_ERR_INVALID_STATE;
    }
    if (output == NULL) {
        return RX_ERR_INVALID_ARG;
    }
    if (dt <= 0.0f || dt > 1.0f) {
        return RX_ERR_INVALID_ARG;
    }

    /* Function logic */
    float error = setpoint - measurement;
    *output = handle->kp * error;
    return RX_OK;
}
```

### Rule 6: Declare Data at Smallest Scope

**Rule:** Minimize variable scope to reduce accidental corruption risks.

**Rationale:** Limits chances of accidental reference or corruption from other code.

**Red Flags:**
- Variables declared at file scope without `static`
- Variables declared far from first use
- Loop counters declared outside loop

**Compliant Example:**
```c
void process_sensors(void)
{
    for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
        float temp_c = 0.0f;  /* Declared at smallest scope */
        if (read_sensor(i, &temp_c) == RX_OK) {
            update_telemetry(i, temp_c);
        }
    }
}
```

### Rule 7: Check All Return Values

**Rule:** Validate all function returns and input parameters; treat warnings as errors.

**Rationale:** Forces explicit failure-condition handling rather than ignoring error codes.

**Detection Patterns:**
```regex
=\s*\w+\s*\([^)]*\)\s*;    # Assignment from function - check for use
\w+\s*\([^)]*\)\s*;        # Function call without assignment - check if void
```

**Compliant Example:**
```c
rx_err_t ret = bus_init(config);
if (ret != RX_OK) {
    RX_LOG_ERROR("Bus init failed: %d", ret);
    return ret;
}
```

**Explicit Ignore:**
```c
(void)optional_cleanup();  /* Explicitly ignored */
```

### Rule 8: Limit Preprocessor Use

**Rule:** Restrict to header inclusion and simple macros; forbid token pasting, recursive macros, and incomplete syntactic units.

**Rationale:** Preprocessor complexity obscures operations, fooling developers and static analysis tools.

**Preference Hierarchy:**
1. **Enums** - For ALL integer constants (preferred)
2. **static const** - For floating-point values ONLY (enum limitation)
3. **Macros** - ONLY for specific cases (never for constants)

**Allowed Macro Uses (Exhaustive List):**
1. **Reducing duplicated code** - Function-like macros (e.g., `RX_RETURN_ON_ERROR`)
2. **Conditional compilation** - Compile-time optimization (e.g., logging macros)
3. **Hardware addresses** - Register pointers (can't use enum for addresses)
4. **Build configuration flags** - Feature selection (e.g., `RX_CRC32_USE_HARDWARE`)

**Forbidden Macro Uses:**
- Simple integer constants (use enum)
- Simple floating-point constants (use static const)
- Function aliases for backward compatibility (project has no releases, no backward compatibility needed)

**No Magic Numbers Policy:**
ALL numeric literals must be named enums, including:
- Array indices (`buf[0]` → `buf[k_idx_high_byte]`)
- Bit shift amounts (`val >> 8` → `val >> k_shift_byte`)
- Protocol offsets (`frame[4]` → `frame[k_offset_payload]`)
- Register bit positions (`1 << 7` → `1 << k_bit_enable`)
- Buffer sizes, timeouts, limits (already covered above)

**Compliant Example:**
```c
/* ALWAYS PREFER: Enum for integer constants */
typedef enum {
    k_motor_state_idle    = 0,
    k_motor_state_running = 1,
    k_motor_state_error   = 2,
    k_timeout_ms          = 1000,  /* Integer - use enum! */
    k_max_retries         = 3,     /* Integer - use enum! */
    k_buffer_size         = 256    /* Integer - use enum! */
} motor_config_t;

/* ONLY for floating-point (enum can't hold float) */
static const float s_max_velocity_mps = 2.5f;  /* Must be const - not integer */
static const float s_pid_kp = 1.0f;            /* Must be const - not integer */

/* NO MAGIC NUMBERS: Array indices as enums */
typedef enum {
    k_idx_high_byte = 0,  /* MSB at index 0 (big-endian) */
    k_idx_low_byte  = 1   /* LSB at index 1 */
} be16_byte_idx_t;

static void write_be16(uint8_t* buf, uint16_t val) {
    buf[k_idx_high_byte] = (uint8_t)(val >> k_shift_byte);  /* Named indices! */
    buf[k_idx_low_byte]  = (uint8_t)(val & k_mask_byte);
}

/* NO MAGIC NUMBERS: Bit shifts as enums */
typedef enum {
    k_shift_byte   = 8,   /* Shift by 8 bits for byte operations */
    k_shift_enable = 7,   /* Enable bit at position 7 */
    k_shift_mode   = 3    /* Mode field starts at bit 3 */
} bit_shifts_t;

/* NO MAGIC NUMBERS: Bit masks as enums */
typedef enum {
    k_mask_byte   = 0xFF,    /* Single byte mask */
    k_mask_enable = 0x80,    /* Enable bit mask (1 << 7) */
    k_mask_mode   = 0x18     /* Mode field mask (bits 3-4) */
} bit_masks_t;

/* NO MAGIC NUMBERS: Protocol offsets as enums */
typedef enum {
    k_offset_sync    = 0,   /* SYNC marker at offset 0 */
    k_offset_length  = 2,   /* Length field at offset 2 */
    k_offset_payload = 4,   /* Payload starts at offset 4 */
    k_offset_crc     = 8    /* CRC at offset 8 */
} frame_offsets_t;

static void parse_frame(const uint8_t* frame) {
    uint16_t sync   = frame[k_offset_sync];      /* Named offset - clear! */
    uint16_t length = frame[k_offset_length];
    /* ... */
}

/* ACCEPTABLE: Macros for reducing duplicated code */
#define RX_RETURN_ON_ERROR(err, tag, msg) \
    do { \
        rx_err_t _err = (err); \
        if (_err != k_rx_ok) { \
            rx_log_error((tag), (msg)); \
            return _err; \
        } \
    } while (0)

/* ACCEPTABLE: Conditional compilation (optimization) */
#if LOG_LEVEL >= k_log_error
#define rx_log_error(tag, msg) internal_rx_log_error((tag), (msg))
#else
#define rx_log_error(tag, msg) ((void)0)  /* Optimized away at compile time */
#endif

/* ACCEPTABLE: Hardware register addresses */
#define CMT0_BASE ((rx_cmt_channel_regs_t*)0x00088000)
#define CMT0      (*CMT0_BASE)

/* ACCEPTABLE: Build configuration flags */
#ifdef __RX__
#define RX_CRC32_USE_HARDWARE
#else
#define RX_CRC32_USE_SOFTWARE
#endif
```

**Non-Compliant Example (AVOID):**
```c
/* BAD: Using macros for constants - NEVER DO THIS */
#define MAX_RETRIES 3          // WRONG! Use enum
#define TIMEOUT_MS 1000        // WRONG! Use enum
#define BUFFER_SIZE 256        // WRONG! Use enum
#define MAX_VELOCITY 2.5f      // WRONG! Use static const float

/* BAD: Magic numbers everywhere */
static void write_be16_bad(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);   // MAGIC! What is 0? What is 8?
    buf[1] = (uint8_t)(val & 0xFF); // MAGIC! What is 1? What is 0xFF?
}

static void parse_frame_bad(const uint8_t* frame) {
    uint16_t sync   = frame[0];     // MAGIC! What's at index 0?
    uint16_t length = frame[2];     // MAGIC! What's at index 2?
    uint8_t* data   = &frame[4];    // MAGIC! Why 4?
}

static void config_register_bad(void) {
    REG = (1 << 7) | (3 << 3);      // MAGIC! Which bits? Why?
}

/* WHY MAGIC NUMBERS ARE BAD:
 * - Unclear intent (what does 0, 8, 0xFF mean?)
 * - Hard to maintain (what if protocol changes?)
 * - Error-prone (easy to use wrong index/shift)
 * - Not searchable (can't grep for "sync offset")
 * - No semantic meaning in debugger
 *
 * WHY MACROS ARE BAD:
 * - No type safety (preprocessor text substitution)
 * - Not visible in debugger
 * - Can't take address of value
 * - No scoping/namespacing
 * - Prone to macro expansion bugs
 *
 * RULE: If it's an integer (even 0, 1, 8!), use enum. Always.
 *       If it's floating-point, use static const.
 *       Never use #define for constants.
 *       Never use literal numbers.
 */
```

### Rule 9: Restrict Pointer Use

**Rule:** Maximum one dereferencing level (`*ptr` OK, `**ptr` forbidden); no function pointers.

**STAR Deviation:** Function pointers are ALLOWED for Dependency Inversion Principle (DIP).

**Compliant DIP Pattern:**
```c
/* INTENTIONAL DEVIATION - DIP interface */
typedef struct rx_error_interface {
    rx_err_t (*record_error)(void* ctx, rx_err_t error, const char* msg);
    bool (*can_retry)(void* ctx);
    void* ctx;
} rx_error_interface_t;

/* Why: Enables mock implementations for unit testing */
```

### Rule 10: Compile with Maximum Warnings

**Rule:** Enable all compiler warnings at highest pedantry; achieve zero-warning builds.

**Required Flags:**
```cmake
-Wall -Wextra -Werror
```

**Check:** Verify CMakeLists.txt contains these flags.

## C Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Functions | snake_case | `motor_init()` |
| Variables | snake_case | `motor_speed` |
| Macros | SCREAMING_SNAKE | `MAX_SPEED` |
| Types | snake_case_t | `motor_config_t` |
| Static functions | internal_ prefix | `internal_validate()` |
| Private functions | priv_ prefix | `priv_reset()` |
| Static variables | s_ prefix | `s_instance_count` |
| Global variables | g_ prefix | `g_system_state` (avoid) |
| Enum values | k_ prefix | `k_state_idle` |

## Unit Suffixes

| Suffix | Unit | Example |
|--------|------|---------|
| `_m` | meters | `distance_m` |
| `_mps` | m/s | `velocity_mps` |
| `_mps2` | m/s^2 | `accel_mps2` |
| `_rad` | radians | `angle_rad` |
| `_rad_per_s` | rad/s | `omega_rad_per_s` |
| `_celsius` | degrees C | `temperature_celsius` |
| `_ms` | milliseconds | `timeout_ms` |
| `_us` | microseconds | `delay_us` |
| `_ma` | milliamps | `current_ma` |
| `_mv` | millivolts | `voltage_mv` |
| `_percent` | 0-100% | `duty_percent` |

## Inclusive Terminology

| Use | Avoid | Context |
|-----|-------|---------|
| Controller | Master | I2C/SPI/1-Wire bus roles |
| Peripheral | Slave | I2C/SPI/1-Wire bus roles |
| COPI | MOSI | SPI data line |
| CIPO | MISO | SPI data line |
| Primary | Master | Configuration |
| Main | Master | Configuration |

## File Documentation Standard

### Required Header Format

Every C source and header file must begin with:

1. **Path comment** (first line)
2. **Doxygen documentation block**

### Path Comment Format

```c
/* path/to/file.ext */
```

**Examples:**
- `/* lib/rx_pid/inc/rx_pid.h */`
- `/* lib/rx_pid/src/rx_pid.c */`
- `/* lib/rx_crc/inc/rx_crc.h */`

### Doxygen Header Block

**Required Tags:**
- `@file` - Exact filename
- `@brief` - One-line summary
- `@date` - Date in YYYY-MM-DD format
- `@copyright` - Copyright notice

**Optional Tags:**
- `@details` - Extended description (recommended for complex modules)
- `@code` - Usage examples (recommended for public API headers)

**Forbidden Tags:**
- `@author` - Not used in this project
- `@version` - Not used (rely on git history)

### Minimal Header Example

```c
/* lib/rx_utils/inc/rx_utils.h */

/**
 * @file rx_utils.h
 * @brief Utility helper functions for RX72N firmware
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */
```

### Extended Header Example (Public API)

```c
/* lib/rx_pid/inc/rx_pid.h */

/**
 * @file rx_pid.h
 * @brief PID controller API for closed-loop motor control
 * @details
 * Provides a stateless PID (Proportional-Integral-Derivative) controller interface with
 * anti-windup, derivative filtering, and configurable output limits. Suitable for embedded
 * systems with deterministic behavior and tunable gains for velocity or position control loops.
 *
 * Direct port from ESP32 star_pid library - pure algorithm, no hardware dependencies.
 *
 * Key Features:
 * - Standard PID algorithm with Kp, Ki, Kd gains
 * - Configurable output limits
 * - Integral anti-windup
 * - Runtime gain tuning
 * - State reset capability
 *
 * Example Usage:
 * @code
 * // Initialize PID Controller
 * rx_pid_handle_t pid;
 * rx_pid_config_t config = {
 *     .kp = 1.0f,
 *     .ki = 0.5f,
 *     .kd = 0.1f,
 *     .output_min = -100.0f,
 *     .output_max = 100.0f
 * };
 * rx_pid_init(&pid, &config);
 *
 * // Compute PID output
 * float output;
 * rx_pid_compute(&pid, setpoint, measured, dt, &output);
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */
```

### Implementation File Example

```c
/* lib/rx_pid/src/rx_pid.c */

/**
 * @file rx_pid.c
 * @brief PID controller implementation for closed-loop motor control
 * @details
 * Implements a stateless PID (Proportional-Integral-Derivative) controller with anti-windup,
 * derivative filtering, and configurable output limits. Designed for embedded systems with
 * no dynamic memory allocation and tunable gains for velocity or position control.
 *
 * Direct port from ESP32 star_pid library - pure algorithm, no hardware dependencies.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */
```

## SOLID Principles for C

### Single Responsibility
- One module = one purpose
- One function = one action
- Separate configuration from logic

### Open/Closed
- Use configuration structs for customization
- Avoid hardcoded values
- Design for extension

### Liskov Substitution
- Interface implementations interchangeable
- Mocks substitute real implementations
- Consistent error handling

### Interface Segregation
- Small, focused interfaces
- Separate read/write operations
- Don't force unused dependencies

### Dependency Inversion
- Use function pointer interfaces
- Inject dependencies via init functions
- Enable unit testing with mocks

**DIP Pattern:**
```c
typedef struct {
    rx_err_t (*read)(void* ctx, uint8_t* data, uint32_t len);
    rx_err_t (*write)(void* ctx, const uint8_t* data, uint32_t len);
    void* ctx;
} bus_interface_t;

rx_err_t driver_init(driver_t* driver, const bus_interface_t* bus);
```
