/**
 * @file test_safety_monitor.cpp
 * @brief Unit tests for the SafetyMonitor class.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <rclcpp/rclcpp.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/bool.hpp>
#include "star_safety_monitor/safety_monitor.hpp"
#include <chrono>
#include <thread>
#include <gtest/gtest.h>

namespace
{
constexpr auto LIFECYCLE_TRANSITION_DELAY = std::chrono::milliseconds(100);
constexpr auto SPIN_TIMEOUT = std::chrono::milliseconds(100);
constexpr double MESSAGE_WAIT_TIMEOUT_S = 2.0;
}  // namespace

class SafetyMonitorTest : public ::testing::Test {
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
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));
}

TEST_F(SafetyMonitorTest, LifecycleActivation)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  // Configure
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  // Activate
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("active"));
}

TEST_F(SafetyMonitorTest, LifecycleDeactivation)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  // Configure and activate
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  // Deactivate
  node->deactivate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));
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
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  // Verify parameters were loaded
  auto hb_timeout = node->get_parameter("heartbeat_timeout_ms");
  EXPECT_EQ(hb_timeout.as_int(), 500);
}

TEST_F(SafetyMonitorTest, DiagnosticsPublication)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  // Create a test subscriber to receive diagnostics
  std::atomic<int> diag_count{0};
  auto test_node = rclcpp::Node::make_shared("test_node");
  auto diag_sub = test_node->create_subscription<
    diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10,
                                           [&diag_count](const diagnostic_msgs::msg::DiagnosticArray::SharedPtr) {
                                             diag_count++;
                                           });

  // Wait for a few diagnostic messages
  auto executor = rclcpp::executors::SingleThreadedExecutor();
  executor.add_node(test_node->get_node_base_interface());
  executor.add_node(node->get_node_base_interface());

  rclcpp::Time start_time = node->now();
  while (diag_count < 2 && (node->now() - start_time).seconds() < MESSAGE_WAIT_TIMEOUT_S) {
    executor.spin_some(SPIN_TIMEOUT);
  }

  EXPECT_GT(diag_count, 0);
}

TEST_F(SafetyMonitorTest, OdometrySubscription)
{
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

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
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  // Publish and spin
  odom_pub->publish(odom_msg);
  executor.spin_some(SPIN_TIMEOUT);

  // Give the node time to process
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  executor.spin_some(SPIN_TIMEOUT);

  // Test passes if no exceptions are thrown
  EXPECT_TRUE(true);
}

TEST_F(SafetyMonitorTest, EmergencyStopTrigger)
{
  // TODO(locked-in): Test E-Stop triggering logic
  GTEST_SKIP() << "Emergency stop tests not yet implemented";
}

TEST_F(SafetyMonitorTest, DiagnosticPublishing)
{
  // TODO(locked-in): Test diagnostic message generation
  GTEST_SKIP() << "Diagnostic publishing tests not yet implemented";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
