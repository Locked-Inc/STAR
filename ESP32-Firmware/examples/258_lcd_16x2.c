/**
 * @file 258_lcd_16x2.c
 * @brief HD44780 16x2 LCD with I2C backpack
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "LCD16X2";

#define PCF8574_ADDR (0x27)
#define LCD_BACKLIGHT (0x08)
#define LCD_EN (0x04)
#define LCD_RS (0x01)

static star_bus_manager_t *g_manager;
static size_t bytes_written;

static void priv_lcd_pulse_enable(uint8_t data) {
  uint8_t d = data | LCD_EN | LCD_BACKLIGHT;
  star_bus_i2c_write(g_manager, "lcd", &d, 1, 0x00, &bytes_written);
  esp_rom_delay_us(1);
  d = (data & ~LCD_EN) | LCD_BACKLIGHT;
  star_bus_i2c_write(g_manager, "lcd", &d, 1, 0x00, &bytes_written);
  esp_rom_delay_us(50);
}

static void priv_lcd_write_4bits(uint8_t data) {
  priv_lcd_pulse_enable(data);
}

static void priv_lcd_send(uint8_t data, uint8_t mode) {
  priv_lcd_write_4bits((data & 0xF0) | mode);
  priv_lcd_write_4bits(((data << 4) & 0xF0) | mode);
}

static void priv_lcd_command(uint8_t cmd) { priv_lcd_send(cmd, 0); }
static void priv_lcd_char(char c) { priv_lcd_send(c, LCD_RS); }

static void priv_lcd_init(void) {
  vTaskDelay(pdMS_TO_TICKS(50));
  priv_lcd_write_4bits(0x30); vTaskDelay(pdMS_TO_TICKS(5));
  priv_lcd_write_4bits(0x30); esp_rom_delay_us(150);
  priv_lcd_write_4bits(0x30);
  priv_lcd_write_4bits(0x20);  /* 4-bit mode */
  priv_lcd_command(0x28);  /* 2 lines, 5x8 */
  priv_lcd_command(0x0C);  /* Display on, cursor off */
  priv_lcd_command(0x06);  /* Entry mode */
  priv_lcd_command(0x01);  /* Clear */
  vTaskDelay(pdMS_TO_TICKS(2));
}

static void priv_lcd_print(const char *str) {
  while (*str) priv_lcd_char(*str++);
}

static void priv_lcd_set_cursor(int row, int col) {
  priv_lcd_command(0x80 | (row ? 0x40 : 0x00) | col);
}

void lcd_16x2_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "LCD_Manager", NULL, NULL);
  g_manager = &manager;

  star_bus_config_t *lcd_bus = star_bus_config_create_i2c(
    "lcd", I2C_NUM_0, PCF8574_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, lcd_bus);
  star_bus_config_init(lcd_bus, &manager);

  priv_lcd_init();
  ESP_LOGI(s_tag, "16x2 LCD initialized");

  priv_lcd_set_cursor(0, 0);
  priv_lcd_print("STAR Firmware");
  priv_lcd_set_cursor(1, 0);
  priv_lcd_print("ESP32 Ready!");

  for (int i = 0; i < 100; i++) {
    priv_lcd_set_cursor(1, 13);
    char buf[4];
    snprintf(buf, 4, "%3d", i);
    priv_lcd_print(buf);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { lcd_16x2_example(); }
