/**
 * @file test_spi_message_converter.cpp
 * @brief Unit tests for SpiMessageConverter ROS2-to-protobuf and protobuf-to-ROS2 conversions.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <cmath>

#include <gtest/gtest.h>  // NOLINT

#include "star_spi_bridge/spi_message_converter.hpp"

using star_spi_bridge::SpiMessageConverter;

// Test robot geometry. Mirrors the production defaults in
// star_spi_driver_node.cpp and the BBB hardware_config.h so test
// expected values (e.g. 2*pi*r per revolution) remain in sync with
// what the production node would compute.
constexpr double kTestTrackWidthM = 0.356;          // left-right wheel center-to-center [m]
constexpr double kTestWheelRadiusM = 0.072;         // rolling radius [m]
constexpr int kTestEncoderTicksPerRev = 11599;      // 341 PPR x 34.02:1 gearbox

class SpiMessageConverterTest : public ::testing::Test
{
protected:
  SpiMessageConverter::Parameters params{
    kTestTrackWidthM, kTestWheelRadiusM, kTestEncoderTicksPerRev};
  SpiMessageConverter converter{params};
};

TEST_F(SpiMessageConverterTest, TwistToVelocity_Forward)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 1.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand cmd;
  EXPECT_TRUE(converter.twist_to_velocity_command(twist, cmd));

  EXPECT_NEAR(cmd.front_left_velocity_mps(), 1.0, 0.001);
  EXPECT_NEAR(cmd.front_right_velocity_mps(), 1.0, 0.001);
}

TEST_F(SpiMessageConverterTest, TwistToVelocity_RotateLeft)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.0;
  twist.angular.z = 2.0;  // 2 rad/s

  star::v1::VelocityCommand cmd;
  EXPECT_TRUE(converter.twist_to_velocity_command(twist, cmd));

  // Differential drive kinematics:
  //   v_right = linear + angular * (track_width / 2)
  //   v_left  = linear - angular * (track_width / 2)
  // For linear=0, angular=2: |v| = track_width.
  const double expected_right = twist.angular.z * (kTestTrackWidthM / 2.0);
  const double expected_left = -expected_right;
  EXPECT_NEAR(cmd.front_right_velocity_mps(), expected_right, 0.001);
  EXPECT_NEAR(cmd.front_left_velocity_mps(), expected_left, 0.001);
}

TEST_F(SpiMessageConverterTest, TwistToVelocity_NaN)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = std::numeric_limits<double>::quiet_NaN();

  star::v1::VelocityCommand cmd;
  EXPECT_FALSE(converter.twist_to_velocity_command(twist, cmd));
}

// =============================================================================
// Tests for telemetry_to_odometry
// =============================================================================

TEST_F(SpiMessageConverterTest, TelemetryToOdometry_FirstMessage)
{
  star::v1::TelemetryData telemetry;
  telemetry.mutable_encoder_front_left()->set_ticks(100);
  telemetry.mutable_encoder_front_right()->set_ticks(100);
  telemetry.mutable_encoder_back_left()->set_ticks(100);
  telemetry.mutable_encoder_back_right()->set_ticks(100);

  nav_msgs::msg::Odometry odom;
  converter.telemetry_to_odometry(telemetry, odom);

  // First message should initialize state but not update pose
  EXPECT_NEAR(odom.pose.pose.position.x, 0.0, 0.001);
  EXPECT_NEAR(odom.pose.pose.position.y, 0.0, 0.001);
}

TEST_F(SpiMessageConverterTest, TelemetryToOdometry_ForwardMovement)
{
  // First message to initialize state
  star::v1::TelemetryData telemetry1;
  telemetry1.mutable_encoder_front_left()->set_ticks(0);
  telemetry1.mutable_encoder_front_right()->set_ticks(0);
  telemetry1.mutable_encoder_back_left()->set_ticks(0);
  telemetry1.mutable_encoder_back_right()->set_ticks(0);

  nav_msgs::msg::Odometry odom1;
  converter.telemetry_to_odometry(telemetry1, odom1);

  // Second message with forward movement (same ticks on both sides = straight)
  star::v1::TelemetryData telemetry2;
  int64_t ticks = 11599;  // One full wheel revolution
  telemetry2.mutable_encoder_front_left()->set_ticks(ticks);
  telemetry2.mutable_encoder_front_right()->set_ticks(ticks);
  telemetry2.mutable_encoder_back_left()->set_ticks(ticks);
  telemetry2.mutable_encoder_back_right()->set_ticks(ticks);

  nav_msgs::msg::Odometry odom2;
  converter.telemetry_to_odometry(telemetry2, odom2);

  // One revolution = 2 * pi * radius (about 0.452 m for the 144 mm wheel)
  double expected_dist = 2.0 * M_PI * kTestWheelRadiusM;
  EXPECT_NEAR(odom2.pose.pose.position.x, expected_dist, 0.01);
  EXPECT_NEAR(odom2.pose.pose.position.y, 0.0, 0.001);
}

TEST_F(SpiMessageConverterTest, TelemetryToOdometry_WithVelocity)
{
  // Create fresh converter for this test
  SpiMessageConverter test_converter{params};

  // First message
  star::v1::TelemetryData telemetry1;
  telemetry1.mutable_encoder_front_left()->set_ticks(0);
  telemetry1.mutable_encoder_front_right()->set_ticks(0);
  telemetry1.mutable_encoder_back_left()->set_ticks(0);
  telemetry1.mutable_encoder_back_right()->set_ticks(0);
  nav_msgs::msg::Odometry odom1;
  test_converter.telemetry_to_odometry(telemetry1, odom1);

  // Second message with velocity data
  star::v1::TelemetryData telemetry2;
  telemetry2.mutable_encoder_front_left()->set_ticks(100);
  telemetry2.mutable_encoder_front_left()->set_velocity_mps(1.0);
  telemetry2.mutable_encoder_front_right()->set_ticks(100);
  telemetry2.mutable_encoder_front_right()->set_velocity_mps(1.0);
  telemetry2.mutable_encoder_back_left()->set_ticks(100);
  telemetry2.mutable_encoder_back_left()->set_velocity_mps(1.0);
  telemetry2.mutable_encoder_back_right()->set_ticks(100);
  telemetry2.mutable_encoder_back_right()->set_velocity_mps(1.0);

  nav_msgs::msg::Odometry odom2;
  test_converter.telemetry_to_odometry(telemetry2, odom2);

  EXPECT_NEAR(odom2.twist.twist.linear.x, 1.0, 0.001);
  EXPECT_NEAR(odom2.twist.twist.angular.z, 0.0, 0.001);
}

// =============================================================================
// Tests for telemetry_to_joint_state
// =============================================================================

TEST_F(SpiMessageConverterTest, TelemetryToJointState_Names)
{
  star::v1::TelemetryData telemetry;
  telemetry.mutable_encoder_front_left()->set_ticks(0);
  telemetry.mutable_encoder_front_right()->set_ticks(0);
  telemetry.mutable_encoder_back_left()->set_ticks(0);
  telemetry.mutable_encoder_back_right()->set_ticks(0);

  sensor_msgs::msg::JointState joint_state;
  converter.telemetry_to_joint_state(telemetry, joint_state);

  ASSERT_EQ(joint_state.name.size(), 4u);
  EXPECT_EQ(joint_state.name[0], "front_left_wheel");
  EXPECT_EQ(joint_state.name[1], "front_right_wheel");
  EXPECT_EQ(joint_state.name[2], "back_left_wheel");
  EXPECT_EQ(joint_state.name[3], "back_right_wheel");
}

TEST_F(SpiMessageConverterTest, TelemetryToJointState_Position)
{
  star::v1::TelemetryData telemetry;
  // One full revolution = 11599 ticks = 2*PI radians
  int64_t one_rev = 11599;
  telemetry.mutable_encoder_front_left()->set_ticks(one_rev);
  telemetry.mutable_encoder_front_right()->set_ticks(one_rev / 2);
  telemetry.mutable_encoder_back_left()->set_ticks(one_rev * 2);
  telemetry.mutable_encoder_back_right()->set_ticks(0);

  sensor_msgs::msg::JointState joint_state;
  converter.telemetry_to_joint_state(telemetry, joint_state);

  ASSERT_EQ(joint_state.position.size(), 4u);
  EXPECT_NEAR(joint_state.position[0], 2.0 * M_PI, 0.01);  // FL: 1 rev
  EXPECT_NEAR(joint_state.position[1], M_PI, 0.01);        // FR: 0.5 rev
  EXPECT_NEAR(joint_state.position[2], 4.0 * M_PI, 0.02);  // BL: 2 rev
  EXPECT_NEAR(joint_state.position[3], 0.0, 0.001);        // BR: 0 rev
}

TEST_F(SpiMessageConverterTest, TelemetryToJointState_Velocity)
{
  star::v1::TelemetryData telemetry;
  double velocity_mps = 1.0;  // 1 m/s linear
  telemetry.mutable_encoder_front_left()->set_velocity_mps(velocity_mps);
  telemetry.mutable_encoder_front_right()->set_velocity_mps(velocity_mps);
  telemetry.mutable_encoder_back_left()->set_velocity_mps(velocity_mps);
  telemetry.mutable_encoder_back_right()->set_velocity_mps(velocity_mps);

  sensor_msgs::msg::JointState joint_state;
  converter.telemetry_to_joint_state(telemetry, joint_state);

  // angular_velocity = linear_velocity / radius (about 13.89 rad/s for v=1, r=0.072)
  double expected_angular = velocity_mps / kTestWheelRadiusM;
  ASSERT_EQ(joint_state.velocity.size(), 4u);
  EXPECT_NEAR(joint_state.velocity[0], expected_angular, 0.1);
  EXPECT_NEAR(joint_state.velocity[1], expected_angular, 0.1);
  EXPECT_NEAR(joint_state.velocity[2], expected_angular, 0.1);
  EXPECT_NEAR(joint_state.velocity[3], expected_angular, 0.1);
}

// =============================================================================
// Round-trip encode/decode test
// =============================================================================

TEST_F(SpiMessageConverterTest, RoundTrip_TwistToVelocityToJointState)
{
  // Create a twist command
  geometry_msgs::msg::Twist twist;
  twist.linear.x = 0.5;
  twist.angular.z = 0.2;

  // Convert to velocity command
  star::v1::VelocityCommand cmd;
  ASSERT_TRUE(converter.twist_to_velocity_command(twist, cmd));

  // Verify the velocity command was set correctly.
  // Differential drive kinematics with kTestTrackWidthM as the track:
  //   v_right = linear + angular * (track_width / 2)
  //   v_left  = linear - angular * (track_width / 2)
  const double half_track = kTestTrackWidthM / 2.0;
  const double expected_right = twist.linear.x + twist.angular.z * half_track;
  const double expected_left = twist.linear.x - twist.angular.z * half_track;
  EXPECT_NEAR(cmd.front_right_velocity_mps(), expected_right, 0.001);
  EXPECT_NEAR(cmd.front_left_velocity_mps(), expected_left, 0.001);
  EXPECT_NEAR(cmd.back_right_velocity_mps(), expected_right, 0.001);
  EXPECT_NEAR(cmd.back_left_velocity_mps(), expected_left, 0.001);

  // Create telemetry with matching velocities (simulating firmware response)
  star::v1::TelemetryData telemetry;
  telemetry.mutable_encoder_front_left()->set_velocity_mps(cmd.front_left_velocity_mps());
  telemetry.mutable_encoder_front_right()->set_velocity_mps(cmd.front_right_velocity_mps());
  telemetry.mutable_encoder_back_left()->set_velocity_mps(cmd.back_left_velocity_mps());
  telemetry.mutable_encoder_back_right()->set_velocity_mps(cmd.back_right_velocity_mps());

  // Convert telemetry to joint state
  sensor_msgs::msg::JointState joint_state;
  converter.telemetry_to_joint_state(telemetry, joint_state);

  // Verify velocities converted to rad/s
  double inv_radius = 1.0 / kTestWheelRadiusM;
  EXPECT_NEAR(joint_state.velocity[0], cmd.front_left_velocity_mps() * inv_radius, 0.1);
  EXPECT_NEAR(joint_state.velocity[1], cmd.front_right_velocity_mps() * inv_radius, 0.1);
}
