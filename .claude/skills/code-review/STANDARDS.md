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
1. **Enums** - For related integer constants
2. **const variables** - For single typed values
3. **Macros** - Only when compile-time evaluation required

**Compliant Example:**
```c
/* PREFER: Type-safe enum */
typedef enum {
    k_motor_state_idle    = 0,
    k_motor_state_running = 1,
    k_motor_state_error   = 2,
} motor_state_t;

/* PREFER: Typed const */
static const float s_max_velocity_mps = 2.5f;

/* ACCEPTABLE: Compile-time macro */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
```

**Non-Compliant Example (AVOID):**
```c
/* BAD: Using macros for simple constants */
#define MAX_RETRIES (3)        // Should be enum
#define TIMEOUT_MS (1000)      // Should be enum or const
#define MAX_VELOCITY (2.5f)    // Should be const

/* WHY BAD:
 * - No type safety
 * - Not visible in debugger
 * - Can't take address
 * - Harder to namespace
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
