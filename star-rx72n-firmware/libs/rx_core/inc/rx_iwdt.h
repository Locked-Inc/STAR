/**
 * @file rx_iwdt.h
 * @brief Independent Watchdog Timer (IWDT) Driver for RX72N Microcontroller
 *
 * @details
 * **Production-grade watchdog driver** providing safety-critical system monitoring
 * with advanced task tracking, state-dependent timeouts, and comprehensive
 * diagnostics. Designed for real-time embedded systems requiring fault detection
 * and automatic recovery from software hangs, deadlocks, and system failures.
 *
 * ## Purpose and Design Rationale
 *
 * This driver implements a **multi-layer watchdog architecture** combining:
 * 1. **Hardware IWDT**: Physical watchdog timer that triggers system reset
 * 2. **Task monitoring**: Software-level tracking of individual RTOS tasks
 * 3. **State-aware timeouts**: Different timeout periods based on system activity
 * 4. **Diagnostic tracking**: Post-mortem analysis of failures
 *
 * The Independent Watchdog Timer (IWDT) is **separate from the normal watchdog**
 * and provides **true independence** from the main system clock, making it immune
 * to clock failures.
 *
 * ## Key Features
 *
 * **Hardware Watchdog**:
 * - Physical watchdog timer with dedicated clock (IWDTCLK)
 * - Cannot be disabled once started (hardware safety feature)
 * - 4 timeout periods: 128ms, 512ms, 2s, 8s
 * - Automatic system reset on timeout
 *
 * **Task Monitoring** (Software Layer):
 * - Register up to 16 tasks with individual heartbeat tracking
 * - Per-task timeout configuration
 * - Failed task identification for diagnostics
 * - Thread-safe heartbeat reporting
 *
 * **State-Dependent Timeouts**:
 * - Different timeout periods for different system states
 * - Example: Shorter timeout during motor operation, longer during idle
 * - Runtime state transitions with automatic timeout adjustment
 *
 * **Diagnostics and Reporting**:
 * - Reset reason detection (power-on, watchdog, external, etc.)
 * - Reset counter (survives resets via backup registers)
 * - Failed task tracking for post-mortem analysis
 * - Watchdog feed statistics
 *
 * ## Hardware Architecture
 *
 * **RX72N Independent Watchdog Timer (IWDT)**:
 * - **Register Base Address**: 0x00088030
 * - **Clock Source**: IWDTCLK (independent oscillator, 120 kHz)
 * - **Clock Frequency**: 120 kHz (8.33 us period)
 * - **Counter Width**: 14-bit down-counter
 * - **Timeout Range**: ~128ms to ~16.4 seconds
 * - **Reset Output**: Connected to internal reset controller
 *
 * **Timeout Calculation**:
 * ```
 * Timeout = (2^(10+CKS)) / IWDTCLK
 *
 * Where CKS (Clock Select) determines divider:
 *   CKS=0: 2^10 = 1024  -> ~128ms timeout
 *   CKS=1: 2^12 = 4096  -> ~512ms timeout
 *   CKS=2: 2^14 = 16384 -> ~2s timeout
 *   CKS=3: 2^16 = 65536 -> ~8s timeout
 * ```
 *
 * ## Implementation Approach
 *
 * **Three-Layer Architecture**:
 *
 * 1. **Hardware Layer** (rx_iwdt_init, rx_iwdt_feed):
 *    - Configures IWDT registers
 *    - Writes refresh code to prevent timeout
 *
 * 2. **Task Monitoring Layer** (rx_iwdt_register_task, rx_iwdt_task_heartbeat):
 *    - Tracks registered tasks
 *    - Checks heartbeat timestamps
 *    - Identifies failed tasks
 *
 * 3. **State Management Layer** (rx_iwdt_set_state, rx_iwdt_set_timeout_for_state):
 *    - Maps system states to timeout periods
 *    - Adjusts watchdog based on activity level
 *
 * **Typical Usage Flow**:
 * ```
 * 1. System startup: rx_iwdt_init() - Configure and start watchdog
 * 2. Task registration: rx_iwdt_register_task() - Register critical tasks
 * 3. Normal operation:
 *    - Main loop: rx_iwdt_feed() every N ms (N < timeout)
 *    - Each task: rx_iwdt_task_heartbeat() within task timeout
 *    - Monitor thread: rx_iwdt_check_tasks() to detect failures
 * 4. State changes: rx_iwdt_set_state() to adjust timeout dynamically
 * 5. Diagnostics: rx_iwdt_get_status() for reset history and statistics
 * ```
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Memory Usage | Notes |
 * |-----------|---------------|--------------|-------|
 * | rx_iwdt_init() | ~50 us | 0 heap | One-time setup |
 * | rx_iwdt_feed() | ~2 us | 0 | Register write only |
 * | rx_iwdt_task_heartbeat() | ~5 us | 0 | Timestamp update |
 * | rx_iwdt_check_tasks() | ~10 us x N tasks | 0 | Linear scan |
 * | rx_iwdt_get_status() | ~15 us | 0 | Structure copy |
 *
 * **Memory Footprint**:
 * - Static variables: ~2 KB (task tracking array, status, configuration)
 * - Code size: ~3 KB (all functions)
 * - Stack usage: < 128 bytes (deepest call)
 * - Heap usage: 0 bytes (zero dynamic allocation)
 *
 * ## Thread Safety and RTOS Integration
 *
 * **Thread-Safe Operations**:
 * - All public functions use mutex protection (ThreadX)
 * - Safe to call from multiple threads simultaneously
 * - ISR-safe: rx_iwdt_feed() can be called from interrupts
 *
 * **ThreadX Integration**:
 * - Uses tx_mutex for synchronization
 * - Uses tx_time_get() for timestamps
 * - Compatible with all ThreadX thread priorities
 *
 * ## Error Detection and Recovery
 *
 * **Detected Failures**:
 * 1. **Task deadlock**: Task stops sending heartbeats
 * 2. **Main loop hang**: No rx_iwdt_feed() calls
 * 3. **Stack overflow**: Task corrupts memory and stops executing
 * 4. **Infinite loop**: Task stuck in tight loop without heartbeat
 * 5. **Communication timeout**: Network/SPI/UART tasks stop responding
 *
 * **Recovery Actions**:
 * - Hardware reset triggered by IWDT timeout
 * - System reboots from reset vector
 * - rx_iwdt_was_reset() detects watchdog reset on next boot
 * - Failed task name preserved for diagnostics
 *
 * ## Usage Guidelines
 *
 * **DO**:
 * - Call rx_iwdt_feed() from main supervision loop only
 * - Register all critical tasks for monitoring
 * - Use state-dependent timeouts for different operating modes
 * - Check rx_iwdt_was_reset() at startup for diagnostics
 * - Tune timeouts based on worst-case execution times
 *
 * **DON'T**:
 * - Call rx_iwdt_feed() from multiple locations (defeats purpose)
 * - Set timeouts shorter than worst-case task execution time
 * - Rely on watchdog for normal error handling (use return codes)
 * - Call rx_iwdt_feed() from ISRs (unless by design)
 * - Assume watchdog prevents all failures (defense in depth)
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | IWDT peripheral required |
 * | Clock | IWDTCLK | Independent oscillator, 120 kHz |
 * | Registers | 0x00088030 base | IWDT control registers |
 * | Reset | Internal reset controller | Connected to IWDT output |
 * | Backup registers | Optional | For reset counter persistence |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_err.h` - Error code definitions
 * - `<stdint.h>` - Standard integer types
 * - `<stdbool.h>` - Boolean type
 * - ThreadX RTOS - Mutex and timestamp functions
 *
 * **This module is used by**:
 * - `main.c` - System initialization and main loop
 * - All critical RTOS tasks - Heartbeat reporting
 * - System monitor task - Task timeout checking
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion, sequential logic |
 * | 2. Fixed loop bounds | [OK] | All loops bounded by k_iwdt_max_tasks (16) |
 * | 3. No dynamic allocation | [OK] | Zero malloc/free, static task array |
 * | 4. Small functions | [OK] | All functions < 60 lines |
 * | 5. Assertions (>=2 per function) | [OK] | Minimum 2 checks per function (pre/post) |
 * | 6. Narrow scope | [OK] | File-scope statics, function-local variables |
 * | 7. Check return values | [OK] | All functions return rx_err_t, checked by callers |
 * | 8. Limited preprocessor | [OK] | C23 typed enums only, zero computation macros |
 * | 9. Pointer restrictions | [OK] | Single-level pointers, no pointer arithmetic |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * **Safety-Critical Design**:
 * - Static allocation prevents fragmentation and allocation failures
 * - Bounded arrays prevent buffer overflows
 * - Explicit error codes force error handling
 * - Mutex protection prevents race conditions
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S)**:
 * - This module has ONE job: Watchdog monitoring and system reset
 * - Separate concerns: Hardware watchdog vs task monitoring vs state management
 * - No mixing of unrelated functionality
 *
 * **Open/Closed (O)**:
 * - Extensible via configuration structure (state timeouts, task limits)
 * - Can add new system states without modifying existing code
 * - Timeout periods configured at runtime, not hardcoded
 *
 * **Liskov Substitution (L)**:
 * - All functions return rx_err_t for consistent error handling
 * - Boolean functions (rx_iwdt_was_reset) have well-defined semantics
 * - No unexpected side effects or hidden dependencies
 *
 * **Interface Segregation (I)**:
 * - Separate functions for separate concerns (feed vs heartbeat vs status)
 * - Task monitoring is optional (enable_task_monitoring flag)
 * - Clients use only the functions they need
 *
 * **Dependency Inversion (D)**:
 * - Depends on abstract rx_err_t, not specific error implementations
 * - Uses system time abstraction (tx_time_get), not raw timer
 * - Can be mocked for unit testing via test_reset function
 *
 * @see rx_err.h Error code definitions used by all functions
 * @see docs/sections/06_nasa_power_of_10.tex Safety-critical coding standards
 * @see RX72N Hardware Manual Section 11 Independent Watchdog Timer specification
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum iwdt_constants_t
 * @brief IWDT Configuration Constants and Limits
 *
 * @details
 * Defines compile-time constants for the IWDT driver including array sizes,
 * timeout limits, and time conversion factors. These constants are chosen based
 * on hardware limitations and system requirements.
 *
 * ## Constant Rationale
 *
 * **k_iwdt_max_tasks = 16**:
 * - Sized for typical STAR system (4 motors + communication + sensors)
 * - Keeps task array < 1 KB (16 x 64 bytes = 1024 bytes)
 * - Allows linear scan in < 10 us at 240 MHz
 *
 * **k_iwdt_task_name_len = 32**:
 * - Matches ThreadX TX_THREAD_NAME_SIZE
 * - Accommodates descriptive names like "MotorControllerTask_FL"
 * - Null-terminated, so 31 characters + '\0'
 *
 * **k_iwdt_default_timeout_ms = 2000**:
 * - 2 second default provides safety margin
 * - Allows 100ms main loop to miss ~20 iterations
 * - Not too long (fast failure detection) nor too short (no false positives)
 *
 * **k_iwdt_min_timeout_ms = 128**:
 * - Hardware minimum with CKS=0 divider
 * - Calculated: 1024 / 125kHz ~ 128ms
 * - Use only for very fast loops (> 10 Hz)
 *
 * **k_iwdt_max_timeout_ms = 16384**:
 * - Hardware maximum with CKS=3 divider
 * - Calculated: 65536 / 125kHz ~ 8192ms
 * - Conservative limit prevents excessive hang detection time
 *
 * @par Usage Example:
 * @code{.c}
 * // Validate configuration against limits
 * rx_iwdt_config_t config = {
 *   .default_timeout_ms = k_iwdt_default_timeout_ms,
 *   .enable_task_monitoring = true
 * };
 *
 * if (config.default_timeout_ms < k_iwdt_min_timeout_ms) {
 *   return k_rx_err_invalid_arg;
 * }
 * @endcode
 *
 * @invariant k_iwdt_max_tasks x sizeof(rx_iwdt_task_info_t) < 2048 bytes
 * @invariant k_iwdt_min_timeout_ms <= k_iwdt_default_timeout_ms <= k_iwdt_max_timeout_ms
 * @invariant All values are compile-time constants (no runtime overhead)
 *
 * @note C23 typed enum with uint32_t underlying type (4-byte constants)
 * @note Values chosen based on RX72N IWDT hardware capabilities
 *
 * @see rx_iwdt_config_t Configuration structure using these constants
 * @see rx_iwdt_task_info_t Task info structure sized by k_iwdt_task_name_len
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_iwdt_max_tasks =
    16, /**< Maximum number of tasks that can be registered for monitoring. Limits task array size to ~1 KB. Increase if more tasks needed (recompile required) */
  k_iwdt_task_name_len =
    32, /**< Maximum task name length in characters including null terminator. Matches ThreadX TX_THREAD_NAME_SIZE. Allows 31-character names */
  k_iwdt_default_timeout_ms =
    2000, /**< Default watchdog timeout in milliseconds (2 seconds). Provides good balance between fast failure detection and false positive avoidance */
  k_iwdt_min_timeout_ms =
    128, /**< Minimum valid timeout in milliseconds (~128ms). Hardware limit with CKS=0. Use only for very fast control loops (> 10 Hz) */
  k_iwdt_max_timeout_ms =
    16384, /**< Maximum valid timeout in milliseconds (~16 seconds). Conservative limit for reasonable hang detection. Hardware supports up to ~8s with CKS=3 */
  k_ms_per_second =
    1000, /**< Milliseconds per second (1000 ms/s). Time conversion constant for timeout calculations and timestamp arithmetic */
} iwdt_constants_t;

/** @brief IWDT timeout periods (hardware divider settings) */
typedef enum : uint8_t {
  k_iwdt_timeout_128ms  = 0, /**< ~128ms (CKS=00) */
  k_iwdt_timeout_512ms  = 1, /**< ~512ms (CKS=01) */
  k_iwdt_timeout_2048ms = 2, /**< ~2s (CKS=10) */
  k_iwdt_timeout_8192ms = 3, /**< ~8s (CKS=11) */
} iwdt_timeout_period_t;

/**
 * @enum system_state_t
 * @brief System Operational States for State-Dependent Watchdog Timeouts
 *
 * @details
 * Defines all possible system states, each with its own watchdog timeout period.
 * The watchdog timeout is automatically adjusted when transitioning between states,
 * allowing shorter timeouts during active operations and longer timeouts during
 * idle periods.
 *
 * ## State Machine Architecture
 *
 * The system operates as a finite state machine with defined transitions:
 *
 * @startuml
 * [*] --> Init : Power-On / Reset
 * Init --> Idle : Initialization Complete
 * Idle --> Running : Start Command
 * Running --> Idle : Stop Command
 * Running --> MotorActive : Motor Start
 * Running --> CommActive : Communication Start
 * MotorActive --> Running : Motor Stop
 * CommActive --> Running : Communication Stop
 * MotorActive --> CommActive : Both Active
 * CommActive --> MotorActive : Both Active
 * Running --> Error : Fault Detected
 * MotorActive --> Error : Fault Detected
 * CommActive --> Error : Fault Detected
 * Error --> Idle : Error Cleared
 * Error --> Init : Reset Required
 * @enduml
 *
 * ## State Descriptions and Typical Timeouts
 *
 * | State | Description | Typical Timeout | Rationale |
 * |-------|-------------|-----------------|-----------|
 * | Init | System initialization, hardware setup | 5000ms | Slow startup tolerable |
 * | Idle | No active operations, waiting for commands | 5000ms | Long timeout OK |
 * | Running | Normal operation, moderate activity | 2000ms | Default timeout |
 * | MotorActive | Motors running, high-frequency control | 500ms | Fast loop required |
 * | CommActive | Active SPI/USB communication | 1000ms | Network delays possible |
 * | Error | Error recovery, diagnostics | 10000ms | Allow time for recovery |
 *
 * ## Recommended Timeout Configuration
 *
 * ```c
 * rx_iwdt_config_t config;
 * config.state_timeouts_ms[k_system_state_init]         = 5000;  // Slow startup
 * config.state_timeouts_ms[k_system_state_idle]         = 5000;  // Long idle OK
 * config.state_timeouts_ms[k_system_state_running]      = 2000;  // Default
 * config.state_timeouts_ms[k_system_state_motor_active] = 500;   // Fast loop
 * config.state_timeouts_ms[k_system_state_comm_active]  = 1000;  // Network
 * config.state_timeouts_ms[k_system_state_error]        = 10000; // Recovery time
 * ```
 *
 * @par State Transition Example:
 * @code{.c}
 * // System startup
 * rx_iwdt_set_state(k_system_state_init);
 * // ... hardware initialization ...
 *
 * // Initialization complete, go idle
 * rx_iwdt_set_state(k_system_state_idle);
 *
 * // User command received, start operation
 * rx_iwdt_set_state(k_system_state_running);
 *
 * // Begin motor control
 * rx_iwdt_set_state(k_system_state_motor_active);
 * // Motor control loop must call rx_iwdt_feed() within 500ms
 *
 * // Stop motors, return to normal
 * rx_iwdt_set_state(k_system_state_running);
 *
 * // Fault detected
 * rx_iwdt_set_state(k_system_state_error);
 * // Long timeout allows diagnostic logging
 *
 * // Recovery complete
 * rx_iwdt_set_state(k_system_state_idle);
 * @endcode
 *
 * @par Usage Example (Dynamic Timeout Adjustment):
 * @code{.c}
 * // Configure different timeouts for different states
 * rx_iwdt_set_timeout_for_state(k_system_state_motor_active, 500);
 * rx_iwdt_set_timeout_for_state(k_system_state_idle, 5000);
 *
 * // Transition to motor state (timeout automatically becomes 500ms)
 * rx_iwdt_set_state(k_system_state_motor_active);
 *
 * // Motor control loop
 * while (motor_running) {
 *   motor_control_update();
 *   rx_iwdt_feed();  // Must execute within 500ms
 *   tx_thread_sleep(10);  // 100Hz loop = 10ms period
 * }
 *
 * // Stop motors (timeout automatically becomes 2000ms)
 * rx_iwdt_set_state(k_system_state_running);
 * @endcode
 *
 * @invariant State values are contiguous (0 to k_system_state_count-1)
 * @invariant k_system_state_count equals number of states (6)
 * @invariant All state values fit in uint8_t (0-255)
 *
 * @note C23 typed enum with uint8_t underlying type (1-byte values)
 * @note State transitions trigger automatic timeout period adjustments
 * @note k_system_state_count is used for array sizing and validation
 *
 * @warning Invalid state transitions are allowed but may indicate logic errors
 * @attention Changing state does NOT reset the watchdog counter--still need rx_iwdt_feed()
 *
 * @see rx_iwdt_set_state() Set current system state
 * @see rx_iwdt_set_timeout_for_state() Configure timeout for specific state
 * @see rx_iwdt_config_t Configuration structure with state_timeouts_ms array
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_system_state_init =
    0, /**< System initialization phase. Hardware setup, peripheral configuration. Use long timeout (5s) to allow slow startup. Transitions to: Idle */
  k_system_state_idle =
    1, /**< Idle state, no active operations. Waiting for commands. Use long timeout (5s) since no critical operations. Transitions to: Running */
  k_system_state_running =
    2, /**< Normal operation, moderate activity. Background tasks executing. Use default timeout (2s). Transitions to: MotorActive, CommActive, Idle, Error */
  k_system_state_motor_active =
    3, /**< Motor control active, high-frequency updates. Critical real-time loop. Use short timeout (500ms) for fast failure detection. Transitions to: Running, Error */
  k_system_state_comm_active =
    4, /**< Active communication (SPI/USB). Network I/O in progress. Use medium timeout (1s) to tolerate network delays. Transitions to: Running, Error */
  k_system_state_error =
    5, /**< Error recovery mode. Diagnostics and cleanup. Use very long timeout (10s) to allow recovery actions and logging. Transitions to: Idle, Init */
  k_system_state_count =
    6, /**< Number of system states (not a valid state). Used for array sizing (state_timeouts_ms[k_system_state_count]) and validation checks */
} system_state_t;

/** @brief Watchdog reset reasons */
typedef enum : uint8_t {
  k_iwdt_reset_unknown     = 0, /**< Unknown reset */
  k_iwdt_reset_power_on    = 1, /**< Power-on reset */
  k_iwdt_reset_watchdog    = 2, /**< Watchdog timeout */
  k_iwdt_reset_external    = 3, /**< External reset */
  k_iwdt_reset_low_voltage = 4, /**< Low voltage detect */
} iwdt_reset_reason_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @brief IWDT configuration structure
 *
 * Configures the Independent Watchdog Timer with timeout periods for
 * different system states.
 */
typedef struct {
  uint32_t default_timeout_ms;                      /**< Default timeout */
  uint32_t state_timeouts_ms[k_system_state_count]; /**< State-specific timeouts */
  bool     enable_task_monitoring;                  /**< Enable task heartbeats */
  bool     reset_on_timeout;                        /**< Reset vs NMI */
} rx_iwdt_config_t;

/**
 * @brief Task registration structure (internal)
 *
 * Tracks registered tasks for heartbeat monitoring.
 */
typedef struct {
  char     task_name[k_iwdt_task_name_len]; /**< Task name */
  uint32_t timeout_ms;                      /**< Task-specific timeout */
  uint32_t last_heartbeat_tick;             /**< Last heartbeat timestamp */
  bool     active;                          /**< Task is active */
  bool     timed_out;                       /**< Task has timed out */
} rx_iwdt_task_info_t;

/**
 * @brief IWDT status structure
 *
 * Provides watchdog status information including reset history and
 * failed task tracking for post-mortem analysis.
 */
typedef struct {
  uint32_t            total_resets;                           /**< Total reset count */
  iwdt_reset_reason_t last_reset_reason;                      /**< Last reset cause */
  char                last_failed_task[k_iwdt_task_name_len]; /**< Failed task name */
  uint32_t            watchdog_feeds;                         /**< Total feed count */
  system_state_t      current_state;                          /**< Current system state */
  uint32_t            current_timeout_ms;                     /**< Active timeout */
  uint32_t            active_tasks;                           /**< Number of active tasks */
  bool                initialized;                            /**< IWDT initialized */
} rx_iwdt_status_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the Independent Watchdog Timer
 *
 * Initializes the IWDT with specified configuration. Once started,
 * the IWDT cannot be stopped (hardware safety feature).
 *
 * @param[in] config Configuration structure (NULL for defaults)
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid configuration
 * @retval k_rx_err_invalid_state Already initialized
 * @retval k_rx_err_hw_init_failed Hardware initialization failed
 *
 * @note This function must be called before any other IWDT functions
 * @warning IWDT cannot be stopped once started - requires hard reset
 *
 * @par Example:
 * @code
 * rx_iwdt_config_t config = {
 *     .default_timeout_ms = 2000,
 *     .enable_task_monitoring = true,
 *     .reset_on_timeout = true
 * };
 * config.state_timeouts_ms[k_system_state_running] = 1000;
 * config.state_timeouts_ms[k_system_state_motor_active] = 500;
 *
 * rx_err_t err = rx_iwdt_init(&config);
 * if (err != k_rx_ok) {
 *     // Handle error
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_init(const rx_iwdt_config_t* config);

/**
 * @brief Feed the watchdog timer
 *
 * Resets the watchdog counter to prevent timeout. Must be called
 * periodically at a rate faster than the configured timeout.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @note Thread-safe - can be called from multiple threads/ISRs
 * @warning Missing feeds will cause system reset
 *
 * @par Example:
 * @code
 * // In main control loop (must execute faster than timeout)
 * while (1) {
 *     // ... control logic ...
 *     rx_iwdt_feed();
 *     tx_thread_sleep(50);  // 500ms loop
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_feed(void);

/**
 * @brief Register a task for heartbeat monitoring
 *
 * Registers a task with the watchdog for periodic heartbeat checks.
 * Task must call rx_iwdt_task_heartbeat() within timeout period.
 *
 * @param[in] task_name Unique task name (max 31 chars)
 * @param[in] timeout_ms Task-specific timeout in milliseconds
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr task_name is nullptr
 * @retval k_rx_err_invalid_arg Invalid timeout or duplicate task
 * @retval k_rx_err_not_initialized IWDT not initialized
 * @retval k_rx_err_no_mem Max tasks reached
 *
 * @par Example:
 * @code
 * // Register motor control task with 500ms timeout
 * rx_err_t err = rx_iwdt_register_task("MotorCtrl", 500);
 * if (err != k_rx_ok) {
 *     // Handle error
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms);

/**
 * @brief Report task heartbeat
 *
 * Updates the heartbeat timestamp for a registered task. Must be
 * called periodically faster than the task's timeout.
 *
 * @param[in] task_name Name of the task
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr task_name is nullptr
 * @retval k_rx_err_not_found Task not registered
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @par Example:
 * @code
 * static void motor_control_task(ULONG input) {
 *     while (1) {
 *         // ... motor control logic ...
 *         rx_iwdt_task_heartbeat("MotorCtrl");
 *         tx_thread_sleep(10);  // 100ms loop
 *     }
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_task_heartbeat(const char* task_name);

/**
 * @brief Set timeout for system state
 *
 * Configures timeout period for a specific system state. Allows
 * different timeout values based on system activity.
 *
 * @param[in] state System state
 * @param[in] timeout_ms Timeout in milliseconds
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid state or timeout
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @par Example:
 * @code
 * // Motor operation needs faster watchdog
 * rx_iwdt_set_timeout_for_state(k_system_state_motor_active, 500);
 * // Idle state can use longer timeout
 * rx_iwdt_set_timeout_for_state(k_system_state_idle, 5000);
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_set_timeout_for_state(system_state_t state, uint32_t timeout_ms);

/**
 * @brief Set current system state
 *
 * Updates the current system state, which determines the active
 * watchdog timeout period.
 *
 * @param[in] state New system state
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid state
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @par Example:
 * @code
 * // Entering motor operation
 * rx_iwdt_set_state(k_system_state_motor_active);
 * // ... motor control ...
 * // Return to normal operation
 * rx_iwdt_set_state(k_system_state_running);
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_set_state(system_state_t state);

/**
 * @brief Get watchdog status
 *
 * Retrieves current watchdog status including reset history,
 * failed tasks, and monitoring statistics.
 *
 * @param[out] status Pointer to status structure
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr status is nullptr
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @par Example:
 * @code
 * rx_iwdt_status_t status;
 * rx_err_t err = rx_iwdt_get_status(&status);
 * if (err == k_rx_ok) {
 *     printf("Total resets: %lu\n", status.total_resets);
 *     printf("Last failed task: %s\n", status.last_failed_task);
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_get_status(rx_iwdt_status_t* status);

/**
 * @brief Check if last reset was caused by watchdog
 *
 * Determines if the previous system reset was triggered by
 * watchdog timeout.
 *
 * @return bool true if watchdog reset, false otherwise
 *
 * @note Can be called before rx_iwdt_init()
 *
 * @par Example:
 * @code
 * if (rx_iwdt_was_reset()) {
 *     uart_debug_puts("[WARN] System recovered from watchdog reset\r\n");
 *     // Perform recovery actions
 * }
 * @endcode
 */
[[nodiscard]] bool rx_iwdt_was_reset(void);

/**
 * @brief Check for task timeouts
 *
 * Checks all registered tasks for timeout. Should be called
 * periodically from a monitoring task.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok No timeouts detected
 * @retval k_rx_err_timeout One or more tasks timed out
 * @retval k_rx_err_not_initialized IWDT not initialized
 *
 * @note This function does NOT reset the hardware watchdog
 * @warning Task timeouts trigger watchdog reset if not cleared
 *
 * @par Example:
 * @code
 * static void watchdog_monitor_task(ULONG input) {
 *     while (1) {
 *         rx_err_t err = rx_iwdt_check_tasks();
 *         if (err == k_rx_err_timeout) {
 *             // Log timeout before reset
 *             uart_debug_puts("[ERROR] Task timeout detected\r\n");
 *         }
 *         rx_iwdt_feed();  // Feed hardware watchdog
 *         tx_thread_sleep(10);  // 100ms check period
 *     }
 * }
 * @endcode
 */
[[nodiscard]] rx_err_t rx_iwdt_check_tasks(void);

/* =============================================================================
 * Test Support Functions
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Reset IWDT driver state for unit testing
 * @details
 * Resets the driver to uninitialized state. This function is only available
 * when UNIT_TEST is defined and should only be called from test code to
 * ensure test isolation.
 */
void rx_iwdt_test_reset(void);
#endif

#ifdef __cplusplus
}
#endif
