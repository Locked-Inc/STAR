/**
 * @file 288_pir_motion.c
 * @brief PIR motion detector
 */

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PIR";

#define PIR_PIN (GPIO_NUM_27)

static volatile uint32_t motion_count = 0;
static volatile int64_t last_motion = 0;

static void IRAM_ATTR pir_isr(void *arg) {
  motion_count++;
  last_motion = esp_timer_get_time();
}

void pir_motion_example(void)
{
  gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
  gpio_set_intr_type(PIR_PIN, GPIO_INTR_POSEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(PIR_PIN, pir_isr, NULL);

  ESP_LOGI(s_tag, "PIR motion sensor started");
  ESP_LOGI(s_tag, "Warming up (30s)...");
  vTaskDelay(pdMS_TO_TICKS(30000));

  ESP_LOGI(s_tag, "Ready - monitoring for motion");

  for (int i = 0; i < 300; i++) {
    static uint32_t prev_count = 0;
    int64_t now = esp_timer_get_time();

    if (motion_count != prev_count) {
      int64_t elapsed = (now - last_motion) / 1000;
      ESP_LOGI(s_tag, "MOTION DETECTED! Count:%lu (elapsed:%lldms)",
               motion_count, elapsed);
      prev_count = motion_count;
    }

    if ((now - last_motion) / 1000000 > 5 && last_motion > 0) {
      ESP_LOGI(s_tag, "Area clear - no motion for 5s");
      last_motion = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  gpio_isr_handler_remove(PIR_PIN);
}

void app_main(void) { pir_motion_example(); }
