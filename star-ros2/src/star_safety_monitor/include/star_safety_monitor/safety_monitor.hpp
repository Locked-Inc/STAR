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

namespace star_safety_monitor
{

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
  // TODO: Implement private methods
  // - Heartbeat monitoring
  // - Sensor health checks
  // - Emergency stop logic
  // - Diagnostic message generation

  // TODO: Add private members
  // - Publishers/Subscribers
  // - Timers
  // - Safety state tracking
};

}  // namespace star_safety_monitor

#endif  // STAR_SAFETY_MONITOR__SAFETY_MONITOR_HPP_
