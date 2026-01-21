// Copyright 2026 Locked Inc.

#include "star_spi_bridge/star_spi_driver_node.hpp"

#include <chrono>

using namespace std::chrono_literals;

namespace star_spi_bridge {
StarSpiDriverNode::StarSpiDriverNode(const rclcpp::NodeOptions &options)
    : rclcpp_lifecycle::LifecycleNode("star_spi_driver", options) {
  // Declare parameters
  declare_parameter("spi_device_path", "/dev/spidev0.0");
  declare_parameter("spi_speed_hz", 10000000); // 10 MHz
  declare_parameter("cmd_vel_timeout_ms", 500);
  declare_parameter("wheel_base", 0.150);
  declare_parameter("wheel_radius", 0.0325);
  declare_parameter("ticks_per_rev", 11599);
}

StarSpiDriverNode::~StarSpiDriverNode() {
  // Ensure cleanup
  if (spi_driver_) {
    spi_driver_->close_device();
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_configure(const rclcpp_lifecycle::State &) {
  RCLCPP_INFO(get_logger(), "Configuring StarSpiDriverNode...");

  // Load parameters
  spi_device_path_ = get_parameter("spi_device_path").as_string();
  spi_speed_hz_ = get_parameter("spi_speed_hz").as_int();
  cmd_vel_timeout_ms_ = get_parameter("cmd_vel_timeout_ms").as_int();

  SpiMessageConverter::Parameters converter_params;
  converter_params.wheel_base = get_parameter("wheel_base").as_double();
  converter_params.wheel_radius = get_parameter("wheel_radius").as_double();
  converter_params.ticks_per_rev = get_parameter("ticks_per_rev").as_int();

  // Initialize components
  spi_driver_ = std::make_unique<SpiDriver>(spi_device_path_, spi_speed_hz_);
  converter_ = std::make_unique<SpiMessageConverter>(converter_params);

  if (!spi_driver_->initialize()) {
    RCLCPP_ERROR(get_logger(), "Failed to initialize SPI driver on %s",
                 spi_device_path_.c_str());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
        CallbackReturn::FAILURE;
  }

  // Create publishers
  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom/unfiltered", 10);
  joint_state_pub_ =
      create_publisher<sensor_msgs::msg::JointState>("joint_states", 10);
  battery_pub_ =
      create_publisher<sensor_msgs::msg::BatteryState>("battery_state", 10);

  // Create subscription
  cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10,
      std::bind(&StarSpiDriverNode::cmd_vel_callback, this,
                std::placeholders::_1));

  // Initialize state
  last_cmd_vel_time_ = get_clock()->now();

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
      CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_activate(const rclcpp_lifecycle::State &) {
  RCLCPP_INFO(get_logger(), "Activating StarSpiDriverNode...");

  odom_pub_->on_activate();
  joint_state_pub_->on_activate();
  battery_pub_->on_activate();

  // Start 100 Hz timer (10ms)
  timer_ = create_wall_timer(
      10ms, std::bind(&StarSpiDriverNode::timer_callback, this));

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
      CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_deactivate(const rclcpp_lifecycle::State &) {
  RCLCPP_INFO(get_logger(), "Deactivating StarSpiDriverNode...");

  // Stop timer
  timer_.reset();

  // Send zero velocity for safety
  // Construct zero command
  star::v1::VelocityCommand cmd;
  cmd.set_front_left_velocity_mps(0.0f);
  cmd.set_front_right_velocity_mps(0.0f);
  cmd.set_back_left_velocity_mps(0.0f);
  cmd.set_back_right_velocity_mps(0.0f);

  // TODO(safety): Send final zero-velocity frame before deactivation

  odom_pub_->on_deactivate();
  joint_state_pub_->on_deactivate();
  battery_pub_->on_deactivate();

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
      CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_cleanup(const rclcpp_lifecycle::State &) {
  RCLCPP_INFO(get_logger(), "Cleaning up StarSpiDriverNode...");

  spi_driver_->close_device();
  spi_driver_.reset();
  converter_.reset();

  odom_pub_.reset();
  joint_state_pub_.reset();
  battery_pub_.reset();
  cmd_vel_sub_.reset();

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
      CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_shutdown(const rclcpp_lifecycle::State &prev_state) {
  RCLCPP_INFO(get_logger(), "Shutting down StarSpiDriverNode...");
  return on_cleanup(prev_state);
}

void StarSpiDriverNode::cmd_vel_callback(
    const geometry_msgs::msg::Twist::SharedPtr msg) {
  current_cmd_vel_ = *msg;
  last_cmd_vel_time_ = get_clock()->now();
}

void StarSpiDriverNode::timer_callback() {
  // Check for command velocity timeout (watchdog safety)
  auto now = get_clock()->now();
  double time_since_cmd = (now - last_cmd_vel_time_).seconds();

  geometry_msgs::msg::Twist cmd_vel_to_send;
  if (time_since_cmd > (cmd_vel_timeout_ms_ / 1000.0)) {
    // Timeout expired: send zero velocity for safety
  } else {
    cmd_vel_to_send = current_cmd_vel_;
  }

  // Convert Twist to protobuf VelocityCommand
  star::v1::VelocityCommand velocity_cmd;

  if (!converter_->twist_to_velocity_command(cmd_vel_to_send, velocity_cmd)) {
    RCLCPP_WARN(get_logger(),
                "Failed to convert Twist to VelocityCommand (NaN/Inf?)");
    // Send zero if conversion fails
    velocity_cmd.set_front_left_velocity_mps(0.0f);
    velocity_cmd.set_front_right_velocity_mps(0.0f);
    velocity_cmd.set_back_left_velocity_mps(0.0f);
    velocity_cmd.set_back_right_velocity_mps(0.0f);
  }

  // Encode protobuf payload into SPI frame
  std::vector<uint8_t> payload(velocity_cmd.ByteSizeLong());
  velocity_cmd.SerializeToArray(payload.data(), payload.size());

  FrameType frame_type = FrameType::VelocityCommand;
  uint8_t flags = 0x00;

  std::vector<uint8_t> tx_frame;
  SpiDriver::encode_frame(tx_seq_++, frame_type, flags, payload, tx_frame);

  // Perform full-duplex SPI transfer
  std::vector<uint8_t> rx_frame;
  if (!spi_driver_->transfer(tx_frame, rx_frame)) {
    RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 1000,
                          "SPI Transfer Failed");
    return;
  }

  // Decode received frame
  uint16_t decoded_seq_;
  FrameType rx_type;
  uint8_t decoded_flags_;
  std::vector<uint8_t> rx_payload;

  if (!SpiDriver::decode_frame(rx_frame, decoded_seq_, rx_type, decoded_flags_,
                               rx_payload)) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                         "SPI Frame Decode Failed (CRC mismatch or garbage)");
    return;
  }

  // Parse telemetry and publish ROS2 messages
  if (rx_type == FrameType::TelemetryData) {
    star::v1::TelemetryData telemetry;
    if (telemetry.ParseFromArray(rx_payload.data(), rx_payload.size())) {
      // Check emergency stop flag from motor controller
      if (telemetry.emergency_stop()) {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 1000,
                              "RX72N EMERGENCY STOP ACTIVE");
      }

      // Publish Odometry
      nav_msgs::msg::Odometry odom;
      odom.header.stamp = now;
      converter_->telemetry_to_odometry(telemetry, odom);
      odom_pub_->publish(odom);

      // Publish Joint States
      sensor_msgs::msg::JointState joint_state;
      joint_state.header.stamp = now;
      converter_->telemetry_to_joint_state(telemetry, joint_state);
      joint_state_pub_->publish(joint_state);

      // Publish Battery State
      sensor_msgs::msg::BatteryState battery_state;
      battery_state.header.stamp = now;
      converter_->telemetry_to_battery_state(telemetry, battery_state);
      battery_pub_->publish(battery_state);
    } else {
      RCLCPP_WARN(get_logger(), "Failed to parse TelemetryData protobuf");
    }
  }
}

} // namespace star_spi_bridge
