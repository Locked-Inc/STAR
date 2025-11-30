#include "include/led_task.h"

#include <esp_log.h>
#include <string.h>

#include "../include/system_config.h"
#include "../modules/include/shared_data.h"

static const char* s_TAG = STAR_SYSTEM_TAG_LEDS;

/* Task context - single instance per system */
static led_task_context_t s_task_context          = {0};
static bool               s_g_context_initialized = false;

/* ========== Private Functions ========== */

static esp_err_t internal_update_led_from_sensor_data(led_task_context_t*  ctx,
                                                      const sensor_data_t* sensor_data)
{
  if (!STAR_SYSTEM_VALIDATE_SENSOR_INDEX(sensor_data->sensor_index)) {
    ESP_LOGE(s_TAG, "Invalid sensor index: %d", sensor_data->sensor_index);
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret;

  if (sensor_data->is_valid && sensor_data->read_result == ESP_OK) {
    ret = led_controller_update_distance(&ctx->led_controller,
                                         (led_index_t)sensor_data->sensor_index,
                                         sensor_data->distance_cm);
    if (ret == ESP_OK) {
      const char* sensor_name = (sensor_data->sensor_index == STAR_SYSTEM_HCSR04_LEFT) ? "Left" : "Right";

      rgb_color_t color = led_controller_distance_to_color(sensor_data->distance_cm);
      if (color.red > STAR_SYSTEM_LED_OFF_THRESHOLD || color.green > STAR_SYSTEM_LED_OFF_THRESHOLD ||
          color.blue > STAR_SYSTEM_LED_OFF_THRESHOLD) {
        ESP_LOGI(s_TAG,
                 "%s LED: R=%.1f%% G=%.1f%% B=%.1f%% (%.1fcm)",
                 sensor_name,
                 color.red,
                 color.green,
                 color.blue,
                 sensor_data->distance_cm);
      } else {
        ESP_LOGI(s_TAG,
                 "%s LED: OFF (%.1fcm > %.0fcm)",
                 sensor_name,
                 sensor_data->distance_cm,
                 STAR_SYSTEM_LED_COLOR_DISTANCE_MAX);
      }
    }
  } else {
    ret = led_controller_turn_off(&ctx->led_controller, (led_index_t)sensor_data->sensor_index);
    if (ret == ESP_OK) {
      const char* sensor_name = (sensor_data->sensor_index == STAR_SYSTEM_HCSR04_LEFT) ? "Left" : "Right";
      ESP_LOGI(s_TAG, "%s LED: OFF (sensor error)", sensor_name);
    }
  }

  if (ret == ESP_OK) {
    ctx->successful_updates++;
  } else {
    ctx->update_failures++;
    ESP_LOGE(s_TAG, "Failed to update LED %d: %s", sensor_data->sensor_index, esp_err_to_name(ret));
  }

  return ret;
}

static void internal_process_sensor_data_queue(led_task_context_t* ctx)
{
  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx == NULL || shared_ctx->sensor_data_queue == NULL) {
    return;
  }

  sensor_data_t sensor_data;
  uint32_t processed = 0;
  const uint32_t max_per_cycle = 8; // Limit processing to prevent starvation
  
  while (processed < max_per_cycle && 
         xQueueReceive(shared_ctx->sensor_data_queue, &sensor_data, 0) == pdTRUE) {
    internal_update_led_from_sensor_data(ctx, &sensor_data);
    processed++;
  }
  
  if (processed > 0) {
    ESP_LOGD(s_TAG, "Processed %lu sensor queue messages", processed);
  }
}

static void internal_update_leds_from_latest_readings(led_task_context_t* ctx)
{
  for (uint8_t i = 0; i < STAR_SYSTEM_NUM_HCSR04; i++) {
    sensor_data_t sensor_data;
    if (shared_data_get_sensor_reading(i, &sensor_data)) {
      uint32_t age_ms = shared_data_get_uptime_ms() - sensor_data.timestamp_ms;

      if (age_ms > (STAR_SYSTEM_TASK_INTERVAL_SENSORS * 5)) {
        ESP_LOGW(s_TAG, "Sensor %d data too old (%lums), turning LED off", i, age_ms);
        led_controller_turn_off(&ctx->led_controller, (led_index_t)i);
      } else {
        internal_update_led_from_sensor_data(ctx, &sensor_data);
      }
    } else {
      led_controller_turn_off(&ctx->led_controller, (led_index_t)i);
    }
  }
}

static void internal_led_task_loop(void* parameter)
{
  led_task_context_t* ctx = (led_task_context_t*)parameter;

  ESP_LOGI(s_TAG, "LED display task started (interval: %dms)", STAR_SYSTEM_TASK_INTERVAL_LEDS);

  TickType_t last_wake_time = xTaskGetTickCount();

  while (ctx->task_running) {
    if (ctx->leds_initialized) {
      internal_process_sensor_data_queue(ctx);

      shared_context_t* shared_ctx = shared_data_get_context();
      if (shared_ctx != NULL && shared_ctx->sensor_data_queue != NULL) {
        if (uxQueueMessagesWaiting(shared_ctx->sensor_data_queue) == 0) {
          internal_update_leds_from_latest_readings(ctx);
        }
      } else {
        internal_update_leds_from_latest_readings(ctx);
      }
    } else {
      ESP_LOGW(s_TAG, "LED controller not initialized, skipping update");
    }

    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(STAR_SYSTEM_TASK_INTERVAL_LEDS));
  }

  ESP_LOGI(s_TAG, "LED display task ending");
  ctx->task_handle = NULL;
  vTaskDelete(NULL);
}

/* ========== Public API Implementation ========== */

esp_err_t led_task_start(const pca9685_handle_t* pca_handle)
{
  if (pca_handle == NULL) {
    ESP_LOGE(s_TAG, "Invalid PCA9685 handle for LED task start");
    return ESP_ERR_INVALID_ARG;
  }

  if (s_g_context_initialized && s_task_context.task_running) {
    ESP_LOGI(s_TAG, "LED task already running");
    return ESP_OK;
  }

  memset(&s_task_context, 0, sizeof(led_task_context_t));

  s_task_context.pca_handle         = pca_handle;
  s_task_context.task_running       = false;
  s_task_context.leds_initialized   = false;
  s_task_context.update_failures    = 0;
  s_task_context.successful_updates = 0;

  esp_err_t ret = led_controller_init(&s_task_context.led_controller, pca_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize LED controller");
    return ret;
  }

  s_task_context.leds_initialized = true;
  s_task_context.task_running     = true;

  BaseType_t task_ret = xTaskCreate(internal_led_task_loop,
                                    "led_task",
                                    STAR_SYSTEM_TASK_STACK_SIZE_LEDS,
                                    &s_task_context,
                                    STAR_SYSTEM_TASK_PRIORITY_LEDS,
                                    &s_task_context.task_handle);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_TAG, "Failed to create LED task");
    s_task_context.task_running = false;
    led_controller_deinit(&s_task_context.led_controller);
    return ESP_ERR_NO_MEM;
  }

  s_g_context_initialized = true;

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->leds_task_handle = s_task_context.task_handle;
  }

  ESP_LOGI(s_TAG, "LED task started successfully");
  return ESP_OK;
}

esp_err_t led_task_stop(void)
{
  if (!s_g_context_initialized || !s_task_context.task_running) {
    ESP_LOGI(s_TAG, "LED task not running");
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Stopping LED task...");

  s_task_context.task_running = false;

  if (s_task_context.task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (s_task_context.leds_initialized) {
    led_controller_deinit(&s_task_context.led_controller);
    s_task_context.leds_initialized = false;
  }

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->leds_task_handle = NULL;
  }

  s_g_context_initialized = false;

  ESP_LOGI(s_TAG,
           "LED task stopped successfully (failures: %lu, successes: %lu)",
           s_task_context.update_failures,
           s_task_context.successful_updates);

  memset(&s_task_context, 0, sizeof(led_task_context_t));

  return ESP_OK;
}

bool led_task_is_running(void)
{
  return s_g_context_initialized && s_task_context.task_running &&
         s_task_context.task_handle != NULL;
}

bool led_task_get_stats(uint32_t* update_failures, uint32_t* successful_updates)
{
  if (!s_g_context_initialized || update_failures == NULL || successful_updates == NULL) {
    return false;
  }

  *update_failures    = s_task_context.update_failures;
  *successful_updates = s_task_context.successful_updates;
  return true;
}

esp_err_t led_task_manual_update(uint8_t sensor_index, float distance_cm)
{
  if (!s_g_context_initialized || !s_task_context.leds_initialized) {
    ESP_LOGE(s_TAG, "LED task not properly initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (!STAR_SYSTEM_VALIDATE_SENSOR_INDEX(sensor_index)) {
    ESP_LOGE(s_TAG, "Invalid sensor index: %d", sensor_index);
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret;

  if (distance_cm < 0.0f) {
    ret = led_controller_turn_off(&s_task_context.led_controller, (led_index_t)sensor_index);
    ESP_LOGI(s_TAG, "Manually turned off LED %d", sensor_index);
  } else {
    ret = led_controller_update_distance(&s_task_context.led_controller,
                                         (led_index_t)sensor_index,
                                         distance_cm);
    ESP_LOGI(s_TAG, "Manually updated LED %d with distance %.1fcm", sensor_index, distance_cm);
  }

  if (ret == ESP_OK) {
    s_task_context.successful_updates++;
  } else {
    s_task_context.update_failures++;
  }

  return ret;
}

esp_err_t led_task_emergency_off(void)
{
  if (!s_g_context_initialized || !s_task_context.leds_initialized) {
    ESP_LOGE(s_TAG, "LED task not properly initialized");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret = led_controller_turn_off_all(&s_task_context.led_controller);

  if (ret == ESP_OK) {
    ESP_LOGI(s_TAG, "Emergency LED shutdown successful");
    s_task_context.successful_updates++;
  } else {
    ESP_LOGE(s_TAG, "Emergency LED shutdown failed: %s", esp_err_to_name(ret));
    s_task_context.update_failures++;
  }

  return ret;
}