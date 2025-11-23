/**
 * @file 279_relay_module.c
 * @brief Multi-channel relay control
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RELAY";

#define RELAY_1 (GPIO_NUM_25)
#define RELAY_2 (GPIO_NUM_26)
#define RELAY_3 (GPIO_NUM_27)
#define RELAY_4 (GPIO_NUM_14)

static const gpio_num_t relays[] = {RELAY_1, RELAY_2, RELAY_3, RELAY_4};
#define NUM_RELAYS (4)

void relay_module_example(void)
{
  for (int i = 0; i < NUM_RELAYS; i++) {
    gpio_set_direction(relays[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relays[i], 0);
  }

  ESP_LOGI(s_tag, "4-channel relay module started");

  /* Sequential activation */
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      gpio_set_level(relays[i], 1);
      ESP_LOGI(s_tag, "Relay %d ON", i + 1);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    for (int i = NUM_RELAYS - 1; i >= 0; i--) {
      gpio_set_level(relays[i], 0);
      ESP_LOGI(s_tag, "Relay %d OFF", i + 1);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  /* Pattern test */
  uint8_t patterns[] = {0x05, 0x0A, 0x0F, 0x00, 0x09, 0x06};
  for (int p = 0; p < 6; p++) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      gpio_set_level(relays[i], (patterns[p] >> i) & 1);
    }
    ESP_LOGI(s_tag, "Pattern: 0x%02X", patterns[p]);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* All off */
  for (int i = 0; i < NUM_RELAYS; i++) {
    gpio_set_level(relays[i], 0);
  }
}

void app_main(void) { relay_module_example(); }
