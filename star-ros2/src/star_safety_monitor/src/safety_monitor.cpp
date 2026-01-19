// Copyright 2026 STAR Team
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "star_safety_monitor/safety_monitor.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace star_safety_monitor
{

SafetyMonitor::SafetyMonitor(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("safety_monitor", options)
{
  RCLCPP_INFO(get_logger(), "SafetyMonitor constructor called");
}

SafetyMonitor::~SafetyMonitor()
{
  RCLCPP_INFO(get_logger(), "SafetyMonitor destructor called");
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_configure(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Configuring SafetyMonitor");

  try {
    // Declare and get parameters
    declare_parameter("heartbeat_timeout_ms", 500);
    declare_parameter("max_linear_velocity", 1.0);
    declare_parameter("max_angular_velocity", 2.0);
    declare_parameter("min_battery_voltage", 10.5);
    declare_parameter("max_battery_current", 30.0);
    declare_parameter("max_battery_temp", 60.0);
    declare_parameter("publish_rate", 10.0);
    declare_parameter("enable_auto_estop", true);
    declare_parameter("estop_recovery_delay", 5.0);
    declare_parameter("stall_detection_threshold", 0.05);
    declare_parameter("stall_samples_required", 5);
    declare_parameter("cmd_vel_timeout_ms", 500);

    heartbeat_timeout_ms_ = get_parameter("heartbeat_timeout_ms").as_int();
    max_linear_velocity_ = get_parameter("max_linear_velocity").as_double();
    max_angular_velocity_ = get_parameter("max_angular_velocity").as_double();
    min_battery_voltage_ = get_parameter("min_battery_voltage").as_double();
    max_battery_current_ = get_parameter("max_battery_current").as_double();
    max_battery_temp_ = get_parameter("max_battery_temp").as_double();
    publish_rate_ = get_parameter("publish_rate").as_double();
    enable_auto_estop_ = get_parameter("enable_auto_estop").as_bool();
    estop_recovery_delay_ = get_parameter("estop_recovery_delay").as_double();
    stall_detection_threshold_ = get_parameter("stall_detection_threshold").as_double();
    stall_samples_required_ = get_parameter("stall_samples_required").as_int();
    cmd_vel_timeout_ms_ = get_parameter("cmd_vel_timeout_ms").as_int();

    RCLCPP_INFO(get_logger(), "Configuration loaded:");
    RCLCPP_INFO(get_logger(), "  Heartbeat timeout: %d ms", heartbeat_timeout_ms_);
    RCLCPP_INFO(get_logger(), "  Max linear velocity: %.2f m/s", max_linear_velocity_);
    RCLCPP_INFO(get_logger(), "  Max angular velocity: %.2f rad/s", max_angular_velocity_);
    RCLCPP_INFO(get_logger(), "  Min battery voltage: %.1f V", min_battery_voltage_);
    RCLCPP_INFO(get_logger(), "  Max battery current: %.1f A", max_battery_current_);

    // Create lifecycle publishers
    diagnostics_pub_ =
      create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    emergency_stop_pub_ = create_publisher<std_msgs::msg::Bool>("/emergency_stop", 10);

    // Create subscribers
    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "/odom", 10,
      std::bind(&SafetyMonitor::odometry_callback, this, std::placeholders::_1));

    diag_sub_ = create_subscription<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", 10,
      std::bind(&SafetyMonitor::diagnostics_callback, this, std::placeholders::_1));

    battery_sub_ = create_subscription<sensor_msgs::msg::BatteryState>(
      "/battery_state", 10,
      std::bind(&SafetyMonitor::battery_state_callback, this, std::placeholders::_1));

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 10,
      std::bind(&SafetyMonitor::cmd_vel_callback, this, std::placeholders::_1));

    // Initialize state
    last_diagnostics_time_ = std::chrono::system_clock::now();
    last_battery_time_ = std::chrono::system_clock::now();
    last_cmd_vel_time_ = std::chrono::system_clock::now();
    heartbeat_times_.clear();
    current_linear_velocity_ = 0.0;
    current_angular_velocity_ = 0.0;
    velocity_exceeded_ = false;
    heartbeat_timeout_triggered_ = false;
    emergency_stop_active_ = false;
    battery_voltage_low_ = false;
    battery_current_high_ = false;
    motor_stall_detected_ = false;
    stall_detection_count_ = 0;
    overall_severity_ = SeverityLevel::OK;

    RCLCPP_INFO(get_logger(), "SafetyMonitor configured successfully");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Configuration failed: %s", e.what());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::FAILURE;
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_activate(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Activating SafetyMonitor");

  try {
    // Activate publishers
    diagnostics_pub_->on_activate();
    emergency_stop_pub_->on_activate();

    // Create monitoring timer (10 Hz by default)
    auto timer_period = std::chrono::milliseconds(
      static_cast<int>(1000.0 / publish_rate_));
    monitoring_timer_ =
      create_wall_timer(timer_period,
        std::bind(&SafetyMonitor::monitoring_timer_callback, this));

    RCLCPP_INFO(get_logger(), "SafetyMonitor activated successfully");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Activation failed: %s", e.what());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::FAILURE;
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_deactivate(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Deactivating SafetyMonitor");

  try {
    // Cancel timer
    if (monitoring_timer_) {
      monitoring_timer_->cancel();
      monitoring_timer_.reset();
    }

    // Deactivate publishers
    diagnostics_pub_->on_deactivate();
    emergency_stop_pub_->on_deactivate();

    RCLCPP_INFO(get_logger(), "SafetyMonitor deactivated successfully");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Deactivation failed: %s", e.what());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::FAILURE;
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_cleanup(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Cleaning up SafetyMonitor");

  try {
    // Reset publishers and subscribers
    diagnostics_pub_.reset();
    emergency_stop_pub_.reset();
    odom_sub_.reset();
    diag_sub_.reset();
    battery_sub_.reset();
    cmd_vel_sub_.reset();

    // Clear state
    heartbeat_times_.clear();

    RCLCPP_INFO(get_logger(), "SafetyMonitor cleaned up successfully");
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Cleanup failed: %s", e.what());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::FAILURE;
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_shutdown(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Shutting down SafetyMonitor");

  // Ensure emergency stop is triggered on shutdown if active
  if (emergency_stop_active_) {
    auto msg = std_msgs::msg::Bool();
    msg.data = true;
    emergency_stop_pub_->publish(msg);
  }

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

void SafetyMonitor::odometry_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
  // Extract current velocities
  current_linear_velocity_ = std::sqrt(
    msg->twist.twist.linear.x * msg->twist.twist.linear.x +
    msg->twist.twist.linear.y * msg->twist.twist.linear.y +
    msg->twist.twist.linear.z * msg->twist.twist.linear.z);

  current_angular_velocity_ = std::sqrt(
    msg->twist.twist.angular.x * msg->twist.twist.angular.x +
    msg->twist.twist.angular.y * msg->twist.twist.angular.y +
    msg->twist.twist.angular.z * msg->twist.twist.angular.z);
}

void SafetyMonitor::diagnostics_callback(
  const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg)
{
  last_diagnostics_time_ = std::chrono::system_clock::now();

  // Update heartbeat times for known nodes
  for (const auto & status : msg->status) {
    heartbeat_times_[status.name] = std::chrono::system_clock::now();
  }
}

void SafetyMonitor::battery_state_callback(
  const sensor_msgs::msg::BatteryState::SharedPtr msg)
{
  last_battery_time_ = std::chrono::system_clock::now();
  battery_voltage_ = msg->voltage;
  battery_current_ = msg->current;
  battery_percentage_ = msg->percentage;
}

void SafetyMonitor::cmd_vel_callback(
  const geometry_msgs::msg::Twist::SharedPtr msg)
{
  last_cmd_vel_ = *msg;
  last_cmd_vel_time_ = std::chrono::system_clock::now();
}

void SafetyMonitor::monitoring_timer_callback()
{
  // Perform safety checks
  check_heartbeat_health();
  check_velocity_limits();
  check_battery_health();
  check_motor_stall();
  check_diagnostic_health();
  update_overall_state();
  publish_diagnostics();
}

void SafetyMonitor::check_heartbeat_health()
{
  auto now = std::chrono::system_clock::now();
  auto timeout_duration = std::chrono::milliseconds(heartbeat_timeout_ms_);

  // Check for stale diagnostics
  auto time_since_diag = now - last_diagnostics_time_;
  if (time_since_diag > timeout_duration) {
    if (!heartbeat_timeout_triggered_) {
      RCLCPP_WARN(get_logger(), "Heartbeat timeout detected!");
      heartbeat_timeout_triggered_ = true;
      if (enable_auto_estop_) {
        emergency_stop_active_ = true;
        estop_trigger_time_ = now;
      }
    }
  } else {
    // Reset timeout if heartbeat recovered
    if (heartbeat_timeout_triggered_) {
      auto estop_duration = now - estop_trigger_time_;
      if (estop_duration > std::chrono::duration<double>(estop_recovery_delay_)) {
        RCLCPP_INFO(get_logger(), "Heartbeat recovered");
        heartbeat_timeout_triggered_ = false;
        emergency_stop_active_ = false;
      }
    }
  }
}

void SafetyMonitor::check_velocity_limits()
{
  velocity_exceeded_ = false;

  if (current_linear_velocity_ > max_linear_velocity_) {
    RCLCPP_WARN(get_logger(),
      "Linear velocity limit exceeded: %.2f > %.2f m/s",
      current_linear_velocity_, max_linear_velocity_);
    velocity_exceeded_ = true;
  }

  if (current_angular_velocity_ > max_angular_velocity_) {
    RCLCPP_WARN(get_logger(),
      "Angular velocity limit exceeded: %.2f > %.2f rad/s",
      current_angular_velocity_, max_angular_velocity_);
    velocity_exceeded_ = true;
  }
}

void SafetyMonitor::check_battery_health()
{
  battery_voltage_low_ = false;
  battery_current_high_ = false;

  // Check battery voltage
  if (battery_voltage_ > 0.0 && battery_voltage_ < min_battery_voltage_) {
    RCLCPP_WARN(get_logger(),
      "Battery voltage low: %.2f V < %.2f V",
      battery_voltage_, min_battery_voltage_);
    battery_voltage_low_ = true;
    if (enable_auto_estop_) {
      emergency_stop_active_ = true;
      estop_trigger_time_ = std::chrono::system_clock::now();
    }
  }

  // Check battery current (absolute value - both charging and discharging)
  double abs_current = std::abs(battery_current_);
  if (abs_current > max_battery_current_) {
    RCLCPP_WARN(get_logger(),
      "Battery current high: %.2f A > %.2f A",
      abs_current, max_battery_current_);
    battery_current_high_ = true;
    if (enable_auto_estop_) {
      emergency_stop_active_ = true;
      estop_trigger_time_ = std::chrono::system_clock::now();
    }
  }

  // Check battery stale (no updates for timeout period)
  auto now = std::chrono::system_clock::now();
  auto battery_age = now - last_battery_time_;
  if (battery_age > std::chrono::milliseconds(heartbeat_timeout_ms_)) {
    RCLCPP_WARN(get_logger(), "Battery state stale");
    // Note: We don't trigger E-Stop for stale battery alone, as it could be
    // a separate subsystem. The heartbeat check will catch RX72N failure.
  }
}

void SafetyMonitor::check_motor_stall()
{
  motor_stall_detected_ = false;

  // Stall detection: command velocity present but actual velocity is near zero
  double cmd_linear = compute_linear_magnitude(last_cmd_vel_);

  // Check if a command was issued recently
  auto now = std::chrono::system_clock::now();
  auto cmd_age = now - last_cmd_vel_time_;

  if (cmd_age < std::chrono::milliseconds(cmd_vel_timeout_ms_) &&
    cmd_linear > stall_detection_threshold_)
  {
    // We have a recent command to move
    if (current_linear_velocity_ < stall_detection_threshold_) {
      // But we're not actually moving
      stall_detection_count_++;

      if (stall_detection_count_ >= stall_samples_required_) {
        RCLCPP_WARN(get_logger(),
          "Motor stall detected: cmd=%.2f m/s, actual=%.2f m/s",
          cmd_linear, current_linear_velocity_);
        motor_stall_detected_ = true;
        if (enable_auto_estop_) {
          emergency_stop_active_ = true;
          estop_trigger_time_ = now;
        }
      }
    } else {
      // Motor is responding, reset counter
      stall_detection_count_ = 0;
    }
  } else {
    // No recent command, reset counter
    stall_detection_count_ = 0;
  }
}

void SafetyMonitor::check_diagnostic_health()
{
  // Additional diagnostic checks can be added here
  // For now, this is a placeholder for future expansion
}

void SafetyMonitor::update_overall_state()
{
  // Determine overall severity level
  overall_severity_ = SeverityLevel::OK;

  if (heartbeat_timeout_triggered_) {
    overall_severity_ = SeverityLevel::ERROR;
  } else if (battery_voltage_low_ || battery_current_high_ || motor_stall_detected_) {
    overall_severity_ = SeverityLevel::WARN;
  } else if (velocity_exceeded_) {
    overall_severity_ = SeverityLevel::WARN;
  }

  // Publish emergency stop signal if needed
  if (emergency_stop_active_) {
    auto msg = std_msgs::msg::Bool();
    msg.data = true;
    emergency_stop_pub_->publish(msg);
  }
}

void SafetyMonitor::publish_diagnostics()
{
  auto diag_array = diagnostic_msgs::msg::DiagnosticArray();
  diag_array.header.stamp = now();

  // System health status
  diagnostic_msgs::msg::DiagnosticStatus system_status;
  system_status.name = "safety_monitor: System Health";
  system_status.hardware_id = "safety_monitor";

  if (overall_severity_ == SeverityLevel::OK) {
    system_status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    system_status.message = "All systems nominal";
  } else if (overall_severity_ == SeverityLevel::WARN) {
    system_status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
    system_status.message = "Warning conditions detected";
  } else if (overall_severity_ == SeverityLevel::ERROR) {
    system_status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    system_status.message = "Critical error - emergency stop triggered";
  }

  // Add velocity data
  diagnostic_msgs::msg::KeyValue kv;
  kv.key = "Linear Velocity (m/s)";
  kv.value = std::to_string(current_linear_velocity_);
  system_status.values.push_back(kv);

  kv.key = "Angular Velocity (rad/s)";
  kv.value = std::to_string(current_angular_velocity_);
  system_status.values.push_back(kv);

  kv.key = "Velocity Limit Exceeded";
  kv.value = velocity_exceeded_ ? "true" : "false";
  system_status.values.push_back(kv);

  kv.key = "Motor Stall Detected";
  kv.value = motor_stall_detected_ ? "true" : "false";
  system_status.values.push_back(kv);

  kv.key = "Heartbeat Timeout";
  kv.value = heartbeat_timeout_triggered_ ? "true" : "false";
  system_status.values.push_back(kv);

  kv.key = "Emergency Stop Active";
  kv.value = emergency_stop_active_ ? "true" : "false";
  system_status.values.push_back(kv);

  // Add heartbeat information for each tracked node
  diagnostic_msgs::msg::DiagnosticStatus heartbeat_status;
  heartbeat_status.name = "safety_monitor: Heartbeat Status";
  heartbeat_status.hardware_id = "safety_monitor";
  heartbeat_status.level = heartbeat_timeout_triggered_ ?
    diagnostic_msgs::msg::DiagnosticStatus::ERROR :
    diagnostic_msgs::msg::DiagnosticStatus::OK;

  auto now_time = std::chrono::system_clock::now();
  for (const auto & [node_name, last_time] : heartbeat_times_) {
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now_time - last_time).count();

    kv.key = node_name;
    kv.value = std::to_string(duration_ms) + " ms";
    heartbeat_status.values.push_back(kv);
  }

  // Battery status
  diagnostic_msgs::msg::DiagnosticStatus battery_status;
  battery_status.name = "safety_monitor: Battery Status";
  battery_status.hardware_id = "safety_monitor";
  if (battery_voltage_low_ || battery_current_high_) {
    battery_status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
    battery_status.message = "Battery warning conditions detected";
  } else {
    battery_status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    battery_status.message = "Battery nominal";
  }

  kv.key = "Voltage (V)";
  kv.value = std::to_string(battery_voltage_);
  battery_status.values.push_back(kv);

  kv.key = "Current (A)";
  kv.value = std::to_string(battery_current_);
  battery_status.values.push_back(kv);

  kv.key = "Percentage (%)";
  kv.value = std::to_string(battery_percentage_ * 100);
  battery_status.values.push_back(kv);

  kv.key = "Voltage Low";
  kv.value = battery_voltage_low_ ? "true" : "false";
  battery_status.values.push_back(kv);

  kv.key = "Current High";
  kv.value = battery_current_high_ ? "true" : "false";
  battery_status.values.push_back(kv);

  diag_array.status.push_back(system_status);
  diag_array.status.push_back(heartbeat_status);
  diag_array.status.push_back(battery_status);

  // Motor status
  diagnostic_msgs::msg::DiagnosticStatus motor_status;
  motor_status.name = "safety_monitor: Motor Status";
  motor_status.hardware_id = "safety_monitor";
  if (motor_stall_detected_) {
    motor_status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
    motor_status.message = "Motor stall detected";
  } else {
    motor_status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    motor_status.message = "Motor nominal";
  }

  kv.key = "Stall Detected";
  kv.value = motor_stall_detected_ ? "true" : "false";
  motor_status.values.push_back(kv);

  kv.key = "Command Velocity (m/s)";
  double cmd_linear = compute_linear_magnitude(last_cmd_vel_);
  kv.value = std::to_string(cmd_linear);
  motor_status.values.push_back(kv);

  kv.key = "Actual Velocity (m/s)";
  kv.value = std::to_string(current_linear_velocity_);
  motor_status.values.push_back(kv);

  kv.key = "Stall Detection Count";
  kv.value = std::to_string(stall_detection_count_);
  motor_status.values.push_back(kv);

  diag_array.status.push_back(motor_status);

  diagnostics_pub_->publish(diag_array);
}

double SafetyMonitor::compute_linear_magnitude(const geometry_msgs::msg::Twist & twist) const
{
  return std::sqrt(
    twist.linear.x * twist.linear.x +
    twist.linear.y * twist.linear.y +
    twist.linear.z * twist.linear.z);
}

}  // namespace star_safety_monitor
