---
name: ros2-style
description: ROS2 C++ style guide and formatting reference for star-ros2 packages
disable-model-invocation: false
user-invokable: true
---

# ROS2 C++ Style Guide

Complete style guide for ROS2 C++ code in the STAR project.

## Philosophy

Maintain consistency with C firmware wherever possible. Only differ for C++-specific features (classes, namespaces, exceptions).

## When to Use This Guide

- **ROS2 packages** (`star-ros2/src/*/`): Use ROS2 C++ style
- **RX72N firmware** (`star-rx72n-firmware/`): Use C firmware style (see root CLAUDE.md)
- **Gateway** (`star-gateway/`): Follow Go conventions (see star-gateway/CLAUDE.md)

## Naming Conventions

### Classes and Types

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

### Methods and Functions

```cpp
// snake_case for methods (same as C firmware)
void publish_telemetry(const TelemetryData & data);
bool is_connected() const;
void on_encoder_data_received(const sensor_msgs::msg::JointState::SharedPtr msg);

// Use verb-based names that clarify actions
void check_for_errors();      // [PASS] Clear intent
void error_check();           // [FAIL] Noun-first is confusing
```

### Variables

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

### Namespaces

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

## File Naming and Organization

### Headers

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

### Source Files

```cpp
// .cpp extension
star_gateway_bridge_node.cpp
message_converter.cpp
grpc_client.cpp
```

**Naming pattern**: `under_scored` for all filenames (matches package names)

## Header Organization

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

## Formatting

### Line Length

- ROS2 C++: **120 characters** (star-ros2/.clang-format)
- C firmware: **100 characters** (star-rx72n-firmware/.clang-format)

### Indentation

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

### Braces

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

## ROS2-Specific Patterns

### Node Inheritance

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

### Publishers and Subscribers

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

### Timers

```cpp
// Use create_wall_timer for periodic operations
timer_ = this->create_wall_timer(
  std::chrono::milliseconds(100),  // 10 Hz
  std::bind(&MyNode::timer_callback, this)
);
```

## Error Handling

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

## Logging

**Use rosconsole, not printf**:

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
printf("Debug message");        // [FAIL] Don't use
std::cout << "Debug" << std::endl;  // [FAIL] Don't use
```

## Documentation

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

## Constants

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

## Differences from C Firmware Style

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

## Formatting Enforcement

```bash
# Format ROS2 C++ code
cd star-ros2
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Check formatting (CI/CD)
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
```

**CI enforcement**: `.github/workflows/ros2.yml` runs clang-format check on all PRs.

## Additional Resources

- **ROS2 C++ Style Guide**: https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html
- **ROS C++ Best Practices**: https://wiki.ros.org/CppStyleGuide
- **Google C++ Style Guide**: https://google.github.io/styleguide/cppguide.html (ROS2 loosely based on this)
- **This project's C style**: See root CLAUDE.md Sec. Code Style

## Quick Reference

### When writing ROS2 nodes, remember:

1. **Classes**: CamelCase (StarGatewayBridgeNode)
2. **Methods**: snake_case (publish_telemetry)
3. **Member variables**: trailing_ underscore (telemetry_pub_)
4. **Files**: .hpp/.cpp with under_scored names
5. **Logging**: RCLCPP_INFO/WARN/ERROR (never printf)
6. **Errors**: throw exceptions (not return codes)
7. **Line limit**: 120 characters
8. **Indentation**: 2 spaces
9. **Namespaces**: star::package_name
10. **Documentation**: Doxygen /** comments for all public APIs
