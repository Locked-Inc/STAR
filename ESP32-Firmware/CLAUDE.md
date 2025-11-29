# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**STAR Firmware** - Sensor and Actuator Abstraction Runtime for ESP32-IDF

A modular, production-ready embedded firmware framework for ESP32 implementing Dependency Inversion Principle (DIP) for loose coupling, testability, and maintainability. The framework provides unified bus abstraction (I2C, SPI, UART, OneWire, SMBus) and comprehensive sensor/actuator drivers.

## Build Commands

### Build
```bash
# Build for ESP32-WROOM (default)
pio run -e esp32_wroom

# Build for ESP32-S3
pio run -e esp32s3
```

### Upload
```bash
# Upload to ESP32-WROOM
pio run -e esp32_wroom --target upload

# Upload to ESP32-S3
pio run -e esp32s3 --target upload
```

### Monitoring
```bash
# Serial monitor
pio device monitor
```

## Call Graph Generation

The STAR firmware includes comprehensive tooling for generating call graphs and execution flow diagrams to help understand the codebase architecture and execution paths.

### Prerequisites

Install the required tools for call graph generation:

```bash
# macOS (using Homebrew)
brew install cflow doxygen graphviz

# Ubuntu/Debian
sudo apt-get install cflow doxygen graphviz

# For Egypt tool (manual installation)
curl -O https://www.gson.org/egypt/download/egypt-1.11.tar.gz
tar -xzf egypt-1.11.tar.gz
cp egypt-1.11/egypt ./
chmod +x egypt
```

### Generate All Call Graphs

Use the automated script to generate comprehensive call graphs:

```bash
# Generate all call graph types
./scripts/generate_callgraph.sh
```

This script generates:
- **GNU cflow** text-based call graphs
- **Egypt** visual call graphs from compiled RTL dumps
- **Doxygen** documentation with interactive call/caller graphs

### Manual Generation

#### GNU cflow (Source-based Analysis)
```bash
# Forward call graph from app_main
cflow --main=app_main src/main.c lib/star_*/src/*.c > forward_callgraph.txt

# Reverse call graph (show callers)
cflow -r --main=app_main src/main.c lib/star_*/src/*.c > reverse_callgraph.txt
```

#### Egypt (Binary Analysis)
```bash
# Build with RTL dump flags (already configured in platformio.ini)
pio run -e esp32_wroom

# Generate DOT format call graph
find . -name "*.expand" -path "*/src/*" -o -name "*.expand" -path "*/star_*/*" | ./egypt > callgraph.dot

# Convert to visual formats
dot -Tpng callgraph.dot -o callgraph.png
dot -Tsvg callgraph.dot -o callgraph.svg
```

#### Doxygen (Documentation with Call Graphs)
```bash
# Generate comprehensive documentation
doxygen Doxyfile

# Open documentation
open docs/doxygen/html/index.html
```

### Output Location

All generated call graphs are stored in `docs/callgraph/`:
- `cflow/` - Text-based GNU cflow output
- `visual/` - Visual call graphs (generated from cflow analysis)
- `doxygen/` - Interactive HTML documentation

### Understanding the Call Graphs

The call graphs reveal the STAR firmware's key architectural patterns:

1. **Dependency Injection Flow**: `app_main()` → interface creation → component initialization
2. **Bus Abstraction**: Sensor drivers → `star_bus_manager` → protocol implementations
3. **Error Handling**: Centralized error interface with retry logic
4. **Resource Management**: Pin validation and bus lifecycle management

### Integration with IDEs

The generated documentation and call graphs can be integrated with various development tools:
- **VS Code**: Use the generated HTML documentation as a reference
- **CLion**: Import DOT files for visualization
- **Vim/Neovim**: Use text-based cflow output for quick reference

## Architecture

### SOLID Principles

This project adheres to the SOLID principles for maintainable and scalable software design:

1. **Single Responsibility Principle (SRP)**: A class should have only one reason to change, meaning it should only have one job or responsibility. Each component in STAR (bus manager, sensor drivers, error handler) has a focused, singular purpose.

2. **Open/Closed Principle (OCP)**: Software entities should be open for extension but closed for modification, allowing new functionality without altering existing code. New sensor drivers and bus protocols can be added without modifying the core framework.

3. **Liskov Substitution Principle (LSP)**: Objects of a superclass should be replaceable with objects of a subclass without affecting the correctness of the program. All bus implementations can be used interchangeably through the unified bus manager interface.

4. **Interface Segregation Principle (ISP)**: Clients should not be forced to depend on interfaces they do not use, promoting smaller, more specific interfaces. Components only depend on the specific interfaces they need (e.g., error interface, pin interface).

5. **Dependency Inversion Principle (DIP)**: High-level modules should not depend on low-level modules; both should depend on abstractions. This is the foundational principle of STAR's architecture (detailed below).

### Embedded Systems Best Practices — Comprehensive Guide

Below is a comprehensive list of principles, guidelines, and habits specifically valuable in embedded development. These apply broadly to microcontrollers, bare-metal systems, RTOS designs, sensor-heavy applications, communication buses, and product-level firmware.

Each entry includes:
- **Name of the principle**
- **What it means**
- **Why it matters in embedded systems**
- **How to apply it**

#### 1. Code Structure & Architecture

**1.1 Keep Modules Single-Purpose (SRP Applied to Embedded)**

**What:** Each module handles one clear responsibility (sensor driver, bus abstraction, task scheduling...)
**Why:** Easier to test, debug, and reuse.
**How:** Avoid "God files" with too many unrelated functions.

**1.2 Use Hardware Abstraction Layers (HALs)**

**What:** Separate high-level logic from hardware-specific details.
**Why:** More portable firmware, easier mocking, and easier switching MCUs.
**How:** Create interfaces via structs + function pointers for drivers.

**1.3 Use Dependency Inversion (DIP)**

**What:** High-level systems depend on abstractions (interfaces), not concrete drivers.
**Why:** Reusable code, easy replacement of I2C/SPI/Mocks.
**How:** Define "bus_interface_t" with function pointers.

**1.4 Design for Extensibility (Embedded Open-Closed Principle)**

**What:** Add new features without modifying existing stable modules.
**Why:** Lowers regression risk.
**How:** Use callbacks, virtual tables, configuration structs.

**1.5 Avoid Deeply Nested Logic**

**What:** Too many nested if/switch structures cause brittle code.
**Why:** Hard to debug on constrained hardware.
**How:** Use guard clauses, state machines, early exits.

**1.6 Prefer Predictable State Machines**

**What:** Represent behavior using state enums + handlers.
**Why:** Debuggable, deterministic, perfect for embedded systems.
**How:** "switch(state)" with explicit transitions.

**1.7 Avoid Global Variables (Unless Meaningful Singletons)**

**What:** Globals make debugging harder and introduce risk in ISR interactions.
**Why:** Hidden side effects are catastrophic in embedded code.
**How:** Pass pointers; use static file-scope variables instead of globals.

#### 2. Performance & Real-Time Behavior

**2.1 Know Your Timing Constraints**

**What:** Hard real-time vs. soft real-time vs. best-effort.
**Why:** Prevent missed deadlines and jitter.
**How:** Profile worst-case execution times (WCET).

**2.2 Minimize Work Inside Interrupts**

**What:** Keep ISRs short and simple.
**Why:** Long ISRs block lower-priority interrupts and break timing.
**How:** Use a flag or queue work to a background task.

**2.3 Avoid Blocking Delays**

**What:** avoid `while(1);` or long `vTaskDelay`.
**Why:** Blocking breaks concurrency and wastes CPU.
**How:** Use non-blocking drivers or event-driven design.

**2.4 Benchmark & Measure, Don't Guess**

**What:** Use timers, cycle counters, and GPIO toggle timing.
**Why:** Profiling ensures you meet deadlines.
**How:** Toggle a pin around code sections and measure with a scope.

#### 3. Memory Management

**3.1 Prefer Static Memory**

**What:** Avoid malloc/free in embedded systems.
**Why:** Fragmentation, leaks, and unpredictability.
**How:** Use static allocations, global buffers, or stack memory.

**3.2 Avoid Large Stack Allocations**

**What:** Stack overflows cause silent corruption.
**Why:** Microcontrollers have tiny stacks.
**How:** Place big arrays in static or global storage.

**3.3 Use const Where Possible**

**What:** Put strings/tables in flash/ROM instead of RAM.
**Why:** Saves RAM on microcontrollers.
**How:** Mark lookup tables, default configs as const.

**3.4 Validate All Buffers and Sizes**

**What:** No unchecked memcpy, dma writes, or ringbuffer ops.
**Why:** Buffer corruption leads to random crashes.
**How:** Always check lengths and boundaries.

#### 4. Safety & Robustness

**4.1 Defensive Programming**

**What:** Assume things will go wrong (bus hangs, sensors fail).
**Why:** Fault tolerance is critical in the field.
**How:** Check return values everywhere.

**4.2 Use Watchdog Timers**

**What:** Automatic reset on firmware lockup.
**Why:** Safety requirement in 99% of systems.
**How:** Kick the watchdog in the main loop or RTOS task.

**4.3 Validate Peripheral Initialization**

**What:** Confirm clocks, buses, GPIO modes succeeded.
**Why:** Avoid silent hardware misconfiguration.
**How:** Assert on unexpected states during boot.

**4.4 CRC or Checksum Critical Data**

**What:** Validate communication packets & stored configuration.
**Why:** Prevent corrupted flash or noisy I2C/SPI errors.
**How:** Use CRC-8/16/32 depending on needs.

**4.5 Fail Safe, Not Spectacularly**

**What:** On error, enter a safe state (not "undefined behavior").
**Why:** Safety issues, especially in robotics/motors.
**How:** Disable actuators, stop motors, blink error LED.

#### 5. Communication, Sensors & Drivers

**5.1 Use Abstract Bus Interfaces**

**What:** Generic I2C/SPI/UART interfaces.
**Why:** Swap drivers easily; mock in tests.
**How:** `bus_ops_t` with `.read`, `.write`, `.init`.

**5.2 Debounce Inputs (Mechanical or Digital)**

**What:** Use software or hardware debounce for buttons/switches.
**Why:** Avoid phantom button presses.
**How:** Time-based filtering or state machine.

**5.3 Handle Sensor Timeouts**

**What:** Never assume a sensor always responds.
**Why:** I2C can hang, sensors lock up.
**How:** Timeout and reset sensor/bus.

**5.4 Validate Sensor Calibration & Config Values**

**What:** Ensure values are within known good ranges.
**Why:** Sensors often report garbage after boot or reset.
**How:** Discard improbable readings.

**5.5 Use Ring Buffers for Streaming Data**

**What:** For UART, SPI DMA, or sensor bursts.
**Why:** Avoid losing data.
**How:** Implement circular queues with head/tail indices.

#### 6. Power Management

**6.1 Sleep Whenever Possible**

**Why:** Reduces power dramatically.
**How:** Use MCU low-power modes.

**6.2 Minimize Wake-ups**

**What:** Avoid periodic timers that wake too often.
**Why:** Lower battery life.
**How:** Batch work or increase intervals.

**6.3 Use DMA to Reduce CPU Load**

**What:** Offload memory transfers.
**Why:** Lower power + better performance.
**How:** Configure DMA for UART, SPI, I2C, ADC.

#### 7. Testing & Debugging

**7.1 Use Mock Drivers**

**What:** Simulate I2C/SPI peripherals.
**Why:** Test logic without hardware.
**How:** DIP with mock bus ops.

**7.2 Log Meaningfully (Serial/RTT/SWO)**

**What:** Include timestamp, module, error codes.
**Why:** Crucial for debugging in field.
**How:** A lightweight logging module.

**7.3 Use Assertions (BUT Carefully)**

**What:** Catch impossible states during development.
**Why:** Helps catch logic errors early.
**How:** `ASSERT(condition)` → blink LED or safe halt.

**7.4 Hardware Debugging Tools**

Essential tools for verifying timing, bus activity, and signal integrity:
- Oscilloscope
- Logic analyzer
- Multimeter
- Serial output

#### 8. Reliability & Fault Tolerance

**8.1 Avoid Floating Inputs**

**What:** Floating GPIO pins read random values.
**Why:** Causes jitter and ghost events.
**How:** Always use pull-up or pull-down.

**8.2 Reset Peripherals if Stuck**

**What:** I2C or SPI can lock up.
**Why:** Recover automatically in field.
**How:** Re-init bus + reconfigure sensor.

**8.3 Handle Brown-Outs**

**What:** Voltage dips cause corrupted flash & erratic behavior.
**How:** Enable brown-out detection (BOD/BOR).

**8.4 Flash Wear-Leveling**

**What:** Avoid rewriting the same flash sector constantly.
**Why:** Flash has finite erase cycles.
**How:** Use rotation, journals, wear-leveling patterns.

#### 9. Style, Maintainability & Documentation

**9.1 Consistent Naming & Formatting**

**Why:** Makes the code readable across teams.
**How:** Use the same naming pattern across all drivers.

**9.2 Document Assumptions & Hardware Behavior**

**Why:** Next engineer (or you in 6 months) will need it.
**How:** Document timing, bus requirements, pin states.

**9.3 Keep Configuration in One Place**

**What:** Central config header/struct.
**Why:** Less scattering; easier to tune.
**How:** "system_config.h" style file.

**9.4 Avoid Magic Numbers**

**Why:** Hard to understand & maintain.
**How:** Named constants with comments.

**9.5 Version Hardware + Firmware Together**

**What:** Track firmware for specific PCB revisions.
**Why:** Avoid mismatched releases.
**How:** Use git tags and HWREV macros.

### Dependency Inversion Principle (DIP)

The codebase is built on DIP to ensure loose coupling and testability. **Always respect this architecture when adding code.**

#### Core Abstraction Layer (`lib/star_core/`)
- Contains **interfaces only** - no implementations
- Defines `star_error_interface_t` and `star_pin_interface_t`
- All higher-level components depend on these interfaces, never on concrete implementations

#### Implementation Modules
- `star_error_handler/` - Provides concrete error handling with retry logic
- `star_pin_validator/` - Validates GPIO pin conflicts
- `star_bus/` - Unified bus abstraction layer

#### Key Pattern
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

**Why this matters:**
- Components can be tested with mock interfaces
- Implementations can be swapped without changing dependent code
- Optional dependencies can be passed as NULL
- No circular dependencies

### Bus Abstraction (`lib/star_bus/`)

The bus manager provides a unified API for all communication protocols:

#### Key Components
- `star_bus_manager.h` - Central bus management
- `star_bus_config.h` - Bus configuration creation
- `star_bus_i2c.h`, `star_bus_spi.h`, `star_bus_uart.h` - Protocol implementations
- `star_bus_async.h` - Asynchronous operations
- `star_bus_batch.h` - Batch transactions

#### Thread Safety
All bus operations are mutex-protected with configurable timeouts. The default mutex timeout is 1000ms (set via `CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS`).

**Important:** Use `star_bus_manager_with_bus()` instead of `star_bus_manager_find_bus()` to avoid race conditions where another thread removes a bus while you're using it.

### Sensor Drivers (`lib/star_sensor_*/`)

Each sensor driver follows a consistent pattern:
- Takes `star_bus_manager_t*` and bus name during initialization
- Uses the bus manager to perform all communication
- Returns structured data via driver-specific data types

Example driver structure:
```c
// Initialize sensor
mpu6050_handle_t imu;
mpu6050_config_t config = {
    .bus_manager = &bus_manager,
    .bus_name = "imu_bus"
};
star_sensor_mpu6050_init(&imu, &config);

// Read data
mpu6050_data_t data;
star_sensor_mpu6050_read(&imu, &data);
```

## Code Style (from styleguide.txt)

### Critical Rules

1. **Always use braces** for control statements (if, for, while), even for single statements
   ```c
   // WRONG
   if (foo) bar();

   // CORRECT
   if (foo) {
       bar();
   }
   ```

2. **Assertions**
   - Use `assert()` for catching programming errors during development
   - Never use assertions for runtime error handling
   - Avoid side effects in assertions (they're removed in release builds)
   - Use `ESP_ERROR_CHECK` for ESP-IDF functions during development
   - Use `ESP_ERROR_CHECK_WITHOUT_ABORT` to log errors without aborting

3. **Inline ASM**
   - Avoid ASM unless absolutely necessary
   - Always use `volatile` for inline ASM
   - Document why ASM is required
   - Use named registers/constraints, not hardcoded register names

4. **Naming Conventions**
   - Functions and variables: `snake_case`
   - Macros and constants: `SCREAMING_SNAKE_CASE`
   - Types: `snake_case_t` suffix
   - Static functions: `internal_` prefix (file-scoped only)
   - Private non-static functions: `priv_` prefix (cross-file within module)
   - Static variables: `s_` prefix
   - Global variables: `g_` prefix (avoid when possible)

5. **Documentation**
   - Use Doxygen-compatible comments
   - Document public APIs thoroughly
   - Explain the "why" not just the "what"

## Project Structure

```
lib/
├── star_core/           # Abstract interfaces (DIP foundation)
├── star_error_handler/  # Error handling with retry logic
├── star_pin_validator/  # GPIO conflict detection
├── star_bus/            # Unified bus abstraction
├── star_sensor_*/       # Sensor drivers (MPU6050, HCSR04, PCA9685, BNO055_BMP280, etc.)
├── star_module_*/       # Communication modules
└── star_bms_bq7850/     # Battery management system

src/
└── main.c               # Application entry point

partitions/
├── partitions_4mb.csv   # ESP32-WROOM partition table
└── partitions_16mb.csv  # ESP32-S3 partition table
```

## Library Structure

Each library follows this structure:
```
lib/star_component/
├── CMakeLists.txt       # CMake configuration
├── library.json         # PlatformIO library metadata
├── include/             # Public headers
│   └── star_component.h
└── src/                 # Implementation files
    └── star_component.c
```

## Platform-Specific Configuration

### ESP32-WROOM (4MB Flash)
- Default I2C: SDA=GPIO21, SCL=GPIO22
- Default SPI: MOSI=23, MISO=19, CLK=18, CS=5
- Default UART: TX=17, RX=16

### ESP32-S3 (16MB Flash + 8MB PSRAM)
- Default I2C: SDA=GPIO8, SCL=GPIO9
- Default SPI: MOSI=11, MISO=13, CLK=12, CS=10
- Default UART: TX=43, RX=44

**Note:** These are configurable via build flags in `platformio.ini`

## Common Patterns

### Initialize Bus Manager with DIP
```c
// 1. Create error handler
error_handler_t error_handler;
error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

// 2. Get interfaces
star_error_interface_t error_iface;
star_pin_interface_t pin_iface;
error_handler_get_interface(&error_iface, &error_handler);
pin_validator_get_interface(&pin_iface);

// 3. Initialize bus manager
star_bus_manager_t bus_manager;
star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);

// 4. Create and add bus
star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
    "sensor_bus", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
star_bus_manager_add_bus(&bus_manager, i2c_bus);
```

### Add a Sensor Driver
```c
// Initialize sensor with bus manager reference
mpu6050_handle_t imu;
mpu6050_config_t cfg = {
    .bus_manager = &bus_manager,
    .bus_name = "sensor_bus"
};
star_sensor_mpu6050_init(&imu, &cfg);

// Read sensor
mpu6050_data_t data;
star_sensor_mpu6050_read(&imu, &data);
```

## Adding New Components

### Adding a Sensor Driver

1. Create library structure:
   ```
   lib/star_sensor_newsensor/
   ├── include/star_sensor_newsensor.h
   ├── src/star_sensor_newsensor.c
   ├── library.json
   └── CMakeLists.txt
   ```

2. Follow existing sensor patterns (see `star_sensor_mpu6050/`)

3. Add dependency injection through bus manager:
   ```c
   typedef struct {
       star_bus_manager_t* bus_manager;
       const char* bus_name;
   } newsensor_config_t;
   ```

### Adding a Bus Protocol

1. Add protocol header in `lib/star_bus/include/star_bus_newprotocol.h`

2. Implement protocol in `lib/star_bus/src/star_bus_newprotocol.c`

3. Add configuration creation function in `star_bus_config.h/c`

## Error Handling

The framework uses ESP-IDF error codes (`esp_err_t`) throughout:

- **ESP_OK**: Success
- **ESP_ERR_INVALID_ARG**: Invalid argument
- **ESP_ERR_INVALID_STATE**: Invalid state for operation
- **ESP_ERR_NO_MEM**: Out of memory
- **ESP_ERR_TIMEOUT**: Operation timed out
- **ESP_FAIL**: Generic failure

The `star_error_handler` provides automatic retry with exponential backoff for transient errors.

## Pin Conflict Detection

Always validate pins before use:
```c
star_validate_pins();  // Checks all registered pins for conflicts
```

The pin validator tracks GPIO assignments and prevents conflicts between different peripherals.

## DIP Pattern vs HAL Pattern

The STAR framework now provides two distinct approaches for using sensor/actuator drivers:

### 1. DIP Pattern (Direct Driver Usage)

**Use when:** You need maximum control, custom error handling, or are building complex systems.

**Characteristics:**
- Manual dependency injection
- Explicit bus manager and error handler setup
- Thread-safe with mutex protection (1000ms timeout)
- Maximum flexibility and testability

**Example:**
```c
// Create error handler
error_handler_t error_handler;
error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

// Get interfaces
star_error_interface_t error_iface;
star_pin_interface_t pin_iface;
error_handler_get_interface(&error_iface, &error_handler);
pin_validator_get_interface(&pin_iface);

// Initialize bus manager
star_bus_manager_t bus_manager;
star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);

// Create and add I2C bus
star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
    "sensor_bus", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
star_bus_manager_add_bus(&bus_manager, i2c_bus);

// Initialize sensor (NULL error_iface creates default internally)
mpu6050_handle_t imu;
mpu6050_config_t cfg = {0};
star_sensor_mpu6050_init(&imu, &bus_manager, "sensor_bus", &error_iface, &cfg);

// Use sensor
mpu6050_data_t data;
star_sensor_mpu6050_read(&imu, &data);

// Cleanup
star_sensor_mpu6050_deinit(&imu);
star_bus_manager_remove_bus(&bus_manager, "sensor_bus");
star_bus_config_destroy(i2c_bus);
star_bus_manager_deinit(&bus_manager);
error_handler_deinit(&error_handler);
```

### 2. HAL Pattern (Convenience Layer)

**Use when:** You need simple, quick setup for single-device applications.

**Characteristics:**
- Automatic dependency setup (no manual DIP)
- Single function initialization
- Internal bus manager and error handler
- Simplified API surface

**Example:**
```c
// Initialize MPU6050 HAL (all setup automatic)
star_hal_mpu6050_handle_t hal_imu;
star_hal_mpu6050_config_t hal_config = {
    .sda_pin = GPIO_NUM_21,
    .scl_pin = GPIO_NUM_22,
    .i2c_freq = 400000,
    .i2c_addr = 0x68
};
star_hal_mpu6050_init(&hal_imu, &hal_config);

// Use sensor (same data types as DIP)
mpu6050_data_t data;
star_hal_mpu6050_read(&hal_imu, &data);

// Cleanup (frees all internal resources)
star_hal_mpu6050_deinit(&hal_imu);
```

### When to Use Each Pattern

| Scenario | Recommended Pattern |
|----------|-------------------|
| Simple single-sensor application | **HAL** |
| Multiple sensors on same bus | **DIP** |
| Custom error handling needed | **DIP** |
| Quick prototyping | **HAL** |
| Production system with logging | **DIP** |
| Unit testing with mocks | **DIP** |
| Educational/example code | **HAL** |
| Complex multi-bus system | **DIP** |

## Thread Safety

All sensor drivers are thread-safe with mutex protection:

**Standard mutex timeout:** 1000ms (configurable via `*_MUTEX_TIMEOUT_MS`)

**Protected operations:**
- Driver initialization (`init`)
- Driver deinitialization (`deinit`)
- State-modifying operations (e.g., `set_mode`, `set_temperature`)

**Non-protected operations:**
- Read operations (use `const` handle pointers)

**Example of thread-safe state modification:**
```c
// From star_sensor_mpu6050.c
esp_err_t star_sensor_mpu6050_set_accel_range(mpu6050_handle_t* handle, mpu6050_accel_range_t range)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(MPU6050_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  // Perform operation
  esp_err_t ret = /* ... */;

  xSemaphoreGive(mutex);
  return ret;
}
```

## Error Handler Injection Pattern

All sensor drivers support optional error handler injection:

**NULL error_iface:** Creates default error handler internally (owned by driver)
**Non-NULL error_iface:** Uses injected error handler (not owned by driver)

**Example with NULL (automatic):**
```c
mpu6050_handle_t imu;
mpu6050_config_t cfg = {0};
star_sensor_mpu6050_init(&imu, &bus_manager, "sensor_bus", NULL, &cfg);
// Driver creates and owns default error handler internally
```

**Example with custom error handler:**
```c
// Create custom error handler
error_handler_t my_handler;
error_handler_init(&my_handler, 5, 200, 10000, reset_callback, reset_ctx);

// Get interface
star_error_interface_t my_error_iface;
error_handler_get_interface(&my_error_iface, &my_handler);

// Inject into driver
mpu6050_handle_t imu;
mpu6050_config_t cfg = {0};
star_sensor_mpu6050_init(&imu, &bus_manager, "sensor_bus", &my_error_iface, &cfg);

// Cleanup (driver does NOT own error handler)
star_sensor_mpu6050_deinit(&imu);
error_handler_deinit(&my_handler);
```

## Available Libraries

### Core Infrastructure
- **star_core** - Abstract interfaces (error_interface, pin_interface)
- **star_error_handler** - Error handling with retry logic
- **star_pin_validator** - GPIO conflict detection
- **star_bus** - Unified bus abstraction (I2C, SPI, UART, GPIO, OneWire, SMBus)

### Sensor/Actuator Drivers (DIP Pattern)
- **star_sensor_pca9685** - 16-channel PWM controller
- **star_sensor_mpu6050** - 6-axis IMU
- **star_sensor_hcsr04** - Ultrasonic distance sensor
- **star_sensor_bno055_bmp280** - 10-DOF IMU (9-axis + pressure)
- **star_bms_bq7850** - Battery management system

### Convenience Libraries
- **star_servo** - Stateless servo angle calculations (no dependencies)

### HAL Libraries (Convenience Pattern)
- **star_hal_pca9685** - PCA9685 with automatic setup
- **star_hal_mpu6050** - MPU6050 with automatic setup
- **star_hal_hcsr04** - HC-SR04 with automatic setup
- **star_hal_bno055_bmp280** - 10-DOF IMU with automatic setup
- **star_hal_bq7850** - BQ7850 BMS with automatic setup

## GPIO Bus Usage Pattern

The HC-SR04 driver demonstrates the GPIO bus pattern:

```c
// Initialize bus manager with GPIO bus
star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
star_bus_manager_add_bus(&bus_manager, gpio_bus);

// Configure pins through GPIO bus
hcsr04_handle_t sensor;
hcsr04_config_t config = {
    .trigger_pin = GPIO_NUM_5,
    .echo_pin = GPIO_NUM_18,
    .temperature_c = 25.0f
};
star_sensor_hcsr04_init(&sensor, &bus_manager, "gpio_bus", &error_iface, &config);

// Driver uses GPIO bus for pin operations internally
float distance_cm;
star_sensor_hcsr04_read_distance(&sensor, &distance_cm);
```

## Servo Control with star_servo

The `star_servo` library provides stateless calculations:

```c
#include "star_servo.h"
#include "star_sensor_pca9685.h"

// Calculate PWM count for angle
uint16_t count = star_servo_angle_to_count(90);  // Center position

// Set servo via PCA9685
star_sensor_pca9685_set_pwm(&pca_handle, 0, 0, count);

// Or use HAL convenience function
star_hal_pca9685_set_servo_angle(&hal_handle, 0, 90);
```

**star_servo functions:**
- `star_servo_angle_to_count()` - Convert degrees to PCA9685 count
- `star_servo_pulse_to_count()` - Convert microseconds to PCA9685 count
- `star_servo_angle_to_pulse()` - Convert degrees to microseconds
- `star_servo_pulse_to_angle()` - Convert microseconds to degrees
- `star_servo_get_center_count()` - Get count for 90° (1.5ms)
- `star_servo_get_min_count()` - Get count for 0° (1.0ms)
- `star_servo_get_max_count()` - Get count for 180° (2.0ms)

