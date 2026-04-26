/**
 * @file safety_monitor.cpp
 * @brief Safety monitor implementation for obstacle and system health detection.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

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
    declare_parameter("publish_rate", 10.0);
    declare_parameter("enable_auto_estop", true);
    declare_parameter("estop_recovery_delay", 5.0);
    declare_parameter("stall_detection_threshold", 0.05);
    declare_parameter("stall_samples_required", 5);
    declare_parameter("cmd_vel_timeout_ms", 500);
    declare_parameter("obstacle_estop_distance", 0.10);
    declare_parameter("obstacle_warn_distance", 0.30);
    declare_parameter("obstacle_clear_count_required", 10);

    heartbeat_timeout_ms_ = get_parameter("heartbeat_timeout_ms").as_int();
    max_linear_velocity_ = get_parameter("max_linear_velocity").as_double();
    max_angular_velocity_ = get_parameter("max_angular_velocity").as_double();
    publish_rate_ = get_parameter("publish_rate").as_double();
    enable_auto_estop_ = get_parameter("enable_auto_estop").as_bool();
    estop_recovery_delay_ = get_parameter("estop_recovery_delay").as_double();
    stall_detection_threshold_ = get_parameter("stall_detection_threshold").as_double();
    stall_samples_required_ = get_parameter("stall_samples_required").as_int();
    cmd_vel_timeout_ms_ = get_parameter("cmd_vel_timeout_ms").as_int();
    obstacle_estop_distance_ = get_parameter("obstacle_estop_distance").as_double();
    obstacle_warn_distance_ = get_parameter("obstacle_warn_distance").as_double();
    obstacle_clear_count_required_ = get_parameter("obstacle_clear_count_required").as_int();

    RCLCPP_INFO(get_logger(), "Configuration loaded:");
    RCLCPP_INFO(get_logger(), "  Heartbeat timeout: %d ms", heartbeat_timeout_ms_);
    RCLCPP_INFO(get_logger(), "  Max linear velocity: %.2f m/s", max_linear_velocity_);
    RCLCPP_INFO(get_logger(), "  Max angular velocity: %.2f rad/s", max_angular_velocity_);

    // Create lifecycle publishers
    diagnostics_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    emergency_stop_pub_ = create_publisher<std_msgs::msg::Bool>("/emergency_stop", 10);

    // Create subscribers
    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("/odom", 10,
                                                             std::bind(&SafetyMonitor::odometry_callback, this,
                                                                       std::placeholders::_1));

    diag_sub_ = create_subscription<
      diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10,
                                             std::bind(&SafetyMonitor::diagnostics_callback, this,
                                                       std::placeholders::_1));

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>("/cmd_vel", 10,
                                                                  std::bind(&SafetyMonitor::cmd_vel_callback, this,
                                                                            std::placeholders::_1));

    // Subscribe to ultrasonic sonar Range topics
    for (size_t i = 0; i < NUM_SONARS; ++i) {
      sonar_subs_[i] = create_subscription<sensor_msgs::msg::Range>(std::string(SONAR_TOPICS[i]), 10,
                                                                    [this,
                                                                     i](const sensor_msgs::msg::Range::SharedPtr msg) {
                                                                      sonar_callback(msg, i);
                                                                    });
      sonar_ranges_[i] = std::numeric_limits<double>::infinity();
    }

    RCLCPP_INFO(get_logger(), "  Obstacle E-stop distance: %.2f m", obstacle_estop_distance_);
    RCLCPP_INFO(get_logger(), "  Obstacle warn distance: %.2f m", obstacle_warn_distance_);

    // Initialize state
    last_diagnostics_time_ = std::chrono::system_clock::now();
    last_cmd_vel_time_ = std::chrono::system_clock::now();
    heartbeat_times_.clear();
    current_linear_velocity_ = 0.0;
    current_angular_velocity_ = 0.0;
    velocity_exceeded_ = false;
    heartbeat_timeout_triggered_ = false;
    emergency_stop_active_ = false;
    motor_stall_detected_ = false;
    obstacle_estop_triggered_ = false;
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

    // Reset heartbeat baseline so the first timer tick doesn't
    // immediately trigger a heartbeat timeout.
    last_diagnostics_time_ = std::chrono::system_clock::now();

    // Create bond to nav2_lifecycle_manager (heartbeat keepalive)
    bond_ = std::make_shared<bond::Bond>("bond", get_name(), shared_from_this());
    bond_->setHeartbeatPeriod(0.25);
    bond_->setHeartbeatTimeout(4.0);
    bond_->start();

    // Create monitoring timer (10 Hz by default)
    auto timer_period = std::chrono::milliseconds(static_cast<int>(1000.0 / publish_rate_));
    monitoring_timer_ = create_wall_timer(timer_period, std::bind(&SafetyMonitor::monitoring_timer_callback, this));

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
    // Destroy bond before deactivating
    bond_.reset();

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
    // Reset bond, publishers and subscribers
    bond_.reset();
    diagnostics_pub_.reset();
    emergency_stop_pub_.reset();
    odom_sub_.reset();
    diag_sub_.reset();
    cmd_vel_sub_.reset();
    for (auto & sub : sonar_subs_) {
      sub.reset();
    }

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

  // Ensure emergency stop is triggered on shutdown if active.
  //
  // emergency_stop_pub_ is a LifecyclePublisher; a publish() call on it is a
  // silent no-op if the publisher is not in the activated sub-state. We can
  // arrive at on_shutdown() from any reachable lifecycle state -- including
  // DEACTIVATED (after on_deactivate() called on_deactivate() on the
  // publisher) or UNCONFIGURED (after on_cleanup() reset() the publisher to
  // nullptr). Guard both: the pointer existence first, then the activation
  // sub-state. If the publisher is currently deactivated, transition it back
  // to activated so the e-stop actually goes out on the wire.
  if (emergency_stop_active_ && emergency_stop_pub_) {
    if (!emergency_stop_pub_->is_activated()) {
      emergency_stop_pub_->on_activate();
    }
    auto msg = std_msgs::msg::Bool();
    msg.data = true;
    emergency_stop_pub_->publish(msg);
  }

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

void SafetyMonitor::odometry_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
  // Extract current velocities
  current_linear_velocity_ = std::sqrt(msg->twist.twist.linear.x * msg->twist.twist.linear.x +
                                       msg->twist.twist.linear.y * msg->twist.twist.linear.y +
                                       msg->twist.twist.linear.z * msg->twist.twist.linear.z);

  current_angular_velocity_ = std::sqrt(msg->twist.twist.angular.x * msg->twist.twist.angular.x +
                                        msg->twist.twist.angular.y * msg->twist.twist.angular.y +
                                        msg->twist.twist.angular.z * msg->twist.twist.angular.z);
}

void SafetyMonitor::diagnostics_callback(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg)
{
  // Ignore our own diagnostics (we publish on the same topic).
  // Only count messages from external nodes as heartbeats.
  bool from_self = true;
  for (const auto & status : msg->status) {
    if (status.hardware_id != "safety_monitor") {
      from_self = false;
      heartbeat_times_[status.name] = std::chrono::system_clock::now();
    }
  }

  if (!from_self) {
    last_diagnostics_time_ = std::chrono::system_clock::now();
  }
}

void SafetyMonitor::cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
{
  last_cmd_vel_ = *msg;
  last_cmd_vel_time_ = std::chrono::system_clock::now();
}

void SafetyMonitor::sonar_callback(const sensor_msgs::msg::Range::SharedPtr msg, size_t sonar_index)
{
  if (sonar_index < NUM_SONARS) {
    sonar_ranges_[sonar_index] = static_cast<double>(msg->range);
  }
}

void SafetyMonitor::monitoring_timer_callback()
{
  // Perform safety checks
  check_heartbeat_health();
  check_velocity_limits();
  check_motor_stall();
  check_obstacle_proximity();
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
      estop_trigger_time_ = now;
    }
  } else {
    // Reset timeout if heartbeat recovered. Do not touch
    // emergency_stop_active_ directly: update_overall_state() derives it
    // from all latched sources, so other causes (obstacle, stall) remain
    // honored when only the heartbeat path clears.
    if (heartbeat_timeout_triggered_) {
      auto estop_duration = now - estop_trigger_time_;
      if (estop_duration > std::chrono::duration<double>(estop_recovery_delay_)) {
        RCLCPP_INFO(get_logger(), "Heartbeat recovered");
        heartbeat_timeout_triggered_ = false;
      }
    }
  }
}

void SafetyMonitor::check_velocity_limits()
{
  velocity_exceeded_ = false;

  if (current_linear_velocity_ > max_linear_velocity_) {
    RCLCPP_WARN(get_logger(), "Linear velocity limit exceeded: %.2f > %.2f m/s", current_linear_velocity_,
                max_linear_velocity_);
    velocity_exceeded_ = true;
  }

  if (current_angular_velocity_ > max_angular_velocity_) {
    RCLCPP_WARN(get_logger(), "Angular velocity limit exceeded: %.2f > %.2f rad/s", current_angular_velocity_,
                max_angular_velocity_);
    velocity_exceeded_ = true;
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

  if (cmd_age < std::chrono::milliseconds(cmd_vel_timeout_ms_) && cmd_linear > stall_detection_threshold_) {
    // We have a recent command to move
    if (current_linear_velocity_ < stall_detection_threshold_) {
      // But we're not actually moving
      stall_detection_count_++;

      if (stall_detection_count_ >= stall_samples_required_) {
        RCLCPP_WARN(get_logger(), "Motor stall detected: cmd=%.2f m/s, actual=%.2f m/s", cmd_linear,
                    current_linear_velocity_);
        motor_stall_detected_ = true;
        estop_trigger_time_ = now;
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

void SafetyMonitor::check_obstacle_proximity()
{
  obstacle_too_close_ = false;

  for (size_t i = 0; i < NUM_SONARS; ++i) {
    double range = sonar_ranges_[i];

    // Skip infinite/NaN readings (no obstacle in range)
    if (!std::isfinite(range)) {
      continue;
    }

    if (range < obstacle_estop_distance_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                           "Obstacle E-STOP: %s sonar reads %.3f m (threshold: %.2f m)", SONAR_NAMES[i], range,
                           obstacle_estop_distance_);
      obstacle_too_close_ = true;
      if (enable_auto_estop_) {
        obstacle_estop_triggered_ = true;
        estop_trigger_time_ = std::chrono::system_clock::now();
      }
    } else if (range < obstacle_warn_distance_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                           "Obstacle warning: %s sonar reads %.3f m (threshold: %.2f m)", SONAR_NAMES[i], range,
                           obstacle_warn_distance_);
    }
  }

  // Obstacle recovery: only release the obstacle-sourced e-stop.
  // Do NOT clear emergency_stop_active_ if heartbeat or stall also set it.
  if (!obstacle_too_close_ && obstacle_estop_triggered_) {
    obstacle_clear_count_++;
    if (obstacle_clear_count_ >= obstacle_clear_count_required_) {
      RCLCPP_INFO(get_logger(), "Obstacle cleared for %d consecutive checks, releasing obstacle E-stop",
                  obstacle_clear_count_required_);
      obstacle_estop_triggered_ = false;
      obstacle_clear_count_ = 0;
    }
  } else if (obstacle_too_close_) {
    obstacle_clear_count_ = 0;
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

  if (heartbeat_timeout_triggered_ || obstacle_too_close_) {
    overall_severity_ = SeverityLevel::ERROR;
  } else if (motor_stall_detected_) {
    overall_severity_ = SeverityLevel::WARN;
  } else if (velocity_exceeded_) {
    overall_severity_ = SeverityLevel::WARN;
  }

  // Derive emergency_stop_active_ from all independent sources.
  if (enable_auto_estop_) {
    emergency_stop_active_ = heartbeat_timeout_triggered_ || obstacle_estop_triggered_ || motor_stall_detected_;
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

  kv.key = "Obstacle Too Close";
  kv.value = obstacle_too_close_ ? "true" : "false";
  system_status.values.push_back(kv);

  kv.key = "Emergency Stop Active";
  kv.value = emergency_stop_active_ ? "true" : "false";
  system_status.values.push_back(kv);

  // Sonar readings
  for (size_t i = 0; i < NUM_SONARS; ++i) {
    kv.key = std::string("Sonar ") + SONAR_NAMES[i] + " (m)";
    if (std::isfinite(sonar_ranges_[i])) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(3) << sonar_ranges_[i];
      kv.value = oss.str();
    } else {
      kv.value = "inf";
    }
    system_status.values.push_back(kv);
  }

  // Add heartbeat information for each tracked node
  diagnostic_msgs::msg::DiagnosticStatus heartbeat_status;
  heartbeat_status.name = "safety_monitor: Heartbeat Status";
  heartbeat_status.hardware_id = "safety_monitor";
  heartbeat_status.level = heartbeat_timeout_triggered_ ? diagnostic_msgs::msg::DiagnosticStatus::ERROR
                                                        : diagnostic_msgs::msg::DiagnosticStatus::OK;

  auto now_time = std::chrono::system_clock::now();
  for (const auto & [node_name, last_time] : heartbeat_times_) {
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_time).count();

    kv.key = node_name;
    kv.value = std::to_string(duration_ms) + " ms";
    heartbeat_status.values.push_back(kv);
  }

  diag_array.status.push_back(system_status);
  diag_array.status.push_back(heartbeat_status);

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
  return std::sqrt(twist.linear.x * twist.linear.x + twist.linear.y * twist.linear.y + twist.linear.z * twist.linear.z);
}

}  // namespace star_safety_monitor
