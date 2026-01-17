// Copyright 2026 Locked Inc.

#ifndef STAR_SPI_BRIDGE__STAR_SPI_DRIVER_NODE_HPP_
#define STAR_SPI_BRIDGE__STAR_SPI_DRIVER_NODE_HPP_

#include "star_spi_bridge/spi_driver.hpp"
#include "star_spi_bridge/spi_message_converter.hpp"

#include <geometry_msgs/msg/twist.hpp>
#include <memory>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <string>

namespace star_spi_bridge
{

class StarSpiDriverNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit StarSpiDriverNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~StarSpiDriverNode() override;

  // Lifecycle transitions
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State & prev_state) override;

private:
  // Callbacks
  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg);
  void timer_callback();  // 100 Hz loop

  // Components
  std::unique_ptr<SpiDriver> spi_driver_;
  std::unique_ptr<SpiMessageConverter> converter_;

  // ROS handles
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::BatteryState>::SharedPtr battery_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  // State
  geometry_msgs::msg::Twist current_cmd_vel_;
  rclcpp::Time last_cmd_vel_time_;
  uint16_t tx_seq_ = 0;

  // Parameters
  std::string spi_device_path_;
  int spi_speed_hz_;
  int cmd_vel_timeout_ms_;
};

}  // namespace star_spi_bridge

#endif  // STAR_SPI_BRIDGE__STAR_SPI_DRIVER_NODE_HPP_
