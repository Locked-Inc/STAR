#include "include/watchdog_task.h"

#include <esp_log.h>
#include <esp_system.h>
#include <string.h>

#include "../include/system_config.h"
#include "../modules/include/shared_data.h"
#include "include/dht22_task.h"
#include "include/led_task.h"
#include "include/sensor_task.h"

static const char* s_TAG = TAG_WATCHDOG;

/* Task context - single instance per system */
static watchdog_task_context_t s_task_context          = {0};
static bool                    s_g_context_initialized = false;

/* ========== Private Functions ========== */

static watchdog_health_level_t internal_assess_sensor_health(void)
{
  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx == NULL) {
    return WATCHDOG_HEALTH_FAULT;
  }

  uint32_t hcsr04_failures = 0;
  uint32_t hcsr04_reads    = 0;
  uint32_t temp_failures   = 0;
  uint32_t temp_reads      = 0;

  if (xSemaphoreTake(shared_ctx->health_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    for (uint8_t i = 0; i < NUM_HCSR04; i++) {
      hcsr04_failures += shared_ctx->health_data.sensor_read_failures[i];
    }
    temp_failures = shared_ctx->health_data.temperature_read_failures;
    hcsr04_reads = shared_ctx->health_data.total_sensor_reads;
    temp_reads = shared_ctx->health_data.total_temperature_reads;

    xSemaphoreGive(shared_ctx->health_mutex);
  } else {
    ESP_LOGE(s_TAG, "Failed to acquire health mutex for sensor assessment");
    return WATCHDOG_HEALTH_FAULT;
  }

  // Assess HC-SR04 sensors (critical for operation)
  watchdog_health_level_t hcsr04_health = WATCHDOG_HEALTH_GOOD;
  if (hcsr04_reads > 0) {
    float hcsr04_failure_rate = (float)hcsr04_failures / hcsr04_reads;
    
    if (hcsr04_failure_rate > SENSOR_FAILURE_RATE_CRITICAL) {
      ESP_LOGE(s_TAG, "Critical HC-SR04 failure rate: %.1f%%", hcsr04_failure_rate * 100);
      hcsr04_health = WATCHDOG_HEALTH_CRITICAL;
    } else if (hcsr04_failure_rate > SENSOR_FAILURE_RATE_DEGRADED) {
      ESP_LOGW(s_TAG, "Elevated HC-SR04 failure rate: %.1f%%", hcsr04_failure_rate * 100);
      hcsr04_health = WATCHDOG_HEALTH_DEGRADED;
    } else {
      ESP_LOGI(s_TAG, "HC-SR04 health good: %.1f%% failure rate", hcsr04_failure_rate * 100);
    }
  } else {
    ESP_LOGW(s_TAG, "No HC-SR04 readings available for health assessment");
    hcsr04_health = WATCHDOG_HEALTH_DEGRADED;
  }

  // Assess DHT22 sensor (optional - more tolerant)
  watchdog_health_level_t temp_health = WATCHDOG_HEALTH_GOOD;
  if (temp_reads > 0) {
    float temp_failure_rate = (float)temp_failures / temp_reads;
    
    // DHT22 is optional - only flag as critical if it was working then failed
    if (temp_failure_rate == 1.0f) {
      ESP_LOGD(s_TAG, "DHT22 completely failed (likely not connected): %.1f%%", temp_failure_rate * 100);
      // Don't degrade system for completely failed optional sensor
    } else if (temp_failure_rate > 0.8f) {
      ESP_LOGW(s_TAG, "DHT22 mostly failing: %.1f%%", temp_failure_rate * 100);
      temp_health = WATCHDOG_HEALTH_DEGRADED;
    } else {
      ESP_LOGI(s_TAG, "DHT22 health good: %.1f%% failure rate", temp_failure_rate * 100);
    }
  } else {
    ESP_LOGD(s_TAG, "No DHT22 readings (sensor likely not connected)");
  }

  // Overall health is the worst of the critical sensors (HC-SR04)
  // DHT22 can degrade but shouldn't cause critical state if not connected
  if (hcsr04_health == WATCHDOG_HEALTH_CRITICAL) {
    return WATCHDOG_HEALTH_CRITICAL;
  } else if (hcsr04_health == WATCHDOG_HEALTH_DEGRADED || temp_health == WATCHDOG_HEALTH_DEGRADED) {
    return WATCHDOG_HEALTH_DEGRADED;
  }

  return WATCHDOG_HEALTH_GOOD;
}

static watchdog_health_level_t internal_assess_task_health(void)
{
  bool     critical_tasks_running = true;
  uint32_t optional_task_failures = 0;

  // Critical tasks that must be running for system operation
  if (!sensor_task_is_running()) {
    ESP_LOGE(s_TAG, "Sensor task not running - CRITICAL");
    critical_tasks_running = false;
  }

  if (!led_task_is_running()) {
    ESP_LOGE(s_TAG, "LED task not running - CRITICAL");
    critical_tasks_running = false;
  }

  // Optional tasks (DHT22 can fail if sensor not connected)
  if (!dht22_task_is_running()) {
    ESP_LOGD(s_TAG, "DHT22 task not running (sensor may not be connected)");
    optional_task_failures++;
  }

  if (!critical_tasks_running) {
    return WATCHDOG_HEALTH_CRITICAL;
  }
  
  // Optional task failures don't degrade system health if sensor isn't connected
  // The DHT22 task should handle sensor failures gracefully and keep running
  
  return WATCHDOG_HEALTH_GOOD;
}

static watchdog_health_level_t internal_assess_system_state(void)
{
  system_state_t current_state = shared_data_get_system_state();

  switch (current_state) {
    case SYSTEM_STATE_RUNNING:
      return WATCHDOG_HEALTH_GOOD;
    case SYSTEM_STATE_ERROR_RECOVERY:
      return WATCHDOG_HEALTH_DEGRADED;
    case SYSTEM_STATE_INITIALIZING:
      return WATCHDOG_HEALTH_DEGRADED;
    case SYSTEM_STATE_FAULT:
    default:
      return WATCHDOG_HEALTH_FAULT;
  }
}

static watchdog_health_level_t internal_assess_overall_health(void)
{
  watchdog_health_level_t sensor_health = internal_assess_sensor_health();
  watchdog_health_level_t task_health   = internal_assess_task_health();
  watchdog_health_level_t state_health  = internal_assess_system_state();

  watchdog_health_level_t worst_health = WATCHDOG_HEALTH_GOOD;

  if (sensor_health > worst_health)
    worst_health = sensor_health;
  if (task_health > worst_health)
    worst_health = task_health;
  if (state_health > worst_health)
    worst_health = state_health;

  return worst_health;
}

static esp_err_t internal_take_recovery_action(watchdog_health_level_t health_level)
{
  esp_err_t ret = ESP_OK;
  s_task_context.recovery_actions_taken++;

  switch (health_level) {
    case WATCHDOG_HEALTH_DEGRADED:
      ESP_LOGW(s_TAG, "System degraded - logging detailed health status");
      shared_data_set_system_state(SYSTEM_STATE_ERROR_RECOVERY);
      break;

    case WATCHDOG_HEALTH_CRITICAL:
      ESP_LOGE(s_TAG, "System critical - attempting emergency LED shutdown");
      shared_data_set_system_state(SYSTEM_STATE_ERROR_RECOVERY);

      if (led_task_is_running()) {
        led_task_emergency_off();
      }
      break;

    case WATCHDOG_HEALTH_FAULT:
      ESP_LOGE(s_TAG, "System fault - initiating system restart");
      shared_data_set_system_state(SYSTEM_STATE_FAULT);

      ESP_LOGE(s_TAG, "System restart in %d ms...", SYSTEM_RESTART_DELAY_MS);
      vTaskDelay(pdMS_TO_TICKS(SYSTEM_RESTART_DELAY_MS));
      esp_restart();
      break;

    case WATCHDOG_HEALTH_GOOD:
      if (shared_data_get_system_state() == SYSTEM_STATE_ERROR_RECOVERY) {
        ESP_LOGI(s_TAG, "System recovered - returning to normal operation");
        shared_data_set_system_state(SYSTEM_STATE_RUNNING);
      }
      break;

    default:
      ret = ESP_ERR_INVALID_ARG;
      break;
  }

  return ret;
}

static void internal_watchdog_task_loop(void* parameter)
{
  watchdog_task_context_t* ctx = (watchdog_task_context_t*)parameter;

  ESP_LOGI(s_TAG, "System watchdog task started (interval: %dms)", TASK_INTERVAL_WATCHDOG);

  TickType_t last_wake_time = xTaskGetTickCount();

  while (ctx->task_running) {
    watchdog_health_level_t current_health = internal_assess_overall_health();
    ctx->last_health_level                 = current_health;
    ctx->health_checks_performed++;

    if (current_health != WATCHDOG_HEALTH_GOOD) {
      ESP_LOGW(s_TAG, "Health check %lu: Level %d", ctx->health_checks_performed, current_health);
      internal_take_recovery_action(current_health);
    } else {
      ESP_LOGI(s_TAG, "Health check %lu: System healthy", ctx->health_checks_performed);
    }

    if (ctx->health_checks_performed % HEALTH_CHECK_LOG_INTERVAL == 0) {
      uint32_t uptime = shared_data_get_uptime_ms();
      ESP_LOGI(s_TAG,
               "Watchdog status - uptime: %lu ms, checks: %lu, recoveries: %lu",
               uptime,
               ctx->health_checks_performed,
               ctx->recovery_actions_taken);
    }

    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_INTERVAL_WATCHDOG));
  }

  ESP_LOGI(s_TAG, "System watchdog task ending");
  ctx->task_handle = NULL;
  vTaskDelete(NULL);
}

/* ========== Public API Implementation ========== */

esp_err_t watchdog_task_start(void)
{
  if (s_g_context_initialized && s_task_context.task_running) {
    ESP_LOGI(s_TAG, "Watchdog task already running");
    return ESP_OK;
  }

  memset(&s_task_context, 0, sizeof(watchdog_task_context_t));

  s_task_context.task_running            = true;
  s_task_context.health_checks_performed = 0;
  s_task_context.recovery_actions_taken  = 0;
  s_task_context.last_health_level       = WATCHDOG_HEALTH_GOOD;

  BaseType_t task_ret = xTaskCreate(internal_watchdog_task_loop,
                                    "watchdog_task",
                                    TASK_STACK_SIZE_WATCHDOG,
                                    &s_task_context,
                                    TASK_PRIORITY_WATCHDOG,
                                    &s_task_context.task_handle);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_TAG, "Failed to create watchdog task");
    s_task_context.task_running = false;
    return ESP_ERR_NO_MEM;
  }

  s_g_context_initialized = true;

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->watchdog_task_handle = s_task_context.task_handle;
  }

  ESP_LOGI(s_TAG, "System watchdog task started successfully");
  return ESP_OK;
}

esp_err_t watchdog_task_stop(void)
{
  if (!s_g_context_initialized || !s_task_context.task_running) {
    ESP_LOGI(s_TAG, "Watchdog task not running");
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Stopping watchdog task...");

  s_task_context.task_running = false;

  if (s_task_context.task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->watchdog_task_handle = NULL;
  }

  s_g_context_initialized = false;

  ESP_LOGI(s_TAG,
           "Watchdog task stopped (checks: %lu, recoveries: %lu)",
           s_task_context.health_checks_performed,
           s_task_context.recovery_actions_taken);

  memset(&s_task_context, 0, sizeof(watchdog_task_context_t));

  return ESP_OK;
}

bool watchdog_task_is_running(void)
{
  return s_g_context_initialized && s_task_context.task_running &&
         s_task_context.task_handle != NULL;
}

bool watchdog_task_get_stats(uint32_t*                health_checks_performed,
                             uint32_t*                recovery_actions_taken,
                             watchdog_health_level_t* last_health_level)
{
  if (!s_g_context_initialized || health_checks_performed == NULL ||
      recovery_actions_taken == NULL || last_health_level == NULL) {
    return false;
  }

  *health_checks_performed = s_task_context.health_checks_performed;
  *recovery_actions_taken  = s_task_context.recovery_actions_taken;
  *last_health_level       = s_task_context.last_health_level;
  return true;
}

watchdog_health_level_t watchdog_task_force_health_check(void)
{
  if (!s_g_context_initialized) {
    ESP_LOGE(s_TAG, "Watchdog task not initialized");
    return WATCHDOG_HEALTH_FAULT;
  }

  watchdog_health_level_t health = internal_assess_overall_health();

  if (health != WATCHDOG_HEALTH_GOOD) {
    ESP_LOGW(s_TAG, "Forced health check: Level %d", health);
    internal_take_recovery_action(health);
  }

  return health;
}

esp_err_t watchdog_task_register_task(TaskHandle_t task_handle, uint32_t max_response_time_ms)
{
  if (!s_g_context_initialized || task_handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(s_TAG, "Registered task for monitoring (timeout: %lums)", max_response_time_ms);
  return ESP_OK;
}