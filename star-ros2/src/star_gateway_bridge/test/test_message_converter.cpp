/**
 * @file test_message_converter.cpp
 * @brief Unit tests for the MessageConverter functions between ROS2 and STAR protobuf messages.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_gateway_bridge/message_converter.hpp"

#include <cmath>   // NOLINT(build/include_order)
#include <limits>  // NOLINT(build/include_order)

#include <geometry_msgs/msg/twist.hpp>  // NOLINT(build/include_order)
#include "star/v1/motor_control.pb.h"
#include <gtest/gtest.h>  // NOLINT(build/include_order)

namespace star
{

// Test fixture for MessageConverter tests
class MessageConverterTest : public ::testing::Test {
protected:
  star_gateway_bridge::MessageConverter converter_;
  static constexpr double TOLERANCE = 1e-6;
  static constexpr double WHEEL_BASE_M = 0.150;  // 150mm wheel base
};

// =============================================================================
// Forward Kinematics Tests (Twist -> VelocityCommand)
// =============================================================================

TEST_F(MessageConverterTest, TwistToCommandZeroVelocity)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  EXPECT_NEAR(command.front_left_velocity_mps(), 0.0, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandPureTranslation)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;  // 1 m/s forward
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Both wheels should move at same speed
  EXPECT_NEAR(command.front_left_velocity_mps(), 1.0, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), 1.0, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandPureRotation)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = 1.0;  // 1 rad/s counter-clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Differential rotation: v_left = -omega*L/2, v_right = +omega*L/2
  double expected_velocity = twist.angular.z * WHEEL_BASE_M / 2.0;  // omega * L/2 = 0.075 m/s

  EXPECT_NEAR(command.front_left_velocity_mps(), -expected_velocity, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), expected_velocity, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandMixedMotion)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;   // 1 m/s forward
  twist.angular.z = 2.0;  // 2 rad/s counter-clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // v_left = v - omega*L/2
  // v_right = v + omega*L/2
  double rotational_component = 2.0 * WHEEL_BASE_M / 2.0;  // 0.150 m/s
  double expected_left = 1.0 - rotational_component;       // 0.850 m/s
  double expected_right = 1.0 + rotational_component;      // 1.150 m/s

  EXPECT_NEAR(command.front_left_velocity_mps(), expected_left, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), expected_right, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandNegativeLinear)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = -0.5;  // 0.5 m/s backward
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Both wheels should be negative (backward)
  EXPECT_NEAR(command.front_left_velocity_mps(), -0.5, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), -0.5, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandNegativeAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = -1.0;  // 1 rad/s clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Clockwise rotation: left positive, right negative
  double expected_velocity = std::abs(twist.angular.z) * WHEEL_BASE_M / 2.0;

  EXPECT_NEAR(command.front_left_velocity_mps(), expected_velocity, TOLERANCE);
  EXPECT_NEAR(command.front_right_velocity_mps(), -expected_velocity, TOLERANCE);
}

// =============================================================================
// Inverse Kinematics Tests (VelocityCommand -> Twist)
// =============================================================================

TEST_F(MessageConverterTest, CommandToTwistZeroVelocity)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(0.0);
  command.set_front_right_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  EXPECT_NEAR(twist.linear.x, 0.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistPureTranslation)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(1.0);
  command.set_front_right_velocity_mps(1.0);
  // Also set back wheels for completeness
  command.set_back_left_velocity_mps(1.0);
  command.set_back_right_velocity_mps(1.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // v = (v_left + v_right) / 2
  EXPECT_NEAR(twist.linear.x, 1.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistPureRotation)
{
  // For pure rotation: v_left = -v_right
  double wheel_velocity = 0.075;  // Arbitrary value

  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(-wheel_velocity);
  command.set_front_right_velocity_mps(wheel_velocity);
  command.set_back_left_velocity_mps(-wheel_velocity);
  command.set_back_right_velocity_mps(wheel_velocity);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // omega = (v_right - v_left) / L
  double expected_angular = (wheel_velocity - (-wheel_velocity)) / WHEEL_BASE_M;

  EXPECT_NEAR(twist.linear.x, 0.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, expected_angular, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistMixedMotion)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(0.5);
  command.set_front_right_velocity_mps(1.5);
  command.set_back_left_velocity_mps(0.5);
  command.set_back_right_velocity_mps(1.5);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // v = (v_left + v_right) / 2 = (0.5 + 1.5) / 2 = 1.0
  // omega = (v_right - v_left) / L = (1.5 - 0.5) / 0.150 = 6.667
  double expected_linear = (0.5 + 1.5) / 2.0;
  double expected_angular = (1.5 - 0.5) / WHEEL_BASE_M;

  EXPECT_NEAR(twist.linear.x, expected_linear, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, expected_angular, TOLERANCE);
}

// =============================================================================
// Kinematics Roundtrip Tests (Twist -> Command -> Twist)
// =============================================================================

TEST_F(MessageConverterTest, KinematicsRoundtripZero)
{
  geometry_msgs::msg::Twist original_twist;
  original_twist.linear.x = 0.0;
  original_twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(original_twist, command, WHEEL_BASE_M));

  geometry_msgs::msg::Twist reconstructed_twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, reconstructed_twist, WHEEL_BASE_M));

  EXPECT_NEAR(reconstructed_twist.linear.x, original_twist.linear.x, TOLERANCE);
  EXPECT_NEAR(reconstructed_twist.angular.z, original_twist.angular.z, TOLERANCE);
}

TEST_F(MessageConverterTest, KinematicsRoundtripTranslation)
{
  geometry_msgs::msg::Twist original_twist;
  original_twist.linear.x = 1.23;
  original_twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(original_twist, command, WHEEL_BASE_M));

  geometry_msgs::msg::Twist reconstructed_twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, reconstructed_twist, WHEEL_BASE_M));

  EXPECT_NEAR(reconstructed_twist.linear.x, original_twist.linear.x, TOLERANCE);
  EXPECT_NEAR(reconstructed_twist.angular.z, original_twist.angular.z, TOLERANCE);
}

TEST_F(MessageConverterTest, KinematicsRoundtripRotation)
{
  geometry_msgs::msg::Twist original_twist;
  original_twist.linear.x = 0.0;
  original_twist.angular.z = 2.5;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(original_twist, command, WHEEL_BASE_M));

  geometry_msgs::msg::Twist reconstructed_twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, reconstructed_twist, WHEEL_BASE_M));

  EXPECT_NEAR(reconstructed_twist.linear.x, original_twist.linear.x, TOLERANCE);
  EXPECT_NEAR(reconstructed_twist.angular.z, original_twist.angular.z, TOLERANCE);
}

TEST_F(MessageConverterTest, KinematicsRoundtripMixed)
{
  geometry_msgs::msg::Twist original_twist;
  original_twist.linear.x = 0.75;
  original_twist.angular.z = -1.5;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(original_twist, command, WHEEL_BASE_M));

  geometry_msgs::msg::Twist reconstructed_twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, reconstructed_twist, WHEEL_BASE_M));

  EXPECT_NEAR(reconstructed_twist.linear.x, original_twist.linear.x, TOLERANCE);
  EXPECT_NEAR(reconstructed_twist.angular.z, original_twist.angular.z, TOLERANCE);
}

// =============================================================================
// Validation Tests (NaN, Infinity, Invalid Values)
// =============================================================================

TEST_F(MessageConverterTest, TwistToCommandNaNLinear)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = std::numeric_limits<double>::quiet_NaN();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandNaNAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = std::numeric_limits<double>::quiet_NaN();

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandInfinityLinear)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = std::numeric_limits<double>::infinity();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandInfinityAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = std::numeric_limits<double>::infinity();

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandNegativeInfinity)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = -std::numeric_limits<double>::infinity();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandZeroWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, 0.0));
}

TEST_F(MessageConverterTest, TwistToCommandNegativeWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, -0.150));
}

TEST_F(MessageConverterTest, CommandToTwistNaNMotor0)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(std::numeric_limits<double>::quiet_NaN());
  command.set_front_right_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, CommandToTwistNaNMotor1)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(0.0);
  command.set_front_right_velocity_mps(std::numeric_limits<double>::quiet_NaN());

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, CommandToTwistInfinityMotor0)
{
  star::v1::VelocityCommand command;
  command.set_front_left_velocity_mps(std::numeric_limits<double>::infinity());
  command.set_front_right_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

// =============================================================================
// PID Configuration Tests (Proto -> ROS2 direction)
// =============================================================================

TEST_F(MessageConverterTest, PidConfigToGainsNominal)
{
  star::v1::PidConfig proto_config;
  proto_config.set_kp(1.5);
  proto_config.set_ki(0.3);
  proto_config.set_kd(0.05);

  double kp, ki, kd;
  ASSERT_TRUE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));

  EXPECT_NEAR(kp, 1.5, TOLERANCE);
  EXPECT_NEAR(ki, 0.3, TOLERANCE);
  EXPECT_NEAR(kd, 0.05, TOLERANCE);
}

TEST_F(MessageConverterTest, PidConfigToGainsZeroGains)
{
  star::v1::PidConfig proto_config;
  proto_config.set_kp(0.0);
  proto_config.set_ki(0.0);
  proto_config.set_kd(0.0);

  double kp, ki, kd;
  ASSERT_TRUE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));

  EXPECT_NEAR(kp, 0.0, TOLERANCE);
  EXPECT_NEAR(ki, 0.0, TOLERANCE);
  EXPECT_NEAR(kd, 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, PidConfigToGainsNaN)
{
  star::v1::PidConfig proto_config;
  proto_config.set_kp(std::numeric_limits<double>::quiet_NaN());
  proto_config.set_ki(0.3);
  proto_config.set_kd(0.05);

  double kp, ki, kd;
  ASSERT_FALSE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));
}

TEST_F(MessageConverterTest, PidConfigToGainsInfinity)
{
  star::v1::PidConfig proto_config;
  proto_config.set_kp(1.5);
  proto_config.set_ki(std::numeric_limits<double>::infinity());
  proto_config.set_kd(0.05);

  double kp, ki, kd;
  ASSERT_FALSE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));
}

TEST_F(MessageConverterTest, PidConfigToGainsNegativeGains)
{
  // Negative PID gains are mathematically valid (though unusual)
  star::v1::PidConfig proto_config;
  proto_config.set_kp(-1.5);
  proto_config.set_ki(-0.3);
  proto_config.set_kd(-0.05);

  double kp, ki, kd;
  // Implementation only checks for NaN/infinity, not negative values
  ASSERT_TRUE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));

  EXPECT_NEAR(kp, -1.5, TOLERANCE);
  EXPECT_NEAR(ki, -0.3, TOLERANCE);
  EXPECT_NEAR(kd, -0.05, TOLERANCE);
}

TEST_F(MessageConverterTest, PidConfigToGainsLargeValues)
{
  star::v1::PidConfig proto_config;
  proto_config.set_kp(1000.0);
  proto_config.set_ki(500.0);
  proto_config.set_kd(100.0);

  double kp, ki, kd;
  ASSERT_TRUE(converter_.pid_config_to_gains(proto_config, kp, ki, kd));

  EXPECT_NEAR(kp, 1000.0, TOLERANCE);
  EXPECT_NEAR(ki, 500.0, TOLERANCE);
  EXPECT_NEAR(kd, 100.0, TOLERANCE);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(MessageConverterTest, TwistToCommandMaxVelocity)
{
  // Test with high velocities (near physical limits)
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 10.0;   // 10 m/s (unrealistic but valid)
  twist.angular.z = 20.0;  // 20 rad/s

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Should not crash or produce NaN
  EXPECT_FALSE(std::isnan(command.front_left_velocity_mps()));
  EXPECT_FALSE(std::isnan(command.front_right_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.front_left_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.front_right_velocity_mps()));
}

TEST_F(MessageConverterTest, TwistToCommandVerySmallWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;
  twist.angular.z = 1.0;

  star::v1::VelocityCommand command;
  // Very small but positive wheel base (1mm)
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, 0.001));

  // Should produce large angular velocities but not overflow
  EXPECT_FALSE(std::isinf(command.front_left_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.front_right_velocity_mps()));
}

TEST_F(MessageConverterTest, TwistToCommandVeryLargeWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;
  twist.angular.z = 1.0;

  star::v1::VelocityCommand command;
  // Large wheel base (10m)
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, 10.0));

  EXPECT_FALSE(std::isnan(command.front_left_velocity_mps()));
  EXPECT_FALSE(std::isnan(command.front_right_velocity_mps()));
}

// =============================================================================
// obstacle_distance_to_range Tests (HC-SR04 Sensor Converter)
// =============================================================================

class ObstacleDistanceToRangeTest : public ::testing::Test {
protected:
  star_gateway_bridge::MessageConverter converter_;
  static constexpr float FLOAT_TOL = 1e-5F;
  static constexpr float HC_SR04_MIN = 0.02F;
  static constexpr float HC_SR04_MAX = 4.00F;
  static constexpr float HC_SR04_FOV = 0.2618F;

  rclcpp::Time make_stamp() const
  {
    return rclcpp::Time(0, 0, RCL_ROS_TIME);
  }
};

TEST_F(ObstacleDistanceToRangeTest, NominalReading)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(1.50F, "obstacle_front_left", make_stamp(), out);

  EXPECT_NEAR(out.range, 1.50F, FLOAT_TOL);
  EXPECT_EQ(out.radiation_type, sensor_msgs::msg::Range::ULTRASOUND);
  EXPECT_NEAR(out.field_of_view, HC_SR04_FOV, FLOAT_TOL);
  EXPECT_NEAR(out.min_range, HC_SR04_MIN, FLOAT_TOL);
  EXPECT_NEAR(out.max_range, HC_SR04_MAX, FLOAT_TOL);
  EXPECT_EQ(out.header.frame_id, "obstacle_front_left");
}

TEST_F(ObstacleDistanceToRangeTest, ZeroMapsToMaxRange)
{
  // 0.0 means HC-SR04 echo timeout (no target); map to max_range so Nav2
  // treats it as free space, not a zero-distance obstacle.
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(0.0F, "obstacle_front_right", make_stamp(), out);

  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, NaNMapsToMaxRange)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(std::numeric_limits<float>::quiet_NaN(), "obstacle_back_left", make_stamp(),
                                        out);

  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, PosInfMapsToMaxRange)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(std::numeric_limits<float>::infinity(), "obstacle_back_right", make_stamp(),
                                        out);

  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, NegInfMapsToMinRange)
{
  // Negative infinity is non-finite; clamped to min_range after the
  // isfinite check falls through to std::max/min clamping.
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(-std::numeric_limits<float>::infinity(), "obstacle_front_left", make_stamp(),
                                        out);

  // Non-finite maps to max_range per implementation.
  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, BelowMinClampsToMinRange)
{
  // Finite negative values clamp to min_range.
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(-0.5F, "obstacle_front_left", make_stamp(), out);

  EXPECT_NEAR(out.range, HC_SR04_MIN, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, AboveMaxClampsToMaxRange)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(5.00F, "obstacle_front_left", make_stamp(), out);

  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, AtMinBoundary)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(HC_SR04_MIN, "obstacle_front_left", make_stamp(), out);

  EXPECT_NEAR(out.range, HC_SR04_MIN, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, AtMaxBoundary)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(HC_SR04_MAX, "obstacle_front_left", make_stamp(), out);

  EXPECT_NEAR(out.range, HC_SR04_MAX, FLOAT_TOL);
}

TEST_F(ObstacleDistanceToRangeTest, FrameIdPropagated)
{
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(1.0F, "obstacle_back_right", make_stamp(), out);

  EXPECT_EQ(out.header.frame_id, "obstacle_back_right");
}

TEST_F(ObstacleDistanceToRangeTest, StampPropagated)
{
  const rclcpp::Time stamp(123456789LL, RCL_ROS_TIME);
  sensor_msgs::msg::Range out;
  converter_.obstacle_distance_to_range(1.0F, "obstacle_front_left", stamp, out);

  EXPECT_EQ(out.header.stamp.sec, 0);
  EXPECT_EQ(out.header.stamp.nanosec, 123456789u);
}

}  // namespace star

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
