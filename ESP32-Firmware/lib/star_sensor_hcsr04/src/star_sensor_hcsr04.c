/* lib/star_sensor_hcsr04/src/star_sensor_hcsr04.c */

#include "star_sensor_hcsr04.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

#include "star_bus_gpio.h"
#include "star_error_handler.h"
#include "star_sensor_hcsr04_constants.h"

static const char* const s_TAG = "hcsr04";

/* Use constant instead of macro for type safety */
static const uint32_t HCSR04_MUTEX_TIMEOUT_MS = 1000;

static void IRAM_ATTR hcsr04_echo_isr_handler(void* const arg)
{
  hcsr04_handle_t* const handle = (hcsr04_handle_t*)arg;

  if (gpio_get_level(handle->echo_pin)) {
    // Rising edge - start timing
    handle->echo_start_time      = (uint32_t)esp_timer_get_time();
    handle->measurement_complete = false;
  } else {
    // Falling edge - end timing
    handle->echo_end_time        = (uint32_t)esp_timer_get_time();
    handle->measurement_complete = true;
  }
}

esp_err_t star_sensor_hcsr04_init(hcsr04_handle_t*        handle,
                                  star_bus_manager_t*     manager,
                                  const char*             bus_name,
                                  star_error_interface_t* error_iface,
                                  const hcsr04_config_t*  config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(hcsr04_handle_t));
  handle->manager       = manager;
  handle->bus_name      = bus_name;
  handle->trigger_pin   = config->trigger_pin;
  handle->echo_pin      = config->echo_pin;
  handle->temperature_c = config->temperature_c;

  // Create mutex first
  handle->mutex = xSemaphoreCreateMutex();
  if (handle->mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  // Handle error interface injection
  if (error_iface == NULL) {
    // Create default error handler internally
    error_handler_t* default_handler = error_handler_create_default();
    if (default_handler == NULL) {
      ESP_LOGE(s_TAG, "Failed to create default error handler");
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    // Allocate and store the interface
    handle->error_iface = (star_error_interface_t*)malloc(sizeof(star_error_interface_t));
    if (handle->error_iface == NULL) {
      ESP_LOGE(s_TAG, "Failed to allocate error interface");
      error_handler_destroy_default(default_handler);
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    error_handler_get_interface(handle->error_iface, default_handler);
    handle->owns_error_handler = true;
    ESP_LOGI(s_TAG, "Created default error handler internally");
  } else {
    handle->error_iface        = error_iface;
    handle->owns_error_handler = false;
    ESP_LOGI(s_TAG, "Using injected error interface");
  }

  ESP_LOGI(s_TAG,
           "Initializing HC-SR04 (trigger=%d, echo=%d)",
           config->trigger_pin,
           config->echo_pin);

  // Configure trigger pin as output through GPIO bus
  esp_err_t ret = star_bus_gpio_configure(manager,
                                          bus_name,
                                          config->trigger_pin,
                                          GPIO_MODE_OUTPUT,
                                          GPIO_FLOATING);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure trigger pin: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  // Set trigger pin low initially
  ret = star_bus_gpio_write(manager, bus_name, config->trigger_pin, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set trigger pin low: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  // Configure echo pin as input with interrupt through GPIO bus
  ret =
    star_bus_gpio_configure(manager, bus_name, config->echo_pin, GPIO_MODE_INPUT, GPIO_FLOATING);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure echo pin: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  // Set up interrupt handler through GPIO bus
  ret = star_bus_gpio_set_interrupt(manager,
                                    bus_name,
                                    config->echo_pin,
                                    GPIO_INTR_ANYEDGE,
                                    hcsr04_echo_isr_handler,
                                    (void*)handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set interrupt: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "HC-SR04 initialization successful (thread-safe with mutex)");
  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_deinit(hcsr04_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex before cleanup
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(HCSR04_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(s_TAG, "Failed to acquire mutex for deinit, continuing anyway");
  }

  // Remove ISR handler (direct GPIO call, not through bus, for cleanup)
  gpio_isr_handler_remove(handle->echo_pin);

  // Clean up error handler if we own it
  if (handle->owns_error_handler && handle->error_iface != NULL) {
    error_handler_t* error_handler = (error_handler_t*)handle->error_iface->ctx;
    error_handler_destroy_default(error_handler);
    free(handle->error_iface);
    handle->error_iface = NULL;
    ESP_LOGI(s_TAG, "Destroyed internally-owned error handler");
  }

  // Clear handle fields
  handle->initialized = false;
  handle->mutex       = NULL;

  // Release and delete mutex
  if (mutex != NULL) {
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
  }

  ESP_LOGI(s_TAG, "HC-SR04 deinitialized");
  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_trigger(hcsr04_handle_t* const handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(HCSR04_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for trigger");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  handle->measurement_complete = false;
  handle->timeout_occurred     = false;
  handle->echo_start_time      = 0;
  handle->echo_end_time        = 0;

  // Send 10us trigger pulse (use GPIO bus)
  esp_err_t ret = star_bus_gpio_write(handle->manager, handle->bus_name, handle->trigger_pin, 0);
  if (ret == ESP_OK) {
    esp_rom_delay_us(2);
    ret = star_bus_gpio_write(handle->manager, handle->bus_name, handle->trigger_pin, 1);
  }
  if (ret == ESP_OK) {
    esp_rom_delay_us(10);
    ret = star_bus_gpio_write(handle->manager, handle->bus_name, handle->trigger_pin, 0);
  }

  xSemaphoreGive(mutex);
  return ret;
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

  if (echo_time_us < 116 || echo_time_us > STAR_HCSR04_TIMEOUT_US) {
    return k_hcsr04_err_invalid_pulse;
  }

  // Speed of sound = 331.4 + 0.606 * T (m/s)
  const float speed_cm_us = (331.4f + 0.606f * temperature_c) / 10000.0f;

  // Distance = (time * speed) / 2 (divide by 2 for round trip)
  *distance_cm = (echo_time_us * speed_cm_us) / 2.0f;

  if (*distance_cm < STAR_HCSR04_MIN_DISTANCE_CM) {
    return k_hcsr04_err_out_of_range_min;
  }
  if (*distance_cm > STAR_HCSR04_MAX_DISTANCE_CM) {
    return k_hcsr04_err_out_of_range_max;
  }

  return ESP_OK;
}

esp_err_t star_sensor_hcsr04_get_result(hcsr04_handle_t* const handle, float* const distance_cm)
{
  if (handle == NULL || distance_cm == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->measurement_complete) {
    return k_hcsr04_err_not_ready;
  }

  if (handle->echo_end_time <= handle->echo_start_time) {
    ESP_LOGE(s_TAG,
             "Invalid timing: start=%lu, end=%lu",
             handle->echo_start_time,
             handle->echo_end_time);
    return k_hcsr04_err_invalid_pulse;
  }

  const uint32_t echo_time = handle->echo_end_time - handle->echo_start_time;

  const esp_err_t ret =
    star_sensor_hcsr04_calculate_distance(echo_time, handle->temperature_c, distance_cm);

  if (ret == ESP_OK) {
    ESP_LOGD(s_TAG, "Distance: %.2f cm (echo: %lu us)", *distance_cm, echo_time);
  } else {
    ESP_LOGW(s_TAG, "Measurement error: %d (echo: %lu us)", ret, echo_time);
  }

  return ret;
}

esp_err_t star_sensor_hcsr04_read_distance(hcsr04_handle_t* const handle, float* const distance_cm)
{
  if (handle == NULL || distance_cm == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_err_t ret = star_sensor_hcsr04_trigger(handle);
  if (ret != ESP_OK) {
    return ret;
  }

  // Wait for measurement with timeout
  const uint32_t start_time    = (uint32_t)esp_timer_get_time();
  const uint32_t timeout_ms    = 60; // Max time for 400cm measurement + margin
  uint32_t       yield_counter = 0;

  while (!handle->measurement_complete) {
    const uint32_t elapsed = ((uint32_t)esp_timer_get_time() - start_time) / 1000;
    if (elapsed > timeout_ms) {
      ESP_LOGW(s_TAG, "Measurement timeout");
      return k_hcsr04_err_timeout;
    }

    /* Aggressive yielding strategy to prevent watchdog timeout */
    if (elapsed < 2) {
      /* First 2ms: very short delays with frequent yields */
      yield_counter++;
      if (yield_counter >= 10) { /* Yield every 10 iterations */
        yield_counter = 0;
        taskYIELD();
      } else {
        esp_rom_delay_us(10); /* Very short delay */
      }
    } else if (elapsed < 10) {
      /* 2-10ms: slightly longer delays but still responsive */
      taskYIELD(); /* Always yield in this range */
      esp_rom_delay_us(50);
    } else {
      /* After 10ms: use vTaskDelay for better scheduling */
      vTaskDelay(1);
    }
  }

  return star_sensor_hcsr04_get_result(handle, distance_cm);
}

esp_err_t star_sensor_hcsr04_set_temperature(hcsr04_handle_t* const handle,
                                             const float            temperature_c)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(HCSR04_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for set_temperature");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  if (temperature_c < -40.0f || temperature_c > 85.0f) {
    ESP_LOGW(s_TAG, "Temperature out of typical range: %.1f C", temperature_c);
  }

  handle->temperature_c = temperature_c;

  xSemaphoreGive(mutex);
  return ESP_OK;
}
