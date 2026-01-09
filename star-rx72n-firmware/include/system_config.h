/* include/system_config.h */

/**
 * @file system_config.h
 * @brief System-wide configuration constants
 * @details
 * Defines ThreadX thread priorities, stack sizes, and timing constants.
 * Separated from motor_config.h for better organization.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

/* =============================================================================
 * Thread Priorities
 * =============================================================================
 */

/**
 * @brief ThreadX thread priorities
 * @details Lower number = higher priority
 *
 * Priority Rationale:
 * - Motor_Controller (2): Highest - deterministic control loop
 * - Comm_Manager (3): High - RPi5 is "High-Level Commander"
 * - Env_Monitor (5): Medium - Isolated from motor timing
 * - System_Health (9): Low - Battery changes slowly
 * - PMOD tasks (10-31): User-defined priorities
 */
typedef enum {
  k_priority_motor_controller = 2,  /**< Highest (deterministic) */
  k_priority_comm_manager     = 3,  /**< High (RPi5 commands) */
  k_priority_env_monitor      = 5,  /**< Medium (sensors) */
  k_priority_system_health    = 9,  /**< Low (diagnostics) */
  k_priority_pmod_start       = 10, /**< PMOD priority range start */
  k_priority_pmod_end         = 31, /**< PMOD priority range end */
} thread_priorities_t;

/* =============================================================================
 * Thread Stack Sizes
 * =============================================================================
 */

/**
 * @brief ThreadX thread stack sizes (bytes)
 * @details Statically allocated stacks (no dynamic memory)
 */
typedef enum {
  k_stack_motor_controller = 4096, /**< Motor control + PID */
  k_stack_comm_manager     = 4096, /**< USB/SPI + Protocol Buffers */
  k_stack_env_monitor      = 3072, /**< Sensors (HC-SR04, DS18B20) */
  k_stack_system_health    = 2048, /**< Battery monitoring + diagnostics */
} thread_stack_sizes_t;

/* =============================================================================
 * ThreadX Tick Conversion Constants
 * =============================================================================
 */

/**
 * @brief ThreadX tick conversion constants
 * @details Convert time periods to ThreadX ticks (100 Hz = 10ms per tick)
 *
 * Usage: tx_thread_sleep(k_threadx_ticks_20ms);
 *
 * NASA Power of 10 Rule 8: No magic numbers - all tick values must use
 * these named constants for clarity and maintainability.
 *
 * Note: For infinite timeout use TX_WAIT_FOREVER macro directly
 */
typedef enum {
  k_threadx_ticks_10ms  = 1,   /**< 10ms = 1 tick at 100Hz */
  k_threadx_ticks_20ms  = 2,   /**< 20ms = 2 ticks (Env_Monitor period) */
  k_threadx_ticks_80ms  = 8,   /**< 80ms = 8 ticks (DS18B20 conversion) */
  k_threadx_ticks_100ms = 10,  /**< 100ms = 10 ticks (Comm_Manager period) */
  k_threadx_ticks_1s    = 100, /**< 1 second = 100 ticks (System_Health period) */
} threadx_tick_constants_t;

#endif /* SYSTEM_CONFIG_H */
