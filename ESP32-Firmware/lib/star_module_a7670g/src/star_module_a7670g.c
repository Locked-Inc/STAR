/* lib/star_module_a7670g/src/star_module_a7670g.c */

#include "star_module_a7670g.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

static const char* s_TAG = "a7670g";

/**
 * @brief Validate and sanitize string for AT command injection prevention
 * @return true if string is safe, false if contains dangerous characters
 */
static bool priv_validate_at_string(const char* str, size_t max_len)
{
  if (str == NULL) {
    return false;
  }
  size_t len = strlen(str);
  if (len == 0 || len > max_len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    char c = str[i];
    // Reject control characters and AT command delimiters
    if (c == '"' || c == '\r' || c == '\n' || c == '\x1A' || c < 0x20) {
      ESP_LOGW(s_TAG, "Invalid character in AT string: 0x%02X", (unsigned char)c);
      return false;
    }
  }
  return true;
}

/**
 * @brief Validate phone number format
 * @return true if valid phone number
 */
static bool validate_phone_number(const char* number)
{
  if (number == NULL) {
    return false;
  }
  size_t len = strlen(number);
  if (len < 3 || len > 20) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    char c = number[i];
    // Allow digits, plus sign (first char only), and common separators
    if (c >= '0' && c <= '9') continue;
    if (c == '+' && i == 0) continue;
    if (c == '-' || c == ' ' || c == '(' || c == ')') continue;
    return false;
  }
  return true;
}

esp_err_t star_module_a7670g_send_at_command(a7670g_handle_t* handle,
                                              const char*      command,
                                              char*            response,
                                              size_t           response_len,
                                              uint32_t         timeout_ms)
{
  if (handle == NULL || command == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Clear UART buffer
  uart_flush_input(handle->uart_port);

  // Send command with CRLF
  char cmd_buf[256];
  snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", command);
  int len = uart_write_bytes(handle->uart_port, cmd_buf, strlen(cmd_buf));
  if (len < 0) {
    ESP_LOGE(s_TAG, "Failed to send command: %s", command);
    return ESP_FAIL;
  }

  ESP_LOGD(s_TAG, "TX: %s", command);

  // Wait for response
  if (response != NULL) {
    memset(response, 0, response_len);
    uint32_t start_time = xTaskGetTickCount();
    size_t rx_index = 0;

    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
      uint8_t data;
      int read_len = uart_read_bytes(handle->uart_port, &data, 1, pdMS_TO_TICKS(100));

      if (read_len > 0 && rx_index < (response_len - 1)) {
        response[rx_index++] = data;
        response[rx_index] = '\0';

        // Check for response terminators
        if (strstr(response, "OK\r\n") || strstr(response, "ERROR\r\n") ||
            strstr(response, "SEND OK") || strstr(response, "+CME ERROR:")) {
          ESP_LOGD(s_TAG, "RX: %s", response);
          return ESP_OK;
        }
      }
    }

    ESP_LOGW(s_TAG, "Command timeout: %s", command);
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t star_module_a7670g_init(a7670g_handle_t* handle, const a7670g_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(a7670g_handle_t));
  handle->uart_port = config->uart_port;
  handle->power_pin = config->power_pin;
  handle->reset_pin = config->reset_pin;
  handle->apn = config->apn;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG,
           "Initializing A7670G (UART=%d, TX=%d, RX=%d)",
           config->uart_port,
           config->tx_pin,
           config->rx_pin);

  // Configure UART
  uart_config_t uart_config = {
    .baud_rate = A7670G_UART_BAUD_RATE,
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

  ret = uart_driver_install(config->uart_port, A7670G_UART_BUF_SIZE, 0, 0, NULL, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Configure power pin
  if (config->power_pin != GPIO_NUM_NC) {
    gpio_config_t power_conf = {
      .pin_bit_mask = (1ULL << config->power_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&power_conf);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to configure power pin: %s", esp_err_to_name(ret));
      uart_driver_delete(handle->uart_port);
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
    gpio_set_level(config->power_pin, 0);
  }

  // Configure reset pin (if provided)
  if (config->reset_pin != GPIO_NUM_NC) {
    gpio_config_t reset_conf = {
      .pin_bit_mask = (1ULL << config->reset_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&reset_conf);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to configure reset pin: %s", esp_err_to_name(ret));
      uart_driver_delete(handle->uart_port);
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
    gpio_set_level(config->reset_pin, 1);  // Reset inactive high
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "A7670G initialization successful");

  return ESP_OK;
}

esp_err_t star_module_a7670g_deinit(a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  star_module_a7670g_power_off(handle);
  uart_driver_delete(handle->uart_port);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_module_a7670g_power_on(const a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->power_pin == GPIO_NUM_NC) {
    ESP_LOGW(s_TAG, "Power pin not configured");
    return ESP_ERR_NOT_SUPPORTED;
  }

  // Power on sequence: pull low for 1s
  gpio_set_level(handle->power_pin, 1);
  vTaskDelay(pdMS_TO_TICKS(1000));
  gpio_set_level(handle->power_pin, 0);
  vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for boot

  ESP_LOGI(s_TAG, "Module powered on");

  return ESP_OK;
}

esp_err_t star_module_a7670g_power_off(const a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->power_pin == GPIO_NUM_NC) {
    return ESP_OK;
  }

  // Power off sequence: pull low for 1.5s
  gpio_set_level(handle->power_pin, 1);
  vTaskDelay(pdMS_TO_TICKS(1500));
  gpio_set_level(handle->power_pin, 0);

  ESP_LOGI(s_TAG, "Module powered off");

  return ESP_OK;
}

esp_err_t star_module_a7670g_reset(const a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->reset_pin == GPIO_NUM_NC) {
    ESP_LOGW(s_TAG, "Reset pin not configured");
    return ESP_ERR_NOT_SUPPORTED;
  }

  gpio_set_level(handle->reset_pin, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(handle->reset_pin, 1);
  vTaskDelay(pdMS_TO_TICKS(5000));  // Wait for boot

  ESP_LOGI(s_TAG, "Module reset");

  return ESP_OK;
}

esp_err_t star_module_a7670g_is_ready(a7670g_handle_t* handle, bool* ready)
{
  if (handle == NULL || ready == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[64];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT", response, sizeof(response), 1000);

  *ready = (ret == ESP_OK && strstr(response, "OK") != NULL);

  return ESP_OK;
}

esp_err_t star_module_a7670g_get_network_info(a7670g_handle_t* handle, a7670g_network_info_t* info)
{
  if (handle == NULL || info == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(info, 0, sizeof(a7670g_network_info_t));

  // Get signal quality
  char response[128];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CSQ", response, sizeof(response), 3000);
  if (ret == ESP_OK) {
    int rssi, ber;
    if (sscanf(response, "+CSQ: %d,%d", &rssi, &ber) == 2) {
      info->signal_quality = rssi;
    }
  }

  // Get registration status
  ret = star_module_a7670g_send_at_command(handle, "AT+CREG?", response, sizeof(response), 3000);
  if (ret == ESP_OK) {
    int n, stat;
    if (sscanf(response, "+CREG: %d,%d", &n, &stat) == 2) {
      info->network_status = stat;
    }
  }

  return ESP_OK;
}

esp_err_t star_module_a7670g_attach_gprs(a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[128];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CGATT=1", response, sizeof(response), 10000);

  if (ret != ESP_OK || strstr(response, "OK") == NULL) {
    ESP_LOGE(s_TAG, "Failed to attach GPRS");
    return ESP_FAIL;
  }

  ESP_LOGI(s_TAG, "GPRS attached");

  return ESP_OK;
}

esp_err_t star_module_a7670g_connect_network(a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized || handle->apn == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Validate APN to prevent AT command injection
  if (!priv_validate_at_string(handle->apn, 64)) {
    ESP_LOGE(s_TAG, "Invalid APN string");
    return ESP_ERR_INVALID_ARG;
  }

  char cmd[128];
  char response[256];

  // Set APN
  snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", handle->apn);
  esp_err_t ret = star_module_a7670g_send_at_command(handle, cmd, response, sizeof(response), 5000);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set APN");
    return ret;
  }

  // Activate PDP context
  ret = star_module_a7670g_send_at_command(handle, "AT+CGACT=1,1", response, sizeof(response), 10000);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to activate PDP context");
    return ret;
  }

  ESP_LOGI(s_TAG, "Network connected");

  return ESP_OK;
}

esp_err_t star_module_a7670g_send_sms(a7670g_handle_t* handle,
                                       const char*      number,
                                       const char*      message)
{
  if (handle == NULL || number == NULL || message == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Validate phone number and message to prevent AT command injection
  if (!validate_phone_number(number)) {
    ESP_LOGE(s_TAG, "Invalid phone number format");
    return ESP_ERR_INVALID_ARG;
  }
  if (!priv_validate_at_string(message, 160)) {
    ESP_LOGE(s_TAG, "Invalid SMS message");
    return ESP_ERR_INVALID_ARG;
  }

  char cmd[256];
  char response[128];

  // Set SMS text mode
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CMGF=1", response, sizeof(response), 3000);
  if (ret != ESP_OK) {
    return ret;
  }

  // Set recipient
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", number);
  ret = star_module_a7670g_send_at_command(handle, cmd, response, sizeof(response), 3000);
  if (ret != ESP_OK) {
    return ret;
  }

  // Send message with Ctrl+Z terminator
  snprintf(cmd, sizeof(cmd), "%s\x1A", message);
  ret = star_module_a7670g_send_at_command(handle, cmd, response, sizeof(response), 30000);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to send SMS");
    return ret;
  }

  ESP_LOGI(s_TAG, "SMS sent to %s", number);

  return ESP_OK;
}

esp_err_t star_module_a7670g_gps_start(a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[128];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CGNSPWR=1", response, sizeof(response), 3000);

  if (ret != ESP_OK || strstr(response, "OK") == NULL) {
    ESP_LOGE(s_TAG, "Failed to start GPS");
    return ESP_FAIL;
  }

  ESP_LOGI(s_TAG, "GPS started");

  return ESP_OK;
}

esp_err_t star_module_a7670g_gps_get_position(a7670g_handle_t* handle, a7670g_gps_data_t* gps_data)
{
  if (handle == NULL || gps_data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[256];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CGNSINF", response, sizeof(response), 3000);

  if (ret != ESP_OK) {
    return ret;
  }

  // Parse GNSS info response
  int run_status, fix_status;
  if (sscanf(response, "+CGNSINF: %d,%d", &run_status, &fix_status) >= 2) {
    gps_data->fix_valid = (fix_status == 1);
    // Full parsing would extract lat, lon, alt, speed, satellites from comma-separated values
  }

  return ESP_OK;
}

esp_err_t star_module_a7670g_gps_stop(a7670g_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  char response[128];
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+CGNSPWR=0", response, sizeof(response), 3000);

  if (ret != ESP_OK) {
    return ESP_FAIL;
  }

  ESP_LOGI(s_TAG, "GPS stopped");

  return ESP_OK;
}

esp_err_t star_module_a7670g_http_get(a7670g_handle_t* handle,
                                       const char*      url,
                                       char*            response,
                                       size_t           response_len)
{
  if (handle == NULL || url == NULL || response == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Validate URL to prevent AT command injection
  if (!priv_validate_at_string(url, 200)) {
    ESP_LOGE(s_TAG, "Invalid URL string");
    return ESP_ERR_INVALID_ARG;
  }

  char cmd[256];
  char resp[512];

  // Initialize HTTP service
  esp_err_t ret = star_module_a7670g_send_at_command(handle, "AT+HTTPINIT", resp, sizeof(resp), 3000);
  if (ret != ESP_OK) {
    return ret;
  }

  // Set URL
  snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
  ret = star_module_a7670g_send_at_command(handle, cmd, resp, sizeof(resp), 3000);
  if (ret != ESP_OK) {
    return ret;
  }

  // Perform GET request
  ret = star_module_a7670g_send_at_command(handle, "AT+HTTPACTION=0", resp, sizeof(resp), 30000);
  if (ret != ESP_OK) {
    return ret;
  }

  // Read response
  ret = star_module_a7670g_send_at_command(handle, "AT+HTTPREAD", response, response_len, 10000);

  // Terminate HTTP service
  star_module_a7670g_send_at_command(handle, "AT+HTTPTERM", resp, sizeof(resp), 3000);

  return ret;
}
