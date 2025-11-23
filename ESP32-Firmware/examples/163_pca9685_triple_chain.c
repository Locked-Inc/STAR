/**
 * @file 163_pca9685_triple_chain.c
 * @brief Three PCA9685 controllers daisy-chained (48 channels)
 *
 * Demonstrates:
 * - 48-channel PWM control
 * - RGB LED strip control across multiple controllers
 * - Synchronized animations
 *
 * Hardware setup:
 * - PCA9685 #1: Address 0x40 (default)
 * - PCA9685 #2: Address 0x41 (A0 high)
 * - PCA9685 #3: Address 0x42 (A1 high)
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "PCA9685_TRIPLE";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define PCA9685_ADDR_1 (0x40)
#define PCA9685_ADDR_2 (0x41)
#define PCA9685_ADDR_3 (0x42)

#define NUM_CONTROLLERS (3)
#define TOTAL_CHANNELS (48)
#define RGB_LEDS (16) /* 48 channels / 3 colors = 16 RGB LEDs */

typedef struct {
  pca9685_handle_t handles[NUM_CONTROLLERS];
  star_bus_manager_t* manager;
} pwm_chain_t;

static void set_unified_channel(pwm_chain_t* chain, uint8_t channel, uint16_t value)
{
  if (channel >= TOTAL_CHANNELS) return;
  int ctrl_idx = channel / 16;
  int local_ch = channel % 16;
  star_sensor_pca9685_set_pwm(&chain->handles[ctrl_idx], local_ch, 0, value);
}

static void set_rgb_led(pwm_chain_t* chain, uint8_t led_idx, uint16_t r, uint16_t g, uint16_t b)
{
  if (led_idx >= RGB_LEDS) return;
  uint8_t base_ch = led_idx * 3;
  set_unified_channel(chain, base_ch + 0, r);
  set_unified_channel(chain, base_ch + 1, g);
  set_unified_channel(chain, base_ch + 2, b);
}

void pca9685_triple_chain_example(void)
{
  pwm_chain_t chain = {0};

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "PWM_Triple", NULL, NULL);
  chain.manager = &manager;

  /* Create I2C configs for all three controllers */
  uint8_t addresses[] = {PCA9685_ADDR_1, PCA9685_ADDR_2, PCA9685_ADDR_3};
  const char *names[] = {"pca_1", "pca_2", "pca_3"};

  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    star_bus_config_t* i2c = star_bus_config_create_i2c(
      names[i], I2C_NUM_0, addresses[i], I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    star_bus_manager_add_bus(&manager, i2c);
    star_bus_config_init(i2c, &manager);

    pca9685_config_t cfg = {
      .i2c_addr = addresses[i],
      .pwm_freq = 1000,
      .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
      .ext_clock = false,
      .invert_output = false
    };
    if (star_sensor_pca9685_init(&chain.handles[i], &manager, names[i], &cfg) != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to init PCA9685 #%d", i + 1);
      return;
    }
    star_sensor_pca9685_set_frequency(&chain.handles[i], 1000);
  }

  ESP_LOGI(s_tag, "Triple PCA9685 chain ready - 48 channels / 16 RGB LEDs");

  /* Rainbow wave across 16 RGB LEDs */
  ESP_LOGI(s_tag, "Rainbow wave animation");
  for (int frame = 0; frame < 200; frame++) {
    for (int led = 0; led < RGB_LEDS; led++) {
      /* HSV to RGB with phase offset per LED */
      float hue = fmodf((float)frame / 30.0f + (float)led / RGB_LEDS, 1.0f);
      float r, g, b;

      int hi = (int)(hue * 6.0f) % 6;
      float f = hue * 6.0f - hi;
      float q = 1.0f - f;

      switch (hi) {
        case 0: r = 1; g = f; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = f; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = f; g = 0; b = 1; break;
        default: r = 1; g = 0; b = q; break;
      }

      set_rgb_led(&chain, led, (uint16_t)(r * 4095), (uint16_t)(g * 4095), (uint16_t)(b * 4095));
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  /* Pulse all LEDs white */
  ESP_LOGI(s_tag, "White pulse animation");
  for (int pulse = 0; pulse < 5; pulse++) {
    for (uint16_t brightness = 0; brightness <= 4095; brightness += 128) {
      for (int led = 0; led < RGB_LEDS; led++) {
        set_rgb_led(&chain, led, brightness, brightness, brightness);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (uint16_t brightness = 4095; brightness > 0; brightness -= 128) {
      for (int led = 0; led < RGB_LEDS; led++) {
        set_rgb_led(&chain, led, brightness, brightness, brightness);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  /* All off */
  for (int led = 0; led < RGB_LEDS; led++) {
    set_rgb_led(&chain, led, 0, 0, 0);
  }

  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    star_sensor_pca9685_deinit(&chain.handles[i]);
  }
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { pca9685_triple_chain_example(); }
