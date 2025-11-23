/**
 * @file 291_vibration_sw420.c
 * @brief SW-420 vibration sensor
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "VIBRATION";

#define VIBR_PIN (GPIO_NUM_27)

static volatile uint32_t vibration_count = 0;

static void IRAM_ATTR vibr_isr(void *arg) {
  vibration_count++;
}

void vibration_sw420_example(void)
{
  gpio_set_direction(VIBR_PIN, GPIO_MODE_INPUT);
  gpio_set_intr_type(VIBR_PIN, GPIO_INTR_POSEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(VIBR_PIN, vibr_isr, NULL);

  ESP_LOGI(s_tag, "SW-420 vibration sensor started");

  for (int i = 0; i < 100; i++) {
    uint32_t count = vibration_count;
    vibration_count = 0;

    if (count > 50) {
      ESP_LOGI(s_tag, "STRONG vibration: %lu events/sec", count);
    } else if (count > 10) {
      ESP_LOGI(s_tag, "Moderate vibration: %lu events/sec", count);
    } else if (count > 0) {
      ESP_LOGI(s_tag, "Light vibration: %lu events/sec", count);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  gpio_isr_handler_remove(VIBR_PIN);
}

void app_main(void) { vibration_sw420_example(); }
