/**
 * @file star_spi_driver_node.cpp
 * @brief ROS2 lifecycle node implementation for the STAR SPI bridge to the RX72N peripheral MCU.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_spi_bridge/star_spi_driver_node.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>
#include <tf2/LinearMath/Quaternion.h>  // NOLINT(build/include_order)

#include "star/v1/motor_control.pb.h"
#include "star/v1/telemetry.pb.h"

using namespace std::chrono_literals;

namespace star_spi_bridge
{
static constexpr int kLogThrottleMs = 1000;

// IMU sensor noise model (variance = sigma^2, all diagonal).
// sigma_orientation ~5.7 deg, sigma_angular_vel ~0.032 rad/s, sigma_accel ~0.1 m/s^2
static constexpr double kImuOrientationVar = 0.01;      // rad^2
static constexpr double kImuAngularVelocityVar = 0.001; // (rad/s)^2
static constexpr double kImuLinearAccelVar = 0.01;      // (m/s^2)^2

// SPI link defaults
static constexpr int kDefaultSpiSpeedHz = 10000000;     // 10 MHz
static constexpr int kDefaultCmdVelTimeoutMs = 500;     // command watchdog (ms)

// Drivetrain kinematics for the goBILDA Wasteland chassis. Source: CAD
// measurement (2026-04) and goBILDA motor datasheet. Must match the
// values in star_bringup/launch/slam.launch.py to keep producers and
// consumers in sync.
static constexpr double kDefaultTrackWidthM = 0.356;    // left-right wheel center-to-center
static constexpr double kDefaultWheelRadiusM = 0.072;   // 144 mm wheel / 2
static constexpr int kDefaultEncoderTicksPerRev = 11599; // 341 PPR x 34.02:1 gearbox

StarSpiDriverNode::StarSpiDriverNode(const rclcpp::NodeOptions & options)
: rclcpp_lifecycle::LifecycleNode("star_spi_driver", options)
{
  // Declare parameters
  declare_parameter("spi_device_path", "/dev/spidev0.0");
  declare_parameter("spi_speed_hz", kDefaultSpiSpeedHz);
  declare_parameter("cmd_vel_timeout_ms", kDefaultCmdVelTimeoutMs);
  declare_parameter("wheel_base", kDefaultTrackWidthM);
  declare_parameter("wheel_radius", kDefaultWheelRadiusM);
  declare_parameter("ticks_per_rev", kDefaultEncoderTicksPerRev);
}

StarSpiDriverNode::~StarSpiDriverNode()
{
  // Ensure cleanup
  if (spi_driver_) {
    spi_driver_->close_device();
  }
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_configure(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "Configuring StarSpiDriverNode...");

  // Load parameters
  spi_device_path_ = get_parameter("spi_device_path").as_string();
  spi_speed_hz_ = get_parameter("spi_speed_hz").as_int();
  cmd_vel_timeout_ms_ = get_parameter("cmd_vel_timeout_ms").as_int();

  SpiMessageConverter::Parameters converter_params;
  converter_params.wheel_base = get_parameter("wheel_base").as_double();
  converter_params.wheel_radius = get_parameter("wheel_radius").as_double();
  converter_params.ticks_per_rev = get_parameter("ticks_per_rev").as_int();

  // Initialize components
  spi_driver_ = std::make_unique<SpiDriver>(spi_device_path_, spi_speed_hz_);
  converter_ = std::make_unique<SpiMessageConverter>(converter_params);

  if (!spi_driver_->initialize()) {
    RCLCPP_ERROR(get_logger(), "Failed to initialize SPI driver on %s",
                 spi_device_path_.c_str());
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
           CallbackReturn::FAILURE;
  }

  // Create publishers
  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom/unfiltered", 10);
  joint_state_pub_ =
    create_publisher<sensor_msgs::msg::JointState>("joint_states", 10);
  imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("imu/data", 10);
  // Obstacle distance topics: ABSOLUTE paths. The safety_monitor (and the
  // gateway_bridge) subscribe to absolute `/star/obstacle/<corner>`. Using
  // relative names here breaks the e-stop chain when this node is launched
  // under any non-root namespace.
  obstacle_front_left_pub_ =
    create_publisher<sensor_msgs::msg::Range>("/star/obstacle/front_left", 10);
  obstacle_front_right_pub_ =
    create_publisher<sensor_msgs::msg::Range>("/star/obstacle/front_right", 10);
  obstacle_back_left_pub_ =
    create_publisher<sensor_msgs::msg::Range>("/star/obstacle/back_left", 10);
  obstacle_back_right_pub_ =
    create_publisher<sensor_msgs::msg::Range>("/star/obstacle/back_right", 10);
  obstacle_detected_pub_ =
    create_publisher<std_msgs::msg::Bool>("star/obstacle_detected", 10);
  estop_reason_pub_ =
    create_publisher<std_msgs::msg::Int32>("star/estop_reason", 10);

  // Create subscriptions
  cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10,
      std::bind(&StarSpiDriverNode::cmd_vel_callback, this,
                std::placeholders::_1));
  emergency_stop_sub_ = create_subscription<std_msgs::msg::Bool>(
      "emergency_stop", 10,
      std::bind(&StarSpiDriverNode::emergency_stop_callback, this,
                std::placeholders::_1));

  // Initialize state
  last_cmd_vel_time_ = get_clock()->now();

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
         CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_activate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "Activating StarSpiDriverNode...");

  odom_pub_->on_activate();
  joint_state_pub_->on_activate();
  imu_pub_->on_activate();
  obstacle_front_left_pub_->on_activate();
  obstacle_front_right_pub_->on_activate();
  obstacle_back_left_pub_->on_activate();
  obstacle_back_right_pub_->on_activate();
  obstacle_detected_pub_->on_activate();
  estop_reason_pub_->on_activate();

  // Start 100 Hz timer (10ms)
  timer_ = create_wall_timer(
      10ms, std::bind(&StarSpiDriverNode::timer_callback, this));

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
         CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "Deactivating StarSpiDriverNode...");

  // Stop timer first so no new transfers start
  timer_.reset();

  // Reset ACK/retry state now that the timer is stopped
  pending_ack_ = false;
  retry_count_ = 0;
  last_tx_frame_.clear();
  last_tx_seq_ = tx_seq_;

  bool success = true;

  // Send zero-velocity with Priority flag for safety before deactivation
  star::v1::SetVelocityRequest zero_req;
  zero_req.mutable_command()->set_front_left_velocity_mps(0.0);
  zero_req.mutable_command()->set_front_right_velocity_mps(0.0);
  zero_req.mutable_command()->set_back_left_velocity_mps(0.0);
  zero_req.mutable_command()->set_back_right_velocity_mps(0.0);

  std::vector<uint8_t> zero_payload(
    static_cast<size_t>(zero_req.ByteSizeLong()));
  if (!zero_req.SerializeToArray(zero_payload.data(),
                                 static_cast<int>(zero_payload.size())))
  {
    RCLCPP_ERROR(get_logger(),
                 "Failed to serialize zero-velocity request during deactivation");
    success = false;
  } else {
    const uint8_t zero_flags =
      static_cast<uint8_t>(FrameFlags::RequiresAck) |
      static_cast<uint8_t>(FrameFlags::Priority);
    std::vector<uint8_t> zero_frame;
    SpiDriver::encode_frame(tx_seq_, FrameType::Command, zero_flags,
                            zero_payload, zero_frame);
    std::vector<uint8_t> dummy_rx;
    if (!spi_driver_->transfer(zero_frame, dummy_rx)) {
      RCLCPP_ERROR(get_logger(), "SPI transfer failed for deactivation stop frame");
      success = false;
    } else {
      tx_seq_++;
    }
  }

  odom_pub_->on_deactivate();
  joint_state_pub_->on_deactivate();
  imu_pub_->on_deactivate();
  obstacle_front_left_pub_->on_deactivate();
  obstacle_front_right_pub_->on_deactivate();
  obstacle_back_left_pub_->on_deactivate();
  obstacle_back_right_pub_->on_deactivate();
  obstacle_detected_pub_->on_deactivate();
  estop_reason_pub_->on_deactivate();

  return success ?
         rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS :
         rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::FAILURE;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "Cleaning up StarSpiDriverNode...");

  spi_driver_->close_device();
  spi_driver_.reset();
  converter_.reset();

  odom_pub_.reset();
  joint_state_pub_.reset();
  imu_pub_.reset();
  obstacle_front_left_pub_.reset();
  obstacle_front_right_pub_.reset();
  obstacle_back_left_pub_.reset();
  obstacle_back_right_pub_.reset();
  obstacle_detected_pub_.reset();
  estop_reason_pub_.reset();
  cmd_vel_sub_.reset();
  emergency_stop_sub_.reset();

  return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::
         CallbackReturn::SUCCESS;
}

rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
StarSpiDriverNode::on_shutdown(const rclcpp_lifecycle::State & prev_state)
{
  RCLCPP_INFO(get_logger(), "Shutting down StarSpiDriverNode...");
  return on_cleanup(prev_state);
}

void StarSpiDriverNode::cmd_vel_callback(
  const geometry_msgs::msg::Twist::SharedPtr msg)
{
  current_cmd_vel_ = *msg;
  last_cmd_vel_time_ = get_clock()->now();
}

/**
 * @brief Emergency stop subscription callback.
 *
 * @details
 * Triggered on any message received on the "emergency_stop" topic.  When
 * msg->data is true the callback serializes an EmergencyStopRequest protobuf,
 * encodes a Priority + RequiresAck SPI frame, and performs a synchronous SPI
 * transfer to the RX72N.  Any protobuf serialization or SPI error is caught,
 * logged, and suppressed so the ROS2 executor is never terminated by an
 * exception from an interrupt-style callback.
 *
 * tx_seq_ is only incremented when the frame encoding and SPI transfer both
 * succeed, preserving the monotonic sequence counter invariant.
 *
 * @param[in] msg  Emergency stop flag; callback returns immediately if false.
 *
 * @pre  The ROS2 executor is running and the emergency_stop subscription is
 *       active (on_configure() completed successfully).
 * @pre  spi_driver_ is non-null and initialized; if null an error is logged
 *       and the function returns without transmitting.
 *
 * @throw None  Any std::exception thrown during protobuf serialization
 *              (star::v1::EmergencyStopRequest::SerializeToArray) or SPI
 *              frame encoding (SpiDriver::encode_frame) is caught inside
 *              this function, logged via RCLCPP_ERROR, and suppressed.
 *              No exception propagates to the caller.
 *
 * @note Runs on the ROS2 executor thread; not re-entrant.
 * @note spi_driver_ may be null if the node is deactivated; guarded with an
 *       early return + RCLCPP_ERROR.
 *
 * @post If msg->data is true and spi_driver_ is valid, an EmergencyStop frame
 *       is transmitted.  tx_seq_ is incremented only on success.
 * @post No exceptions propagate out of this callback.
 *
 * @code
 * // Published externally (e.g. from a safety monitor node):
 * std_msgs::msg::Bool estop_msg;
 * estop_msg.data = true;
 * emergency_stop_pub->publish(estop_msg);
 * // Internally, this callback will:
 * //   1. Serialize EmergencyStopRequest with reason string.
 * //   2. Encode as a Priority + RequiresAck SPI frame.
 * //   3. Call spi_driver_->transfer(tx_frame, rx_frame).
 * //   4. Increment tx_seq_ only on success.
 * @endcode
 *
 * @see star::v1::EmergencyStopRequest  Protobuf message serialized in this callback.
 * @see SpiDriver::encode_frame         Frame encoding used before transfer.
 * @see StarSpiDriverNode::spi_driver_  SPI driver instance used for the transfer.
 * @see StarSpiDriverNode::tx_seq_      Sequence counter incremented on success.
 *
 * @since Version 1.0.0

*/
void StarSpiDriverNode::emergency_stop_callback(
  const std_msgs::msg::Bool::SharedPtr msg)
{
  if (!msg->data) {
    return;
  }

  RCLCPP_ERROR(get_logger(), "Emergency stop requested via /emergency_stop topic");

  if (!spi_driver_) {
    RCLCPP_ERROR(get_logger(),
                 "Emergency stop received but SPI driver is not initialized");
    return;
  }

  try {
    bool retransmit_timer_paused = false;
    const auto resume_retransmit_timer = [&]() {
        if (retransmit_timer_paused && timer_) {
          timer_->reset();
          retransmit_timer_paused = false;
        }
      };

    star::v1::EmergencyStopRequest estop;
    estop.set_reason("Manual E-Stop via /emergency_stop topic");

    std::vector<uint8_t> payload(static_cast<size_t>(estop.ByteSizeLong()));
    if (!estop.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
      RCLCPP_ERROR(get_logger(), "Failed to serialize EmergencyStopRequest");
      return;
    }

    const uint8_t flags =
      static_cast<uint8_t>(FrameFlags::RequiresAck) |
      static_cast<uint8_t>(FrameFlags::Priority);
    std::vector<uint8_t> tx_frame;
    SpiDriver::encode_frame(tx_seq_, FrameType::Command, flags, payload, tx_frame);

    // Cancel any in-flight retransmit state so we don't resend stale commands.
    // The retransmit logic is driven by timer_ via timer_callback().
    if (pending_ack_) {
      pending_ack_ = false;
      last_tx_frame_.clear();
      retry_count_ = 0;
      if (timer_) {
        timer_->cancel();
        retransmit_timer_paused = true;
      }
    }

    std::vector<uint8_t> rx_frame;
    if (!spi_driver_->transfer(tx_frame, rx_frame)) {
      RCLCPP_ERROR(get_logger(), "SPI transfer failed during emergency stop");
      resume_retransmit_timer();
      return;
    }

    // Validate ACK/NACK response before accepting the transfer
    uint16_t rx_seq = 0;
    FrameType rx_type = FrameType::Ping;
    uint8_t rx_flags = 0;
    std::vector<uint8_t> rx_payload;
    if (!SpiDriver::decode_frame(rx_frame, rx_seq, rx_type, rx_flags, rx_payload)) {
      RCLCPP_ERROR(get_logger(),
                   "Failed to decode ACK response during emergency stop");
      resume_retransmit_timer();
      return;
    }
    if (rx_type == FrameType::Nack) {
      RCLCPP_ERROR(get_logger(),
                   "Emergency stop NACK received (seq=%u); command may not have been applied",
                   rx_seq);
      resume_retransmit_timer();
      return;
    }
    if (rx_type != FrameType::Ack || rx_seq != tx_seq_) {
      RCLCPP_ERROR(get_logger(),
                   "Unexpected response during emergency stop: type=0x%02X seq=%u (expected Ack seq=%u)",
                   static_cast<uint8_t>(rx_type), rx_seq, tx_seq_);
      resume_retransmit_timer();
      return;
    }

    // Only increment sequence after confirmed ACK
    tx_seq_++;

    resume_retransmit_timer();
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Exception in emergency_stop_callback: %s", e.what());
    if (timer_) {
      timer_->reset();
    }
  }
}

void StarSpiDriverNode::timer_callback()
{
  try {
    // Check for command velocity timeout (watchdog safety)
    auto now = get_clock()->now();
    double time_since_cmd = (now - last_cmd_vel_time_).seconds();

    geometry_msgs::msg::Twist cmd_vel_to_send;
    if (time_since_cmd <= (cmd_vel_timeout_ms_ / 1000.0)) {
      cmd_vel_to_send = current_cmd_vel_;
    }
    // else: zero velocity (default-constructed Twist)

    // ACK/NACK flow control: retransmit cached frame on timeout/NACK
    std::vector<uint8_t> tx_frame;
    bool is_retransmit = false;
    if (pending_ack_ && retry_count_ < K_MAX_RETRIES && !last_tx_frame_.empty()) {
      // Retransmit the cached frame
      tx_frame = last_tx_frame_;
      is_retransmit = true;
      retry_count_++;
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                           "Retransmitting frame (attempt %d/%d)", retry_count_,
                           K_MAX_RETRIES);
    } else {
      if (pending_ack_ && retry_count_ >= K_MAX_RETRIES) {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                              "ACK timeout after %d retries; dropping frame",
                              K_MAX_RETRIES);
        pending_ack_ = false;
        retry_count_ = 0;
      }

      // Convert Twist to protobuf VelocityCommand wrapped in SetVelocityRequest.
      // Firmware expects SetVelocityRequest, not bare VelocityCommand.
      star::v1::VelocityCommand velocity_cmd;
      if (!converter_->twist_to_velocity_command(cmd_vel_to_send, velocity_cmd)) {
        RCLCPP_WARN(get_logger(),
                    "Failed to convert Twist to VelocityCommand (NaN/Inf?)");
        velocity_cmd.set_front_left_velocity_mps(0.0);
        velocity_cmd.set_front_right_velocity_mps(0.0);
        velocity_cmd.set_back_left_velocity_mps(0.0);
        velocity_cmd.set_back_right_velocity_mps(0.0);
      }

      star::v1::SetVelocityRequest request;
      *request.mutable_command() = velocity_cmd;

      std::vector<uint8_t> payload(static_cast<size_t>(request.ByteSizeLong()));
      if (!request.SerializeToArray(payload.data(),
                                    static_cast<int>(payload.size())))
      {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                              "Failed to serialize SetVelocityRequest; skipping cycle");
        return;
      }

      const uint8_t flags = static_cast<uint8_t>(FrameFlags::RequiresAck);
      last_tx_seq_ = tx_seq_;
      SpiDriver::encode_frame(tx_seq_, FrameType::Command, flags, payload,
                              tx_frame);
      last_tx_frame_ = tx_frame;
      pending_ack_ = true;
      retry_count_ = 0;
    }

    // Perform full-duplex SPI transfer
    std::vector<uint8_t> rx_frame;
    if (!spi_driver_->transfer(tx_frame, rx_frame)) {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                            "SPI Transfer Failed");
      return;
    }

    // Increment tx_seq_ only after a successful new-frame transfer (not retransmit)
    if (!is_retransmit) {
      tx_seq_++;
    }

    // Decode received frame
    uint16_t rx_seq = 0;
    FrameType rx_type = FrameType::Ping;
    uint8_t rx_flags = 0;
    std::vector<uint8_t> rx_payload;

    if (!SpiDriver::decode_frame(rx_frame, rx_seq, rx_type, rx_flags, rx_payload)) {
      RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), kLogThrottleMs,
          "SPI Frame Decode Failed (CRC mismatch or garbage)");
      return;
    }

    // Dispatch on received frame type
    switch (rx_type) {
      case FrameType::Ack: {
          if (rx_seq == last_tx_seq_) {
            pending_ack_ = false;
            retry_count_ = 0;
          }
          break;
        }

      case FrameType::Nack: {
          RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), kLogThrottleMs,
                               "NACK received for seq=%u", rx_seq);
          // pending_ack_ stays true; retransmit will happen next tick
          break;
        }

      case FrameType::Ping: {
          RCLCPP_DEBUG(get_logger(), "PING received from RX72N");
          break;
        }

      case FrameType::Response: {
          // Response implies command was received; clear ACK state
          pending_ack_ = false;
          retry_count_ = 0;

          star::v1::TelemetryData telemetry;
          if (!telemetry.ParseFromArray(rx_payload.data(),
                                        static_cast<int>(rx_payload.size())))
          {
            RCLCPP_WARN(get_logger(), "Failed to parse TelemetryData protobuf");
            break;
          }

          // Check emergency stop flag from motor controller
          if (telemetry.emergency_stop()) {
            RCLCPP_ERROR_THROTTLE(
              get_logger(), *get_clock(), kLogThrottleMs,
              "RX72N EMERGENCY STOP ACTIVE: reason=%s",
              star::v1::EstopReason_Name(telemetry.estop_reason()).c_str());
          }

          // Publish estop reason (always, so consumers see 0/UNKNOWN when inactive)
          std_msgs::msg::Int32 estop_reason_msg;
          estop_reason_msg.data = static_cast<int32_t>(telemetry.estop_reason());
          estop_reason_pub_->publish(estop_reason_msg);

          // Publish Odometry
          nav_msgs::msg::Odometry odom;
          odom.header.stamp = now;
          converter_->telemetry_to_odometry(telemetry, odom);
          odom_pub_->publish(odom);

          // Publish Joint States
          sensor_msgs::msg::JointState joint_state;
          joint_state.header.stamp = now;
          converter_->telemetry_to_joint_state(telemetry, joint_state);
          joint_state_pub_->publish(joint_state);

          // Publish IMU data if populated
          const auto & imu_data = telemetry.imu();
          constexpr double kImuEpsilon = 1e-6;
          const bool imu_populated =
            (std::fabs(imu_data.accel_z_mps2()) > kImuEpsilon ||
            std::fabs(imu_data.gyro_z_rad_per_s()) > kImuEpsilon);
          if (imu_populated) {
            sensor_msgs::msg::Imu imu_msg;
            imu_msg.header.stamp = now;
            imu_msg.header.frame_id = "imu_link";

            tf2::Quaternion q;
            q.setRPY(imu_data.roll_rad(), imu_data.pitch_rad(), imu_data.yaw_rad());
            imu_msg.orientation.x = q.x();
            imu_msg.orientation.y = q.y();
            imu_msg.orientation.z = q.z();
            imu_msg.orientation.w = q.w();
            imu_msg.orientation_covariance = {
              kImuOrientationVar, 0.0, 0.0,
              0.0, kImuOrientationVar, 0.0,
              0.0, 0.0, kImuOrientationVar};

            imu_msg.angular_velocity.x = imu_data.gyro_x_rad_per_s();
            imu_msg.angular_velocity.y = imu_data.gyro_y_rad_per_s();
            imu_msg.angular_velocity.z = imu_data.gyro_z_rad_per_s();
            imu_msg.angular_velocity_covariance = {
              kImuAngularVelocityVar, 0.0, 0.0,
              0.0, kImuAngularVelocityVar, 0.0,
              0.0, 0.0, kImuAngularVelocityVar};

            imu_msg.linear_acceleration.x = imu_data.accel_x_mps2();
            imu_msg.linear_acceleration.y = imu_data.accel_y_mps2();
            imu_msg.linear_acceleration.z = imu_data.accel_z_mps2();
            imu_msg.linear_acceleration_covariance = {
              kImuLinearAccelVar, 0.0, 0.0,
              0.0, kImuLinearAccelVar, 0.0,
              0.0, 0.0, kImuLinearAccelVar};

            imu_pub_->publish(imu_msg);
          }

          // Publish obstacle ranges
          sensor_msgs::msg::Range obs_fl, obs_fr, obs_bl, obs_br;
          obs_fl.header.stamp = now;
          obs_fr.header.stamp = now;
          obs_bl.header.stamp = now;
          obs_br.header.stamp = now;
          converter_->telemetry_to_obstacle_ranges(telemetry, obs_fl, obs_fr,
                                                   obs_bl, obs_br);
          obstacle_front_left_pub_->publish(obs_fl);
          obstacle_front_right_pub_->publish(obs_fr);
          obstacle_back_left_pub_->publish(obs_bl);
          obstacle_back_right_pub_->publish(obs_br);

          std_msgs::msg::Bool detected_msg;
          detected_msg.data = telemetry.obstacle().any_obstacle();
          obstacle_detected_pub_->publish(detected_msg);
          break;
        }

      default: {
          RCLCPP_DEBUG(get_logger(), "Received unhandled frame type: 0x%02X",
                       static_cast<uint8_t>(rx_type));
          break;
        }
    }
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Unhandled exception in timer_callback: %s", e.what());
  } catch (...) {
    RCLCPP_ERROR(get_logger(), "Unhandled non-standard exception in timer_callback");
  }
}

}  // namespace star_spi_bridge
