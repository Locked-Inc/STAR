# AGENTS.md - STAR Project Development Guide

This document provides comprehensive guidance for agentic coding tools working on the STAR (Simultaneous Tracking and Robotics) project. It contains build commands, testing procedures, and code style guidelines to ensure consistent, high-quality contributions.

## Project Overview

STAR is a distributed robotics platform with:

- **star-rx72n-firmware/**: Renesas RX72N motor controller (ThreadX RTOS, safety-critical C)
- **star-proto/**: Protocol Buffers schemas with multi-language code generation
- **star-gateway/**: Go gateway service (gRPC bridge to ROS2)
- **star-ros2/**: ROS2 integration (C++ with Jazzy, RTAB-Map SLAM)
- **star-ui/**: TypeScript React UI
- **matlab/**: Motor system identification and PID design

## Build, Lint, and Test Commands

### Protocol Buffers (star-proto/)

```bash
# Lint and format
buf lint proto/
buf format --diff proto/

# Generate code for all targets
buf generate proto/ --template buf.gen.yaml --include-imports

# Run Go tests
cd tests/go && go test ./...
```

### Gateway Service (star-gateway/)

```bash
# Build
cd star-gateway
go build ./cmd/star-gateway

# Test
go test ./...
go test -v ./...          # Verbose output
go test -cover ./...      # With coverage

# Lint and format
golangci-lint run
go fmt ./...
go vet ./...
```

### UI (star-ui/)

```bash
# Development
npm run dev

# Build and lint
npm run build
npm run lint

# Test
npm run test
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
colcon test-result --verbose

# Format and review
./scripts/format-ros2.sh
./scripts/format-ros2.sh --check    # CI mode
./scripts/review-ros2.sh           # Automated code review
./scripts/review-ros2.sh --report review.txt
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

# Generate docs
./scripts/compile_doxygen.sh
```

### Running Single Tests

```bash
# Go (gateway)
go test -run TestSpecificFunction ./internal/transport

# TypeScript (UI)
npm run test -- --testNamePattern="test name"

# ROS2 (single package)
colcon test --packages-select star_spi_bridge

# Protocol Buffers (specific target)
cd tests/go && go test -run TestMotorControl
```

## Code Style Guidelines

### General Project Rules

- **Terminology**: Use inclusive terms - Controller/Peripheral (not master/slave), COPI/CIPO (not MOSI/MISO), Primary/Main (not master)
- **No Backward Compatibility**: No deprecation macros, shims, or version checks - update all call sites directly
- **No Magic Numbers**: ALL numeric literals must be named constants
- **Error Handling**: Check ALL return values, propagate errors, never ignore failures
- **Documentation**: Use Doxygen format (`/** @brief */`) for all functions and types

### Constants and Macros Hierarchy

1. **Enums** - For ALL integer constants (mandatory)

   ```c
   typedef enum {
       k_motor_state_idle    = 0,
       k_motor_state_running = 1,
       k_timeout_ms          = 1000,    // Integer constant → enum
       k_max_retries         = 3        // Integer constant → enum
   } motor_config_t;
   ```

2. **const variables** - ONLY for floating-point

   ```c
   static const float s_max_velocity_mps = 2.5f;
   static const float s_pid_kp = 1.0f;
   ```

3. **Macros** - ONLY for these cases:
   - Code reduction: `RX_RETURN_ON_ERROR(err, tag, msg)`
   - Conditional compilation: `#ifdef __RX__`
   - Build configuration: Hardware register addresses (use inline accessors)

### C Firmware Style (star-rx72n-firmware/)

#### Naming Conventions

- Functions/variables: `snake_case`
- Macros/constants: `SCREAMING_SNAKE_CASE`
- Types: `snake_case_t`
- Static functions: `internal_` prefix
- Private functions: `priv_` prefix
- Static variables: `s_` prefix
- Global variables: `g_` prefix (avoid)

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

esp_err_t led_task_create(void) {
    return tx_thread_create(&led_thread, "LED", led_task_entry,
                           0, led_task_stack, sizeof(led_task_stack),
                           PRIORITY, PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
}
```

#### Memory Management

- **Zero dynamic allocation** (safety-critical)
- All buffers statically allocated with enum-defined sizes
- ThreadX stacks are static arrays
- Example: `char items[k_max_items][k_max_desc_len]`

#### Hardware Access

```c
// Use register structures, never raw pointers
SYSTEM.PRCR = 0xA50F;  // Unlock protection
CMT0.CMCR = 0x0042;    // Configure timer

// Inline accessor functions for registers
static inline CMT_Type* cmt0(void) {
    return (CMT_Type*)k_cmt0_base_addr;
}
```

#### Array Indexing

```c
// Named indices document byte ordering
typedef enum {
    k_be16_byte_high = 0,  // High byte (MSB) at index 0
    k_be16_byte_low  = 1   // Low byte (LSB) at index 1
} be16_byte_idx_t;

buf[k_be16_byte_high] = (val >> 8);
buf[k_be16_byte_low]  = (val & 0xFF);
```

#### Variable Declaration

```c
// All declarations at function start
static rx_err_t wait_for_event(uint32_t timeout_us) {
    uint32_t start   = get_time_us();
    uint32_t elapsed = 0;

    while (true) {
        elapsed = get_time_us() - start;
        if (elapsed >= timeout_us) return k_rx_err_timeout;
    }
}
```

### C++ ROS2 Style (star-ros2/)

#### Naming Conventions

**Classes and Types**:

```cpp
// CamelCase for classes
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  StarGatewayBridgeNode();
};

// CamelCase for structs used as types
struct TelemetryData {
  double battery_voltage_;
  double current_ma_;
};

// Type aliases use CamelCase
using TelemetryPtr = std::shared_ptr<TelemetryData>;
```

**Methods and Variables**:

```cpp
// snake_case for methods
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
```

**Namespaces**:

```cpp
// Package-based namespaces (under_scored)
namespace star {
namespace spi_bridge {

class SpiDriverNode : public rclcpp::LifecycleNode {
public:
  SpiDriverNode();
};

}  // namespace spi_bridge
}  // namespace star
```

#### Error Handling

```cpp
// Use exceptions (different from C firmware)
void publish_telemetry() {
  if (!grpc_channel_) {
    throw std::runtime_error("gRPC channel not initialized");
  }
}

// Catch exceptions in callbacks
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
// ROS2 logging macros (required)
RCLCPP_INFO(this->get_logger(), "Node started");
RCLCPP_WARN(this->get_logger(), "Connection lost");
RCLCPP_ERROR(this->get_logger(), "Failed: %s", error_msg.c_str());
RCLCPP_DEBUG(this->get_logger(), "Processing message %d", count);

// Throttled logging
RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
  "Stale telemetry (%ldms > %dms)", cmd_age_ms, timeout_ms_);
```

#### Node Patterns

```cpp
// Lifecycle nodes for safety-critical components
class StarSpiDriverNode : public rclcpp_lifecycle::LifecycleNode {
public:
  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override;
};
```

#### Publishers/Subscribers

```cpp
class MyNode : public rclcpp::Node {
public:
  MyNode() : Node("my_node") {
    // Publisher
    telemetry_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/telemetry", 10);

    // Subscriber with lambda
    cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 10,
      std::bind(&MyNode::cmd_vel_callback, this, std::placeholders::_1));
  }

private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr telemetry_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
};
```

#### Constants

```cpp
// Prefer enums and const (same philosophy as C)
enum class MotorState {
  kIdle = 0,
  kRunning = 1,
  kError = 2
};

const int DEFAULT_QOS_DEPTH = 10;
const double MAX_LINEAR_VELOCITY_MPS = 1.0;

// Static const for class-specific
class MyNode : public rclcpp::Node {
private:
  static constexpr int kMaxRetries = 3;
  static constexpr double kTimeoutS = 5.0;
};
```

### Go Gateway Style (star-gateway/)

Follows standard Go conventions with project-specific patterns:

- Standard Go naming (PascalCase for exported, camelCase for unexported)
- Interfaces for dependency injection and testing
- Structured logging with context
- Error handling with wrapped errors

### TypeScript UI Style (star-ui/)

Standard TypeScript/React conventions:

- PascalCase for components and types
- camelCase for variables and functions
- Strict TypeScript with no `any` types
- React hooks and functional components preferred
- ESLint with React-specific rules

### Testing Requirements

#### Unit Tests

- **C Firmware**: Unity framework, 100% branch coverage for critical functions
- **C++ ROS2**: gtest/gmock, minimum 80% code coverage
- **Go Gateway**: Standard testing package with table-driven tests
- **TypeScript UI**: Vitest with React Testing Library

#### Integration Tests

- **ROS2**: Hardware-in-the-loop testing where applicable
- **Gateway**: End-to-end SPI communication tests
- **Firmware**: Virtual RX72N simulator testing

### Code Quality Tools

#### Automated Review

```bash
# Use CodeRabbit for AI-powered code review
coderabbit review --plain
coderabbit review --prompt-only  # Token-efficient mode
coderabbit review --plain path/to/file.go
```

#### Formatting Enforcement

- **C**: clang-format with `.clang-format` config
- **C++**: clang-format with 120 char line limit
- **Go**: `go fmt`
- **TypeScript**: ESLint + Prettier

### Commit Messages

Follow conventional commits:

```
feat(spi): implement SPI device initialization with ioctl

Add SPI configuration using ioctl() system calls:
- Set SPI mode (CPOL=0, CPHA=0)
- Configure 10MHz clock speed
- Set 8-bit word size
- Test with loopback mode

Closes #137
```

### Security Best Practices

- Never commit secrets or keys to repository
- Use environment variables for sensitive configuration
- Validate all inputs, especially from network sources
- Follow principle of least privilege
- No logging of sensitive data

### NASA Power of 10 Rules (Firmware Compliance)

1. **Simplify Control Flow** ✓ - No goto, recursion, fixed loop bounds
2. **Fixed Loop Upper-Bounds** ✓ - All loops have provable bounds
3. **No Dynamic Memory** ✓ - Zero malloc/free in RX72N firmware
4. **Short Functions** ✓ - ~60 lines max, single verifiable units
5. **Assertions/Validation** ✓ - 2+ checks per function, pre/post conditions
6. **Smallest Scope** ✓ - Variables declared close to first use
7. **Check Return Values** ✓ - All returns validated or explicitly cast void
8. **Limit Preprocessor** ✓ - Enums for constants, macros only for 3 specific cases
9. **Restrict Pointers** ⚠️ - Intentional deviation for DIP (function pointers allowed)
10. **Maximum Warnings** ✓ - -Wall -Wextra -Werror, zero warnings

## Additional Resources

- **Main CLAUDE.md**: Project-wide coding standards and architecture
- **docs/sections/\*.tex**: Technical documentation source files
- **GitHub Issues**: https://github.com/Locked-Inc/STAR/issues
- **ROS2 Docs**: https://docs.ros.org/en/jazzy/
- **ThreadX Docs**: https://github.com/eclipse-threadx/rtos-docs</content>
  <parameter name="filePath">/Users/cesarmagana/Documents/GitHub/STAR/AGENTS.md
