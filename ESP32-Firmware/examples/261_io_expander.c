/**
 * @file 261_io_expander.c
 * @brief PCF8574 I2C GPIO expander
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "IO_EXPAND";

#define PCF8574_ADDR (0x20)

void io_expander_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IO_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "pcf8574", I2C_NUM_0, PCF8574_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "PCF8574 I/O expander started");

  /* Set all outputs high (inputs with pull-up) */
  uint8_t output = 0xFF;
  size_t bytes_written;
  size_t bytes_read;
  star_bus_i2c_write(&manager, "pcf8574", &output, 1, 0x00, &bytes_written);

  for (int i = 0; i < 100; i++) {
    /* Read inputs */
    uint8_t input;
    star_bus_i2c_read(&manager, "pcf8574", &input, 1, 0x00, &bytes_read);
    ESP_LOGI(s_tag, "Input: 0x%02X", input);

    /* LED pattern on lower 4 bits */
    output = (output & 0xF0) | ((i % 16) & 0x0F);
    star_bus_i2c_write(&manager, "pcf8574", &output, 1, 0x00, &bytes_written);

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { io_expander_example(); }
