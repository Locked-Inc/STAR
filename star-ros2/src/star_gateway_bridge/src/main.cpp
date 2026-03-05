/**
 * @file main.cpp
 * @brief Entry point for running star_gateway_bridge node as standalone process.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "star_gateway_bridge/star_gateway_bridge_node.hpp"

/**
 * @brief Main entry point for star_gateway_bridge standalone executable.
 *
 * Usage:
 *   ros2 run star_gateway_bridge star_gateway_bridge_main
 *
 * Parameters can be set via command line:
 *   ros2 run star_gateway_bridge star_gateway_bridge_main \
 *     --ros-args -p gateway_address:=192.168.1.100:50051 \
 *                -p telemetry_rate_hz:=20.0

*/
int main(int argc, char **argv)
{
  // Initialize ROS2
  rclcpp::init(argc, argv);

  // Create and spin node
  auto node = std::make_shared<star::star_gateway_bridge::StarGatewayBridgeNode>();

  RCLCPP_INFO(node->get_logger(), "STAR Gateway Bridge node running");
  RCLCPP_INFO(node->get_logger(), "Press Ctrl+C to exit");

  // Spin until shutdown
  rclcpp::spin(node);

  // Cleanup
  rclcpp::shutdown();

  RCLCPP_INFO(node->get_logger(), "STAR Gateway Bridge node exited cleanly");

  return 0;
}
