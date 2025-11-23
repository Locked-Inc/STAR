/**
 * @file 301_watchdog_timer.c
 * @brief Task watchdog timer usage
 */

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "WDT";

void watchdog_timer_example(void)
{
  ESP_LOGI(s_tag, "Watchdog timer example");

  /* Configure watchdog with 5 second timeout */
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 5000,
    .idle_core_mask = 0,
    .trigger_panic = false,
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  ESP_LOGI(s_tag, "Watchdog configured (5s timeout)");

  /* Normal operation - feed watchdog regularly */
  for (int i = 0; i < 20; i++) {
    ESP_LOGI(s_tag, "Working... %d/20", i + 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_task_wdt_reset();
  }

  ESP_LOGI(s_tag, "Simulating stuck task (not feeding WDT)...");
  /* Let watchdog trigger */
  vTaskDelay(pdMS_TO_TICKS(6000));

  ESP_LOGI(s_tag, "Watchdog should have triggered!");
  esp_task_wdt_delete(NULL);
}

void app_main(void) { watchdog_timer_example(); }
