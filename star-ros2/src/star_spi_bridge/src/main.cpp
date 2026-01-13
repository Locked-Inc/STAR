// Copyright 2026 Locked Inc.

#include <rclcpp/rclcpp.hpp>

#include "star_spi_bridge/star_spi_driver_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  rclcpp::executors::SingleThreadedExecutor executor;
  auto node =
    std::make_shared<star_spi_bridge::StarSpiDriverNode>();

  executor.add_node(node->get_node_base_interface());
  executor.spin();

  rclcpp::shutdown();
  return 0;
}
