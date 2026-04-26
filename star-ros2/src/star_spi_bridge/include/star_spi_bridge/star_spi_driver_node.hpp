/**
 * @file star_spi_driver_node.hpp
 * @brief ROS2 lifecycle node for the STAR SPI bridge to the RX72N peripheral MCU.
 *
 * @details
 * Declares StarSpiDriverNode, a rclcpp_lifecycle::LifecycleNode that owns the
 * SpiDriver and SpiMessageConverter instances.  At 100 Hz the node sends a
 * framed protobuf VelocityCommand to the RX72N over SPI and publishes the
 * decoded TelemetryData as standard ROS2 messages (odometry, joint states,
 * IMU, obstacle ranges).
 *
 * Lifecycle transitions:
 *  - on_configure  : initialize SPI device, create publishers/subscriptions.
 *  - on_activate   : start 100 Hz timer, activate lifecycle publishers.
 *  - on_deactivate : send zero-velocity frame, stop timer, deactivate publishers.
 *  - on_cleanup    : release all resources.
 *  - on_shutdown   : delegate to on_cleanup.
 *
 * @author Locked Inc.
 * @date 2026
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>

#include "star_spi_bridge/spi_driver.hpp"
#include "star_spi_bridge/spi_message_converter.hpp"

namespace star_spi_bridge
{

class StarSpiDriverNode : public rclcpp_lifecycle::LifecycleNode {
public:
  explicit StarSpiDriverNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~StarSpiDriverNode() override;

  // Lifecycle transitions
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State & prev_state) override;
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State & prev_state) override;

private:
  // Callbacks
  /**
   * @brief ROS2 subscription callback for incoming velocity commands.
   *
   * @details
   * Called by the ROS2 executor thread when a new geometry_msgs::msg::Twist
   * arrives on the "cmd_vel" topic.  Stores the message in current_cmd_vel_
   * and records the receive timestamp in last_cmd_vel_time_ so the 100 Hz
   * timer watchdog can detect stale commands.
   *
   * @param[in] msg  Shared pointer to the received Twist message.
   *
   * @pre  The ROS2 node and cmd_vel subscription are fully initialized (i.e.,
   *       on_configure() has completed successfully).
   * @pre  msg != nullptr and contains a valid, finite Twist (linear/angular).
   *
   * @note Executed on the ROS2 executor thread (single-threaded executor by
   *       default).  Not concurrently re-entrant; no mutex is required provided
   *       a single-threaded executor is used.  If a multi-threaded executor is
   *       used, access to current_cmd_vel_ and last_cmd_vel_time_ must be guarded.
   *
   * @post current_cmd_vel_ updated to the received velocity.
   * @post last_cmd_vel_time_ updated to the current ROS clock time.
   *
   * @since Version 1.0.0
   */
  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg);

  /**
   * @brief ROS2 subscription callback for emergency stop requests.
   *
   * @details
   * Called by the ROS2 executor thread when a std_msgs::msg::Bool arrives on
   * the "emergency_stop" topic with data == true.  Serializes an
   * EmergencyStopRequest protobuf, encodes it as a Priority + RequiresAck SPI
   * frame, and performs a synchronous SPI transfer to the RX72N.
   *
   * @param[in] msg  Shared pointer to the received Bool message.  If msg->data
   *                 is false the callback returns immediately without action.
   *
   * @pre  The ROS2 executor is running and the emergency_stop subscription is
   *       active (on_configure() has completed successfully).
   * @pre  spi_driver_ is non-null and the SPI device is initialized; if null,
   *       an error is logged and the callback returns early without transferring.
   *
   * @note Executed on the ROS2 executor thread.  Blocks until the SPI transfer
   *       completes (typically < 1 ms at 10 MHz).  Not concurrently re-entrant
   *       with timer_callback; use a single-threaded executor.
   *
   * @post If msg->data is true, an EmergencyStopRequest frame is sent to the
   *       RX72N via SPI.  No ACK/NACK flow-control state is modified.
   * @post SPI transfer errors are logged via RCLCPP_ERROR but do not propagate.
   *
   * @since Version 1.0.0
   */
  void emergency_stop_callback(const std_msgs::msg::Bool::SharedPtr msg);

  /**
   * @brief 100 Hz periodic control and telemetry loop.
   *
   * @details
   * Fired at exactly 10 ms intervals by the wall timer created in on_activate().
   * Each invocation performs:
   *  1. Command-velocity watchdog: zero the velocity if no cmd_vel arrived
   *     within cmd_vel_timeout_ms_.
   *  2. ACK/NACK flow control: retransmit the last frame if pending_ack_ is
   *     set and retry_count_ has not exhausted K_MAX_RETRIES.
   *  3. Encode a SetVelocityRequest protobuf into a STAR SPI frame.
   *  4. Full-duplex SPI transfer (send command, receive telemetry simultaneously).
   *  5. Decode the received frame and dispatch on type (Ack, Nack, Response).
   *  6. On Response: parse TelemetryData and publish Odometry, JointState,
   *     IMU (if populated), obstacle Range messages, and obstacle detected flag.
   *
   * @par Frequency and Timing:
   * 100 Hz is required to match the RX72N firmware control loop rate and to
   * maintain sub-10ms command latency for safe differential drive operation.
   * Real-time budget: SPI transfer at 10 MHz for a max 1040-byte frame takes
   * approx. 0.83 ms; total callback execution must remain well under 10 ms.
   *
   * @pre  The ROS2 executor is running and the node is in the Active lifecycle
   *       state (on_activate() has completed successfully).
   * @pre  spi_driver_ is non-null and the SPI interface is initialized and
   *       ready for full-duplex transfer.
   *
   * @note Executed on the ROS2 executor thread.  Not re-entrant.  The timer
   *       is wall-clock based (not simulation-clock); on a real-time OS or
   *       with elevated thread priority the 10 ms period is met reliably.
   * @note No mutex protects current_cmd_vel_; assume a single-threaded executor.
   *
   * @post SPI frame transmitted and received each call.
   * @post ROS2 topics published when telemetry Response frames are decoded.
   *
   * @since Version 1.0.0
   */
  void timer_callback();  // 100 Hz loop

  // -- Private constants ---------------------------------------------------
  /** @brief Maximum number of retransmit attempts before dropping a frame. */
  static constexpr int K_MAX_RETRIES = 3;

  // Components
  std::unique_ptr<SpiDriver> spi_driver_;
  std::unique_ptr<SpiMessageConverter> converter_;

  // ROS handles -- subscriptions
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr emergency_stop_sub_;

  // ROS handles -- lifecycle publishers
  rclcpp_lifecycle::LifecyclePublisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Range>::SharedPtr obstacle_front_left_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Range>::SharedPtr obstacle_front_right_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Range>::SharedPtr obstacle_back_left_pub_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Range>::SharedPtr obstacle_back_right_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::Bool>::SharedPtr obstacle_detected_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::Int32>::SharedPtr
    estop_reason_pub_; /**< E-STOP reason code (star_v1_EstopReason, Int32). Valid when emergency_stop is true. */

  rclcpp::TimerBase::SharedPtr timer_;

  // State -- command velocity
  geometry_msgs::msg::Twist current_cmd_vel_;
  rclcpp::Time last_cmd_vel_time_;
  uint16_t tx_seq_ = 0;

  // State -- ACK/NACK flow control
  bool pending_ack_ = false;
  int retry_count_ = 0;
  uint16_t last_tx_seq_ = 0;
  std::vector<uint8_t> last_tx_frame_;

  // Parameters
  std::string spi_device_path_;
  int spi_speed_hz_;
  int cmd_vel_timeout_ms_;
};

}  // namespace star_spi_bridge
