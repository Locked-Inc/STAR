/**
 * @file 305_timer_callback.c
 * @brief ESP timer and FreeRTOS timer callbacks
 */

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static const char *s_tag = "TIMER";

static void priv_esp_timer_callback(void *arg) {
  static int count = 0;
  count++;
  ESP_LOGI(s_tag, "ESP timer fired: %d", count);
}

static void priv_freertos_timer_callback(TimerHandle_t timer) {
  static int count = 0;
  count++;
  ESP_LOGI(s_tag, "FreeRTOS timer fired: %d", count);
}

void timer_callback_example(void)
{
  /* ESP high-resolution timer */
  ESP_LOGI(s_tag, "=== ESP Timer (high resolution) ===");

  esp_timer_create_args_t esp_timer_args = {
    .callback = priv_esp_timer_callback,
    .name = "esp_periodic"
  };

  esp_timer_handle_t esp_timer;
  esp_timer_create(&esp_timer_args, &esp_timer);
  esp_timer_start_periodic(esp_timer, 200000);  /* 200ms */

  vTaskDelay(pdMS_TO_TICKS(2000));
  esp_timer_stop(esp_timer);
  esp_timer_delete(esp_timer);

  /* FreeRTOS software timer */
  ESP_LOGI(s_tag, "=== FreeRTOS Timer (software) ===");

  TimerHandle_t freertos_timer = xTimerCreate(
    "freertos_timer", pdMS_TO_TICKS(300), pdTRUE, NULL, priv_freertos_timer_callback);

  xTimerStart(freertos_timer, 0);
  vTaskDelay(pdMS_TO_TICKS(2000));
  xTimerStop(freertos_timer, 0);
  xTimerDelete(freertos_timer, 0);

  ESP_LOGI(s_tag, "Timer demo complete");
}

void app_main(void) { timer_callback_example(); }
