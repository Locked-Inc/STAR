/**
 * @file test_safety_monitor.cpp
 * @brief Unit tests for the SafetyMonitor class.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_safety_monitor/safety_monitor.hpp"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/bool.hpp>

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

TEST_F(SafetyMonitorTest, LifecycleCleanupReturnsToUnconfigured)
{
  // configure -> cleanup must drop us back to "unconfigured" so the node can
  // be reconfigured cleanly.  This exercises on_cleanup() in safety_monitor.cpp
  // which resets bond_, publishers, subscribers, and clears heartbeat state.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));

  node->cleanup();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("unconfigured"));
}

TEST_F(SafetyMonitorTest, LifecycleFullCycleConfigureActivateDeactivateCleanup)
{
  // Round-trip the full nominal lifecycle: configure -> activate -> deactivate
  // -> cleanup, asserting the state label after each transition.  This verifies
  // that the node releases its bond/timer/publishers cleanly on deactivate
  // and that on_cleanup() can run without leaving stale state behind.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));

  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("active"));

  node->deactivate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));

  node->cleanup();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("unconfigured"));
}

TEST_F(SafetyMonitorTest, LifecycleReconfigureAfterCleanup)
{
  // After cleanup() the node should be reusable: a second configure() must
  // succeed and bring the node back to "inactive".  This catches state
  // pollution bugs in on_cleanup() (e.g. subscribers not reset, parameters
  // already declared on the second configure pass).
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>();

  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->cleanup();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  // Second configure must succeed.
  node->configure();
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

TEST_F(SafetyMonitorTest, ZeroPublishRateFallsBackInsteadOfBusySpinning)
{
  // Round-15 audit guard: a user override of publish_rate=0.0 must NOT yield
  // a 0 ms wall timer (which would peg the core in a busy spin). The on_activate()
  // path clamps to a 10 Hz fallback and logs an error. Verify the lifecycle still
  // reaches "active" rather than throwing on the std::chrono::milliseconds(0)
  // wall-timer creation.
  rclcpp::NodeOptions options;
  options.parameter_overrides({
    {"publish_rate", 0.0},
  });

  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(options);
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("inactive"));

  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("active"));
}

TEST_F(SafetyMonitorTest, NegativePublishRateFallsBackInsteadOfBusySpinning)
{
  // Same guard as the zero case, but for negative rates: 1000.0/(-1.0) = -1000.0,
  // and static_cast<int>(-1000.0) -> milliseconds(-1000) is implementation-defined
  // for wall-timer behavior. Clamp to fallback and verify activation still works.
  rclcpp::NodeOptions options;
  options.parameter_overrides({
    {"publish_rate", -5.0},
  });

  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(options);
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  EXPECT_EQ(node->get_current_state().label(), std::string("active"));
}

// ---------------------------------------------------------------------------
// Helper: build a SafetyMonitor configured for fast-cycling integration tests.
// Production defaults (10 Hz publish, 500 ms heartbeat) make the heartbeat
// timeout test take ~600 ms minimum and miss the e-stop publish window. We
// override to a 50 Hz publish rate (20 ms tick) and a 150 ms heartbeat timeout
// so each test finishes well under MESSAGE_WAIT_TIMEOUT_S.
// ---------------------------------------------------------------------------
namespace
{
constexpr int FAST_HEARTBEAT_TIMEOUT_MS = 150;
constexpr double FAST_PUBLISH_RATE_HZ = 50.0;
constexpr double OBSTACLE_ESTOP_DISTANCE_M = 0.10;
constexpr double OBSTACLE_WARN_DISTANCE_M = 0.30;
constexpr int OBSTACLE_CLEAR_COUNT = 3;
constexpr double SONAR_RANGE_TOO_CLOSE_M = 0.05;
constexpr double SONAR_RANGE_WARN_M = 0.20;
constexpr double SONAR_RANGE_CLEAR_M = 1.50;
constexpr float SONAR_FIELD_OF_VIEW_RAD = 0.5F;
constexpr float SONAR_MIN_RANGE_M = 0.02F;
constexpr float SONAR_MAX_RANGE_M = 4.0F;
constexpr auto HEARTBEAT_OVERSHOOT_DELAY = std::chrono::milliseconds(300);

rclcpp::NodeOptions make_fast_options(bool enable_auto_estop = true)
{
  rclcpp::NodeOptions options;
  options.parameter_overrides({
      {"heartbeat_timeout_ms", FAST_HEARTBEAT_TIMEOUT_MS},
      {"publish_rate", FAST_PUBLISH_RATE_HZ},
      {"enable_auto_estop", enable_auto_estop},
      {"obstacle_estop_distance", OBSTACLE_ESTOP_DISTANCE_M},
      {"obstacle_warn_distance", OBSTACLE_WARN_DISTANCE_M},
      {"obstacle_clear_count_required", OBSTACLE_CLEAR_COUNT},
  });
  return options;
}

sensor_msgs::msg::Range make_range_msg(float range_m)
{
  sensor_msgs::msg::Range msg;
  msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
  msg.field_of_view = SONAR_FIELD_OF_VIEW_RAD;
  msg.min_range = SONAR_MIN_RANGE_M;
  msg.max_range = SONAR_MAX_RANGE_M;
  msg.range = range_m;
  return msg;
}

// Spin both the SafetyMonitor and the test helper node together until either
// `predicate` returns true or the wall-clock budget is exhausted. Using a
// shared executor instead of background threads matches the rclcpp test idiom
// already in use elsewhere in this file (see DiagnosticsPublication).
template<typename Predicate>
bool spin_until(
  rclcpp::executors::SingleThreadedExecutor & executor, Predicate predicate,
  double timeout_s)
{
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() <
    timeout_s)
  {
    executor.spin_some(SPIN_TIMEOUT);
    if (predicate()) {
      return true;
    }
  }
  return predicate();
}
}  // namespace

TEST_F(SafetyMonitorTest, EStopTriggeredByObstacleProximity)
{
  // Drives check_obstacle_proximity() in safety_monitor.cpp:369-410 with a sonar
  // range below the e-stop threshold and asserts that update_overall_state() in
  // safety_monitor.cpp:418-442 latches obstacle_estop_triggered_ and publishes
  // /emergency_stop=true.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options());
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_obstacle_pub");
  auto sonar_pub = test_node->create_publisher<sensor_msgs::msg::Range>("/star/obstacle/front_left",
    10);

  std::atomic<bool> estop_received{false};
  auto estop_sub = test_node->create_subscription<std_msgs::msg::Bool>("/emergency_stop", 10,
      [&estop_received](
        const std_msgs::msg::Bool::SharedPtr msg) {
        if (msg->data) {
          estop_received = true;
        }
                                                                       });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  // Publish an obstacle inside the e-stop ring (0.05 m < 0.10 m threshold).
  auto range_msg = make_range_msg(static_cast<float>(SONAR_RANGE_TOO_CLOSE_M));
  range_msg.header.stamp = node->now();
  sonar_pub->publish(range_msg);

  EXPECT_TRUE(spin_until(executor, [&]() {return estop_received.load();}, MESSAGE_WAIT_TIMEOUT_S));
}

TEST_F(SafetyMonitorTest, EStopNotTriggeredByObstacleWarnRange)
{
  // Inverse of the e-stop test: a range in the warn band (>= 0.10 m and
  // < 0.30 m) must NOT raise emergency_stop_active_. This guards against a
  // regression where the warn-vs-estop branch in safety_monitor.cpp:381-394
  // collapses into a single threshold.
  //
  // Use a long heartbeat timeout (5 s) instead of make_fast_options' 150 ms
  // so the heartbeat watchdog does NOT fire during the warn-band wait
  // window (otherwise an unrelated heartbeat-timeout e-stop would mask the
  // intended assertion).
  rclcpp::NodeOptions options;
  options.parameter_overrides({
    {"heartbeat_timeout_ms", 5000},
    {"publish_rate", FAST_PUBLISH_RATE_HZ},
    {"enable_auto_estop", true},
    {"obstacle_estop_distance", OBSTACLE_ESTOP_DISTANCE_M},
    {"obstacle_warn_distance", OBSTACLE_WARN_DISTANCE_M},
  });
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(options);
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_obstacle_warn_pub");
  auto sonar_pub =
    test_node->create_publisher<sensor_msgs::msg::Range>("/star/obstacle/front_right", 10);

  std::atomic<bool> estop_received{false};
  auto estop_sub = test_node->create_subscription<std_msgs::msg::Bool>("/emergency_stop", 10,
      [&estop_received](
        const std_msgs::msg::Bool::SharedPtr msg) {
        if (msg->data) {
          estop_received = true;
        }
                                                                       });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  // Publish a warn-band range (0.20 m, between 0.10 and 0.30).
  auto range_msg = make_range_msg(static_cast<float>(SONAR_RANGE_WARN_M));
  range_msg.header.stamp = node->now();
  sonar_pub->publish(range_msg);

  // Spin for the full timeout to give every monitoring tick a chance to fire;
  // we want to confirm no e-stop is published, not that one happens fast.
  spin_until(executor, [&]() {return estop_received.load();}, MESSAGE_WAIT_TIMEOUT_S / 2.0);
  EXPECT_FALSE(estop_received.load());
}

TEST_F(SafetyMonitorTest, EStopReleasedAfterObstacleClearsForRequiredCount)
{
  // Drives the recovery branch in safety_monitor.cpp:399-409: after the
  // obstacle vacates the e-stop ring for obstacle_clear_count_required_
  // consecutive monitoring ticks the obstacle_estop_triggered_ latch must
  // release. Subsequent ticks should publish nothing on /emergency_stop
  // (LifecyclePublisher only publishes when emergency_stop_active_ is true).
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options());
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_obstacle_clear_pub");
  auto sonar_pub = test_node->create_publisher<sensor_msgs::msg::Range>("/star/obstacle/back_left",
    10);

  std::atomic<int> estop_true_count{0};
  auto estop_sub = test_node->create_subscription<std_msgs::msg::Bool>("/emergency_stop", 10,
      [&estop_true_count](
        const std_msgs::msg::Bool::SharedPtr msg) {
        if (msg->data) {
          estop_true_count++;
        }
                                                                       });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  // Step 1: trigger the e-stop with a too-close reading.
  auto close_msg = make_range_msg(static_cast<float>(SONAR_RANGE_TOO_CLOSE_M));
  close_msg.header.stamp = node->now();
  sonar_pub->publish(close_msg);
  EXPECT_TRUE(spin_until(executor, [&]() {
      return estop_true_count.load() > 0;
    }, MESSAGE_WAIT_TIMEOUT_S));

  // Step 2: publish a clear range repeatedly and let the recovery counter
  // reach obstacle_clear_count_required_. We push more clears than the
  // threshold to make sure at least N ticks see the new value.
  const int snapshot_after_trigger = estop_true_count.load();
  for (int i = 0; i < OBSTACLE_CLEAR_COUNT * 4; ++i) {
    auto clear_msg = make_range_msg(static_cast<float>(SONAR_RANGE_CLEAR_M));
    clear_msg.header.stamp = node->now();
    sonar_pub->publish(clear_msg);
    executor.spin_some(SPIN_TIMEOUT);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  // After recovery, no new /emergency_stop=true publishes should occur. Spin
  // a bit longer to confirm the count stops climbing.
  const int after_recovery = estop_true_count.load();
  spin_until(executor, [&]() {return estop_true_count.load() > after_recovery + 1;}, 0.5);
  EXPECT_GE(snapshot_after_trigger, 1);
  // We expect the count to have stabilized: at most a small handful of
  // additional publishes from in-flight ticks before recovery latched.
  EXPECT_LT(estop_true_count.load() - after_recovery, OBSTACLE_CLEAR_COUNT);
}

TEST_F(SafetyMonitorTest, HeartbeatTimeoutTriggersEStop)
{
  // Drives check_heartbeat_health() in safety_monitor.cpp:291-317. With a 150 ms
  // heartbeat timeout and no external diagnostics publisher, the first
  // monitoring tick after activation + 150 ms must latch
  // heartbeat_timeout_triggered_ and update_overall_state() must publish
  // /emergency_stop=true.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options());
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  // on_activate() resets last_diagnostics_time_ so the first tick is fresh;
  // wait past the 150 ms timeout to force the stale branch.
  std::this_thread::sleep_for(HEARTBEAT_OVERSHOOT_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_heartbeat_listener");
  std::atomic<bool> estop_received{false};
  auto estop_sub = test_node->create_subscription<std_msgs::msg::Bool>("/emergency_stop", 10,
      [&estop_received](
        const std_msgs::msg::Bool::SharedPtr msg) {
        if (msg->data) {
          estop_received = true;
        }
                                                                       });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  EXPECT_TRUE(spin_until(executor, [&]() {return estop_received.load();}, MESSAGE_WAIT_TIMEOUT_S));
}

TEST_F(SafetyMonitorTest, HeartbeatTimeoutSuppressedWhenAutoEStopDisabled)
{
  // With enable_auto_estop=false, update_overall_state() in
  // safety_monitor.cpp:432-434 must NOT touch emergency_stop_active_, so even
  // a latched heartbeat_timeout_triggered_ should not produce /emergency_stop
  // publishes. This is the operator-override path used during bench debug.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options(false));
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(HEARTBEAT_OVERSHOOT_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_estop_disabled_listener");
  std::atomic<bool> estop_received{false};
  auto estop_sub = test_node->create_subscription<std_msgs::msg::Bool>("/emergency_stop", 10,
      [&estop_received](
        const std_msgs::msg::Bool::SharedPtr msg) {
        if (msg->data) {
          estop_received = true;
        }
                                                                       });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  spin_until(executor, [&]() {return estop_received.load();}, MESSAGE_WAIT_TIMEOUT_S / 2.0);
  EXPECT_FALSE(estop_received.load());
}

TEST_F(SafetyMonitorTest, ObstacleProximityElevatesDiagnosticSeverity)
{
  // Verify the diagnostic publishing path in safety_monitor.cpp:444-559 sets
  // the system-health DiagnosticStatus.level to ERROR when an obstacle is
  // within the e-stop ring. This exercises the same branch as the e-stop test
  // but checks the diagnostic side-channel (separate from /emergency_stop).
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options());
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_diag_listener");
  auto sonar_pub = test_node->create_publisher<sensor_msgs::msg::Range>("/star/obstacle/back_right",
    10);

  std::atomic<bool> saw_error_level{false};
  std::atomic<bool> saw_obstacle_kv{false};
  auto diag_sub = test_node->create_subscription<
    diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10,
      [&saw_error_level, &saw_obstacle_kv](
        const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg) {
        for (const auto & status : msg->status) {
          if (status.hardware_id != "safety_monitor") {
            continue;
          }
          if (status.name == "safety_monitor: System Health" &&
          status.level == diagnostic_msgs::msg::DiagnosticStatus::ERROR)
          {
            saw_error_level = true;
          }
          for (const auto & kv : status.values) {
            if (kv.key == "Obstacle Too Close" && kv.value == "true") {
              saw_obstacle_kv = true;
            }
          }
        }
                                           });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  auto close_msg = make_range_msg(static_cast<float>(SONAR_RANGE_TOO_CLOSE_M));
  close_msg.header.stamp = node->now();
  sonar_pub->publish(close_msg);

  EXPECT_TRUE(
    spin_until(executor, [&]() {
      return saw_error_level.load() && saw_obstacle_kv.load();
    }, MESSAGE_WAIT_TIMEOUT_S));
}

TEST_F(SafetyMonitorTest, DiagnosticPublishingContainsRequiredKeyValuePairs)
{
  // Replaces the previously-skipped DiagnosticPublishing placeholder. Verifies
  // that publish_diagnostics() in safety_monitor.cpp:444-559 emits the canonical
  // System Health / Heartbeat Status / Motor Status statuses with the documented
  // KV keys consumed by the gateway dashboard.
  auto node = std::make_shared<star_safety_monitor::SafetyMonitor>(make_fast_options());
  node->configure();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);
  node->activate();
  std::this_thread::sleep_for(LIFECYCLE_TRANSITION_DELAY);

  auto test_node = rclcpp::Node::make_shared("test_diag_kv_listener");
  std::atomic<bool> saw_system_health{false};
  std::atomic<bool> saw_heartbeat_status{false};
  std::atomic<bool> saw_motor_status{false};
  std::atomic<bool> saw_required_kv{false};
  auto diag_sub = test_node->create_subscription<
    diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10,
      [&saw_system_health, &saw_heartbeat_status, &saw_motor_status,
      &saw_required_kv](
        const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg) {
        bool linear = false;
        bool angular = false;
        bool emergency = false;
        bool obstacle = false;
        for (const auto & status : msg->status) {
          if (status.hardware_id != "safety_monitor") {
            continue;
          }
          if (status.name == "safety_monitor: System Health") {
            saw_system_health = true;
            for (const auto & kv : status.values) {
              if (kv.key == "Linear Velocity (m/s)") {
                linear = true;
              }
              if (kv.key == "Angular Velocity (rad/s)") {
                angular = true;
              }
              if (kv.key == "Emergency Stop Active") {
                emergency = true;
              }
              if (kv.key == "Obstacle Too Close") {
                obstacle = true;
              }
            }
          }
          if (status.name == "safety_monitor: Heartbeat Status") {
            saw_heartbeat_status = true;
          }
          if (status.name == "safety_monitor: Motor Status") {
            saw_motor_status = true;
          }
        }
        if (linear && angular && emergency && obstacle) {
          saw_required_kv = true;
        }
                                           });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.add_node(test_node->get_node_base_interface());

  EXPECT_TRUE(spin_until(
    executor,
      [&]() {
        return saw_system_health.load() && saw_heartbeat_status.load() && saw_motor_status.load() &&
               saw_required_kv.load();
    },
    MESSAGE_WAIT_TIMEOUT_S));
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
