/**
 * @file 175_bh1750_auto_brightness.c
 * @brief BH1750 auto brightness control for displays/LEDs
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_bh1750.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>

static const char *s_tag = "BH1750_BRIGHT";

#define LED_PIN (GPIO_NUM_2)

static void set_led_brightness(uint8_t percent)
{
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (percent * 255) / 100);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void bh1750_brightness_example(void)
{
  /* Setup PWM for LED */
  ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 5000
  };
  ledc_timer_config(&timer);

  ledc_channel_config_t channel = {
    .gpio_num = LED_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0
  };
  ledc_channel_config(&channel);

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Light_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "bh1750", I2C_NUM_0, 0x23, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  bh1750_handle_t sensor;
  bh1750_config_t cfg = {
    .i2c_addr = 0x23,
    .mode = BH1750_MODE_CONTINUOUS,
    .resolution = BH1750_RES_HIGH,
    .mtreg = BH1750_DEFAULT_MTREG
  };
  star_sensor_bh1750_init(&sensor, &manager, "bh1750", &cfg);

  for (int i = 0; i < 100; i++) {
    float lux;
    if (star_sensor_bh1750_read_light(&sensor, &lux) == ESP_OK) {
      /* Map lux to inverse brightness (darker ambient = brighter LED) */
      uint8_t brightness;
      if (lux < 10) brightness = 100;
      else if (lux > 500) brightness = 10;
      else brightness = 100 - (uint8_t)((lux - 10) * 90 / 490);

      set_led_brightness(brightness);
      ESP_LOGI(s_tag, "Lux: %.1f, LED: %d%%", lux, brightness);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  star_sensor_bh1750_deinit(&sensor);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { bh1750_brightness_example(); }
