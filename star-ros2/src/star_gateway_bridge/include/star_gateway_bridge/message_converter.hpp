/**
 * @file message_converter.hpp
 * @brief Bidirectional conversion between ROS2 standard messages and STAR
 * Protocol Buffers.
 *
 * @details
 * Provides converter functions for translating between ROS2 message types and
 * the STAR protobuf schema used by the gateway service. Static methods are
 * stateless and thread-safe. BBB telemetry methods (telemetry_to_odometry,
 * telemetry_to_joint_state) hold dead-reckoning state and are NOT thread-safe.
 *
 * @note Static conversion functions are thread-safe. Instance BBB methods
 * must be called from a single thread (the ROS2 timer executor).
 *
 * @since Version 1.0.0

 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include <cmath>

#include <rclcpp/rclcpp.hpp>

#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>

#include "star/v1/motor_control.pb.h"
#include "star/v1/telemetry.pb.h"
#include "star/v1/ui.pb.h"

namespace star::star_gateway_bridge
{

/**
 * @brief Message converter for ROS2 <-> Protobuf translation.
 *
 * @details
 * Provides conversion functions with input validation and unit conversions.
 * Static methods (twist_to_velocity_command, odometry_to_proto, etc.) are
 * stateless and thread-safe. BBB telemetry methods (telemetry_to_odometry,
 * telemetry_to_joint_state) are stateful -- they hold dead-reckoning pose
 * and previous encoder tick baselines -- and are NOT thread-safe. Call
 * init_bbb_params() before using any BBB conversion method.
 *
 * @note BBB conversion methods mutate internal state and must be called from
 * a single thread (the ROS2 timer executor).
 *
 * @see StarGatewayBridgeNode  Owner of the converter instance.
 * @since Version 1.0.0
 */
class MessageConverter {
public:
  // ===========================================================================
  // Public Constants
  // ===========================================================================

  /**
   * @brief Default differential-drive wheel base (m) used when callers do not
   *        supply an explicit value to the conversion APIs.
   *
   * @details
   * Matches the STAR robot wheel track: 150 mm centre-to-centre between
   * left and right driven wheels.  Override at the call site when testing
   * with a different chassis.
   *
   * @note Thread-safe; compile-time constant, read-only.
   * @since Version 1.0.0
   */
  static constexpr double DEFAULT_WHEEL_BASE_M = 0.356;

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
   * @param wheel_base Distance between wheels in meters (default: 0.356m
   *                   for the goBILDA Wasteland chassis)
   * @param sequence Optional sequence number for command ordering
   * @return true if conversion successful, false if input validation failed
   */
  static bool twist_to_velocity_command(
    const geometry_msgs::msg::Twist & twist,
    ::star::v1::VelocityCommand & command,
    double wheel_base = DEFAULT_WHEEL_BASE_M,
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
    ::star::v1::SystemStatus & system_status);

  /**
   * @brief Convert nav_msgs/Odometry to star::v1::OdometryData.
   *
   * @details
   * Extracts 2-D pose (x, y, yaw) from the EKF-filtered odometry message and
   * maps linear/angular velocities directly. Yaw is derived from the quaternion
   * orientation using the standard atan2 formula for planar rotation.
   *
   * Timestamp is converted from ROS (sec + nanosec) to microseconds since
   * epoch.
   *
   * @param[in]  ros_odom   Incoming EKF-filtered odometry message.
   * @param[out] proto_odom Output OdometryData protobuf populated in-place.
   *
   * @pre  ros_odom quaternion is a valid unit quaternion (finite components).
   * @pre  ros_odom.header.stamp is valid (sec >= 0).
   * @post proto_odom fields x_m, y_m, theta_rad, linear_velocity_mps,
   *       angular_velocity_rad_per_s, and timestamp_us are all set.
   * @post Any non-finite position, quaternion, or velocity component is
   * replaced with 0.0 before writing to proto_odom.
   *
   * @note Thread-safe; does not access shared state.
   * @throw None.
   *
   * @see slam_pose_to_proto()  Alternative source for map-frame pose.
   *
   * @since Version 1.0.0
   */
  static void odometry_to_proto(
    const nav_msgs::msg::Odometry & ros_odom,
    ::star::v1::OdometryData & proto_odom);

  /**
   * @brief Convert geometry_msgs/PoseWithCovarianceStamped (SLAM pose) to
   * star::v1::OdometryData.
   *
   * @details
   * Extracts 2-D pose (x, y, yaw) from the slam_toolbox pose estimate. Because
   * SLAM pose messages do not carry velocity information, linear_velocity_mps
   * and angular_velocity_rad_per_s are zeroed so the UI can distinguish
   * SLAM-sourced data from EKF odometry.
   *
   * Timestamp is converted from ROS (sec + nanosec) to microseconds since
   * epoch.
   *
   * @param[in]  slam_pose  Incoming SLAM pose estimate (map frame).
   * @param[out] proto_odom Output OdometryData protobuf populated in-place.
   *
   * @pre  slam_pose quaternion is a valid unit quaternion.
   * @pre  slam_pose position and quaternion components are finite values.
   * @post proto_odom fields x_m, y_m, theta_rad, and timestamp_us are set.
   * @post proto_odom linear_velocity_mps == 0 and angular_velocity_rad_per_s ==
   * 0.
   *
   * @note Thread-safe; does not access shared state.
   * @note Telemetry arbitration between SLAM pose and EKF odometry is handled
   * by StarGatewayBridgeNode using cache timestamps and source priority.
   *
   * @throw None.
   *
   * @see odometry_to_proto()  EKF-filtered alternative that includes
   * velocities.
   *
   * @since Version 1.0.0
   */
  static void slam_pose_to_proto(
    const geometry_msgs::msg::PoseWithCovarianceStamped & slam_pose,
    ::star::v1::OdometryData & proto_odom);

  /**
   * @brief Convert sensor_msgs/LaserScan to star::v1::LidarScan (max
   * MAX_LIDAR_SAMPLES samples).
   *
   * @details
   * Downsamples the raw LaserScan to at most MAX_LIDAR_SAMPLES evenly-spaced
   * points using a stride computed as ceil(total / MAX_LIDAR_SAMPLES). Invalid
   * readings (NaN, infinity, or outside [range_min, range_max]) are encoded as
   * angle=0 / range=0 / intensity=0 per the proto convention so the UI can
   * identify them.
   *
   * Timestamp is converted from ROS (sec + nanosec) to microseconds since
   * epoch.
   *
   * @param[in]  ros_scan   Incoming LaserScan message from the LiDAR driver.
   * @param[out] proto_scan Output LidarScan protobuf cleared and populated
   * in-place.
   *
   * @pre  ros_scan.ranges is non-empty for any output to be generated.
   * @pre  ros_scan.angle_increment is finite and non-zero; range_min <=
   * range_max.
   * @post proto_scan contains at most MAX_LIDAR_SAMPLES samples.
   * @post proto_scan.timestamp_us reflects the ROS header stamp.
   *
   * @note Thread-safe; does not access shared state.
   * @note Missing intensity data (intensities.size() < ranges.size()) is filled
   *       with 0.0 without error.
   * @throw None.
   * @return true if conversion succeeds, false when metadata validation fails.
   *
   * @since Version 1.0.0
   */
  static bool laserscan_to_proto(
    const sensor_msgs::msg::LaserScan & ros_scan,
    ::star::v1::LidarScan & proto_scan);

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
   * @param wheel_base Distance between wheels in meters (default: 0.356m
   *                   for the goBILDA Wasteland chassis)
   * @return true if conversion successful, false if input validation failed
   */
  static bool
  velocity_command_to_twist(
    const ::star::v1::VelocityCommand & command,
    geometry_msgs::msg::Twist & twist,
    double wheel_base = DEFAULT_WHEEL_BASE_M);

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
    const ::star::v1::PidConfig & pid_config,
    double & kp, double & ki, double & kd);

  /**
   * @brief Convert a single HC-SR04 ultrasonic distance reading to a
   * sensor_msgs/Range message.
   *
   * @details
   * Populates a sensor_msgs::msg::Range message with HC-SR04 hardware
   * parameters (ULTRASOUND radiation type, 15-degree FOV, 0.02-4.00 m range)
   * and writes the supplied distance into the range field. A distance of 0.0
   * (reported by the firmware when the sensor echo timed out) is treated as
   * "no echo" and mapped to max_range so Nav2 costmap layers interpret it as
   * free space rather than a zero-distance obstacle.
   *
   * Non-finite or out-of-range distances are clamped to [min_range, max_range]
   * before writing.
   *
   * @param[in]  distance_m  Distance reading from the ObstacleData field, in
   *             metres. 0.0 indicates a sensor timeout (no echo); all other
   *             values are clamped to [0.02, 4.00] m.
   * @param[in]  frame_id    TF2 frame ID for this sensor (e.g.
   *             "obstacle_front_left"). Must be a non-empty ASCII string.
   * @param[in]  stamp       ROS2 timestamp to write into the message header.
   * @param[out] out         Range message populated in-place; all fields are
   *             overwritten by this function.
   *
   * @pre  frame_id must be a non-empty ASCII string.
   * @pre  stamp must be a valid rclcpp::Time (not Time(0) unless intentional).
   * @post out.radiation_type == sensor_msgs::msg::Range::ULTRASOUND.
   * @post out.range is in [0.02, 4.00] m after clamping.
   *
   * @note Thread-safe; stateless pure function with no shared state.
   * @note HC-SR04 spec: measurement range 2 cm to 400 cm, 15-degree cone.
   *
   * @see obstacle_poll_timer_callback() in StarGatewayBridgeNode, which calls
   *      this function four times per polling cycle (once per sensor).
   *
   * @since Version 1.0.0
   */
  static void obstacle_distance_to_range(
    float distance_m, const std::string & frame_id,
    const rclcpp::Time & stamp, sensor_msgs::msg::Range & out);

  // ===========================================================================
  // BBB Telemetry -> ROS2 Conversions (Protobuf -> ROS2)
  // ===========================================================================

  /**
   * @struct BbbParams
   * @brief Platform geometry and encoder configuration for BBB telemetry
   *        conversions.
   *
   * @details
   * Defines the physical parameters of the STAR differential-drive robot
   * needed to convert raw encoder ticks into metric odometry and joint states.
   * All values must be strictly positive.
   *
   * @code
   * MessageConverter::BbbParams params{0.356, 0.072, 11599};
   * converter.init_bbb_params(params);
   * @endcode
   *
   * @see init_bbb_params()          Applies these parameters to the converter.
   * @see telemetry_to_odometry()    Consumes wheel_base and ticks_per_rev.
   * @see telemetry_to_joint_state() Consumes wheel_radius and ticks_per_rev.
   *
   * @since Version 1.1.0
   */
  struct BbbParams
  {
    double wheel_base;     /**< Track width in metres (centre-to-centre between
                                left and right wheels). Must be > 0. */
    double wheel_radius;   /**< Wheel radius in metres. Must be > 0. */
    int32_t ticks_per_rev; /**< Encoder ticks per full wheel revolution.
                                Must be > 0. STAR default: 11599 (341 PPR *
                                34.02:1 gear ratio). */
  };

  /**
   * @brief Initialize BBB telemetry converter with platform parameters.
   *
   * @details
   * Must be called before telemetry_to_odometry() or telemetry_to_joint_state().
   * Resets all dead-reckoning state (pose, encoder tick baselines) so the
   * converter starts fresh.
   *
   * @param[in] params Platform geometry and encoder configuration.
   *
   * @pre params.wheel_base > 0, params.wheel_radius > 0, params.ticks_per_rev > 0.
   * @post Dead-reckoning state reset to origin (0, 0, 0).
   * @post BBB conversion methods are ready for use.
   *
   * @throw std::invalid_argument If any parameter is non-positive.
   *
   * @note Not thread-safe; call from a single thread before starting timers.
   *
   * @code
   * MessageConverter::BbbParams p{0.356, 0.072, 11599};
   * converter.init_bbb_params(p);
   * @endcode
   *
   * @see telemetry_to_odometry()    Uses these params for dead-reckoning.
   * @see telemetry_to_joint_state() Uses these params for tick-to-rad conversion.
   *
   * @since Version 1.1.0
   */
  void init_bbb_params(const BbbParams & params);

  /**
   * @brief Convert TelemetryData encoder readings to nav_msgs/Odometry via
   *        dead-reckoning integration.
   *
   * @details
   * Computes pose (x, y, theta) from cumulative encoder tick deltas using
   * differential drive kinematics. Velocity is taken from encoder velocity_mps
   * fields. Covariance matrices are set to realistic values for
   * robot_localization EKF fusion. Returns false if any encoder sub-message
   * is missing, leaving odom and internal state unmodified.
   *
   * @param[in]  telemetry  TelemetryData from BBB via gateway gRPC.
   * @param[out] odom       Populated Odometry message (odom frame, base_link child).
   *
   * @return true if conversion succeeded, false if encoder data incomplete.
   * @retval true  All four encoder sub-messages present; odom populated.
   * @retval false Incomplete telemetry; odom and DR state unchanged.
   *
   * @pre init_bbb_params() must have been called.
   * @pre telemetry contains encoder_front_left/right/back_left/right sub-messages.
   * @post odom.header.frame_id == "odom", odom.child_frame_id == "base_link".
   * @post Internal dead-reckoning state (dr_x_, dr_y_, dr_theta_) is updated.
   *
   * @throw None.
   * @note NOT thread-safe; mutates dead-reckoning state.
   *
   * @code
   * nav_msgs::msg::Odometry odom;
   * if (converter.telemetry_to_odometry(telemetry, odom)) {
   *   odom.header.stamp = node->now();
   *   odom_pub->publish(odom);
   * }
   * @endcode
   *
   * @see init_bbb_params()          Must be called first.
   * @see telemetry_to_joint_state() Companion encoder conversion.
   *
   * @since Version 1.1.0
   */
  bool telemetry_to_odometry(
    const ::star::v1::TelemetryData & telemetry,
    nav_msgs::msg::Odometry & odom);

  /**
   * @brief Convert TelemetryData IMU readings to sensor_msgs/Imu.
   *
   * @details
   * Maps BNO055/MPU-9250 IMU data (quaternion, gyro, accel) from protobuf to
   * ROS2 Imu message. Orientation covariance is set for yaw-only fusion (2D
   * mode). Returns false if the IMU sub-message is absent.
   *
   * @param[in]  telemetry  TelemetryData from BBB via gateway gRPC.
   * @param[out] imu_msg    Populated Imu message (imu_link frame).
   *
   * @return true if IMU data present, false if telemetry.has_imu() is false.
   * @retval true  IMU sub-message present; imu_msg populated.
   * @retval false No IMU data; imu_msg unchanged.
   *
   * @pre telemetry.imu() sub-message is populated.
   * @post imu_msg.header.frame_id == "imu_link".
   *
   * @throw None.
   * @note Thread-safe; stateless conversion.
   *
   * @code
   * sensor_msgs::msg::Imu imu;
   * if (MessageConverter::telemetry_to_imu(telemetry, imu)) {
   *   imu.header.stamp = node->now();
   *   imu_pub->publish(imu);
   * }
   * @endcode
   *
   * @see telemetry_to_odometry() Companion encoder conversion.
   *
   * @since Version 1.1.0
   */
  static bool telemetry_to_imu(
    const ::star::v1::TelemetryData & telemetry,
    sensor_msgs::msg::Imu & imu_msg);

  /**
   * @brief Convert TelemetryData encoder readings to sensor_msgs/JointState.
   *
   * @details
   * Reports position (radians) and velocity (rad/s) for all four wheel joints.
   * Returns false if any encoder sub-message is missing.
   *
   * @param[in]  telemetry    TelemetryData from BBB via gateway gRPC.
   * @param[out] joint_state  Populated JointState message.
   *
   * @return true if all encoder data present, false if incomplete.
   * @retval true  All four encoder sub-messages present; joint_state populated.
   * @retval false Incomplete data; joint_state unchanged.
   *
   * @pre init_bbb_params() must have been called.
   * @post joint_state.name has 4 entries matching wheel joint names.
   *
   * @throw None.
   * @note Uses bbb_params_ but does not mutate state; safe to call concurrently
   *       if bbb_params_ is not being written.
   *
   * @code
   * sensor_msgs::msg::JointState js;
   * if (converter.telemetry_to_joint_state(telemetry, js)) {
   *   js.header.stamp = node->now();
   *   joint_state_pub->publish(js);
   * }
   * @endcode
   *
   * @see init_bbb_params()       Must be called first.
   * @see telemetry_to_odometry() Companion encoder conversion.
   *
   * @since Version 1.1.0
   */
  bool telemetry_to_joint_state(
    const ::star::v1::TelemetryData & telemetry,
    sensor_msgs::msg::JointState & joint_state);

  // ===========================================================================
  // Validation Helpers
  // ===========================================================================

  /**
   * @brief Validate double value for NaN and infinity.
   * @param value Value to validate
   * @return true if value is finite (not NaN, not infinity)
   * @note Thread-safe; stateless pure function.
   * @since Version 1.0.0
   */
  static bool is_valid_double(double value) {return std::isfinite(value);}

  /**
   * @brief Clamp value to range [min, max].
   * @param value Value to clamp
   * @param min Minimum allowed value
   * @param max Maximum allowed value
   * @return Clamped value
   * @note Thread-safe; stateless pure function.
   * @since Version 1.0.0
   */
  static double clamp(double value, double min, double max)
  {
    return std::max(min, std::min(max, value));
  }

  /**
   * @brief Convert ROS2 timestamp to microseconds since epoch.
   * @param time ROS2 time
   * @return Microseconds since epoch
   * @note Thread-safe; stateless pure function.
   * @since Version 1.0.0
   */
  static int64_t ros_time_to_us(const rclcpp::Time & time);

private:
  // BBB dead-reckoning state (for telemetry_to_odometry)
  BbbParams bbb_params_{0.356, 0.072, 11599};
  bool bbb_params_initialized_{false};
  double dr_x_{0.0};      /**< Dead-reckoning X position (m). */
  double dr_y_{0.0};      /**< Dead-reckoning Y position (m). */
  double dr_theta_{0.0};  /**< Dead-reckoning heading (rad). */
  int64_t prev_ticks_fl_{0};
  int64_t prev_ticks_fr_{0};
  int64_t prev_ticks_bl_{0};
  int64_t prev_ticks_br_{0};
  bool first_odom_msg_{true};

  static double normalize_angle(double angle);

  // Differential drive kinematics constants
  static constexpr double MAX_VELOCITY_MPS =
    2.0;   /**< VelocityCommand valid range (m/s); wheel and output velocities
              are clamped to [-MAX_VELOCITY_MPS, +MAX_VELOCITY_MPS]. */
  static constexpr double MAX_ANGULAR_VEL =
    4.0;   /**< Maximum angular velocity (rad/s); angular component of Twist is
              clamped to [-MAX_ANGULAR_VEL, +MAX_ANGULAR_VEL]. */
  static constexpr size_t MAX_LIDAR_SAMPLES =
    500;   /**< Maximum number of LiDAR samples forwarded per scan frame;
              caps LidarScan protobuf size to bound bandwidth and UI rendering
              cost. laserscan_to_proto() downsamples evenly when the raw scan
              exceeds this limit. */
};

} // namespace star::star_gateway_bridge
