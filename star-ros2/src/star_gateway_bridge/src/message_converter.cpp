/**
 * @file message_converter.cpp
 * @brief Bidirectional conversion between ROS2 standard messages and STAR Protocol Buffers.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_gateway_bridge/message_converter.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

namespace star::star_gateway_bridge
{

namespace
{

/** @brief Microseconds in one second for ROS timestamp conversion. */
constexpr int64_t US_PER_SEC = 1'000'000LL;
/** @brief Nanoseconds in one microsecond for ROS timestamp conversion. */
constexpr int64_t NS_PER_US = 1'000LL;
/** @brief Maximum valid nanoseconds in a ROS2 timestamp. */
constexpr uint32_t MAX_NANOSECONDS = 1'000'000'000U;
/** @brief Pi constant for portable use in place of M_PI. */
constexpr double PI = 3.14159265358979323846;

/**
 * @brief Convert ROS builtin time stamp to microseconds since epoch.
 * @param[in] stamp ROS time stamp with sec + nanosec fields.
 * @return Timestamp in microseconds since epoch.
 * @pre stamp.sec >= 0.
 * @pre stamp.nanosec < MAX_NANOSECONDS.
 * @post Return value is non-negative.
 * @post Return value is finite and representable in int64_t.
 */
inline int64_t ros_stamp_to_us(const builtin_interfaces::msg::Time & stamp)
{
  assert(stamp.sec >= 0);
  assert(stamp.nanosec < MAX_NANOSECONDS);
  const int64_t timestamp_us = static_cast<int64_t>(stamp.sec) * US_PER_SEC +
    static_cast<int64_t>(stamp.nanosec) / NS_PER_US;
  assert(timestamp_us >= 0);
  return timestamp_us;
}

/**
 * @brief Convert quaternion orientation to planar yaw angle.
 * @param[in] q Quaternion orientation (w, x, y, z).
 * @return Yaw angle in radians in [-pi, pi], or 0.0 for invalid input.
 * @pre q.w, q.x, q.y, q.z are finite values.
 * @pre Quaternion represents a valid orientation input.
 * @post Return value is finite.
 * @post Function has no side effects.
 */
inline double quaternion_to_yaw_2d(const geometry_msgs::msg::Quaternion & q)
{
  if (!std::isfinite(q.w) || !std::isfinite(q.x) || !std::isfinite(q.y) ||
    !std::isfinite(q.z))
  {
    return 0.0;
  }

  const double yaw = std::atan2(2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));

  const double safe_yaw = std::isfinite(yaw) ? yaw : 0.0;
  return std::clamp(safe_yaw, -PI, PI);
}

}  // namespace

// ===========================================================================
// ROS2 -> Protobuf Conversions
// ===========================================================================

bool MessageConverter::twist_to_velocity_command(
  const geometry_msgs::msg::Twist & twist,
  ::star::v1::VelocityCommand & command, double wheel_base,
  uint32_t sequence)
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
    clamp(twist.linear.x, -MAX_VELOCITY_MPS, MAX_VELOCITY_MPS);
  double angular =
    clamp(twist.angular.z, -MAX_ANGULAR_VEL, MAX_ANGULAR_VEL);

  // Differential drive kinematics: (linear, angular) -> (left, right)
  // left_vel = linear - (angular * wheel_base / 2)
  // right_vel = linear + (angular * wheel_base / 2)
  double half_base = wheel_base / 2.0;
  double left_velocity = linear - (angular * half_base);
  double right_velocity = linear + (angular * half_base);

  // Clamp wheel velocities to VelocityCommand valid range [-2.0, 2.0] m/s
  left_velocity = clamp(left_velocity, -MAX_VELOCITY_MPS, MAX_VELOCITY_MPS);
  right_velocity =
    clamp(right_velocity, -MAX_VELOCITY_MPS, MAX_VELOCITY_MPS);

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
  ::star::v1::SystemStatus & system_status)
{
  // For now, implement basic string parsing
  // In production, use a JSON library (e.g., nlohmann/json) for robust parsing
  // This is a placeholder implementation

  const std::string & data = status_msg.data;

  // Simple keyword-based parsing (not robust, but sufficient for MVP)
  if (data.find("MANUAL") != std::string::npos) {
    system_status.set_mode(::star::v1::ROBOT_MODE_MANUAL);
  } else if (data.find("AUTONOMOUS") != std::string::npos) {
    system_status.set_mode(::star::v1::ROBOT_MODE_AUTONOMOUS);
  } else if (data.find("MAPPING") != std::string::npos) {
    system_status.set_mode(::star::v1::ROBOT_MODE_MAPPING);
  } else if (data.find("EMERGENCY_STOP") != std::string::npos) {
    system_status.set_mode(::star::v1::ROBOT_MODE_EMERGENCY_STOP);
  } else {
    system_status.set_mode(::star::v1::ROBOT_MODE_IDLE);
  }

  // Connection status (default to CONNECTED if we're receiving messages)
  system_status.set_connection_status(::star::v1::CONNECTION_STATUS_CONNECTED);

  // JSON parsing of additional fields is not yet implemented; a keyword-based
  // fallback is used for MVP. Tracked for future improvement.
  // For now, assume all subsystems are connected
  system_status.set_rx72n_connected(true);
  system_status.set_lidar_connected(true);
  system_status.set_ros_connected(true);

  return true;
}

/**
 * @brief Convert nav_msgs/Odometry to star::v1::OdometryData.
 *
 * @details
 * Converts EKF-filtered odometry into STAR odometry telemetry. Position and
 * velocity values are sanitized so non-finite inputs are written as 0.0.
 * Orientation quaternion is reduced to 2D yaw (radians) via atan2, with
 * non-finite quaternion components producing yaw=0.0.
 *
 * Units:
 * - Position: meters
 * - Orientation: radians
 * - Velocity: meters/sec and radians/sec
 * - Timestamp: microseconds since epoch
 *
 * @param[in] ros_odom Incoming ROS odometry message.
 * @param[out] proto_odom Output protobuf odometry message.
 *
 * @pre ros_odom.header.stamp.sec >= 0.
 * @pre ros_odom.header.stamp.nanosec < 1_000_000_000.
 * @post proto_odom timestamp_us is populated from ROS header stamp.
 * @post proto_odom pose/velocity fields are finite (non-finite inputs become
 * 0.0).
 *
 * @note Thread-safe and stateless; no shared data is accessed.
 */
void MessageConverter::odometry_to_proto(
  const nav_msgs::msg::Odometry & ros_odom,
  ::star::v1::OdometryData & proto_odom)
{
  assert(ros_odom.header.stamp.sec >= 0);
  assert(ros_odom.header.stamp.nanosec < MAX_NANOSECONDS);

  // Sanitize position: replace non-finite values with 0.0
  const double x = std::isfinite(ros_odom.pose.pose.position.x) ?
    ros_odom.pose.pose.position.x :
    0.0;
  const double y = std::isfinite(ros_odom.pose.pose.position.y) ?
    ros_odom.pose.pose.position.y :
    0.0;

  // Extract yaw from quaternion.
  const double yaw = quaternion_to_yaw_2d(ros_odom.pose.pose.orientation);

  // Sanitize velocities: replace non-finite values with 0.0
  const double lin = std::isfinite(ros_odom.twist.twist.linear.x) ?
    ros_odom.twist.twist.linear.x :
    0.0;
  const double ang = std::isfinite(ros_odom.twist.twist.angular.z) ?
    ros_odom.twist.twist.angular.z :
    0.0;

  proto_odom.set_x_m(x);
  proto_odom.set_y_m(y);
  proto_odom.set_theta_rad(yaw);
  proto_odom.set_linear_velocity_mps(lin);
  proto_odom.set_angular_velocity_rad_per_s(ang);

  // Timestamp: ROS stamp -> microseconds
  const int64_t ts_us = ros_stamp_to_us(ros_odom.header.stamp);
  proto_odom.set_timestamp_us(ts_us);

  assert(std::isfinite(proto_odom.x_m()));
  assert(std::isfinite(proto_odom.y_m()));
  assert(std::isfinite(proto_odom.theta_rad()));
  assert(std::isfinite(proto_odom.linear_velocity_mps()));
  assert(std::isfinite(proto_odom.angular_velocity_rad_per_s()));
}

/**
 * @brief Convert SLAM pose to star::v1::OdometryData.
 *
 * @details
 * Converts slam_toolbox map-frame pose into STAR odometry telemetry. Position
 * and orientation are sanitized so non-finite inputs are replaced by 0.0.
 * Timestamp is converted to microseconds since epoch. Twist is not available
 * in PoseWithCovarianceStamped, so linear and angular velocity are set to 0.0.
 *
 * Units:
 * - Position: meters
 * - Orientation: radians (yaw)
 * - Timestamp: microseconds since epoch
 *
 * @param[in] slam_pose Incoming SLAM pose with covariance.
 * @param[out] proto_odom Output protobuf odometry message.
 *
 * @pre slam_pose.header.stamp.sec >= 0.
 * @pre slam_pose.header.stamp.nanosec < 1_000_000_000.
 * @post proto_odom pose and timestamp fields are populated.
 * @post proto_odom velocities are set to 0.0.
 *
 * @note Thread-safe and stateless; no shared data is accessed.
 */
void MessageConverter::slam_pose_to_proto(
  const geometry_msgs::msg::PoseWithCovarianceStamped & slam_pose,
  ::star::v1::OdometryData & proto_odom)
{
  assert(slam_pose.header.stamp.sec >= 0);
  assert(slam_pose.header.stamp.nanosec < MAX_NANOSECONDS);

  // Sanitize position: replace non-finite values with 0.0
  const double x = std::isfinite(slam_pose.pose.pose.position.x) ?
    slam_pose.pose.pose.position.x :
    0.0;
  const double y = std::isfinite(slam_pose.pose.pose.position.y) ?
    slam_pose.pose.pose.position.y :
    0.0;

  // Extract yaw from quaternion.
  const double yaw = quaternion_to_yaw_2d(slam_pose.pose.pose.orientation);

  proto_odom.set_x_m(x);
  proto_odom.set_y_m(y);
  proto_odom.set_theta_rad(yaw);

  // SLAM pose has no twist; zero these out so UI can detect it
  proto_odom.set_linear_velocity_mps(0.0);
  proto_odom.set_angular_velocity_rad_per_s(0.0);

  // Timestamp: ROS stamp -> microseconds
  const int64_t ts_us = ros_stamp_to_us(slam_pose.header.stamp);
  proto_odom.set_timestamp_us(ts_us);

  assert(std::isfinite(proto_odom.x_m()));
  assert(std::isfinite(proto_odom.y_m()));
  assert(std::isfinite(proto_odom.theta_rad()));
}

/**
 * @brief Convert sensor_msgs/LaserScan to star::v1::LidarScan.
 *
 * @details
 * Downsamples ROS LaserScan to a bounded number of samples for telemetry.
 * Sample stride is computed so output contains at most MAX_LIDAR_SAMPLES.
 * Invalid readings (non-finite or out of [range_min, range_max]) are encoded
 * as zeros per STAR proto convention. Timestamp is converted to microseconds.
 *
 * @param[in] ros_scan Incoming ROS LaserScan frame.
 * @param[out] proto_scan Output protobuf scan message.
 *
 * @pre ros_scan.ranges is non-empty.
 * @pre ros_scan metadata is valid: finite angle_increment and range_min <=
 * range_max.
 * @post proto_scan contains at most MAX_LIDAR_SAMPLES entries.
 * @post proto_scan.timestamp_us is set from ros_scan.header.stamp.
 *
 * @note Downsampling uses uniform stride: ceil(total_ranges /
 * MAX_LIDAR_SAMPLES). Invalid readings are encoded as zero
 * angle/range/intensity.
 */
bool MessageConverter::laserscan_to_proto(
  const sensor_msgs::msg::LaserScan & ros_scan,
  ::star::v1::LidarScan & proto_scan)
{
  proto_scan.Clear();

  const size_t total = ros_scan.ranges.size();

  // Validate scan metadata before processing
  if (total == 0 || !std::isfinite(ros_scan.angle_min) ||
    !std::isfinite(ros_scan.angle_increment) ||
    ros_scan.angle_increment == 0.0f || !std::isfinite(ros_scan.range_min) ||
    !std::isfinite(ros_scan.range_max) ||
    ros_scan.range_min > ros_scan.range_max)
  {
    RCLCPP_WARN(rclcpp::get_logger("message_converter"),
      "Invalid LaserScan metadata: angle_min=%f angle_increment=%f "
      "range_min=%f range_max=%f total=%zu",
      static_cast<double>(ros_scan.angle_min),
      static_cast<double>(ros_scan.angle_increment),
      static_cast<double>(ros_scan.range_min),
      static_cast<double>(ros_scan.range_max), total);
    return false;
  }

  // Compute stride so we emit <= MAX_LIDAR_SAMPLES evenly-spaced points
  const size_t stride = (total + static_cast<size_t>(MAX_LIDAR_SAMPLES) - 1) /
    static_cast<size_t>(MAX_LIDAR_SAMPLES);

  for (size_t i = 0; i < total; i += stride) {
    const float range = ros_scan.ranges[i];
    // Skip invalid readings (NaN, inf, or out-of-range)
    if (!std::isfinite(range) || range < ros_scan.range_min ||
      range > ros_scan.range_max)
    {
      proto_scan.add_angle_rad(0.0f);
      proto_scan.add_range_m(0.0f);  // 0 = invalid per proto convention
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

  const int64_t ts_us = ros_stamp_to_us(ros_scan.header.stamp);
  proto_scan.set_timestamp_us(ts_us);

  assert(proto_scan.range_m_size() <= MAX_LIDAR_SAMPLES);
  assert(proto_scan.timestamp_us() >= 0);
  return true;
}

// ===========================================================================
// Protobuf -> ROS2 Conversions
// ===========================================================================

bool MessageConverter::velocity_command_to_twist(
  const ::star::v1::VelocityCommand & command,
  geometry_msgs::msg::Twist & twist, double wheel_base)
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
  const ::star::v1::PidConfig & pid_config, double & kp, double & ki,
  double & kd)
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

/**
 * @brief Convert a rclcpp::Time timestamp to microseconds since epoch.
 *
 * @details
 * Converts the nanosecond representation returned by
 * rclcpp::Time::nanoseconds() to microseconds by dividing by NS_PER_US
 * (1000). Precision loss of up to 0.999 us is acceptable for telemetry
 * timestamping purposes.
 *
 * @param[in] time ROS2 timestamp to convert.
 * @return Microseconds since epoch (int64_t).
 *
 * @pre  time is a valid, initialized rclcpp::Time.
 * @pre  time.nanoseconds() is representable in int64_t.
 * @post Return value is non-negative for times after the ROS epoch (i.e.
 *       stamps with sec >= 0).
 * @post Return value equals time.nanoseconds() / NS_PER_US.
 *
 * @note Thread-safe; stateless pure function with no side effects.
 * @since Version 1.0.0
 */
int64_t MessageConverter::ros_time_to_us(const rclcpp::Time & time)
{
  assert(time.nanoseconds() >= 0);
  assert(NS_PER_US > 0);
  return time.nanoseconds() / NS_PER_US;
}

/**
 * @brief Convert a single HC-SR04 ultrasonic distance reading to a
 * sensor_msgs/Range message.
 *
 * @details
 * Populates all fields of the output Range message with HC-SR04 hardware
 * parameters. A distance of 0.0 m (firmware reports this when the sensor echo
 * times out) is treated as "no echo" and mapped to max_range so Nav2 costmap
 * layers interpret it as free space rather than a zero-distance obstacle.
 * Non-finite or out-of-[min_range, max_range] values are clamped before
 * writing.
 *
 * HC-SR04 hardware parameters applied:
 * - radiation_type: ULTRASOUND (1)
 * - field_of_view: 0.2618 rad (~15 degrees full cone angle)
 * - min_range: 0.02 m (2 cm blind zone)
 * - max_range: 4.00 m (nominal maximum echo distance)
 *
 * @param[in]  distance_m  Distance reading in metres. 0.0 signals a sensor
 *             timeout and is mapped to max_range.
 * @param[in]  frame_id    TF2 frame ID for this sensor.
 * @param[in]  stamp       ROS2 timestamp written into message header.
 * @param[out] out         Range message populated in-place.
 *
 * @pre  frame_id must be a non-empty ASCII string.
 * @pre  stamp must be a valid rclcpp::Time.
 * @post out.radiation_type == sensor_msgs::msg::Range::ULTRASOUND.
 * @post out.range is in [0.02, 4.00] m after clamping.
 *
 * @note Thread-safe; stateless pure function with no shared state.
 * @note HC-SR04 spec: measurement range 2 cm to 400 cm, ~15-degree cone FOV.
 *
 * @since Version 1.0.0
 */
void MessageConverter::obstacle_distance_to_range(
  float distance_m, const std::string & frame_id,
  const rclcpp::Time & stamp, sensor_msgs::msg::Range & out)
{
  assert(!frame_id.empty());

  static constexpr float HC_SR04_MIN_RANGE_M = 0.02F;
  static constexpr float HC_SR04_MAX_RANGE_M = 4.00F;
  static constexpr float HC_SR04_FOV_RAD = 0.2618F; // ~15 degrees

  out.header.frame_id = frame_id;
  out.header.stamp = stamp;
  out.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
  out.field_of_view = HC_SR04_FOV_RAD;
  out.min_range = HC_SR04_MIN_RANGE_M;
  out.max_range = HC_SR04_MAX_RANGE_M;

  // 0.0 m means the firmware received no echo (sensor timeout): treat as free.
  if (distance_m == 0.0F) {
    out.range = HC_SR04_MAX_RANGE_M;
    return;
  }

  // Clamp non-finite or out-of-spec values to valid range.
  if (!std::isfinite(distance_m)) {
    out.range = HC_SR04_MAX_RANGE_M;
    return;
  }

  out.range = std::max(HC_SR04_MIN_RANGE_M,
                       std::min(HC_SR04_MAX_RANGE_M, distance_m));
}

}  // namespace star::star_gateway_bridge
