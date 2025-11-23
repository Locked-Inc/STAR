/* lib/star_sensor_rplidar_c1/src/star_sensor_rplidar_c1.c */

#include "star_sensor_rplidar_c1.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

static const char* s_TAG = "rplidar";

// Send command to RPLiDAR
static esp_err_t send_command(rplidar_handle_t* handle, uint8_t cmd, const uint8_t* payload, uint8_t payload_len)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t buffer[16];
  uint8_t index = 0;

  // Start flag
  buffer[index++] = RPLIDAR_SYNC_BYTE1;

  // Command byte
  buffer[index++] = cmd;

  // Payload (if any)
  if (payload != NULL && payload_len > 0) {
    if (payload_len > sizeof(buffer) - 3) {
      ESP_LOGE(s_TAG, "Payload too large");
      return ESP_ERR_INVALID_SIZE;
    }
    buffer[index++] = payload_len;
    memcpy(&buffer[index], payload, payload_len);
    index += payload_len;

    // Checksum
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < payload_len; i++) {
      checksum ^= payload[i];
    }
    checksum ^= cmd;
    checksum ^= payload_len;
    buffer[index++] = checksum;
  }

  int len = uart_write_bytes(handle->uart_port, buffer, index);
  if (len < 0) {
    ESP_LOGE(s_TAG, "Failed to send command");
    return ESP_FAIL;
  }

  return ESP_OK;
}

// Parse standard scan response (5 bytes per point)
static esp_err_t parse_scan_response(rplidar_handle_t* handle, const uint8_t* data)
{
  if (handle->scan_count >= handle->scan_capacity) {
    ESP_LOGW(s_TAG, "Scan buffer full");
    return ESP_ERR_NO_MEM;
  }

  // Parse measurement node (standard scan format)
  uint8_t quality = data[0] >> 2;
  bool start_flag = (data[0] & 0x01) ? true : false;
  bool not_start_flag = (data[0] & 0x02) ? false : true;  // Inverted

  if (start_flag != not_start_flag) {
    ESP_LOGW(s_TAG, "Invalid start flag");
    return ESP_ERR_INVALID_RESPONSE;
  }

  uint16_t angle_q6 = ((uint16_t)data[2] << 8) | data[1];
  uint16_t distance_q2 = ((uint16_t)data[4] << 8) | data[3];

  // Convert to physical units (Q6 fixed-point: divide by 64)
  float angle = (float)angle_q6 / 64.0f;
  float distance = (float)distance_q2 / 4000.0f;  // Q2 to meters

  // Store in buffer
  rplidar_scan_point_t* point = &handle->scan_buffer[handle->scan_count];
  point->angle_deg = angle;
  point->distance_m = distance;
  point->quality = quality;
  point->start_flag = start_flag;
  point->timestamp_us = esp_timer_get_time();

  handle->scan_count++;

  // Reset buffer on new rotation
  if (start_flag && handle->scan_count > 1) {
    handle->scan_count = 1;  // Keep current point as first of new rotation
  }

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_init(rplidar_handle_t* handle, const rplidar_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(rplidar_handle_t));
  handle->uart_port = config->uart_port;
  handle->motor_pwm_pin = config->motor_pwm_pin;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG,
           "Initializing RPLiDAR (UART=%d, TX=%d, RX=%d)",
           config->uart_port,
           config->tx_pin,
           config->rx_pin);

  // Configure UART
  uart_config_t uart_config = {
    .baud_rate = RPLIDAR_UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
  };

  ret = uart_param_config(config->uart_port, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = uart_set_pin(config->uart_port,
                     config->tx_pin,
                     config->rx_pin,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = uart_driver_install(config->uart_port, RPLIDAR_UART_BUF_SIZE, 0, 0, NULL, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Configure motor PWM pin if provided
  if (config->motor_pwm_pin != GPIO_NUM_NC) {
    gpio_config_t motor_conf = {
      .pin_bit_mask = (1ULL << config->motor_pwm_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&motor_conf);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to configure motor pin: %s", esp_err_to_name(ret));
      uart_driver_delete(handle->uart_port);
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
    gpio_set_level(config->motor_pwm_pin, 0);  // Motor off initially
  }

  // Allocate default scan buffer (1024 points)
  handle->scan_capacity = 1024;
  handle->scan_buffer = (rplidar_scan_point_t*)malloc(handle->scan_capacity * sizeof(rplidar_scan_point_t));
  if (handle->scan_buffer == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate scan buffer");
    uart_driver_delete(handle->uart_port);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NO_MEM;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "RPLiDAR initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_deinit(rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  star_sensor_rplidar_stop_scan(handle);
  star_sensor_rplidar_stop_motor(handle);

  if (handle->scan_buffer != NULL) {
    free(handle->scan_buffer);
    handle->scan_buffer = NULL;
  }

  uart_driver_delete(handle->uart_port);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_start_motor(const rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->motor_pwm_pin == GPIO_NUM_NC) {
    ESP_LOGW(s_TAG, "Motor control pin not configured");
    return ESP_ERR_NOT_SUPPORTED;
  }

  gpio_set_level(handle->motor_pwm_pin, 1);
  ESP_LOGI(s_TAG, "Motor started");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_stop_motor(const rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->motor_pwm_pin == GPIO_NUM_NC) {
    return ESP_OK;  // No motor control, nothing to do
  }

  gpio_set_level(handle->motor_pwm_pin, 0);
  ESP_LOGI(s_TAG, "Motor stopped");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_start_scan(rplidar_handle_t* handle, bool express_mode)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Stop any existing scan
  if (handle->scanning) {
    star_sensor_rplidar_stop_scan(handle);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Start motor
  star_sensor_rplidar_start_motor(handle);
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for motor to stabilize

  // Send scan command
  uint8_t cmd = express_mode ? RPLIDAR_CMD_EXPRESS_SCAN : RPLIDAR_CMD_SCAN;
  esp_err_t ret = send_command(handle, cmd, NULL, 0);
  if (ret != ESP_OK) {
    return ret;
  }

  handle->scanning = true;
  handle->scan_count = 0;
  ESP_LOGI(s_TAG, "Scan started (%s mode)", express_mode ? "express" : "standard");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_stop_scan(rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = send_command(handle, RPLIDAR_CMD_STOP, NULL, 0);
  if (ret != ESP_OK) {
    return ret;
  }

  handle->scanning = false;
  ESP_LOGI(s_TAG, "Scan stopped");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_process(rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->scanning) {
    return ESP_OK;
  }

  // Read available data
  uint8_t data[5];
  int len = uart_read_bytes(handle->uart_port, data, 5, 0);

  if (len != 5) {
    return ESP_OK;  // Not enough data yet
  }

  // Parse scan response
  return parse_scan_response(handle, data);
}

esp_err_t star_sensor_rplidar_get_scan_data(const rplidar_handle_t* handle,
                                             rplidar_scan_point_t*   points,
                                             uint16_t                max_points,
                                             uint16_t*               count)
{
  if (handle == NULL || points == NULL || count == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t copy_count = (handle->scan_count < max_points) ? handle->scan_count : max_points;

  memcpy(points, handle->scan_buffer, copy_count * sizeof(rplidar_scan_point_t));
  *count = copy_count;

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_reset(rplidar_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = send_command(handle, RPLIDAR_CMD_RESET, NULL, 0);
  if (ret != ESP_OK) {
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for reset to complete
  ESP_LOGI(s_TAG, "Device reset");

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_get_info(rplidar_handle_t* handle, rplidar_device_info_t* info)
{
  if (handle == NULL || info == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = send_command(handle, RPLIDAR_CMD_GET_INFO, NULL, 0);
  if (ret != ESP_OK) {
    return ret;
  }

  // Wait for response (20 bytes)
  uint8_t response[20];
  int len = uart_read_bytes(handle->uart_port, response, 20, pdMS_TO_TICKS(1000));
  if (len != 20) {
    ESP_LOGE(s_TAG, "Failed to read device info");
    return ESP_ERR_TIMEOUT;
  }

  // Parse response
  info->model = response[0];
  info->firmware_minor = response[1];
  info->firmware_major = response[2];
  info->hardware = response[3];
  memcpy(info->serial_number, &response[4], 16);

  ESP_LOGI(s_TAG,
           "Device: Model=%d, FW=%d.%d, HW=%d",
           info->model,
           info->firmware_major,
           info->firmware_minor,
           info->hardware);

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_get_health(rplidar_handle_t* handle, rplidar_health_info_t* health)
{
  if (handle == NULL || health == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = send_command(handle, RPLIDAR_CMD_GET_HEALTH, NULL, 0);
  if (ret != ESP_OK) {
    return ret;
  }

  // Wait for response (3 bytes)
  uint8_t response[3];
  int len = uart_read_bytes(handle->uart_port, response, 3, pdMS_TO_TICKS(1000));
  if (len != 3) {
    ESP_LOGE(s_TAG, "Failed to read health info");
    return ESP_ERR_TIMEOUT;
  }

  // Parse response
  health->status = response[0];
  health->error_code = ((uint16_t)response[2] << 8) | response[1];

  ESP_LOGI(s_TAG, "Health: Status=%d, Error=%d", health->status, health->error_code);

  return ESP_OK;
}

esp_err_t star_sensor_rplidar_set_scan_buffer_size(rplidar_handle_t* handle, uint16_t capacity)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (capacity == 0 || capacity > RPLIDAR_MAX_SCAN_POINTS) {
    ESP_LOGE(s_TAG, "Invalid buffer capacity: %d", capacity);
    return ESP_ERR_INVALID_ARG;
  }

  // Reallocate buffer
  rplidar_scan_point_t* new_buffer = (rplidar_scan_point_t*)realloc(handle->scan_buffer, capacity * sizeof(rplidar_scan_point_t));
  if (new_buffer == NULL) {
    ESP_LOGE(s_TAG, "Failed to reallocate scan buffer");
    return ESP_ERR_NO_MEM;
  }

  handle->scan_buffer = new_buffer;
  handle->scan_capacity = capacity;
  handle->scan_count = 0;

  ESP_LOGI(s_TAG, "Scan buffer resized to %d points", capacity);

  return ESP_OK;
}
