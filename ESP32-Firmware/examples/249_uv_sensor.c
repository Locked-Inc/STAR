/**
 * @file 249_uv_sensor.c
 * @brief VEML6070 UV sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "UV_SENSOR";

#define VEML6070_ADDR_CMD (0x38)
#define VEML6070_ADDR_MSB (0x39)

void uv_sensor_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "UV_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "veml6070", I2C_NUM_0, VEML6070_ADDR_CMD, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Init: integration time 1T */
  uint8_t cmd = 0x02;
  size_t bytes_written;
  star_bus_i2c_write(&manager, "veml6070", &cmd, 1, 0x00, &bytes_written);

  ESP_LOGI(s_tag, "VEML6070 UV sensor started");

  size_t bytes_read;

  for (int i = 0; i < 100; i++) {
    uint8_t msb, lsb;
    star_bus_i2c_read(&manager, "veml6070", &lsb, 1, 0x00, &bytes_read);

    /* Read MSB from different address */
    star_bus_config_t *i2c_msb = star_bus_config_create_i2c(
      "veml_msb", I2C_NUM_0, VEML6070_ADDR_MSB, GPIO_NUM_21, GPIO_NUM_22, 100000);
    star_bus_manager_add_bus(&manager, i2c_msb);
    star_bus_config_init(i2c_msb, &manager);
    star_bus_i2c_read(&manager, "veml_msb", &msb, 1, 0x00, &bytes_read);

    uint16_t uv_raw = (msb << 8) | lsb;
    int uv_index = uv_raw / 100;

    ESP_LOGI(s_tag, "UV raw: %d, UV Index: %d", uv_raw, uv_index);

    if (uv_index >= 11) ESP_LOGI(s_tag, "EXTREME UV - Avoid sun!");
    else if (uv_index >= 8) ESP_LOGI(s_tag, "Very high UV - Use protection");
    else if (uv_index >= 6) ESP_LOGI(s_tag, "High UV - Sunscreen recommended");
    else if (uv_index >= 3) ESP_LOGI(s_tag, "Moderate UV");
    else ESP_LOGI(s_tag, "Low UV");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { uv_sensor_example(); }
