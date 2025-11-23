/**
 * @file 282_rotary_encoder.c
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

static volatile int32_t position = 0;
static volatile bool button_pressed = false;
static QueueHandle_t s_enc_queue;

static void IRAM_ATTR encoder_isr(void *arg) {
  static uint8_t last_state = 0;
  uint8_t state = (gpio_get_level(ENC_A) << 1) | gpio_get_level(ENC_B);

  int8_t delta = 0;
  if ((last_state == 0 && state == 1) || (last_state == 3 && state == 2)) {
    delta = 1;
  }
  if ((last_state == 0 && state == 2) || (last_state == 3 && state == 1)) {
    delta = -1;
  }

  if (delta) {
    position += delta;
    xQueueSendFromISR(s_enc_queue, &delta, NULL);
  }
  last_state = state;
}

static void IRAM_ATTR button_isr(void *arg) {
  button_pressed = true;
}

void rotary_encoder_example(void)
{
  s_enc_queue = xQueueCreate(32, sizeof(int8_t));

  gpio_set_direction(ENC_A, GPIO_MODE_INPUT);
  gpio_set_direction(ENC_B, GPIO_MODE_INPUT);
  gpio_set_direction(ENC_BTN, GPIO_MODE_INPUT);
  gpio_pullup_en(ENC_A);
  gpio_pullup_en(ENC_B);
  gpio_pullup_en(ENC_BTN);

  gpio_set_intr_type(ENC_A, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(ENC_B, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(ENC_BTN, GPIO_INTR_NEGEDGE);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(ENC_A, encoder_isr, NULL);
  gpio_isr_handler_add(ENC_B, encoder_isr, NULL);
  gpio_isr_handler_add(ENC_BTN, button_isr, NULL);

  ESP_LOGI(s_tag, "Rotary encoder started");

  for (int i = 0; i < 200; i++) {
    int8_t delta;
    if (xQueueReceive(s_enc_queue, &delta, pdMS_TO_TICKS(100))) {
      ESP_LOGI(s_tag, "Position: %ld", position);
    }

    if (button_pressed) {
      button_pressed = false;
      position = 0;
      ESP_LOGI(s_tag, "Button pressed - reset to 0");
    }
  }

  gpio_isr_handler_remove(ENC_A);
  gpio_isr_handler_remove(ENC_B);
  gpio_isr_handler_remove(ENC_BTN);
  vQueueDelete(s_enc_queue);
}

void app_main(void) { rotary_encoder_example(); }
