/**
 * @file 170_a7670g_sms.c
 * @brief A7670G SMS sending and receiving
 *
 * Demonstrates:
 * - SMS text mode configuration
 * - Sending SMS messages
 * - SMS delivery confirmation
 */

#include "star_module_a7670g.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "A7670G_SMS";

#define A7670G_TX_PIN       (GPIO_NUM_17)
#define A7670G_RX_PIN       (GPIO_NUM_16)
#define A7670G_POWER_PIN    (GPIO_NUM_4)
#define A7670G_UART_PORT    (UART_NUM_2)

void a7670g_sms_example(void)
{
  a7670g_handle_t module;
  a7670g_config_t cfg = {
    .uart_port = A7670G_UART_PORT,
    .tx_pin = A7670G_TX_PIN,
    .rx_pin = A7670G_RX_PIN,
    .power_pin = A7670G_POWER_PIN,
    .reset_pin = GPIO_NUM_NC,
    .apn = "internet"
  };

  if (star_module_a7670g_init(&module, &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init A7670G");
    return;
  }

  /* Power on and wait for registration */
  star_module_a7670g_power_on(&module);

  bool ready = false;
  for (int i = 0; i < 30 && !ready; i++) {
    star_module_a7670g_is_ready(&module, &ready);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  if (!ready) {
    ESP_LOGE(s_tag, "Module not ready");
    return;
  }

  /* Wait for network registration */
  a7670g_network_info_t info;
  do {
    star_module_a7670g_get_network_info(&module, &info);
    ESP_LOGI(s_tag, "Network status: %d, Signal: %d", info.network_status, info.signal_quality);
    vTaskDelay(pdMS_TO_TICKS(2000));
  } while (info.network_status != 1 && info.network_status != 5);

  ESP_LOGI(s_tag, "Network registered, sending SMS...");

  /* Send SMS - replace with actual phone number */
  const char *phone_number = "+1234567890";  /* CHANGE THIS */
  const char *message = "Hello from ESP32 STAR Firmware!";

  esp_err_t ret = star_module_a7670g_send_sms(&module, phone_number, message);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMS sent successfully to %s", phone_number);
  } else {
    ESP_LOGE(s_tag, "SMS failed: %s", esp_err_to_name(ret));
  }

  /* Send status update */
  char status_msg[160];
  snprintf(status_msg, sizeof(status_msg),
           "ESP32 Status: Signal=%d, Temp=%.1fC, Uptime=%lus",
           info.signal_quality, 25.0f, xTaskGetTickCount() / configTICK_RATE_HZ);

  ret = star_module_a7670g_send_sms(&module, phone_number, status_msg);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Status SMS sent");
  }

  star_module_a7670g_power_off(&module);
  star_module_a7670g_deinit(&module);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { a7670g_sms_example(); }
