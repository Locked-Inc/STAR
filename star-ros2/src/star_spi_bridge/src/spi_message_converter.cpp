/**
 * @file spi_message_converter.cpp
 * @brief Message conversion implementation between ROS2 message types and STAR protobuf for the SPI bridge.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_spi_bridge/spi_message_converter.hpp"

#include <tf2/LinearMath/Quaternion.h>  // NOLINT(build/include_order)
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace star_spi_bridge
{

SpiMessageConverter::SpiMessageConverter(const Parameters & params)
: params_(params)
{}

bool SpiMessageConverter::twist_to_velocity_command(const geometry_msgs::msg::Twist & twist,
                                                    star::v1::VelocityCommand & command)
{
  if (!std::isfinite(twist.linear.x) || !std::isfinite(twist.angular.z)) {
    return false;
  }

  double linear_x = twist.linear.x;
  double angular_z = twist.angular.z;

  // Differential drive kinematics
  // v_r = v + (w * L / 2)
  // v_l = v - (w * L / 2)
  double right_vel = linear_x + (angular_z * params_.wheel_base / 2.0);
  double left_vel = linear_x - (angular_z * params_.wheel_base / 2.0);

  // Clamp to hardware limits (e.g. +/- 2.0 m/s as per spec)
  // Although the firmware should also handle this, good to be safe.
  // The spec says: "Velocity Limits: +/-2.0 m/s per wheel"
  const double k_max_vel = 2.0;
  right_vel = std::clamp(right_vel, -k_max_vel, k_max_vel);
  left_vel = std::clamp(left_vel, -k_max_vel, k_max_vel);

  command.set_front_left_velocity_mps(static_cast<float>(left_vel));
  command.set_back_left_velocity_mps(static_cast<float>(left_vel));
  command.set_front_right_velocity_mps(static_cast<float>(right_vel));
  command.set_back_right_velocity_mps(static_cast<float>(right_vel));

  return true;
}

void SpiMessageConverter::telemetry_to_odometry(const star::v1::TelemetryData & telemetry,
                                                nav_msgs::msg::Odometry & odom)
{
  // Extract encoder data from telemetry
  const auto & enc_fl = telemetry.encoder_front_left();
  const auto & enc_fr = telemetry.encoder_front_right();
  const auto & enc_bl = telemetry.encoder_back_left();
  const auto & enc_br = telemetry.encoder_back_right();

  // Encoder ticks are cumulative; we compute deltas between messages
  int64_t ticks_fl = enc_fl.ticks();
  int64_t ticks_fr = enc_fr.ticks();
  int64_t ticks_bl = enc_bl.ticks();
  int64_t ticks_br = enc_br.ticks();

  if (first_odom_msg_) {
    prev_ticks_fl_ = ticks_fl;
    prev_ticks_fr_ = ticks_fr;
    prev_ticks_bl_ = ticks_bl;
    prev_ticks_br_ = ticks_br;
    first_odom_msg_ = false;
    return;  // No movement to report on first message
  }

  // Calculate deltas
  int64_t delta_fl = ticks_fl - prev_ticks_fl_;
  int64_t delta_fr = ticks_fr - prev_ticks_fr_;
  int64_t delta_bl = ticks_bl - prev_ticks_bl_;
  int64_t delta_br = ticks_br - prev_ticks_br_;

  // Update previous
  prev_ticks_fl_ = ticks_fl;
  prev_ticks_fr_ = ticks_fr;
  prev_ticks_bl_ = ticks_bl;
  prev_ticks_br_ = ticks_br;

  // Convert to distance
  double m_per_tick = (2.0 * M_PI * params_.wheel_radius) / params_.ticks_per_rev;

  double dist_fl = delta_fl * m_per_tick;
  double dist_fr = delta_fr * m_per_tick;
  double dist_bl = delta_bl * m_per_tick;
  double dist_br = delta_br * m_per_tick;

  double dist_left = (dist_fl + dist_bl) / 2.0;
  double dist_right = (dist_fr + dist_br) / 2.0;

  double dist_center = (dist_right + dist_left) / 2.0;
  double delta_theta = (dist_right - dist_left) / params_.wheel_base;

  // Update pose
  double avg_theta = theta_ + delta_theta / 2.0;
  x_ += dist_center * std::cos(avg_theta);
  y_ += dist_center * std::sin(avg_theta);
  theta_ = normalize_angle(theta_ + delta_theta);

  // Populate Odometry message
  // Note: Header timestamp should be set by the caller (node)
  odom.header.frame_id = "odom";
  odom.child_frame_id = "base_link";

  odom.pose.pose.position.x = x_;
  odom.pose.pose.position.y = y_;
  odom.pose.pose.position.z = 0.0;

  tf2::Quaternion q;
  q.setRPY(0, 0, theta_);
  odom.pose.pose.orientation.x = q.x();
  odom.pose.pose.orientation.y = q.y();
  odom.pose.pose.orientation.z = q.z();
  odom.pose.pose.orientation.w = q.w();

  // Populate twist from encoder-reported velocities when available
  double v_fl = enc_fl.velocity_mps();
  double v_fr = enc_fr.velocity_mps();
  double v_bl = enc_bl.velocity_mps();
  double v_br = enc_br.velocity_mps();

  bool has_velocity_data = (v_fl != 0.0 || v_fr != 0.0 || v_bl != 0.0 || v_br != 0.0);
  if (has_velocity_data) {
    double v_left = (v_fl + v_bl) / 2.0;
    double v_right = (v_fr + v_br) / 2.0;

    double v_linear = (v_right + v_left) / 2.0;
    double v_angular = (v_right - v_left) / params_.wheel_base;

    odom.twist.twist.linear.x = v_linear;
    odom.twist.twist.angular.z = v_angular;
  }

  // Pose covariance -- row-major 6x6 (x, y, z, roll, pitch, yaw)
  // Zero covariance makes robot_localization unstable; use realistic dead-reckoning values.
  odom.pose.covariance[0] = 0.1;   // x
  odom.pose.covariance[7] = 0.1;   // y
  odom.pose.covariance[14] = 1e6;  // z (not observed)
  odom.pose.covariance[21] = 1e6;  // roll (not observed)
  odom.pose.covariance[28] = 1e6;  // pitch (not observed)
  odom.pose.covariance[35] = 0.1;  // yaw

  // Twist covariance -- same layout
  odom.twist.covariance[0] = 0.05;   // vx
  odom.twist.covariance[7] = 1e6;    // vy (not observed -- diff drive constraint)
  odom.twist.covariance[14] = 1e6;   // vz
  odom.twist.covariance[21] = 1e6;   // roll rate
  odom.twist.covariance[28] = 1e6;   // pitch rate
  odom.twist.covariance[35] = 0.05;  // angular z (yaw rate)
}

void SpiMessageConverter::telemetry_to_joint_state(const star::v1::TelemetryData & telemetry,
                                                   sensor_msgs::msg::JointState & joint_state)
{
  // Extract encoder data from telemetry
  const auto & enc_fl = telemetry.encoder_front_left();
  const auto & enc_fr = telemetry.encoder_front_right();
  const auto & enc_bl = telemetry.encoder_back_left();
  const auto & enc_br = telemetry.encoder_back_right();

  joint_state.name = {"front_left_wheel", "front_right_wheel", "back_left_wheel", "back_right_wheel"};

  // Position (radians)
  double rad_per_tick = (2.0 * M_PI) / params_.ticks_per_rev;
  joint_state.position = {static_cast<double>(enc_fl.ticks()) * rad_per_tick,
                          static_cast<double>(enc_fr.ticks()) * rad_per_tick,
                          static_cast<double>(enc_bl.ticks()) * rad_per_tick,
                          static_cast<double>(enc_br.ticks()) * rad_per_tick};

  // Velocity (rad/s)
  // velocity_mps = radius * angular_velocity_rad_s
  // angular_velocity = velocity_mps / radius
  double inv_radius = 1.0 / params_.wheel_radius;
  joint_state.velocity = {enc_fl.velocity_mps() * inv_radius, enc_fr.velocity_mps() * inv_radius,
                          enc_bl.velocity_mps() * inv_radius, enc_br.velocity_mps() * inv_radius};
}

/**
 * @brief Convert TelemetryData obstacle fields to four sensor_msgs::msg::Range messages.
 *
 * @details
 * Reads per-sensor distances from telemetry.obstacle() and populates four
 * Range messages with HC-SR04 ultrasonic sensor metadata (radiation_type,
 * field_of_view, min_range, max_range) and the measured distance.
 * If a distance value is not finite (NaN or Inf), the corresponding msg.range
 * is set to std::numeric_limits<float>::quiet_NaN() so consumers can detect
 * invalid readings.  Finite values outside the sensor operating range
 * [MIN_RANGE_M, MAX_RANGE_M] are clamped to the nearest boundary so that
 * all published Range messages carry values within the declared min/max.
 *
 * @param[in]  telemetry    Protobuf telemetry payload from the RX72N.  Must
 *                          contain a populated obstacle sub-message.
 * @param[out] front_left   Populated Range message for the front-left sensor.
 * @param[out] front_right  Populated Range message for the front-right sensor.
 * @param[out] back_left    Populated Range message for the back-left sensor.
 * @param[out] back_right   Populated Range message for the back-right sensor.
 *
 * @pre  telemetry.obstacle() is present (contains at least a default-constructed
 *       ObstacleData sub-message).
 * @pre  Distance values from telemetry are expected to be in [0.02, 4.0] m;
 *       finite values outside this range are clamped to the boundary.
 *
 * @post Each output Range message has radiation_type, field_of_view, min_range,
 *       and max_range set to HC-SR04 sensor constants.
 * @post Each output Range message has header.frame_id populated and msg.range
 *       is either a finite value clamped to [MIN_RANGE_M, MAX_RANGE_M] or
 *       quiet_NaN() if the source was non-finite.
 *
 * @note Not thread-safe; the SpiMessageConverter instance must not be accessed
 *       concurrently from multiple threads.
 *
 * @see SpiMessageConverter::telemetry_to_odometry    Related telemetry conversion.
 * @see SpiMessageConverter::telemetry_to_joint_state Related telemetry conversion.
 *
 * @since Version 1.0.0

*/
void SpiMessageConverter::telemetry_to_obstacle_ranges(const star::v1::TelemetryData & telemetry,
                                                       sensor_msgs::msg::Range & front_left,
                                                       sensor_msgs::msg::Range & front_right,
                                                       sensor_msgs::msg::Range & back_left,
                                                       sensor_msgs::msg::Range & back_right)
{
  // HC-SR04 ultrasonic sensor characteristics
  static constexpr float FIELD_OF_VIEW_RAD = 0.2618F;  // ~15 degrees
  static constexpr float MIN_RANGE_M = 0.02F;
  static constexpr float MAX_RANGE_M = 4.0F;

  auto populate = [&](sensor_msgs::msg::Range & msg, const std::string & frame_id, float distance_m) {
    assert(!frame_id.empty());
    msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
    msg.field_of_view = FIELD_OF_VIEW_RAD;
    msg.min_range = MIN_RANGE_M;
    msg.max_range = MAX_RANGE_M;
    msg.header.frame_id = frame_id;
    if (std::isfinite(distance_m)) {
      // Clamp finite values to the HC-SR04 valid operating range
      msg.range = std::clamp(distance_m, MIN_RANGE_M, MAX_RANGE_M);
    } else {
      msg.range = std::numeric_limits<float>::quiet_NaN();
    }
    // Postcondition: range is a clamped finite value in [MIN_RANGE_M, MAX_RANGE_M], or NaN
    assert(!std::isinf(msg.range));
  };

  const auto & obs = telemetry.obstacle();
  populate(front_left, "obstacle_sensor_front_left", obs.distance_front_left_m());
  populate(front_right, "obstacle_sensor_front_right", obs.distance_front_right_m());
  populate(back_left, "obstacle_sensor_back_left", obs.distance_back_left_m());
  populate(back_right, "obstacle_sensor_back_right", obs.distance_back_right_m());
}

double SpiMessageConverter::normalize_angle(double angle)
{
  while (angle > M_PI) {
    angle -= 2.0 * M_PI;
  }
  while (angle < -M_PI) {
    angle += 2.0 * M_PI;
  }
  return angle;
}

}  // namespace star_spi_bridge
