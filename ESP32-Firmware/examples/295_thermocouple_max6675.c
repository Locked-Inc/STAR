/**
 * @file 295_thermocouple_max6675.c
 * @brief MAX6675 K-type thermocouple
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "THERMO";

#define MAX6675_CS (GPIO_NUM_5)

void thermocouple_max6675_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Thermo_Manager", NULL, NULL);

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = MAX6675_CS,
    .queue_size = 1,
  };
  star_bus_config_t *spi = star_bus_config_create_spi_device(
    "max6675", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg);
  star_bus_manager_add_bus(&manager, spi);
  star_bus_config_init(spi, &manager);

  ESP_LOGI(s_tag, "MAX6675 thermocouple started");

  for (int i = 0; i < 100; i++) {
    uint8_t rx[2] = {0};
    star_bus_spi_receive(&manager, "max6675", rx, 2, 0);

    uint16_t data = (rx[0] << 8) | rx[1];

    if (data & 0x04) {
      ESP_LOGI(s_tag, "Thermocouple disconnected!");
    } else {
      float temp_c = (data >> 3) * 0.25f;
      float temp_f = temp_c * 9.0f / 5.0f + 32.0f;

      ESP_LOGI(s_tag, "Temperature: %.2fC (%.2fF)", temp_c, temp_f);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { thermocouple_max6675_example(); }
