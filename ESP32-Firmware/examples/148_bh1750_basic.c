/**
 * @file 148_bh1750_basic.c
 * @brief Basic BH1750 ambient light sensor example
 *
 * Demonstrates:
 * - Initializing BH1750 via I2C
 * - Different measurement modes
 * - Lux reading and light level categorization
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_bh1750.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "BH1750_BASIC";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define BH1750_ADDR     (0x23)  /* ADDR pin low */

void bh1750_basic_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Light_Manager", NULL, NULL);

  /* Create I2C config */
  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "bh1750", I2C_NUM_0, BH1750_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize BH1750 */
  bh1750_handle_t sensor;
  bh1750_config_t cfg = {
    .i2c_addr = BH1750_ADDR,
    .mode = BH1750_MODE_CONTINUOUS,
    .resolution = BH1750_RES_HIGH,
    .mtreg = BH1750_DEFAULT_MTREG
  };

  ret = star_sensor_bh1750_init(&sensor, &manager, "bh1750", &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init BH1750");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(TAG, "BH1750 initialized");

  /* Main reading loop */
  for (int i = 0; i < 50; i++) {
    float lux;
    ret = star_sensor_bh1750_read_light(&sensor, &lux);

    if (ret == ESP_OK) {
      const char* level;
      if (lux < 1.0f) {
        level = "Very Dark";
      } else if (lux < 50.0f) {
        level = "Dark";
      } else if (lux < 200.0f) {
        level = "Dim";
      } else if (lux < 500.0f) {
        level = "Normal Indoor";
      } else if (lux < 1000.0f) {
        level = "Bright Indoor";
      } else if (lux < 10000.0f) {
        level = "Overcast Outdoor";
      } else if (lux < 50000.0f) {
        level = "Daylight";
      } else {
        level = "Direct Sunlight";
      }

      ESP_LOGI(TAG, "Light: %.1f lux (%s)", lux, level);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  /* Cleanup */
  star_sensor_bh1750_deinit(&sensor);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(TAG, "Example complete");
}

void app_main(void)
{
  bh1750_basic_example();
}
