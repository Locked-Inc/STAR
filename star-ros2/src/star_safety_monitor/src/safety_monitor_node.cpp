// Copyright 2026 STAR Team
// Licensed under MIT License

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
