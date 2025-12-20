/**
 * @file main.c
 * @brief Application entry point and system initialization for STAR ESP32 firmware
 * @details
 * Main entry point for ESP32 firmware. Initializes the bus manager, temperature sensor,
 * ARQ communication layer, and telemetry task. The system handles temperature readings
 * from DS18B20 sensor and communicates with Raspberry Pi 5 over SPI using Protocol Buffers.
 *
 * Architecture:
 * - Bus Manager: Unified I2C/SPI/1-Wire abstraction layer
 * - DS18B20: Temperature sensor communication over 1-Wire protocol
 * - ARQ Layer: Reliable SPI peripheral communication with RPi5 controller
 * - Telemetry Task: Handles Protocol Buffer message processing and responses
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "main.h"

#include <string.h>

#include "esp_log.h"
#include "star_arq.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_sensor_ds18b20.h"
#include "tasks/telemetry_task.h"

static const char* s_tag = "MAIN";

void app_main(void)
{
  ESP_LOGI(s_tag, "STAR Temperature System Starting...");

  /* 1. Initialize bus manager */
  star_bus_manager_t bus_manager;
  esp_err_t          ret = star_bus_manager_init(&bus_manager, "main", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Bus manager init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 2. Create OneWire bus for DS18B20 */
  star_bus_config_t* onewire =
    star_bus_config_create_onewire("temp_onewire",
                                   CONFIG_STAR_DS18B20_DATA_GPIO,
                                   false /* External power (not parasitic) */);
  star_bus_manager_add_bus(&bus_manager, onewire);

  /* 3. Create SPI3 peripheral for RPi5 communication */
  star_bus_config_t* spi = star_bus_config_create_spi_peripheral(
    "rpi_spi",
    SPI3_HOST,
    CONFIG_STAR_SPI_COPI_GPIO, /* Controller Out, Peripheral In */
    CONFIG_STAR_SPI_CIPO_GPIO, /* Controller In, Peripheral Out */
    CONFIG_STAR_SPI_CLK_GPIO,  /* Serial Clock */
    CONFIG_STAR_SPI_CS_GPIO,   /* Chip Select */
    k_spi_queue_size,
    0 /* SPI mode 0 */);
  star_bus_manager_add_bus(&bus_manager, spi);

  /* 4. Initialize DS18B20 temperature sensor */
  static star_ds18b20_handle_t temp_sensor;
  star_ds18b20_config_t        temp_cfg = {
           .bus_manager = &bus_manager,
           .bus_name    = "temp_onewire",
           .resolution = k_star_ds18b20_resolution_12_bit, /* 0.0625°C precision, 750ms conversion */
           .use_rom = false,                               /* Skip ROM addressing (single sensor) */
  };
  ret = star_sensor_ds18b20_init(&temp_sensor, &temp_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "DS18B20 init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 5. Initialize ARQ layer */
  static star_arq_handle_t arq;
  star_arq_config_t        arq_cfg = {
           .bus_manager      = &bus_manager,
           .spi_bus_name     = "rpi_spi",
           .max_retries      = k_arq_max_retries,
           .retry_timeout_ms = k_arq_retry_timeout_ms,
  };
  ret = star_arq_init(&arq, &arq_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "ARQ init failed: %s", esp_err_to_name(ret));
    return;
  }

  /* 6. Create telemetry task */
  telemetry_task_create(&temp_sensor, &arq);

  ESP_LOGI(s_tag, "Initialization complete. System running.");

  /* app_main returns, freeing main task stack */
}
