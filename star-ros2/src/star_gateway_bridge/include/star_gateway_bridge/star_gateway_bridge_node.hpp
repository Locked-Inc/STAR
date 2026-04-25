/**
 * @file star_gateway_bridge_node.hpp
 * @brief ROS2 Gateway Bridge Node -- bridges the ROS2 ecosystem with the Go
 * gateway service.
 *
 * @details
 * This node handles bidirectional communication:
 * - ROS2 -> Gateway: Forward telemetry (robot status, odometry, LiDAR) for UI
 * display.
 * - Gateway -> ROS2: Poll teleop commands and PID gain updates from UI.
 *
 * @note Requires an active gRPC connection to the Go gateway on startup. If the
 * connection fails the node continues and retries via the watchdog timer.
 *
 * @since Version 1.0.0

 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include <grpcpp/grpcpp.h>  // NOLINT(build/include_order)

#include "star/v1/gateway_service.grpc.pb.h"
#include "star/v1/telemetry.grpc.pb.h"
#include "star_gateway_bridge/message_converter.hpp"

namespace star::star_gateway_bridge
{

/**
 * @brief ROS2 node that bridges the ROS2 ecosystem with the Go gateway service
 * via gRPC.
 *
 * Architecture:
 *   ROS2 Topics/Services <-> StarGatewayBridgeNode <-> gRPC <-> Go Gateway <->
 * UI
 *
 * Responsibilities:
 * 1. Subscribe to /robot_status ROS2 topic
 * 2. Forward critical telemetry to Gateway via gRPC for UI display
 * 3. Poll Gateway for teleop commands from UI
 * 4. Publish teleop commands to /teleop/cmd_vel ROS2 topic
 * 5. Expose /set_pid_gains ROS2 service for PID tuning from UI
 *
 * Safety Features:
 * - Teleop command staleness check (500ms timeout -> zero velocity)
 * - Connection watchdog (5s interval, automatic reconnection)
 * - Synchronous unary gRPC calls bounded by a per-call deadline (100ms)
 * - Input validation (NaN, infinity checks via MessageConverter)
 * - Graceful shutdown (stop command on node destruction)
 *
 * gRPC Service Definition (to be implemented in Phase 4):
 *   service GatewayService {
 *     rpc ForwardTelemetry(TelemetryRequest) returns (TelemetryResponse);
 *     rpc GetTeleopCommand(TeleopRequest) returns (TeleopResponse);
 *     rpc SetPIDGains(PIDGainsRequest) returns (PIDGainsResponse);
 *   }
 */
class StarGatewayBridgeNode : public rclcpp::Node {
public:
  /**
   * @brief Construct StarGatewayBridgeNode with ROS2 and gRPC initialization.
   *
   * Declares parameters:
   * - gateway_address: Gateway gRPC server address (default: "localhost:50051")
   * - telemetry_rate_hz: Telemetry forwarding rate (default: 10.0 Hz)
   * - teleop_rate_hz: Teleop polling rate (default: 50.0 Hz)
   * - watchdog_timeout_sec: Connection health check interval (default: 5.0s)
   * - teleop_timeout_ms: Teleop command staleness timeout (default: 500ms)
   * - grpc_deadline_ms: gRPC call deadline (default: 100ms)
   * - wheel_base: Distance between left and right wheel centers in meters
   *               (default: 0.356m). Note: legacy name -- this parameter is
   *               actually the track width used for differential-drive
   *               twist-to-wheel-velocity conversion. A rename to
   *               'track_width' is tracked as a separate cleanup task to
   *               avoid touching gateway_bridge + spi_bridge + 30+ test
   *               sites + launch files in this PR.
   *
   * @param options ROS2 node options for component configuration
   */
  explicit StarGatewayBridgeNode(
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  /**
   * @brief Destructor - publishes a zero-velocity Twist on /teleop/cmd_vel
   * and lets the gRPC channel/stubs tear down via shared_ptr destruction.
   *
   * @details
   * Cancels the obstacle poll timer, resets the obstacle publishers and
   * the odom/slam_pose/scan subscriptions, and publishes a single
   * geometry_msgs::msg::Twist with all-zero linear/angular components on
   * teleop_cmd_vel_pub_ (if it is still alive) so any downstream consumer
   * stops moving before the node is destroyed. No explicit gRPC shutdown
   * RPC is issued: grpc_channel_, grpc_stub_, and telemetry_svc_stub_ own
   * their resources via shared_ptr and are released when the node is
   * destroyed. The "Closing gRPC channel" log line records that release.
   */
  ~StarGatewayBridgeNode();

private:
  // ===========================================================================
  // Initialization
  // ===========================================================================

  /**
   * @brief Initialize gRPC client connection to Gateway service.
   *
   * Creates gRPC channel and stub. If connection fails, logs warning and
   * schedules reconnection attempt.
   *
   * @return true if connection successful, false otherwise
   */
  bool initialize_grpc_client();

  /**
   * @brief Initialize ROS2 interfaces (subscribers, publishers, services,
   * timers).
   */
  void initialize_ros_interfaces();

  // ===========================================================================
  // ROS2 Callbacks
  // ===========================================================================

  /**
   * @brief Callback for /robot_status topic (std_msgs/String).
   *
   * Caches latest robot status for telemetry forwarding.
   * Uses non-blocking mutex (try_lock) to avoid delaying callback.
   *
   * @param msg Robot status message (JSON format)
   */
  void robot_status_callback(const std_msgs::msg::String::SharedPtr msg);

  /**
   * @brief Service callback for /set_pid_gains (std_srvs/SetBool placeholder).
   *
   * TODO: Define custom service type for PID gains (kp, ki, kd).
   * For now, uses SetBool as placeholder.
   *
   * @param request Service request
   * @param response Service response
   */
  void set_pid_gains_callback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response);

  // ===========================================================================
  // Timer Callbacks
  // ===========================================================================

  /**
   * @brief Timer callback for telemetry forwarding (10 Hz).
   *
   * Forwards cached robot status to Gateway via gRPC.
   * Uses a synchronous unary gRPC call bounded by a per-call deadline; the
   * call blocks the caller for up to grpc_deadline_ms_ before returning.
   */
  void telemetry_forward_timer_callback();

  /**
   * @brief Timer callback for teleop command polling (50 Hz).
   *
   * Polls Gateway for latest teleop command from UI.
   * Publishes to /teleop/cmd_vel if command is fresh (<500ms old).
   * Publishes zero velocity if command is stale (safety feature).
   */
  void teleop_poll_timer_callback();

  /**
   * @brief Timer callback for connection health check (5s interval).
   *
   * Monitors gRPC connection health and triggers reconnection if needed.
   */
  void connection_watchdog_callback();

  /**
   * @brief Timer callback that polls TelemetryService::GetTelemetry and
   * publishes ObstacleData fields to ROS2 topics at telemetry_rate_hz_.
   *
   * @details
   * Calls TelemetryService::GetTelemetry on the Go gateway using
   * telemetry_svc_stub_ to obtain the latest RX72N TelemetryData, which
   * includes the ObstacleData obstacle sub-message populated by firmware PR
   * #371. Each sensor distance is converted to a sensor_msgs/Range message via
   * MessageConverter::obstacle_distance_to_range() and published on the
   * corresponding /star/obstacle/<side> topic. The any_obstacle boolean is
   * forwarded on /star/obstacle_detected (std_msgs/Bool).
   *
   * The callback is a no-op when grpc_connected_ is false or
   * telemetry_svc_stub_ is null (e.g. during initial connection or after a
   * gRPC failure). A WARN log is emitted at most once per 5 seconds on gRPC
   * errors to avoid log spam.
   *
   * @pre grpc_connected_ must be true for any gRPC call to be attempted.
   * @pre telemetry_svc_stub_ must be non-null (initialised in
   *      initialize_grpc_client()).
   * @post Five ROS2 messages published when telemetry data with an obstacle
   *       sub-message is available: four sensor_msgs/Range and one
   *       std_msgs/Bool.
   * @post grpc_connected_ is set to false only on transport-level gRPC errors
   *       (UNAVAILABLE, DEADLINE_EXCEEDED, INTERNAL) to trigger watchdog
   *       reconnection; application-level errors do not alter the flag.
   *
   * @note Called by obstacle_timer_ at telemetry_rate_hz_ (default 10 Hz).
   * @note Not thread-safe; called exclusively from the ROS2 timer executor
   *       thread.
   * @note All exceptions are caught internally; the callback never propagates
   *       exceptions to the ROS2 executor.
   *
   * @since Version 1.0.0
   */
  void obstacle_poll_timer_callback();

  // ===========================================================================
  // gRPC Helpers
  // ===========================================================================

  /**
   * @brief Reconnect to Gateway gRPC server.
   *
   * Called when connection health check fails or gRPC call returns error.
   * Uses exponential backoff for retry attempts.
   *
   * @return true if reconnection successful, false otherwise
   */
  bool reconnect_grpc_client();

  /**
   * @brief Check if gRPC channel is in READY state.
   * @return true if connected, false otherwise
   */
  bool is_grpc_connected() const;

  // ===========================================================================
  // Member Variables
  // ===========================================================================

  // ROS2 Publishers
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr teleop_cmd_vel_pub_;

  /** @brief Publisher for front-left HC-SR04 sensor distance (sensor_msgs/Range). */
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr obstacle_fl_pub_;
  /** @brief Publisher for front-right HC-SR04 sensor distance (sensor_msgs/Range). */
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr obstacle_fr_pub_;
  /** @brief Publisher for back-left HC-SR04 sensor distance (sensor_msgs/Range). */
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr obstacle_bl_pub_;
  /** @brief Publisher for back-right HC-SR04 sensor distance (sensor_msgs/Range). */
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr obstacle_br_pub_;
  /** @brief Publisher for any-obstacle detection flag (std_msgs/Bool). */
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr obstacle_detected_pub_;

  // ROS2 Subscribers
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr robot_status_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr
    odom_sub_;   /**< Receives filtered odometry and updates
                    cached_ekf_odometry_ under odometry_mutex_. */
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr
    slam_pose_sub_;   /**< Receives SLAM pose and updates cached_slam_pose_
                         under odometry_mutex_. */
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr
    scan_sub_;   /**< Receives LaserScan telemetry and updates
                    cached_lidar_scan_ under lidar_mutex_. */

  // ROS2 Services
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr set_pid_gains_service_;

  // ROS2 Timers
  rclcpp::TimerBase::SharedPtr telemetry_timer_;
  rclcpp::TimerBase::SharedPtr teleop_timer_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;
  /** @brief Timer that polls TelemetryService::GetTelemetry and publishes
   *         obstacle data at telemetry_rate_hz_ (default 10 Hz). */
  rclcpp::TimerBase::SharedPtr obstacle_timer_;

  // gRPC Client
  std::shared_ptr<grpc::Channel> grpc_channel_;
  std::unique_ptr<star::v1::GatewayService::Stub> grpc_stub_;
  /**
   * @brief gRPC stub for TelemetryService (GetTelemetry) on the same channel.
   *
   * @details
   * Created in initialize_grpc_client() using the same grpc_channel_ as
   * grpc_stub_. Used by obstacle_poll_timer_callback() to retrieve the latest
   * RX72N TelemetryData (including ObstacleData) from the Go gateway.
   *
   * @note Shares grpc_channel_ with grpc_stub_; no separate connection needed.
   * @since Version 1.0.0
   */
  std::unique_ptr<star::v1::TelemetryService::Stub> telemetry_svc_stub_;

  // Cached telemetry data (protected by mutexes)
  std::mutex robot_status_mutex_;
  std::optional<std_msgs::msg::String> cached_robot_status_;

  std::mutex odometry_mutex_; /**< Guards cached_ekf_odometry_,
                                 cached_slam_pose_, and their timestamps. */
  std::optional<star::v1::OdometryData>
  cached_ekf_odometry_;     /**< Latest EKF odometry proto (/odometry/filtered).
                             */
  std::optional<star::v1::OdometryData>
  cached_slam_pose_;     /**< Latest SLAM pose proto (/slam_toolbox/pose). */
  int64_t cached_ekf_timestamp_us_{
    0};   /**< Timestamp of cached_ekf_odometry_ in microseconds. */
  int64_t cached_slam_timestamp_us_{
    0};   /**< Timestamp of cached_slam_pose_ in microseconds. */
  std::mutex lidar_mutex_; /**< Guards cached_lidar_scan_. */
  std::optional<star::v1::LidarScan>
  cached_lidar_scan_;     /**< Latest lidar scan proto. */

  // Parameters (cached for performance)
  std::string gateway_address_;
  double telemetry_rate_hz_;
  double teleop_rate_hz_;
  double watchdog_timeout_sec_;
  int teleop_timeout_ms_;
  int grpc_deadline_ms_;
  double wheel_base_;

  // Connection state
  bool grpc_connected_;
  int reconnect_attempts_;
  static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
  static constexpr int RECONNECT_BACKOFF_MS_BASE = 100;
  static constexpr int MAX_BACKOFF_EXPONENT = 5;
  /**
   * @brief Sequence gap threshold used to detect UI restart vs dropped frames.
   *
   * @details
   * UI sequence counters restart at 0 after a browser/app restart. A very large
   * forward jump (>= 10000) is therefore interpreted as a stream restart
   * instead of packet loss so drop metrics are not inflated by reconnect
   * events.
   */
  static constexpr uint32_t SEQUENCE_RESTART_THRESHOLD = 10000;

  // Message converter (stateless utility)
  MessageConverter converter_;

  // Frame drop detection for teleop commands and telemetry
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr
    diagnostics_pub_;
  rclcpp::TimerBase::SharedPtr diagnostics_timer_;

  uint32_t last_teleop_sequence_{0};
  uint64_t total_teleop_frames_{0};
  uint64_t teleop_frames_dropped_{0};
  bool first_teleop_frame_{true};

  uint32_t last_telemetry_sequence_{0};
  uint64_t total_telemetry_frames_{0};
  uint64_t telemetry_frames_dropped_{0};
  bool first_telemetry_frame_{true};

  // Methods

  /**
   * @brief Publish teleop and telemetry frame-drop diagnostics at 1 Hz.
   *
   * @details
   * Builds a DiagnosticArray containing two DiagnosticStatus entries:
   * one for teleop command drops and one for telemetry frame drops. Severity
   * levels are OK / WARN / ERROR / STALE based on the calculated drop rate.
   *
   * @pre  diagnostics_pub_ has been initialized by initialize_ros_interfaces().
   * @pre  diagnostics_timer_ fires at 1 Hz after initialize_ros_interfaces().
   * @post A DiagnosticArray is published on /diagnostics with two status
   * entries.
   * @post total_teleop_frames_ and total_telemetry_frames_ are read but not
   * modified.
   *
   * @note Called by diagnostics_timer_ at 1 Hz.
   * @throw None.
   */
  void publish_diagnostics();

  /**
   * @brief Detect and log gaps in the teleop command sequence number stream.
   *
   * @details
   * Compares current_sequence against the last observed sequence number.
   * Any positive gap (accounting for uint32 wraparound) increments
   * teleop_frames_dropped_ and emits a WARN log. Gaps >=
   * SEQUENCE_RESTART_THRESHOLD are treated as
   * a system restart and logged at WARN without incrementing the drop counter.
   *
   * @param[in] current_sequence Sequence number of the latest teleop frame.
   *
   * @pre  first_teleop_frame_ is false after the first call returns.
   * @pre  teleop_poll_timer_callback() is the sole caller (single-threaded ROS2
   * timer).
   * @post last_teleop_sequence_ updated to current_sequence.
   * @post total_teleop_frames_ incremented by 1.
   * @post teleop_frames_dropped_ incremented by the gap size
   *       (if gap < SEQUENCE_RESTART_THRESHOLD).
   *
   * @note Called from teleop_poll_timer_callback() on every fresh command.
   * @throw None.
   */
  void check_teleop_sequence_continuity(uint32_t current_sequence);

  /**
   * @brief Detect and log gaps in the outgoing telemetry sequence number
   * stream.
   *
   * @details
   * Mirrors check_teleop_sequence_continuity() for the telemetry direction.
   * Gaps >= SEQUENCE_RESTART_THRESHOLD are treated as a system restart and
   * ignored.
   *
   * @param[in] current_sequence Sequence number of the latest telemetry frame.
   *
   * @pre  first_telemetry_frame_ is false after the first call returns.
   * @pre  telemetry_forward_timer_callback() is the sole caller
   * (single-threaded ROS2 timer).
   * @post last_telemetry_sequence_ updated to current_sequence.
   * @post total_telemetry_frames_ incremented by 1.
   * @post telemetry_frames_dropped_ incremented by the gap size
   *       (if gap < SEQUENCE_RESTART_THRESHOLD).
   *
   * @note Called from telemetry_forward_timer_callback() on each successful
   * gRPC send.
   * @throw None.
   */
  void check_telemetry_sequence_continuity(uint32_t current_sequence);
};

} // namespace star::star_gateway_bridge
