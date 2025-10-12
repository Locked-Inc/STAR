/* esp32-firmware/main/main.c */

#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "pynq_wifi_handler.h"
#include "pynq_wifi_uart.h"
#include "star_pin_validator.h"

static const char* TAG = "main";

/**
 * @brief Callback for received packets from UART transport
 * @param packet Received packet
 */
static void on_packet_received(protocol_packet_t* packet)
{
  command_handler_process(packet);
}

void app_main(void)
{
  ESP_LOGI(TAG, "PYNQ WiFi Bridge Firmware Starting");

  /* Initialize NVS (required for WiFi) */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /* Initialize command handler */
  if (!command_handler_init()) {
    ESP_LOGE(TAG, "Failed to initialize command handler");
    return;
  }

#ifdef CONFIG_PYNQ_TRANSPORT_UART
  /* Initialize UART transport */
  if (!uart_transport_init(on_packet_received)) {
    ESP_LOGE(TAG, "Failed to initialize UART transport");
    return;
  }

  ESP_LOGI(TAG, "UART transport initialized");
#endif

#ifdef CONFIG_PYNQ_TRANSPORT_SPI
  /* TODO: Initialize SPI transport */
  ESP_LOGW(TAG, "SPI transport not yet implemented");
#endif

  /* Validate all registered pins for conflicts */
  ret = star_validate_pins();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Pin validation failed! Check logs for conflicts.");
    return;
  }

  ESP_LOGI(TAG, "PYNQ WiFi Bridge Firmware Ready");
}
