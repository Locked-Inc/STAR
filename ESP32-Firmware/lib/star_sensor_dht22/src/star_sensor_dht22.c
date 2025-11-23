/* lib/star_sensor_dht22/src/star_sensor_dht22.c */

#include <driver/gpio.h>

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>
#include <string.h>

static const char* s_TAG = "dht22";

// Critical timing constants (microseconds)
#define DHT22_START_SIGNAL_LOW_US (1000)
#define DHT22_START_SIGNAL_HIGH_US (30)
#define DHT22_RESPONSE_LOW_US (80)
#define DHT22_RESPONSE_HIGH_US (80)
#define DHT22_BIT_START_LOW_US (50)
#define DHT22_BIT_THRESHOLD_US (40)  // >40us = 1, <40us = 0

static inline void dht22_set_output(gpio_num_t pin)
{
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

static inline void dht22_set_input(gpio_num_t pin)
{
  gpio_set_direction(pin, GPIO_MODE_INPUT);
}

static inline void dht22_write(gpio_num_t pin, uint32_t level)
{
  gpio_set_level(pin, level);
}

static inline int dht22_read(gpio_num_t pin)
{
  return gpio_get_level(pin);
}

// Wait for pin to reach expected level with timeout
static esp_err_t dht22_wait_for_level(gpio_num_t pin, int expected_level, uint32_t timeout_us)
{
  uint32_t start = (uint32_t)esp_timer_get_time();

  while (dht22_read(pin) != expected_level) {
    uint32_t elapsed = (uint32_t)esp_timer_get_time() - start;
    if (elapsed > timeout_us) {
      return DHT22_ERR_TIMEOUT;
    }
    ets_delay_us(1);
  }

  return ESP_OK;
}

// Measure pulse width in microseconds
static uint32_t dht22_measure_pulse_us(gpio_num_t pin, int level, uint32_t timeout_us)
{
  uint32_t start = (uint32_t)esp_timer_get_time();

  while (dht22_read(pin) == level) {
    uint32_t elapsed = (uint32_t)esp_timer_get_time() - start;
    if (elapsed > timeout_us) {
      return 0;  // Timeout
    }
    ets_delay_us(1);
  }

  return (uint32_t)esp_timer_get_time() - start;
}

esp_err_t star_sensor_dht22_init(dht22_handle_t* handle, const dht22_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(dht22_handle_t));
  handle->data_pin = config->data_pin;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing DHT22 (data_pin=%d)", config->data_pin);

  // Configure data pin as output (open-drain with pull-up)
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << config->data_pin),
    .mode = GPIO_MODE_OUTPUT_OD,
    .pull_up_en = config->enable_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure data pin: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Set pin high (idle state)
  dht22_write(config->data_pin, 1);

  handle->initialized = true;
  handle->last_read_time = 0;
  ESP_LOGI(s_TAG, "DHT22 initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_dht22_deinit(dht22_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  gpio_reset_pin(handle->data_pin);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_dht22_can_read(const dht22_handle_t* handle, bool* can_read)
{
  if (handle == NULL || can_read == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);  // Convert to ms
  uint32_t elapsed = current_time - handle->last_read_time;

  *can_read = (elapsed >= DHT22_MIN_SAMPLE_PERIOD_MS);
  return ESP_OK;
}

esp_err_t star_sensor_dht22_read(dht22_handle_t* handle, dht22_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Check minimum sample period
  bool can_read = false;
  star_sensor_dht22_can_read(handle, &can_read);
  if (!can_read) {
    ESP_LOGW(s_TAG, "Reading too soon, minimum 2s interval required");
    return DHT22_ERR_TOO_SOON;
  }

  esp_err_t ret;
  uint8_t raw_data[5] = {0};  // 40 bits + 8-bit checksum

  // Disable interrupts during timing-critical section
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);

  // Send start signal: Pull low for 1ms
  dht22_set_output(handle->data_pin);
  dht22_write(handle->data_pin, 0);
  ets_delay_us(DHT22_START_SIGNAL_LOW_US);

  // Pull high for 20-40us
  dht22_write(handle->data_pin, 1);
  ets_delay_us(DHT22_START_SIGNAL_HIGH_US);

  // Switch to input mode to read response
  dht22_set_input(handle->data_pin);

  // Wait for sensor to pull low (response)
  ret = dht22_wait_for_level(handle->data_pin, 0, DHT22_TIMEOUT_US);
  if (ret != ESP_OK) {
    portEXIT_CRITICAL(&mux);
    ESP_LOGE(s_TAG, "No response from sensor");
    return DHT22_ERR_NO_RESPONSE;
  }

  // Wait for sensor to pull high
  ret = dht22_wait_for_level(handle->data_pin, 1, DHT22_TIMEOUT_US);
  if (ret != ESP_OK) {
    portEXIT_CRITICAL(&mux);
    ESP_LOGE(s_TAG, "Invalid response timing");
    return DHT22_ERR_TIMEOUT;
  }

  // Wait for sensor to pull low (start of data)
  ret = dht22_wait_for_level(handle->data_pin, 0, DHT22_TIMEOUT_US);
  if (ret != ESP_OK) {
    portEXIT_CRITICAL(&mux);
    ESP_LOGE(s_TAG, "Data start timeout");
    return DHT22_ERR_TIMEOUT;
  }

  // Read 40 bits of data
  for (int i = 0; i < DHT22_DATA_BITS; i++) {
    // Wait for bit start (low->high)
    ret = dht22_wait_for_level(handle->data_pin, 1, DHT22_TIMEOUT_US);
    if (ret != ESP_OK) {
      portEXIT_CRITICAL(&mux);
      ESP_LOGE(s_TAG, "Bit start timeout at bit %d", i);
      return DHT22_ERR_TIMEOUT;
    }

    // Measure high pulse width
    uint32_t pulse_us = dht22_measure_pulse_us(handle->data_pin, 1, DHT22_TIMEOUT_US);
    if (pulse_us == 0) {
      portEXIT_CRITICAL(&mux);
      ESP_LOGE(s_TAG, "Pulse measurement timeout at bit %d", i);
      return DHT22_ERR_TIMEOUT;
    }

    // Decode bit: >40us = 1, <40us = 0
    uint8_t byte_idx = i / 8;
    uint8_t bit_idx = 7 - (i % 8);  // MSB first

    if (pulse_us > DHT22_BIT_THRESHOLD_US) {
      raw_data[byte_idx] |= (1 << bit_idx);
    }
  }

  portEXIT_CRITICAL(&mux);

  // Validate checksum
  uint8_t checksum = (raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3]) & 0xFF;
  if (checksum != raw_data[4]) {
    ESP_LOGE(s_TAG,
             "Checksum error: calculated=0x%02X, received=0x%02X",
             checksum,
             raw_data[4]);
    return DHT22_ERR_CHECKSUM;
  }

  // Parse humidity (16-bit, 0.1% resolution)
  uint16_t humidity_raw = ((uint16_t)raw_data[0] << 8) | raw_data[1];
  data->humidity_rh = humidity_raw / 10.0f;

  // Parse temperature (16-bit, 0.1°C resolution, MSB = sign)
  uint16_t temp_raw = ((uint16_t)(raw_data[2] & 0x7F) << 8) | raw_data[3];
  data->temperature_c = temp_raw / 10.0f;
  if (raw_data[2] & 0x80) {
    data->temperature_c = -data->temperature_c;  // Negative temperature
  }

  // Range validation
  if (data->humidity_rh < 0.0f || data->humidity_rh > 100.0f) {
    ESP_LOGW(s_TAG, "Humidity out of range: %.1f%%", data->humidity_rh);
    return ESP_ERR_INVALID_RESPONSE;
  }
  if (data->temperature_c < -40.0f || data->temperature_c > 80.0f) {
    ESP_LOGW(s_TAG, "Temperature out of range: %.1f°C", data->temperature_c);
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Update last read time and cache data
  handle->last_read_time = (uint32_t)(esp_timer_get_time() / 1000);
  handle->last_data = *data;

  ESP_LOGD(s_TAG, "Temp: %.1f°C, Humidity: %.1f%%", data->temperature_c, data->humidity_rh);

  return ESP_OK;
}

esp_err_t star_sensor_dht22_get_last_reading(const dht22_handle_t* handle, dht22_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->last_read_time == 0) {
    ESP_LOGW(s_TAG, "No reading available yet");
    return ESP_ERR_INVALID_STATE;
  }

  *data = handle->last_data;
  return ESP_OK;
}
