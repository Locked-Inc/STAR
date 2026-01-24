// message_converter.cpp - ROS2 ↔ Protobuf Message Converter Implementation
// Bidirectional conversion between ROS2 standard messages and STAR Protocol
// Buffers.
//
// STAR Project - Texas A&M University
// Copyright 2026 STAR Project
// January 2026

#include "star_gateway_bridge/message_converter.hpp"

#include <chrono>

#include "rclcpp/rclcpp.hpp"

namespace star {

// ===========================================================================
// ROS2 → Protobuf Conversions
// ===========================================================================

bool MessageConverter::twist_to_velocity_command(
    const geometry_msgs::msg::Twist &twist, star::v1::VelocityCommand &command,
    double wheel_base, uint32_t sequence) {
  // Validate inputs for NaN/infinity
  if (!is_valid_double(twist.linear.x) || !is_valid_double(twist.angular.z)) {
    RCLCPP_WARN(rclcpp::get_logger("message_converter"),
                "Invalid Twist: NaN/infinity in linear.x or angular.z");
    return false;
  }

  if (!is_valid_double(wheel_base) || wheel_base <= 0.0) {
    RCLCPP_ERROR(rclcpp::get_logger("message_converter"),
                 "Invalid wheel_base: must be positive and finite");
    return false;
  }

  // Clamp input velocities to safe ranges
  double linear =
      clamp(twist.linear.x, -k_max_velocity_mps, k_max_velocity_mps);
  double angular =
      clamp(twist.angular.z, -k_max_angular_vel, k_max_angular_vel);

  // Differential drive kinematics: (linear, angular) → (left, right)
  // left_vel = linear - (angular * wheel_base / 2)
  // right_vel = linear + (angular * wheel_base / 2)
  double half_base = wheel_base / 2.0;
  double left_velocity = linear - (angular * half_base);
  double right_velocity = linear + (angular * half_base);

  // Clamp wheel velocities to VelocityCommand valid range [-2.0, 2.0] m/s
  left_velocity = clamp(left_velocity, -k_max_velocity_mps, k_max_velocity_mps);
  right_velocity =
      clamp(right_velocity, -k_max_velocity_mps, k_max_velocity_mps);

  // Populate protobuf message
  // Note: front_left/back_left = left side, front_right/back_right = right side
  // for differential drive
  command.set_front_left_velocity_mps(left_velocity);
  command.set_back_left_velocity_mps(left_velocity);
  command.set_front_right_velocity_mps(right_velocity);
  command.set_back_right_velocity_mps(right_velocity);
  command.set_sequence(sequence);

  // Timestamp in microseconds since epoch
  auto now = std::chrono::system_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count();
  command.set_timestamp_us(us);

  return true;
}

bool MessageConverter::battery_state_to_proto(
    const sensor_msgs::msg::BatteryState &ros_battery,
    star::v1::BatteryState &proto_battery) {
  // ROS2 sensor_msgs/BatteryState uses NaN to indicate "unknown" values
  // We need to check each field before conversion

  // Voltage: V → mV (check for valid value first)
  if (is_valid_double(ros_battery.voltage)) {
    auto *current_data = proto_battery.mutable_current();
    current_data->set_voltage_mv(
        static_cast<uint32_t>(ros_battery.voltage * k_v_to_mv));
  }

  // Current: A → mA (positive = charging, negative = discharging)
  if (is_valid_double(ros_battery.current)) {
    auto *current_data = proto_battery.mutable_current();
    current_data->set_current_ma(
        static_cast<int32_t>(ros_battery.current * k_a_to_ma));
  }

  // Capacity: Ah → mAh
  if (is_valid_double(ros_battery.capacity)) {
    auto *soc_data = proto_battery.mutable_soc();
    soc_data->set_full_capacity_mah(
        static_cast<uint32_t>(ros_battery.capacity * k_ah_to_mah));
  }

  if (is_valid_double(ros_battery.design_capacity)) {
    auto *soc_data = proto_battery.mutable_soc();
    soc_data->set_design_capacity_mah(
        static_cast<uint32_t>(ros_battery.design_capacity * k_ah_to_mah));
  }

  if (is_valid_double(ros_battery.charge)) {
    auto *soc_data = proto_battery.mutable_soc();
    soc_data->set_remaining_capacity_mah(
        static_cast<uint32_t>(ros_battery.charge * k_ah_to_mah));
  }

  // Percentage: 0-1 → 0-100%
  if (is_valid_double(ros_battery.percentage)) {
    auto *soc_data = proto_battery.mutable_soc();
    soc_data->set_relative_soc_percent(
        static_cast<int32_t>(ros_battery.percentage * k_percent_to_int));
  }

  // Temperature: °C → deci-Celsius (first temperature sensor only)
  if (!ros_battery.cell_temperature.empty()) {
    float temp_c = ros_battery.cell_temperature[0];
    if (is_valid_double(temp_c)) {
      auto *temp_data = proto_battery.mutable_temperatures();
      temp_data->add_temp_deci_celsius(
          static_cast<int32_t>(temp_c * k_c_to_decic));
      temp_data->set_valid_sensors(1);
      temp_data->set_avg_temp_deci_celsius(
          static_cast<int32_t>(temp_c * k_c_to_decic));
    }
  }

  // Power state mapping (ROS2 → Protobuf enum)
  auto *status = proto_battery.mutable_status();
  switch (ros_battery.power_supply_status) {
  case sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_CHARGING:
    status->set_state(star::v1::BATTERY_STATE_ENUM_CHARGING);
    status->set_charging(true);
    status->set_discharging(false);
    break;
  case sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_DISCHARGING:
    status->set_state(star::v1::BATTERY_STATE_ENUM_DISCHARGING);
    status->set_charging(false);
    status->set_discharging(true);
    break;
  case sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_FULL:
    status->set_state(star::v1::BATTERY_STATE_ENUM_FULL);
    status->set_fully_charged(true);
    break;
  case sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_NOT_CHARGING:
    status->set_state(star::v1::BATTERY_STATE_ENUM_IDLE);
    break;
  default:
    status->set_state(star::v1::BATTERY_STATE_ENUM_UNKNOWN);
    break;
  }

  // Timestamp in microseconds
  auto now = std::chrono::system_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count();
  proto_battery.set_timestamp_us(us);

  return true;
}

bool MessageConverter::string_to_system_status(
    const std_msgs::msg::String &status_msg,
    star::v1::SystemStatus &system_status) {
  // For now, implement basic string parsing
  // In production, use a JSON library (e.g., nlohmann/json) for robust parsing
  // This is a placeholder implementation

  const std::string &data = status_msg.data;

  // Simple keyword-based parsing (not robust, but sufficient for MVP)
  if (data.find("MANUAL") != std::string::npos) {
    system_status.set_mode(star::v1::ROBOT_MODE_MANUAL);
  } else if (data.find("AUTONOMOUS") != std::string::npos) {
    system_status.set_mode(star::v1::ROBOT_MODE_AUTONOMOUS);
  } else if (data.find("MAPPING") != std::string::npos) {
    system_status.set_mode(star::v1::ROBOT_MODE_MAPPING);
  } else if (data.find("EMERGENCY_STOP") != std::string::npos) {
    system_status.set_mode(star::v1::ROBOT_MODE_EMERGENCY_STOP);
  } else {
    system_status.set_mode(star::v1::ROBOT_MODE_IDLE);
  }

  // Connection status (default to CONNECTED if we're receiving messages)
  system_status.set_connection_status(star::v1::CONNECTION_STATUS_CONNECTED);

  // TODO(star): Parse additional fields from JSON when available
  // For now, assume all subsystems are connected
  system_status.set_rx72n_connected(true);
  system_status.set_lidar_connected(true);
  system_status.set_ros_connected(true);

  return true;
}

// ===========================================================================
// Protobuf → ROS2 Conversions
// ===========================================================================

bool MessageConverter::velocity_command_to_twist(
    const star::v1::VelocityCommand &command, geometry_msgs::msg::Twist &twist,
    double wheel_base) {
  // Validate protobuf inputs
  // Note: front_left/back_left = left side, front_right/back_right = right side
  // for differential drive Average left/right side velocities for differential
  // drive
  double left_vel =
      (command.front_left_velocity_mps() + command.back_left_velocity_mps()) /
      2.0;
  double right_vel =
      (command.front_right_velocity_mps() + command.back_right_velocity_mps()) /
      2.0;

  if (!is_valid_double(left_vel) || !is_valid_double(right_vel)) {
    RCLCPP_WARN(rclcpp::get_logger("message_converter"),
                "Invalid VelocityCommand: NaN/infinity in wheel velocities");
    return false;
  }

  if (!is_valid_double(wheel_base) || wheel_base <= 0.0) {
    RCLCPP_ERROR(rclcpp::get_logger("message_converter"),
                 "Invalid wheel_base: must be positive and finite");
    return false;
  }

  // Differential drive inverse kinematics: (left, right) → (linear, angular)
  // linear = (left + right) / 2
  // angular = (right - left) / wheel_base
  double linear_x = (left_vel + right_vel) / 2.0;
  double angular_z = (right_vel - left_vel) / wheel_base;

  // Populate ROS2 Twist message
  twist.linear.x = linear_x;
  twist.linear.y = 0.0;
  twist.linear.z = 0.0;

  twist.angular.x = 0.0;
  twist.angular.y = 0.0;
  twist.angular.z = angular_z;

  return true;
}

bool MessageConverter::pid_config_to_gains(
    const star::v1::PidConfig &pid_config, double &kp, double &ki, double &kd) {
  // Extract gains from protobuf (no unit conversion needed)
  kp = pid_config.kp();
  ki = pid_config.ki();
  kd = pid_config.kd();

  // Validate gains are finite
  if (!is_valid_double(kp) || !is_valid_double(ki) || !is_valid_double(kd)) {
    RCLCPP_WARN(rclcpp::get_logger("message_converter"),
                "Invalid PidConfig: NaN/infinity in gains");
    return false;
  }

  return true;
}

// ===========================================================================
// Utility Functions
// ===========================================================================

int64_t MessageConverter::ros_time_to_us(const rclcpp::Time &time) {
  return time.nanoseconds() / 1000;
}

} // namespace star
