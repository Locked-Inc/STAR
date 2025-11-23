/**
 * @file 275_loadcell_hx711.c
 * @brief HX711 load cell amplifier
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "LOADCELL";

#define HX711_DOUT (GPIO_NUM_32)
#define HX711_SCK  (GPIO_NUM_33)

static int32_t priv_hx711_read(void) {
  while (gpio_get_level(HX711_DOUT));  /* Wait for ready */

  int32_t value = 0;
  for (int i = 0; i < 24; i++) {
    gpio_set_level(HX711_SCK, 1);
    esp_rom_delay_us(1);
    value = (value << 1) | gpio_get_level(HX711_DOUT);
    gpio_set_level(HX711_SCK, 0);
    esp_rom_delay_us(1);
  }

  /* 25th pulse for gain 128 */
  gpio_set_level(HX711_SCK, 1);
  esp_rom_delay_us(1);
  gpio_set_level(HX711_SCK, 0);

  /* Sign extend */
  if (value & 0x800000) value |= 0xFF000000;
  return value;
}

void loadcell_hx711_example(void)
{
  gpio_set_direction(HX711_DOUT, GPIO_MODE_INPUT);
  gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT);
  gpio_set_level(HX711_SCK, 0);

  ESP_LOGI(s_tag, "HX711 load cell started");

  /* Tare */
  int32_t tare = 0;
  for (int i = 0; i < 10; i++) {
    tare += priv_hx711_read();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  tare /= 10;
  ESP_LOGI(s_tag, "Tare offset: %ld", tare);

  float scale = 420.0f;  /* Calibration factor */

  for (int i = 0; i < 100; i++) {
    int32_t raw = priv_hx711_read();
    float weight = (raw - tare) / scale;

    ESP_LOGI(s_tag, "Raw:%ld Weight:%.2f g", raw, weight);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void app_main(void) { loadcell_hx711_example(); }
