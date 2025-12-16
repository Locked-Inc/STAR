# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

Note: ESP-IDF APIs still use legacy terminology internally (e.g., `I2C_MODE_SLAVE`, `mosi_io_num`). Map these to our terminology in comments and documentation.

## Project Overview

**STAR Firmware** - Sensor and Actuator Abstraction Runtime for ESP32-IDF

A modular, production-ready embedded firmware framework for ESP32 implementing Dependency Inversion Principle (DIP) for loose coupling, testability, and maintainability. The framework provides unified bus abstraction (I2C, SPI, UART, GPIO, ADC, OneWire) and specialized motor control components including motor drivers, encoders, PID controllers, and temperature monitoring.

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
```

### Generate All Call Graphs

Use the automated script to generate comprehensive call graphs:

```bash
# Generate all call graph types
./scripts/generate_callgraph.sh
```

This script generates:
- **GNU cflow** text-based call graphs
- **Doxygen** documentation with interactive call/caller graphs

### Manual Generation

#### GNU cflow (Source-based Analysis)
```bash
# Forward call graph from app_main
cflow --main=app_main src/main.c lib/star_*/src/*.c > forward_callgraph.txt

# Reverse call graph (show callers)
cflow -r --main=app_main src/main.c lib/star_*/src/*.c > reverse_callgraph.txt
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

### Motor Control Components

The motor control libraries follow a consistent pattern:
- **star_motor**: Low-level MCPWM-based motor control (no bus dependency)
- **star_drv8243**: Integration layer using bus manager for ADC/GPIO
- **star_encoder**: Position feedback using PCNT peripheral (no bus dependency)
- **star_pid**: Stateless PID algorithm (no dependencies)

Example motor driver structure:
```c
// Initialize DRV8243 motor driver
star_drv8243_handle_t motor_driver;
star_drv8243_config_t config = {
    .bus_manager = &bus_manager,
    .gpio_bus_name = "gpio_bus",
    .adc_bus_name = "adc_bus",
    .pin_pwm_ph = GPIO_NUM_25,
    .pin_pwm_en = GPIO_NUM_26,
    .pin_ipropi = ADC_CHANNEL_6,
    .pin_nfault = GPIO_NUM_35,
    .pwm_freq_hz = 20000,
    .current_limit_ma = 2000,
};
star_drv8243_init(&motor_driver, &config);

// Control motor
star_drv8243_set_speed(&motor_driver, 50.0f);
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
├── star_core/              # Abstract interfaces (DIP foundation)
├── star_error_handler/     # Error handling with retry logic
├── star_pin_validator/     # GPIO conflict detection
├── star_bus/               # Unified bus abstraction (I2C, SPI, UART, GPIO, ADC, OneWire)
├── star_motor/             # Brushed DC motor control (MCPWM-based)
├── star_drv8243/           # DRV8243 H-bridge motor driver integration
├── star_encoder/           # Quadrature encoder driver (PCNT-based)
├── star_pid/               # PID controller for closed-loop control
├── star_sensor_ds18b20/    # DS18B20 temperature sensor (1-Wire)
└── star_bms_bq7850/        # BQ7850 battery management system

src/
└── main.c                  # Application entry point

partitions/
├── partitions_4mb.csv      # ESP32-WROOM partition table
└── partitions_16mb.csv     # ESP32-S3 partition table
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
- Default SPI: COPI=23, CIPO=19, CLK=18, CS=5
- Default UART: TX=17, RX=16

### ESP32-S3 (16MB Flash + 8MB PSRAM)
- Default I2C: SDA=GPIO8, SCL=GPIO9
- Default SPI: COPI=11, CIPO=13, CLK=12, CS=10
- Default UART: TX=43, RX=44

**Note:** These are configurable via build flags in `platformio.ini`

## Common Patterns

### Initialize Bus Manager for Motor Control
```c
// 1. Create error handler (optional)
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

// 4. Create buses for motor control
star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
star_bus_config_t* adc_bus = star_bus_config_create_adc(
    "adc_bus", ADC_UNIT_1, ADC_CHANNEL_6, ADC_BITWIDTH_12, ADC_ATTEN_DB_12);

star_bus_manager_add_bus(&bus_manager, gpio_bus);
star_bus_manager_add_bus(&bus_manager, adc_bus);
```

### Initialize Motor Driver with Closed-Loop Control
```c
// 1. Initialize motor driver
star_drv8243_handle_t motor;
star_drv8243_config_t motor_cfg = {
    .bus_manager = &bus_manager,
    .gpio_bus_name = "gpio_bus",
    .adc_bus_name = "adc_bus",
    .pin_pwm_ph = GPIO_NUM_25,
    .pin_pwm_en = GPIO_NUM_26,
    .pin_ipropi = ADC_CHANNEL_6,
    .pin_nfault = GPIO_NUM_35,
    .pwm_freq_hz = 20000,
    .current_limit_ma = 2000,
};
star_drv8243_init(&motor, &motor_cfg);

// 2. Initialize encoder
star_encoder_handle_t encoder;
star_encoder_config_t enc_cfg = {
    .pin_a = GPIO_NUM_32,
    .pin_b = GPIO_NUM_33,
    .filter_value = 1000,
    .high_limit = 10000,
    .low_limit = -10000,
};
star_encoder_init(&encoder, &enc_cfg);

// 3. Initialize PID
star_pid_handle_t pid;
star_pid_config_t pid_cfg = {
    .kp = 1.0f, .ki = 0.5f, .kd = 0.1f,
    .output_min = -100.0f, .output_max = 100.0f,
};
star_pid_init(&pid, &pid_cfg);

// 4. Control loop
float setpoint = 100.0f;  // Target RPM
float velocity_rpm;
star_encoder_get_velocity_rpm(&encoder, 10.0f, 500, &velocity_rpm);

float output;
star_pid_compute(&pid, setpoint, velocity_rpm, 0.01f, &output);
star_drv8243_set_speed(&motor, output);
```

## Adding New Components

### Adding a Motor Control Component

1. Create library structure:
   ```
   lib/star_motor_component/
   ├── include/star_motor_component.h
   ├── src/star_motor_component.c
   ├── library.json
   └── CMakeLists.txt
   ```

2. Follow existing patterns:
   - **Low-level hardware**: No bus dependency (see `star_motor`, `star_encoder`, `star_pid`)
   - **Integration layer**: Use bus manager for ADC/GPIO (see `star_drv8243`)

3. For integration components, add dependency injection through bus manager:
   ```c
   typedef struct {
       star_bus_manager_t* bus_manager;
       const char* gpio_bus_name;
       const char* adc_bus_name;
       // Component-specific config
   } motor_component_config_t;
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

## Complete Motor Control System Example

Here's a complete example combining all motor control components:

```c
#include "star_drv8243.h"
#include "star_encoder.h"
#include "star_pid.h"
#include "star_sensor_ds18b20.h"

// === Setup Phase ===

// 1. Initialize bus manager
star_bus_manager_t bus_manager;
star_bus_manager_init(&bus_manager, "main", NULL, NULL);

// 2. Create buses
star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
star_bus_config_t* adc_bus = star_bus_config_create_adc(
    "adc_bus", ADC_UNIT_1, ADC_CHANNEL_6, ADC_BITWIDTH_12, ADC_ATTEN_DB_12);
star_bus_config_t* onewire_bus = star_bus_config_create_onewire(
    "temp_bus", GPIO_NUM_4, false);

star_bus_manager_add_bus(&bus_manager, gpio_bus);
star_bus_manager_add_bus(&bus_manager, adc_bus);
star_bus_manager_add_bus(&bus_manager, onewire_bus);

// 3. Initialize motor driver
star_drv8243_handle_t motor;
star_drv8243_config_t motor_cfg = {
    .bus_manager = &bus_manager,
    .gpio_bus_name = "gpio_bus",
    .adc_bus_name = "adc_bus",
    .pin_pwm_ph = GPIO_NUM_25,
    .pin_pwm_en = GPIO_NUM_26,
    .pin_ipropi = ADC_CHANNEL_6,
    .pin_nfault = GPIO_NUM_35,
    .pwm_freq_hz = 20000,
    .current_limit_ma = 2000,
};
star_drv8243_init(&motor, &motor_cfg);

// 4. Initialize encoder
star_encoder_handle_t encoder;
star_encoder_config_t enc_cfg = {
    .pin_a = GPIO_NUM_32,
    .pin_b = GPIO_NUM_33,
    .filter_value = 1000,
    .high_limit = 10000,
    .low_limit = -10000,
};
star_encoder_init(&encoder, &enc_cfg);

// 5. Initialize PID controller
star_pid_handle_t pid;
star_pid_config_t pid_cfg = {
    .kp = 1.0f,
    .ki = 0.5f,
    .kd = 0.1f,
    .output_min = -100.0f,
    .output_max = 100.0f,
    .integral_min = -50.0f,
    .integral_max = 50.0f,
};
star_pid_init(&pid, &pid_cfg);

// 6. Initialize temperature sensor
star_ds18b20_handle_t temp_sensor;
star_ds18b20_config_t temp_cfg = {
    .bus_manager = &bus_manager,
    .bus_name = "temp_bus",
    .resolution = k_star_ds18b20_resolution_12_bit,
    .use_rom = false,
};
star_sensor_ds18b20_init(&temp_sensor, &temp_cfg);

// === Control Loop (1000Hz) ===

float setpoint_rpm = 100.0f;
float dt = 0.001f;  // 1ms

while (1) {
    // Read encoder position
    float velocity_rpm;
    star_encoder_get_velocity_rpm(&encoder, dt * 1000, 500, &velocity_rpm);

    // Compute PID output
    float pid_output;
    star_pid_compute(&pid, setpoint_rpm, velocity_rpm, dt, &pid_output);

    // Apply to motor
    star_drv8243_set_speed(&motor, pid_output);

    // Monitor current and temperature
    float current_ma, temp_c;
    star_drv8243_read_current(&motor, &current_ma);
    star_sensor_ds18b20_read_temp(&temp_sensor, &temp_c);

    // Check for faults
    bool fault;
    star_drv8243_get_fault_status(&motor, &fault);
    if (fault || temp_c > 85.0f) {
        star_drv8243_stop(&motor, true);  // Emergency brake
    }

    vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay
}
```

## Available Libraries

### Core Infrastructure
- **star_core** - Abstract interfaces (error_interface, pin_interface)
- **star_error_handler** - Error handling with retry logic
- **star_pin_validator** - GPIO conflict detection
- **star_bus** - Unified bus abstraction (I2C, SPI, UART, GPIO, ADC, OneWire)

### Motor Control Components
- **star_motor** - Brushed DC motor control using ESP32 MCPWM peripheral
- **star_drv8243** - DRV8243 H-bridge motor driver with current sensing and fault detection
- **star_encoder** - Quadrature encoder driver using ESP32 PCNT peripheral
- **star_pid** - Generic PID controller with anti-windup for closed-loop control

### Sensor Drivers
- **star_sensor_ds18b20** - DS18B20 digital temperature sensor (1-Wire)

### Power Management
- **star_bms_bq7850** - BQ7850 battery management system

## Motor Control Usage

### Basic Motor Control with star_motor

The `star_motor` library provides MCPWM-based H-bridge control:

```c
#include "star_motor.h"

// Initialize motor controller
star_motor_handle_t motor;
star_motor_config_t config = {
    .group_id = 0,                    // MCPWM group 0
    .timer_resolution_hz = 10000000,  // 10MHz timer
    .pwm_freq_hz = 20000,             // 20kHz PWM
    .pin_pwm_a = GPIO_NUM_25,         // H-bridge IN1
    .pin_pwm_b = GPIO_NUM_26,         // H-bridge IN2
    .dead_time_ns = 1000,             // 1us dead-time
    .fault_pin = -1,                  // No fault pin
};
star_motor_init(&motor, &config);

// Control motor
star_motor_set_duty(&motor, 50.0f);   // 50% forward
star_motor_set_duty(&motor, -75.0f);  // 75% reverse
star_motor_stop(&motor, true);        // Brake

// Cleanup
star_motor_deinit(&motor);
```

### DRV8243 Motor Driver Integration

The `star_drv8243` library integrates motor control with current sensing and fault detection:

```c
#include "star_drv8243.h"

// Create GPIO and ADC buses
star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
star_bus_config_t* adc_bus = star_bus_config_create_adc(
    "adc_bus", ADC_UNIT_1, ADC_CHANNEL_6, ADC_BITWIDTH_12, ADC_ATTEN_DB_12);
star_bus_manager_add_bus(&bus_manager, gpio_bus);
star_bus_manager_add_bus(&bus_manager, adc_bus);

// Initialize DRV8243 driver
star_drv8243_handle_t motor_driver;
star_drv8243_config_t config = {
    .bus_manager = &bus_manager,
    .gpio_bus_name = "gpio_bus",
    .adc_bus_name = "adc_bus",
    .pin_pwm_ph = GPIO_NUM_25,        // Phase control
    .pin_pwm_en = GPIO_NUM_26,        // Enable/Speed control
    .pin_ipropi = ADC_CHANNEL_6,      // Current sense (GPIO34)
    .pin_nfault = GPIO_NUM_35,        // Fault detect
    .pwm_freq_hz = 20000,             // 20 kHz PWM
    .current_limit_ma = 2000,         // 2A limit
    .ki_propi = 525,                  // IPROPI ratio
};
star_drv8243_init(&motor_driver, &config);

// Control and monitor
star_drv8243_set_speed(&motor_driver, 50.0f);

float current_ma;
star_drv8243_read_current(&motor_driver, &current_ma);

bool fault;
star_drv8243_get_fault_status(&motor_driver, &fault);

// Cleanup
star_drv8243_deinit(&motor_driver);
```

### Encoder Position Tracking

The `star_encoder` library provides quadrature encoder support:

```c
#include "star_encoder.h"

// Initialize encoder
star_encoder_handle_t encoder;
star_encoder_config_t config = {
    .pin_a = GPIO_NUM_25,
    .pin_b = GPIO_NUM_26,
    .filter_value = 1000,     // Glitch filter
    .high_limit = 10000,
    .low_limit = -10000,
};
star_encoder_init(&encoder, &config);

// Read position
int32_t position;
star_encoder_get_count(&encoder, &position);

// Calculate velocity
float velocity_rpm;
star_encoder_get_velocity_rpm(&encoder, 10.0f, 500, &velocity_rpm);

// Reset and cleanup
star_encoder_reset_count(&encoder);
star_encoder_deinit(&encoder);
```

### PID Controller for Closed-Loop Control

The `star_pid` library provides a complete PID implementation:

```c
#include "star_pid.h"

// Initialize PID controller
star_pid_handle_t pid;
star_pid_config_t config = {
    .kp = 1.0f,            // Proportional gain
    .ki = 0.5f,            // Integral gain
    .kd = 0.1f,            // Derivative gain
    .output_min = -100.0f, // Min output (-100% duty)
    .output_max = 100.0f,  // Max output (+100% duty)
    .integral_min = -50.0f,
    .integral_max = 50.0f,
};
star_pid_init(&pid, &config);

// PID control loop (typically 1000Hz)
float setpoint = 100.0f;  // Target RPM
float measured = 95.0f;   // Actual RPM from encoder
float dt = 0.001f;        // 1ms = 1000Hz

float output;
star_pid_compute(&pid, setpoint, measured, dt, &output);

// Apply output to motor
star_motor_set_duty(&motor, output);

// Runtime tuning
star_pid_set_gains(&pid, 1.5f, 0.6f, 0.15f);
star_pid_reset(&pid);

// Cleanup
star_pid_deinit(&pid);
```

### DS18B20 Temperature Monitoring

The `star_sensor_ds18b20` library provides temperature sensing (useful for motor/driver thermal monitoring):

```c
#include "star_sensor_ds18b20.h"

// Create 1-Wire bus
star_bus_config_t* onewire_bus = star_bus_config_create_onewire(
    "temp_onewire", GPIO_NUM_4, false);
star_bus_manager_add_bus(&bus_manager, onewire_bus);

// Initialize DS18B20
star_ds18b20_handle_t temp_sensor;
star_ds18b20_config_t config = {
    .bus_manager = &bus_manager,
    .bus_name = "temp_onewire",
    .resolution = k_star_ds18b20_resolution_12_bit,
    .use_rom = false,  // Single sensor
};
star_sensor_ds18b20_init(&temp_sensor, &config);

// Read temperature
float temperature_c;
star_sensor_ds18b20_read_temp(&temp_sensor, &temperature_c);

// Change resolution
star_sensor_ds18b20_set_resolution(&temp_sensor,
    k_star_ds18b20_resolution_10_bit);

// Cleanup
star_sensor_ds18b20_deinit(&temp_sensor);
```

