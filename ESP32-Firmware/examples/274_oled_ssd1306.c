/**
 * @file 274_oled_ssd1306.c
 * @brief SSD1306 128x64 OLED display
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "OLED";

#define SSD1306_ADDR (0x3C)

static star_bus_config_t *oled_bus;
static star_bus_manager_t *oled_manager;

static void priv_oled_cmd(uint8_t cmd) {
  uint8_t buf[2] = {0x00, cmd};
  size_t bytes_written;
  star_bus_i2c_write(oled_manager, "ssd1306", buf, 2, 0x00, &bytes_written);
}

static void priv_oled_init(void) {
  priv_oled_cmd(0xAE);  /* Display off */
  priv_oled_cmd(0xD5); priv_oled_cmd(0x80);  /* Clock */
  priv_oled_cmd(0xA8); priv_oled_cmd(0x3F);  /* Multiplex */
  priv_oled_cmd(0xD3); priv_oled_cmd(0x00);  /* Offset */
  priv_oled_cmd(0x40);  /* Start line */
  priv_oled_cmd(0x8D); priv_oled_cmd(0x14);  /* Charge pump */
  priv_oled_cmd(0x20); priv_oled_cmd(0x00);  /* Memory mode */
  priv_oled_cmd(0xA1);  /* Segment remap */
  priv_oled_cmd(0xC8);  /* COM scan */
  priv_oled_cmd(0xDA); priv_oled_cmd(0x12);  /* COM pins */
  priv_oled_cmd(0x81); priv_oled_cmd(0xCF);  /* Contrast */
  priv_oled_cmd(0xD9); priv_oled_cmd(0xF1);  /* Pre-charge */
  priv_oled_cmd(0xDB); priv_oled_cmd(0x40);  /* VCOMH */
  priv_oled_cmd(0xA4);  /* Display RAM */
  priv_oled_cmd(0xA6);  /* Normal display */
  priv_oled_cmd(0xAF);  /* Display on */
}

static void priv_oled_clear(void) {
  uint8_t buf[17];
  buf[0] = 0x40;
  memset(buf + 1, 0x00, 16);

  priv_oled_cmd(0x21); priv_oled_cmd(0); priv_oled_cmd(127);  /* Column */
  priv_oled_cmd(0x22); priv_oled_cmd(0); priv_oled_cmd(7);    /* Page */

  size_t bytes_written;
  for (int i = 0; i < 64; i++) {
    star_bus_i2c_write(oled_manager, "ssd1306", buf, 17, 0x40, &bytes_written);
  }
}

void oled_ssd1306_example(void)
{
  static star_bus_manager_t manager;
  star_bus_manager_init(&manager, "OLED_Manager", NULL, NULL);
  oled_manager = &manager;

  oled_bus = star_bus_config_create_i2c(
    "ssd1306", I2C_NUM_0, SSD1306_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, oled_bus);
  star_bus_config_init(oled_bus, &manager);

  priv_oled_init();
  priv_oled_clear();

  ESP_LOGI(s_tag, "SSD1306 OLED initialized");

  /* Draw pattern */
  uint8_t buf[17];
  buf[0] = 0x40;
  for (int page = 0; page < 8; page++) {
    priv_oled_cmd(0xB0 + page);
    priv_oled_cmd(0x00);
    priv_oled_cmd(0x10);
    memset(buf + 1, 0xAA << (page % 2), 16);
    size_t bytes_written;
    for (int i = 0; i < 8; i++) {
      star_bus_i2c_write(&manager, "ssd1306", buf, 17, 0x40, &bytes_written);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(5000));
  star_bus_manager_deinit(&manager);
}

void app_main(void) { oled_ssd1306_example(); }
