/**
 * @file 263_dac_mcp4725.c
 * @brief MCP4725 12-bit I2C DAC
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "DAC";

#define MCP4725_ADDR (0x60)

void dac_mcp4725_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "DAC_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mcp4725", I2C_NUM_0, MCP4725_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "MCP4725 12-bit DAC started");

  /* Generate sine wave */
  for (int cycle = 0; cycle < 10; cycle++) {
    for (int i = 0; i < 360; i += 5) {
      float rad = i * 3.14159f / 180.0f;
      uint16_t value = (uint16_t)((sinf(rad) + 1.0f) * 2047.5f);

      uint8_t cmd[2] = {(value >> 4) & 0xFF, (value << 4) & 0xF0};
      size_t written;
      star_bus_i2c_write(&manager, "mcp4725", cmd, 2, 0x40, &written);

      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { dac_mcp4725_example(); }
