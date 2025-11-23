/**
 * @file 236_relay_array.c
 * @brief 8-channel relay control
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RELAY";

static const gpio_num_t relay_pins[] = {
  GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32,
  GPIO_NUM_33, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13
};
#define NUM_RELAYS (8)

static void priv_set_relay(int ch, bool on) {
  if (ch >= 0 && ch < NUM_RELAYS) {
    gpio_set_level(relay_pins[ch], on ? 0 : 1);  /* Active low */
    ESP_LOGI(s_tag, "Relay %d: %s", ch, on ? "ON" : "OFF");
  }
}

void relay_array_example(void)
{
  for (int i = 0; i < NUM_RELAYS; i++) {
    gpio_set_direction(relay_pins[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relay_pins[i], 1);  /* Off (active low) */
  }

  ESP_LOGI(s_tag, "8-channel relay array initialized");

  /* Sequential activation */
  for (int i = 0; i < NUM_RELAYS; i++) {
    priv_set_relay(i, true);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  for (int i = NUM_RELAYS - 1; i >= 0; i--) {
    priv_set_relay(i, false);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  /* Binary counting pattern */
  for (int count = 0; count < 256; count++) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      priv_set_relay(i, (count >> i) & 1);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  /* All off */
  for (int i = 0; i < NUM_RELAYS; i++) {
    priv_set_relay(i, false);
  }
}

void app_main(void) { relay_array_example(); }
