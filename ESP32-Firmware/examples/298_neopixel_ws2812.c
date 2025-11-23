/**
 * @file 298_neopixel_ws2812.c
 * @brief WS2812B addressable LED strip
 */

#include <esp_log.h>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "NEOPIXEL";

#define LED_PIN (GPIO_NUM_18)
#define NUM_LEDS (30)
#define RMT_CHANNEL (RMT_CHANNEL_0)

static uint8_t s_led_data[NUM_LEDS * 3];

static void priv_ws2812_write(void) {
  rmt_item32_t items[NUM_LEDS * 24 + 1];
  int idx = 0;

  for (int led = 0; led < NUM_LEDS; led++) {
    uint8_t g = s_led_data[led * 3 + 0];
    uint8_t r = s_led_data[led * 3 + 1];
    uint8_t b = s_led_data[led * 3 + 2];
    uint32_t color = (g << 16) | (r << 8) | b;

    for (int bit = 23; bit >= 0; bit--) {
      if (color & (1 << bit)) {
        items[idx].duration0 = 8;
        items[idx].level0 = 1;
        items[idx].duration1 = 4;
        items[idx].level1 = 0;
      } else {
        items[idx].duration0 = 4;
        items[idx].level0 = 1;
        items[idx].duration1 = 8;
        items[idx].level1 = 0;
      }
      idx++;
    }
  }

  items[idx].duration0 = 0;
  items[idx].level0 = 0;
  items[idx].duration1 = 0;
  items[idx].level1 = 0;

  rmt_write_items(RMT_CHANNEL, items, idx + 1, true);
}

static void priv_set_pixel(int n, uint8_t r, uint8_t g, uint8_t b) {
  if (n < NUM_LEDS) {
    s_led_data[n * 3 + 0] = g;
    s_led_data[n * 3 + 1] = r;
    s_led_data[n * 3 + 2] = b;
  }
}

void neopixel_ws2812_example(void)
{
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_PIN, RMT_CHANNEL);
  config.clk_div = 2;
  rmt_config(&config);
  rmt_driver_install(RMT_CHANNEL, 0, 0);

  ESP_LOGI(s_tag, "WS2812B NeoPixel strip started (%d LEDs)", NUM_LEDS);

  /* Rainbow chase */
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int offset = 0; offset < 256; offset += 8) {
      for (int i = 0; i < NUM_LEDS; i++) {
        int hue = (i * 256 / NUM_LEDS + offset) % 256;
        int section = hue / 43;
        int remainder = (hue % 43) * 6;

        uint8_t r, g, b;
        switch (section) {
          case 0: r = 255; g = remainder; b = 0; break;
          case 1: r = 255 - remainder; g = 255; b = 0; break;
          case 2: r = 0; g = 255; b = remainder; break;
          case 3: r = 0; g = 255 - remainder; b = 255; break;
          case 4: r = remainder; g = 0; b = 255; break;
          default: r = 255; g = 0; b = 255 - remainder; break;
        }
        priv_set_pixel(i, r / 4, g / 4, b / 4);
      }
      priv_ws2812_write();
      vTaskDelay(pdMS_TO_TICKS(30));
    }
  }

  /* Clear */
  for (int i = 0; i < NUM_LEDS; i++) priv_set_pixel(i, 0, 0, 0);
  priv_ws2812_write();

  rmt_driver_uninstall(RMT_CHANNEL);
}

void app_main(void) { neopixel_ws2812_example(); }
