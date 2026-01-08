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

## ROS2 C++ Style Guide

**IMPORTANT:** ROS2 C++ code follows different conventions than C firmware. This section supplements the C style guide above.

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
// camelCase for methods (ROS2 convention)
void publishTelemetry(const TelemetryData & data);
bool isConnected() const;
void onBatteryStateReceived(const sensor_msgs::msg::BatteryState::SharedPtr msg);

// Use verb-based names that clarify actions
void checkForErrors();      // ✓ Clear intent
void errorCheck();          // ✗ Noun-first is confusing
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
  void telemetryCallback();  // Timer callback

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
      std::bind(&MyNode::cmdVelCallback, this, std::placeholders::_1)
    );
  }

private:
  void cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
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
  std::bind(&MyNode::timerCallback, this)
);
```

### Error Handling

**ROS2 uses exceptions** (different from C firmware):

```cpp
// Throw exceptions for errors
void publishTelemetry()
{
  if (!grpc_channel_) {
    throw std::runtime_error("gRPC channel not initialized");
  }

  // ...
}

// Catch exceptions in callbacks (avoid crashing node)
void timerCallback()
{
  try {
    publishTelemetry();
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

**Doxygen comments** (use /// for C++):

```cpp
/// @brief Brief description of class
///
/// Detailed description can span multiple lines. Explain purpose,
/// usage, and any important details.
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  /// @brief Constructor for gateway bridge node
  ///
  /// @param options Node options for configuration
  explicit StarGatewayBridgeNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  /// @brief Callback for telemetry publishing timer
  ///
  /// Forwards latest telemetry to Gateway via gRPC. Called at 10 Hz.
  void telemetryTimerCallback();

  rclcpp::TimerBase::SharedPtr telemetry_timer_;  ///< Timer for telemetry publishing
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
| **Methods** | snake_case | camelCase |
| **Variables** | snake_case | under_scored |
| **Member vars** | `s_` prefix | trailing `_` |
| **Line limit** | 100 chars | 120 chars |
| **Namespaces** | Not used | star::package_name:: |
| **Error handling** | Return codes | Exceptions |
| **Logging** | uart_puts() | RCLCPP_INFO/WARN/ERROR |
| **Include guards** | `STAR_RX72N_FILE_H` | `PACKAGE_FILE_HPP_` |

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
