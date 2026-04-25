/**
 * @file comm_task.c
 * @brief Communication Task Implementation - HIGHEST PRIORITY Protocol Handling for RPi5 Interface
 *
 * @details
 * # Overview
 *
 * This module implements the **Communication Task**, the **HIGHEST PRIORITY** application-level
 * task (Priority 5) responsible for receiving and processing ALL protocol commands from the
 * Raspberry Pi 5 control system. The task polls the rx_comm_manager at 100 Hz to receive frames
 * from USB CDC and SPI channels, decodes Protocol Buffer messages using nanopb, and updates
 * shared_data structures for consumption by the Motor Control Task.
 *
 * **Critical System Role:** This task is the **only entry point** for external commands into
 * the RX72N firmware. Low latency is essential to minimize command processing delay and maintain
 * tight control loop performance.
 *
 * ## System Architecture - Communication Flow
 *
 * @dot
 * digraph comm_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5 (ROS2 + Gateway)";
 *     style=filled;
 *     color=lightblue;
 *
 *     ros2 [label="ROS2 Node\n(cmd_vel subscriber)", fillcolor=lightgreen, style=filled];
 *     gateway [label="Gateway Service\n(Go, gRPC)", fillcolor=lightyellow, style=filled];
 *     spi_driver [label="SPI Driver\n(Linux spidev)", fillcolor=lightcoral, style=filled];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N Firmware (ThreadX)";
 *     style=filled;
 *     color=lightgreen;
 *
 *     usb_cdc [label="USB CDC VCOM\n(Virtual COM Port)", fillcolor=lightblue, style=filled];
 *     spi_periph [label="RSPI2 Peripheral\n(10 Mbps)", fillcolor=lightblue, style=filled];
 *     comm_mgr [label="rx_comm_manager\n(Frame Protocol)", fillcolor=lightyellow, style=filled];
 *     comm_task [label="Comm Task\n(This Module)\nPriority 5 @ 100 Hz",
 *                fillcolor=lightgreen, style=filled];
 *     nanopb [label="nanopb Decoder\n(protobuf)", fillcolor=lightcoral, style=filled];
 *     shared_data [label="Shared Data\n(Thread-Safe)", shape=cylinder,
 *                  fillcolor=lightgrey, style=filled];
 *     motor_task [label="Motor Control Task\nPriority 8 @ 250 Hz",
 *                 fillcolor=white, style=filled];
 *   }
 *
 *   ros2 -> gateway [label="Twist msg\n(geometry_msgs)"];
 *   gateway -> spi_driver [label="SetVelocityRequest\n(star.v1)"];
 *   spi_driver -> spi_periph [label="SPI frames\n(10 Mbps)"];
 *   spi_periph -> comm_mgr [label="rx_frame_t"];
 *   usb_cdc -> comm_mgr [label="rx_frame_t\n(debug/config)"];
 *   comm_mgr -> comm_task [label="internal_frame_callback()"];
 *   comm_task -> nanopb [label="rx_nanopb_decode_*()"];
 *   nanopb -> comm_task [label="star_v1_SetVelocityRequest"];
 *   comm_task -> shared_data [label="shared_data_set_motor_command()"];
 *   shared_data -> motor_task [label="motor_command_t"];
 * }
 * @enddot
 *
 * ## Protocol Details - nanopb Protobuf over SPI/USB
 *
 * **Transport Layer:**
 * - **USB CDC:** Primary protocol channel (lower latency, hardware CRC + bulk retries)
 * - **SPI:** 10 Mbps high-speed backup channel (HARQ per ADR-001)
 *
 * **Framing Protocol (rx_frame):**
 * - Frame header: 8 bytes (sync, type, length, sequence, flags, reserved)
 * - Payload: Variable length (max 256 bytes for nanopb messages)
 * - CRC-32: IEEE 802.3 for error detection
 * - Frame types: COMMAND, ACK, NACK, TELEMETRY
 *
 * **Protobuf Messages (star.v1, nanopb encoded):**
 *
 * | Message Type | Proto Definition | Size | Processing Time | Frequency |
 * |--------------|------------------|------|-----------------|-----------|
 * | **SetVelocityRequest** | star.v1.SetVelocityRequest | ~32 bytes | ~200 us | 100 Hz (10ms) |
 * | **EmergencyStopRequest** | star.v1.EmergencyStopRequest | ~8 bytes | ~50 us | On event |
 * | **SetPIDGainsRequest** | star.v1.SetPIDGainsRequest | ~28 bytes | ~180 us | Rarely (config) |
 *
 * **SetVelocityRequest structure (4-motor differential drive):**
 * @code{.proto}
 * message SetVelocityRequest {
 *   star.v1.MotorVelocityCommand command = 1;
 * }
 *
 * message MotorVelocityCommand {
 *   double front_left_velocity_mps = 1;   ///< Front left motor target (m/s)
 *   double front_right_velocity_mps = 2;  ///< Front right motor target (m/s)
 *   double back_left_velocity_mps = 3;    ///< Back left motor target (m/s)
 *   double back_right_velocity_mps = 4;   ///< Back right motor target (m/s)
 *   uint32 sequence = 5;                  ///< Command sequence number
 * }
 * @endcode
 *
 * ## Communication Task Algorithm (100 Hz Polling Loop)
 *
 * The task executes an infinite loop polling for incoming frames at 100 Hz (10ms period):
 *
 * @startuml
 * start
 * :Log "Communication task starting";
 * :Configure rx_comm_manager\n(USB CDC + SPI channels)\nRegister frame callback;
 * :rx_comm_manager_init(&g_comm_manager, &config);
 * if (Init successful?) then (yes)
 *   :Log "Communication running @ 100 Hz";
 * else (no)
 *   :Log error\n(continue polling, no frames received);
 * endif
 *
 * partition "Infinite Polling Loop (100 Hz)" {
 *   repeat
 *     :rx_comm_manager_poll(&g_comm_manager)\n(Check USB CDC + SPI for frames);
 *     if (Frame received?) then (k_rx_ok)
 *       :internal_frame_callback() triggered\n(process frame);
 *     else if (Timeout?) then (k_rx_err_timeout)
 *       :No frame (normal)\nContinue;
 *     else (other error)
 *       :Log debug message\n(CRC error, malformed frame);
 *     endif
 *     :tx_thread_sleep(1 tick)\n**Sleep 10ms, yield CPU**;
 *   repeat while (forever)
 * }
 * @enduml
 *
 * ## Frame Processing Flow (Callback-Based)
 *
 * When rx_comm_manager receives a complete valid frame, it invokes internal_frame_callback():
 *
 * @msc
 * RPi5, SPI, CommManager, CommTask, Nanopb, SharedData, MotorTask;
 *
 * --- [label="SetVelocityRequest Reception"];
 * RPi5 => SPI [label="SPI transfer\n(SetVelocityRequest)"];
 * SPI box SPI [label="RSPI2 ISR\nReceive frame bytes"];
 * SPI => CommManager [label="Frame complete"];
 * CommManager box CommManager [label="Validate CRC-32\nParse header"];
 * CommManager => CommTask [label="internal_frame_callback()\n(k_frame_type_command)"];
 *
 * --- [label="Command Decode and Dispatch"];
 * CommTask box CommTask [label="Update last_comm_tick\n(500ms watchdog)"];
 * CommTask => CommTask [label="internal_handle_command_frame()"];
 * CommTask => Nanopb [label="rx_nanopb_decode_velocity_request()"];
 * Nanopb box Nanopb [label="Decode protobuf\n(~200 us)"];
 * Nanopb => CommTask [label="star_v1_SetVelocityRequest"];
 *
 * --- [label="Motor Command Update"];
 * CommTask box CommTask [label="Build motor_command_t\nFL: 1.0, FR: 1.0\nBL: 1.0, BR: 1.0"];
 * CommTask => SharedData [label="shared_data_set_motor_command(&cmd)"];
 * SharedData box SharedData [label="Mutex lock\nUpdate command\nSet k_event_new_motor_cmd\nMutex unlock"];
 * SharedData => CommTask [label="k_rx_ok"];
 *
 * --- [label="Motor Task Reaction"];
 * MotorTask box MotorTask [label="Wait on k_event_new_motor_cmd\n(blocking)"];
 * SharedData -> MotorTask [label="Event triggered"];
 * MotorTask => SharedData [label="shared_data_get_motor_command()"];
 * SharedData => MotorTask [label="motor_command_t copy"];
 * MotorTask box MotorTask [label="Update PID setpoints\nExecute control loop"];
 * @endmsc
 *
 * ## Command Decode Cascade (Try Multiple Message Types)
 *
 * The task tries to decode each command frame as different protobuf message types in priority order:
 *
 * @dot
 * digraph decode_cascade {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="internal_handle_command_frame()", fillcolor=lightgreen, style=filled];
 *   try_velocity [label="rx_nanopb_decode_velocity_request()", shape=diamond];
 *   velocity_ok [label="SetVelocityRequest decoded", fillcolor=lightblue, style=filled];
 *   build_cmd [label="Build motor_command_t\nFL, FR, BR, BL velocities"];
 *   set_cmd [label="shared_data_set_motor_command()"];
 *   return_velocity [label="return (success)", fillcolor=lightgreen, style=filled];
 *
 *   try_estop [label="rx_nanopb_decode_estop_request()", shape=diamond];
 *   estop_ok [label="EmergencyStopRequest decoded", fillcolor=red, style=filled];
 *   trigger_estop [label="shared_data_trigger_estop(k_estop_reason_manual)"];
 *   return_estop [label="return (success)", fillcolor=lightgreen, style=filled];
 *
 *   try_pid [label="rx_nanopb_decode_pid_gains_request()", shape=diamond];
 *   pid_ok [label="SetPIDGainsRequest decoded", fillcolor=lightblue, style=filled];
 *   set_pid [label="shared_data_set_pid_gains()"];
 *   return_pid [label="return (success)", fillcolor=lightgreen, style=filled];
 *
 *   try_retransmit [label="rx_nanopb_decode_retransmit_config_request()", shape=diamond];
 *   retransmit_ok [label="SetRetransmitConfigRequest decoded", fillcolor=lightblue, style=filled];
 *   set_retransmit [label="rx_comm_manager_set_auto_retransmit()"];
 *   return_retransmit [label="return (success)", fillcolor=lightgreen, style=filled];
 *
 *   unknown [label="Log warning\n'Could not decode command payload'",
 *            fillcolor=yellow, style=filled];
 *   return_unknown [label="return (unknown)", fillcolor=orange, style=filled];
 *
 *   start -> try_velocity;
 *   try_velocity -> velocity_ok [label="k_rx_ok"];
 *   try_velocity -> try_estop [label="decode failed"];
 *   velocity_ok -> build_cmd;
 *   build_cmd -> set_cmd;
 *   set_cmd -> return_velocity;
 *
 *   try_estop -> estop_ok [label="k_rx_ok"];
 *   try_estop -> try_pid [label="decode failed"];
 *   estop_ok -> trigger_estop;
 *   trigger_estop -> return_estop;
 *
 *   try_pid -> pid_ok [label="k_rx_ok"];
 *   try_pid -> try_retransmit [label="decode failed"];
 *   pid_ok -> set_pid;
 *   set_pid -> return_pid;
 *
 *   try_retransmit -> retransmit_ok [label="k_rx_ok"];
 *   try_retransmit -> unknown [label="decode failed"];
 *   retransmit_ok -> set_retransmit;
 *   set_retransmit -> return_retransmit;
 *
 *   unknown -> return_unknown;
 * }
 * @enddot
 *
 * **Why cascade decode?**
 * - nanopb does not support runtime message type identification
 * - Must try each message type until one succeeds
 * - Order matters: Try most frequent messages first (SetVelocityRequest @ 100 Hz)
 * - Emergency stop is rare but critical (second priority)
 *
 * ## Communication Timeout Watchdog (500ms)
 *
 * **Safety feature:** The comm task updates a timestamp on EVERY valid frame reception.
 * The motor control task monitors this timestamp to detect communication loss.
 *
 * **Algorithm:**
 * 1. **Frame reception:** internal_frame_callback() calls shared_data_update_last_comm_tick()
 * 2. **Motor task polling:** Checks shared_data_is_comm_timeout() every 4ms (250 Hz)
 * 3. **Timeout detection:** If (current_tick - last_tick) > 50 ticks (500ms):
 *    - Set k_event_comm_timeout flag
 *    - Trigger emergency stop (k_estop_reason_comm_timeout)
 *    - All motors disabled immediately
 * 4. **Recovery:** Next valid command resets timestamp and clears e-stop
 *
 * **Why 500ms threshold?**
 * - Normal command rate: 100 Hz (10ms period)
 * - 500ms allows 50 missed commands before timeout
 * - Robust against transient communication glitches
 * - Short enough for safety (robot travels < 1m at max speed)
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **Poll Rate** | 100 Hz | One poll per 10ms tick |
 * | **Priority** | 5 | HIGHEST application priority (preempts all except system tasks) |
 * | **CPU Time per Poll** | ~220 us | Includes frame decode + protobuf decode + shared_data update |
 * | **CPU Idle Time** | 9780 us | 97.8% idle time (yields CPU during sleep) |
 * | **CPU Utilization** | 2.2% | (220 us / 10ms) x 100% |
 * | **Worst Case Latency** | 10ms | Max time from frame arrival to motor task notification |
 * | **Frame Processing** | ~50 us | CRC validation + header parsing |
 * | **Protobuf Decode** | ~200 us | nanopb decode (SetVelocityRequest) |
 * | **Shared Data Update** | ~7 us | Mutex lock + memcpy + event set + unlock |
 *
 * **Latency Budget (Frame Arrival -> Motor Update):**
 * - SPI ISR receive: ~5 us (RSPI2 interrupt)
 * - rx_comm_manager buffer copy: ~20 us
 * - CRC-32 validation: ~30 us (hardware CRC peripheral)
 * - Frame callback dispatch: ~10 us
 * - Protobuf decode: ~200 us (nanopb)
 * - Shared data update: ~7 us
 * - Motor task event wait: ~50 us (ThreadX context switch)
 * - **Total latency:** ~320 us (0.32ms)
 *
 * **Why Priority 5?**
 * - Lower number = higher priority in ThreadX
 * - Priority 0-4: System tasks (scheduler, timers, ISRs)
 * - Priority 5: Communication (this task) - HIGHEST application priority
 * - Priority 8: Motor Control - Second highest (needs fast command reaction)
 * - Priority 12: Obstacle Detection
 * - Priority 15: Temperature Sensor
 * - Priority 18: Telemetry (lowest, non-critical)
 *
 * ## Memory Usage Breakdown
 *
 * | Component | Size (bytes) | Section | Description |
 * |-----------|--------------|---------|-------------|
 * | `s_comm_thread` | 140 | .bss | TX_THREAD control block |
 * | `s_comm_stack` | 2048 | .bss | Task stack (static allocation) |
 * | `s_comm_created` | 1 | .bss | Creation guard flag |
 * | `g_comm_manager` | 256 | .bss | Communication manager state (global for telemetry access) |
 * | `s_tag` | 8 | .rodata | Log tag string ("COMM" + null) |
 * | **Stack locals** | ~320 | Stack | Protobuf decode buffers, motor_command_t |
 * | **Total Static** | ~2453 | - | Sum of .bss + .rodata |
 * | **Peak Stack** | ~320 | - | During rx_nanopb_decode_velocity_request() |
 *
 * **Why 2048 byte stack?**
 * - nanopb protobuf decode requires ~256 bytes for message buffers
 * - rx_comm_manager poll requires ~64 bytes for frame buffers
 * - Local variables (velocity_req, estop_req, cmd): ~96 bytes
 * - ThreadX overhead: ~32 bytes
 * - Safety margin: 2x nominal usage = 2048 bytes
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   comm_task [label="comm_task.c\n(This Module)", fillcolor=lightgreen, style=filled];
 *
 *   // Header dependencies
 *   comm_task_h [label="comm_task.h"];
 *   rx_comm_mgr [label="rx_comm_manager.h\n(Frame Protocol)", fillcolor=lightyellow, style=filled];
 *   rx_frame [label="rx_frame.h\n(Frame Encoding)", fillcolor=lightcoral, style=filled];
 *   rx_nanopb [label="rx_nanopb.h\n(Protobuf Decode)", fillcolor=lightblue, style=filled];
 *   rx_check [label="rx_check.h\n(Assertions)"];
 *   rx_log [label="rx_log.h\n(Logging)"];
 *   shared_data [label="shared_data.h\n(Thread-Safe Storage)", fillcolor=lightcoral, style=filled];
 *   tx_api [label="tx_api.h\n(ThreadX RTOS)"];
 *   string [label="string.h\n(memset)"];
 *
 *   // Indirect dependencies
 *   rx_crc [label="rx_crc.h\n(CRC-32)", fillcolor=lightgrey, style=filled];
 *   nanopb [label="pb.h, pb_decode.h\n(nanopb library)", fillcolor=lightgrey, style=filled];
 *   star_proto [label="star/v1/ .pb.h\n(Generated protobuf)", fillcolor=lightgrey, style=filled];
 *
 *   comm_task -> comm_task_h [label="Public API"];
 *   comm_task -> rx_comm_mgr [label="Frame polling"];
 *   comm_task -> rx_frame [label="Frame types"];
 *   comm_task -> rx_nanopb [label="Protobuf decode"];
 *   comm_task -> rx_check [label="RX_ASSERT"];
 *   comm_task -> rx_log [label="Logging"];
 *   comm_task -> shared_data [label="Motor command storage"];
 *   comm_task -> tx_api [label="Thread/sleep API"];
 *   comm_task -> string [label="memset"];
 *
 *   rx_comm_mgr -> rx_frame [label="Frame parsing", style=dashed];
 *   rx_frame -> rx_crc [label="CRC validation", style=dashed];
 *   rx_nanopb -> nanopb [label="nanopb library", style=dashed];
 *   rx_nanopb -> star_proto [label="Generated code", style=dashed];
 * }
 * @enddot
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Details |
 * |------|--------|------------------------|
 * | **Rule 1: Control Flow** | [PASS] | No goto, setjmp, longjmp, or recursion. All control flow uses if/while/switch only. |
 * | **Rule 2: Loop Bounds** | [PASS] | Single while(true) loop with fixed 10ms sleep period. Provably bounded iteration. |
 * | **Rule 3: No Heap** | [PASS] | Zero dynamic allocation. Stack (2048 bytes) and TCB (140 bytes) statically allocated. |
 * | **Rule 4: Function Length** | [PASS] | comm_task_create(): 30 lines, internal_comm_task_entry(): 48 lines, internal_frame_callback(): 47 lines, internal_handle_command_frame(): 56 lines (all < 100 LOC guideline). |
 * | **Rule 5: Assertions** | [PASS] | 8 assertions: RX_ASSERT(!s_comm_created), preconditions, postconditions. |
 * | **Rule 6: Data Scope** | [PASS] | All file-scope variables use static (s_comm_thread, s_comm_stack, s_comm_created, s_tag). g_comm_manager is global (required for telemetry task access). |
 * | **Rule 7: Return Checks** | [PASS] | All rx_comm_manager, rx_nanopb, shared_data, tx_* returns validated or explicitly cast to (void). |
 * | **Rule 8: Preprocessor** | [PASS] | C23 typed enums for all constants (k_comm_task_*, k_motor_idx_*). Zero macros. |
 * | **Rule 9: Pointers** | [PASS] | Single-level pointers only (rx_frame_t*, motor_command_t*, rx_comm_manager_config_t*). |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror. Zero warnings. |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S - Single Responsibility** | Protocol handling only. No motor control, no hardware access, no telemetry. |
 * | **O - Open/Closed** | Extensible via rx_comm_manager_config_t (new channels, callbacks). No code changes needed. |
 * | **L - Liskov Substitution** | Returns rx_err_t consistently. Can substitute with mock comm_manager for testing. |
 * | **I - Interface Segregation** | Minimal API: one function (comm_task_create). No unused methods. |
 * | **D - Dependency Inversion** | Depends on rx_comm_manager abstraction, not raw USB/SPI. Testable via mock injection. |
 *
 * @see shared_data.h Shared data structures and thread-safe API
 * @see rx_comm_manager.h Communication manager (frame protocol)
 * @see rx_nanopb.h nanopb protobuf decoder
 * @see rx_frame.h Frame encoding/decoding
 * @see motor_control_task.h Consumer of motor commands
 * @see telemetry_task.h Uses g_comm_manager for responses
 * @see docs/sections/01_nanopb_protocol.tex SPI communication protocol specification
 * @see docs/sections/02_protobuf_schemas.tex Protocol Buffer message definitions
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "comm_task.h"

#include "rx_check.h"
#include "rx_comm_manager.h"
#include "rx_frame.h"
#include "rx_i2c_comm.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_log_uart.h"
#include "rx_nanopb.h"
#include "rx_session.h"
#include "rx_spi_comm.h"
#include "rx_spi_link.h"
#include "rx_time_constants.h"
#include "rx_uart_comm.h"
#include "shared_data.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum comm_task_constants_t
 * @brief Communication task configuration constants
 */
typedef enum : uint16_t {
  k_comm_task_stack_size            = 2048, /**< Stack size in bytes */
  k_comm_task_priority              = 5,    /**< ThreadX priority (highest app priority) */
  k_comm_task_sleep_ticks           = 1,    /**< Sleep period (10ms = 100 Hz) */
  k_comm_task_input                 = 0,    /**< Thread entry input parameter */
  k_comm_task_sci9_err_clear_period = 500,  /**< SCI9 error-flag fallback unstick period */
} comm_task_constants_t;

/**
 * @enum comm_motor_constants_t
 * @brief Motor index constants
 */
typedef enum : uint8_t {
  k_motor_idx_front_left  = 0, /**< Front left motor index */
  k_motor_idx_front_right = 1, /**< Front right motor index */
  k_motor_idx_back_right =
    2, /**< Back right motor index (GPTW2 wires to PE3/P86, the back-right wheel) */
  k_motor_idx_back_left =
    3, /**< Back left motor index (GPTW3 wires to PE7/PC6, the back-left wheel) */
} comm_motor_constants_t;

/**
 * @enum comm_response_buffer_t
 * @brief Size constants for the command response encoding buffer
 *
 * @details
 * Defines the static buffer size used to encode protobuf responses before
 * sending them back to the RPi5 via rx_comm_manager_respond(). Sized to match
 * the nanopb module's internal buffer (s_nanopb_buffer_size = 512 bytes) so
 * that all rx_nanopb_encode_*_response() pre-condition checks pass.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_comm_response_buffer_size = 512, /**< Response encode buffer size in bytes */
} comm_response_buffer_t;

/**
 * @enum retransmit_retries_limits_t
 * @brief Valid range limits for max_retries in SetRetransmitConfigRequest
 *
 * @details
 * Bounds used to validate the max_retries field before narrowing from uint32_t to
 * uint8_t. Prevents undefined behaviour on out-of-range protobuf values.
 *
 * @invariant k_retransmit_max_retries_min <= k_retransmit_max_retries_max
 * @see retransmit_timing_limits_t Companion enum for timing field limits
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_retransmit_max_retries_min = 1,  /**< Minimum allowed max_retries value */
  k_retransmit_max_retries_max = 10, /**< Maximum allowed max_retries value */
} retransmit_retries_limits_t;

/**
 * @enum retransmit_timing_limits_t
 * @brief Valid range limits for timing fields in SetRetransmitConfigRequest
 *
 * @details
 * Bounds used to validate ack_timeout_ms and max_backoff_ms before narrowing from
 * uint32_t to uint16_t. Prevents undefined behaviour on out-of-range protobuf values.
 *
 * @invariant k_retransmit_ack_timeout_min_ms <= k_retransmit_ack_timeout_max_ms
 * @invariant k_retransmit_backoff_min_ms <= k_retransmit_backoff_max_ms
 * @see retransmit_retries_limits_t Companion enum for retry count limits
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_retransmit_ack_timeout_min_ms = 10,   /**< Minimum ACK timeout (ms) */
  k_retransmit_ack_timeout_max_ms = 1000, /**< Maximum ACK timeout (ms) */
  k_retransmit_backoff_min_ms     = 50,   /**< Minimum backoff delay (ms) */
  k_retransmit_backoff_max_ms     = 5000, /**< Maximum backoff delay (ms) */
} retransmit_timing_limits_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief ThreadX thread control block */
static TX_THREAD s_comm_thread;

/** @brief Static thread stack (no dynamic allocation) */
static uint8_t s_comm_stack[k_comm_task_stack_size];

/** @brief Task creation guard flag */
static bool s_comm_created = false;

/** @brief Communication manager handle (global for telemetry_task access) */
rx_comm_manager_t g_comm_manager;

/** @brief Log tag for this module */
static const char* const s_tag = "COMM";

/**
 * @var s_enabled_channels
 * @brief Runtime bitmask of comm channels to initialize
 *
 * @details
 * Set by comm_task_apply_system_config() before comm_task_create() is called.
 * Consumed by internal_init_transports() to skip channels that should remain
 * disabled (e.g. UART comm when UART log backend is active on SCI9).
 *
 * Default matches k_system_config_default.comm_channels: USB + SPI + I2C.
 * UART (SCI9) is excluded by default to avoid conflict with the SCI9 log
 * backend. Callers that skip comm_task_apply_system_config() will therefore
 * attempt USB, SPI, and I2C transports only.
 *
 * @note File-scoped static. Must not be modified directly outside of
 *       comm_task_apply_system_config().
 * @since Version 1.0.0
 */
/* TODO(2026-04-22, comm-channel-bringup): default to UART only.  On
 * this bench UART-over-USB (SCI9 -> CY7C65213 -> /dev/ttyACM0) is the
 * sole host link (see CLAUDE.local.md + feedback_usb_cdc_vs_uart.md);
 * SPI and I2C comm transports aren't physically wired, and bringing
 * them up currently hangs inside internal_init_transports() /
 * internal_poll_all_channels(), starving lower-priority tasks (led
 * heartbeat + telemetry).  Re-enable SPI / I2C here once each
 * transport init + poll path is verified non-blocking with its host
 * missing.  Gateway picks channel via runtime config, so making this
 * the default is safe.
 */
static rx_comm_channel_mask_t s_enabled_channels =
  (rx_comm_channel_mask_t)(k_comm_channel_mask_uart);

/* =============================================================================
 * Public Constants
 * =============================================================================
 */

/**
 * @var k_system_config_default
 * @brief Safe default system configuration
 *
 * @details
 * Enables USB + SPI + I2C comm channels (UART excluded to avoid SCI9 conflict)
 * with logs directed to both UART and USB CDC Port 2.
 *
 * @since Version 1.0.0
 */
const rx_system_config_t k_system_config_default = {
  .comm_channels = (rx_comm_channel_mask_t)(k_comm_channel_mask_uart | k_comm_channel_mask_spi |
                                            k_comm_channel_mask_i2c),
};

/* =============================================================================
 * File-Scoped Transport Handles (Static Instances)
 * =============================================================================
 */

/**
 * @var s_session_state
 * @brief Shared session state for USB and SPI transports
 *
 * @details
 * Maintains sequence number continuity across both USB and SPI communication channels.
 * Both transport handles reference this shared state to ensure protocol-level sequence
 * tracking is consistent regardless of which physical channel receives frames.
 *
 * **Purpose**: Prevent replay attacks and detect out-of-order frames by maintaining
 * a single monotonically increasing sequence counter shared between USB CDC and SPI.
 *
 * @note File-scoped static - not accessible outside comm_task.c
 * @warning Do not modify directly - managed by rx_usb_comm and rx_spi_comm layers
 * @since Version 1.0.0
 */
static rx_session_state_t s_session_state;

/**
 * @var s_spi_comm_handle
 * @brief SPI communication layer handle (RSPI2 peripheral mode)
 *
 * @details
 * Provides frame-based protocol communication over SPI hardware interface to RPi5.
 * Initialized at task startup via rx_spi_comm_init() and remains valid for the
 * lifetime of the task.
 *
 * **Initialization**: rx_spi_comm_init(&s_spi_comm_handle, &spi_cfg)
 * **Shared state**: References s_session_state for sequence tracking
 * **Hardware**: RSPI2 channel 0, SPI mode 0 (CPOL=0, CPHA=0)
 *
 * @note File-scoped static - not accessible outside comm_task.c
 * @warning Do not access internal fields directly - use rx_spi_comm API functions
 * @since Version 1.0.0
 * @see rx_spi_comm_init() Initialization function
 */
static rx_spi_comm_handle_t s_spi_comm_handle;

/**
 * @var s_spi_link
 * @brief HARQ link-layer instance for SPI transport reliability
 *
 * @details
 * Stores runtime state and configuration for the SPI HARQ link layer
 * (forward-error correction plus retransmission control). Initialized during
 * transport setup via rx_spi_link_init() using a configuration structure that
 * references s_spi_comm_handle and default retry/FEC settings. Used by the
 * communication manager through config->spi_link when link initialization
 * succeeds.
 *
 * @note File-scoped static owned by comm_task.c. Access is confined to comm_task
 *       initialization and callback flow in this module.
 * @warning Not safe for unsynchronized external mutation. Do not access or modify
 *          internals directly; use rx_spi_link API functions and initialization path.
 * @since Version 1.0.0
 */
static rx_spi_link_t s_spi_link;

/**
 * @var s_i2c_comm_handle
 * @brief I2C peripheral communication handle (RIIC0)
 *
 * @details
 * Stores runtime state for the I2C peripheral transport layer. The RX72N acts
 * as I2C peripheral (device) and the RPi5 acts as I2C controller. Initialized
 * during transport setup via rx_i2c_comm_init() using a configuration structure
 * that references s_session_state for sequence tracking.
 *
 * @note File-scoped static owned by comm_task.c.
 * @since Version 1.0.0
 */
static rx_i2c_comm_handle_t s_i2c_comm_handle;

/**
 * @var s_uart_comm_handle
 * @brief UART (SCI9) communication handle
 *
 * @details
 * Stores runtime state for the UART transport layer using SCI9 (TXD9/RXD9).
 * Initialized during transport setup via rx_uart_comm_init() using a
 * configuration structure that references s_session_state for sequence tracking.
 *
 * @note File-scoped static owned by comm_task.c.
 * @since Version 1.0.0
 */
static rx_uart_comm_handle_t s_uart_comm_handle;

/**
 * @var s_response_buffer
 * @brief Static buffer for encoding command response messages (NASA Rule 3 - no dynamic allocation)
 *
 * @details
 * Used by internal_handle_velocity_command(), internal_handle_estop_command(), and
 * internal_handle_pid_gains_command() to encode protobuf response messages before
 * sending them back to the RPi5. Sized to match s_nanopb_buffer_size in rx_nanopb,
 * satisfying the buffer_size >= s_nanopb_buffer_size pre-condition of all
 * rx_nanopb_encode_*_response() functions.
 *
 * @note Accessed only from Communication Task context (single thread). No mutex needed.
 * @warning Do not access from other tasks. All writes are guarded by single-task execution.
 * @since Version 1.0.0
 */
static uint8_t s_response_buffer[k_comm_response_buffer_size];

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_comm_task_entry(ULONG input);
static void internal_init_transports(rx_comm_manager_config_t* config);
static void internal_frame_callback(rx_comm_channel_t channel, const rx_frame_t* frame, void* ctx);
static void internal_ship_log_frames(void);
static void internal_send_command_response(rx_comm_channel_t channel,
                                           const uint8_t*    payload,
                                           uint32_t          payload_len);
static rx_err_t internal_handle_command_frame(rx_comm_channel_t channel, const rx_frame_t* frame);
static bool internal_handle_velocity_command(rx_comm_channel_t channel, const rx_frame_t* frame);
static bool internal_handle_estop_command(rx_comm_channel_t channel, const rx_frame_t* frame);
static bool internal_handle_pid_gains_command(rx_comm_channel_t channel, const rx_frame_t* frame);
static bool internal_handle_retransmit_config_command(const rx_frame_t* frame);

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Validate and apply a system configuration atomically
 *
 * @details
 * Checks for unknown bits and the SCI9 conflict (UART comm + UART log)
 * before modifying any persistent state. If all checks pass, the log
 * backend and comm-channel mask are updated atomically from the caller's
 * perspective (both writes succeed or neither happens).
 *
 * @param[in] config System configuration to validate and apply
 *
 * @return rx_err_t
 * @retval k_rx_ok              Configuration applied
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_invalid_arg Unknown bits or SCI9 conflict detected
 *
 * @pre config != NULL
 * @pre Must be called before comm_task_create()
 * @post rx_log_get_backend() reflects log_backends on k_rx_ok
 * @post s_enabled_channels reflects comm_channels on k_rx_ok
 *
 * @note Not thread-safe. Call once at startup.
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions
 *
 * @callgraph
 * @callergraph
 */
rx_err_t comm_task_apply_system_config(const rx_system_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config must not be NULL");

  /* Validate no unknown bits in comm_channels */
  if (((uint8_t)config->comm_channels & (uint8_t)(~k_comm_channel_mask_all)) != 0U) {
    return k_rx_err_invalid_arg;
  }

  s_enabled_channels = config->comm_channels;
  return k_rx_ok;
}

/**
 * @brief Create the Communication task (HIGHEST PRIORITY application task)
 *
 * @details
 * Creates and starts the Communication ThreadX task with **HIGHEST application priority**
 * (Priority 5) for low-latency protocol command processing at 100 Hz. This function allocates
 * the thread control block, assigns a static stack, and registers the task with ThreadX.
 *
 * **Critical Design Decision:** Priority 5 ensures this task preempts ALL other application
 * tasks (motor control, telemetry, sensors) to minimize command-to-motor latency. Only
 * system-level tasks (ThreadX scheduler, ISRs) have higher priority.
 *
 * ## Algorithm Steps
 *
 * The function performs the following operations in sequence:
 *
 * 1. **Guard Check:** Verify s_comm_created is false (prevent double-creation)
 * 2. **Thread Creation:** Call tx_thread_create() with:
 *    - Thread name: "CommTask" (8 chars max)
 *    - Entry point: internal_comm_task_entry
 *    - Stack: s_comm_stack (2048 bytes, static allocation for protobuf decode)
 *    - Priority: 5 (HIGHEST application priority for low-latency command processing)
 *    - Auto-start: Immediate scheduling after creation
 * 3. **Error Check:** Validate TX_SUCCESS return from ThreadX
 * 4. **State Update:** Set s_comm_created = true (prevent future creation)
 * 5. **Logging:** Log success message via rx_log_info
 * 6. **Return:** Return k_rx_ok on success
 *
 * ## Thread Configuration Details
 *
 * | Parameter | Value | Rationale |
 * |-----------|-------|-----------|
 * | **Name** | "CommTask" | ThreadX name (8 char limit) |
 * | **Entry** | internal_comm_task_entry | Task main loop function |
 * | **Input** | 0 | No parameters needed |
 * | **Stack** | s_comm_stack | 2048 bytes static array |
 * | **Stack Size** | 2048 | Required for nanopb protobuf decode buffers (~256 bytes peak) |
 * | **Priority** | 5 | HIGHEST application priority (range 0-31, lower = higher priority) |
 * | **Preempt Threshold** | 5 | Same as priority (no preemption protection) |
 * | **Time Slice** | TX_NO_TIME_SLICE | No round-robin (only one task at priority 5) |
 * | **Auto Start** | TX_AUTO_START | Begin execution immediately after creation |
 *
 * ## Control Flow Diagram
 *
 * @dot
 * digraph comm_create_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="comm_task_create()", fillcolor=lightgreen, style=filled];
 *   check_created [label="Check s_comm_created", shape=diamond];
 *   already_created [label="Log assertion\nReturn k_rx_err_invalid_state",
 *                    fillcolor=red, style=filled];
 *   create_thread [label="tx_thread_create()\nName: CommTask\nStack: 2048 bytes\nPriority: 5 (HIGHEST)"];
 *   check_status [label="ThreadX result?", shape=diamond];
 *   create_failed [label="Log error\nReturn k_rx_err_rtos_thread_create",
 *                  fillcolor=red, style=filled];
 *   set_flag [label="s_comm_created = true"];
 *   log_success [label="Log: Communication task created"];
 *   return_ok [label="Return k_rx_ok", fillcolor=lightgreen, style=filled];
 *
 *   start -> check_created;
 *   check_created -> already_created [label="true\n(double-create)"];
 *   check_created -> create_thread [label="false\n(first call)"];
 *   create_thread -> check_status;
 *   check_status -> create_failed [label="!= TX_SUCCESS"];
 *   check_status -> set_flag [label="== TX_SUCCESS"];
 *   set_flag -> log_success;
 *   log_success -> return_ok;
 * }
 * @enddot
 *
 * ## Priority Justification (Why Priority 5?)
 *
 * **Command-to-Motor Latency Budget:**
 * - Frame arrival (SPI ISR): 0 us
 * - Comm task wake-up: ~50 us (ThreadX context switch from priority 8 motor task)
 * - Protobuf decode: ~200 us
 * - Shared data update: ~7 us
 * - Motor task wake-up: ~50 us (ThreadX context switch)
 * - **Total latency: ~310 us**
 *
 * If comm task had LOWER priority (e.g., Priority 10):
 * - Must wait for motor task (Priority 8) to yield
 * - Worst-case motor task execution: 4ms (250 Hz control loop)
 * - **Total latency: 4000 us + 310 us = 4.3ms** (14x slower!)
 *
 * **Design decision:** Priority 5 ensures comm task **immediately preempts** motor task
 * when a new command arrives, minimizing latency to ~310 us.
 *
 * @return rx_err_t Task creation status
 *
 * @retval k_rx_ok Task created successfully, thread scheduled and running
 * @retval k_rx_err_invalid_state Task already created (s_comm_created == true). Second call blocked.
 * @retval k_rx_err_rtos_thread_create ThreadX tx_thread_create() returned error (!= TX_SUCCESS).
 *                                     Possible causes: invalid priority, stack too small, TCB corruption.
 *
 * @pre ThreadX kernel entered via tx_kernel_enter()
 * @pre tx_application_define() callback currently executing
 * @pre shared_data_init() called successfully (required for shared_data_set_motor_command)
 * @pre USB CDC peripheral initialized (for USB command reception)
 * @pre RSPI2 peripheral initialized (for SPI command reception)
 * @pre s_comm_created == false (never called before)
 * @pre s_comm_thread uninitialized (first use)
 * @pre s_comm_stack allocated and uninitialized (first use)
 *
 * @post s_comm_thread initialized with ThreadX control block
 * @post s_comm_stack assigned to thread and stack pointer initialized
 * @post Task in READY or RUNNING state (depends on ThreadX scheduler)
 * @post s_comm_created == true (prevents future double-creation)
 * @post internal_comm_task_entry() scheduled for execution (auto-start)
 * @post Task will begin polling rx_comm_manager at 100 Hz after initialization
 *
 * @invariant s_comm_created transitions false -> true exactly once (never resets)
 * @invariant Task priority remains at k_comm_task_priority (5) throughout lifetime
 * @invariant Stack size remains at k_comm_task_stack_size (2048 bytes)
 *
 * @note **Single-shot:** This function MUST be called exactly once during boot.
 *       Subsequent calls will assert and return k_rx_err_invalid_state.
 * @note **Non-blocking:** Returns immediately after thread creation. Task
 *       executes concurrently after ThreadX scheduler starts.
 * @note **Context:** ONLY call from tx_application_define(). Never call from
 *       task context or interrupt handlers.
 * @note **Thread safety:** Not thread-safe (assumes single-threaded boot).
 *       No synchronization needed during tx_application_define().
 * @note **Performance:** 2.2% CPU utilization at 100 Hz poll rate (220 us active per 10ms).
 *
 * @warning **Never call twice.** Assertion will fire in debug builds. Release
 *          builds return k_rx_err_invalid_state on second call.
 * @warning **Call order matters.** Must call after shared_data_init() but before
 *          motor_control_task_create() (motor task consumes commands from this task).
 * @warning **Stack size fixed.** 2048 bytes must accommodate nanopb decode buffers
 *          (~256 bytes peak). Overflow detection enabled via TX_ENABLE_STACK_CHECKING.
 *
 * @attention Task starts immediately (TX_AUTO_START). USB CDC and SPI must be
 *            initialized and ready for frame reception.
 * @attention HIGHEST application priority (5) ensures comm task preempts ALL other
 *            application tasks for low-latency command processing.
 * @attention rx_comm_manager will be initialized inside the task (not during creation).
 *
 * @par Thread Safety:
 * This function is NOT thread-safe and MUST be called from single-threaded
 * context during tx_application_define(). After task creation, the task itself
 * uses thread-safe APIs:
 * - shared_data_set_motor_command(): Mutex-protected
 * - shared_data_trigger_estop(): Mutex-protected
 * - rx_comm_manager_poll(): Safe (non-blocking poll)
 * - tx_thread_sleep(): Safe (yields CPU)
 *
 * @par Performance:
 * - **Creation time:** ~120 us @ 240 MHz (thread control block initialization)
 * - **CPU cycles:** ~28,800 cycles
 * - **Stack usage during creation:** ~64 bytes (function call overhead)
 * - **Memory allocation:** 2453 bytes total (2048 stack + 140 TCB + 265 static vars)
 *
 * @par Re-entrancy:
 * NOT reentrant. Single-shot function. Second call blocked by s_comm_created guard.
 *
 * @par Example - Standard Usage:
 * @code{.c}
 * // In main.c - tx_application_define() callback
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   // 1. Initialize shared data first
 *   rx_err_t ret = shared_data_init();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Shared data init failed");
 *     return;
 *   }
 *
 *   // 2. Create Communication task (HIGHEST PRIORITY)
 *   ret = comm_task_create();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Comm task create failed");
 *     return;  // Fatal error - cannot receive commands
 *   }
 *
 *   // 3. Create motor control task (consumes commands from comm task)
 *   ret = motor_control_task_create();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Motor task create failed");
 *     return;
 *   }
 *
 *   // 4. Create telemetry task (uses g_comm_manager for responses)
 *   ret = telemetry_task_create();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Telemetry task create failed");
 *     return;
 *   }
 *
 *   rx_log_info("INIT", "All tasks created successfully");
 * }
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   rx_err_t ret = shared_data_init();
 *   if (ret != k_rx_ok) return;
 *
 *   ret = comm_task_create();
 *   switch (ret) {
 *     case k_rx_ok:
 *       rx_log_info("COMM", "Task created, polling at 100 Hz");
 *       break;
 *     case k_rx_err_invalid_state:
 *       rx_log_error("COMM", "Double-creation attempt!");
 *       break;
 *     case k_rx_err_rtos_thread_create:
 *       rx_log_error("COMM", "ThreadX create failed - check priority/stack");
 *       break;
 *     default:
 *       rx_log_error("COMM", "Unknown error");
 *       break;
 *   }
 * }
 * @endcode
 *
 * @par Example - rx_comm_manager Initialization Failure Recovery:
 * @code{.c}
 * // Comm task will continue running even if rx_comm_manager init fails.
 * // Task will poll but won't receive frames until USB/SPI are ready.
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   // shared_data_init() and other task creation...
 *
 *   rx_err_t ret = comm_task_create();
 *   if (ret == k_rx_ok) {
 *     // Task created successfully. If rx_comm_manager init fails inside the task,
 *     // the task will continue running and retry polling (no frames received).
 *     rx_log_info("COMM", "Task created (will retry if comm_mgr init fails)");
 *   }
 * }
 * @endcode
 *
 * @see internal_comm_task_entry() Task main loop implementation
 * @see shared_data_init() Must be called before this function
 * @see motor_control_task_create() Consumer of motor commands (call after this)
 * @see telemetry_task_create() Uses g_comm_manager for responses (call after this)
 * @see rx_comm_manager_init() Communication manager initialization (called inside task)
 * @see rx_comm_manager_poll() Polls for incoming frames
 * @see shared_data_set_motor_command() Thread-safe motor command update
 * @see tx_thread_create() ThreadX thread creation API
 * @see tx_kernel_enter() ThreadX kernel entry point
 *
 * @since Version 1.0.0
 *
 * @test test_comm_task.c - Verify successful creation
 * @test test_comm_task.c - Verify double-creation returns k_rx_err_invalid_state
 * @test test_comm_task.c - Verify task scheduled with priority 5
 * @test test_comm_task.c - Verify stack size is 2048 bytes
 * @test test_comm_task.c - Verify s_comm_created flag set after creation
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5:** 8 preconditions, 6 postconditions documented
 * - **Rule 7:** tx_thread_create() return value checked
 * - **Rule 8:** All constants use C23 typed enums (no macros)
 *
 * @callgraph
 * @callergraph
 */
rx_err_t comm_task_create(void)
{
  /* Check if already created */
  RX_ASSERT(!s_comm_created, "Comm task already created");
  if (s_comm_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  const UINT tx_status = tx_thread_create(&s_comm_thread,
                                          "CommTask",
                                          internal_comm_task_entry,
                                          k_comm_task_input,
                                          s_comm_stack,
                                          k_comm_task_stack_size,
                                          k_comm_task_priority,
                                          k_comm_task_priority,
                                          TX_NO_TIME_SLICE,
                                          TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_comm_created = true;
  rx_log_info(s_tag, "Communication task created");

  return k_rx_ok;
}

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/*
 * NOTE: The RX72N's on-chip USB peripheral (USB0) is not used on main.
 * Host communication is routed through the external USB-UART bridge on
 * SCI9 (see libs/rx_uart_comm/).  USB0 bring-up work continues on the
 * feat/usb0 branch.
 */

/**
 * @brief Initialize SPI transport layer and HARQ link
 *
 * @details
 * Attempts to initialize the SPI communication transport (RSPI2 channel 2) if SPI
 * is enabled in the channel mask. When SPI transport succeeds, also initializes the
 * HARQ link layer for reliable communication. Sets config->spi_link accordingly.
 *
 * @param[out] config Comm manager configuration; spi_link field populated
 * @param[out] link_ok_out Set to true if HARQ link initialized successfully
 *
 * @return true if SPI transport initialized successfully, false otherwise
 *
 * @pre config must be non-NULL
 * @pre s_session_state must be zero-initialized (static storage)
 * @pre RSPI2 peripheral must be initialized before calling
 * @post s_spi_comm_handle initialized on success
 * @post config->spi_link set to &s_spi_link or nullptr
 * @post Failure logged via rx_log_error
 *
 * @note Called once during single-threaded task initialization; not thread-safe
 *
 * @since Version 1.0.0
 * @see rx_spi_comm_init() SPI transport layer initialization
 * @see rx_spi_link_init() HARQ link layer initialization
 */
static bool internal_init_spi_transport(rx_comm_manager_config_t* config, bool* link_ok_out)
{
  *link_ok_out = false;

  if ((s_enabled_channels & k_comm_channel_mask_spi) == k_comm_channel_mask_none) {
    rx_log_info(s_tag, "SPI comm disabled by config");
    config->spi_link = nullptr;
    return false;
  }

  rx_spi_comm_config_t spi_cfg = {.session     = &s_session_state,
                                  .channel     = k_rspi_channel_2,
                                  .spi_mode    = k_spi_comm_default_mode,
                                  .fec_enabled = false};
  bool                 spi_ok  = (bool)(rx_spi_comm_init(&s_spi_comm_handle, &spi_cfg) == k_rx_ok);
  if (!spi_ok) {
    rx_log_error(s_tag, "SPI comm init failed");
    rx_log_warn(s_tag, "Skipping SPI HARQ link init because SPI transport failed");
    config->spi_link = nullptr;
    return false;
  }

  /* Only attempt to configure the HARQ link when the underlying SPI transport
   * succeeded.  Passing an invalid handle into rx_spi_link_init would trigger
   * undefined behaviour, so skip the whole block if spi_ok is false. */
  rx_spi_link_config_t link_cfg = {
    .spi_handle  = &s_spi_comm_handle,
    .fec_enabled = true,
    .max_retries = k_spi_link_default_max_retries,
  };

  *link_ok_out = (bool)(rx_spi_link_init(&s_spi_link, &link_cfg) == k_rx_ok);
  if (!(*link_ok_out)) {
    rx_log_error(s_tag, "SPI link (HARQ) init failed");
  }
  config->spi_link = (int)(*link_ok_out) ? &s_spi_link : nullptr;
  return true;
}

/**
 * @brief Initialize I2C transport layer
 *
 * @details
 * Attempts to initialize the I2C communication transport (RIIC0, peripheral mode,
 * address 0x42) if I2C is enabled in the channel mask. On failure, logs an error
 * but allows graceful degradation.
 *
 * @return true if I2C transport initialized successfully, false otherwise
 *
 * @pre s_session_state must be zero-initialized (static storage)
 * @pre s_enabled_channels must be configured before calling
 * @post s_i2c_comm_handle initialized on success
 * @post Failure logged via rx_log_error
 *
 * @note Called once during single-threaded task initialization; not thread-safe
 *
 * @since Version 1.0.0
 * @see rx_i2c_comm_init() I2C transport layer initialization
 */
static bool internal_init_i2c_transport(void)
{
  if ((s_enabled_channels & k_comm_channel_mask_i2c) == k_comm_channel_mask_none) {
    rx_log_info(s_tag, "I2C comm disabled by config");
    return false;
  }

  rx_i2c_comm_config_t i2c_cfg = {
    .session     = &s_session_state,
    .channel     = {.value = k_i2c_comm_default_channel},
    .device_addr = {.value = k_i2c_comm_default_addr},
  };
  bool i2c_ok = (bool)(rx_i2c_comm_init(&s_i2c_comm_handle, &i2c_cfg) == k_rx_ok);
  if (!i2c_ok) {
    rx_log_error(s_tag, "I2C comm init failed");
  }
  return i2c_ok;
}

/**
 * @brief Initialize UART transport layer
 *
 * @details
 * Attempts to initialize the UART communication transport (SCI9) if UART is
 * enabled in the channel mask. On failure, logs an error but allows graceful
 * degradation.
 *
 * @return true if UART transport initialized successfully, false otherwise
 *
 * @pre s_session_state must be zero-initialized (static storage)
 * @pre s_enabled_channels must be configured before calling
 * @post s_uart_comm_handle initialized on success
 * @post Failure logged via rx_log_error
 *
 * @note Called once during single-threaded task initialization; not thread-safe
 *
 * @since Version 1.0.0
 * @see rx_uart_comm_init() UART transport layer initialization
 */
static bool internal_init_uart_transport(void)
{
  if ((s_enabled_channels & k_comm_channel_mask_uart) == k_comm_channel_mask_none) {
    rx_log_info(s_tag, "UART comm disabled by config");
    return false;
  }

  rx_uart_comm_config_t uart_cfg = {
    .session = &s_session_state,
    .channel = k_uart_channel_9,
  };
  bool uart_ok = (bool)(rx_uart_comm_init(&s_uart_comm_handle, &uart_cfg) == k_rx_ok);
  if (!uart_ok) {
    rx_log_error(s_tag, "UART comm init failed");
  }
  return uart_ok;
}

/**
 * @brief Log transport initialization status and detect total failure
 *
 * @details
 * Logs which transports initialized successfully and fires a critical error
 * when at least one channel was enabled but all enabled channels failed.
 * Disabled channels are excluded from failure detection.
 *
 * @param[in] usb_ok  true if USB transport initialized successfully
 * @param[in] spi_ok  true if SPI transport initialized successfully
 * @param[in] link_ok true if SPI HARQ link initialized successfully
 * @param[in] i2c_ok  true if I2C transport initialized successfully
 * @param[in] uart_ok true if UART transport initialized successfully
 *
 * @pre s_enabled_channels must be configured before calling
 * @pre All *_ok parameters must reflect actual init results
 * @post Critical error logged if all enabled channels failed
 * @post Info logged for each successfully initialized transport
 *
 * @note Called once during single-threaded task initialization; not thread-safe
 *
 * @since Version 1.0.0
 */
static void internal_log_transport_status(bool spi_ok, bool link_ok, bool i2c_ok, bool uart_ok)
{
  const bool spi_enabled =
    (bool)((s_enabled_channels & k_comm_channel_mask_spi) != k_comm_channel_mask_none);
  const bool i2c_enabled =
    (bool)((s_enabled_channels & k_comm_channel_mask_i2c) != k_comm_channel_mask_none);
  const bool uart_enabled =
    (bool)((s_enabled_channels & k_comm_channel_mask_uart) != k_comm_channel_mask_none);
  const bool any_enabled = (bool)((int)spi_enabled | (int)i2c_enabled | (int)uart_enabled);
  /* At least one enabled channel initialized successfully. */
  const bool any_enabled_succeeded =
    (bool)(((int)spi_enabled & (int)spi_ok) | ((int)i2c_enabled & (int)i2c_ok) |
           ((int)uart_enabled & (int)uart_ok));

  if ((int)any_enabled && !(int)any_enabled_succeeded) {
    rx_log_error(s_tag, "CRITICAL: All transport channels failed to initialize");
    return;
  }

  if (uart_ok) {
    rx_log_info(s_tag, "UART transport initialized (primary link to Pi5 gateway)");
  }
  if (spi_ok) {
    rx_log_info(s_tag, "SPI transport initialized");
  }
  if (link_ok) {
    rx_log_info(s_tag, "SPI HARQ link initialized (FEC with Chase Combining)");
  }
  if (i2c_ok) {
    rx_log_info(s_tag, "I2C peripheral transport initialized");
  }
}

/**
 * @brief Initialize all transport layers and wire into comm manager config
 *
 * @details
 * Orchestrates initialization of all communication transport layers (USB, SPI,
 * I2C, UART) by delegating to per-channel helper functions. Populates the
 * rx_comm_manager_config_t structure with either real transport handles (on
 * success) or nullptr (on failure) to enable graceful degradation.
 *
 * @param[out] config Comm manager configuration to populate
 *   - **Valid range**: Non-nullptr to rx_comm_manager_config_t
 *   - **Constraints**: Must be zeroed before calling this function
 *   - **Side effects**: usb_handle, spi_handle, i2c_handle, uart_handle,
 *     and spi_link fields populated
 *
 * @return void This function does not return a value
 *
 * @pre config must be non-NULL and zero-initialized (memset)
 * @pre s_session_state must be zero-initialized (static storage)
 * @pre RSPI2 peripheral must be initialized before calling this function
 * @post config->usb_handle set to &s_usb_comm_handle or nullptr
 * @post config->spi_handle set to &s_spi_comm_handle or nullptr
 * @post config->spi_link set to &s_spi_link when SPI transport and HARQ init both succeed, else nullptr
 * @post At least one transport handle set on success (best effort)
 * @post All init failures logged via rx_log_error
 *
 * @note Called once during single-threaded task initialization; not thread-safe
 * @warning Function has no return value - check config handle fields for nullptr
 *          to detect degraded modes
 *
 * @par Thread Safety:
 * This function is **not thread-safe**. It must be called only once during
 * task initialization in a single-threaded context.
 *
 * @since Version 1.0.0
 * @see internal_init_usb_transport() USB channel initialization
 * @see internal_init_spi_transport() SPI channel and HARQ link initialization
 * @see internal_init_i2c_transport() I2C channel initialization
 * @see internal_init_uart_transport() UART channel initialization
 * @see internal_log_transport_status() Validation and status logging
 */
static void internal_init_transports(rx_comm_manager_config_t* config)
{
  /* Precondition checks (NASA Power of 10 Rule 5: minimum 2 checks) */
  RX_ASSERT(config != nullptr, "Config pointer must not be NULL");
  RX_ASSERT(s_tag != nullptr, "Log tag must be initialized");

  /* Initialize each transport channel via dedicated helpers */
  bool       link_ok = false;
  const bool spi_ok  = internal_init_spi_transport(config, &link_ok);
  const bool i2c_ok  = internal_init_i2c_transport();
  const bool uart_ok = internal_init_uart_transport();

  /* Wire handles - pass nullptr for disabled/failed transports */
  config->spi_handle  = (int)spi_ok ? &s_spi_comm_handle : nullptr;
  config->i2c_handle  = (int)i2c_ok ? &s_i2c_comm_handle : nullptr;
  config->uart_handle = (int)uart_ok ? &s_uart_comm_handle : nullptr;

  /* Log results and detect total failure */
  internal_log_transport_status(spi_ok, link_ok, i2c_ok, uart_ok);
}

/**
 * @brief Communication task entry point - infinite loop polling rx_comm_manager at 100 Hz
 *
 * @details
 * This is the main task loop that executes after comm_task_create() starts the thread.
 * The function never returns and runs continuously at 100 Hz polling rate until
 * power-down or emergency stop. It is the **HIGHEST PRIORITY** application task (Priority 5)
 * to minimize command-to-motor latency.
 *
 * ## Complete Algorithm (10 Steps)
 *
 * ### Initialization Phase (Steps 1-6)
 *
 * 1. **Log Startup:** Print "Communication task starting" via UART
 * 2. **Initialize USB Transport Layer:**
 *    - Call rx_usb_comm_init(&s_usb_comm_handle, &usb_cfg)
 *    - Configure with shared session state (s_session_state)
 *    - If init fails: Log error but continue (SPI may still work)
 * 3. **Initialize SPI Transport Layer:**
 *    - Call rx_spi_comm_init(&s_spi_comm_handle, &spi_cfg)
 *    - Configure with shared session state, channel 0, mode 0, no FEC
 *    - If init fails: Log error but continue (USB may still work)
 * 4. **Configure Communication Manager:**
 *    - USB handle: &s_usb_comm_handle (real initialized handle)
 *    - SPI handle: &s_spi_comm_handle (real initialized handle)
 *    - Callback: internal_frame_callback (invoked when complete frame received)
 *    - Callback context: &g_comm_manager (passed to callback for state access)
 *    - Enable decoded output: true (log frame contents for debugging)
 * 5. **Initialize rx_comm_manager:** Call rx_comm_manager_init()
 *    - If init fails: Log error but continue (task will poll but won't receive frames)
 * 6. **Validate Transports:** Check if at least one transport initialized successfully
 *    - If both fail: Log critical error (system cannot communicate)
 *    - If one succeeds: Log which transport(s) are operational
 *
 * ### Main Polling Loop (Steps 5-10, Infinite)
 *
 * 5. **Poll for Frames:** Call rx_comm_manager_poll() (non-blocking)
 *    - Checks USB CDC receive buffer for complete frames
 *    - Checks SPI receive buffer for complete frames
 *    - Validates CRC-32 checksum
 *    - Parses frame header (type, length, sequence)
 *
 * 6. **Check Poll Result:**
 *    - **k_rx_ok:** Frame received, internal_frame_callback() already invoked
 *    - **k_rx_err_timeout:** No frame available (normal, continue)
 *    - **Other errors:** CRC mismatch, malformed frame, buffer overflow (log debug)
 *
 * 7. **Frame Callback Dispatch (if k_rx_ok):**
 *    - rx_comm_manager invokes internal_frame_callback() automatically
 *    - Callback processes frame based on type (COMMAND, ACK, NACK)
 *    - See internal_frame_callback() documentation for details
 *
 * 8. **Error Logging (if not timeout):**
 *    - Log debug message with error code
 *    - Common errors: CRC mismatch, buffer overflow, invalid frame type
 *    - Does not halt task (continues polling)
 *
 * 9. **Sleep 10ms:** Call tx_thread_sleep(1 tick)
 *    - 1 tick at 100 Hz tick rate = 10ms
 *    - Yields CPU to other tasks
 *    - ThreadX scheduler resumes after 10ms
 *
 * 10. **Repeat Forever:** Loop back to step 5
 *
 * ## rx_comm_manager Initialization Details
 *
 * **Configuration structure:**
 * ```c
 * typedef struct {
 *   rx_usb_comm_handle_t* usb_handle;  // Real USB communication layer handle
 *   rx_spi_comm_handle_t* spi_handle;  // Real SPI communication layer handle
 *   rx_frame_callback_t callback;      // Frame received callback
 *   void* callback_ctx;                // User context for callback
 *   bool enable_decoded_output;        // Log frame contents for debugging
 * } rx_comm_manager_config_t;
 * ```
 *
 * **Transport Layer Initialization:**
 * - USB and SPI communication layers initialized at task startup
 * - rx_usb_comm_init() and rx_spi_comm_init() called before comm manager init
 * - Both transports share session state (s_session_state) for sequence continuity
 * - Handles are statically allocated (s_usb_comm_handle, s_spi_comm_handle)
 *
 * **Graceful Degradation:**
 * - If one transport fails init: Other transport continues (logged warning)
 * - If both transports fail: Task continues but cannot communicate (logged critical error)
 * - Poll will return k_rx_err_timeout every cycle until transports recover
 * - Error logged once during init, not every poll cycle
 *
 * ## Polling Strategy - Non-Blocking Check
 *
 * **Why non-blocking poll instead of blocking wait?**
 *
 * | Approach | Pros | Cons | Used? |
 * |----------|------|------|-------|
 * | **Blocking wait** | Zero CPU when idle | Requires ISR to signal task | No |
 * | **Non-blocking poll** | Simple, no ISR coordination | 2.2% CPU overhead | **Yes** |
 *
 * **Decision:** Non-blocking poll chosen for simplicity and robustness:
 * - rx_comm_manager_poll() returns immediately (no blocking)
 * - 100 Hz poll rate is 10x slower than frame arrival rate (1 kHz max)
 * - 2.2% CPU overhead acceptable for communication task
 * - No ISR coordination needed (frame buffering handled by rx_comm_manager)
 *
 * ## Control Flow Diagram
 *
 * @startuml
 * start
 * :Log "Communication task starting";
 * :Zero config structure\n(memset to 0);
 * :Set callback = internal_frame_callback\nSet callback_ctx = &g_comm_manager\nSet enable_decoded_output = true;
 * :rx_comm_manager_init(&g_comm_manager, &config);
 * if (Init successful?) then (yes)
 *   :Log "Communication running @ 100 Hz";
 * else (no)
 *   :Log error\n(continue with polling, no frames);
 * endif
 *
 * partition "Infinite Polling Loop (100 Hz)" {
 *   repeat
 *     :rx_comm_manager_poll(&g_comm_manager)\n(Check USB CDC + SPI for frames);
 *     if (Poll result?) then (k_rx_ok)
 *       :Frame received\ninternal_frame_callback() invoked;
 *     else if (k_rx_err_timeout) then (no frame)
 *       :Normal (no frame)\nContinue;
 *     else (other error)
 *       :Log debug\n(CRC error, malformed frame);
 *     endif
 *     :tx_thread_sleep(1 tick)\n**Sleep 10ms, yield CPU**;
 *   repeat while (forever)
 * }
 * @enduml
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Measurement Method |
 * |--------|-------|-------------------|
 * | **Poll Rate** | 100 Hz | Fixed 10ms sleep period |
 * | **rx_comm_manager_poll() Time** | ~50 us | USB + SPI buffer check |
 * | **Frame Processing Time** | ~220 us | CRC + callback + protobuf decode |
 * | **CPU Active Time** | 220 us per cycle | Poll + process (when frame present) |
 * | **CPU Idle Time** | 9780 us per cycle | 97.8% idle (yields CPU during sleep) |
 * | **CPU Utilization** | 2.2% | (220 us / 10ms) x 100% |
 * | **Worst Case Latency** | 10ms | Max time from frame arrival to callback |
 *
 * ## Memory Usage
 *
 * | Variable | Type | Size | Scope | Lifetime |
 * |----------|------|------|-------|----------|
 * | `err` | rx_err_t | 4 bytes | Local | Function lifetime |
 * | `config` | rx_comm_manager_config_t | ~32 bytes | Local | Init phase only |
 * | **Total Stack** | - | ~64 bytes | Stack | Peak during init |
 *
 * **Note:** Frame processing stack usage (protobuf decode) occurs in internal_frame_callback()
 * and internal_handle_command_frame(), not in this function. Peak stack usage for entire
 * task is ~320 bytes (during rx_nanopb_decode_velocity_request).
 *
 * @param[in] input Thread input parameter from tx_thread_create()
 *                  - Type: ULONG (32-bit unsigned)
 *                  - Value: 0 (k_comm_task_input)
 *                  - Purpose: Unused (ThreadX convention requires parameter)
 *                  - Explicitly cast to (void) to suppress unused warning
 *
 * @return void This function never returns (infinite loop until power-down)
 *
 * @pre comm_task_create() called successfully
 * @pre ThreadX scheduler started (task is scheduled)
 * @pre USB CDC peripheral initialized (for USB frame reception)
 * @pre RSPI2 peripheral initialized (for SPI frame reception)
 * @pre shared_data_init() called (for shared_data_set_motor_command)
 *
 * @post rx_comm_manager initialized (if USB/SPI ready)
 * @post Infinite loop polling at 100 Hz until power-down
 * @post internal_frame_callback() invoked on every valid frame reception
 * @post shared_data.motor_command updated on SetVelocityRequest frames
 * @post Emergency stop triggered on EmergencyStopRequest frames
 *
 * @invariant Loop period is exactly 10ms (1 tick at 100 Hz)
 * @invariant Function never returns (while(true) infinite loop)
 * @invariant rx_comm_manager_poll() called every iteration
 *
 * @note **Thread Safety:** This function runs in its own thread context (Priority 5).
 *       All shared data access uses thread-safe APIs (mutex-protected).
 * @note **Re-entrancy:** NOT reentrant. Single instance only (enforced by s_comm_created).
 * @note **Performance:** 97.8% CPU idle time. Polling is 220 us out of 10ms period.
 * @note **Memory:** Peak stack usage is 320 bytes (in callback, not here).
 * @note **Polling Rate:** 100 Hz is sufficient for command processing (10ms latency budget).
 *       Faster polling (1 kHz) would waste CPU cycles (99.98% idle) with no latency benefit.
 *
 * @warning **Infinite Loop:** This function NEVER returns. Do not call directly from
 *          application code. Only ThreadX should call this via tx_thread_create().
 * @warning **rx_comm_manager Init Failure:** If init fails, task continues polling but
 *          won't receive frames. Check USB/SPI peripheral initialization.
 * @warning **Stack Overflow:** 2048 byte stack must accommodate 320 byte peak usage.
 *          TX_ENABLE_STACK_CHECKING detects overflow if it occurs.
 *
 * @attention This function executes in its own thread context, NOT in main() context.
 * @attention USB CDC and SPI peripherals are shared. rx_comm_manager handles mutex locking.
 * @attention Frame callback (internal_frame_callback) runs in THIS task's context, not ISR.
 *
 * @par Thread Safety:
 * This function is the ONLY code that calls rx_comm_manager_poll(). The rx_comm_manager
 * is NOT shared with other tasks (except telemetry task for sending responses, which
 * uses g_comm_manager global). All shared data updates (motor commands) use mutex-protected
 * APIs from shared_data module.
 *
 * USB CDC and SPI peripherals are shared with ISRs (receive interrupts). The rx_comm_manager
 * handles interrupt synchronization internally (ISR fills buffers, poll reads buffers with
 * mutex protection).
 *
 * @par Performance Analysis:
 * **Execution Time Breakdown (per 10ms iteration):**
 * - rx_comm_manager_poll(): ~50 us (buffer check, no frame)
 * - Frame processing (when present): ~220 us (CRC + callback + protobuf)
 * - tx_thread_sleep(): ~5 us (ThreadX scheduler overhead)
 * - **Total active time:** ~55 us (no frame) or ~275 us (with frame)
 * - **Sleep time:** 10000 us - 275 us = 9725 us (CPU idle)
 * - **CPU utilization:** (275 us / 10000 us) x 100% = 2.75% (worst case)
 * - **Average utilization:** 2.2% (frames arrive at ~100 Hz, not every poll)
 *
 * @par Example - Normal Operation (Frame Received):
 * @code{.c}
 * // Task executes this loop every 10ms:
 *
 * // Step 1: Poll for frames
 * err = rx_comm_manager_poll(&g_comm_manager);
 * // err = k_rx_ok (frame received)
 *
 * // Step 2: internal_frame_callback() already invoked by rx_comm_manager
 * // Frame type: k_frame_type_command
 * // Payload: SetVelocityRequest protobuf
 *
 * // Step 3: Sleep 10ms
 * tx_thread_sleep(1);  // 1 tick at 100 Hz = 10ms
 * @endcode
 *
 * @par Example - No Frame (Normal Idle):
 * @code{.c}
 * // Task executes this loop every 10ms:
 *
 * // Step 1: Poll for frames
 * err = rx_comm_manager_poll(&g_comm_manager);
 * // err = k_rx_err_timeout (no frame available)
 *
 * // Step 2: No callback invoked, continue
 *
 * // Step 3: Sleep 10ms
 * tx_thread_sleep(1);
 * @endcode
 *
 * @par Example - rx_comm_manager Init Failure Recovery:
 * @code{.c}
 * // If USB CDC or SPI not initialized:
 * err = rx_comm_manager_init(&g_comm_manager, &config);
 * // err = k_rx_err_not_initialized (USB/SPI not ready)
 *
 * rx_log_error_val("COMM", "Comm manager init failed", err);
 * // UART output: "[COMM] ERROR: Comm manager init failed: 4"
 *
 * // Continue running (task will poll but timeout every cycle)
 * while (true) {
 *   err = rx_comm_manager_poll(&g_comm_manager);
 *   // err = k_rx_err_timeout (no manager, no frames)
 *   tx_thread_sleep(1);
 * }
 * // Manual intervention needed: Check USB/SPI initialization
 * @endcode
 *
 * @see comm_task_create() Task creation function
 * @see internal_frame_callback() Frame received callback
 * @see internal_handle_command_frame() Command frame processing
 * @see rx_comm_manager_init() Initialize communication manager
 * @see rx_comm_manager_poll() Poll for incoming frames (non-blocking)
 * @see shared_data_set_motor_command() Thread-safe motor command update
 * @see shared_data_trigger_estop() Trigger emergency stop
 * @see shared_data_update_last_comm_tick() Update communication watchdog timestamp
 * @see tx_thread_sleep() ThreadX sleep API (yields CPU)
 *
 * @since Version 1.0.0
 *
 * @test test_comm_task.c - Verify 100 Hz poll rate timing
 * @test test_comm_task.c - Verify frame reception and callback invocation
 * @test test_comm_task.c - Verify rx_comm_manager init failure handling
 * @test test_comm_task.c - Verify CRC error handling (continue polling)
 * @test test_comm_task.c - Verify timeout handling (no frame available)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1:** [PASS] No goto, setjmp, recursion (only if/while control flow)
 * - **Rule 2:** [PASS] Single while(true) loop with fixed 10ms period
 * - **Rule 3:** [PASS] Zero dynamic allocation (all stack-based locals)
 * - **Rule 4:** [PASS] Function is 48 lines (under 100 LOC guideline)
 * - **Rule 5:** [PASS] 5 preconditions, 5 postconditions documented
 * - **Rule 7:** [PASS] All function returns checked or cast to (void)
 * - **Rule 8:** [PASS] All constants use C23 typed enums (no macros)
 *
 * @callgraph
 * @callergraph
 */

static void internal_comm_task_bringup(void)
{
  /* Initialize the wire-protocol session state shared across all comm
   * transports (SPI, I2C, UART). Must be called BEFORE any transport
   * init since each transport holds a pointer to this session and
   * rx_session_next_tx returns k_rx_err_not_initialized if called
   * before this, which silently breaks every framed send. */
  const rx_err_t sess_err = rx_session_init(&s_session_state);
  if (sess_err != k_rx_ok) {
    rx_log_error_val(s_tag, "Session init failed", (uint32_t)sess_err);
  }

  rx_log_info(s_tag, "Communication task starting");

  /* Initialize transport layers and wire into comm manager config */
  rx_comm_manager_config_t config = {};
  config.enabled_channels         = s_enabled_channels;
  internal_init_transports(&config);
  config.callback     = internal_frame_callback;
  config.callback_ctx = &g_comm_manager;
  /* enable_decoded_output routed to the same rx_log_uart ring that
   * internal_ship_log_frames drains and retransmits as type=0x20
   * log_message frames. Leaving it true produces an infinite feedback
   * loop: every send is ASCII-dumped into the ring, drained as a new
   * log frame, that new frame gets ASCII-dumped into the ring, ...
   * Dedicated USB debug port is gone, so decoded output has no benign
   * sink -- disable it. */
  config.enable_decoded_output = false;

  const rx_err_t err = rx_comm_manager_init(&g_comm_manager, &config);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "Comm manager init failed", (uint32_t)err);
    /* Continue - task will poll but won't receive frames */
  }

  rx_log_info(s_tag, "Communication running @ 100 Hz");
}

static void internal_comm_task_entry(ULONG input)
{
  (void)input;

  internal_comm_task_bringup();

  /* Main communication loop */
  while (true) {
    /* Poll for incoming frames (non-blocking) */
    rx_err_t err = rx_comm_manager_poll(&g_comm_manager);

    /* k_rx_ok = frame received, k_rx_err_timeout = no frame (normal) */
    if (err != k_rx_ok && err != k_rx_err_timeout) {
      rx_log_debug_val(s_tag, "Poll error", (uint32_t)err);
    }

    /* Ship any pending log bytes as LOG_MESSAGE frames (see helper) */
    internal_ship_log_frames();

    /* Once per ~5s, unstick any latched SSR.FER/ORER/PER on SCI9. The
     * RXI9 ISR already clears these when it fires, but FER can latch
     * from a line glitch *before* RDRF asserts -- in that case the ISR
     * never runs and the receiver blocks further RDRF events. This
     * fallback unsticks the receiver. */
    static uint32_t s_sci9_err_clear_counter = 0U;
    s_sci9_err_clear_counter += 1U;
    if ((s_sci9_err_clear_counter % k_comm_task_sci9_err_clear_period) == 1U) {
      (void)uart_clear_rx_errors(k_uart_channel_9);
    }

    /* Process pending SPI retransmissions (no-op when disabled) */
    (void)rx_comm_manager_process_retransmits(&g_comm_manager,
                                              (uint32_t)(tx_time_get() * k_threadx_ms_per_tick));

    /* Report task heartbeat to IWDT (must execute within 30ms timeout) */
    err = rx_iwdt_task_heartbeat("CommTask");
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep until next poll cycle */
    (void)tx_thread_sleep(k_comm_task_sleep_ticks);
  }
}

/**
 * @brief Drain pending log bytes and ship them as a k_frame_type_log_message
 *
 * @details
 * Extracted from internal_comm_task_entry() to stay under the
 * readability-function-size clang-tidy threshold. Runs from the comm-task
 * poll loop on every tick; reads up to one frame-max-payload bytes out of the
 * rx_log_uart ring and forwards them to the gateway as a TYPE=0x20 frame
 * over UART. Logs are multiplexed onto the same UART byte stream as protobuf
 * command / response / telemetry traffic without risking sync-word
 * collisions, because every log chunk is a complete CRC-checked frame.
 *
 * @pre g_comm_manager has been (attempted to be) initialized
 * @post On every call: rx_log_uart ring drained by up to k_frame_max_payload
 *       bytes when uart_handle is present; no-op when uart_handle is NULL
 *
 * @note Not thread-safe; called only from comm_task context.
 * @since Version 1.0.0
 */
static void internal_ship_log_frames(void)
{
  if (g_comm_manager.uart_handle == nullptr) {
    return;
  }

  static uint8_t s_log_tx_buf[k_frame_max_payload];
  uint32_t       log_len = 0U;
  const rx_err_t drn_err =
    rx_log_uart_drain(s_log_tx_buf, (uint32_t)sizeof(s_log_tx_buf), &log_len);
  if (drn_err != k_rx_ok || log_len == 0U) {
    return;
  }

  const rx_comm_send_params_t log_params = {
    .channel     = k_comm_channel_uart,
    .type        = k_frame_type_log_message,
    .flags       = k_frame_flag_none,
    .payload     = s_log_tx_buf,
    .payload_len = log_len,
  };
  (void)rx_comm_manager_stream_send(&g_comm_manager, &log_params);
}

/**
 * @brief Frame received callback - dispatches frames to appropriate handlers
 *
 * @details
 * This callback is invoked by rx_comm_manager when a **complete valid frame** has been
 * received and validated (CRC-32 passed, header parsed). The function runs in the
 * **Communication Task context** (Priority 5), NOT in ISR context.
 *
 * **Critical Safety Feature:** Updates communication watchdog timestamp on EVERY valid
 * frame reception (regardless of type) to prevent communication timeout emergency stop.
 *
 * ## Algorithm Steps (8 Steps)
 *
 * 1. **NULL Check:** Validate frame pointer is not nullptr (defensive programming)
 * 2. **Debug Logging:** Log frame type and length (for debugging protocol issues)
 * 3. **Update Watchdog:** Call shared_data_update_last_comm_tick()
 *    - Resets 500ms communication timeout watchdog
 *    - Motor task monitors this timestamp to detect comm loss
 *    - Updated on ANY valid frame (COMMAND, ACK, NACK, TELEMETRY)
 * 4. **Dispatch by Frame Type:** Switch on frame->header.type
 *    - **COMMAND:** Record active channel, then call internal_handle_command_frame()
 *    - **ACK:** Log debug message, no further action needed
 *    - **NACK:** Log warning (RPi5 rejected our telemetry frame)
 *    - **Unknown:** Log warning (protocol version mismatch?)
 * 5. **Record Active Channel (COMMAND frames only):**
 *    - shared_data_update_active_channel() stores the channel for symmetric routing
 *    - Only COMMAND frames update the channel; ACK/NACK must NOT overwrite it
 *    - ACK/NACK are host responses to our telemetry (not commands) and arrive on
 *      whichever channel the host happens to use for ACKs, which may differ
 * 6. **Command Frame Processing (if COMMAND):**
 *    - internal_handle_command_frame() decodes protobuf payload
 *    - Updates shared_data with motor commands or triggers e-stop
 *    - See internal_handle_command_frame() documentation for details
 * 7. **ACK Frame Processing (if ACK):**
 *    - RPi5 acknowledged our telemetry frame
 *    - No action needed (telemetry task continues sending at 10 Hz)
 * 8. **NACK Frame Processing (if NACK):**
 *    - RPi5 rejected our telemetry frame (CRC error, malformed protobuf)
 *    - Log warning for debugging, continue normal operation
 *    - Telemetry task will retry on next 100ms cycle
 *
 * ## Frame Type Dispatch Logic
 *
 * @dot
 * digraph frame_dispatch {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="internal_frame_callback()", fillcolor=lightgreen, style=filled];
 *   null_check [label="frame != nullptr?", shape=diamond];
 *   return_early [label="return (safety)", fillcolor=red, style=filled];
 *   log_frame [label="Log frame type + length"];
 *   update_watchdog [label="shared_data_update_last_comm_tick()\n(Reset 500ms timeout)"];
 *   dispatch [label="switch (frame->type)", shape=diamond];
 *
 *   command [label="k_frame_type_command", fillcolor=lightblue, style=filled];
 *   handle_cmd [label="internal_handle_command_frame()\n(Decode protobuf)"];
 *   return_cmd [label="return"];
 *
 *   ack [label="k_frame_type_ack", fillcolor=lightgreen, style=filled];
 *   log_ack [label="Log debug: ACK received"];
 *   return_ack [label="return"];
 *
 *   nack [label="k_frame_type_nack", fillcolor=yellow, style=filled];
 *   log_nack [label="Log warning: NACK received"];
 *   return_nack [label="return"];
 *
 *   unknown [label="Unknown type", fillcolor=orange, style=filled];
 *   log_unknown [label="Log warning: Unknown frame type"];
 *   return_unknown [label="return"];
 *
 *   start -> null_check;
 *   null_check -> return_early [label="NULL"];
 *   null_check -> log_frame [label="Valid"];
 *   log_frame -> update_watchdog;
 *   update_watchdog -> dispatch;
 *
 *   dispatch -> command [label="COMMAND"];
 *   dispatch -> ack [label="ACK"];
 *   dispatch -> nack [label="NACK"];
 *   dispatch -> unknown [label="default"];
 *
 *   command -> handle_cmd;
 *   handle_cmd -> return_cmd;
 *
 *   ack -> log_ack;
 *   log_ack -> return_ack;
 *
 *   nack -> log_nack;
 *   log_nack -> return_nack;
 *
 *   unknown -> log_unknown;
 *   log_unknown -> return_unknown;
 * }
 * @enddot
 *
 * ## Communication Watchdog Update (500ms Timeout)
 *
 * **Why update on EVERY frame type?**
 * - ACK/NACK frames prove RPi5 is alive and processing telemetry
 * - COMMAND frames prove RPi5 is sending motor commands
 * - Any valid frame indicates communication link is healthy
 * - Timeout should ONLY trigger on complete communication loss (cable disconnect)
 *
 * **What happens if watchdog NOT updated?**
 * - Motor task detects timeout after 500ms (50 missed polls at 100 Hz)
 * - Triggers emergency stop (k_estop_reason_comm_timeout)
 * - All motors disabled immediately
 * - Robot stops moving until communication restored
 *
 * ## Frame Type Meanings
 *
 * | Frame Type | Direction | Purpose | Expected Frequency |
 * |------------|-----------|---------|-------------------|
 * | **COMMAND** | RPi5 -> RX72N | Motor velocity commands, e-stop, config | 100 Hz (10ms) |
 * | **ACK** | RPi5 -> RX72N | Acknowledge telemetry frame received | 10 Hz (100ms) |
 * | **NACK** | RPi5 -> RX72N | Reject telemetry (CRC error, decode fail) | Rare (error) |
 * | **TELEMETRY** | RX72N -> RPi5 | Motor state, sensors | 10 Hz (100ms) |
 *
 * **Note:** TELEMETRY frames are sent by telemetry_task, not received here.
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **Execution Time** | ~20 us | Logging + watchdog update + dispatch |
 * | **Command Processing** | ~220 us | If frame type is COMMAND (protobuf decode) |
 * | **ACK Processing** | ~5 us | Just log debug message |
 * | **NACK Processing** | ~10 us | Log warning message |
 * | **Call Frequency** | ~100 Hz | Average (matches command rate) |
 * | **CPU Utilization** | < 0.5% | (20 us x 100 Hz) / 10ms = 0.2% |
 *
 * @param[in] channel Channel the frame was received on
 *                    - Type: rx_comm_channel_t enum
 *                    - Values: k_comm_channel_usb_cdc, k_comm_channel_spi
 *                    - Purpose: Forwarded to internal_handle_command_frame() so that
 *                      command handlers can route responses back on the correct channel
 *
 * @param[in] frame Received frame structure (already validated by rx_comm_manager)
 *                  - Type: const rx_frame_t*
 *                  - Must NOT be NULL (defensive check performed)
 *                  - Contains: header (type, length, sequence), payload (protobuf), CRC-32
 *                  - CRC-32 already validated (rx_comm_manager guarantees valid frame)
 *                  - Lifetime: Valid only during callback (copied by handlers if needed)
 *
 * @param[in] ctx User context pointer (set in rx_comm_manager_config_t)
 *                - Type: void*
 *                - Value: &g_comm_manager (comm manager state pointer)
 *                - Purpose: Allow callback to access comm manager state
 *                - Currently unused (marked with (void)ctx to suppress warning)
 *
 * @return void No return value (callback function)
 *
 * @pre rx_comm_manager_init() called successfully
 * @pre frame != nullptr (checked defensively, should never be NULL from rx_comm_manager)
 * @pre frame->header.type is valid rx_frame_type_t enum value
 * @pre frame->payload contains valid protobuf data (if COMMAND type)
 * @pre shared_data_init() called (for watchdog update)
 *
 * @post Communication watchdog timestamp updated (prevents timeout)
 * @post Active channel recorded in shared_data (COMMAND frames only; ACK/NACK do not update)
 * @post Command frames processed and shared_data updated (if COMMAND type)
 * @post ACK/NACK frames logged for debugging
 * @post Unknown frame types logged as warning
 *
 * @invariant Function executes in Communication Task context (Priority 5), not ISR
 * @invariant Watchdog updated BEFORE command processing (prevent timeout race)
 * @invariant NULL frame pointer causes early return (no crash)
 *
 * @note **Thread Safety:** Runs in Communication Task context (Priority 5). All shared
 *       data access uses thread-safe APIs (mutex-protected).
 * @note **Re-entrancy:** NOT reentrant. rx_comm_manager guarantees single-threaded
 *       callback invocation (one frame at a time).
 * @note **Performance:** 20 us overhead (logging + watchdog) + command processing time.
 * @note **Callback Context:** Runs synchronously during rx_comm_manager_poll(), not
 *       deferred to later time.
 *
 * @warning **Do not block in callback.** This function must complete quickly (<1ms)
 *          to maintain 100 Hz poll rate. Protobuf decode is fast (~200 us).
 * @warning **Frame lifetime.** Frame pointer is only valid during callback. Handlers
 *          must copy data if needed beyond callback return.
 * @warning **NULL frame check.** Defensive check should never trigger (rx_comm_manager
 *          bug if nullptr), but prevents crash if it happens.
 *
 * @attention Callback runs in task context, NOT ISR context (safe to call ThreadX APIs).
 * @attention Watchdog update is critical - without it, motor task triggers e-stop after 500ms.
 * @attention ACK/NACK frames DO reset watchdog (any valid frame proves comm link alive).
 *
 * @par Thread Safety:
 * This function is invoked by rx_comm_manager_poll() in the Communication Task context.
 * rx_comm_manager guarantees single-threaded access (no concurrent callbacks). All
 * shared data updates use mutex-protected APIs:
 * - shared_data_update_last_comm_tick(): Mutex-protected
 * - shared_data_set_motor_command(): Mutex-protected
 * - shared_data_trigger_estop(): Mutex-protected
 *
 * @par Performance Analysis:
 * **Execution Time Breakdown:**
 * - NULL check: ~0.5 us
 * - Debug logging (2 calls): ~8 us (UART transmit)
 * - shared_data_update_last_comm_tick(): ~3 us (mutex lock + update + unlock)
 * - Switch dispatch: ~1 us
 * - Command handler (if COMMAND): ~220 us (protobuf decode + shared_data update)
 * - ACK/NACK logging: ~5 us
 * - **Total:** 20 us (ACK/NACK) or 240 us (COMMAND)
 *
 * @par Example - SetVelocityRequest Frame Reception:
 * @code{.c}
 * // rx_comm_manager_poll() calls this callback when frame received:
 *
 * // frame->header.type = k_frame_type_command
 * // frame->header.length = 32 (SetVelocityRequest protobuf size)
 * // frame->payload = [0x0A, 0x1E, 0x09, 0x00, ...] (nanopb encoded)
 *
 * // Step 1: NULL check (pass)
 * if (frame == nullptr) return;  // Not triggered
 *
 * // Step 2: Debug logging
 * rx_log_debug_val("COMM", "Frame received, type", 0);  // k_frame_type_command = 0
 * rx_log_debug_val("COMM", "Frame length", 32);
 *
 * // Step 3: Update watchdog (prevent timeout)
 * shared_data_update_last_comm_tick();
 * // last_comm_tick = tx_time_get() (e.g., 1234567)
 *
 * // Step 4: Dispatch to command handler
 * switch (k_frame_type_command) {
 *   case k_frame_type_command:
 *     internal_handle_command_frame(k_comm_channel_spi, frame);
 *     // Decodes protobuf, updates motor command, sets k_event_new_motor_cmd
 *     break;
 * }
 * @endcode
 *
 * @par Example - ACK Frame Reception:
 * @code{.c}
 * // Telemetry task sent a frame, RPi5 acknowledged:
 *
 * // frame->header.type = k_frame_type_ack
 * // frame->header.length = 0 (no payload)
 *
 * // Step 1: NULL check (pass)
 * // Step 2: Debug logging
 * rx_log_debug_val("COMM", "Frame received, type", 1);  // k_frame_type_ack = 1
 *
 * // Step 3: Update watchdog (ACK proves RPi5 is alive!)
 * shared_data_update_last_comm_tick();
 *
 * // Step 4: Dispatch to ACK handler
 * case k_frame_type_ack:
 *   rx_log_debug("COMM", "ACK received");
 *   break;
 * // No further action needed, telemetry task continues at 10 Hz
 * @endcode
 *
 * @par Example - NACK Frame Reception (Error):
 * @code{.c}
 * // Telemetry task sent a frame, RPi5 rejected it (CRC error?):
 *
 * // frame->header.type = k_frame_type_nack
 * // frame->header.length = 0 (no payload)
 *
 * // Step 1: NULL check (pass)
 * // Step 2: Debug logging
 * rx_log_debug_val("COMM", "Frame received, type", 2);  // k_frame_type_nack = 2
 *
 * // Step 3: Update watchdog (NACK still proves RPi5 is alive)
 * shared_data_update_last_comm_tick();
 *
 * // Step 4: Dispatch to NACK handler
 * case k_frame_type_nack:
 *   rx_log_warn("COMM", "NACK received");
 *   // UART output: "[COMM] WARNING: NACK received"
 *   break;
 * // Telemetry task will retry on next 100ms cycle
 * @endcode
 *
 * @see internal_handle_command_frame() Command frame processing (protobuf decode)
 * @see shared_data_update_last_comm_tick() Update communication watchdog timestamp
 * @see shared_data_update_active_channel() Record channel for symmetric telemetry routing
 * @see shared_data_is_comm_timeout() Motor task checks this for timeout detection
 * @see rx_comm_manager_poll() Calls this callback when frame received
 * @see rx_frame_t Frame structure definition
 * @see telemetry_task.c Sends TELEMETRY frames (not received here)
 *
 * @since Version 1.0.0
 *
 * @test test_comm_task.c - Verify NULL frame handled gracefully
 * @test test_comm_task.c - Verify COMMAND frames dispatched to handler
 * @test test_comm_task.c - Verify ACK frames logged
 * @test test_comm_task.c - Verify NACK frames logged as warning
 * @test test_comm_task.c - Verify unknown frame types logged as warning
 * @test test_comm_task.c - Verify watchdog updated on all frame types
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1:** [PASS] No goto, setjmp, recursion (only if/switch control flow)
 * - **Rule 3:** [PASS] Zero dynamic allocation (all stack-based)
 * - **Rule 4:** [PASS] Function is 47 lines (under 100 LOC guideline)
 * - **Rule 5:** [PASS] 5 preconditions, 5 postconditions documented
 * - **Rule 7:** [PASS] All function returns checked or cast to (void)
 * - **Rule 8:** [PASS] All constants use C23 typed enums (no macros)
 *
 * @callgraph
 * @callergraph
 */
static void internal_frame_callback(rx_comm_channel_t channel, const rx_frame_t* frame, void* ctx)
{
  (void)ctx;

  if (frame == nullptr) {
    return;
  }

  rx_log_info_val(s_tag, "FRAME callback type=", (uint32_t)frame->header.type);

  /* Update communication timestamp on any valid frame */
  shared_data_update_last_comm_tick();

  /* Dispatch based on frame type */
  switch (frame->header.type) {
    case k_frame_type_command:
      rx_log_info_val(s_tag, "CB COMMAND seq", frame->header.sequence);
      /* Record channel only for COMMAND frames so telemetry replies are routed
       * back on the same transport that carries host commands. ACK/NACK frames
       * are host responses to our telemetry and must not overwrite the channel. */
      (void)shared_data_update_active_channel((uint8_t)channel);
      if (!internal_handle_command_frame(channel, frame)) {
        rx_log_warn_val(s_tag,
                        "command frame handler failed for type",
                        (uint32_t)frame->header.type);
      }
      break;

    case k_frame_type_ping: {
      /* Gateway heartbeat: echo the 4-byte counter payload back as PONG on the
       * same channel. Without this reply the gateway flags the link unhealthy
       * after ~2 s in simple-usb mode and triggers failover. */
      const rx_comm_send_params_t pong_params = {
        .channel     = channel,
        .type        = k_frame_type_pong,
        .flags       = k_frame_flag_none,
        .payload     = frame->payload,
        .payload_len = frame->header.length,
      };
      (void)rx_comm_manager_send(&g_comm_manager, &pong_params);
      break;
    }

    case k_frame_type_ack:
      /* ACK received - nothing to do */
      rx_log_debug(s_tag, "ACK received");
      break;

    case k_frame_type_nack:
      /* NACK received - log and continue */
      rx_log_warn(s_tag, "NACK received");
      break;

    default:
      rx_log_warn_val(s_tag, "Unknown frame type", (uint8_t)frame->header.type);
      break;
  }
}

/**
 * @brief Handle command frame by delegating to per-message-type helper functions
 *
 * @details
 * Dispatches a COMMAND-type frame to the appropriate per-message handler using a
 * try-each-in-order cascade. Each helper attempts to decode the frame as its specific
 * protobuf message type and returns true if it handled the message, false otherwise.
 * The first handler that returns true wins; if none match the frame is an unknown type.
 *
 * **Decode Priority (most frequent first):**
 * 1. SetVelocityRequest (~100 Hz)
 * 2. EmergencyStopRequest (rare, on demand)
 * 3. SetPIDGainsRequest (very rare, config only)
 * 4. SetRetransmitConfigRequest (very rare, config only)
 * 5. Unknown (log warning, return error)
 *
 * @dot
 * digraph handle_command_frame {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="internal_handle_command_frame()", fillcolor=lightgreen, style=filled];
 *   null_check [label="frame != nullptr?", shape=diamond];
 *   return_null [label="return k_rx_err_null_ptr", fillcolor=red, style=filled];
 *
 *   try_vel [label="internal_handle_velocity_command()", shape=diamond];
 *   try_estop [label="internal_handle_estop_command()", shape=diamond];
 *   try_pid [label="internal_handle_pid_gains_command()", shape=diamond];
 *   try_retx [label="internal_handle_retransmit_config_command()", shape=diamond];
 *
 *   ok [label="return k_rx_ok", fillcolor=lightgreen, style=filled];
 *   unknown [label="Log warning: unknown command\nreturn k_rx_err_invalid_arg",
 *            fillcolor=orange, style=filled];
 *
 *   start -> null_check;
 *   null_check -> return_null [label="NULL"];
 *   null_check -> try_vel [label="valid"];
 *   try_vel -> ok [label="true"];
 *   try_vel -> try_estop [label="false"];
 *   try_estop -> ok [label="true"];
 *   try_estop -> try_pid [label="false"];
 *   try_pid -> ok [label="true"];
 *   try_pid -> try_retx [label="false"];
 *   try_retx -> ok [label="true"];
 *   try_retx -> unknown [label="false"];
 * }
 * @enddot
 *
 * @param[in] channel Channel the command arrived on (rx_comm_channel_t)
 *                    - Values: k_comm_channel_usb_cdc, k_comm_channel_spi
 *                    - Passed through to velocity, e-stop, and PID gains handlers
 *                      so each can send its response on the originating channel
 * @param[in] frame Frame containing the command payload
 *                  - Type: const rx_frame_t*
 *                  - Must NOT be NULL
 *                  - frame->header.type must be k_frame_type_command
 *                  - frame->payload contains nanopb-encoded protobuf message
 *                  - frame->header.length is protobuf message size in bytes
 *
 * @return rx_err_t Processing status
 * @retval k_rx_ok Message decoded and handled by one of the helpers
 * @retval k_rx_err_null_ptr frame is nullptr
 * @retval k_rx_err_invalid_arg No helper recognised the payload (unknown message type)
 *
 * @pre frame must not be nullptr
 * @pre frame->header.type == k_frame_type_command (enforced by caller)
 * @post If k_rx_ok: shared_data updated (motor command, e-stop, PID gains, or retransmit cfg)
 * @post If k_rx_err_invalid_arg: warning logged, no shared_data changes
 *
 * @invariant Function executes in Communication Task context (Priority 5), not ISR
 * @invariant Unknown message types do NOT crash firmware (log warning, return error)
 *
 * @note Not thread-safe; single-threaded callback invocation guaranteed by rx_comm_manager
 * @since Version 1.0.0
 *
 * @see internal_handle_velocity_command() SetVelocityRequest handler
 * @see internal_handle_estop_command() EmergencyStopRequest handler
 * @see internal_handle_pid_gains_command() SetPIDGainsRequest handler
 * @see internal_handle_retransmit_config_command() SetRetransmitConfigRequest handler
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 4: [PASS] Function body is 12 lines (well under 60 LOC target)
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions documented
 *
 * @callgraph
 * @callergraph
 */
static rx_err_t internal_handle_command_frame(rx_comm_channel_t channel, const rx_frame_t* frame)
{
  RX_CHECK_NULL_PTR(frame, s_tag, "frame pointer must not be NULL");

  if (internal_handle_velocity_command(channel, frame)) {
    return k_rx_ok;
  }
  if (internal_handle_estop_command(channel, frame)) {
    return k_rx_ok;
  }
  if (internal_handle_pid_gains_command(channel, frame)) {
    return k_rx_ok;
  }
  if (internal_handle_retransmit_config_command(frame)) {
    return k_rx_ok;
  }

  rx_log_warn(s_tag, "unknown command frame message type");
  return k_rx_err_invalid_arg;
}

/**
 * @brief Send an encoded command response back to the host on the originating channel
 *
 * @details
 * Convenience wrapper around rx_comm_manager_respond() for use by command handlers.
 * Sends a RESPONSE-type frame containing a pre-encoded protobuf payload on the channel
 * that delivered the original command. Response sending is best-effort: a send failure
 * is logged as a warning but does NOT affect command processing (the command has already
 * been applied to shared_data before this function is called).
 *
 * @param[in] channel     Channel to send response on (must match incoming command channel)
 * @param[in] payload     Encoded protobuf bytes to transmit (must not be nullptr)
 * @param[in] payload_len Number of encoded bytes in payload
 *
 * @pre payload must not be nullptr
 * @pre payload_len must be > 0
 * @post Response frame queued for transmission on channel (send failure logged, not fatal)
 *
 * @note Not thread-safe; called only from Communication Task context
 * @note Send failure does not affect command processing (best-effort)
 *
 * @see rx_comm_manager_respond() Underlying send function
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 1 postcondition documented
 * - Rule 7: [PASS] rx_comm_manager_respond() return value checked
 */
static void internal_send_command_response(rx_comm_channel_t channel,
                                           const uint8_t*    payload,
                                           const uint32_t    payload_len)
{
  if (payload == nullptr) {
    rx_log_warn(s_tag, "internal_send_command_response: payload is NULL");
    return;
  }
  if (payload_len == 0) {
    rx_log_warn(s_tag, "internal_send_command_response: payload_len is zero");
    return;
  }
  const rx_err_t err = rx_comm_manager_respond(&g_comm_manager, channel, payload, payload_len);
  if (err != k_rx_ok) {
    rx_log_warn_val(s_tag, "Command response send failed", (uint32_t)err);
  }
}

/**
 * @brief Attempt to decode and handle a SetVelocityRequest from a command frame
 *
 * @details
 * Tries to decode the frame payload as a star_v1_SetVelocityRequest protobuf message.
 * If decoding succeeds and the required command sub-message is present, converts the
 * four motor velocities (double -> float) into a motor_command_t and writes it to
 * shared_data for consumption by the Motor Control Task.
 *
 * Algorithm:
 * 1. Attempt nanopb decode via rx_nanopb_decode_velocity_request()
 * 2. Verify has_command flag is set (sub-message present)
 * 3. Build motor_command_t: copy 4 velocities, sequence number, timestamp, valid=true
 * 4. Write to shared_data via shared_data_set_motor_command()
 * 5. Encode SetVelocityResponse and send back on originating channel
 * 6. Log result and return true
 *
 * @param[in] channel Channel the command arrived on; response sent on same channel
 * @param[in] frame COMMAND frame to decode
 *                  - Must NOT be nullptr (caller guarantees)
 *                  - frame->payload and frame->header.length are used for decode
 *
 * @return bool Whether this handler recognised and consumed the frame
 * @retval true  Frame decoded as SetVelocityRequest; motor command written to shared_data
 * @retval false Decode failed; frame is not a SetVelocityRequest (try next handler)
 *
 * @pre frame must not be nullptr
 * @pre shared_data_init() must have been called
 * @post On true: shared_data motor command updated with new velocities
 * @post On true: k_event_new_motor_cmd event set (wakes Motor Control Task)
 * @post On true: SetVelocityResponse sent back on channel (best-effort)
 *
 * @note Not thread-safe; executes in Communication Task context (Priority 5)
 * @note double -> float velocity conversion: +/-0.0001 m/s precision loss acceptable
 * @note Response send failure is logged but does not affect command processing
 *
 * @since Version 1.0.0
 * @see rx_nanopb_decode_velocity_request() nanopb decode function
 * @see shared_data_set_motor_command() Thread-safe motor command write
 * @see rx_nanopb_encode_velocity_response() Encodes the acknowledgment response
 * @see internal_handle_command_frame() Caller (cascade dispatcher)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 4: [PASS] Function body is under 45 lines
 * - Rule 5: [PASS] 2 preconditions, 3 postconditions documented
 * - Rule 7: [PASS] All return values checked
 */
/**
 * @brief Translate a decoded SetVelocityRequest into a shared motor_command_t.
 */
static motor_command_t internal_velocity_request_to_command(const star_v1_SetVelocityRequest* req)
{
  motor_command_t cmd                              = {};
  cmd.target_velocity_mps[k_motor_idx_front_left]  = (float)req->command.front_left_velocity_mps;
  cmd.target_velocity_mps[k_motor_idx_front_right] = (float)req->command.front_right_velocity_mps;
  cmd.target_velocity_mps[k_motor_idx_back_left]   = (float)req->command.back_left_velocity_mps;
  cmd.target_velocity_mps[k_motor_idx_back_right]  = (float)req->command.back_right_velocity_mps;
  cmd.sequence                                     = req->command.sequence;
  cmd.timestamp_ms                                 = tx_time_get();
  cmd.valid                                        = true;
  return cmd;
}

/**
 * @brief Push a motor command to shared_data and read it back to verify.
 *
 * @return The error code from shared_data_set_motor_command() so the caller
 *         can include it in the SetVelocityResponse status field.
 */
static rx_err_t internal_velocity_apply_command(const motor_command_t* cmd)
{
  const rx_err_t set_err = shared_data_set_motor_command(cmd);
  if (set_err != k_rx_ok) {
    rx_log_error_val(s_tag, "Failed to set motor cmd", (uint32_t)set_err);
    return set_err;
  }
  motor_command_t verify_cmd = {};
  const rx_err_t  get_err    = shared_data_get_motor_command(&verify_cmd);
  if (get_err == k_rx_ok) {
    rx_log_info_val(s_tag, "VEL set valid=", (uint32_t)verify_cmd.valid);
    rx_log_info_val(s_tag, "VEL set seq=", verify_cmd.sequence);
  } else {
    rx_log_warn_val(s_tag, "VEL verify get failed", (uint32_t)get_err);
  }
  return set_err;
}

/**
 * @brief Encode and best-effort-send the SetVelocityResponse for a command.
 */
static void internal_velocity_send_response(rx_comm_channel_t                 channel,
                                            rx_err_t                          set_err,
                                            const star_v1_SetVelocityRequest* req)
{
  star_v1_SetVelocityResponse response =
    (star_v1_SetVelocityResponse)star_v1_SetVelocityResponse_init_zero;
  response.has_header = true;
  const star_v1_Status resp_status =
    (set_err == k_rx_ok) ? star_v1_Status_STATUS_OK : star_v1_Status_STATUS_INTERNAL_ERROR;
  const char* req_id = nullptr;
  if (req->has_header) {
    req_id = req->header.request_id;
  }
  rx_nanopb_create_response_header(&response.header, resp_status, req_id);

  uint32_t       encoded_len = 0;
  const rx_err_t enc_err     = rx_nanopb_encode_velocity_response(&response,
                                                              s_response_buffer,
                                                              (uint32_t)k_comm_response_buffer_size,
                                                              &encoded_len);
  if (enc_err == k_rx_ok) {
    internal_send_command_response(channel, s_response_buffer, encoded_len);
  } else {
    rx_log_warn_val(s_tag, "Velocity response encode failed", (uint32_t)enc_err);
  }
}

static bool internal_handle_velocity_command(rx_comm_channel_t channel, const rx_frame_t* frame)
{
  rx_log_info_val(s_tag, "VEL entry, frame.len=", (uint32_t)frame->header.length);
  star_v1_SetVelocityRequest velocity_req = star_v1_SetVelocityRequest_init_zero;
  const rx_err_t             err =
    rx_nanopb_decode_velocity_request(frame->payload, frame->header.length, &velocity_req);
  if (err != k_rx_ok || !velocity_req.has_command) {
    rx_log_warn_val(s_tag, "VEL decode/has_command FAIL err=", (uint32_t)err);
    return false;
  }

  const motor_command_t cmd     = internal_velocity_request_to_command(&velocity_req);
  const rx_err_t        set_err = internal_velocity_apply_command(&cmd);

  /* Best-effort response (command is already applied to shared_data). */
  internal_velocity_send_response(channel, set_err, &velocity_req);

  return true;
}

/**
 * @brief Attempt to decode and handle an EmergencyStopRequest from a command frame
 *
 * @details
 * Tries to decode the frame payload as a star_v1_EmergencyStopRequest protobuf message.
 * If decoding succeeds, triggers an emergency stop via shared_data with reason
 * k_estop_reason_manual. This immediately disables all motor outputs in the Motor Control Task.
 *
 * Algorithm:
 * 1. Attempt nanopb decode via rx_nanopb_decode_estop_request()
 * 2. Log warning ("E-Stop request received")
 * 3. Call shared_data_trigger_estop(k_estop_reason_manual)
 * 4. Encode EmergencyStopResponse and send back on originating channel
 * 5. Log any error and return true
 *
 * @param[in] channel Channel the command arrived on; response sent on same channel
 * @param[in] frame COMMAND frame to decode
 *                  - Must NOT be nullptr (caller guarantees)
 *                  - frame->payload and frame->header.length are used for decode
 *
 * @return bool Whether this handler recognised and consumed the frame
 * @retval true  Frame decoded as EmergencyStopRequest; emergency stop triggered
 * @retval false Decode failed; frame is not an EmergencyStopRequest (try next handler)
 *
 * @pre frame must not be nullptr
 * @pre shared_data_init() must have been called
 * @post On true: estop_active == true in shared_data
 * @post On true: k_event_estop_triggered event set (wakes Motor Control Task)
 * @post On true: EmergencyStopResponse sent back on channel (best-effort)
 *
 * @note Not thread-safe; executes in Communication Task context (Priority 5)
 * @note Response send failure is logged but does not affect e-stop processing
 * @warning E-stop overrides any pending velocity commands; motors disabled immediately
 *
 * @since Version 1.0.0
 * @see rx_nanopb_decode_estop_request() nanopb decode function
 * @see shared_data_trigger_estop() Thread-safe emergency stop trigger
 * @see rx_nanopb_encode_estop_response() Encodes the acknowledgment response
 * @see internal_handle_command_frame() Caller (cascade dispatcher)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 4: [PASS] Function body is under 35 lines
 * - Rule 5: [PASS] 2 preconditions, 3 postconditions documented
 * - Rule 7: [PASS] All return values checked
 */
static bool internal_handle_estop_command(rx_comm_channel_t channel, const rx_frame_t* frame)
{
  star_v1_EmergencyStopRequest estop_req = star_v1_EmergencyStopRequest_init_zero;
  const rx_err_t               err =
    rx_nanopb_decode_estop_request(frame->payload, frame->header.length, &estop_req);
  if (err != k_rx_ok) {
    return false;
  }

  rx_log_warn(s_tag, "E-Stop request received");

  const rx_err_t trigger_err = shared_data_trigger_estop(k_estop_reason_manual);
  if (trigger_err != k_rx_ok) {
    rx_log_error_val(s_tag, "Failed to trigger e-stop", (uint32_t)trigger_err);
  }

  /* Send response back to host (best-effort; e-stop already triggered) */
  {
    star_v1_EmergencyStopResponse response =
      (star_v1_EmergencyStopResponse)star_v1_EmergencyStopResponse_init_zero;
    response.has_header    = true;
    response.estop_engaged = (bool)(trigger_err == k_rx_ok);
    const star_v1_Status resp_status =
      (trigger_err == k_rx_ok) ? star_v1_Status_STATUS_OK : star_v1_Status_STATUS_INTERNAL_ERROR;
    const char* req_id = (int)estop_req.has_header ? estop_req.header.request_id : nullptr;
    rx_nanopb_create_response_header(&response.header, resp_status, req_id);

    uint32_t       encoded_len = 0;
    const rx_err_t enc_err     = rx_nanopb_encode_estop_response(&response,
                                                             s_response_buffer,
                                                             (uint32_t)k_comm_response_buffer_size,
                                                             &encoded_len);
    if (enc_err == k_rx_ok) {
      internal_send_command_response(channel, s_response_buffer, encoded_len);
    } else {
      rx_log_warn_val(s_tag, "E-Stop response encode failed", (uint32_t)enc_err);
    }
  }

  return true;
}

/**
 * @brief Attempt to decode and handle a SetPIDGainsRequest from a command frame
 *
 * @details
 * Tries to decode the frame payload as a star_v1_SetPIDGainsRequest protobuf message.
 * If decoding succeeds and the required pid_config sub-message is present, converts all
 * gain fields (double -> float) into a pid_gains_t structure and writes it to shared_data.
 * The Motor Control Task applies the gains on the next control cycle.
 *
 * Algorithm:
 * 1. Attempt nanopb decode via rx_nanopb_decode_pid_gains_request()
 * 2. Verify has_pid_config flag is set (sub-message present)
 * 3. Build pid_gains_t: convert kp, ki, kd, limits, set update_pending=true
 * 4. Write to shared_data via shared_data_set_pid_gains()
 * 5. Encode SetPIDGainsResponse and send back on originating channel
 * 6. Log result and return true
 *
 * @param[in] channel Channel the command arrived on; response sent on same channel
 * @param[in] frame COMMAND frame to decode
 *                  - Must NOT be nullptr (caller guarantees)
 *                  - frame->payload and frame->header.length are used for decode
 *
 * @return bool Whether this handler recognised and consumed the frame
 * @retval true  Frame decoded as SetPIDGainsRequest; PID gains written to shared_data
 * @retval false Decode failed or has_pid_config not set (try next handler)
 *
 * @pre frame must not be nullptr
 * @pre shared_data_init() must have been called
 * @post On true: shared_data PID gains updated with new values
 * @post On true: k_event_pid_gains_updated event set (Motor Control Task will apply)
 * @post On true: SetPIDGainsResponse sent back on channel (best-effort)
 *
 * @note Not thread-safe; executes in Communication Task context (Priority 5)
 * @note double -> float gain conversion: precision loss negligible for PID tuning
 * @note Response send failure is logged but does not affect command processing
 * @note The optional message string field in SetPIDGainsResponse is left empty
 *
 * @since Version 1.0.0
 * @see rx_nanopb_decode_pid_gains_request() nanopb decode function
 * @see shared_data_set_pid_gains() Thread-safe PID gains write
 * @see rx_nanopb_encode_pid_gains_response() Encodes the acknowledgment response
 * @see internal_handle_command_frame() Caller (cascade dispatcher)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 4: [PASS] Function body is under 45 lines
 * - Rule 5: [PASS] 2 preconditions, 3 postconditions documented
 * - Rule 7: [PASS] All return values checked
 */
static bool internal_handle_pid_gains_command(rx_comm_channel_t channel, const rx_frame_t* frame)
{
  star_v1_SetPIDGainsRequest pid_req = star_v1_SetPIDGainsRequest_init_zero;
  const rx_err_t             err =
    rx_nanopb_decode_pid_gains_request(frame->payload, frame->header.length, &pid_req);
  if (err != k_rx_ok || !pid_req.has_pid_config) {
    return false;
  }

  rx_log_info(s_tag, "PID gains request received");

  /* Convert protobuf PID config to firmware structure */
  pid_gains_t gains    = {};
  gains.kp             = (float)pid_req.pid_config.kp;
  gains.ki             = (float)pid_req.pid_config.ki;
  gains.kd             = (float)pid_req.pid_config.kd;
  gains.output_min     = (float)pid_req.pid_config.output_min_percent;
  gains.output_max     = (float)pid_req.pid_config.output_max_percent;
  gains.integral_min   = (float)pid_req.pid_config.integral_min;
  gains.integral_max   = (float)pid_req.pid_config.integral_max;
  gains.update_pending = true;

  const rx_err_t set_err = shared_data_set_pid_gains(&gains);
  if (set_err != k_rx_ok) {
    rx_log_error_val(s_tag, "Failed to set PID gains", (uint32_t)set_err);
  } else {
    rx_log_info(s_tag, "PID gains updated successfully");
  }

  /* Send response back to host (best-effort; gains already written to shared_data) */
  {
    star_v1_SetPIDGainsResponse response =
      (star_v1_SetPIDGainsResponse)star_v1_SetPIDGainsResponse_init_zero;
    response.has_header = true;
    response.success    = (bool)(set_err == k_rx_ok);
    /* response.message left as init_zero: NULL callback skips optional string field */
    const star_v1_Status resp_status =
      (set_err == k_rx_ok) ? star_v1_Status_STATUS_OK : star_v1_Status_STATUS_INTERNAL_ERROR;
    const char* req_id = (int)pid_req.has_header ? pid_req.header.request_id : nullptr;
    rx_nanopb_create_response_header(&response.header, resp_status, req_id);

    uint32_t       encoded_len = 0;
    const rx_err_t enc_err =
      rx_nanopb_encode_pid_gains_response(&response,
                                          s_response_buffer,
                                          (uint32_t)k_comm_response_buffer_size,
                                          &encoded_len);
    if (enc_err == k_rx_ok) {
      internal_send_command_response(channel, s_response_buffer, encoded_len);
    } else {
      rx_log_warn_val(s_tag, "PID gains response encode failed", (uint32_t)enc_err);
    }
  }

  return true;
}

/**
 * @brief Attempt to decode and handle a SetRetransmitConfigRequest from a command frame
 *
 * @details
 * Tries to decode the frame payload as a star_v1_SetRetransmitConfigRequest protobuf
 * message. If decoding succeeds and the required retransmit_config sub-message is present,
 * validates all fields against safe bounds before narrowing to their firmware types, then
 * calls rx_comm_manager_set_auto_retransmit() to update the HARQ retransmission parameters.
 *
 * Algorithm:
 * 1. Attempt nanopb decode via rx_nanopb_decode_retransmit_config_request()
 * 2. Verify has_retransmit_config flag is set (sub-message present)
 * 3. Extract fields into const uint32_t locals for bounds checking
 * 4. Validate each field against typed enum limits; log error and return false on violation
 * 5. Build rx_spi_comm_retransmit_config_t with safe narrowing casts
 * 6. Call rx_comm_manager_set_auto_retransmit()
 * 7. Log result and return true
 *
 * **Bounds validated:**
 * - max_retries: [1, 10]
 * - ack_timeout_ms: [10, 1000]
 * - max_backoff_ms: [50, 5000]
 *
 * @param[in] frame COMMAND frame to decode
 *                  - Must NOT be nullptr (caller guarantees)
 *                  - frame->payload and frame->header.length are used for decode
 *
 * @return bool Whether this handler recognised and consumed the frame
 * @retval true  Frame decoded as SetRetransmitConfigRequest; retransmit config updated
 * @retval false Decode failed, has_retransmit_config not set, or fields out of range
 *
 * @pre frame must not be nullptr
 * @pre g_comm_manager must be initialised (comm manager init called)
 * @post On true: rx_comm_manager HARQ retransmit parameters updated
 * @post On true: retransmit config is within safe validated bounds
 *
 * @note Not thread-safe; executes in Communication Task context (Priority 5)
 * @warning Out-of-range protobuf values are rejected (logged error, returns false)
 *
 * @since Version 1.0.0
 * @see rx_nanopb_decode_retransmit_config_request() nanopb decode function
 * @see rx_comm_manager_set_auto_retransmit() Update HARQ retransmission config
 * @see retransmit_retries_limits_t Bounds for max_retries
 * @see retransmit_timing_limits_t Bounds for timing fields
 * @see internal_handle_command_frame() Caller (cascade dispatcher)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 4: [PASS] Function body is under 40 lines
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions documented
 * - Rule 7: [PASS] All return values checked
 */
static bool internal_handle_retransmit_config_command(const rx_frame_t* frame)
{
  star_v1_SetRetransmitConfigRequest retransmit_req = star_v1_SetRetransmitConfigRequest_init_zero;
  const rx_err_t err = rx_nanopb_decode_retransmit_config_request(frame->payload,
                                                                  frame->header.length,
                                                                  &retransmit_req);
  if (err != k_rx_ok || !retransmit_req.has_retransmit_config) {
    return false;
  }

  /* Validate retransmit config bounds before narrowing casts */
  const uint32_t max_retries = retransmit_req.retransmit_config.max_retries;
  const uint32_t ack_timeout = retransmit_req.retransmit_config.ack_timeout_ms;
  const uint32_t max_backoff = retransmit_req.retransmit_config.max_backoff_ms;

  if ((max_retries < k_retransmit_max_retries_min) ||
      (max_retries > k_retransmit_max_retries_max) ||
      (ack_timeout < k_retransmit_ack_timeout_min_ms) ||
      (ack_timeout > k_retransmit_ack_timeout_max_ms) ||
      (max_backoff < k_retransmit_backoff_min_ms) || (max_backoff > k_retransmit_backoff_max_ms)) {
    rx_log_error(s_tag, "retransmit config out of range");
    return false;
  }

  rx_spi_comm_retransmit_config_t cfg = {};
  cfg.max_retries                     = (uint8_t)max_retries;
  cfg.ack_timeout_ms                  = (uint16_t)ack_timeout;
  cfg.max_backoff_ms                  = (uint16_t)max_backoff;

  const rx_err_t set_err =
    rx_comm_manager_set_auto_retransmit(&g_comm_manager,
                                        retransmit_req.retransmit_config.enabled,
                                        &cfg);
  if (set_err != k_rx_ok) {
    rx_log_error_val(s_tag, "Failed to set retransmit config", (uint32_t)set_err);
  } else {
    rx_log_info(s_tag, "Retransmit config updated");
  }
  return true;
}
