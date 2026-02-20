#include "include/dht22_task.h"

#include <esp_log.h>
#include <string.h>

#include "../include/system_config.h"
#include "../modules/include/shared_data.h"
#include "include/sensor_task.h"
#include "star_bus_dht22_proprietary.h"

static const char *s_TAG =
    "DHT22_TASK"; /* Use STAR_SYSTEM_CONFIG.log_tags.dht22 */

/* Task context - single instance per system */
static dht22_task_context_t s_task_context = {0};
static bool s_g_context_initialized = false;

/* ========== Private Functions ========== */

static esp_err_t internal_init_dht22(dht22_task_context_t *const ctx) {
  ESP_LOGI(s_TAG, "Initializing DHT22 on GPIO %d",
           STAR_SYSTEM_CONFIG.gpio.dht22_data);

  star_dht22_config_t dht22_config = STAR_DHT22_CONFIG_DEFAULT();
  dht22_config.gpio_pin = STAR_SYSTEM_CONFIG.gpio.dht22_data;

  esp_err_t ret =
      star_bus_dht22_init(ctx->bus_manager, ctx->bus_name, &dht22_config);

  if (ret == ESP_OK) {
    ctx->sensor_initialized = true;
    ctx->last_valid_temperature =
        STAR_SYSTEM_CONFIG.health.default_temperature_c;
    ESP_LOGI(s_TAG, "DHT22 initialized successfully");
  } else {
    ESP_LOGE(s_TAG, "Failed to initialize DHT22: %s", esp_err_to_name(ret));
  }

  return ret;
}

static void internal_deinit_dht22(dht22_task_context_t *const ctx) {
  if (!ctx->sensor_initialized) {
    return;
  }

  star_bus_dht22_deinit(ctx->bus_manager, ctx->bus_name);
  ctx->sensor_initialized = false;
  ESP_LOGI(s_TAG, "DHT22 deinitialized");
}

static esp_err_t internal_read_dht22(dht22_task_context_t *const ctx) {
  star_dht22_data_t dht22_data;
  const esp_err_t read_result =
      star_bus_dht22_read(ctx->bus_manager, ctx->bus_name, &dht22_data);

  const bool data_valid = (read_result == ESP_OK && dht22_data.checksum_valid);

  const temperature_data_t temp_data = shared_data_create_temperature_data(
      dht22_data.temperature_c, dht22_data.humidity_percent, read_result,
      dht22_data.checksum_valid);

  shared_data_update_health(STAR_SYSTEM_CONFIG.num_hcsr04_sensors, data_valid);

  shared_context_t *shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL && shared_ctx->temperature_data_queue != NULL) {
    if (xQueueSend(shared_ctx->temperature_data_queue, &temp_data, 0) !=
        pdTRUE) {
      ESP_LOGW(s_TAG, "Temperature data queue full, dropping measurement");
    }
  }

  if (data_valid) {
    ctx->last_valid_temperature = dht22_data.temperature_c;
    ctx->consecutive_failures = 0;

    shared_data_set_temperature(dht22_data.temperature_c, true);

    const esp_err_t temp_update_ret =
        sensor_task_update_temperature(dht22_data.temperature_c);
    if (temp_update_ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to update sensor task temperature: %s",
               esp_err_to_name(temp_update_ret));
    }

    ESP_LOGI(s_TAG, "DHT22 reading: %.1fdegC, %.1f%% RH",
             dht22_data.temperature_c, dht22_data.humidity_percent);
  } else {
    ctx->consecutive_failures++;
    ESP_LOGW(s_TAG, "DHT22 read failed: %s (consecutive failures: %lu)",
             esp_err_to_name(read_result), ctx->consecutive_failures);

    if (ctx->consecutive_failures >=
        STAR_SYSTEM_CONFIG.health.max_sensor_failures) {
      ESP_LOGE(s_TAG, "DHT22 sensor may be faulty - %lu consecutive failures",
               ctx->consecutive_failures);

      shared_data_set_temperature(ctx->last_valid_temperature, false);
    }
  }

  return read_result;
}

static void internal_dht22_task_loop(void *const parameter) {
  dht22_task_context_t *const ctx = (dht22_task_context_t *)parameter;

  ESP_LOGI(s_TAG, "DHT22 monitoring task started (interval: %dms)",
           STAR_SYSTEM_CONFIG.tasks.dht22.interval_ms);

  TickType_t last_wake_time = xTaskGetTickCount();

  while (ctx->task_running) {
    if (ctx->sensor_initialized) {
      internal_read_dht22(ctx);
    } else {
      ESP_LOGW(s_TAG, "DHT22 not initialized, skipping read");
    }

    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.tasks.dht22.interval_ms));
  }

  ESP_LOGI(s_TAG, "DHT22 monitoring task ending");
  ctx->task_handle = NULL;
  vTaskDelete(NULL);
}

/* ========== Public API Implementation ========== */

esp_err_t dht22_task_start(star_bus_manager_t *const bus_manager,
                           const char *const bus_name) {
  if (bus_manager == NULL || bus_name == NULL) {
    ESP_LOGE(s_TAG, "Invalid arguments for DHT22 task start");
    return ESP_ERR_INVALID_ARG;
  }

  if (s_g_context_initialized && s_task_context.task_running) {
    ESP_LOGI(s_TAG, "DHT22 task already running");
    return ESP_OK;
  }

  memset(&s_task_context, 0, sizeof(dht22_task_context_t));

  s_task_context.bus_manager = bus_manager;
  s_task_context.bus_name = bus_name;
  s_task_context.task_running = false;
  s_task_context.sensor_initialized = false;
  s_task_context.last_valid_temperature =
      STAR_SYSTEM_CONFIG.health.default_temperature_c;
  s_task_context.consecutive_failures = 0;

  esp_err_t ret = internal_init_dht22(&s_task_context);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize DHT22 sensor");
    return ret;
  }

  s_task_context.task_running = true;

  const BaseType_t task_ret = xTaskCreate(
      internal_dht22_task_loop, STAR_SYSTEM_CONFIG.tasks.dht22.name,
      STAR_SYSTEM_CONFIG.tasks.dht22.stack_size, &s_task_context,
      STAR_SYSTEM_CONFIG.tasks.dht22.priority, &s_task_context.task_handle);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_TAG, "Failed to create DHT22 task");
    s_task_context.task_running = false;
    internal_deinit_dht22(&s_task_context);
    return ESP_ERR_NO_MEM;
  }

  s_g_context_initialized = true;

  shared_context_t *shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->dht22_task_handle = s_task_context.task_handle;
  }

  ESP_LOGI(s_TAG, "DHT22 task started successfully");
  return ESP_OK;
}

esp_err_t dht22_task_stop(void) {
  if (!s_g_context_initialized || !s_task_context.task_running) {
    ESP_LOGI(s_TAG, "DHT22 task not running");
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Stopping DHT22 task...");

  s_task_context.task_running = false;

  if (s_task_context.task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  internal_deinit_dht22(&s_task_context);

  shared_context_t *shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->dht22_task_handle = NULL;
  }

  s_g_context_initialized = false;
  memset(&s_task_context, 0, sizeof(dht22_task_context_t));

  ESP_LOGI(s_TAG, "DHT22 task stopped successfully");
  return ESP_OK;
}

bool dht22_task_is_running(void) {
  return s_g_context_initialized && s_task_context.task_running &&
         s_task_context.task_handle != NULL;
}

bool dht22_task_get_last_temperature(float *temperature_c) {
  if (!s_g_context_initialized || temperature_c == NULL) {
    return false;
  }

  *temperature_c = s_task_context.last_valid_temperature;
  return true;
}

bool dht22_task_get_health(uint32_t *consecutive_failures,
                           bool *sensor_initialized) {
  if (!s_g_context_initialized || consecutive_failures == NULL ||
      sensor_initialized == NULL) {
    return false;
  }

  *consecutive_failures = s_task_context.consecutive_failures;
  *sensor_initialized = s_task_context.sensor_initialized;
  return true;
}