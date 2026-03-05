/**
 * @file spi_message_converter.hpp
 * @brief Message conversion utilities between ROS2 message types and protobuf
 *        representations for the STAR SPI bridge.
 *
 * @details
 * Declares SpiMessageConverter, which converts between ROS2 geometry, nav,
 * sensor messages and the star::v1 protobuf types exchanged over the SPI link
 * with the RX72N peripheral MCU.
 *
 * Conversion directions:
 *  - ROS2 -> Protobuf: geometry_msgs::msg::Twist -> star::v1::VelocityCommand
 *  - Protobuf -> ROS2: star::v1::TelemetryData -> nav_msgs::msg::Odometry,
 *                      sensor_msgs::msg::JointState, sensor_msgs::msg::Range
 *
 * @note Instances are NOT thread-safe.  Each SpiMessageConverter holds
 *       internal dead-reckoning state (pose, previous encoder ticks) that must
 *       not be accessed concurrently from multiple threads.
 *
 * @author Locked Inc.
 * @date 2026
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/range.hpp>

#include "star/v1/motor_control.pb.h"
#include "star/v1/telemetry.pb.h"

namespace star_spi_bridge
{

class SpiMessageConverter {
public:
  struct Parameters
  {
    double wheel_base;
    double wheel_radius;
    int32_t ticks_per_rev;
  };

  explicit SpiMessageConverter(const Parameters & params);

  // ROS2 -> Protobuf
  bool twist_to_velocity_command(
    const geometry_msgs::msg::Twist & twist,
    star::v1::VelocityCommand & command);

  // Protobuf -> ROS2
  void telemetry_to_odometry(
    const star::v1::TelemetryData & telemetry,
    nav_msgs::msg::Odometry & odom);
  void telemetry_to_joint_state(
    const star::v1::TelemetryData & telemetry,
    sensor_msgs::msg::JointState & joint_state);
  /**
   * @brief Convert TelemetryData obstacle fields to four Range messages.
   *
   * @details
   * Populates each sensor_msgs::msg::Range with the HC-SR04 ultrasonic sensor
   * characteristics (radiation_type, field_of_view, min/max range) and the
   * per-sensor distance reported in telemetry.obstacle().  The header.frame_id
   * of each output reflects the physical sensor mounting location.
   *
   * @param[in]  telemetry    Protobuf telemetry payload received from the
   *                          RX72N; must contain a populated obstacle sub-message.
   * @param[out] front_left   Range message for the front-left ultrasonic sensor.
   * @param[out] front_right  Range message for the front-right ultrasonic sensor.
   * @param[out] back_left    Range message for the back-left ultrasonic sensor.
   * @param[out] back_right   Range message for the back-right ultrasonic sensor.
   *
   * @pre  telemetry.obstacle() sub-message is present and populated by the
   *       RX72N firmware.
   * @pre  Distance values in telemetry are expected to be within the valid
   *       HC-SR04 sensor range (0.02 m - 4.0 m); finite out-of-range values
   *       are clamped to the nearest boundary by this function, and non-finite
   *       values are set to NaN.
   *
   * @post Each output Range message has radiation_type, field_of_view,
   *       min_range, and max_range set to HC-SR04 sensor constants.
   * @post Each output Range message has header.frame_id and range set to a
   *       finite value clamped to [0.02, 4.0] m, or NaN for non-finite inputs;
   *       no exceptions are thrown for valid inputs.
   *
   * @note Not thread-safe; the SpiMessageConverter instance must not be used
   *       concurrently from multiple threads.
   *
   * @see SpiMessageConverter::telemetry_to_odometry  Related conversion helper
   * @see SpiMessageConverter::telemetry_to_joint_state  Related conversion helper
   *
   * @code
   * sensor_msgs::msg::Range fl, fr, bl, br;
   * converter.telemetry_to_obstacle_ranges(telemetry, fl, fr, bl, br);
   * obstacle_front_left_pub_->publish(fl);
   * @endcode
   *
   * @since Version 1.0.0
   */
  void telemetry_to_obstacle_ranges(
    const star::v1::TelemetryData & telemetry,
    sensor_msgs::msg::Range & front_left,
    sensor_msgs::msg::Range & front_right,
    sensor_msgs::msg::Range & back_left,
    sensor_msgs::msg::Range & back_right);

private:
  Parameters params_;

  // Odometry state
  double x_ = 0.0;
  double y_ = 0.0;
  double theta_ = 0.0;

  // Encoder state for delta calculation
  int64_t prev_ticks_fl_ = 0;
  int64_t prev_ticks_fr_ = 0;
  int64_t prev_ticks_bl_ = 0;
  int64_t prev_ticks_br_ = 0;
  bool first_odom_msg_ = true;

  static double normalize_angle(double angle);
};

}  // namespace star_spi_bridge
