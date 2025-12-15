#ifndef WATCHDOG_TASK_H
#define WATCHDOG_TASK_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

/*
 * System Watchdog Task
 * Monitors system health and handles recovery from fault conditions
 * Implements graceful degradation and automatic recovery strategies
 */

typedef enum {
  k_watchdog_health_good,
  k_watchdog_health_degraded,
  k_watchdog_health_critical,
  k_watchdog_health_fault
} watchdog_health_level_t;

typedef struct {
  TaskHandle_t            task_handle;
  bool                    task_running;
  uint32_t                health_checks_performed;
  uint32_t                recovery_actions_taken;
  watchdog_health_level_t last_health_level;
} watchdog_task_context_t;

/**
 * @brief Start the system watchdog task
 * 
 * Creates and starts the FreeRTOS task for system health monitoring.
 * The task will periodically check system health and take recovery actions
 * when necessary.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t watchdog_task_start(void);

/**
 * @brief Stop the system watchdog task
 * 
 * Cleanly shuts down the watchdog task.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t watchdog_task_stop(void);

/**
 * @brief Check if watchdog task is running
 * 
 * @return true if task is active and monitoring system health
 */
bool watchdog_task_is_running(void);

/**
 * @brief Get watchdog task statistics
 * 
 * Returns information about watchdog performance and recovery actions.
 * 
 * @param health_checks_performed Number of health checks performed
 * @param recovery_actions_taken Number of recovery actions taken
 * @param last_health_level Last assessed system health level
 * @return true if statistics are available
 */
bool watchdog_task_get_stats(uint32_t*                health_checks_performed,
                             uint32_t*                recovery_actions_taken,
                             watchdog_health_level_t* last_health_level);

/**
 * @brief Force immediate system health check
 * 
 * Triggers an immediate health assessment and recovery action if needed.
 * 
 * @return watchdog_health_level_t Current system health level
 */
watchdog_health_level_t watchdog_task_force_health_check(void);

/**
 * @brief Register a task for health monitoring
 * 
 * Adds a task to the watchdog's monitoring list.
 * 
 * @param task_handle Task handle to monitor
 * @param max_response_time_ms Maximum expected response time
 * @return esp_err_t ESP_OK on success
 */
esp_err_t watchdog_task_register_task(const TaskHandle_t task_handle,
                                      const uint32_t     max_response_time_ms);

#endif /* WATCHDOG_TASK_H */