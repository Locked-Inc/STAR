# GitHub Copilot Agent Configuration - STAR Project

## Quick Reference

**STAR (Spatial Topography Accessibility Robot)** is a distributed robotics platform for autonomous indoor ADA-compliance auditing, with custom PCB hardware, Renesas RX72N motor control firmware (ThreadX RTOS), a Raspberry Pi 5 control system (ROS2 Jazzy), and Protocol Buffers communication over SPI.

**System Architecture:**
```
Operator -> Grafana cockpit (browser, panel-29 + Pi5 cockpit-API)
         -> Gateway (Go on RPi5, gRPC/HTTP <-> ROS2 bridge)
         -> ROS2 (C++ on RPi5, robot control + SLAM)
         -> SPI Bridge (ROS2 node, 10 Mbps SPI)
         -> RX72N Firmware (C + ThreadX, real-time motor control + nanopb)
```

---

## Essential Commands

### Protocol Buffers (star-proto/)

```bash
# Lint and format (run from star-proto/)
cd star-proto
buf lint proto/
buf format --diff proto/

# Generate code for all targets (run from workspace root)
buf generate star-proto/proto

# Alternative: Use Makefile (preferred)
make proto-gen
make proto-gen-firmware    # Copy nanopb to firmware
make proto-gen-go          # Go module setup

# Run Go tests
cd star-proto/tests/go && go test ./...
```

### Gateway Service (star-gateway/)

```bash
# Build
cd star-gateway
go build ./cmd/star-gateway

# Test
go test ./...
go test -v ./...             # Verbose output
go test -cover ./...         # With coverage
go test -run TestFuncName    # Single test

# Lint and format
golangci-lint run
go fmt ./...
go vet ./...
```

### ROS2 (star-ros2/)

```bash
# Build
cd star-ros2
colcon build --symlink-install
colcon build --packages-select star_spi_bridge  # Specific package

# Source workspace
source install/setup.bash

# Test
colcon test
colcon test --packages-select star_spi_bridge
colcon test-result --verbose

# Format and review
./scripts/ros2/format-ros2.sh
./scripts/ros2/format-ros2.sh --check    # CI mode
./scripts/ros2/review-ros2.sh            # Automated code review
```

### Firmware (star-rx72n-firmware/)

```bash
# Build in Docker
./build.sh

# Clean
./clean.sh

# Flash (requires E2 Lite/J-Link)
./flash.sh

# Debug (requires J-Link)
./debug.sh

# Format code
./scripts/format_code.sh

# Generate Doxygen docs
./scripts/compile_doxygen.sh

# Run unit tests
cd tests
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure
```

### Code Quality Tools

```bash
# CodeRabbit - AI-powered code review
coderabbit review --plain              # Human-readable analysis
coderabbit review --prompt-only        # Token-efficient mode for AI agents
coderabbit review --plain path/to/file.go  # Specific files
cr review --plain                      # Shorthand alias

# When to use:
# - Before commits to catch issues early
# - After implementing features for improvement suggestions
# - During refactoring to ensure code quality
# - Use --prompt-only when working with AI agents to save tokens

# Makefile automation
make proto-gen              # Generate Protocol Buffers (all targets)
make test-rx72n            # RX72N unit tests + proto regen
make test-gateway          # Gateway service tests
make clean                 # Clean all build artifacts
```

### Doxygen Documentation

```bash
# Generate RX72N firmware docs
cd star-rx72n-firmware
doxygen Doxyfile
# Output: docs/doxygen/html/index.html

# Generate ROS2 docs
cd star-ros2
doxygen Doxyfile
# Output: docs/doxygen/html/index.html

# Check for warnings (CI/CD fails on warnings)
grep -i "warning" doxygen_warnings.log
```

---

## Project Architecture

### Components

| Component | Description | Language/Framework |
|-----------|-------------|--------------------|
| `star-rx72n-firmware/` | Renesas RX72N motor controller firmware | C + ThreadX RTOS |
| `star-proto/` | Protocol Buffers schemas with code generation | Proto3 -> Go/nanopb/C++ |
| `star-gateway/` | Gateway service (Grafana cockpit <-> ROS2 bridge) | Go + gRPC |
| `star-ros2/` | ROS2 integration + SLAM | C++ (ROS2 Jazzy) |
| `matlab/` | Motor system identification + PID design | MATLAB |
| `schematic/` | PCB designs | KiCad |

### Hardware

- **Main Controller:** Raspberry Pi 5
- **Motor Controller:** Renesas RX72N (4MB Flash, 512KB SRAM, 240 MHz)
- **Motors:** 4x 6V brushed DC gearmotors (210 RPM, 341 PPR Hall encoders)
- **Motor Drivers:** DRV8263H H-bridge with current sensing and SPI fault diagnostics
- **Lidar:** RPLiDAR C1 (12m range, IP54)
- **Communication:** 10 Mbps SPI (RPi5 <-> RX72N) with nanopb + CRC-32

### Gateway Architecture (Recent Refactoring)

**Commits 39cae45ff, a803d667e added new internal packages:**

- **`internal/server/`** - gRPC/HTTP server implementation
  - Separates server concerns from gateway logic
  - Files: `config.go`, `grpc.go`, `http.go` + tests

- **`internal/manager/`** - Connection lifecycle management
  - Health monitoring, hotplug detection, heartbeat logic
  - 18 files managing device connections
  - Recent updates: `manager.go` (Feb 11), `config.go` (Feb 11)

- **`internal/app/`** - Application orchestration
  - Coordinates server and manager components

---

## Code Style Standards

### General Project Rules

- **Terminology**: Use inclusive terms - Controller/Peripheral (not master/slave), COPI/CIPO (not MOSI/MISO), CS/Chip Select (not SS/Slave Select), Primary/Main (not master)
- **No Backward Compatibility**: No deprecation macros, shims, or version checks - update all call sites directly (in-house project, zero backward compatibility requirements)
- **No Magic Numbers**: ALL numeric literals must be named constants (typed enums)
- **Error Handling**: Check ALL return values, propagate errors, never ignore failures
- **Documentation**: Use Doxygen format with ALL applicable tags for all functions and types

### C Firmware Style (star-rx72n-firmware/)

#### Naming Conventions

- Functions/variables: `snake_case`
- Macros/constants: `SCREAMING_SNAKE_CASE`
- Types: `snake_case_t`
- Static functions: `internal_` prefix
- Private functions: `priv_` prefix
- Static variables: `s_` prefix
- Global variables: `g_` prefix (avoid)

#### Constants and Macros Hierarchy

**STRICT PREFERENCE HIERARCHY:**

1. **C23 Typed Enums** - MANDATORY for ALL integer constants

   ```c
   // [PASS] CORRECT: C23 typed enums with explicit underlying type (MANDATORY)
   typedef enum : uint8_t {
       k_motor_state_idle    = 0,
       k_motor_state_running = 1,
       k_motor_state_error   = 2,
   } motor_state_t;

   typedef enum : uint16_t {
       k_timeout_ms  = 1000,    // Integer constant -> enum
       k_max_retries = 3,       // Integer constant -> enum
   } motor_config_t;

   // [FAIL] WRONG: Untyped enum (missing `: uint8_t` type specifier)
   typedef enum {
       k_motor_state_idle = 0,  // Missing underlying type!
   } motor_state_t;

   // [FAIL] WRONG: Never use macros for integer constants
   #define TIMEOUT_MS (1000)  // Should be enum!
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
   // [PASS] CORRECT: Floating-point must use const
   static const float s_max_velocity_mps = 2.5F;
   static const float s_pid_kp = 1.0F;

   // [FAIL] WRONG: Never use macros for floats
   #define MAX_VELOCITY_MPS (2.5F)  // Should be const!
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

   // [PASS] ALLOWED: Conditional compilation
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

#### No Magic Numbers (ZERO TOLERANCE)

**ALL numeric literals must be named typed enums, including:**

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
```

**Why this matters:**
- Self-documenting code (k_idx_high_byte vs 0)
- Searchable (grep for "high_byte" finds all uses)
- Maintainable (change offset in one place)
- Debugger-friendly (see names, not numbers)
- Compile-time checked (typos caught)
- **Typed enums guarantee size** (uint8_t is always 1 byte)
- **ABI stability** (enum size won't change between compiler versions)
- **Predictable memory layout** (critical for embedded systems)

#### Hardware Register Access

```c
// [PASS] CORRECT: Inline accessor functions with typed enum addresses
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

#### ThreadX Patterns

```c
// Task creation
TX_THREAD led_thread;
static uint8_t led_task_stack[1024];

static void led_task_entry(ULONG input) {
    while (1) {
        // Task logic
        tx_thread_sleep(50);  // 50 ticks
    }
}

// [PASS] CORRECT: ThreadX uses UINT return type
UINT led_task_create(void) {
    return tx_thread_create(&led_thread, "LED", led_task_entry,
                           0, led_task_stack, sizeof(led_task_stack),
                           PRIORITY, PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
}

// [FAIL] WRONG: Never use esp_err_t (that's ESP-IDF for ESP32!)
// esp_err_t led_task_create(void) { ... }  // WRONG!
```

#### Memory Management

- **Zero dynamic allocation** (safety-critical)
- All buffers statically allocated with enum-defined sizes
- ThreadX stacks are static arrays
- Example: `char items[k_max_items][k_max_desc_len]`

### C++ ROS2 Style (star-ros2/)

#### Naming Conventions

**Classes and Types:**

```cpp
// CamelCase for classes (ROS2 convention)
class StarGatewayBridgeNode : public rclcpp::Node {
  // ...
};

// CamelCase for structs used as types
struct TelemetryData {
  double encoder_ticks_;
  double current_ma_;
};

// Type aliases use CamelCase
using TelemetryPtr = std::shared_ptr<TelemetryData>;
```

**Methods and Variables:**

```cpp
// snake_case for methods (same as C firmware)
void publish_telemetry(const TelemetryData & data);
bool is_connected() const;

// snake_case for variables
int loop_counter = 0;
std::string node_name = "gateway_bridge";

// Member variables with trailing underscore
class MyNode : public rclcpp::Node {
private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr telemetry_pub_;
  std::shared_ptr<grpc::Channel> grpc_channel_;
  bool is_connected_;
};

// Constants: ALL_CAPITALS or static constexpr
const int DEFAULT_QOS_DEPTH = 10;
const double MAX_LINEAR_VELOCITY_MPS = 1.0;

// Static const for class-specific
class MyNode : public rclcpp::Node {
private:
  static constexpr int kMaxRetries = 3;
  static constexpr double kTimeoutS = 5.0;
};
```

**Namespaces:**

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

#### File Organization

- **Headers:** Use `.hpp` extension (NOT `.h`)
- **Include guards:** `PACKAGE_FILE_NAME_HPP_` format
- **Line limit:** 120 characters (C firmware is 100)

**Standard include order:**
1. License and copyright
2. Include guard
3. ROS2 core includes (`<rclcpp/rclcpp.hpp>`)
4. ROS2 message includes (`<geometry_msgs/msg/twist.hpp>`)
5. Project includes (`"star/v1/motor_control.pb.h"`)
6. System C++ includes (`<memory>`, `<string>`)
7. System C includes (`<cstdint>`)
8. Namespace declaration

#### Error Handling

```cpp
// ROS2 uses exceptions (different from C firmware)
void publish_telemetry() {
  if (!grpc_channel_) {
    throw std::runtime_error("gRPC channel not initialized");
  }
  // ...
}

// Catch exceptions in callbacks (avoid crashing node)
void timer_callback() {
  try {
    publish_telemetry();
  } catch (const std::exception & e) {
    RCLCPP_ERROR(this->get_logger(), "Failed: %s", e.what());
  }
}
```

#### Logging

```cpp
// ROS2 logging macros (REQUIRED - never use printf/cout)
RCLCPP_INFO(this->get_logger(), "Node started");
RCLCPP_WARN(this->get_logger(), "Connection lost");
RCLCPP_ERROR(this->get_logger(), "Failed: %s", error_msg.c_str());
RCLCPP_DEBUG(this->get_logger(), "Processing message %d", count);

// Throttled logging (max once per 5 seconds)
RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
  "Stale telemetry (%ldms > %dms)", cmd_age_ms, timeout_ms_);

// [FAIL] NEVER use printf/cout in ROS2 nodes
// printf("Debug message");               // WRONG!
// std::cout << "Debug" << std::endl;     // WRONG!
```

#### Node Patterns

```cpp
// Inherit from rclcpp::Node for basic nodes
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  StarGatewayBridgeNode();
private:
  void telemetry_callback();
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
};
```

### Go Gateway Style (star-gateway/)

Follows standard Go conventions with project-specific patterns:

- Standard Go naming (PascalCase for exported, camelCase for unexported)
- Interfaces for dependency injection and testing
- Structured logging with context
- Error handling with wrapped errors (`fmt.Errorf("failed to connect: %w", err)`)

**See `star-gateway/CLAUDE.md` for detailed Go style guide.**

### Protocol Buffers Style (star-proto/)

**Boston Dynamics-based style guide:**

- **Proto3 only**, 100 char line limit, 4-space indent
- **Naming:** Messages `PascalCase`, fields `snake_case`, enums `SCREAMING_SNAKE`
- **Enum zero value:** Must end with `_UNKNOWN`
- **Units:** MKS system with suffixes (`_mps`, `_rad`, `_ma`, `_celsius`)
- **Headers:** Include `RequestHeader`/`ResponseHeader` in all RPC messages

**Code Generation Targets:**

| Target | Plugin | Output |
|--------|--------|--------|
| Go | buf.build/protocolbuffers/go, buf.build/grpc/go | `gen/go/` |
| C (RX72N) | nanopb_generator | `gen/nanopb/` |
| C++ (ROS2 bridge) | buf.build/protocolbuffers/cpp, buf.build/grpc/cpp | `gen/cpp/` |

**nanopb Considerations:**

Configure field sizes in `.options` files for RX72N (no dynamic allocation):
```
star.v1.RequestHeader.request_id max_size:64
```

---

## Documentation Requirements

### Doxygen Policy

**CRITICAL:** ALL code in the STAR project MUST be documented with comprehensive Doxygen comments using ALL applicable tags.

**Documentation Coverage Requirements:**

1. **Every file** must have complete file-level documentation
2. **Every function** must use ALL applicable Doxygen tags
3. **Every struct/enum** must document ALL members
4. **Every variable** (global, static, member) must be documented
5. **Every typedef** must have full documentation
6. **Every macro** must be documented with usage examples

**Rule:** If a Doxygen tag is applicable to a code element, it MUST be used. Do not omit tags.

#### Minimum Required Tags for Functions

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

**Additional tags when applicable:** `@par`, `@warning`, `@attention`, `@invariant`, `@todo`, `@bug`, `@deprecated`, `@test`, `@startuml/@dot/@msc`, `@callgraph/@callergraph`

#### Example: Complete Function Documentation

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
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 4 preconditions, 3 postconditions
 */
rx_err_t rx_pid_compute(rx_pid_handle_t* pid, float setpoint,
                        float measured, float dt_sec, float* output);
```

#### CI/CD Enforcement

- Doxygen warnings will cause build failures
- All public APIs must be fully documented
- Run `doxygen Doxyfile 2>&1 | grep warning` to check for warnings

---

## NASA Power of 10 Rules (Firmware Compliance)

The STAR project follows NASA/JPL Power of 10 rules for safety-critical embedded code with one intentional deviation for testability.

1. **Simplify Control Flow** [PASS] - No `goto`, `setjmp`/`longjmp`, or recursion
2. **Fixed Loop Upper-Bounds** [PASS] - All loops have statically provable bounds
3. **No Dynamic Memory** [PASS] - Zero malloc/free in RX72N firmware (safety-critical)
4. **Short Functions** [PASS] - ~60 lines max, single verifiable units
5. **Assertions/Validation** [PASS] - 2+ checks per function, pre/post conditions
6. **Smallest Scope** [PASS] - Variables declared close to first use
7. **Check Return Values** [PASS] - All returns validated or explicitly cast `(void)`
8. **Limit Preprocessor** [PASS] - C23 typed enums for ALL constants, macros only for 3 cases
9. **Restrict Pointers** [WARN] - **INTENTIONAL DEVIATION** for Dependency Inversion Principle (DIP)
   - **Standard:** Maximum one level of dereferencing, no function pointers
   - **STAR Deviation:** Function pointers ALLOWED for hardware abstraction and unit testing
   - **Why:** Enables mock implementations for testability
   - Example: `typedef struct { rx_err_t (*read)(void* ctx, ...); void* ctx; } bus_interface_t;`
10. **Maximum Warnings** [PASS] - `-Wall -Wextra -Werror`, zero warnings, CI/CD enforced

---

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

---

## Simulator Support (e^2 Studio)

### Purpose

The Renesas e^2 studio simulator allows testing firmware **logic** without real hardware:

- [PASS] Algorithm validation (PID controllers, state machines)
- [PASS] Protocol parsing and encoding
- [PASS] Error handling paths
- [PASS] Interactive debugging (breakpoints, step-through, variable inspection)
- [FAIL] Timing behavior (not cycle-accurate)
- [FAIL] Hardware peripheral specifics (USB, SPI, clocks fully functional)

**WARNING**: Simulator builds are FOR LOGIC TESTING ONLY. Always validate critical paths on real hardware before deployment.

### Building for Simulator

#### Option 1: e^2 studio (Interactive Debugging)

**Creating Simulator Build Configuration:**
1. In e^2 studio, right-click project "star-rx72n-firmware" -> Properties
2. Navigate to: C/C++ Build -> Manage Configurations
3. Click "New..." button
4. Name: **"Simulator Debug"**
5. Select "Copy settings from": **"Debug"**
6. Click OK

**Adding RX_SIMULATOR_MODE Define:**
1. Still in Properties, select configuration: **"Simulator Debug"** (top dropdown)
2. Navigate to: C/C++ Build -> Settings
3. Expand: **"Compiler"** -> click **"Preprocessor"**
4. In "Defined symbols (-D)" section, click "Add" (green + icon)
5. Enter: **`RX_SIMULATOR_MODE`** (no value needed)
6. Click OK -> Apply -> Close

**Building:**
1. Project -> Build Configurations -> Set Active -> **"Simulator Debug"**
2. Project -> Build Project (Ctrl+B)
3. Verify build succeeds with warning: "RX_SIMULATOR_MODE: This build is FOR SIMULATOR ONLY"

**Launching Simulator:**
1. Run -> Debug As -> Renesas GDB Hardware Debugging
2. Ensure "Simulator" is selected as target device (not hardware emulator)
3. Set breakpoints, step through code, inspect variables
4. Logs appear in Console view (Window -> Show View -> Console)

#### Option 2: CMake (Automated Testing)

```bash
cd star-rx72n-firmware/tests
cmake .. -DCMAKE_BUILD_TYPE=Debug  # RX_SIMULATOR_MODE auto-enabled
make -j$(nproc)
ctest --output-on-failure
```

### What Works vs. What Doesn't

**Works in Simulator:**
- Control flow (branching, loops, function calls)
- Algorithms (PID, filtering, state machines)
- Protocol logic (parsing, encoding, CRC validation)
- Error paths (timeout handling, validation failures)
- Data structures (struct manipulation, array operations)
- Logging (output to console via stdout)

**Doesn't Work in Simulator:**
- **Clock/Oscillator:** External 24 MHz crystal, PLL/PPLL lock timing
- **USB:** Enumeration, bulk transfers (limited or no support)
- **SPI:** External device communication (no physical devices)
- **UART:** Serial transmission (redirected to console)
- **Timers:** Precise timing, interrupt latency
- **DMA:** Transfer behavior, timing
- **Cycle-accurate timing:** Not modeled

**Use Hardware For:**
- Clock tree validation (actual 240 MHz operation)
- USB enumeration and bulk transfers
- SPI communication with real devices (DRV8263H fault register readback/diagnostics, sensors)
- UART communication (actual baud rates)
- Interrupt latency verification
- DMA transfer validation
- Real-time performance analysis
- Final integration testing

---

## Testing Standards

### Unit Tests

- **C Firmware**: Unity framework, 100% branch coverage for critical functions
- **C++ ROS2**: gtest/gmock, minimum 80% code coverage
- **Go Gateway**: Standard testing package with table-driven tests

### Integration Tests

- **ROS2**: Hardware-in-the-loop testing where applicable
- **Gateway**: End-to-end SPI communication tests
- **Firmware**: Virtual RX72N simulator testing (logic only)

### Test Execution

```bash
# Firmware unit tests
cd star-rx72n-firmware/tests
cmake .. && make && ctest --output-on-failure

# Gateway tests
cd star-gateway && go test -v -cover ./...

# ROS2 tests
cd star-ros2 && colcon test && colcon test-result --verbose
```

---

## Boundaries

### [PASS] Always Do

- **Run tests before commits:** `go test ./...`, `colcon test`, `ctest`, etc.
- **Format code:** `clang-format`, `go fmt`, `buf format`
- **Use C23 typed enums** for ALL integer constants (`: uint8_t`, `: uint16_t`, etc.)
- **Document functions** with ALL applicable Doxygen tags (`@brief`, `@param`, `@return`, `@retval`, `@pre`, `@post`, etc.)
- **Follow NASA Power of 10 rules** (except Rule 9 deviation for function pointers)
- **Check return values** (or explicitly cast to `(void)`)
- **Use inclusive terminology** (Controller/Peripheral, COPI/CIPO, CS, Primary/Main)
- **Use RCLCPP logging** in ROS2 nodes (`RCLCPP_INFO`, `RCLCPP_ERROR`, etc.)
- **Follow conventional commits:** `feat(spi): add device initialization`

### [WARN] Ask First

- **Database schema changes**
- **Breaking changes to Protocol Buffer schemas** (field numbers, types, deletions)
- **Modifying CI/CD workflows** (`.github/workflows/`)
- **Adding new dependencies** to any package
- **Force-pushing to shared branches**
- **Destructive git operations** (`reset --hard`, `clean -f`, `branch -D`)
- **Skipping pre-commit hooks** (`--no-verify`) without explicit approval

### ? Never Do

- **Commit secrets, API keys, or credentials** to repository
- **Use backward compatibility shims** (no deprecation macros, function aliases, wrappers)
- **Use untyped enums** (missing `: uint8_t` type specifier) - MANDATORY for RX72N firmware
- **Use `#define` for integer constants** (use typed enums instead)
- **Use `esp_err_t` in RX72N code** (firmware uses `rx_err_t`, ThreadX uses `UINT`)
- **Modify `node_modules/`, `vendor/`, or generated code directories**
- **Use magic numbers** (ALL numeric literals must be named constants)
- **Dynamic memory allocation in RX72N firmware** (safety-critical, zero malloc/free)
- **Use `printf`/`cout` in ROS2 nodes** (use `RCLCPP_INFO`/`RCLCPP_ERROR` instead)
- **Add AI attribution to commits or PRs** (no "Co-Authored-By: Claude", write natural messages)

---

## Key Documentation

### LaTeX Source Files (Authoritative)

Always reference `.tex` files in `docs/sections/` for accurate technical information:

- `03_hardware_pinout.tex` - Complete GPIO pin assignments and peripheral connections
- `01_nanopb_protocol.tex` - SPI communication protocol specification
- `02_protobuf_schemas.tex` - Protocol Buffer message definitions
- `04_style_guide.tex` - Protocol Buffer coding standards
- `05_c_style_guide.tex` - C firmware style guide (198KB)
- `06_nasa_power_of_10.tex` - Safety-critical coding rules
- `07_gateway_architecture.tex` - Gateway service design
- `09_usb_cdc_protocol.tex` - USB CDC communication protocol
- `11_ros2_cpp_style_guide.tex` - ROS2 C++ style guide

**Compiled:** `docs/star_documentation.pdf` (1.5MB) - reference `.tex` files for latest changes

### Component-Specific Guides

- `star-rx72n-firmware/CLAUDE.md` - Detailed RX72N firmware guide
- `star-gateway/CLAUDE.md` - Gateway service architecture and build guide
- `CLAUDE.md` (project root) - Comprehensive reference for Claude Code (this file is for GitHub Copilot)

### Motor Control

**PID Tuning Workflow:**
1. Measure motor step response -> estimate time constant (tau = 75ms)
2. Run MATLAB: `motor_model_1st_order.m` -> `pid_design_velocity.m` -> `pid_discretize.m`
3. Update RX72N firmware with new gains (Kp=0.286, Ki=8.01)
4. Test closed-loop at 100 Hz control rate

**Motor Model:** G(s) = 3.665 / (0.075s + 1)

---

## Additional Resources

- **GitHub Issues**: https://github.com/Locked-Inc/STAR/issues
- **ROS2 Jazzy Docs**: https://docs.ros.org/en/jazzy/
- **ThreadX Docs**: https://github.com/eclipse-threadx/rtos-docs
- **Protocol Buffers Guide**: https://protobuf.dev/
- **NASA Power of 10**: http://spinroot.com/gerard/pdf/P10.pdf
