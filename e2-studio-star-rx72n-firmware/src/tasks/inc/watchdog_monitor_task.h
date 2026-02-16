/* src/tasks/inc/watchdog_monitor_task.h */

/**
 * @file watchdog_monitor_task.h
 * @brief Watchdog Monitor Task - IWDT supervision and system health monitoring
 *
 * @details
 * Dedicated high-priority task that ensures system liveness by:
 * 1. Checking all registered tasks for heartbeat timeouts
 * 2. Feeding the hardware Independent Watchdog Timer (IWDT)
 * 3. Reporting own heartbeat for self-monitoring
 *
 * ## Architecture
 *
 * - **Priority**: 6 (between Communication @ 5 and Motor Control @ 8)
 * - **Period**: 10ms (100 Hz)
 * - **Stack**: 512 bytes
 * - **Responsibilities**: Task timeout detection + hardware watchdog feeding
 *
 * ## Safety Model
 *
 * - Hardware IWDT triggers reset after 2 seconds without feed
 * - Task monitoring detects individual task failures within 30-3000ms
 * - Dual-layer protection: software (task) + hardware (IWDT)
 *
 * ## Task Priority Hierarchy
 *
 * | Priority | Task | Period |
 * |----------|------|--------|
 * | 5 | Communication | 10ms |
 * | **6** | **Watchdog Monitor** | **10ms** |
 * | 8 | Motor Control | 10ms |
 * | 12 | Obstacle Detection | 20ms |
 * | 15 | BMS Monitor | 1000ms |
 * | 15 | Temperature Sensor | 1000ms |
 * | 17 | LED Status | 50ms |
 * | 18 | Telemetry | 50ms |
 *
 * ## Initialization Sequence
 *
 * ```
 * tx_application_define()
 *   ├─ rx_iwdt_init(&config)              [Initialize hardware]
 *   ├─ rx_iwdt_register_task("CommTask", 30)  [Register all tasks]
 *   ├─ ...
 *   ├─ rx_iwdt_set_state(k_system_state_init)
 *   ├─ Create all tasks (including watchdog monitor)
 *   └─ rx_iwdt_set_state(k_system_state_running)
 * ```
 *
 * ## Main Loop Logic
 *
 * ```
 * while (1) {
 *   check_tasks();   // Detect task timeouts
 *   feed();          // Feed hardware watchdog
 *   heartbeat();     // Self-report
 *   sleep(1 tick);   // 10ms @ 100 Hz
 * }
 * ```
 *
 * @par Usage Example:
 * @code
 * // In tx_application_define()
 * rx_err_t err;
 *
 * // Step 1: Initialize IWDT
 * err = rx_iwdt_init(&config);
 * RX_ASSERT(err == k_rx_ok, "IWDT init failed");
 *
 * // Step 2: Register all tasks
 * err = rx_iwdt_register_task("CommTask", 30);
 * RX_ASSERT(err == k_rx_ok, "Task registration failed");
 * // ... register other tasks ...
 *
 * // Step 3: Create watchdog monitor task (AFTER registration)
 * err = watchdog_monitor_task_create();
 * RX_ASSERT(err == k_rx_ok, "Watchdog task creation failed");
 * @endcode
 *
 * @see rx_iwdt.h IWDT driver API
 * @see main.c System initialization and task creation
 *
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and start the watchdog monitor task
 *
 * @details
 * Creates a dedicated task that runs at 100 Hz to supervise system health.
 * Must be called AFTER rx_iwdt_init() and task registration.
 *
 * **Task configuration**:
 * - Name: "WatchdogMon"
 * - Priority: 6 (high priority, below comm, above motor control)
 * - Stack: 512 bytes
 * - Period: 10ms (100 Hz)
 *
 * **Main loop**:
 * ```
 * while (1) {
 *   rx_iwdt_check_tasks();  // Detect task timeouts
 *   rx_iwdt_feed();         // Feed hardware watchdog
 *   rx_iwdt_task_heartbeat("WatchdogMon");  // Self-report
 *   tx_thread_sleep(1);     // 10ms @ 100 Hz
 * }
 * ```
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_invalid_state Task already created
 * @retval k_rx_err_rtos_thread_create ThreadX task creation failed
 *
 * @pre IWDT initialized via rx_iwdt_init()
 * @pre All tasks registered via rx_iwdt_register_task()
 * @post Watchdog monitor running at Priority 6, 100 Hz
 *
 * @note Must be called from tx_application_define() during system init
 * @warning Task creation failure is fatal - RX_ASSERT in caller
 *
 * @see rx_iwdt_init() Initialize IWDT before calling this
 * @see rx_iwdt_register_task() Register tasks before creating monitor
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t watchdog_monitor_task_create(void);

#ifdef __cplusplus
}
#endif
