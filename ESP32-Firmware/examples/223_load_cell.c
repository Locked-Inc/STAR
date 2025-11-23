/**
 * @file 223_load_cell.c
 * @brief HX711 load cell weight measurement
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "LOAD_CELL";

#define HX711_DOUT (GPIO_NUM_32)
#define HX711_SCK  (GPIO_NUM_33)

static float s_calibration_factor = 420.0f;
static int32_t s_tare_offset = 0;

static int32_t priv_hx711_read_raw(void)
{
  /* Wait for data ready */
  while (gpio_get_level(HX711_DOUT)) {
    vTaskDelay(1);
  }

  int32_t value = 0;
  portDISABLE_INTERRUPTS();

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

  portENABLE_INTERRUPTS();

  /* Sign extend 24-bit to 32-bit */
  if (value & 0x800000) {
    value |= 0xFF000000;
  }

  return value;
}

static int32_t priv_hx711_read_average(int samples)
{
  int64_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += priv_hx711_read_raw();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return sum / samples;
}

static void priv_tare(void)
{
  ESP_LOGI(s_tag, "Taring... remove all weight");
  vTaskDelay(pdMS_TO_TICKS(2000));
  s_tare_offset = priv_hx711_read_average(20);
  ESP_LOGI(s_tag, "Tare offset: %ld", s_tare_offset);
}

static float priv_get_weight(void)
{
  int32_t raw = priv_hx711_read_average(5);
  return (raw - s_tare_offset) / s_calibration_factor;
}

void load_cell_example(void)
{
  gpio_set_direction(HX711_DOUT, GPIO_MODE_INPUT);
  gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT);
  gpio_set_level(HX711_SCK, 0);

  ESP_LOGI(s_tag, "HX711 load cell initialized");

  priv_tare();

  for (int i = 0; i < 100; i++) {
    float weight = priv_get_weight();
    ESP_LOGI(s_tag, "Weight: %.2f g", weight);

    if (weight > 1000) {
      ESP_LOGI(s_tag, "Heavy load detected!");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void app_main(void) { load_cell_example(); }
