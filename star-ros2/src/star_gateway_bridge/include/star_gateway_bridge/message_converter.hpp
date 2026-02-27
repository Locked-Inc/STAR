// message_converter.hpp - ROS2 <-> Protobuf Message Converter
// Bidirectional conversion between ROS2 standard messages and STAR Protocol
// Buffers.
//
// STAR Project - Texas A&M University
// January 2026

#ifndef STAR_GATEWAY_BRIDGE__MESSAGE_CONVERTER_HPP_
#define STAR_GATEWAY_BRIDGE__MESSAGE_CONVERTER_HPP_

#include <cmath>

#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_msgs/msg/string.hpp>

#include "star/v1/motor_control.pb.h"
#include "star/v1/telemetry.pb.h"
#include "star/v1/ui.pb.h"

namespace star
{

/**
 * @brief Message converter for ROS2 <-> Protobuf translation.
 *
 * Provides stateless conversion functions with input validation and unit
 * conversions. All conversions validate for NaN/infinity and clamp values to
 * safe ranges.
 */
class MessageConverter {
public:
  // ===========================================================================
  // ROS2 -> Protobuf Conversions
  // ===========================================================================

  /**
   * @brief Convert ROS2 Twist message to Protobuf VelocityCommand.
   *
   * Maps differential drive velocities from ROS2 standard Twist to STAR
   * VelocityCommand. Uses differential drive kinematics: (linear, angular) ->
   * (motor_0, motor_1) wheel velocities. Note: motor_0 = left wheel, motor_1 =
   * right wheel for differential drive.
   *
   * Conversions:
   * - Linear velocity (m/s) -> motor_0/motor_1 wheel velocities (m/s)
   * - Angular velocity (rad/s) -> Wheel velocity differential (m/s)
   * - Timestamp -> Microseconds since epoch
   *
   * Safety features:
   * - NaN/infinity validation (returns false on invalid input)
   * - Velocity clamping to +/-2.0 m/s (VelocityCommand valid range)
   *
   * @param twist ROS2 Twist message (differential drive command)
   * @param command Output VelocityCommand protobuf
   * @param wheel_base Distance between wheels in meters (default: 0.150m)
   * @param sequence Optional sequence number for command ordering
   * @return true if conversion successful, false if input validation failed
   */
  static bool twist_to_velocity_command(
    const geometry_msgs::msg::Twist & twist,
    star::v1::VelocityCommand & command,
    double wheel_base = 0.150,
    uint32_t sequence = 0);

  /**
   * @brief Convert ROS2 String message to Protobuf SystemStatus.
   *
   * Parses JSON-formatted robot status string into SystemStatus protobuf.
   * Expected format: {"mode": "MANUAL", "esp32": true, "lidar": true, ...}
   *
   * @param status_msg ROS2 String message (JSON format)
   * @param system_status Output SystemStatus protobuf
   * @return true if conversion successful, false if parse failed
   */
  static bool string_to_system_status(
    const std_msgs::msg::String & status_msg,
    star::v1::SystemStatus & system_status);

  /** Convert nav_msgs/Odometry -> star::v1::OdometryData. */
  static void odometry_to_proto(
    const nav_msgs::msg::Odometry & ros_odom,
    star::v1::OdometryData & proto_odom);

  /** Convert geometry_msgs/PoseWithCovarianceStamped (SLAM pose) -> star::v1::OdometryData. */
  static void slam_pose_to_proto(
    const geometry_msgs::msg::PoseWithCovarianceStamped & slam_pose,
    star::v1::OdometryData & proto_odom);

  /** Convert sensor_msgs/LaserScan -> star::v1::LidarScan (<=500 samples). */
  static void laserscan_to_proto(
    const sensor_msgs::msg::LaserScan & ros_scan,
    star::v1::LidarScan & proto_scan);

  // ===========================================================================
  // Protobuf -> ROS2 Conversions
  // ===========================================================================

  /**
   * @brief Convert Protobuf VelocityCommand to ROS2 Twist message.
   *
   * Maps STAR VelocityCommand (motor_0/motor_1 wheel velocities) to ROS2 Twist
   * (linear/angular velocities) using differential drive kinematics.
   * Note: motor_0 = left wheel, motor_1 = right wheel for differential drive.
   *
   * Conversions:
   * - motor_0/motor_1 wheel velocities (m/s) -> Linear velocity (m/s)
   * - Wheel velocity differential (m/s) -> Angular velocity (rad/s)
   *
   * Kinematics:
   * - linear.x = (motor_0 + motor_1) / 2
   * - angular.z = (motor_1 - motor_0) / wheel_base
   *
   * @param command Input VelocityCommand protobuf
   * @param twist Output ROS2 Twist message
   * @param wheel_base Distance between wheels in meters (default: 0.150m)
   * @return true if conversion successful, false if input validation failed
   */
  static bool
  velocity_command_to_twist(
    const star::v1::VelocityCommand & command,
    geometry_msgs::msg::Twist & twist,
    double wheel_base = 0.150);

  /**
   * @brief Convert Protobuf PidConfig to individual gains (for ROS2 service).
   *
   * Extracts PID gains from STAR PidConfig protobuf.
   * No unit conversion needed (all values are dimensionless gains).
   *
   * @param pid_config Input PidConfig protobuf
   * @param kp Output proportional gain
   * @param ki Output integral gain
   * @param kd Output derivative gain
   * @return true if conversion successful, false if input validation failed
   */
  static bool pid_config_to_gains(
    const star::v1::PidConfig & pid_config,
    double & kp, double & ki, double & kd);

  // ===========================================================================
  // Validation Helpers
  // ===========================================================================

  /**
   * @brief Validate double value for NaN and infinity.
   * @param value Value to validate
   * @return true if value is finite (not NaN, not infinity)
   */
  static bool is_valid_double(double value) {return std::isfinite(value);}

  /**
   * @brief Clamp value to range [min, max].
   * @param value Value to clamp
   * @param min Minimum allowed value
   * @param max Maximum allowed value
   * @return Clamped value
   */
  static double clamp(double value, double min, double max)
  {
    return std::max(min, std::min(max, value));
  }

  /**
   * @brief Convert ROS2 timestamp to microseconds since epoch.
   * @param time ROS2 time
   * @return Microseconds since epoch
   */
  static int64_t ros_time_to_us(const rclcpp::Time & time);

private:
  // Differential drive kinematics constants
  static constexpr double k_max_velocity_mps =
    2.0;   // VelocityCommand valid range
  static constexpr double k_max_angular_vel =
    4.0;   // Maximum angular velocity (rad/s)

};

} // namespace star

#endif // STAR_GATEWAY_BRIDGE__MESSAGE_CONVERTER_HPP_
