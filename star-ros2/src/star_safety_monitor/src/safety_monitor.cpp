// Copyright 2026 STAR Team
// Licensed under MIT License

#include "star_safety_monitor/safety_monitor.hpp"

namespace star_safety_monitor
{

SafetyMonitor::SafetyMonitor(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("safety_monitor", options)
{
  RCLCPP_INFO(get_logger(), "SafetyMonitor constructor called");

  // TODO: Declare parameters
  // - heartbeat_timeout_ms
  // - max_linear_velocity
  // - max_angular_velocity
  // - min_battery_voltage
  // - max_battery_current
  // - publish_rate
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

  // TODO: Initialize publishers and subscribers
  // - Create diagnostic publisher
  // - Create emergency stop publisher
  // - Subscribe to battery state
  // - Subscribe to odometry
  // - Subscribe to system diagnostics

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_activate(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Activating SafetyMonitor");

  // TODO: Start monitoring timers
  // - Create heartbeat watchdog timer
  // - Create diagnostic publish timer
  // - Activate lifecycle publishers

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_deactivate(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Deactivating SafetyMonitor");

  // TODO: Stop monitoring
  // - Cancel timers
  // - Deactivate lifecycle publishers

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_cleanup(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Cleaning up SafetyMonitor");

  // TODO: Release resources
  // - Reset publishers/subscribers
  // - Clear state tracking

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
SafetyMonitor::on_shutdown(const rclcpp_lifecycle::State & previous_state)
{
  (void)previous_state;
  RCLCPP_INFO(get_logger(), "Shutting down SafetyMonitor");

  // TODO: Perform graceful shutdown

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
}

}  // namespace star_safety_monitor
