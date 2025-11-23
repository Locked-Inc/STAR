/**
 * @file 257_segment_display.c
 * @brief TM1637 4-digit 7-segment display
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "7SEG";

#define TM1637_CLK (GPIO_NUM_25)
#define TM1637_DIO (GPIO_NUM_26)

static const uint8_t digits[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void priv_tm1637_start(void) {
  gpio_set_level(TM1637_DIO, 0);
  esp_rom_delay_us(2);
}

static void priv_tm1637_stop(void) {
  gpio_set_level(TM1637_CLK, 0);
  esp_rom_delay_us(2);
  gpio_set_level(TM1637_DIO, 0);
  esp_rom_delay_us(2);
  gpio_set_level(TM1637_CLK, 1);
  esp_rom_delay_us(2);
  gpio_set_level(TM1637_DIO, 1);
}

static void priv_tm1637_write_byte(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    gpio_set_level(TM1637_CLK, 0);
    gpio_set_level(TM1637_DIO, byte & (1 << i) ? 1 : 0);
    esp_rom_delay_us(3);
    gpio_set_level(TM1637_CLK, 1);
    esp_rom_delay_us(3);
  }
  gpio_set_level(TM1637_CLK, 0);
  gpio_set_direction(TM1637_DIO, GPIO_MODE_INPUT);
  esp_rom_delay_us(5);
  gpio_set_level(TM1637_CLK, 1);
  esp_rom_delay_us(2);
  gpio_set_direction(TM1637_DIO, GPIO_MODE_OUTPUT);
}

static void priv_tm1637_display(int num, bool colon) {
  uint8_t segs[4] = {
    digits[(num / 1000) % 10],
    digits[(num / 100) % 10] | (colon ? 0x80 : 0),
    digits[(num / 10) % 10],
    digits[num % 10]
  };

  priv_tm1637_start();
  priv_tm1637_write_byte(0x40);  /* Data command */
  priv_tm1637_stop();

  priv_tm1637_start();
  priv_tm1637_write_byte(0xC0);  /* Address command */
  for (int i = 0; i < 4; i++) priv_tm1637_write_byte(segs[i]);
  priv_tm1637_stop();

  priv_tm1637_start();
  priv_tm1637_write_byte(0x8F);  /* Display on, max brightness */
  priv_tm1637_stop();
}

void segment_display_example(void)
{
  gpio_set_direction(TM1637_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction(TM1637_DIO, GPIO_MODE_OUTPUT);
  gpio_set_level(TM1637_CLK, 1);
  gpio_set_level(TM1637_DIO, 1);

  ESP_LOGI(s_tag, "7-segment display started");

  /* Count up */
  for (int i = 0; i <= 9999; i += 7) {
    priv_tm1637_display(i, (i / 100) % 2);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void app_main(void) { segment_display_example(); }
