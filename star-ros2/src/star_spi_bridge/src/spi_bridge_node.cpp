#include <memory>
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
 */
class SpiBridgeNode : public rclcpp::Node
{
public:
  SpiBridgeNode()
  : Node("star_spi_bridge")
  {
    RCLCPP_INFO(this->get_logger(), "Initializing STAR SPI Bridge Node");

    // Parameters
    this->declare_parameter("spi_device", "/dev/spidev0.0");
    this->declare_parameter("spi_speed", 10000000);
    this->declare_parameter("wheel_radius", 0.0325); // 65mm diameter
    this->declare_parameter("wheel_base", 0.150);   // 150mm track width

    // Subscribers
    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 10, std::bind(&SpiBridgeNode::cmd_vel_callback, this, std::placeholders::_1));

    // Publishers
    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/odom", 10);
    joint_state_pub_ = this->create_publisher<sensor_msgs::msg::JointState>("/joint_states", 10);

    // TODO: Initialize SPI bus and Protobuf handles
    
    // Timer for SPI communication loop (e.g., 50Hz)
    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(20), std::bind(&SpiBridgeNode::spi_loop, this));
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    // Translate Twist (linear.x, angular.z) to motor speeds
    // RCLCPP_INFO(this->get_logger(), "Received velocity command: v=%.2f, w=%.2f", msg->linear.x, msg->angular.z);
    last_twist_ = *msg;
  }

  void spi_loop()
  {
    // 1. Prepare SPI command frame with last_twist_ data
    // 2. Transmit/Receive over SPI
    // 3. Process received telemetry (encoders, battery, etc.)
    // 4. Publish Odometry and JointState
    
    publish_odometry();
  }

  void publish_odometry()
  {
    auto message = nav_msgs::msg::Odometry();
    message.header.stamp = this->get_clock()->now();
    message.header.frame_id = "odom";
    message.child_frame_id = "base_link";

    // Placeholder odometry calculation
    odom_pub_->publish(message);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  geometry_msgs::msg::Twist last_twist_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SpiBridgeNode>());
  rclcpp::shutdown();
  return 0;
}
