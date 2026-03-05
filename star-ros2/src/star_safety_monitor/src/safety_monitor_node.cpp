/* star-ros2/src/star_safety_monitor/src/safety_monitor_node.cpp */

/**
 * @file safety_monitor_node.cpp
 * @brief Permission is hereby granted, free of charge, to any person obtaining a copy
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <memory>
#include <rclcpp/rclcpp.hpp>
#include "star_safety_monitor/safety_monitor.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();
  return 0;
}
