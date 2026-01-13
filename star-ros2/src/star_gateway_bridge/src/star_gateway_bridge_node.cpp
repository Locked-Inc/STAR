// star_gateway_bridge_node.cpp - ROS2 Gateway Bridge Node Implementation
// Bridges ROS2 ecosystem with Go gateway service via gRPC.
//
// STAR Project - Texas A&M University
// January 2026

#include "star_gateway_bridge/star_gateway_bridge_node.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace star
{

StarGatewayBridgeNode::StarGatewayBridgeNode(const rclcpp::NodeOptions &options)
    : Node("star_gateway_bridge", options), grpc_connected_(false), reconnect_attempts_(0)
{
  RCLCPP_INFO(this->get_logger(), "Initializing STAR Gateway Bridge Node");

  // Declare parameters with defaults
  this->declare_parameter("gateway_address", "localhost:50051");
  this->declare_parameter("telemetry_rate_hz", 10.0);
  this->declare_parameter("teleop_rate_hz", 50.0);
  this->declare_parameter("watchdog_timeout_sec", 5.0);
  this->declare_parameter("teleop_timeout_ms", 500);
  this->declare_parameter("grpc_deadline_ms", 100);
  this->declare_parameter("wheel_base", 0.150);  // 150mm track width

  // Cache parameters
  gateway_address_ = this->get_parameter("gateway_address").as_string();
  telemetry_rate_hz_ = this->get_parameter("telemetry_rate_hz").as_double();
  teleop_rate_hz_ = this->get_parameter("teleop_rate_hz").as_double();
  watchdog_timeout_sec_ = this->get_parameter("watchdog_timeout_sec").as_double();
  teleop_timeout_ms_ = this->get_parameter("teleop_timeout_ms").as_int();
  grpc_deadline_ms_ = this->get_parameter("grpc_deadline_ms").as_int();
  wheel_base_ = this->get_parameter("wheel_base").as_double();

  RCLCPP_INFO(this->get_logger(),
              "Configuration: gateway=%s, telemetry_rate=%.1fHz, teleop_rate=%.1fHz, "
              "watchdog=%.1fs, teleop_timeout=%dms, grpc_deadline=%dms, wheel_base=%.3fm",
              gateway_address_.c_str(),
              telemetry_rate_hz_,
              teleop_rate_hz_,
              watchdog_timeout_sec_,
              teleop_timeout_ms_,
              grpc_deadline_ms_,
              wheel_base_);

  // Initialize gRPC client
  if (!initialize_grpc_client()) {
    RCLCPP_WARN(
      this->get_logger(), "Failed to connect to Gateway at %s - will retry in background", gateway_address_.c_str());
  }

  // Initialize ROS2 interfaces
  initialize_ros_interfaces();

  RCLCPP_INFO(this->get_logger(), "STAR Gateway Bridge Node initialized successfully");
}

StarGatewayBridgeNode::~StarGatewayBridgeNode()
{
  RCLCPP_INFO(this->get_logger(), "Shutting down STAR Gateway Bridge Node");

  // Send stop command before shutdown
  auto zero_twist = geometry_msgs::msg::Twist();
  if (teleop_cmd_vel_pub_) {
    teleop_cmd_vel_pub_->publish(zero_twist);
    RCLCPP_INFO(this->get_logger(), "Sent stop command on shutdown");
  }

  // Close gRPC channel gracefully
  if (grpc_channel_) {
    RCLCPP_INFO(this->get_logger(), "Closing gRPC channel");
  }
}

// ===========================================================================
// Initialization
// ===========================================================================

bool StarGatewayBridgeNode::initialize_grpc_client()
{
  RCLCPP_INFO(this->get_logger(), "Connecting to Gateway gRPC server at %s", gateway_address_.c_str());

  // Create gRPC channel with keepalive settings
  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);           // 10s keepalive ping
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);         // 5s keepalive timeout
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);  // Allow keepalive without calls

  grpc_channel_ = grpc::CreateCustomChannel(gateway_address_, grpc::InsecureChannelCredentials(), args);

  if (!grpc_channel_) {
    RCLCPP_ERROR(this->get_logger(), "Failed to create gRPC channel");
    return false;
  }

  // Wait for channel to be ready (with timeout)
  auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(grpc_deadline_ms_);

  if (!grpc_channel_->WaitForConnected(deadline)) {
    RCLCPP_WARN(this->get_logger(), "gRPC channel not ready within %dms", grpc_deadline_ms_);
    grpc_connected_ = false;
    return false;
  }

  // Create gRPC stub
  grpc_stub_ = star::v1::GatewayService::NewStub(grpc_channel_);

  RCLCPP_INFO(this->get_logger(), "Successfully connected to Gateway gRPC server");
  grpc_connected_ = true;
  reconnect_attempts_ = 0;

  return true;
}

void StarGatewayBridgeNode::initialize_ros_interfaces()
{
  RCLCPP_INFO(this->get_logger(), "Initializing ROS2 interfaces");

  // Publishers
  teleop_cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/teleop/cmd_vel", 10);

  // Subscribers
  robot_status_sub_ = this->create_subscription<std_msgs::msg::String>(
    "/robot_status", 10, std::bind(&StarGatewayBridgeNode::robot_status_callback, this, std::placeholders::_1));

  battery_state_sub_ = this->create_subscription<sensor_msgs::msg::BatteryState>(
    "/battery_state", 10, std::bind(&StarGatewayBridgeNode::battery_state_callback, this, std::placeholders::_1));

  // Services
  set_pid_gains_service_ = this->create_service<std_srvs::srv::SetBool>(
    "/set_pid_gains",
    std::bind(&StarGatewayBridgeNode::set_pid_gains_callback, this, std::placeholders::_1, std::placeholders::_2));

  // Timers
  auto telemetry_period_ms = static_cast<int>(1000.0 / telemetry_rate_hz_);
  telemetry_timer_ = this->create_wall_timer(std::chrono::milliseconds(telemetry_period_ms),
                                             std::bind(&StarGatewayBridgeNode::telemetry_forward_timer_callback, this));

  auto teleop_period_ms = static_cast<int>(1000.0 / teleop_rate_hz_);
  teleop_timer_ = this->create_wall_timer(std::chrono::milliseconds(teleop_period_ms),
                                          std::bind(&StarGatewayBridgeNode::teleop_poll_timer_callback, this));

  auto watchdog_period_ms = static_cast<int>(watchdog_timeout_sec_ * 1000.0);
  watchdog_timer_ = this->create_wall_timer(std::chrono::milliseconds(watchdog_period_ms),
                                            std::bind(&StarGatewayBridgeNode::connection_watchdog_callback, this));

  RCLCPP_INFO(this->get_logger(),
              "ROS2 interfaces initialized: telemetry=%dms, teleop=%dms, watchdog=%dms",
              telemetry_period_ms,
              teleop_period_ms,
              watchdog_period_ms);
}

// ===========================================================================
// ROS2 Callbacks
// ===========================================================================

void StarGatewayBridgeNode::robot_status_callback(const std_msgs::msg::String::SharedPtr msg)
{
  // Use try_lock to avoid blocking callback (non-blocking pattern)
  if (robot_status_mutex_.try_lock()) {
    cached_robot_status_ = *msg;
    robot_status_mutex_.unlock();
  }
  // If try_lock fails, skip this update (cache will use previous value)
}

void StarGatewayBridgeNode::battery_state_callback(const sensor_msgs::msg::BatteryState::SharedPtr msg)
{
  // Use try_lock to avoid blocking callback (non-blocking pattern)
  if (battery_state_mutex_.try_lock()) {
    cached_battery_state_ = *msg;
    battery_state_mutex_.unlock();
  }
  // If try_lock fails, skip this update (cache will use previous value)
}

void StarGatewayBridgeNode::set_pid_gains_callback(const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
                                                   std::shared_ptr<std_srvs::srv::SetBool::Response> response)
{
  // TODO Phase 4: Implement PID gains service after defining custom service type
  // For now, placeholder implementation
  (void)request;  // Unused parameter (placeholder service)

  RCLCPP_INFO(this->get_logger(), "set_pid_gains service called (placeholder)");

  // TODO: Parse PID gains from request, forward to Gateway via gRPC
  // TODO: Gateway will forward to motor controller via SPI bridge

  response->success = false;
  response->message = "PID gains service not yet implemented (Phase 4)";
}

// ===========================================================================
// Timer Callbacks
// ===========================================================================

void StarGatewayBridgeNode::telemetry_forward_timer_callback()
{
  if (!grpc_connected_) {
    RCLCPP_DEBUG_THROTTLE(
      this->get_logger(), *this->get_clock(), 5000, "Skipping telemetry forward - gRPC not connected");
    return;
  }

  // Get cached telemetry (non-blocking)
  std::optional<std_msgs::msg::String> robot_status;
  std::optional<sensor_msgs::msg::BatteryState> battery_state;

  if (robot_status_mutex_.try_lock()) {
    robot_status = cached_robot_status_;
    robot_status_mutex_.unlock();
  }

  if (battery_state_mutex_.try_lock()) {
    battery_state = cached_battery_state_;
    battery_state_mutex_.unlock();
  }

  // Forward telemetry to Gateway via gRPC
  if (grpc_stub_ && (robot_status.has_value() || battery_state.has_value())) {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(grpc_deadline_ms_));

    star::v1::ForwardTelemetryRequest request;

    // Set request header
    auto *header = request.mutable_header();
    header->set_request_id("telemetry_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    if (robot_status.has_value()) {
      converter_.string_to_system_status(*robot_status, *request.mutable_system_status());
    }

    if (battery_state.has_value()) {
      converter_.battery_state_to_proto(*battery_state, *request.mutable_battery_state());
    }

    star::v1::ForwardTelemetryResponse response;
    grpc::Status status = grpc_stub_->ForwardTelemetry(&context, request, &response);

    if (!status.ok()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(),
                           *this->get_clock(),
                           5000,
                           "ForwardTelemetry gRPC failed: %s",
                           status.error_message().c_str());
      grpc_connected_ = false;
    }
  }

  RCLCPP_DEBUG_THROTTLE(this->get_logger(),
                        *this->get_clock(),
                        10000,
                        "Telemetry forward: robot_status=%s, battery_state=%s",
                        robot_status.has_value() ? "cached" : "none",
                        battery_state.has_value() ? "cached" : "none");
}

void StarGatewayBridgeNode::teleop_poll_timer_callback()
{
  if (!grpc_connected_ || !grpc_stub_) {
    // Publish zero velocity when not connected (safety feature)
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Poll Gateway for latest teleop command via gRPC
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(grpc_deadline_ms_));

  star::v1::GetTeleopCommandRequest request;

  // Set request header
  auto *header = request.mutable_header();
  header->set_request_id("teleop_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

  star::v1::GetTeleopCommandResponse response;

  grpc::Status status = grpc_stub_->GetTeleopCommand(&context, request, &response);

  if (!status.ok()) {
    RCLCPP_WARN_THROTTLE(
      this->get_logger(), *this->get_clock(), 5000, "GetTeleopCommand gRPC failed: %s", status.error_message().c_str());
    grpc_connected_ = false;
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Check if command is available
  if (!response.command_available()) {
    RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 10000, "No teleop command available from Gateway");
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Check command staleness (safety feature)
  auto now_us =
    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto cmd_age_us = now_us - response.command().timestamp_us();
  auto cmd_age_ms = cmd_age_us / 1000;

  if (cmd_age_ms > teleop_timeout_ms_) {
    RCLCPP_WARN_THROTTLE(this->get_logger(),
                         *this->get_clock(),
                         1000,
                         "Teleop command stale (%ldms > %dms) - sending zero velocity",
                         cmd_age_ms,
                         teleop_timeout_ms_);
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Convert and publish fresh command
  geometry_msgs::msg::Twist twist;
  if (converter_.velocity_command_to_twist(response.command(), twist, wheel_base_)) {
    teleop_cmd_vel_pub_->publish(twist);
  } else {
    RCLCPP_WARN(this->get_logger(), "Failed to convert VelocityCommand to Twist");
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
  }
}

void StarGatewayBridgeNode::connection_watchdog_callback()
{
  if (!is_grpc_connected()) {
    RCLCPP_WARN_THROTTLE(
      this->get_logger(), *this->get_clock(), 10000, "Gateway gRPC connection unhealthy - attempting reconnection");
    grpc_connected_ = false;
    reconnect_grpc_client();
  }
}

// ===========================================================================
// gRPC Helpers
// ===========================================================================

bool StarGatewayBridgeNode::reconnect_grpc_client()
{
  if (reconnect_attempts_ >= k_max_reconnect_attempts) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(),
                          *this->get_clock(),
                          30000,
                          "Max reconnection attempts (%d) reached - giving up",
                          k_max_reconnect_attempts);
    return false;
  }

  reconnect_attempts_++;

  // Exponential backoff
  int backoff_ms = k_reconnect_backoff_ms_base * (1 << std::min(reconnect_attempts_, 5));

  RCLCPP_INFO(this->get_logger(),
              "Reconnection attempt %d/%d (backoff: %dms)",
              reconnect_attempts_,
              k_max_reconnect_attempts,
              backoff_ms);

  std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

  return initialize_grpc_client();
}

bool StarGatewayBridgeNode::is_grpc_connected() const
{
  if (!grpc_channel_) {
    return false;
  }

  auto state = grpc_channel_->GetState(false);  // false = don't try to connect
  return (state == GRPC_CHANNEL_READY);
}

}  // namespace star

// Component registration for composable node
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(star::StarGatewayBridgeNode)
