/**
 * @file rx_obstacle_detect.h
 * @brief Safety-Critical Obstacle Detection System with Emergency Motor Stop
 *
 * @details
 * Provides a ThreadX-based real-time obstacle detection system using HC-SR04
 * ultrasonic sensors with immediate emergency stop capability. This module
 * continuously monitors configured sensors and automatically stops all motors
 * when obstacles are detected within a configurable safety threshold.
 *
 * **System Overview:**
 *
 * This is a simplified safety system focused on collision avoidance through
 * emergency motor stop, not full Dynamic Window Approach (DWA) path planning.
 * The module runs autonomously in a ThreadX task, polling HC-SR04 sensors at
 * a configurable rate and triggering emergency stops when obstacles breach
 * the safety perimeter.
 *
 * **Architecture:**
 *
 * @dot
 * digraph obstacle_detect_arch {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   user [label="User Application"];
 *   api [label="rx_obstacle_detect API\n(This Module)", fillcolor=lightblue, style=filled];
 *   task [label="Detection ThreadX Task\n(Internal)"];
 *   hcsr04 [label="HC-SR04 Sensors\n(rx_hcsr04)"];
 *   motors [label="Motor Drivers\n(rx_motor)"];
 *   callback [label="Event Callback\n(Optional)", shape=ellipse];
 *
 *   user -> api [label="init/start/stop"];
 *   api -> task [label="creates/controls"];
 *   task -> hcsr04 [label="polls @ 10-100 Hz"];
 *   task -> motors [label="emergency stop"];
 *   task -> callback [label="notify state change"];
 *   callback -> user [label="obstacle events"];
 * }
 * @enddot
 *
 * **State Machine:**
 *
 * @startuml
 * [*] --> Stopped : init()
 * Stopped --> Running : start()
 * Running --> Obstacle : obstacle detected
 * Obstacle --> Running : obstacle cleared
 * Running --> Stopped : stop()
 * Obstacle --> Stopped : stop()
 * Stopped --> [*] : deinit()
 *
 * state Running {
 *   [*] --> Polling
 *   Polling --> Debouncing : distance < threshold
 *   Debouncing --> Polling : distance >= threshold
 *   Debouncing --> EmergencyStop : debounce_count >= samples
 * }
 * @enduml
 *
 * **Features:**
 *
 * | Feature | Description |
 * |---------|-------------|
 * | **Multi-Sensor Support** | Up to 8 HC-SR04 sensors (front, sides, back) |
 * | **Configurable Threshold** | Stop distance 2-400cm (typical: 30cm) |
 * | **Debouncing** | 1-10 consecutive readings to confirm (typical: 3) |
 * | **Polling Rate** | 10-1000ms interval (typical: 20ms = 50Hz) |
 * | **ThreadX Integration** | Autonomous operation in dedicated task |
 * | **Event Callbacks** | Optional notification on state changes |
 * | **Statistics** | Track polls, events, false positives |
 * | **Emergency Stop** | Immediate motor shutdown on detection |
 *
 * **Detection Logic:**
 *
 * @msc
 * Task, Sensor1, Sensor2, Motors, Callback;
 *
 * --- [label="Normal Operation"];
 * Task => Sensor1 [label="measure()"];
 * Sensor1 => Task [label="50cm (OK)"];
 * Task => Sensor2 [label="measure()"];
 * Sensor2 => Task [label="60cm (OK)"];
 * Task note Task [label="wait poll_interval_ms"];
 *
 * --- [label="Obstacle Detection"];
 * Task => Sensor1 [label="measure()"];
 * Sensor1 => Task [label="25cm (< 30cm threshold)"];
 * Task box Task [label="debounce_counter++ (1)"];
 * Task note Task [label="wait poll_interval_ms"];
 *
 * Task => Sensor1 [label="measure()"];
 * Sensor1 => Task [label="23cm (< threshold)"];
 * Task box Task [label="debounce_counter++ (2)"];
 *
 * Task => Sensor1 [label="measure()"];
 * Sensor1 => Task [label="22cm (< threshold)"];
 * Task box Task [label="debounce_counter++ (3 = threshold!)"];
 * Task => Motors [label="EMERGENCY STOP"];
 * Motors box Motors [label="All motors stopped"];
 * Task => Callback [label="obstacle_detected=true"];
 * Task box Task [label="state = OBSTACLE"];
 * @endmsc
 *
 * **Performance Characteristics:**
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | **Detection Latency** | poll_interval_ms x debounce_samples | e.g., 20ms x 3 = 60ms worst-case |
 * | **CPU Usage** | ~5% @ 50Hz, 4 sensors | RX72N @ 240 MHz |
 * | **Memory (Static)** | ~2.5 KB | Stack (2048) + handle struct (512) |
 * | **Memory (Dynamic)** | 0 | Zero heap allocation |
 * | **Thread Priority** | 8 (configurable) | Higher = more responsive |
 *
 * **Safety Considerations:**
 *
 * @warning This is a last-resort safety system, NOT primary collision avoidance
 * @warning Detection latency depends on polling rate and debounce settings
 * @warning Sensors have blind spots - physical robot design must account for this
 * @warning At maximum polling rate (100Hz, 1 debounce), minimum latency is 10ms
 * @warning At typical settings (50Hz, 3 debounce), worst-case latency is 60ms
 *
 * @attention Robot traveling at 1 m/s moves 6cm during worst-case 60ms detection
 * @attention Always leave safety margin: threshold > max_travel_during_latency
 * @attention For 1 m/s max speed, use threshold >= 10cm (60ms x 1m/s + margin)
 *
 * @par Hardware Requirements:
 *
 * | Resource | Requirement | Notes |
 * |----------|-------------|-------|
 * | **CPU** | RX72N @ 240 MHz | ~12k CPU cycles per poll (4 sensors) |
 * | **RAM** | 2.5 KB static | Task stack + handle structure |
 * | **ThreadX** | Required | Task creation and event flags |
 * | **HC-SR04 Sensors** | 1-8 sensors | Each requires 2 GPIO pins |
 * | **Motor Controllers** | 1-4 motors | Must support emergency stop |
 *
 * @par Module Dependencies:
 * - `rx_hcsr04`: HC-SR04 ultrasonic sensor driver
 * - `rx_motor`: Motor control with emergency stop capability
 * - `rx_err`: Error code definitions
 * - `tx_api`: ThreadX RTOS for task management
 * - `rx_check`: Assertion and validation macros
 * - `rx_threadx_config`: ThreadX configuration constants
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | All loops use enum constants |
 * | 3. No dynamic memory | [OK] | Zero malloc/free - static allocation only |
 * | 4. Functions <=60 lines | [OK] | Longest: `internal_poll_sensors` (54 lines) |
 * | 5. Min 2 assertions/func | [OK] | All public APIs validate inputs |
 * | 6. Smallest scope | [OK] | Variables declared at first use |
 * | 7. Check return values | [OK] | All HC-SR04 and motor calls checked |
 * | 8. Limit preprocessor | [OK] | Only C23 typed enums, no macros |
 * | 9. Restrict pointers | [OK] | Max one level of dereferencing |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S):**
 * - Module has ONE job: Monitor sensors and stop motors on obstacle detection
 * - Does NOT handle path planning, navigation, or motor control logic
 * - Does NOT configure sensors or motors - only uses them
 *
 * **Open/Closed (O):**
 * - Extensible via configuration (threshold, debounce, polling rate)
 * - Open for extension via callback mechanism
 * - Closed for modification - core logic is stable
 *
 * **Dependency Inversion (D):**
 * - Depends on `rx_hcsr04` and `rx_motor` abstractions, not implementations
 * - Accepts sensor/motor handles via dependency injection (config)
 * - Callback mechanism allows user code to react without modifying module
 *
 * @see rx_hcsr04.h HC-SR04 ultrasonic sensor driver
 * @see rx_motor.h Motor control with emergency stop
 * @see rx_obstacle_detect.c Implementation file
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04.h"
#include "rx_motor.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rx_obstacle_detect_constants_t
 * @brief Obstacle detection system configuration constants
 *
 * @details
 * Defines limits and defaults for the obstacle detection system including
 * maximum sensor/motor counts, ThreadX task configuration, and resource limits.
 *
 * @par Value Rationale:
 *
 * | Constant | Value | Rationale |
 * |----------|-------|-----------|
 * | max_sensors | 8 | Typical robot: 4 front, 2 sides, 2 rear |
 * | max_motors | 4 | STAR robot has 4 drive motors |
 * | task_stack_size | 2048 bytes | Sufficient for nested calls + local buffers |
 * | task_priority | 8 | Medium priority (0=highest, 31=lowest) |
 *
 * **Task Priority Considerations:**
 * - Priority 8 balances responsiveness with system load
 * - Higher priority (lower number) reduces detection latency
 * - Lower priority (higher number) reduces CPU contention
 * - Typical ThreadX priorities: 0-7 critical, 8-15 high, 16-23 normal, 24-31 low
 *
 * @see rx_obstacle_detect_config_t Uses these limits for validation
 * @see rx_obstacle_detect_t Allocates arrays sized by these constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**< @brief Maximum number of HC-SR04 sensors supported simultaneously
   *
   * @details
   * Maximum sensors that can be monitored in a single detection instance.
   * Each sensor requires 2 GPIO pins (TRIG, ECHO) and ~64 bytes RAM.
   *
   * @par Value: 8 sensors
   * @par Memory Impact: 8 x 64 bytes = 512 bytes (handle struct arrays)
   * @par Typical Usage: 4-6 sensors for 360deg coverage
   */
  k_obstacle_detect_max_sensors = 8,

  /**< @brief Maximum number of motors controlled by emergency stop
   *
   * @details
   * Maximum motors that will be stopped when obstacle detected. Each motor
   * is an `rx_motor_handle_t` pointer in configuration.
   *
   * @par Value: 4 motors
   * @par Memory Impact: 4 x sizeof(rx_motor_handle_t*) = 16 bytes
   * @par STAR Robot: Uses all 4 motors (front-left, front-right, rear-left, rear-right)
   */
  k_obstacle_detect_max_motors = 4,

  /**< @brief Detection task stack size in bytes
   *
   * @details
   * Stack memory allocated for the ThreadX detection task. Must be sufficient
   * for deepest call chain plus local variables and ThreadX overhead.
   *
   * @par Value: 2048 bytes (2 KB)
   *
   * @par Stack Usage Analysis:
   * - ThreadX overhead: ~256 bytes
   * - Task entry function: ~64 bytes
   * - internal_poll_sensors(): ~128 bytes
   * - rx_hcsr04_measure_blocking(): ~256 bytes (deepest call)
   * - rx_motor_stop(): ~128 bytes
   * - Margin: ~1216 bytes (safe margin for future changes)
   *
   * @par Rationale:
   * 2048 bytes provides ~4x safety margin over measured usage (~512 bytes).
   * ThreadX stack overflow detection will catch underruns during testing.
   *
   * @warning Insufficient stack causes undefined behavior (crashes, corruption)
   */
  k_obstacle_detect_task_stack_size = 2048,

  /**< @brief Detection task ThreadX priority level
   *
   * @details
   * ThreadX task priority for obstacle detection. Lower number = higher priority.
   * Priority 8 provides good responsiveness without starving other tasks.
   *
   * @par Value: 8 (medium-high priority)
   *
   * @par Priority Levels in STAR System:
   * - 0-3: Critical interrupt handlers
   * - 4-7: Safety-critical tasks (e.g., motor control PID)
   * - **8-11: High-priority tasks (obstacle detection)**
   * - 12-15: Normal application tasks
   * - 16-31: Background/low-priority tasks
   *
   * @par Rationale:
   * Priority 8 ensures:
   * - Obstacle detection preempts normal tasks
   * - Does not starve motor control (priority 5)
   * - Typical 20ms poll interval leaves CPU time for other tasks
   *
   * @par Performance Impact:
   * - @ 50Hz polling (20ms), task runs ~1ms/poll = 5% CPU @ 240 MHz
   * - Leaves 95% CPU for other tasks
   *
   * @see tx_thread_create() ThreadX task creation uses this priority
   */
  k_obstacle_detect_task_priority = 8,
} rx_obstacle_detect_constants_t;

/**
 * @enum rx_obstacle_detect_state_t
 * @brief Obstacle detection system operational states
 *
 * @details
 * Defines the three possible states of the obstacle detection system.
 * State transitions are controlled by user commands (start/stop) and
 * sensor measurements (obstacle detected/cleared).
 *
 * **State Transition Diagram:**
 *
 * @startuml
 * [*] --> STOPPED : rx_obstacle_detect_init()
 *
 * STOPPED --> RUNNING : rx_obstacle_detect_start()
 *
 * RUNNING --> OBSTACLE : any sensor < threshold\n(after debouncing)
 * RUNNING --> STOPPED : rx_obstacle_detect_stop()
 *
 * OBSTACLE --> RUNNING : all sensors >= threshold\n(obstacle cleared)
 * OBSTACLE --> STOPPED : rx_obstacle_detect_stop()
 *
 * STOPPED --> [*] : rx_obstacle_detect_deinit()
 *
 * note right of STOPPED
 *   Task suspended
 *   No polling active
 *   Motors controllable
 * end note
 *
 * note right of RUNNING
 *   Task active
 *   Polling sensors @ poll_interval_ms
 *   Motors controllable
 * end note
 *
 * note right of OBSTACLE
 *   Task active
 *   Polling continues
 *   **Motors STOPPED (emergency)**
 * end note
 * @enduml
 *
 * **State Transition Table:**
 *
 * | Current State | Event | Next State | Action | Guard |
 * |---------------|-------|------------|--------|-------|
 * | STOPPED | start() | RUNNING | Resume ThreadX task | initialized == true |
 * | RUNNING | distance < threshold | OBSTACLE | Emergency stop motors | debounce_counter >= samples |
 * | RUNNING | stop() | STOPPED | Suspend task | none |
 * | OBSTACLE | distance >= threshold | RUNNING | (motors remain stopped) | all sensors clear |
 * | OBSTACLE | stop() | STOPPED | Suspend task | none |
 * | STOPPED | clear_obstacle() | STOPPED | Clear debounce state | none (manual override) |
 *
 * @par State Descriptions:
 * - **STOPPED**: System initialized but not monitoring. ThreadX task suspended. Motors controllable.
 * - **RUNNING**: Active monitoring, no obstacles. Task polling sensors at configured rate. Motors controllable.
 * - **OBSTACLE**: Obstacle within threshold. Motors emergency-stopped. Monitoring continues for clearance.
 *
 * @see rx_obstacle_detect_get_state() Query current state
 * @see rx_obstacle_detect_start() Transition STOPPED -> RUNNING
 * @see rx_obstacle_detect_stop() Transition to STOPPED
 * @see internal_poll_sensors() Implements RUNNING -> OBSTACLE transition
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**< @brief Detection system stopped - not monitoring sensors
   *
   * @details
   * Initial state after `rx_obstacle_detect_init()`. ThreadX detection task
   * is created but suspended. No sensor polling occurs. Motors are controllable
   * by application.
   *
   * @par Entry Actions:
   * - Suspend ThreadX detection task
   * - Clear event flags
   *
   * @par Do Actions:
   * - None (task suspended)
   *
   * @par Exit Actions:
   * - None
   *
   * @par Valid Transitions:
   * - STOPPED -> RUNNING: via `rx_obstacle_detect_start()`
   *
   * @par Value: 0 (default/initial state)
   */
  k_obstacle_detect_state_stopped = 0,

  /**< @brief Detection active - monitoring sensors, no obstacle detected
   *
   * @details
   * Normal operational state. ThreadX task actively polling all configured
   * sensors at `poll_interval_ms` rate. No obstacles within threshold distance.
   * Motors controllable by application.
   *
   * @par Entry Actions:
   * - Resume ThreadX detection task
   * - Set event flags (start flag)
   * - Reset debounce counters
   *
   * @par Do Actions:
   * - Poll each sensor via `rx_hcsr04_measure_blocking()`
   * - Check distance against threshold
   * - Increment debounce counter if distance < threshold
   * - Clear debounce counter if distance >= threshold
   * - Sleep for poll_interval_ms between polls
   *
   * @par Exit Actions:
   * - Suspend task (if stop requested)
   * - OR emergency stop motors (if obstacle confirmed)
   *
   * @par Valid Transitions:
   * - RUNNING -> OBSTACLE: When debounce_counter >= debounce_samples
   * - RUNNING -> STOPPED: via `rx_obstacle_detect_stop()`
   *
   * @par Value: 1
   */
  k_obstacle_detect_state_running = 1,

  /**< @brief Obstacle detected - motors emergency stopped
   *
   * @details
   * Critical state indicating obstacle within safety threshold. All configured
   * motors have been emergency-stopped. Monitoring continues to detect when
   * obstacle clears. Motors cannot be controlled until obstacle clears or
   * user calls `rx_obstacle_detect_clear_obstacle()`.
   *
   * @par Entry Actions:
   * - Call `internal_stop_all_motors()` (emergency stop)
   * - Invoke user callback with obstacle_detected=true
   * - Increment obstacle_events counter
   *
   * @par Do Actions:
   * - Continue polling sensors
   * - Check if ALL sensors clear (distance >= threshold)
   * - Monitor for user stop request
   *
   * @par Exit Actions:
   * - Invoke user callback with obstacle_detected=false (if clearing)
   * - OR suspend task (if stop requested)
   *
   * @par Valid Transitions:
   * - OBSTACLE -> RUNNING: When all sensors >= threshold (automatic)
   * - OBSTACLE -> STOPPED: via `rx_obstacle_detect_stop()`
   *
   * @par Value: 2
   *
   * @warning Motors remain stopped until obstacle clears or manual override
   * @attention Application must NOT attempt to control motors in this state
   */
  k_obstacle_detect_state_obstacle = 2,
} rx_obstacle_detect_state_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @typedef rx_obstacle_detect_callback_t
 * @brief User callback function for obstacle detection state change events
 *
 * @details
 * Optional callback invoked by the detection task when obstacle state changes:
 * - Obstacle detected (distance < threshold after debouncing)
 * - Obstacle cleared (distance >= threshold)
 *
 * The callback is invoked from the ThreadX detection task context. Keep
 * processing brief to avoid delaying sensor polling. Do NOT perform blocking
 * operations or lengthy computations in the callback.
 *
 * **Callback Execution Context:**
 * - Called from: ThreadX obstacle detection task
 * - Task priority: 8 (k_obstacle_detect_task_priority)
 * - Stack: Detection task stack (2048 bytes)
 * - Interrupt level: Task level (not ISR)
 *
 * @par Typical Use Cases:
 * - Log obstacle events to console or storage
 * - Update UI/telemetry with obstacle status
 * - Trigger additional safety responses
 * - Notify higher-level navigation system
 * - Record statistics or diagnostics
 *
 * @param[in] obstacle_detected Obstacle state change direction
 *   - true: Obstacle detected (entering OBSTACLE state)
 *   - false: Obstacle cleared (returning to RUNNING state)
 *   - Type: bool
 *
 * @param[in] sensor_idx Index of sensor that triggered this event
 *   - Range: [0, sensor_count-1]
 *   - Type: uint8_t
 *   - For detected events: Sensor that first exceeded debounce threshold
 *   - For cleared events: Last sensor that cleared
 *   - Maps to sensors[] array in rx_obstacle_detect_config_t
 *
 * @param[in] distance_cm Measured distance in centimeters at time of event
 *   - Range: [2.0, 400.0] cm (HC-SR04 valid range)
 *   - Type: float
 *   - Unit: centimeters
 *   - For detected events: Final distance that triggered (< threshold)
 *   - For cleared events: Distance when sensor cleared (>= threshold)
 *   - May be slightly different from threshold due to polling intervals
 *
 * @param[in] user_data User-provided context pointer from configuration
 *   - Type: void* (caller-defined)
 *   - Passed through from rx_obstacle_detect_config_t.user_data
 *   - Can be NULL if no context needed
 *   - Typical usage: Pointer to application state structure
 *
 * @pre Callback is called from ThreadX task context (not ISR)
 * @pre sensor_idx < sensor_count (validated before callback)
 * @pre distance_cm within HC-SR04 valid range
 *
 * @post Callback should return promptly (< 1ms recommended)
 * @post Must not call blocking functions (sleep, pend, etc.)
 * @post Must not call obstacle detection APIs (would deadlock)
 *
 * @note Thread Safety: Callback may be interrupted by higher-priority tasks
 * @note Re-entrancy: Will NOT be called recursively (single detection task)
 * @note Null Handling: If callback is nullptr in config, no callback invoked
 *
 * @warning DO NOT block or delay in callback - delays sensor polling
 * @warning DO NOT call rx_obstacle_detect_* APIs from callback - causes deadlock
 * @warning DO NOT perform heavy computation - run on detection task stack
 *
 * @par Example - Logging Callback:
 * @code{.c}
 * void obstacle_event_callback(bool obstacle_detected, uint8_t sensor_idx,
 *                              float distance_cm, void* user_data) {
 *     if (obstacle_detected) {
 *         printf("OBSTACLE! Sensor %u: %.1f cm\n", sensor_idx, distance_cm);
 *     } else {
 *         printf("Cleared. Sensor %u: %.1f cm\n", sensor_idx, distance_cm);
 *     }
 * }
 * @endcode
 *
 * @par Example - Telemetry Update:
 * @code{.c}
 * typedef struct {
 *     telemetry_t* telem;
 *     uint32_t     event_count;
 * } app_context_t;
 *
 * void obstacle_telemetry_callback(bool obstacle_detected, uint8_t sensor_idx,
 *                                  float distance_cm, void* user_data) {
 *     app_context_t* ctx = (app_context_t*)user_data;
 *     ctx->event_count++;
 *     ctx->telem->obstacle_active = obstacle_detected;
 *     ctx->telem->min_distance_cm = distance_cm;
 * }
 * @endcode
 *
 * @see rx_obstacle_detect_config_t Configuration includes callback and user_data
 * @see rx_obstacle_detect_init() Registers callback during initialization
 * @see internal_poll_sensors() Invokes callback on state changes
 *
 * @since Version 1.0.0
 */
typedef void (*rx_obstacle_detect_callback_t)(bool    obstacle_detected,
                                              uint8_t sensor_idx,
                                              float   distance_cm,
                                              void*   user_data);

/**
 * @struct rx_obstacle_detect_config_t
 * @brief Obstacle detection system configuration parameters
 *
 * @details
 * Configuration structure passed to `rx_obstacle_detect_init()` to set up
 * the detection system. Defines which sensors and motors to use, detection
 * thresholds, debouncing behavior, and optional event callbacks.
 *
 * **Configuration Guidelines:**
 *
 * | Parameter | Typical Range | Recommended | Notes |
 * |-----------|---------------|-------------|-------|
 * | sensor_count | 1-8 | 4 | Front coverage minimum |
 * | motor_count | 1-4 | 4 | All drive motors |
 * | threshold_cm | 10-100 | 30 | Depends on max speed |
 * | debounce_samples | 1-5 | 3 | Balance latency vs false positives |
 * | poll_interval_ms | 10-100 | 20 | 50Hz good balance |
 *
 * **Threshold Selection Formula:**
 *
 * Choose threshold based on maximum robot velocity and acceptable stopping distance:
 *
 * @f[
 *   \text{threshold}_{cm} \geq v_{max} \times t_{latency} + d_{stop} + \text{margin}
 * @f]
 *
 * where:
 * - @f$ v_{max} @f$ = maximum robot velocity (cm/s)
 * - @f$ t_{latency} @f$ = detection latency (s) = poll_interval x debounce_samples
 * - @f$ d_{stop} @f$ = braking distance (cm)
 * - margin = safety factor (typically 5-10cm)
 *
 * **Example Calculation:**
 * - Max velocity: 100 cm/s (1 m/s)
 * - Poll interval: 20ms, Debounce: 3 -> Latency = 60ms = 0.06s
 * - Travel during latency: 100 x 0.06 = 6cm
 * - Braking distance: ~10cm (depends on motor/PID)
 * - Margin: 10cm
 * - **Threshold: 6 + 10 + 10 = 26cm -> use 30cm**
 *
 * @par Field Constraints:
 *
 * | Field | Constraint | Validation |
 * |-------|------------|------------|
 * | sensors | Non-nullptr | Checked in init |
 * | sensor_count | [1, 8] | Checked in init |
 * | motors | Non-nullptr | Checked in init |
 * | motor_count | [1, 4] | Checked in init |
 * | threshold_cm | [2.0, 400.0] | HC-SR04 range |
 * | debounce_samples | [1, 10] | Checked in init |
 * | poll_interval_ms | [10, 1000] | Checked in init |
 * | callback | Can be NULL | Optional |
 * | user_data | Can be NULL | Optional |
 *
 * @par Usage Example:
 * @code{.c}
 * // Setup sensors
 * rx_hcsr04_t sensor_front, sensor_left, sensor_right, sensor_rear;
 * rx_hcsr04_t* sensors[] = {&sensor_front, &sensor_left, &sensor_right, &sensor_rear};
 *
 * // Setup motors
 * rx_motor_handle_t motor_fl, motor_fr, motor_rl, motor_rr;
 * rx_motor_handle_t* motors[] = {&motor_fl, &motor_fr, &motor_rl, &motor_rr};
 *
 * // Configure obstacle detection
 * rx_obstacle_detect_config_t config = {
 *     .sensors = sensors,
 *     .sensor_count = 4,
 *     .motors = motors,
 *     .motor_count = 4,
 *     .detection_threshold_cm = 30.0f,  // 30cm safety perimeter
 *     .debounce_samples = 3,            // 3 consecutive readings
 *     .poll_interval_ms = 20,           // 50Hz polling
 *     .callback = obstacle_event_handler,
 *     .user_data = &app_context
 * };
 *
 * rx_obstacle_detect_t detector;
 * rx_err_t ret = rx_obstacle_detect_init(&detector, &config);
 * @endcode
 *
 * @see rx_obstacle_detect_init() Uses this configuration
 * @see internal_validate_config() Validates field constraints
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**< @brief Array of pointers to initialized HC-SR04 sensor handles
   *
   * @details
   * Array of sensor handles to monitor for obstacles. Each sensor must be
   * pre-initialized via `rx_hcsr04_init()` before passing to obstacle detection.
   *
   * @par Requirements:
   * - Must be non-NULL
   * - Must contain sensor_count valid pointers
   * - Each element must point to initialized rx_hcsr04_t
   * - Array must remain valid for lifetime of detection system
   *
   * @par Typical Configuration:
   * - 4 sensors: Front, rear, left, right (90deg coverage)
   * - 6 sensors: Frontx2, sidesx2, rearx2 (180deg coverage)
   * - 8 sensors: Full 360deg coverage
   *
   * @warning Sensors must remain initialized while detection is running
   */
  rx_hcsr04_t** sensors;

  /**< @brief Number of sensors in sensors array
   *
   * @details
   * Count of valid sensor pointers in the sensors array.
   *
   * @par Valid Range: [1, k_obstacle_detect_max_sensors] (1-8)
   * @par Typical Value: 4 (front/rear/left/right)
   *
   * @warning Must match actual sensors array size
   */
  uint8_t sensor_count;

  /**< @brief Array of pointers to initialized motor control handles
   *
   * @details
   * Array of motor handles to stop when obstacle detected. Each motor must
   * be pre-initialized via `rx_motor_init()` before passing to obstacle detection.
   *
   * @par Requirements:
   * - Must be non-NULL
   * - Must contain motor_count valid pointers
   * - Each element must point to initialized rx_motor_handle_t
   * - Array must remain valid for lifetime of detection system
   *
   * @par Typical Configuration:
   * - 2 motors: Differential drive (left, right)
   * - 4 motors: Four-wheel drive (FL, FR, RL, RR)
   *
   * @warning Motors must remain initialized while detection is running
   */
  rx_motor_handle_t** motors;

  /**< @brief Number of motors in motors array
   *
   * @details
   * Count of valid motor pointers in the motors array.
   *
   * @par Valid Range: [1, k_obstacle_detect_max_motors] (1-4)
   * @par Typical Value: 4 (four-wheel drive)
   *
   * @warning Must match actual motors array size
   */
  uint8_t motor_count;

  /**< @brief Obstacle detection threshold distance in centimeters
   *
   * @details
   * Minimum safe distance from obstacles. If any sensor measures distance
   * below this threshold (after debouncing), emergency motor stop is triggered.
   *
   * @par Valid Range: [2.0, 400.0] cm (HC-SR04 specification)
   * @par Typical Values:
   * - 10cm: Very close operation (indoor, slow speeds)
   * - 30cm: General purpose (outdoor, moderate speeds)
   * - 50cm: High-speed operation (fast robots)
   *
   * @par Unit: Centimeters (cm)
   *
   * @par Selection Guidelines:
   * threshold >= (max_speed_cm/s x latency_s) + braking_distance_cm + 10cm margin
   *
   * @warning Too small: Insufficient stopping time -> collisions
   * @warning Too large: Excessive false stops, limited mobility
   */
  float detection_threshold_cm;

  /**< @brief Number of consecutive sensor readings required to confirm obstacle
   *
   * @details
   * Debounce count to filter out transient sensor noise or spurious readings.
   * Obstacle is only confirmed after this many consecutive readings below threshold.
   *
   * @par Valid Range: [1, 10]
   * @par Typical Values:
   * - 1: No debouncing (fastest response, more false positives)
   * - 3: Good balance (recommended)
   * - 5: Heavy debouncing (slower response, fewer false positives)
   *
   * @par Impact on Latency:
   * detection_latency = poll_interval_ms x debounce_samples
   * - 20ms x 3 = 60ms latency
   * - 10ms x 3 = 30ms latency
   *
   * @warning Higher values increase detection latency
   */
  uint32_t debounce_samples;

  /**< @brief Sensor polling interval in milliseconds
   *
   * @details
   * Time delay between sensor measurement cycles. Determines polling rate
   * and CPU usage.
   *
   * @par Valid Range: [10, 1000] ms
   * @par Typical Values:
   * - 10ms: 100Hz (very responsive, higher CPU usage)
   * - 20ms: 50Hz (good balance, recommended)
   * - 50ms: 20Hz (slower response, lower CPU usage)
   *
   * @par Unit: Milliseconds (ms)
   *
   * @par CPU Impact:
   * - 100Hz (10ms): ~10% CPU @ 4 sensors
   * - 50Hz (20ms): ~5% CPU @ 4 sensors
   * - 20Hz (50ms): ~2% CPU @ 4 sensors
   *
   * @par Latency Impact:
   * Worst-case detection latency = poll_interval_ms x debounce_samples
   *
   * @warning Lower intervals increase CPU load
   * @warning Higher intervals increase detection latency
   */
  uint32_t poll_interval_ms;

  /**< @brief Optional callback for obstacle state change events
   *
   * @details
   * Function pointer invoked when obstacles are detected or cleared.
   * Set to NULL if no callback needed.
   *
   * @par Valid Values: Function pointer or nullptr
   * @par Typical Usage: Logging, telemetry updates, UI notifications
   *
   * @see rx_obstacle_detect_callback_t Callback signature
   *
   * @warning Callback invoked from detection task - keep processing brief
   */
  rx_obstacle_detect_callback_t callback;

  /**< @brief User-defined context pointer passed to callback
   *
   * @details
   * Arbitrary user data passed through to callback function. Typically
   * points to application state structure. Set to NULL if not needed.
   *
   * @par Valid Values: Any pointer or nullptr
   * @par Lifetime: Must remain valid while detection is running
   *
   * @par Typical Usage:
   * - Pointer to application context struct
   * - Pointer to telemetry buffer
   * - Pointer to logging state
   *
   * @warning If non-NULL, must remain valid until deinit
   */
  void* user_data;
} rx_obstacle_detect_config_t;

/**
 * @brief Obstacle detection handle
 *
 * Maintains detection state and statistics. Caller allocates and passes to
 * init. Do not modify fields directly after initialization.
 */
typedef struct {
  /* Configuration (copied from config) */
  rx_hcsr04_t*                  sensors[k_obstacle_detect_max_sensors]; /**< Sensor handles */
  uint8_t                       sensor_count;                           /**< Number of sensors */
  rx_motor_handle_t*            motors[k_obstacle_detect_max_motors];   /**< Motor handles */
  uint8_t                       motor_count;                            /**< Number of motors */
  float                         detection_threshold_cm; /**< Stop distance threshold */
  uint32_t                      debounce_samples;       /**< Debounce sample count */
  uint32_t                      poll_interval_ms;       /**< Polling interval */
  rx_obstacle_detect_callback_t callback;               /**< Event callback */
  void*                         user_data;              /**< User context */

  /* ThreadX resources */
  TX_THREAD            thread; /**< Detection task thread */
  uint8_t              thread_stack[k_obstacle_detect_task_stack_size]; /**< Task stack */
  TX_EVENT_FLAGS_GROUP event_flags;                                     /**< Control event flags */

  /* State */
  bool                       initialized;    /**< True if initialized */
  rx_obstacle_detect_state_t state;          /**< Current detection state */
  bool                       stop_requested; /**< True if stop requested */

  /* Debouncing state (per-sensor) */
  uint32_t debounce_counter[k_obstacle_detect_max_sensors]; /**< Consecutive detections */
  bool     obstacle_active[k_obstacle_detect_max_sensors];  /**< Per-sensor obstacle state */

  /* Statistics */
  uint32_t total_polls;          /**< Total sensor polls performed */
  uint32_t obstacle_events;      /**< Total obstacle detection events */
  uint32_t false_positive_count; /**< Debounced false positives */
} rx_obstacle_detect_t;

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize obstacle detection system with sensors and motors
 *
 * @details
 * Initializes the obstacle detection system by creating a ThreadX task that
 * autonomously monitors HC-SR04 sensors and triggers emergency motor stops
 * when obstacles breach the configured safety threshold.
 *
 * **Initialization Sequence:**
 * 1. Validate input parameters (handle, config, sensor/motor arrays)
 * 2. Clear handle structure (memset to zero)
 * 3. Copy configuration parameters to handle
 * 4. Copy sensor and motor handle pointers
 * 5. Create ThreadX event flags for task control
 * 6. Create ThreadX detection task (priority 8, 2048 byte stack)
 * 7. Task starts suspended - call rx_obstacle_detect_start() to begin monitoring
 *
 * **Memory Allocation:**
 * - Handle structure: Caller-allocated (~2.6 KB static)
 * - ThreadX task stack: Inside handle (2048 bytes)
 * - ThreadX event flags: Inside handle (~32 bytes)
 * - No heap allocation (k_rx_ok)
 *
 * @param[out] handle Obstacle detection handle to initialize
 *   - Must be non-NULL
 *   - Caller-allocated (declare as rx_obstacle_detect_t)
 *   - Must NOT be already initialized
 *   - Cleared by this function
 *   - Remains valid until rx_obstacle_detect_deinit()
 *
 * @param[in] config Configuration structure
 *   - Must be non-NULL
 *   - All fields must be valid (checked by internal_validate_config)
 *   - Contents copied to handle (config can be stack-allocated)
 *   - Sensor/motor arrays must remain valid (only pointers copied)
 *
 * @return Initialization result
 * @retval k_rx_ok System initialized successfully, ready to start
 * @retval k_rx_err_null_ptr handle or config is nullptr
 * @retval k_rx_err_null_ptr config->sensors or config->motors is nullptr
 * @retval k_rx_err_null_ptr Any sensor or motor handle in arrays is nullptr
 * @retval k_rx_err_invalid_arg sensor_count not in [1, 8]
 * @retval k_rx_err_invalid_arg motor_count not in [1, 4]
 * @retval k_rx_err_invalid_arg threshold_cm not in [2.0, 400.0]
 * @retval k_rx_err_invalid_arg debounce_samples not in [1, 10]
 * @retval k_rx_err_invalid_arg poll_interval_ms not in [10, 1000]
 * @retval k_rx_err_invalid_state handle->initialized is already true
 * @retval k_rx_err_rtos_error ThreadX event flags creation failed
 * @retval k_rx_err_rtos_error ThreadX task creation failed
 *
 * @pre handle must point to valid rx_obstacle_detect_t storage
 * @pre config must point to valid configuration
 * @pre All sensors in config must be initialized via rx_hcsr04_init()
 * @pre All motors in config must be initialized via rx_motor_init()
 * @pre ThreadX must be running (tx_kernel_enter() completed)
 *
 * @post On success: handle->initialized = true
 * @post On success: ThreadX task created but suspended
 * @post On success: handle->state = k_obstacle_detect_state_stopped
 * @post On failure: handle unchanged (or partially cleared)
 * @post On failure: No ThreadX resources allocated
 *
 * @note Not thread-safe - do not call concurrently on same handle
 * @note Re-entrant for different handles
 * @note Task starts suspended - call rx_obstacle_detect_start() to begin
 *
 * @warning Do not modify handle fields after initialization
 * @warning Do not deinitialize sensors/motors while detection is running
 * @warning Ensure ThreadX kernel is running before calling
 *
 * @par Performance:
 * Execution time: ~100us (ThreadX task creation overhead)
 * Memory: 2.6 KB static (handle structure with embedded stack)
 *
 * @par Example:
 * @code{.c}
 * // Declare handle (static or global recommended for large struct)
 * static rx_obstacle_detect_t detector;
 *
 * // Setup sensor and motor arrays
 * rx_hcsr04_t* sensors[] = {&sensor_front, &sensor_rear};
 * rx_motor_handle_t* motors[] = {&motor_left, &motor_right};
 *
 * // Configure detection system
 * rx_obstacle_detect_config_t config = {
 *     .sensors = sensors,
 *     .sensor_count = 2,
 *     .motors = motors,
 *     .motor_count = 2,
 *     .detection_threshold_cm = 30.0f,
 *     .debounce_samples = 3,
 *     .poll_interval_ms = 20,
 *     .callback = my_callback,
 *     .user_data = &my_context
 * };
 *
 * // Initialize
 * rx_err_t ret = rx_obstacle_detect_init(&detector, &config);
 * if (ret != k_rx_ok) {
 *     printf("Init failed: %d\n", ret);
 *     return ret;
 * }
 *
 * // Start monitoring
 * ret = rx_obstacle_detect_start(&detector);
 * @endcode
 *
 * @see rx_obstacle_detect_config_t Configuration structure
 * @see rx_obstacle_detect_start() Start monitoring after init
 * @see rx_obstacle_detect_deinit() Cleanup and release resources
 * @see internal_validate_config() Internal configuration validation
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: [OK] No dynamic memory - all allocation is static
 * - Rule 5: [OK] Minimum 6 preconditions validated
 * - Rule 7: [OK] All ThreadX return values checked
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                               const rx_obstacle_detect_config_t* config);

/**
 * @brief Deinitialize obstacle detection system
 *
 * Stops detection task, releases ThreadX resources, and resets handle state.
 * Does NOT deinitialize the sensor or motor handles (caller's responsibility).
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_deinit(rx_obstacle_detect_t* handle);

/* =============================================================================
 * Public API - Control
 * =============================================================================
 */

/**
 * @brief Start obstacle detection monitoring
 *
 * Starts the detection task to begin polling sensors. If an obstacle is
 * already detected, motors will be stopped immediately.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle);

/**
 * @brief Stop obstacle detection monitoring
 *
 * Stops the detection task. Motors will NOT be automatically restarted even
 * if they were previously stopped due to obstacle detection.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_stop(rx_obstacle_detect_t* handle);

/**
 * @brief Resume motors after obstacle cleared
 *
 * Clears the obstacle state and allows motors to be controlled again.
 * This is a manual override - the user must verify the obstacle is truly
 * cleared before calling this function.
 *
 * If detection is still running and obstacle is still present, motors
 * will be stopped again on the next poll cycle.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_clear_obstacle(rx_obstacle_detect_t* handle);

/* =============================================================================
 * Public API - Status
 * =============================================================================
 */

/**
 * @brief Get current detection state
 *
 * Returns the current state of the obstacle detection system.
 *
 * @param[in]  handle    Obstacle detection handle
 * @param[out] out_state Current state
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or out_state is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_get_state(const rx_obstacle_detect_t* handle,
                                                    rx_obstacle_detect_state_t* out_state);

/**
 * @brief Check if obstacle is currently detected
 *
 * Returns true if any sensor currently detects an obstacle within the
 * threshold distance (after debouncing).
 *
 * @param[in] handle Obstacle detection handle
 *
 * @return true if obstacle detected
 * @return false if no obstacle or handle is nullptr/invalid
 */
[[nodiscard]] bool rx_obstacle_detect_is_obstacle_detected(const rx_obstacle_detect_t* handle);

/**
 * @brief Get obstacle detection statistics
 *
 * Returns statistics about detection performance and events.
 *
 * @param[in]  handle              Obstacle detection handle
 * @param[out] out_total_polls     Total sensor polls (can be NULL)
 * @param[out] out_obstacle_events Total obstacle events (can be NULL)
 * @param[out] out_false_positives Debounced false positives (can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_get_stats(const rx_obstacle_detect_t* handle,
                                                    uint32_t*                   out_total_polls,
                                                    uint32_t*                   out_obstacle_events,
                                                    uint32_t* out_false_positives);

/**
 * @brief Reset statistics counters
 *
 * Resets all statistics counters to zero.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_obstacle_detect_reset_stats(rx_obstacle_detect_t* handle);

/* =============================================================================
 * Internal Functions Exposed for Unit Testing
 * =============================================================================
 */

#ifdef UNIT_TEST
#include "rx_check.h"

/**
 * @brief Poll all sensors and update obstacle state (exposed for unit testing)
 * @param[in,out] handle Obstacle detection handle
 * @return k_rx_ok on success
 */
rx_err_t internal_poll_sensors(rx_obstacle_detect_t* handle);

/**
 * @brief Stop all configured motors (exposed for unit testing)
 * @param[in] handle Obstacle detection handle
 * @return k_rx_ok if all motors stopped
 */
rx_err_t internal_stop_all_motors(const rx_obstacle_detect_t* handle);

/**
 * @brief Invoke user callback if registered (exposed for unit testing)
 * @param[in] handle Obstacle detection handle
 * @param[in] obstacle_detected True if obstacle detected, false if cleared
 * @param[in] sensor_idx Sensor index that triggered the event
 * @param[in] distance_cm Measured distance in centimeters
 */
void internal_invoke_callback(const rx_obstacle_detect_t* handle,
                              bool                        obstacle_detected,
                              uint8_t                     sensor_idx,
                              float                       distance_cm);

/**
 * @brief Control the outer task loop in unit test builds
 *
 * @details
 * Sets the s_task_running flag that controls the outer while loop of
 * internal_detection_task_entry(). Set to false before calling the
 * task entry to make it execute exactly one outer iteration and then exit.
 *
 * @param[in] running True to keep the loop running, false to stop after one iteration
 */
void rx_obstacle_detect_test_set_task_running(bool running);

/**
 * @brief Exercise internal_detection_task_entry for unit test coverage
 *
 * @details
 * Calls internal_detection_task_entry with the provided handle cast to ULONG.
 * Exposed only in UNIT_TEST builds via this header.
 *
 * @param[in] input Handle cast to ULONG as the task entry expects
 */
void rx_obstacle_detect_test_run_task_entry(ULONG input);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
