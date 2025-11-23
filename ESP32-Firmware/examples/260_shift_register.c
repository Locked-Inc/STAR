/**
 * @file 260_shift_register.c
 * @brief 74HC595 shift register output expansion
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SHIFT_REG";

#define SR_DATA  (GPIO_NUM_25)
#define SR_CLOCK (GPIO_NUM_26)
#define SR_LATCH (GPIO_NUM_27)

static void priv_shift_out(uint8_t data) {
  gpio_set_level(SR_LATCH, 0);
  for (int i = 7; i >= 0; i--) {
    gpio_set_level(SR_CLOCK, 0);
    gpio_set_level(SR_DATA, (data >> i) & 1);
    gpio_set_level(SR_CLOCK, 1);
  }
  gpio_set_level(SR_LATCH, 1);
}

void shift_register_example(void)
{
  gpio_set_direction(SR_DATA, GPIO_MODE_OUTPUT);
  gpio_set_direction(SR_CLOCK, GPIO_MODE_OUTPUT);
  gpio_set_direction(SR_LATCH, GPIO_MODE_OUTPUT);

  ESP_LOGI(s_tag, "74HC595 shift register started");

  /* Running LED */
  for (int i = 0; i < 32; i++) {
    priv_shift_out(1 << (i % 8));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* Binary count */
  for (int i = 0; i < 256; i++) {
    priv_shift_out(i);
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  priv_shift_out(0);
}

void app_main(void) { shift_register_example(); }
