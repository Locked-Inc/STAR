/**
 * @file 154_a7670g_basic.c
 * @brief A7670G 4G LTE module basic example
 *
 * Demonstrates:
 * - Module initialization and AT commands
 * - Network registration
 * - Signal quality check
 */

#include "star_module_a7670g.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "A7670G_EXAMPLE";

#define A7670G_TX_PIN       (GPIO_NUM_17)
#define A7670G_RX_PIN       (GPIO_NUM_16)
#define A7670G_POWER_PIN    (GPIO_NUM_4)
#define A7670G_UART_PORT    (UART_NUM_2)

void a7670g_example(void)
{
  esp_err_t ret;

  a7670g_handle_t module;
  a7670g_config_t cfg = {
    .uart_port = A7670G_UART_PORT,
    .tx_pin = A7670G_TX_PIN,
    .rx_pin = A7670G_RX_PIN,
    .power_pin = A7670G_POWER_PIN,
    .reset_pin = GPIO_NUM_NC,
    .apn = "internet"  /* Adjust for your carrier */
  };

  ret = star_module_a7670g_init(&module, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init A7670G");
    return;
  }

  /* Power on module */
  ESP_LOGI(s_tag, "Powering on module...");
  star_module_a7670g_power_on(&module);

  /* Wait for module to be ready */
  bool ready = false;
  for (int i = 0; i < 30; i++) {
    star_module_a7670g_is_ready(&module, &ready);
    if (ready) break;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  if (!ready) {
    ESP_LOGE(s_tag, "Module not responding");
    star_module_a7670g_deinit(&module);
    return;
  }

  ESP_LOGI(s_tag, "Module ready");

  /* Get network info */
  a7670g_network_info_t info;
  star_module_a7670g_get_network_info(&module, &info);

  const char *reg_status;
  switch (info.network_status) {
    case A7670G_NET_STATUS_UNKNOWN: reg_status = "Not registered"; break;
    case A7670G_NET_STATUS_REGISTERED_HOME: reg_status = "Registered (home)"; break;
    case A7670G_NET_STATUS_SEARCHING: reg_status = "Searching"; break;
    case A7670G_NET_STATUS_DENIED: reg_status = "Registration denied"; break;
    case A7670G_NET_STATUS_REGISTERED_ROAMING: reg_status = "Registered (roaming)"; break;
    default: reg_status = "Unknown"; break;
  }

  ESP_LOGI(s_tag, "Network: %s, Signal: %d", reg_status, info.signal_quality);

  /* Connect to network */
  if (info.network_status == A7670G_NET_STATUS_REGISTERED_HOME ||
      info.network_status == A7670G_NET_STATUS_REGISTERED_ROAMING) {
    ret = star_module_a7670g_attach_gprs(&module);
    if (ret == ESP_OK) {
      ret = star_module_a7670g_connect_network(&module);
      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Connected to network!");
      }
    }
  }

  star_module_a7670g_power_off(&module);
  star_module_a7670g_deinit(&module);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { a7670g_example(); }
