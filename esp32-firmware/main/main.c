/* esp32-firmware/main/main.c */

#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "pynq_wifi_handler.h"
#include "pynq_wifi_transport.h"
#include "star_bus_manager.h"
#include "star_pin_validator.h"

static const char* TAG = "main";

/* Global bus manager */
static star_bus_manager_t g_bus_manager;

/**
 * @brief Callback for received packets from transport
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

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_bus_manager, "main");
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize bus manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Initialize command handler */
  if (!command_handler_init()) {
    ESP_LOGE(TAG, "Failed to initialize command handler");
    return;
  }

#ifdef CONFIG_PYNQ_TRANSPORT_UART
  /* Configure UART transport */
  transport_config_t transport_cfg = {
    .type        = k_transport_uart,
    .bus_manager = &g_bus_manager,
    .config =
      {
        .uart =
          {
            .bus_name    = "pynq_uart",
            .baud_rate   = CONFIG_PYNQ_UART_BAUD_RATE,
            .tx_pin      = CONFIG_PYNQ_UART_TX_GPIO,
            .rx_pin      = CONFIG_PYNQ_UART_RX_GPIO,
            .rx_buf_size = 2048,
            .tx_buf_size = 2048,
          },
      },
  };

  /* Initialize transport */
  if (!transport_init(&transport_cfg, on_packet_received)) {
    ESP_LOGE(TAG, "Failed to initialize UART transport");
    return;
  }

  ESP_LOGI(TAG, "UART transport initialized");
#endif

#ifdef CONFIG_PYNQ_TRANSPORT_SPI
  /* Configure SPI transport */
  transport_config_t transport_cfg = {
    .type        = k_transport_spi,
    .bus_manager = &g_bus_manager,
    .config =
      {
        .spi =
          {
            .bus_name        = "pynq_spi",
            .spi_host        = CONFIG_PYNQ_SPI_HOST,
            .copi_pin        = CONFIG_PYNQ_SPI_COPI_GPIO,
            .cipo_pin        = CONFIG_PYNQ_SPI_CIPO_GPIO,
            .sclk_pin        = CONFIG_PYNQ_SPI_SCLK_GPIO,
            .cs_pin          = CONFIG_PYNQ_SPI_CS_GPIO,
            .transaction_len = 2048,
            .queue_size      = 3,
            .mode            = 0,
          },
      },
  };

  /* Initialize transport */
  if (!transport_init(&transport_cfg, on_packet_received)) {
    ESP_LOGE(TAG, "Failed to initialize SPI transport");
    return;
  }

  ESP_LOGI(TAG, "SPI transport initialized");
#endif

  /* Validate all registered pins for conflicts */
  ret = star_validate_pins();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Pin validation failed! Check logs for conflicts.");
    return;
  }

  ESP_LOGI(TAG, "PYNQ WiFi Bridge Firmware Ready");
}
