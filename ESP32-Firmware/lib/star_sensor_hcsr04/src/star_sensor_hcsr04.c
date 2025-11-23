/* lib/star_sensor_hcsr04/src/star_sensor_hcsr04.c */

#include "star_sensor_hcsr04.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* s_TAG = "hcsr04";

static void IRAM_ATTR hcsr04_echo_isr_handler(void* arg)
{
  hcsr04_handle_t* handle = (hcsr04_handle_t*)arg;

  if (gpio_get_level(handle->echo_pin)) {
    // Rising edge - start timing
    handle->echo_start_time = (uint32_t)esp_timer_get_time();
    handle->measurement_complete = false;
  } else {
    // Falling edge - end timing
    handle->echo_end_time = (uint32_t)esp_timer_get_time();
    handle->measurement_complete = true;
  }
}

esp_err_t star_sensor_hcsr04_init(hcsr04_handle_t* handle, const hcsr04_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(hcsr04_handle_t));
  handle->trigger_pin = config->trigger_pin;
  handle->echo_pin = config->echo_pin;
  handle->temperature_c = config->temperature_c;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing HC-SR04 (trigger=%d, echo=%d)", config->trigger_pin, config->echo_pin);

  // Configure trigger pin as output
  gpio_config_t trigger_conf = {
    .pin_bit_mask = (1ULL << config->trigger_pin),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  ret = gpio_config(&trigger_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure trigger pin: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }
  gpio_set_level(config->trigger_pin, 0);

  // Configure echo pin as input with interrupt
  gpio_config_t echo_conf = {
    .pin_bit_mask = (1ULL << config->echo_pin),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_ANYEDGE
  };
  ret = gpio_config(&echo_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure echo pin: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Install ISR service if not already installed
  ret = gpio_install_isr_service(0);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(s_TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Add ISR handler
  ret = gpio_isr_handler_add(config->echo_pin, hcsr04_echo_isr_handler, (void*)handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "HC-SR04 initialization successful");
  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_deinit(hcsr04_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  gpio_isr_handler_remove(handle->echo_pin);
  gpio_reset_pin(handle->trigger_pin);
  gpio_reset_pin(handle->echo_pin);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_trigger(hcsr04_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  handle->measurement_complete = false;
  handle->timeout_occurred = false;
  handle->echo_start_time = 0;
  handle->echo_end_time = 0;

  // Send 10us trigger pulse
  gpio_set_level(handle->trigger_pin, 0);
  esp_rom_delay_us(2);
  gpio_set_level(handle->trigger_pin, 1);
  esp_rom_delay_us(10);
  gpio_set_level(handle->trigger_pin, 0);

  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_is_complete(const hcsr04_handle_t* handle, bool* complete)
{
  if (handle == NULL || complete == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  *complete = handle->measurement_complete;
  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_calculate_distance(uint32_t echo_time_us,
                                                float    temperature_c,
                                                float*   distance_cm)
{
  if (distance_cm == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (echo_time_us < 116 || echo_time_us > HCSR04_TIMEOUT_US) {
    return HCSR04_ERR_INVALID_PULSE;
  }

  // Speed of sound = 331.4 + 0.606 * T (m/s)
  float speed_cm_us = (331.4f + 0.606f * temperature_c) / 10000.0f;

  // Distance = (time * speed) / 2 (divide by 2 for round trip)
  *distance_cm = (echo_time_us * speed_cm_us) / 2.0f;

  if (*distance_cm < HCSR04_MIN_DISTANCE_CM) {
    return HCSR04_ERR_OUT_OF_RANGE_MIN;
  }
  if (*distance_cm > HCSR04_MAX_DISTANCE_CM) {
    return HCSR04_ERR_OUT_OF_RANGE_MAX;
  }

  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_get_result(hcsr04_handle_t* handle, float* distance_cm)
{
  if (handle == NULL || distance_cm == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->measurement_complete) {
    return HCSR04_ERR_NOT_READY;
  }

  if (handle->echo_end_time <= handle->echo_start_time) {
    ESP_LOGE(s_TAG, "Invalid timing: start=%lu, end=%lu", handle->echo_start_time, handle->echo_end_time);
    return HCSR04_ERR_INVALID_PULSE;
  }

  uint32_t echo_time = handle->echo_end_time - handle->echo_start_time;

  esp_err_t ret = star_sensor_hcsr04_calculate_distance(echo_time, handle->temperature_c, distance_cm);

  if (ret == ESP_OK) {
    ESP_LOGD(s_TAG, "Distance: %.2f cm (echo: %lu us)", *distance_cm, echo_time);
  } else {
    ESP_LOGW(s_TAG, "Measurement error: %d (echo: %lu us)", ret, echo_time);
  }

  return ret;
}

esp_err_t star_sensor_hcsr04_read_distance(hcsr04_handle_t* handle, float* distance_cm)
{
  if (handle == NULL || distance_cm == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = star_sensor_hcsr04_trigger(handle);
  if (ret != ESP_OK) {
    return ret;
  }

  // Wait for measurement with timeout
  uint32_t start_time = (uint32_t)esp_timer_get_time();
  uint32_t timeout_ms = 60;  // Max time for 400cm measurement + margin

  while (!handle->measurement_complete) {
    uint32_t elapsed = ((uint32_t)esp_timer_get_time() - start_time) / 1000;
    if (elapsed > timeout_ms) {
      ESP_LOGW(s_TAG, "Measurement timeout");
      return HCSR04_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return star_sensor_hcsr04_get_result(handle, distance_cm);
}

esp_err_t star_sensor_hcsr04_set_temperature(hcsr04_handle_t* handle, float temperature_c)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (temperature_c < -40.0f || temperature_c > 85.0f) {
    ESP_LOGW(s_TAG, "Temperature out of typical range: %.1f C", temperature_c);
  }

  handle->temperature_c = temperature_c;
  return ESP_OK;
}
