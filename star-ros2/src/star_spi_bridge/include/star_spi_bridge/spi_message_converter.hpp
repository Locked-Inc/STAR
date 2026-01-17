// Copyright 2026 Locked Inc.

#ifndef STAR_SPI_BRIDGE__SPI_MESSAGE_CONVERTER_HPP_
#define STAR_SPI_BRIDGE__SPI_MESSAGE_CONVERTER_HPP_

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include "star/v1/motor_control.pb.h"
#include "star/v1/telemetry.pb.h"

namespace star_spi_bridge
{

class SpiMessageConverter
{
public:
  struct Parameters
  {
    double wheel_base;
    double wheel_radius;
    int32_t ticks_per_rev;
  };

  explicit SpiMessageConverter(const Parameters &params);

  // ROS2 -> Protobuf
  bool twist_to_velocity_command(const geometry_msgs::msg::Twist &twist,
                                 star::v1::VelocityCommand &command);

  // Protobuf -> ROS2
  void telemetry_to_odometry(const star::v1::TelemetryData &telemetry,
                             nav_msgs::msg::Odometry &odom);
  void telemetry_to_joint_state(const star::v1::TelemetryData &telemetry,
                                sensor_msgs::msg::JointState &joint_state);
  void telemetry_to_battery_state(const star::v1::TelemetryData &telemetry,
                                  sensor_msgs::msg::BatteryState &battery_state);

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

#endif  // STAR_SPI_BRIDGE__SPI_MESSAGE_CONVERTER_HPP_
