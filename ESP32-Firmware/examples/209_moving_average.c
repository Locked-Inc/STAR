/**
 * @file 209_moving_average.c
 * @brief Moving average filter for sensor smoothing
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "MOV_AVG";

#define WINDOW_SIZE (10)

typedef struct {
  float buffer[WINDOW_SIZE];
  int index;
  int count;
  float sum;
} moving_avg_t;

static void priv_moving_avg_init(moving_avg_t *ma)
{
  memset(ma->buffer, 0, sizeof(ma->buffer));
  ma->index = 0;
  ma->count = 0;
  ma->sum = 0;
}

static float priv_moving_avg_update(moving_avg_t *ma, float value)
{
  ma->sum -= ma->buffer[ma->index];
  ma->buffer[ma->index] = value;
  ma->sum += value;
  ma->index = (ma->index + 1) % WINDOW_SIZE;
  if (ma->count < WINDOW_SIZE) ma->count++;
  return ma->sum / ma->count;
}

void moving_average_example(void)
{
  dht22_handle_t dht;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &cfg);

  moving_avg_t temp_filter, hum_filter;
  priv_moving_avg_init(&temp_filter);
  priv_moving_avg_init(&hum_filter);

  ESP_LOGI(s_tag, "Starting moving average filter demo");

  for (int i = 0; i < 100; i++) {
    dht22_data_t data;
    if (star_sensor_dht22_read(&dht, &data) == ESP_OK) {
      float filtered_temp = priv_moving_avg_update(&temp_filter, data.temperature_c);
      float filtered_hum = priv_moving_avg_update(&hum_filter, data.humidity_rh);

      ESP_LOGI(s_tag, "Raw: T=%.1f H=%.1f | Filtered: T=%.2f H=%.2f",
               data.temperature_c, data.humidity_rh, filtered_temp, filtered_hum);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_sensor_dht22_deinit(&dht);
}

void app_main(void) { moving_average_example(); }
