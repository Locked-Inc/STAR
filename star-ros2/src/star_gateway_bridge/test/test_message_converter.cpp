// Copyright (c) 2026 STAR Project
// Licensed under MIT

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include "star/v1/battery_management.pb.h"
#include "star/v1/motor_control.pb.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

#include "star_gateway_bridge/message_converter.hpp"

namespace star {

// Test fixture for MessageConverter tests
class MessageConverterTest : public ::testing::Test {
protected:
  MessageConverter        converter_;
  static constexpr double TOLERANCE    = 1e-6;
  static constexpr double WHEEL_BASE_M = 0.150; // 150mm wheel base
};

// =============================================================================
// Forward Kinematics Tests (Twist → VelocityCommand)
// =============================================================================

TEST_F(MessageConverterTest, TwistToCommandZeroVelocity)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 0.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  EXPECT_NEAR(command.motor_0_velocity_mps(), 0.0, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandPureTranslation)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0; // 1 m/s forward
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Both wheels should move at same speed
  EXPECT_NEAR(command.motor_0_velocity_mps(), 1.0, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), 1.0, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandPureRotation)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 0.0;
  twist.angular.z = 1.0; // 1 rad/s counter-clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Differential rotation: v_left = -ω*L/2, v_right = +ω*L/2
  double expected_velocity = twist.angular.z * WHEEL_BASE_M / 2.0; // ω * L/2 = 0.075 m/s

  EXPECT_NEAR(command.motor_0_velocity_mps(), -expected_velocity, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), expected_velocity, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandMixedMotion)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0; // 1 m/s forward
  twist.angular.z = 2.0; // 2 rad/s counter-clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // v_left = v - ω*L/2
  // v_right = v + ω*L/2
  double rotational_component = 2.0 * WHEEL_BASE_M / 2.0;   // 0.150 m/s
  double expected_left        = 1.0 - rotational_component; // 0.850 m/s
  double expected_right       = 1.0 + rotational_component; // 1.150 m/s

  EXPECT_NEAR(command.motor_0_velocity_mps(), expected_left, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), expected_right, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandNegativeLinear)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = -0.5; // 0.5 m/s backward
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Both wheels should be negative (backward)
  EXPECT_NEAR(command.motor_0_velocity_mps(), -0.5, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), -0.5, TOLERANCE);
}

TEST_F(MessageConverterTest, TwistToCommandNegativeAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 0.0;
  twist.angular.z = -1.0; // 1 rad/s clockwise

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Clockwise rotation: left positive, right negative
  double expected_velocity = std::abs(twist.angular.z) * WHEEL_BASE_M / 2.0;

  EXPECT_NEAR(command.motor_0_velocity_mps(), expected_velocity, TOLERANCE);
  EXPECT_NEAR(command.motor_1_velocity_mps(), -expected_velocity, TOLERANCE);
}

// =============================================================================
// Inverse Kinematics Tests (VelocityCommand → Twist)
// =============================================================================

TEST_F(MessageConverterTest, CommandToTwistZeroVelocity)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(0.0);
  command.set_motor_1_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  EXPECT_NEAR(twist.linear.x, 0.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistPureTranslation)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(1.0);
  command.set_motor_1_velocity_mps(1.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // v = (v_left + v_right) / 2
  EXPECT_NEAR(twist.linear.x, 1.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistPureRotation)
{
  // For pure rotation: v_left = -v_right
  double wheel_velocity = 0.075; // Arbitrary value

  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(-wheel_velocity);
  command.set_motor_1_velocity_mps(wheel_velocity);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // ω = (v_right - v_left) / L
  double expected_angular = (wheel_velocity - (-wheel_velocity)) / WHEEL_BASE_M;

  EXPECT_NEAR(twist.linear.x, 0.0, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, expected_angular, TOLERANCE);
}

TEST_F(MessageConverterTest, CommandToTwistMixedMotion)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(0.5);
  command.set_motor_1_velocity_mps(1.5);

  geometry_msgs::msg::Twist twist;
  ASSERT_TRUE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));

  // v = (v_left + v_right) / 2 = (0.5 + 1.5) / 2 = 1.0
  // ω = (v_right - v_left) / L = (1.5 - 0.5) / 0.150 = 6.667
  double expected_linear  = (0.5 + 1.5) / 2.0;
  double expected_angular = (1.5 - 0.5) / WHEEL_BASE_M;

  EXPECT_NEAR(twist.linear.x, expected_linear, TOLERANCE);
  EXPECT_NEAR(twist.angular.z, expected_angular, TOLERANCE);
}

// =============================================================================
// Kinematics Roundtrip Tests (Twist → Command → Twist)
// =============================================================================

TEST_F(MessageConverterTest, KinematicsRoundtripZero)
{
  geometry_msgs::msg::Twist original_twist;
  original_twist.linear.x  = 0.0;
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
  original_twist.linear.x  = 1.23;
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
  original_twist.linear.x  = 0.0;
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
  original_twist.linear.x  = 0.75;
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
  twist.linear.x  = std::numeric_limits<double>::quiet_NaN();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandNaNAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 0.0;
  twist.angular.z = std::numeric_limits<double>::quiet_NaN();

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandInfinityLinear)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = std::numeric_limits<double>::infinity();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandInfinityAngular)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 0.0;
  twist.angular.z = std::numeric_limits<double>::infinity();

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandNegativeInfinity)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = -std::numeric_limits<double>::infinity();
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, TwistToCommandZeroWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, 0.0));
}

TEST_F(MessageConverterTest, TwistToCommandNegativeWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0;
  twist.angular.z = 0.0;

  star::v1::VelocityCommand command;
  ASSERT_FALSE(converter_.twist_to_velocity_command(twist, command, -0.150));
}

TEST_F(MessageConverterTest, CommandToTwistNaNMotor0)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(std::numeric_limits<double>::quiet_NaN());
  command.set_motor_1_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, CommandToTwistNaNMotor1)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(0.0);
  command.set_motor_1_velocity_mps(std::numeric_limits<double>::quiet_NaN());

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

TEST_F(MessageConverterTest, CommandToTwistInfinityMotor0)
{
  star::v1::VelocityCommand command;
  command.set_motor_0_velocity_mps(std::numeric_limits<double>::infinity());
  command.set_motor_1_velocity_mps(0.0);

  geometry_msgs::msg::Twist twist;
  ASSERT_FALSE(converter_.velocity_command_to_twist(command, twist, WHEEL_BASE_M));
}

// =============================================================================
// Battery State Conversion Tests
// =============================================================================

TEST_F(MessageConverterTest, BatteryStateToProtoNominal)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage             = 12.6; // V
  ros_battery.current             = -2.5; // A (negative = discharging)
  ros_battery.percentage          = 85.0; // %
  ros_battery.charge              = 3.0;  // Ah
  ros_battery.capacity            = 5.0;  // Ah
  ros_battery.design_capacity     = 5.0;  // Ah
  ros_battery.power_supply_status = sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_DISCHARGING;
  ros_battery.power_supply_health = sensor_msgs::msg::BatteryState::POWER_SUPPLY_HEALTH_GOOD;
  ros_battery.present             = true;

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  // Voltage: V → mV
  EXPECT_NEAR(proto_battery.voltage_mv(), 12600.0, 1.0);

  // Current: A → mA
  EXPECT_NEAR(proto_battery.current_ma(), -2500.0, 1.0);

  // State of Charge: % (unchanged)
  EXPECT_NEAR(proto_battery.state_of_charge_percent(), 85.0, TOLERANCE);

  // Charge: Ah → mAh
  EXPECT_NEAR(proto_battery.charge_mah(), 3000.0, 1.0);

  // Capacity: Ah → mAh
  EXPECT_NEAR(proto_battery.capacity_mah(), 5000.0, 1.0);

  // Status
  EXPECT_EQ(proto_battery.status(), star::v1::BatteryState_ChargeStatus_CHARGE_STATUS_DISCHARGING);

  // Health
  EXPECT_EQ(proto_battery.health(), star::v1::BatteryState_Health_HEALTH_GOOD);

  // Present
  EXPECT_TRUE(proto_battery.present());
}

TEST_F(MessageConverterTest, BatteryStateToProtoZeroValues)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage             = 0.0;
  ros_battery.current             = 0.0;
  ros_battery.percentage          = 0.0;
  ros_battery.charge              = 0.0;
  ros_battery.capacity            = 0.0;
  ros_battery.design_capacity     = 0.0;
  ros_battery.power_supply_status = sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_UNKNOWN;
  ros_battery.power_supply_health = sensor_msgs::msg::BatteryState::POWER_SUPPLY_HEALTH_UNKNOWN;
  ros_battery.present             = false;

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  EXPECT_NEAR(proto_battery.voltage_mv(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_battery.current_ma(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_battery.state_of_charge_percent(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_battery.charge_mah(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_battery.capacity_mah(), 0.0, TOLERANCE);
  EXPECT_EQ(proto_battery.status(), star::v1::BatteryState_ChargeStatus_CHARGE_STATUS_UNKNOWN);
  EXPECT_EQ(proto_battery.health(), star::v1::BatteryState_Health_HEALTH_UNKNOWN);
  EXPECT_FALSE(proto_battery.present());
}

TEST_F(MessageConverterTest, BatteryStateToProtoCharging)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage             = 13.8;
  ros_battery.current             = 3.2; // Positive = charging
  ros_battery.percentage          = 65.0;
  ros_battery.power_supply_status = sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_CHARGING;

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  EXPECT_NEAR(proto_battery.voltage_mv(), 13800.0, 1.0);
  EXPECT_NEAR(proto_battery.current_ma(), 3200.0, 1.0);
  EXPECT_EQ(proto_battery.status(), star::v1::BatteryState_ChargeStatus_CHARGE_STATUS_CHARGING);
}

TEST_F(MessageConverterTest, BatteryStateToProtoFull)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage             = 12.6;
  ros_battery.current             = 0.0;
  ros_battery.percentage          = 100.0;
  ros_battery.power_supply_status = sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_FULL;

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  EXPECT_NEAR(proto_battery.state_of_charge_percent(), 100.0, TOLERANCE);
  EXPECT_EQ(proto_battery.status(), star::v1::BatteryState_ChargeStatus_CHARGE_STATUS_FULL);
}

TEST_F(MessageConverterTest, BatteryStateToProtoNaNVoltage)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage = std::numeric_limits<double>::quiet_NaN();

  star::v1::BatteryState proto_battery;
  ASSERT_FALSE(converter_.battery_state_to_proto(ros_battery, proto_battery));
}

TEST_F(MessageConverterTest, BatteryStateToProtoInfinityCurrent)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage = 12.0;
  ros_battery.current = std::numeric_limits<double>::infinity();

  star::v1::BatteryState proto_battery;
  ASSERT_FALSE(converter_.battery_state_to_proto(ros_battery, proto_battery));
}

TEST_F(MessageConverterTest, BatteryStateToProtoNegativePercentage)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage    = 12.0;
  ros_battery.percentage = -5.0; // Invalid

  star::v1::BatteryState proto_battery;
  ASSERT_FALSE(converter_.battery_state_to_proto(ros_battery, proto_battery));
}

TEST_F(MessageConverterTest, BatteryStateToProtoOverHundredPercent)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage    = 12.0;
  ros_battery.percentage = 105.0; // Invalid

  star::v1::BatteryState proto_battery;
  ASSERT_FALSE(converter_.battery_state_to_proto(ros_battery, proto_battery));
}

// =============================================================================
// PID Configuration Tests
// =============================================================================

TEST_F(MessageConverterTest, PidConfigToProtoNominal)
{
  // ROS2 PID gains (dimensionless)
  double kp = 1.5;
  double ki = 0.3;
  double kd = 0.05;

  star::v1::PIDConfig proto_config;
  ASSERT_TRUE(converter_.pid_gains_to_proto(kp, ki, kd, proto_config));

  // Verify gains are preserved
  EXPECT_NEAR(proto_config.kp(), kp, TOLERANCE);
  EXPECT_NEAR(proto_config.ki(), ki, TOLERANCE);
  EXPECT_NEAR(proto_config.kd(), kd, TOLERANCE);
}

TEST_F(MessageConverterTest, PidConfigToProtoZeroGains)
{
  star::v1::PIDConfig proto_config;
  ASSERT_TRUE(converter_.pid_gains_to_proto(0.0, 0.0, 0.0, proto_config));

  EXPECT_NEAR(proto_config.kp(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_config.ki(), 0.0, TOLERANCE);
  EXPECT_NEAR(proto_config.kd(), 0.0, TOLERANCE);
}

TEST_F(MessageConverterTest, PidConfigToProtoNegativeKp)
{
  star::v1::PIDConfig proto_config;
  ASSERT_FALSE(converter_.pid_gains_to_proto(-1.0, 0.3, 0.05, proto_config));
}

TEST_F(MessageConverterTest, PidConfigToProtoNegativeKi)
{
  star::v1::PIDConfig proto_config;
  ASSERT_FALSE(converter_.pid_gains_to_proto(1.5, -0.3, 0.05, proto_config));
}

TEST_F(MessageConverterTest, PidConfigToProtoNegativeKd)
{
  star::v1::PIDConfig proto_config;
  ASSERT_FALSE(converter_.pid_gains_to_proto(1.5, 0.3, -0.05, proto_config));
}

TEST_F(MessageConverterTest, PidConfigToProtoNaN)
{
  star::v1::PIDConfig proto_config;
  ASSERT_FALSE(converter_.pid_gains_to_proto(std::numeric_limits<double>::quiet_NaN(), 0.3, 0.05, proto_config));
}

TEST_F(MessageConverterTest, PidConfigToProtoInfinity)
{
  star::v1::PIDConfig proto_config;
  ASSERT_FALSE(converter_.pid_gains_to_proto(1.5, std::numeric_limits<double>::infinity(), 0.05, proto_config));
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(MessageConverterTest, TwistToCommandMaxVelocity)
{
  // Test with high velocities (near physical limits)
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 10.0; // 10 m/s (unrealistic but valid)
  twist.angular.z = 20.0; // 20 rad/s

  star::v1::VelocityCommand command;
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, WHEEL_BASE_M));

  // Should not crash or produce NaN
  EXPECT_FALSE(std::isnan(command.motor_0_velocity_mps()));
  EXPECT_FALSE(std::isnan(command.motor_1_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.motor_0_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.motor_1_velocity_mps()));
}

TEST_F(MessageConverterTest, TwistToCommandVerySmallWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0;
  twist.angular.z = 1.0;

  star::v1::VelocityCommand command;
  // Very small but positive wheel base (1mm)
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, 0.001));

  // Should produce large angular velocities but not overflow
  EXPECT_FALSE(std::isinf(command.motor_0_velocity_mps()));
  EXPECT_FALSE(std::isinf(command.motor_1_velocity_mps()));
}

TEST_F(MessageConverterTest, TwistToCommandVeryLargeWheelBase)
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x  = 1.0;
  twist.angular.z = 1.0;

  star::v1::VelocityCommand command;
  // Large wheel base (10m)
  ASSERT_TRUE(converter_.twist_to_velocity_command(twist, command, 10.0));

  EXPECT_FALSE(std::isnan(command.motor_0_velocity_mps()));
  EXPECT_FALSE(std::isnan(command.motor_1_velocity_mps()));
}

TEST_F(MessageConverterTest, BatteryStateToProtoVeryLargeCurrent)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage = 12.0;
  ros_battery.current = 1000.0; // 1000A (unrealistic but valid)

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  // Should convert correctly: 1000A = 1,000,000mA
  EXPECT_NEAR(proto_battery.current_ma(), 1000000.0, 1.0);
}

TEST_F(MessageConverterTest, BatteryStateToProtoVerySmallVoltage)
{
  sensor_msgs::msg::BatteryState ros_battery;
  ros_battery.voltage = 0.001; // 1mV

  star::v1::BatteryState proto_battery;
  ASSERT_TRUE(converter_.battery_state_to_proto(ros_battery, proto_battery));

  // Should convert correctly: 0.001V = 1mV
  EXPECT_NEAR(proto_battery.voltage_mv(), 1.0, 0.1);
}

} // namespace star

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
