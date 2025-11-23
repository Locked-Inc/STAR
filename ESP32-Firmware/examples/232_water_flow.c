/**
 * @file 232_water_flow.c
 * @brief Water flow sensor with pulse counting
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/pcnt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "WATER_FLOW";

#define FLOW_SENSOR_PIN (GPIO_NUM_34)
#define PCNT_UNIT       (PCNT_UNIT_0)
#define PULSES_PER_LITER (450.0f) 

static volatile int32_t total_pulses = 0;

void water_flow_example(void)
{
  pcnt_config_t pcnt_cfg = {
    .pulse_gpio_num = FLOW_SENSOR_PIN,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .pos_mode = PCNT_COUNT_INC,
    .neg_mode = PCNT_COUNT_DIS,
    .counter_h_lim = 32767,
    .unit = PCNT_UNIT,
    .channel = PCNT_CHANNEL_0,
  };

  pcnt_unit_config(&pcnt_cfg);
  pcnt_set_filter_value(PCNT_UNIT, 100);
  pcnt_filter_enable(PCNT_UNIT);
  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);

  ESP_LOGI(s_tag, "Water flow sensor started");

  int16_t last_count = 0;
  float total_liters = 0;

  for (int i = 0; i < 300; i++) {
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT, &count);

    int16_t pulses = count - last_count;
    if (pulses < 0) pulses += 32767;
    last_count = count;

    float liters = pulses / PULSES_PER_LITER;
    total_liters += liters;
    float flow_rate = liters * 60;  /* L/min */

    if (flow_rate > 0.1f) {
      ESP_LOGI(s_tag, "Flow: %.2f L/min, Total: %.2f L", flow_rate, total_liters);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  ESP_LOGI(s_tag, "Total water used: %.2f liters", total_liters);
}

void app_main(void) { water_flow_example(); }
