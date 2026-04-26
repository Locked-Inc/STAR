/**
 * @file safety_monitor_node.cpp
 * @brief ROS2 node entry point for the safety monitor.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <rclcpp/rclcpp.hpp>
#include "star_safety_monitor/safety_monitor.hpp"
#include <memory>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();
  return 0;
}
