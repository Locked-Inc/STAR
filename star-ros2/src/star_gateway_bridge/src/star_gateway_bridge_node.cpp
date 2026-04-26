/**
 * @file star_gateway_bridge_node.cpp
 * @brief Bridges ROS2 ecosystem with Go gateway service via gRPC.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_gateway_bridge/star_gateway_bridge_node.hpp"

#include <chrono>
#include <exception>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace star::star_gateway_bridge
{

// ---------------------------------------------------------------------------
// Obstacle publisher constants
// ---------------------------------------------------------------------------
static constexpr int OBSTACLE_QOS_DEPTH = 10;
static constexpr std::string_view TOPIC_OBSTACLE_FRONT_LEFT = "/star/obstacle/front_left";
static constexpr std::string_view TOPIC_OBSTACLE_FRONT_RIGHT = "/star/obstacle/front_right";
static constexpr std::string_view TOPIC_OBSTACLE_BACK_LEFT = "/star/obstacle/back_left";
static constexpr std::string_view TOPIC_OBSTACLE_BACK_RIGHT = "/star/obstacle/back_right";
static constexpr std::string_view TOPIC_OBSTACLE_DETECTED = "/star/obstacle_detected";

/**
 * @brief Construct and fully initialise the STAR Gateway Bridge ROS2 node.
 *
 * @details
 * Declares and caches all ROS2 parameters, initialises the gRPC client
 * channel to the Go gateway service, creates publishers, subscribers, and
 * timer callbacks, and starts the connection watchdog. All member variables
 * (grpc_connected_, reconnect_attempts_, sequence counters, etc.) are set
 * to their safe initial state before any callbacks are registered.
 *
 * @param[in] options  ROS2 node options forwarded to rclcpp::Node. May
 * contain remappings, parameter overrides, or intra-process settings.
 * Invalid or missing required parameters are logged and default values
 * are applied; no exception is thrown for parameter validation failures.
 *
 * @throw std::runtime_error  Thrown if the gRPC channel cannot be
 * constructed (e.g. invalid gateway_address format).
 *
 * @pre gateway_address node parameter must be a valid host:port string
 * (non-empty, with a colon-separated port number).
 * @pre ROS2 node options must be compatible with rclcpp::Node construction
 * (no conflicting intra-process or remapping settings).
 * @post reconnect_attempts_ == 0 on return.
 * @post On success (initialize_grpc_client() returns true): grpc_connected_
 * == true and grpc_stub_/telemetry_svc_stub_ are populated.
 * @post On failure (initialize_grpc_client() returns false): grpc_connected_
 * == false; the constructor still completes and the connection watchdog
 * timer will retry in the background.
 * @post All publishers, subscribers, and timer callbacks are initialised
 * and the connection watchdog is started before the constructor returns.
 *
 * @note Not thread-safe during construction; the node must be fully
 * constructed in a single thread before being added to an executor.
 * @note grpc_connected_ is initialised to false and reconnect_attempts_
 * to 0 in the member-initialiser list, then potentially flipped to true
 * by initialize_grpc_client() before the constructor returns.
 *
 * @since Version 1.0.0

*/
StarGatewayBridgeNode::StarGatewayBridgeNode(const rclcpp::NodeOptions & options)
: Node("star_gateway_bridge", options),
  grpc_connected_(false),
  reconnect_attempts_(0)
{
  RCLCPP_INFO(this->get_logger(), "Initializing STAR Gateway Bridge Node");

  // Declare parameters with defaults
  this->declare_parameter("gateway_address", "localhost:50051");
  this->declare_parameter("telemetry_rate_hz", 10.0);
  this->declare_parameter("teleop_rate_hz", 50.0);
  this->declare_parameter("watchdog_timeout_sec", 5.0);
  this->declare_parameter("teleop_timeout_ms", 500);
  this->declare_parameter("grpc_deadline_ms", 100);
  this->declare_parameter("wheel_base", 0.356);  // 356mm track width (304mm inner + 52mm wheel)

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
              "watchdog=%.1fs, teleop_timeout=%dms, grpc_deadline=%dms, "
              "wheel_base=%.3fm",
              gateway_address_.c_str(), telemetry_rate_hz_, teleop_rate_hz_, watchdog_timeout_sec_, teleop_timeout_ms_,
              grpc_deadline_ms_, wheel_base_);

  // Initialize gRPC client
  if (!initialize_grpc_client()) {
    RCLCPP_WARN(this->get_logger(), "Failed to connect to Gateway at %s - will retry in background",
                gateway_address_.c_str());
  }

  // Initialize ROS2 interfaces
  initialize_ros_interfaces();

  RCLCPP_INFO(this->get_logger(), "STAR Gateway Bridge Node initialized successfully");
}

StarGatewayBridgeNode::~StarGatewayBridgeNode()
{
  RCLCPP_INFO(this->get_logger(), "Shutting down STAR Gateway Bridge Node");

  // Cancel obstacle timer before resetting publishers to prevent
  // obstacle_poll_timer_callback() from running after publishers are null.
  if (obstacle_timer_) {
    obstacle_timer_->cancel();
    obstacle_timer_.reset();
  }

  // Reset subscriptions and obstacle publishers before core publishers go away
  odom_sub_.reset();
  slam_pose_sub_.reset();
  scan_sub_.reset();
  obstacle_fl_pub_.reset();
  obstacle_fr_pub_.reset();
  obstacle_bl_pub_.reset();
  obstacle_br_pub_.reset();
  obstacle_detected_pub_.reset();

  // Send stop command on teleop topic before shutdown
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
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);    // 10s keepalive ping
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);  // 5s keepalive timeout
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS,
              1);  // Allow keepalive without calls

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

  // Create gRPC stubs (all services share the same channel)
  grpc_stub_ = star::v1::GatewayService::NewStub(grpc_channel_);
  telemetry_svc_stub_ = star::v1::TelemetryService::NewStub(grpc_channel_);

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

  // Obstacle distance publishers (HC-SR04 ultrasonic sensors, one per corner)
  obstacle_fl_pub_ = this->create_publisher<sensor_msgs::msg::Range>(std::string(TOPIC_OBSTACLE_FRONT_LEFT),
                                                                     OBSTACLE_QOS_DEPTH);
  obstacle_fr_pub_ = this->create_publisher<sensor_msgs::msg::Range>(std::string(TOPIC_OBSTACLE_FRONT_RIGHT),
                                                                     OBSTACLE_QOS_DEPTH);
  obstacle_bl_pub_ = this->create_publisher<sensor_msgs::msg::Range>(std::string(TOPIC_OBSTACLE_BACK_LEFT),
                                                                     OBSTACLE_QOS_DEPTH);
  obstacle_br_pub_ = this->create_publisher<sensor_msgs::msg::Range>(std::string(TOPIC_OBSTACLE_BACK_RIGHT),
                                                                     OBSTACLE_QOS_DEPTH);
  obstacle_detected_pub_ = this->create_publisher<std_msgs::msg::Bool>(std::string(TOPIC_OBSTACLE_DETECTED),
                                                                       OBSTACLE_QOS_DEPTH);

  // Subscribers
  robot_status_sub_ = this->create_subscription<
    std_msgs::msg::String>("/robot_status", 10,
                           std::bind(&StarGatewayBridgeNode::robot_status_callback, this, std::placeholders::_1));

  /**
   * @brief Subscribe to /odometry/filtered and cache EKF odometry as protobuf.
   *
   * @details
   * Uses fixed depth QoS (10) appropriate for filtered odometry streams and
   * converts each incoming message via converter_.odometry_to_proto(). Shared
   * cache updates are synchronized with odometry_mutex_.
   *
   * Topic: /odometry/filtered (nav_msgs/msg/Odometry)
   * Thread safety: writes cached_ekf_odometry_ and cached_ekf_timestamp_us_
   * under odometry_mutex_.
   */
  // Subscribe to odometry (EKF-filtered preferred)
  odom_sub_ = this->create_subscription<
    nav_msgs::msg::Odometry>("/odometry/filtered", 10, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
    try {
      star::v1::OdometryData proto_odom;
      converter_.odometry_to_proto(*msg, proto_odom);
      std::lock_guard<std::mutex> lock(odometry_mutex_);
      cached_ekf_timestamp_us_ = proto_odom.timestamp_us();
      cached_ekf_odometry_ = proto_odom;
    } catch (const std::exception & ex) {
      RCLCPP_ERROR(this->get_logger(), "Odometry callback failed: %s", ex.what());
    } catch (...) {
      RCLCPP_ERROR(this->get_logger(), "Odometry callback failed: unknown exception");
    }
  });

  /**
   * @brief Subscribe to /slam_toolbox/pose and cache SLAM pose as protobuf.
   *
   * @details
   * Uses SensorDataQoS() to match high-rate sensor pipelines and avoid stale
   * data under load. Converts each pose via converter_.slam_pose_to_proto().
   * Shared odometry cache updates are synchronized with odometry_mutex_.
   *
   * Topic: /slam_toolbox/pose (geometry_msgs/msg/PoseWithCovarianceStamped)
   * Thread safety: writes cached_slam_pose_ and cached_slam_timestamp_us_
   * under odometry_mutex_.
   */
  // Subscribe to SLAM pose (map frame) -- selected by telemetry arbitration
  // when newer than EKF (and as tie-breaker).
  slam_pose_sub_ = this->create_subscription<
    geometry_msgs::msg::PoseWithCovarianceStamped>("/slam_toolbox/pose", rclcpp::SensorDataQoS(),
                                                   [this](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr
                                                            msg) {
                                                     try {
                                                       star::v1::OdometryData proto_odom;
                                                       converter_.slam_pose_to_proto(*msg, proto_odom);
                                                       std::lock_guard<std::mutex> lock(odometry_mutex_);
                                                       cached_slam_timestamp_us_ = proto_odom.timestamp_us();
                                                       cached_slam_pose_ = proto_odom;
                                                     } catch (const std::exception & ex) {
                                                       RCLCPP_ERROR(this->get_logger(), "SLAM pose callback failed: %s",
                                                                    ex.what());
                                                     } catch (...) {
                                                       RCLCPP_ERROR(this->get_logger(),
                                                                    "SLAM pose callback failed: unknown exception");
                                                     }
                                                   });

  /**
   * @brief Subscribe to /scan and cache converted LiDAR scan protobuf.
   *
   * @details
   * Uses SensorDataQoS() to match LiDAR publisher behavior and prioritize
   * low-latency delivery. Converts each message via
   * converter_.laserscan_to_proto(); invalid metadata is rejected by the
   * converter and not cached.
   *
   * Topic: /scan (sensor_msgs/msg/LaserScan)
   * Thread safety: writes cached_lidar_scan_ under lidar_mutex_.
   */
  // Subscribe to LiDAR scan -- SensorDataQoS matches sllidar_node publisher QoS
  scan_sub_ = this->create_subscription<
    sensor_msgs::msg::LaserScan>("/scan", rclcpp::SensorDataQoS(),
                                 [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) {
                                   try {
                                     star::v1::LidarScan proto_scan;
                                     if (!converter_.laserscan_to_proto(*msg, proto_scan)) {
                                       return;
                                     }
                                     std::lock_guard<std::mutex> lock(lidar_mutex_);
                                     cached_lidar_scan_ = proto_scan;
                                   } catch (const std::exception & ex) {
                                     RCLCPP_ERROR(this->get_logger(), "LaserScan callback failed: %s", ex.what());
                                   } catch (...) {
                                     RCLCPP_ERROR(this->get_logger(), "LaserScan callback failed: unknown exception");
                                   }
                                 });

  // Services
  set_pid_gains_service_ = this->create_service<
    std_srvs::srv::SetBool>("/set_pid_gains", std::bind(&StarGatewayBridgeNode::set_pid_gains_callback, this,
                                                        std::placeholders::_1, std::placeholders::_2));

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

  // Obstacle polling timer runs at telemetry_rate_hz_ (default 10 Hz).
  obstacle_timer_ = this->create_wall_timer(std::chrono::milliseconds(telemetry_period_ms),
                                            std::bind(&StarGatewayBridgeNode::obstacle_poll_timer_callback, this));

  // Create diagnostics publisher
  diagnostics_pub_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);

  // Create diagnostics timer (1 Hz for human readability)
  diagnostics_timer_ = this->create_wall_timer(std::chrono::seconds(1),
                                               std::bind(&StarGatewayBridgeNode::publish_diagnostics, this));

  RCLCPP_INFO(this->get_logger(), "ROS2 interfaces initialized: telemetry=%dms, teleop=%dms, watchdog=%dms",
              telemetry_period_ms, teleop_period_ms, watchdog_period_ms);
  RCLCPP_INFO(this->get_logger(), "Diagnostics publisher initialized");
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

void StarGatewayBridgeNode::set_pid_gains_callback(const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
                                                   std::shared_ptr<std_srvs::srv::SetBool::Response> response)
{
  // TODO(star): Phase 4: Implement PID gains service after defining custom
  // service type. For now, placeholder implementation.
  (void)request;  // Unused parameter (placeholder service)

  RCLCPP_INFO(this->get_logger(), "set_pid_gains service called (placeholder)");

  // TODO(star): Parse PID gains from request, forward to Gateway via gRPC
  // TODO(star): Gateway will forward to motor controller via SPI bridge

  response->success = false;
  response->message = "PID gains service not yet implemented (Phase 4)";
}

// ===========================================================================
// Timer Callbacks
// ===========================================================================

void StarGatewayBridgeNode::telemetry_forward_timer_callback()
{
  if (!grpc_connected_) {
    RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                          "Skipping telemetry forward - gRPC not connected");
    return;
  }

  // Get cached telemetry (non-blocking)
  std::optional<std_msgs::msg::String> robot_status;

  if (robot_status_mutex_.try_lock()) {
    robot_status = cached_robot_status_;
    robot_status_mutex_.unlock();
  }

  // Forward telemetry to Gateway via gRPC
  if (grpc_stub_) {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(grpc_deadline_ms_));

    star::v1::ForwardTelemetryRequest request;

    // Set request header
    auto * header = request.mutable_header();
    header->set_request_id("telemetry_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    if (robot_status.has_value()) {
      converter_.string_to_system_status(*robot_status, *request.mutable_system_status());
    }

    // Populate odometry using explicit SLAM/EKF arbitration.
    // Priority: newer timestamp wins; SLAM wins ties for map-frame stability.
    {
      std::lock_guard<std::mutex> lock(odometry_mutex_);
      if (cached_slam_pose_.has_value() &&
          (!cached_ekf_odometry_.has_value() || cached_slam_timestamp_us_ >= cached_ekf_timestamp_us_)) {
        *request.mutable_odometry() = *cached_slam_pose_;
      } else if (cached_ekf_odometry_.has_value()) {
        *request.mutable_odometry() = *cached_ekf_odometry_;
      }
    }

    // Populate lidar scan if available
    {
      std::lock_guard<std::mutex> lock(lidar_mutex_);
      if (cached_lidar_scan_.has_value()) {
        *request.mutable_lidar_scan() = *cached_lidar_scan_;
      }
    }

    star::v1::ForwardTelemetryResponse response;
    grpc::Status status = grpc_stub_->ForwardTelemetry(&context, request, &response);

    if (!status.ok()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "ForwardTelemetry gRPC failed: %s",
                           status.error_message().c_str());
      grpc_connected_ = false;
    } else {
      // Successful transmission - increment frame counter
      total_telemetry_frames_++;
    }
  }

  bool odom_has = false;
  bool scan_has = false;
  {
    std::lock_guard<std::mutex> lock(odometry_mutex_);
    odom_has = cached_slam_pose_.has_value() || cached_ekf_odometry_.has_value();
  }
  {
    std::lock_guard<std::mutex> lock(lidar_mutex_);
    scan_has = cached_lidar_scan_.has_value();
  }

  RCLCPP_DEBUG_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                        "Telemetry forward: robot_status=%s, odom=%s, scan=%s",
                        robot_status.has_value() ? "cached" : "none", odom_has ? "cached" : "none",
                        scan_has ? "cached" : "none");
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
  auto * header = request.mutable_header();
  header->set_request_id("teleop_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

  star::v1::GetTeleopCommandResponse response;

  grpc::Status status = grpc_stub_->GetTeleopCommand(&context, request, &response);

  if (!status.ok()) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "GetTeleopCommand gRPC failed: %s",
                         status.error_message().c_str());
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
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
  auto cmd_age_us = now_us - response.command().timestamp_us();
  auto cmd_age_ms = cmd_age_us / 1000;

  if (cmd_age_ms > teleop_timeout_ms_) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                         "Teleop command stale (%ldms > %dms) - sending zero velocity", cmd_age_ms, teleop_timeout_ms_);
    auto zero_twist = geometry_msgs::msg::Twist();
    teleop_cmd_vel_pub_->publish(zero_twist);
    return;
  }

  // Convert and publish fresh command
  geometry_msgs::msg::Twist twist;
  if (converter_.velocity_command_to_twist(response.command(), twist, wheel_base_)) {
    // Check sequence continuity for frame drop detection
    uint32_t current_seq = response.command().sequence();
    check_teleop_sequence_continuity(current_seq);

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
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                         "Gateway gRPC connection unhealthy - attempting reconnection");
    grpc_connected_ = false;
    reconnect_grpc_client();
  }
}

/**
 * @brief Poll TelemetryService::GetTelemetry and publish ObstacleData to ROS2.
 *
 * @details
 * Called by obstacle_timer_ at telemetry_rate_hz_ (default 10 Hz). Retrieves
 * the latest RX72N TelemetryData from the Go gateway via
 * TelemetryService::GetTelemetry, extracts the ObstacleData sub-message, and
 * publishes it as five ROS2 messages:
 *   - /star/obstacle/front_left  (sensor_msgs/Range)
 *   - /star/obstacle/front_right (sensor_msgs/Range)
 *   - /star/obstacle/back_left   (sensor_msgs/Range)
 *   - /star/obstacle/back_right  (sensor_msgs/Range)
 *   - /star/obstacle_detected    (std_msgs/Bool)
 *
 * The callback is a no-op when grpc_connected_ is false or
 * telemetry_svc_stub_ is null. gRPC errors mark grpc_connected_ as false to
 * trigger watchdog reconnection and are throttled to one WARN log per 5 s.
 *
 * @pre grpc_connected_ must be true for any gRPC call to be attempted.
 * @pre telemetry_svc_stub_ must be non-null (set in initialize_grpc_client()).
 * @post Five ROS2 messages are published when an obstacle sub-message is
 *       present in the gateway response.
 * @post grpc_connected_ is set to false only for transport-level gRPC failures
 *       (UNAVAILABLE, DEADLINE_EXCEEDED, INTERNAL); application-level errors
 *       (NOT_FOUND, INVALID_ARGUMENT, etc.) are throttle-logged but do not
 *       change grpc_connected_ or trigger watchdog reconnection.
 *
 * @note Not thread-safe; called exclusively from the ROS2 timer executor.
 * @note Range messages use HC-SR04 constants (ULTRASOUND, 15-deg FOV,
 *       0.02-4.00 m). A firmware-reported distance of 0.0 m (no echo) is
 *       mapped to max_range by MessageConverter::obstacle_distance_to_range().
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions documented above.
 */
void StarGatewayBridgeNode::obstacle_poll_timer_callback()
{
  try {
    if (!grpc_connected_ || !telemetry_svc_stub_) {
      return;
    }

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(grpc_deadline_ms_));

    star::v1::GetTelemetryRequest request;
    request.mutable_header()->set_request_id(
      "obstacle_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    star::v1::GetTelemetryResponse response;
    grpc::Status status = telemetry_svc_stub_->GetTelemetry(&context, request, &response);

    if (!status.ok()) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "GetTelemetry (obstacle) gRPC failed: %s",
                           status.error_message().c_str());
      // Only mark the connection as lost for transport-level failures.
      // Application-level errors (NOT_FOUND, INVALID_ARGUMENT, etc.) do not
      // indicate a connectivity loss and should not trigger reconnection.
      const grpc::StatusCode code = status.error_code();
      if (code == grpc::StatusCode::UNAVAILABLE || code == grpc::StatusCode::DEADLINE_EXCEEDED ||
          code == grpc::StatusCode::INTERNAL) {
        grpc_connected_ = false;
      }
      return;
    }

    if (!response.has_telemetry() || !response.telemetry().has_obstacle()) {
      return;
    }

    const auto & obs = response.telemetry().obstacle();
    const rclcpp::Time stamp = this->now();

    sensor_msgs::msg::Range range_msg;

    converter_.obstacle_distance_to_range(obs.distance_front_left_m(), "obstacle_front_left", stamp, range_msg);
    obstacle_fl_pub_->publish(range_msg);

    converter_.obstacle_distance_to_range(obs.distance_front_right_m(), "obstacle_front_right", stamp, range_msg);
    obstacle_fr_pub_->publish(range_msg);

    converter_.obstacle_distance_to_range(obs.distance_back_left_m(), "obstacle_back_left", stamp, range_msg);
    obstacle_bl_pub_->publish(range_msg);

    converter_.obstacle_distance_to_range(obs.distance_back_right_m(), "obstacle_back_right", stamp, range_msg);
    obstacle_br_pub_->publish(range_msg);

    std_msgs::msg::Bool detected_msg;
    detected_msg.data = obs.any_obstacle();
    obstacle_detected_pub_->publish(detected_msg);
  } catch (const std::exception & e) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 5000, "Exception in obstacle_poll_timer_callback: %s",
                          e.what());
  } catch (...) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                          "Unknown exception in obstacle_poll_timer_callback");
  }
}

// ===========================================================================
// gRPC Helpers
// ===========================================================================

/**
 * @brief Attempt to reconnect the gRPC client to the Go gateway with
 *        exponential backoff.
 *
 * @details
 * Called by connection_watchdog_callback() when is_grpc_connected() returns
 * false.  Increments reconnect_attempts_ on every call; gives up and returns
 * false once MAX_RECONNECT_ATTEMPTS is reached.  Each attempt sleeps for
 * RECONNECT_BACKOFF_MS_BASE * 2^min(reconnect_attempts_, MAX_BACKOFF_EXPONENT)
 * milliseconds before calling initialize_grpc_client().
 *
 * @return true   initialize_grpc_client() succeeded and the stub is ready.
 * @return false  Max reconnection attempts reached; logging throttled to once
 *                per 30 s.
 *
 * @retval true   gRPC channel entered READY state within the deadline.
 * @retval false  Either MAX_RECONNECT_ATTEMPTS exhausted or
 *                initialize_grpc_client() reported a timeout.
 *
 * @pre Called from connection_watchdog_callback() when is_grpc_connected()
 * returns false; gRPC stub and channel state must be validable by
 * initialize_grpc_client().
 * @pre reconnect_attempts_ and MAX_RECONNECT_ATTEMPTS are accessible and
 * RECONNECT_BACKOFF_MS_BASE > 0.
 * @post reconnect_attempts_ is incremented on each invocation.
 * @post Returns true if initialize_grpc_client() succeeds and the channel
 * enters READY state within the deadline; returns false if
 * MAX_RECONNECT_ATTEMPTS is reached or a timeout occurs; error logging is
 * throttled at the MAX_RECONNECT_ATTEMPTS limit to avoid log spam.
 *
 * @note Blocks the calling thread (watchdog timer callback) via
 *       std::this_thread::sleep_for for the computed backoff duration.
 * @note Error logging is throttled to once per 30 s at the
 *       MAX_RECONNECT_ATTEMPTS limit to avoid log spam.
 * @since Version 1.0.0
 */
bool StarGatewayBridgeNode::reconnect_grpc_client()
{
  assert(MAX_RECONNECT_ATTEMPTS > 0);
  assert(RECONNECT_BACKOFF_MS_BASE > 0 && reconnect_attempts_ >= 0);
  if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 30000,
                          "Max reconnection attempts (%d) reached - giving up", MAX_RECONNECT_ATTEMPTS);
    return false;
  }

  reconnect_attempts_++;

  // Exponential backoff
  int backoff_ms = RECONNECT_BACKOFF_MS_BASE * (1 << std::min(reconnect_attempts_, MAX_BACKOFF_EXPONENT));

  RCLCPP_INFO(this->get_logger(), "Reconnection attempt %d/%d (backoff: %dms)", reconnect_attempts_,
              MAX_RECONNECT_ATTEMPTS, backoff_ms);

  std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

  return initialize_grpc_client();
}

bool StarGatewayBridgeNode::is_grpc_connected() const
{
  if (!grpc_channel_) {
    return false;
  }

  // false = don't try to connect
  auto state = grpc_channel_->GetState(false);
  return state == GRPC_CHANNEL_READY;
}

void StarGatewayBridgeNode::check_teleop_sequence_continuity(uint32_t current_sequence)
{
  if (first_teleop_frame_) {
    last_teleop_sequence_ = current_sequence;
    first_teleop_frame_ = false;
    RCLCPP_INFO(this->get_logger(), "First teleop frame received, sequence=%u", current_sequence);
  } else {
    uint32_t expected_seq = last_teleop_sequence_ + 1;

    // Detect sequence gap (accounting for uint32 wraparound)
    if (current_sequence != expected_seq) {
      uint32_t gap = current_sequence - expected_seq;

      // Sanity check: ignore huge gaps (likely system reset or wraparound)
      if (gap < SEQUENCE_RESTART_THRESHOLD) {
        teleop_frames_dropped_ += gap;
        RCLCPP_WARN(this->get_logger(),
                    "Teleop frame drop: expected seq %u, got %u (gap=%u, "
                    "total_dropped=%lu)",
                    expected_seq, current_sequence, gap, teleop_frames_dropped_);
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

void StarGatewayBridgeNode::check_telemetry_sequence_continuity(uint32_t current_sequence)
{
  if (first_telemetry_frame_) {
    last_telemetry_sequence_ = current_sequence;
    first_telemetry_frame_ = false;
    RCLCPP_INFO(this->get_logger(), "First telemetry frame received, sequence=%u", current_sequence);
  } else {
    uint32_t expected_seq = last_telemetry_sequence_ + 1;

    // Detect sequence gap (accounting for uint32 wraparound)
    if (current_sequence != expected_seq) {
      uint32_t gap = current_sequence - expected_seq;

      // Sanity check: ignore huge gaps (likely system reset or wraparound)
      if (gap < SEQUENCE_RESTART_THRESHOLD) {
        telemetry_frames_dropped_ += gap;
        RCLCPP_WARN(this->get_logger(),
                    "Telemetry frame drop: expected seq %u, got %u (gap=%u, "
                    "total_dropped=%lu)",
                    expected_seq, current_sequence, gap, telemetry_frames_dropped_);
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

void StarGatewayBridgeNode::publish_diagnostics()
{
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
      status.message = "Minor teleop drops (" + std::to_string(drop_rate) + "%)";
    } else {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      status.message = "Critical teleop loss (" + std::to_string(drop_rate) + "%)";
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

  double drop_rate = total_teleop_frames_ > 0 ? (teleop_frames_dropped_ * 100.0) / total_teleop_frames_ : 0.0;
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
    double telemetry_drop_rate = (telemetry_frames_dropped_ * 100.0) / total_telemetry_frames_;

    if (telemetry_drop_rate < 5.0) {
      telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
      telemetry_status.message = "Minor telemetry drops (" + std::to_string(telemetry_drop_rate) + "%)";
    } else {
      telemetry_status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      telemetry_status.message = "Critical telemetry loss (" + std::to_string(telemetry_drop_rate) + "%)";
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

  double telemetry_drop_rate = total_telemetry_frames_ > 0
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

}  // namespace star::star_gateway_bridge

// Component registration for composable node
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(star::star_gateway_bridge::StarGatewayBridgeNode)
