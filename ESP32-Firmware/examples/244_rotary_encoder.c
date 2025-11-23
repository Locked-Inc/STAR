/**
 * @file 244_rotary_encoder.c
 * @brief Rotary encoder with button
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static const char *s_tag = "ENCODER";

#define ENC_A   (GPIO_NUM_32)
#define ENC_B   (GPIO_NUM_33)
#define ENC_BTN (GPIO_NUM_25)

static volatile int32_t encoder_pos = 0;
static volatile bool last_a = false;

static void IRAM_ATTR encoder_isr(void *arg)
{
  bool a = gpio_get_level(ENC_A);
  bool b = gpio_get_level(ENC_B);

  if (a != last_a) {
    if (a == b) encoder_pos++;
    else encoder_pos--;
    last_a = a;
  }
}

void rotary_encoder_example(void)
{
  gpio_set_direction(ENC_A, GPIO_MODE_INPUT);
  gpio_set_direction(ENC_B, GPIO_MODE_INPUT);
  gpio_set_direction(ENC_BTN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(ENC_A, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(ENC_B, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(ENC_BTN, GPIO_PULLUP_ONLY);

  gpio_set_intr_type(ENC_A, GPIO_INTR_ANYEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(ENC_A, encoder_isr, NULL);

  ESP_LOGI(s_tag, "Rotary encoder started");

  int32_t last_pos = 0;
  int value = 50;  /* 0-100 range */

  for (int i = 0; i < 500; i++) {
    int32_t pos = encoder_pos;
    bool btn = !gpio_get_level(ENC_BTN);

    if (pos != last_pos) {
      int delta = pos - last_pos;
      value += delta;
      if (value < 0) value = 0;
      if (value > 100) value = 100;
      last_pos = pos;
      ESP_LOGI(s_tag, "Position: %ld, Value: %d%%", pos, value);
    }

    if (btn) {
      ESP_LOGI(s_tag, "Button pressed! Value reset to 50");
      value = 50;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }

  gpio_isr_handler_remove(ENC_A);
}

void app_main(void) { rotary_encoder_example(); }
