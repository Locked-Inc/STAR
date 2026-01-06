#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

/**
 * @brief ROS2 node that bridges between the ROS2 navigation stack and the RX72N motor controller.
 *
 * This node handles:
 * 1. Subscribing to /cmd_vel and translating it to SPI commands.
 * 2. Reading encoder data from SPI and publishing Odometry and JointStates.
 *
 * Safety features:
 * - Command watchdog: Stops robot if no cmd_vel received within timeout
 * - Input validation: Rejects NaN and out-of-range velocities
 * - Non-blocking control loop: Uses try_lock to avoid blocking real-time loop
 * - Graceful shutdown: Sends stop command on node destruction
 */
class SpiBridgeNode : public rclcpp::Node
{
public:
  SpiBridgeNode()
  : Node("star_spi_bridge")
  {
    RCLCPP_INFO(this->get_logger(), "Initializing STAR SPI Bridge Node");

    // Hardware parameters
    this->declare_parameter("spi_device", "/dev/spidev0.0");
    this->declare_parameter("spi_speed", 10000000);
    this->declare_parameter("wheel_radius", 0.0325);  // 65mm diameter
    this->declare_parameter("wheel_base", 0.150);     // 150mm track width

    // Safety parameters
    this->declare_parameter("cmd_timeout_sec", 0.5);
    this->declare_parameter("max_linear_vel", 1.0);   // m/s
    this->declare_parameter("max_angular_vel", 2.0);  // rad/s
    this->declare_parameter("control_rate_hz", 50.0);

    // Cache safety parameters
    cmd_timeout_sec_ = this->get_parameter("cmd_timeout_sec").as_double();
    max_linear_vel_ = this->get_parameter("max_linear_vel").as_double();
    max_angular_vel_ = this->get_parameter("max_angular_vel").as_double();
    double control_rate = this->get_parameter("control_rate_hz").as_double();

    // Subscribers
    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 10, std::bind(&SpiBridgeNode::cmd_vel_callback, this, std::placeholders::_1));

    // Publishers
    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/odom", 10);
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>("/joint_states", 10);

    // TODO: Initialize SPI bus and Protobuf handles

    // Timer for SPI communication loop
    auto period_ms = static_cast<int>(1000.0 / control_rate);
    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(period_ms), std::bind(&SpiBridgeNode::spi_loop, this));

    RCLCPP_INFO(this->get_logger(),
      "SPI Bridge initialized: timeout=%.2fs, max_v=%.2fm/s, max_w=%.2frad/s, rate=%dHz",
      cmd_timeout_sec_, max_linear_vel_, max_angular_vel_, period_ms);
  }

  ~SpiBridgeNode()
  {
    RCLCPP_INFO(this->get_logger(), "Shutting down SPI Bridge - sending stop command");
    send_motor_command(0.0, 0.0);
  }

private:
  /**
   * @brief Validates a twist command for NaN and range limits.
   * @param twist The twist message to validate
   * @return true if valid, false otherwise
   */
  bool validate_twist(const geometry_msgs::msg::Twist & twist)
  {
    // Check for NaN values
    if (std::isnan(twist.linear.x) || std::isnan(twist.linear.y) ||
        std::isnan(twist.linear.z) || std::isnan(twist.angular.x) ||
        std::isnan(twist.angular.y) || std::isnan(twist.angular.z))
    {
      RCLCPP_WARN(this->get_logger(), "Rejecting cmd_vel with NaN values");
      return false;
    }

    // Check for infinite values
    if (std::isinf(twist.linear.x) || std::isinf(twist.angular.z))
    {
      RCLCPP_WARN(this->get_logger(), "Rejecting cmd_vel with infinite values");
      return false;
    }

    // Check velocity limits (warn but still clamp)
    if (std::abs(twist.linear.x) > max_linear_vel_ * 2.0 ||
        std::abs(twist.angular.z) > max_angular_vel_ * 2.0)
    {
      RCLCPP_WARN(this->get_logger(),
        "cmd_vel significantly exceeds limits: v=%.2f (max=%.2f), w=%.2f (max=%.2f)",
        twist.linear.x, max_linear_vel_, twist.angular.z, max_angular_vel_);
    }

    return true;
  }

  /**
   * @brief Clamps velocity values to safe limits.
   */
  geometry_msgs::msg::Twist clamp_twist(const geometry_msgs::msg::Twist & twist)
  {
    geometry_msgs::msg::Twist clamped = twist;
    clamped.linear.x = std::clamp(twist.linear.x, -max_linear_vel_, max_linear_vel_);
    clamped.angular.z = std::clamp(twist.angular.z, -max_angular_vel_, max_angular_vel_);
    return clamped;
  }

  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    if (!validate_twist(*msg)) {
      return;  // Reject invalid commands
    }

    auto clamped = clamp_twist(*msg);

    std::lock_guard<std::mutex> lock(twist_mutex_);
    last_twist_ = clamped;
    last_cmd_time_ = this->get_clock()->now();
    has_received_cmd_ = true;
  }

  void spi_loop()
  {
    // Get current command (non-blocking)
    std::optional<geometry_msgs::msg::Twist> cmd = get_current_command();

    if (!cmd.has_value()) {
      // No valid command - robot should be stopped
      send_motor_command(0.0, 0.0);
      publish_odometry();
      return;
    }

    // Send command to motors via SPI
    send_motor_command(cmd->linear.x, cmd->angular.z);

    // TODO: Read encoder telemetry from SPI response

    publish_odometry();
  }

  /**
   * @brief Gets the current valid command, checking watchdog timeout.
   * @return The current twist command, or nullopt if no valid command
   *
   * Uses try_lock to avoid blocking the real-time control loop.
   * If lock acquisition fails, returns the cached command if still valid.
   */
  std::optional<geometry_msgs::msg::Twist> get_current_command()
  {
    auto now = this->get_clock()->now();

    // Try to acquire lock without blocking
    if (twist_mutex_.try_lock()) {
      if (has_received_cmd_) {
        cached_twist_ = last_twist_;
        cached_cmd_time_ = last_cmd_time_;
      }
      twist_mutex_.unlock();
    }
    // If try_lock fails, we use the cached values from last successful lock

    // Check if we've ever received a command
    if (!has_received_cmd_) {
      // Log once at startup, then throttle
      RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "Waiting for first cmd_vel message...");
      return std::nullopt;
    }

    // Check watchdog timeout
    double cmd_age = (now - cached_cmd_time_).seconds();
    if (cmd_age > cmd_timeout_sec_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
        "cmd_vel timeout: no command for %.2fs (limit: %.2fs) - stopping robot",
        cmd_age, cmd_timeout_sec_);
      return std::nullopt;
    }

    return cached_twist_;
  }

  /**
   * @brief Sends motor command to RX72N via SPI.
   * @param linear_vel Linear velocity in m/s
   * @param angular_vel Angular velocity in rad/s
   */
  void send_motor_command(double linear_vel, double angular_vel)
  {
    // TODO: Implement actual SPI communication
    // 1. Convert velocities to wheel speeds using differential drive kinematics
    // 2. Encode into protobuf message
    // 3. Send via SPI to RX72N

    (void)linear_vel;
    (void)angular_vel;
  }

  void publish_odometry()
  {
    auto message = nav_msgs::msg::Odometry();
    message.header.stamp = this->get_clock()->now();
    message.header.frame_id = "odom";
    message.child_frame_id = "base_link";

    // TODO: Calculate odometry from encoder data received via SPI
    odom_pub_->publish(message);
  }

  // ROS2 interfaces
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  // Thread-safe command storage
  std::mutex twist_mutex_;
  geometry_msgs::msg::Twist last_twist_{};
  rclcpp::Time last_cmd_time_{0, 0, RCL_ROS_TIME};
  std::atomic<bool> has_received_cmd_{false};

  // Cached values for non-blocking access (updated on successful lock)
  geometry_msgs::msg::Twist cached_twist_{};
  rclcpp::Time cached_cmd_time_{0, 0, RCL_ROS_TIME};

  // Safety parameters (cached for performance)
  double cmd_timeout_sec_{0.5};
  double max_linear_vel_{1.0};
  double max_angular_vel_{2.0};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<SpiBridgeNode>();

  RCLCPP_INFO(node->get_logger(), "SPI Bridge node started");

  rclcpp::spin(node);

  // Node destructor will send stop command
  RCLCPP_INFO(node->get_logger(), "SPI Bridge node shutting down");

  rclcpp::shutdown();
  return 0;
}
