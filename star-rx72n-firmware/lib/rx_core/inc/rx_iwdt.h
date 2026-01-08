/* lib/rx_core/inc/rx_iwdt.h */

/**
 * @file rx_iwdt.h
 * @brief Independent Watchdog Timer (IWDT) Driver for RX72N
 * @details
 * Enhanced IWDT driver with task monitoring, status reporting, and system
 * state-dependent timeouts. Provides safety-critical watchdog functionality
 * for detecting and recovering from system hangs, task deadlocks, and
 * communication failures.
 *
 * Features:
 * - Task registration and heartbeat monitoring
 * - System state-dependent timeout periods
 * - Watchdog status reporting with reset history
 * - Failed task tracking for post-mortem analysis
 * - Thread-safe operation with ThreadX
 * - Zero dynamic allocation (safety-critical)
 *
 * Hardware: RX72N Independent Watchdog Timer (IWDT)
 * - Register Base: 0x00088030
 * - Clock: IWDTCLK (independent from system clock)
 * - Timeout Range: ~128ms to ~16.4s
 * - Cannot be stopped once started (hardware safety feature)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_IWDT_H
#define STAR_RX72N_IWDT_H

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

/** @brief IWDT configuration constants */
typedef enum {
  k_iwdt_max_tasks          = 16,    /**< Maximum monitored tasks */
  k_iwdt_task_name_len      = 32,    /**< Max task name length */
  k_iwdt_default_timeout_ms = 2000,  /**< Default timeout (2s) */
  k_iwdt_min_timeout_ms     = 128,   /**< Minimum timeout */
  k_iwdt_max_timeout_ms     = 16384, /**< Maximum timeout */
  k_ms_per_second           = 1000,  /**< Milliseconds per second (time conversion) */
} iwdt_constants_t;

/** @brief IWDT timeout periods (hardware divider settings) */
typedef enum {
  k_iwdt_timeout_128ms  = 0, /**< ~128ms (CKS=00) */
  k_iwdt_timeout_512ms  = 1, /**< ~512ms (CKS=01) */
  k_iwdt_timeout_2048ms = 2, /**< ~2s (CKS=10) */
  k_iwdt_timeout_8192ms = 3, /**< ~8s (CKS=11) */
} iwdt_timeout_period_t;

/** @brief System states for state-dependent timeouts */
typedef enum {
  k_system_state_init         = 0, /**< System initialization */
  k_system_state_idle         = 1, /**< Idle state */
  k_system_state_running      = 2, /**< Normal operation */
  k_system_state_motor_active = 3, /**< Motor operation */
  k_system_state_comm_active  = 4, /**< Communication active */
  k_system_state_error        = 5, /**< Error recovery */
  k_system_state_count        = 6, /**< Number of states */
} system_state_t;

/** @brief Watchdog reset reasons */
typedef enum {
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
rx_err_t rx_iwdt_init(const rx_iwdt_config_t* config);

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
rx_err_t rx_iwdt_feed(void);

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
 * @retval k_rx_err_null_pointer task_name is NULL
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
rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms);

/**
 * @brief Report task heartbeat
 *
 * Updates the heartbeat timestamp for a registered task. Must be
 * called periodically faster than the task's timeout.
 *
 * @param[in] task_name Name of the task
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_pointer task_name is NULL
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
rx_err_t rx_iwdt_task_heartbeat(const char* task_name);

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
rx_err_t rx_iwdt_set_timeout_for_state(system_state_t state, uint32_t timeout_ms);

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
rx_err_t rx_iwdt_set_state(system_state_t state);

/**
 * @brief Get watchdog status
 *
 * Retrieves current watchdog status including reset history,
 * failed tasks, and monitoring statistics.
 *
 * @param[out] status Pointer to status structure
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_pointer status is NULL
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
rx_err_t rx_iwdt_get_status(rx_iwdt_status_t* status);

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
 *     uart_puts("[WARN] System recovered from watchdog reset\r\n");
 *     // Perform recovery actions
 * }
 * @endcode
 */
bool rx_iwdt_was_reset(void);

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
 *             uart_puts("[ERROR] Task timeout detected\r\n");
 *         }
 *         rx_iwdt_feed();  // Feed hardware watchdog
 *         tx_thread_sleep(10);  // 100ms check period
 *     }
 * }
 * @endcode
 */
rx_err_t rx_iwdt_check_tasks(void);

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

#endif /* STAR_RX72N_IWDT_H */
