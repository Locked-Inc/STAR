/**
 * @file 219_oled_display.c
 * @brief SSD1306 OLED display with sensor data
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "OLED";

#define SSD1306_ADDR (0x3C)
#define OLED_WIDTH (128)
#define OLED_HEIGHT (64)

static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static const uint8_t font_5x7[][5] = {
  ['0' - ' '] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
  ['1' - ' '] = {0x00, 0x42, 0x7F, 0x40, 0x00},
  ['2' - ' '] = {0x42, 0x61, 0x51, 0x49, 0x46},
  ['3' - ' '] = {0x21, 0x41, 0x45, 0x4B, 0x31},
  ['4' - ' '] = {0x18, 0x14, 0x12, 0x7F, 0x10},
  ['5' - ' '] = {0x27, 0x45, 0x45, 0x45, 0x39},
  ['6' - ' '] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
  ['7' - ' '] = {0x01, 0x71, 0x09, 0x05, 0x03},
  ['8' - ' '] = {0x36, 0x49, 0x49, 0x49, 0x36},
  ['9' - ' '] = {0x06, 0x49, 0x49, 0x29, 0x1E},
  ['.' - ' '] = {0x00, 0x60, 0x60, 0x00, 0x00},
  [':' - ' '] = {0x00, 0x36, 0x36, 0x00, 0x00},
  ['C' - ' '] = {0x3E, 0x41, 0x41, 0x41, 0x22},
  ['H' - ' '] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
  ['T' - ' '] = {0x01, 0x01, 0x7F, 0x01, 0x01},
  ['%' - ' '] = {0x23, 0x13, 0x08, 0x64, 0x62},
  [' ' - ' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
};

static void priv_ssd1306_cmd(uint8_t cmd)
{
  uint8_t buf[2] = {0x00, cmd};
  i2c_master_write_to_device(I2C_NUM_0, SSD1306_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static void priv_ssd1306_init(void)
{
  priv_ssd1306_cmd(0xAE);  /* Display off */
  priv_ssd1306_cmd(0xD5); priv_ssd1306_cmd(0x80);  /* Clock div */
  priv_ssd1306_cmd(0xA8); priv_ssd1306_cmd(0x3F);  /* Multiplex */
  priv_ssd1306_cmd(0xD3); priv_ssd1306_cmd(0x00);  /* Display offset */
  priv_ssd1306_cmd(0x40);  /* Start line */
  priv_ssd1306_cmd(0x8D); priv_ssd1306_cmd(0x14);  /* Charge pump */
  priv_ssd1306_cmd(0x20); priv_ssd1306_cmd(0x00);  /* Memory mode */
  priv_ssd1306_cmd(0xA1);  /* Segment remap */
  priv_ssd1306_cmd(0xC8);  /* COM scan dir */
  priv_ssd1306_cmd(0xDA); priv_ssd1306_cmd(0x12);  /* COM pins */
  priv_ssd1306_cmd(0x81); priv_ssd1306_cmd(0xCF);  /* Contrast */
  priv_ssd1306_cmd(0xD9); priv_ssd1306_cmd(0xF1);  /* Precharge */
  priv_ssd1306_cmd(0xDB); priv_ssd1306_cmd(0x40);  /* VCOMH */
  priv_ssd1306_cmd(0xA4);  /* Resume */
  priv_ssd1306_cmd(0xA6);  /* Normal display */
  priv_ssd1306_cmd(0xAF);  /* Display on */
}

static void priv_ssd1306_update(void)
{
  priv_ssd1306_cmd(0x21); priv_ssd1306_cmd(0); priv_ssd1306_cmd(127);  /* Column */
  priv_ssd1306_cmd(0x22); priv_ssd1306_cmd(0); priv_ssd1306_cmd(7);    /* Page */

  uint8_t buf[OLED_WIDTH + 1];
  buf[0] = 0x40;
  for (int page = 0; page < 8; page++) {
    memcpy(&buf[1], &oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
    i2c_master_write_to_device(I2C_NUM_0, SSD1306_ADDR, buf, OLED_WIDTH + 1, pdMS_TO_TICKS(100));
  }
}

static void priv_draw_char(int x, int y, char c)
{
  if (c < ' ' || c > 'z') return;
  const uint8_t *glyph = font_5x7[c - ' '];
  for (int col = 0; col < 5; col++) {
    for (int row = 0; row < 8; row++) {
      if (glyph[col] & (1 << row)) {
        int px = x + col;
        int py = y + row;
        if (px < OLED_WIDTH && py < OLED_HEIGHT) {
          oled_buffer[(py / 8) * OLED_WIDTH + px] |= (1 << (py % 8));
        }
      }
    }
  }
}

static void priv_draw_string(int x, int y, const char *str)
{
  while (*str) {
    priv_draw_char(x, y, *str++);
    x += 6;
  }
}

void oled_display_example(void)
{
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000
  };
  i2c_param_config(I2C_NUM_0, &conf);
  i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

  priv_ssd1306_init();

  dht22_handle_t dht;
  dht22_config_t dht_cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &dht_cfg);

  ESP_LOGI(s_tag, "OLED display initialized");

  for (int i = 0; i < 100; i++) {
    memset(oled_buffer, 0, sizeof(oled_buffer));

    dht22_data_t data;
    star_sensor_dht22_read(&dht, &data);

    char line[32];
    priv_draw_string(0, 0, "STAR Firmware");
    snprintf(line, sizeof(line), "T: %.1fC", data.temperature_c);
    priv_draw_string(0, 20, line);
    snprintf(line, sizeof(line), "H: %.1f%%", data.humidity_rh);
    priv_draw_string(0, 36, line);

    priv_ssd1306_update();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  star_sensor_dht22_deinit(&dht);
  i2c_driver_delete(I2C_NUM_0);
}

void app_main(void) { oled_display_example(); }
