/**
 * @file 218_neopixel_strip.c
 * @brief WS2812B NeoPixel LED strip control
 */

#include <esp_log.h>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <math.h>

static const char *s_tag = "NEOPIXEL";

#define LED_PIN       (GPIO_NUM_18)
#define NUM_LEDS (30)
#define RMT_CHANNEL   (RMT_CHANNEL_0)

typedef struct {
  uint8_t g, r, b;
} led_color_t;

static led_color_t led_strip[NUM_LEDS];
static rmt_item32_t rmt_items[NUM_LEDS * 24 + 1];

#define T0H 14  /* 0.35us */
#define T0L 36  /* 0.90us */
#define T1H 36  /* 0.90us */
#define T1L 14  /* 0.35us */

static void priv_update_strip(void)
{
  int item_idx = 0;
  for (int led = 0; led < NUM_LEDS; led++) {
    uint8_t *bytes = (uint8_t*)&led_strip[led];
    for (int byte = 0; byte < 3; byte++) {
      for (int bit = 7; bit >= 0; bit--) {
        if (bytes[byte] & (1 << bit)) {
          rmt_items[item_idx].duration0 = T1H;
          rmt_items[item_idx].level0 = 1;
          rmt_items[item_idx].duration1 = T1L;
          rmt_items[item_idx].level1 = 0;
        } else {
          rmt_items[item_idx].duration0 = T0H;
          rmt_items[item_idx].level0 = 1;
          rmt_items[item_idx].duration1 = T0L;
          rmt_items[item_idx].level1 = 0;
        }
        item_idx++;
      }
    }
  }

  rmt_write_items(RMT_CHANNEL, rmt_items, NUM_LEDS * 24, true);
}

static void priv_set_pixel(int idx, uint8_t r, uint8_t g, uint8_t b)
{
  if (idx >= 0 && idx < NUM_LEDS) {
    led_strip[idx].r = r;
    led_strip[idx].g = g;
    led_strip[idx].b = b;
  }
}

static void priv_clear_strip(void)
{
  memset(led_strip, 0, sizeof(led_strip));
}

static uint8_t priv_wheel(uint8_t pos)
{
  if (pos < 85) return pos * 3;
  if (pos < 170) return 255 - (pos - 85) * 3;
  return 0;
}

void neopixel_example(void)
{
  rmt_config_t rmt_cfg = {
    .rmt_mode = RMT_MODE_TX,
    .channel = RMT_CHANNEL,
    .gpio_num = LED_PIN,
    .clk_div = 2,
    .mem_block_num = 1,
    .tx_config = {
      .idle_level = RMT_IDLE_LEVEL_LOW,
      .idle_output_en = true
    }
  };

  rmt_config(&rmt_cfg);
  rmt_driver_install(RMT_CHANNEL, 0, 0);

  ESP_LOGI(s_tag, "NeoPixel strip initialized: %d LEDs", NUM_LEDS);

  /* Rainbow cycle */
  for (int frame = 0; frame < 500; frame++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t pos = (i * 256 / NUM_LEDS + frame) & 0xFF;
      priv_set_pixel(i, priv_wheel(pos), priv_wheel(pos + 85), priv_wheel(pos + 170));
    }
    priv_update_strip();
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  /* Chase effect */
  for (int frame = 0; frame < 200; frame++) {
    priv_clear_strip();
    int pos = frame % NUM_LEDS;
    priv_set_pixel(pos, 255, 255, 255);
    priv_set_pixel((pos + 1) % NUM_LEDS, 100, 100, 100);
    priv_set_pixel((pos + 2) % NUM_LEDS, 25, 25, 25);
    priv_update_strip();
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  priv_clear_strip();
  priv_update_strip();
  rmt_driver_uninstall(RMT_CHANNEL);
}

void app_main(void) { neopixel_example(); }
