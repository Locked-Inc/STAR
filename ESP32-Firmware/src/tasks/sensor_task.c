#include "include/sensor_task.h"

#include <esp_log.h>
#include <string.h>

#include "../include/system_config.h"
#include "../modules/include/shared_data.h"

static const char* s_TAG = STAR_SYSTEM_TAG_SENSORS;

/* Task context - single instance per system */
static sensor_task_context_t s_task_context          = {0};
static bool                  s_g_context_initialized = false;

/* ========== Private Functions ========== */

static esp_err_t internal_init_sensors(sensor_task_context_t* ctx)
{
  const hcsr04_config_t sensor_configs[STAR_SYSTEM_NUM_HCSR04] = {
    {STAR_SYSTEM_GPIO_LEFT_TRIG, STAR_SYSTEM_GPIO_LEFT_ECHO, STAR_SYSTEM_DEFAULT_TEMPERATURE_C},
    {STAR_SYSTEM_GPIO_RIGHT_TRIG, STAR_SYSTEM_GPIO_RIGHT_ECHO, STAR_SYSTEM_DEFAULT_TEMPERATURE_C}};

  esp_err_t ret = ESP_OK;

  for (uint8_t i = 0; i < STAR_SYSTEM_NUM_HCSR04; i++) {
    esp_err_t init_ret = star_sensor_hcsr04_init(&ctx->sensor_handles[i],
                                                 ctx->bus_manager,
                                                 ctx->bus_name,
                                                 ctx->error_iface,
                                                 &sensor_configs[i]);
    if (init_ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to init HC-SR04 sensor %d: %s", i, esp_err_to_name(init_ret));

      for (uint8_t j = 0; j < i; j++) {
        star_sensor_hcsr04_deinit(&ctx->sensor_handles[j]);
      }
      ret = init_ret;
      break;
    }
  }

  if (ret == ESP_OK) {
    ctx->sensors_initialized = true;
    ESP_LOGI(s_TAG, "All %d HC-SR04 sensors initialized successfully", STAR_SYSTEM_NUM_HCSR04);
  }

  return ret;
}

static void internal_deinit_sensors(sensor_task_context_t* ctx)
{
  if (!ctx->sensors_initialized) {
    return;
  }

  for (uint8_t i = 0; i < STAR_SYSTEM_NUM_HCSR04; i++) {
    star_sensor_hcsr04_deinit(&ctx->sensor_handles[i]);
  }

  ctx->sensors_initialized = false;
  ESP_LOGI(s_TAG, "All HC-SR04 sensors deinitialized");
}

static esp_err_t internal_read_single_sensor(sensor_task_context_t* ctx, uint8_t sensor_index)
{
  if (!STAR_SYSTEM_VALIDATE_SENSOR_INDEX(sensor_index)) {
    return ESP_ERR_INVALID_ARG;
  }

  float     distance_cm;
  esp_err_t read_result =
    star_sensor_hcsr04_read_distance(&ctx->sensor_handles[sensor_index], &distance_cm);

  sensor_data_t sensor_data =
    shared_data_create_sensor_data(sensor_index, distance_cm, read_result);

  shared_data_set_sensor_reading(sensor_index, &sensor_data);
  shared_data_update_health(sensor_index, (read_result == ESP_OK));

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL && shared_ctx->sensor_data_queue != NULL) {
    if (xQueueSend(shared_ctx->sensor_data_queue, &sensor_data, 0) != pdTRUE) {
      ESP_LOGW(s_TAG, "Sensor data queue full, dropping measurement");
    }
  }

  if (read_result == ESP_OK) {
    const char* sensor_name = (sensor_index == STAR_SYSTEM_HCSR04_LEFT) ? "Left" : "Right";
    ESP_LOGI(s_TAG, "%s sensor: %.1f cm", sensor_name, distance_cm);
  } else {
    ESP_LOGE(s_TAG, "Failed to read sensor %d: %s", sensor_index, esp_err_to_name(read_result));
  }

  return read_result;
}

static void internal_sensor_task_loop(void* parameter)
{
  sensor_task_context_t* ctx = (sensor_task_context_t*)parameter;

  ESP_LOGI(s_TAG, "Sensor monitoring task started");

  while (ctx->task_running) {
    TickType_t start_time = xTaskGetTickCount();

    for (uint8_t i = 0; i < STAR_SYSTEM_NUM_HCSR04; i++) {
      if (!ctx->task_running) {
        break;
      }

      internal_read_single_sensor(ctx, i);

      if (i < STAR_SYSTEM_NUM_HCSR04 - 1) {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }

    TickType_t elapsed       = xTaskGetTickCount() - start_time;
    TickType_t target_period = pdMS_TO_TICKS(STAR_SYSTEM_TASK_INTERVAL_SENSORS);

    if (elapsed < target_period) {
      vTaskDelay(target_period - elapsed);
    }
  }

  ESP_LOGI(s_TAG, "Sensor monitoring task ending");
  ctx->task_handle = NULL;
  vTaskDelete(NULL);
}

/* ========== Public API Implementation ========== */

esp_err_t sensor_task_start(star_bus_manager_t*     bus_manager,
                            star_error_interface_t* error_iface,
                            const char*             bus_name)
{
  if (bus_manager == NULL || bus_name == NULL) {
    ESP_LOGE(s_TAG, "Invalid arguments for sensor task start");
    return ESP_ERR_INVALID_ARG;
  }

  if (s_g_context_initialized && s_task_context.task_running) {
    ESP_LOGI(s_TAG, "Sensor task already running");
    return ESP_OK;
  }

  memset(&s_task_context, 0, sizeof(sensor_task_context_t));

  s_task_context.bus_manager         = bus_manager;
  s_task_context.error_iface         = error_iface;
  s_task_context.bus_name            = bus_name;
  s_task_context.task_running        = false;
  s_task_context.sensors_initialized = false;

  esp_err_t ret = internal_init_sensors(&s_task_context);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize sensors");
    return ret;
  }

  s_task_context.task_running = true;

  BaseType_t task_ret = xTaskCreate(internal_sensor_task_loop,
                                    "sensor_task",
                                    STAR_SYSTEM_TASK_STACK_SIZE_SENSORS,
                                    &s_task_context,
                                    STAR_SYSTEM_TASK_PRIORITY_SENSORS,
                                    &s_task_context.task_handle);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_TAG, "Failed to create sensor task");
    s_task_context.task_running = false;
    internal_deinit_sensors(&s_task_context);
    return ESP_ERR_NO_MEM;
  }

  s_g_context_initialized = true;

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->sensors_task_handle = s_task_context.task_handle;
  }

  ESP_LOGI(s_TAG, "Sensor task started successfully");
  return ESP_OK;
}

esp_err_t sensor_task_stop(void)
{
  if (!s_g_context_initialized || !s_task_context.task_running) {
    ESP_LOGI(s_TAG, "Sensor task not running");
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Stopping sensor task...");

  s_task_context.task_running = false;

  if (s_task_context.task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  internal_deinit_sensors(&s_task_context);

  shared_context_t* shared_ctx = shared_data_get_context();
  if (shared_ctx != NULL) {
    shared_ctx->sensors_task_handle = NULL;
  }

  s_g_context_initialized = false;
  memset(&s_task_context, 0, sizeof(sensor_task_context_t));

  ESP_LOGI(s_TAG, "Sensor task stopped successfully");
  return ESP_OK;
}

bool sensor_task_is_running(void)
{
  return s_g_context_initialized && s_task_context.task_running &&
         s_task_context.task_handle != NULL;
}

esp_err_t sensor_task_update_temperature(float temperature_c)
{
  if (!s_g_context_initialized || !s_task_context.sensors_initialized) {
    ESP_LOGE(s_TAG, "Sensor task not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (!STAR_SYSTEM_VALIDATE_TEMPERATURE(temperature_c)) {
    ESP_LOGE(s_TAG, "Invalid temperature: %.1f°C", temperature_c);
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  for (uint8_t i = 0; i < STAR_SYSTEM_NUM_HCSR04; i++) {
    esp_err_t update_ret =
      star_sensor_hcsr04_set_temperature(&s_task_context.sensor_handles[i], temperature_c);
    if (update_ret != ESP_OK) {
      ESP_LOGE(s_TAG,
               "Failed to update temperature for sensor %d: %s",
               i,
               esp_err_to_name(update_ret));
      ret = update_ret;
    }
  }

  if (ret == ESP_OK) {
    ESP_LOGI(s_TAG, "Temperature updated to %.1f°C for all sensors", temperature_c);
  }

  return ret;
}