/**
 * @file 262_adc_external.c
 * @brief ADS1115 16-bit external ADC
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ADS1115";

#define ADS1115_ADDR (0x48)

void adc_external_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "ADC_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "ads1115", I2C_NUM_0, ADS1115_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "ADS1115 16-bit ADC started");

  size_t bytes_written;
  size_t bytes_read;

  for (int ch = 0; ch < 4; ch++) {
    for (int i = 0; i < 25; i++) {
      /* Configure for single-ended reading on channel */
      uint16_t config = 0xC183 | ((ch + 4) << 12);
      uint8_t cfg_data[2] = {config >> 8, config & 0xFF};
      star_bus_i2c_write(&manager, "ads1115", cfg_data, 2, 0x01, &bytes_written);

      vTaskDelay(pdMS_TO_TICKS(10));

      /* Read conversion */
      uint8_t data[2];
      star_bus_i2c_read(&manager, "ads1115", data, 2, 0x00, &bytes_read);

      int16_t raw = (data[0] << 8) | data[1];
      float voltage = raw * 4.096f / 32768.0f;

      ESP_LOGI(s_tag, "CH%d: Raw=%d, Voltage=%.4fV", ch, raw, voltage);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { adc_external_example(); }
