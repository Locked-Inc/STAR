// message_converter.cpp - ROS2 <-> Protobuf Message Converter Implementation
// Bidirectional conversion between ROS2 standard messages and STAR Protocol
// Buffers.
//
// STAR Project - Texas A&M University
// Copyright 2026 STAR Project
// January 2026

#include "star_gateway_bridge/message_converter.hpp"

#include <chrono>
namespace star
{

// ===========================================================================
// ROS2 -> Protobuf Conversions
// ===========================================================================

bool MessageConverter::twist_to_velocity_command(
  const geometry_msgs::msg::Twist & twist, star::v1::VelocityCommand & command,
  double wheel_base, uint32_t sequence)
{
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

  // Differential drive kinematics: (linear, angular) -> (left, right)
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

bool MessageConverter::string_to_system_status(
  const std_msgs::msg::String & status_msg,
  star::v1::SystemStatus & system_status)
{
  // For now, implement basic string parsing
  // In production, use a JSON library (e.g., nlohmann/json) for robust parsing
  // This is a placeholder implementation

  const std::string & data = status_msg.data;

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

void MessageConverter::odometry_to_proto(
  const nav_msgs::msg::Odometry & ros_odom,
  star::v1::OdometryData & proto_odom)
{
  proto_odom.set_x_m(ros_odom.pose.pose.position.x);
  proto_odom.set_y_m(ros_odom.pose.pose.position.y);

  // Extract yaw from quaternion (no tf2 dependency needed for 2D)
  const auto & q = ros_odom.pose.pose.orientation;
  const double yaw = std::atan2(
    2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  proto_odom.set_theta_rad(yaw);

  proto_odom.set_linear_velocity_mps(ros_odom.twist.twist.linear.x);
  proto_odom.set_angular_velocity_rad_per_s(ros_odom.twist.twist.angular.z);

  // Timestamp: ROS stamp -> microseconds
  const int64_t ts_us =
    static_cast<int64_t>(ros_odom.header.stamp.sec) * 1'000'000LL +
    static_cast<int64_t>(ros_odom.header.stamp.nanosec) / 1'000LL;
  proto_odom.set_timestamp_us(ts_us);
}

void MessageConverter::slam_pose_to_proto(
  const geometry_msgs::msg::PoseWithCovarianceStamped & slam_pose,
  star::v1::OdometryData & proto_odom)
{
  proto_odom.set_x_m(slam_pose.pose.pose.position.x);
  proto_odom.set_y_m(slam_pose.pose.pose.position.y);

  // Extract yaw from quaternion (same inline formula as odometry_to_proto)
  const auto & q = slam_pose.pose.pose.orientation;
  const double yaw = std::atan2(
    2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  proto_odom.set_theta_rad(yaw);

  // SLAM pose has no twist; zero these out so UI can detect it
  proto_odom.set_linear_velocity_mps(0.0);
  proto_odom.set_angular_velocity_rad_per_s(0.0);

  // Timestamp: ROS stamp -> microseconds
  const int64_t ts_us =
    static_cast<int64_t>(slam_pose.header.stamp.sec) * 1'000'000LL +
    static_cast<int64_t>(slam_pose.header.stamp.nanosec) / 1'000LL;
  proto_odom.set_timestamp_us(ts_us);
}

/** @brief Maximum number of LiDAR samples forwarded per scan frame.
 *
 *  @details Caps the size of LidarScan protobuf messages to bound bandwidth
 *  and UI rendering cost. The laserscan_to_proto() converter downsamples
 *  evenly when the raw scan exceeds this limit.
 */
static constexpr int kMaxLidarSamples = 500;

void MessageConverter::laserscan_to_proto(
  const sensor_msgs::msg::LaserScan & ros_scan,
  star::v1::LidarScan & proto_scan)
{
  proto_scan.Clear();

  const size_t total = ros_scan.ranges.size();
  if (total == 0) {return;}

  // Compute stride so we emit <= kMaxLidarSamples evenly-spaced points
  const size_t stride =
    (total + static_cast<size_t>(kMaxLidarSamples) - 1) /
    static_cast<size_t>(kMaxLidarSamples);

  for (size_t i = 0; i < total; i += stride) {
    const float range = ros_scan.ranges[i];
    // Skip invalid readings (NaN, inf, or out-of-range)
    if (!std::isfinite(range) || range < ros_scan.range_min ||
      range > ros_scan.range_max)
    {
      proto_scan.add_angle_rad(0.0f);
      proto_scan.add_range_m(0.0f);   // 0 = invalid per proto convention
      proto_scan.add_intensity(0.0f);
    } else {
      const float angle =
        ros_scan.angle_min + static_cast<float>(i) * ros_scan.angle_increment;
      proto_scan.add_angle_rad(angle);
      proto_scan.add_range_m(range);
      proto_scan.add_intensity(
        i < ros_scan.intensities.size() ? ros_scan.intensities[i] : 0.0f);
    }
  }

  const int64_t ts_us =
    static_cast<int64_t>(ros_scan.header.stamp.sec) * 1'000'000LL +
    static_cast<int64_t>(ros_scan.header.stamp.nanosec) / 1'000LL;
  proto_scan.set_timestamp_us(ts_us);
}

// ===========================================================================
// Protobuf -> ROS2 Conversions
// ===========================================================================

bool MessageConverter::velocity_command_to_twist(
  const star::v1::VelocityCommand & command, geometry_msgs::msg::Twist & twist,
  double wheel_base)
{
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

  // Differential drive inverse kinematics: (left, right) -> (linear, angular)
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
  const star::v1::PidConfig & pid_config, double & kp, double & ki, double & kd)
{
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

int64_t MessageConverter::ros_time_to_us(const rclcpp::Time & time)
{
  return time.nanoseconds() / 1000;
}

} // namespace star
