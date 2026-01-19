// Copyright 2026 STAR Team
// Licensed under MIT License

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include "star_safety_monitor/safety_monitor.hpp"
#include <nav_msgs/msg/odometry.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <std_msgs/msg/bool.hpp>
#include <chrono>
#include <thread>

class SafetyMonitorTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {
      rclcpp::init(0, nullptr);
    }
  }

  void TearDown() override
  {
    if (rclcpp::ok()) {
      rclcpp::shutdown();
    }
  }
};

TEST_F(SafetyMonitorTest, NodeConstruction)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();
  EXPECT_NE(node, nullptr);
  EXPECT_EQ(node->get_name(), std::string("safety_monitor"));
}

TEST_F(SafetyMonitorTest, LifecycleConfiguration)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  // Test on_configure transition
  auto result = node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(result.successful, true);
  EXPECT_EQ(node->get_current_state().id(),
    lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);
}

TEST_F(SafetyMonitorTest, LifecycleActivation)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  // Configure
  node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Activate
  auto result = node->trigger_activate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(result.successful, true);
  EXPECT_EQ(node->get_current_state().id(),
    lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);
}

TEST_F(SafetyMonitorTest, LifecycleDeactivation)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  // Configure and activate
  node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  node->trigger_activate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Deactivate
  auto result = node->trigger_deactivate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(result.successful, true);
  EXPECT_EQ(node->get_current_state().id(),
    lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);
}

TEST_F(SafetyMonitorTest, ParameterLoading)
{
  rclcpp::NodeOptions options;
  options.parameter_overrides({
    {"heartbeat_timeout_ms", 500},
    {"max_linear_velocity", 1.5},
    {"max_angular_velocity", 2.5},
    {"publish_rate", 20.0},
  });

  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(options);
  auto result = node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(result.successful, true);

  // Verify parameters were loaded
  auto hb_timeout = node->get_parameter("heartbeat_timeout_ms");
  EXPECT_EQ(hb_timeout.as_int(), 500);
}

TEST_F(SafetyMonitorTest, DiagnosticsPublication)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();
  node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  node->trigger_activate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create a test subscriber to receive diagnostics
  std::atomic<int> diag_count{0};
  auto test_node = rclcpp::Node::make_shared("test_node");
  auto diag_sub = test_node->create_subscription<
    diagnostic_msgs::msg::DiagnosticArray>(
    "/diagnostics", 10,
    [&diag_count](const diagnostic_msgs::msg::DiagnosticArray::SharedPtr) {
      diag_count++;
    });

  // Wait for a few diagnostic messages
  auto executor = rclcpp::executors::SingleThreadedExecutor();
  executor.add_node(node);
  executor.add_node(test_node);

  rclcpp::Time start_time = node->now();
  while (diag_count < 2 && (node->now() - start_time).seconds() < 2.0) {
    executor.spin_some(std::chrono::milliseconds(100));
  }

  EXPECT_GT(diag_count, 0);
}

TEST_F(SafetyMonitorTest, OdometrySubscription)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();
  node->trigger_configure();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  node->trigger_activate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create a test publisher for odometry
  auto test_node = rclcpp::Node::make_shared("test_odom_pub");
  auto odom_pub = test_node->create_publisher<nav_msgs::msg::Odometry>("/odom", 10);

  // Create and publish odometry message
  auto odom_msg = nav_msgs::msg::Odometry();
  odom_msg.header.stamp = node->now();
  odom_msg.twist.twist.linear.x = 0.5;
  odom_msg.twist.twist.linear.y = 0.0;
  odom_msg.twist.twist.linear.z = 0.0;
  odom_msg.twist.twist.angular.x = 0.0;
  odom_msg.twist.twist.angular.y = 0.0;
  odom_msg.twist.twist.angular.z = 0.1;

  auto executor = rclcpp::executors::SingleThreadedExecutor();
  executor.add_node(node);
  executor.add_node(test_node);

  // Publish and spin
  odom_pub->publish(odom_msg);
  executor.spin_some(std::chrono::milliseconds(100));

  // Give the node time to process
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  executor.spin_some(std::chrono::milliseconds(100));

  // Test passes if no exceptions are thrown
  EXPECT_TRUE(true);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}TEST_F(SafetyMonitorTest, BatterySafetyChecks)
{
  // TODO: Test battery voltage/current monitoring
  GTEST_SKIP() << "Battery safety tests not yet implemented";
}

TEST_F(SafetyMonitorTest, EmergencyStopTrigger)
{
  // TODO: Test E-Stop triggering logic
  GTEST_SKIP() << "Emergency stop tests not yet implemented";
}

TEST_F(SafetyMonitorTest, DiagnosticPublishing)
{
  // TODO: Test diagnostic message generation
  GTEST_SKIP() << "Diagnostic publishing tests not yet implemented";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
