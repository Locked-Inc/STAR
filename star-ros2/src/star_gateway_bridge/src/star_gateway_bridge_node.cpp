// star_gateway_bridge_node.cpp - ROS2 Gateway Bridge Node Implementation
// Bridges ROS2 ecosystem with Go gateway service via gRPC.
//
// STAR Project - Texas A&M University
// Copyright 2026 STAR Project
// January 2026

#include "star_gateway_bridge/star_gateway_bridge_node.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace star {

StarGatewayBridgeNode::StarGatewayBridgeNode(const rclcpp::NodeOptions &options)
    : Node("star_gateway_bridge", options), grpc_connected_(false),
      reconnect_attempts_(0) {
  RCLCPP_INFO(this->get_logger(), "Initializing STAR Gateway Bridge Node");

  // Declare parameters with defaults
  this->declare_parameter("gateway_address", "localhost:50051");
  this->declare_parameter("telemetry_rate_hz", 10.0);
  this->declare_parameter("teleop_rate_hz", 50.0);
  this->declare_parameter("watchdog_timeout_sec", 5.0);
  this->declare_parameter("teleop_timeout_ms", 500);
  this->declare_parameter("grpc_deadline_ms", 100);
  this->declare_parameter("wheel_base", 0.150); // 150mm track width

  // Cache parameters
  gateway_address_ = this->get_parameter("gateway_address").as_string();
  telemetry_rate_hz_ = this->get_parameter("telemetry_rate_hz").as_double();
  teleop_rate_hz_ = this->get_parameter("teleop_rate_hz").as_double();
  watchdog_timeout_sec_ =
      this->get_parameter("watchdog_timeout_sec").as_double();
  teleop_timeout_ms_ = this->get_parameter("teleop_timeout_ms").as_int();
  grpc_deadline_ms_ = this->get_parameter("grpc_deadline_ms").as_int();
  wheel_base_ = this->get_parameter("wheel_base").as_double();

  RCLCPP_INFO(
      this->get_logger(),
      "Configuration: gateway=%s, telemetry_rate=%.1fHz, teleop_rate=%.1fHz, "
      "watchdog=%.1fs, teleop_timeout=%dms, grpc_deadline=%dms, "
      "wheel_base=%.3fm",
      gateway_address_.c_str(), telemetry_rate_hz_, teleop_rate_hz_,
      watchdog_timeout_sec_, teleop_timeout_ms_, grpc_deadline_ms_,
      wheel_base_);

  // Initialize gRPC client
  if (!initialize_grpc_client()) {
    RCLCPP_WARN(this->get_logger(),
                "Failed to connect to Gateway at %s - will retry in background",
                gateway_address_.c_str());
  }

  // Initialize ROS2 interfaces
  initialize_ros_interfaces();

  RCLCPP_INFO(this->get_logger(),
              "STAR Gateway Bridge Node initialized successfully");
}

StarGatewayBridgeNode::~StarGatewayBridgeNode() {
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

bool StarGatewayBridgeNode::initialize_grpc_client() {
  RCLCPP_INFO(this->get_logger(), "Connecting to Gateway gRPC server at %s",
              gateway_address_.c_str());

  // Create gRPC channel with keepalive settings
  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);   // 10s keepalive ping
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000); // 5s keepalive timeout
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS,
              1); // Allow keepalive without calls

  grpc_channel_ = grpc::CreateCustomChannel(
      gateway_address_, grpc::InsecureChannelCredentials(), args);

  if (!grpc_channel_) {
    RCLCPP_ERROR(this->get_logger(), "Failed to create gRPC channel");
    return false;
  }

  // Wait for channel to be ready (with timeout)
  auto deadline = std::chrono::system_clock::now() +
                  std::chrono::milliseconds(grpc_deadline_ms_);

  if (!grpc_channel_->WaitForConnected(deadline)) {
    RCLCPP_WARN(this->get_logger(), "gRPC channel not ready within %dms",
                grpc_deadline_ms_);
    grpc_connected_ = false;
    return false;
  }

  // Create gRPC stub
  grpc_stub_ = star::v1::GatewayService::NewStub(grpc_channel_);

  RCLCPP_INFO(this->get_logger(),
              "Successfully connected to Gateway gRPC server");
  grpc_connected_ = true;
  reconnect_attempts_ = 0;

  return true;
}

void StarGatewayBridgeNode::initialize_ros_interfaces() {
  RCLCPP_INFO(this->get_logger(), "Initializing ROS2 interfaces");

  // Publishers
  teleop_cmd_vel_pub_ =
      this->create_publisher<geometry_msgs::msg::Twist>("/teleop/cmd_vel", 10);

  // Subscribers
  robot_status_sub_ = this->create_subscription<std_msgs::msg::String>(
      "/robot_status", 10,
      std::bind(&StarGatewayBridgeNode::robot_status_callback, this,
                std::placeholders::_1));

  battery_state_sub_ =
      this->create_subscription<sensor_msgs::msg::BatteryState>(
          "/battery_state", 10,
          std::bind(&StarGatewayBridgeNode::battery_state_callback, this,
                    std::placeholders::_1));

  // Services
  set_pid_gains_service_ = this->create_service<std_srvs::srv::SetBool>(
      "/set_pid_gains",
      std::bind(&StarGatewayBridgeNode::set_pid_gains_callback, this,
                std::placeholders::_1, std::placeholders::_2));

  // Timers
  auto telemetry_period_ms = static_cast<int>(1000.0 / telemetry_rate_hz_);
  telemetry_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(telemetry_period_ms),
      std::bind(&StarGatewayBridgeNode::telemetry_forward_timer_callback,
                this));

  auto teleop_period_ms = static_cast<int>(1000.0 / teleop_rate_hz_);
  teleop_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(teleop_period_ms),
      std::bind(&StarGatewayBridgeNode::teleop_poll_timer_callback, this));

  auto watchdog_period_ms = static_cast<int>(watchdog_timeout_sec_ * 1000.0);
  watchdog_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(watchdog_period_ms),
      std::bind(&StarGatewayBridgeNode::connection_watchdog_callback, this));

  // Create diagnostics publisher
  diagnostics_pub_ =
      this->create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
          "/diagnostics", 10);

  // Create diagnostics timer (1 Hz for human readability)
  diagnostics_timer_ = this->create_wall_timer(
      std::chrono::seconds(1),
      std::bind(&StarGatewayBridgeNode::publish_diagnostics, this));

  RCLCPP_INFO(
      this->get_logger(),
      "ROS2 interfaces initialized: telemetry=%dms, teleop=%dms, watchdog=%dms",
      telemetry_period_ms, teleop_period_ms, watchdog_period_ms);
  RCLCPP_INFO(this->get_logger(), "Diagnostics publisher initialized");
}

// ===========================================================================
// ROS2 Callbacks
// ===========================================================================

void StarGatewayBridgeNode::robot_status_callback(
    const std_msgs::msg::String::SharedPtr msg) {
  // Use try_lock to avoid blocking callback (non-blocking pattern)
  if (robot_status_mutex_.try_lock()) {
    cached_robot_status_ = *msg;
    robot_status_mutex_.unlock();
  }
  // If try_lock fails, skip this update (cache will use previous value)
}

void StarGatewayBridgeNode::battery_state_callback(
    const sensor_msgs::msg::BatteryState::SharedPtr msg) {
  // Use try_lock to avoid blocking callback (non-blocking pattern)
  if (battery_state_mutex_.try_lock()) {
    cached_battery_state_ = *msg;
    battery_state_mutex_.unlock();
  }
  // If try_lock fails, skip this update (cache will use previous value)
}

void StarGatewayBridgeNode::set_pid_gains_callback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
  // TODO(star): Phase 4: Implement PID gains service after defining custom
  // service type. For now, placeholder implementation.
  (void)request; // Unused parameter (placeholder service)

  RCLCPP_INFO(this->get_logger(), "set_pid_gains service called (placeholder)");

  // TODO(star): Parse PID gains from request, forward to Gateway via gRPC
  // TODO(star): Gateway will forward to motor controller via SPI bridge

  response->success = false;
  response->message = "PID gains service not yet implemented (Phase 4)";
}

// ===========================================================================
// Timer Callbacks
// ===========================================================================

void StarGatewayBridgeNode::telemetry_forward_timer_callback() {
  if (!grpc_connected_) {
    RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                          "Skipping telemetry forward - gRPC not connected");
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
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(grpc_deadline_ms_));

    star::v1::ForwardTelemetryRequest request;

    // Set request header
    auto *header = request.mutable_header();
    header->set_request_id(
        "telemetry_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));

    if (robot_status.has_value()) {
      converter_.string_to_system_status(*robot_status,
                                         *request.mutable_system_status());
    }

    if (battery_state.has_value()) {
      converter_.battery_state_to_proto(*battery_state,
                                        *request.mutable_battery_state());
    }

    star::v1::ForwardTelemetryResponse response;
    grpc::Status status =
        grpc_stub_->ForwardTelemetry(&context, request, &response);

    if (!status.ok()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                           "ForwardTelemetry gRPC failed: %s",
                           status.error_message().c_str());
      grpc_connected_ = false;
    } else {
      // Successful transmission - increment frame counter
      total_telemetry_frames_++;
    }
  }

  RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                        "Telemetry forward: robot_status=%s, battery_state=%s",
                        robot_status.has_value() ? "cached" : "none",
                        battery_state.has_value() ? "cached" : "none");
}

void StarGatewayBridgeNode::teleop_poll_timer_callback() {
  if (!grpc_connected_ || !grpc_stub_) {
    // Publish zero velocity when not connected (safety feature)
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Poll Gateway for latest teleop command via gRPC
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(grpc_deadline_ms_));

  star::v1::GetTeleopCommandRequest request;

  // Set request header
  auto *header = request.mutable_header();
  header->set_request_id(
      "teleop_" +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count()));

  star::v1::GetTeleopCommandResponse response;

  grpc::Status status =
      grpc_stub_->GetTeleopCommand(&context, request, &response);

  if (!status.ok()) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "GetTeleopCommand gRPC failed: %s",
                         status.error_message().c_str());
    grpc_connected_ = false;
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Check if command is available
  if (!response.command_available()) {
    RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                          "No teleop command available from Gateway");
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Check command staleness (safety feature)
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  auto cmd_age_us = now_us - response.command().timestamp_us();
  auto cmd_age_ms = cmd_age_us / 1000;

  if (cmd_age_ms > teleop_timeout_ms_) {
    RCLCPP_WARN_THROTTLE(
        this->get_logger(), *this->get_clock(), 1000,
        "Teleop command stale (%ldms > %dms) - sending zero velocity",
        cmd_age_ms, teleop_timeout_ms_);
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Convert and publish fresh command
  geometry_msgs::msg::Twist twist;
  if (converter_.velocity_command_to_twist(response.command(), twist,
                                           wheel_base_)) {
    // Check sequence continuity for frame drop detection
    uint32_t current_seq = response.command().sequence();
    check_teleop_sequence_continuity(current_seq);

    teleop_cmd_vel_pub_->publish(twist);
  } else {
    RCLCPP_WARN(this->get_logger(),
                "Failed to convert VelocityCommand to Twist");
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
  }
}

void StarGatewayBridgeNode::connection_watchdog_callback() {
  if (!is_grpc_connected()) {
    RCLCPP_WARN_THROTTLE(
        this->get_logger(), *this->get_clock(), 10000,
        "Gateway gRPC connection unhealthy - attempting reconnection");
    grpc_connected_ = false;
    reconnect_grpc_client();
  }
}

// ===========================================================================
// gRPC Helpers
// ===========================================================================

bool StarGatewayBridgeNode::reconnect_grpc_client() {
  if (reconnect_attempts_ >= k_max_reconnect_attempts) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 30000,
                          "Max reconnection attempts (%d) reached - giving up",
                          k_max_reconnect_attempts);
    return false;
  }

  reconnect_attempts_++;

  // Exponential backoff
  int backoff_ms =
      k_reconnect_backoff_ms_base * (1 << std::min(reconnect_attempts_, 5));

  RCLCPP_INFO(this->get_logger(), "Reconnection attempt %d/%d (backoff: %dms)",
              reconnect_attempts_, k_max_reconnect_attempts, backoff_ms);

  std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

  return initialize_grpc_client();
}

bool StarGatewayBridgeNode::is_grpc_connected() const {
  if (!grpc_channel_) {
    return false;
  }

  // false = don't try to connect
  auto state = grpc_channel_->GetState(false);
  return state == GRPC_CHANNEL_READY;
}

void StarGatewayBridgeNode::check_teleop_sequence_continuity(
    uint32_t current_sequence) {
  if (first_teleop_frame_) {
    last_teleop_sequence_ = current_sequence;
    first_teleop_frame_ = false;
    RCLCPP_INFO(this->get_logger(), "First teleop frame received, sequence=%u",
                current_sequence);
  } else {
    uint32_t expected_seq = last_teleop_sequence_ + 1;

    // Detect sequence gap (accounting for uint32 wraparound)
    if (current_sequence != expected_seq) {
      uint32_t gap = current_sequence - expected_seq;

      // Sanity check: ignore huge gaps (likely system reset or wraparound)
      if (gap < 10000) {
        teleop_frames_dropped_ += gap;
        RCLCPP_WARN(this->get_logger(),
                    "Teleop frame drop: expected seq %u, got %u (gap=%u, "
                    "total_dropped=%lu)",
                    expected_seq, current_sequence, gap,
                    teleop_frames_dropped_);
      } else {
        RCLCPP_WARN(this->get_logger(),
                    "Teleop sequence reset detected: %u -> %u (ignoring as "
                    "system restart)",
                    last_teleop_sequence_, current_sequence);
      }
    }

    last_teleop_sequence_ = current_sequence;
  }

  total_teleop_frames_++;
}

void StarGatewayBridgeNode::check_telemetry_sequence_continuity(
    uint32_t current_sequence) {
  if (first_telemetry_frame_) {
    last_telemetry_sequence_ = current_sequence;
    first_telemetry_frame_ = false;
    RCLCPP_INFO(this->get_logger(),
                "First telemetry frame received, sequence=%u",
                current_sequence);
  } else {
    uint32_t expected_seq = last_telemetry_sequence_ + 1;

    // Detect sequence gap (accounting for uint32 wraparound)
    if (current_sequence != expected_seq) {
      uint32_t gap = current_sequence - expected_seq;

      // Sanity check: ignore huge gaps (likely system reset or wraparound)
      if (gap < 10000) {
        telemetry_frames_dropped_ += gap;
        RCLCPP_WARN(this->get_logger(),
                    "Telemetry frame drop: expected seq %u, got %u (gap=%u, "
                    "total_dropped=%lu)",
                    expected_seq, current_sequence, gap,
                    telemetry_frames_dropped_);
      } else {
        RCLCPP_WARN(this->get_logger(),
                    "Telemetry sequence reset detected: %u -> %u (ignoring as "
                    "system restart)",
                    last_telemetry_sequence_, current_sequence);
      }
    }

    last_telemetry_sequence_ = current_sequence;
  }

  total_telemetry_frames_++;
}

void StarGatewayBridgeNode::publish_diagnostics() {
  auto diag_array = diagnostic_msgs::msg::DiagnosticArray();
  diag_array.header.stamp = this->now();

  diagnostic_msgs::msg::DiagnosticStatus status;
  status.name = "star_gateway_bridge/teleop_command_drops";
  status.hardware_id = "gateway_bridge";

  // Determine severity level
  if (total_teleop_frames_ == 0) {
    status.level = diagnostic_msgs::msg::DiagnosticStatus::STALE;
    status.message = "No teleop commands received yet";
  } else if (teleop_frames_dropped_ == 0) {
    status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    status.message = "No teleop frame drops detected";
  } else {
    double drop_rate = (teleop_frames_dropped_ * 100.0) / total_teleop_frames_;

    if (drop_rate < 1.0) {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
      status.message =
          "Minor teleop drops (" + std::to_string(drop_rate) + "%)";
    } else {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      status.message =
          "Critical teleop loss (" + std::to_string(drop_rate) + "%)";
    }
  }

  // Add detailed metrics as key-value pairs
  diagnostic_msgs::msg::KeyValue kv;

  kv.key = "total_frames";
  kv.value = std::to_string(total_teleop_frames_);
  status.values.push_back(kv);

  kv.key = "dropped_frames";
  kv.value = std::to_string(teleop_frames_dropped_);
  status.values.push_back(kv);

  double drop_rate =
      total_teleop_frames_ > 0
          ? (teleop_frames_dropped_ * 100.0) / total_teleop_frames_
          : 0.0;
  kv.key = "drop_rate_percent";
  kv.value = std::to_string(drop_rate);
  status.values.push_back(kv);

  kv.key = "last_sequence";
  kv.value = std::to_string(last_teleop_sequence_);
  status.values.push_back(kv);

  diag_array.status.push_back(status);

  // Add telemetry diagnostics
  diagnostic_msgs::msg::DiagnosticStatus telemetry_status;
  telemetry_status.name = "star_gateway_bridge/telemetry_frame_drops";
  telemetry_status.hardware_id = "gateway_bridge";

  if (total_telemetry_frames_ == 0) {
    telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::STALE;
    telemetry_status.message = "No telemetry frames received";
  } else if (telemetry_frames_dropped_ == 0) {
    telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    telemetry_status.message = "No telemetry drops";
  } else {
    double telemetry_drop_rate =
        (telemetry_frames_dropped_ * 100.0) / total_telemetry_frames_;

    if (telemetry_drop_rate < 5.0) {
      telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
      telemetry_status.message = "Minor telemetry drops (" +
                                 std::to_string(telemetry_drop_rate) + "%)";
    } else {
      telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      telemetry_status.message = "Critical telemetry loss (" +
                                 std::to_string(telemetry_drop_rate) + "%)";
    }
  }

  // Add telemetry metrics
  diagnostic_msgs::msg::KeyValue telemetry_kv;

  telemetry_kv.key = "total_frames";
  telemetry_kv.value = std::to_string(total_telemetry_frames_);
  telemetry_status.values.push_back(telemetry_kv);

  telemetry_kv.key = "dropped_frames";
  telemetry_kv.value = std::to_string(telemetry_frames_dropped_);
  telemetry_status.values.push_back(telemetry_kv);

  double telemetry_drop_rate =
      total_telemetry_frames_ > 0
          ? (telemetry_frames_dropped_ * 100.0) / total_telemetry_frames_
          : 0.0;
  telemetry_kv.key = "drop_rate_percent";
  telemetry_kv.value = std::to_string(telemetry_drop_rate);
  telemetry_status.values.push_back(telemetry_kv);

  telemetry_kv.key = "last_sequence";
  telemetry_kv.value = std::to_string(last_telemetry_sequence_);
  telemetry_status.values.push_back(telemetry_kv);

  diag_array.status.push_back(telemetry_status);

  // Publish combined diagnostics
  diagnostics_pub_->publish(diag_array);
}

} // namespace star

// Component registration for composable node
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(star::StarGatewayBridgeNode)
