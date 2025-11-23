/**
 * @file 266_color_tcs34725.c
 * @brief TCS34725 RGB color sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "COLOR";

#define TCS34725_ADDR (0x29)

void color_tcs34725_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Color_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "tcs34725", I2C_NUM_0, TCS34725_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Enable device */
  uint8_t enable[] = {0x03};
  size_t bytes_written;
  star_bus_i2c_write(&manager, "tcs34725", enable, 1, 0x80 | 0x00, &bytes_written);
  vTaskDelay(pdMS_TO_TICKS(50));

  ESP_LOGI(s_tag, "TCS34725 color sensor started");

  for (int i = 0; i < 100; i++) {
    uint8_t data[8];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "tcs34725", data, 8, 0x80 | 0x20 | 0x14, &bytes_read);  /* Auto-increment, CDATA */

    uint16_t c = data[0] | (data[1] << 8);
    uint16_t r = data[2] | (data[3] << 8);
    uint16_t g = data[4] | (data[5] << 8);
    uint16_t b = data[6] | (data[7] << 8);

    ESP_LOGI(s_tag, "R:%d G:%d B:%d Clear:%d", r, g, b, c);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { color_tcs34725_example(); }
