// Copyright 2026 STAR Team
// Licensed under MIT License

#ifndef STAR_SAFETY_MONITOR__SAFETY_MONITOR_HPP_
#define STAR_SAFETY_MONITOR__SAFETY_MONITOR_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/bool.hpp>
#include <chrono>
#include <map>
#include <memory>

namespace star_safety_monitor
{

// Enum for safety severity levels
enum class SeverityLevel
{
  OK = 0,
  WARN = 1,
  ERROR = 2,
  STALE = 3
};

class SafetyMonitor : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit SafetyMonitor(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~SafetyMonitor();

  // Lifecycle callbacks
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State & previous_state) override;

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & previous_state) override;

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State & previous_state) override;

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State & previous_state) override;

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State & previous_state) override;

private:
  // Subscription callbacks
  void odometry_callback(const nav_msgs::msg::Odometry::SharedPtr msg);
  void diagnostics_callback(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg);

  // Timer callback for monitoring and publishing diagnostics
  void monitoring_timer_callback();

  // Monitoring and safety check methods
  void check_heartbeat_health();
  void check_velocity_limits();
  void check_diagnostic_health();
  void update_overall_state();
  void publish_diagnostics();

  // Helper methods
  void add_diagnostic_status(
    diagnostic_msgs::msg::DiagnosticStatus & status,
    const std::string & name,
    SeverityLevel level,
    const std::string & message);

  // Publishers and Subscribers
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diag_sub_;

  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    diagnostics_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::Bool>::SharedPtr emergency_stop_pub_;

  // Timer
  rclcpp::TimerBase::SharedPtr monitoring_timer_;

  // Safety parameters (declared in on_configure)
  int heartbeat_timeout_ms_;
  double max_linear_velocity_;
  double max_angular_velocity_;
  double min_battery_voltage_;
  double max_battery_current_;
  double max_battery_temp_;
  double publish_rate_;
  bool enable_auto_estop_;
  double estop_recovery_delay_;

  // State tracking
  std::chrono::system_clock::time_point last_diagnostics_time_;
  std::map<std::string, std::chrono::system_clock::time_point> heartbeat_times_;
  double current_linear_velocity_{0.0};
  double current_angular_velocity_{0.0};
  bool velocity_exceeded_{false};
  bool heartbeat_timeout_triggered_{false};
  bool emergency_stop_active_{false};
  std::chrono::system_clock::time_point estop_trigger_time_;

  SeverityLevel overall_severity_{SeverityLevel::OK};
};

}  // namespace star_safety_monitor

#endif  // STAR_SAFETY_MONITOR__SAFETY_MONITOR_HPP_
