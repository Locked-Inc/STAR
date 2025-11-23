/**
 * @file 299_deep_sleep.c
 * @brief ESP32 deep sleep power management
 */

#include <esp_log.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SLEEP";

#define WAKEUP_PIN (GPIO_NUM_33)

RTC_DATA_ATTR static int boot_count = 0;

void deep_sleep_example(void)
{
  boot_count++;

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const char *reason;

  switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT0: reason = "External GPIO"; break;
    case ESP_SLEEP_WAKEUP_TIMER: reason = "Timer"; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: reason = "Touchpad"; break;
    case ESP_SLEEP_WAKEUP_ULP: reason = "ULP"; break;
    default: reason = "Power on"; break;
  }

  ESP_LOGI(s_tag, "Boot #%d, wakeup: %s", boot_count, reason);

  /* Do some work */
  ESP_LOGI(s_tag, "Doing work for 5 seconds...");
  vTaskDelay(pdMS_TO_TICKS(5000));

  /* Configure wakeup sources */
  ESP_LOGI(s_tag, "Configuring wakeup sources:");

  /* Timer wakeup (30 seconds) */
  esp_sleep_enable_timer_wakeup(30 * 1000000ULL);
  ESP_LOGI(s_tag, "  - Timer: 30s");

  /* GPIO wakeup */
  rtc_gpio_pullup_en(WAKEUP_PIN);
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
  ESP_LOGI(s_tag, "  - GPIO%d: LOW level", WAKEUP_PIN);

  ESP_LOGI(s_tag, "Entering deep sleep...");
  esp_deep_sleep_start();
}

void app_main(void) { deep_sleep_example(); }
