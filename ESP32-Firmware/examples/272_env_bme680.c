/**
 * @file 272_env_bme680.c
 * @brief BME680 environmental sensor (temp, humidity, pressure, gas)
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BME680";

#define BME680_ADDR (0x77)

void env_bme680_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Env_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "bme680", I2C_NUM_0, BME680_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure forced mode */
  uint8_t cfg_hum[] = {0x01};   /* humidity oversampling x1 */
  uint8_t cfg_ctrl[] = {0x25};  /* temp x1, press x1, forced mode */
  size_t bytes_written;
  star_bus_i2c_write(&manager, "bme680", cfg_hum, 1, 0x72, &bytes_written);
  star_bus_i2c_write(&manager, "bme680", cfg_ctrl, 1, 0x74, &bytes_written);

  ESP_LOGI(s_tag, "BME680 environmental sensor started");

  for (int i = 0; i < 50; i++) {
    /* Trigger measurement */
    uint8_t trig[] = {0x25};
    star_bus_i2c_write(&manager, "bme680", trig, 1, 0x74, &bytes_written);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read data */
    uint8_t data[8];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "bme680", data, 8, 0x1F, &bytes_read);

    /* Raw values (simplified, needs calibration) */
    int32_t press_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    int32_t hum_raw = (data[6] << 8) | data[7];

    ESP_LOGI(s_tag, "Temp_raw:%ld Press_raw:%ld Hum_raw:%ld",
             temp_raw, press_raw, hum_raw);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { env_bme680_example(); }
