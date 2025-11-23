/**
 * @file 210_event_driven.c
 * @brief Event-driven sensor architecture
 */

#include "star_sensor_mpu6050.h"
#include "star_sensor_dht22.h"
#include "star_bus_config.h"

#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "EVENT_DRIVEN";

ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

enum {
  SENSOR_EVENT_MOTION_DETECTED,
  SENSOR_EVENT_TEMP_HIGH,
  SENSOR_EVENT_TEMP_LOW,
  SENSOR_EVENT_HUMIDITY_HIGH,
};

typedef struct {
  float accel_magnitude;
  float temperature;
  float humidity;
} sensor_event_data_t;

static void priv_sensor_event_handler(void *handler_arg, esp_event_base_t base,
                                 int32_t id, void *event_data)
{
  sensor_event_data_t *data = (sensor_event_data_t*)event_data;

  switch (id) {
    case SENSOR_EVENT_MOTION_DETECTED:
      ESP_LOGI(s_tag, "EVENT: Motion detected! Magnitude: %.2f g", data->accel_magnitude);
      break;
    case SENSOR_EVENT_TEMP_HIGH:
      ESP_LOGI(s_tag, "EVENT: High temperature! %.1f C", data->temperature);
      break;
    case SENSOR_EVENT_TEMP_LOW:
      ESP_LOGI(s_tag, "EVENT: Low temperature! %.1f C", data->temperature);
      break;
    case SENSOR_EVENT_HUMIDITY_HIGH:
      ESP_LOGI(s_tag, "EVENT: High humidity! %.1f%%", data->humidity);
      break;
  }
}

static void priv_imu_monitor_task(void *arg)
{
  mpu6050_handle_t *imu = (mpu6050_handle_t*)arg;
  float prev_magnitude = 1.0f;

  while (1) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(imu, &accel);

    float magnitude = sqrtf(accel.x_g * accel.x_g +
                           accel.y_g * accel.y_g +
                           accel.z_g * accel.z_g);

    if (fabsf(magnitude - prev_magnitude) > 0.5f) {
      sensor_event_data_t event_data = { .accel_magnitude = magnitude };
      esp_event_post(SENSOR_EVENTS, SENSOR_EVENT_MOTION_DETECTED, &event_data,
                    sizeof(event_data), 0);
    }

    prev_magnitude = magnitude;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void priv_dht_monitor_task(void *arg)
{
  dht22_handle_t *dht = (dht22_handle_t*)arg;

  while (1) {
    dht22_data_t data;
    if (star_sensor_dht22_read(dht, &data) == ESP_OK) {
      sensor_event_data_t event_data = {
        .temperature = data.temperature_c,
        .humidity = data.humidity_rh
      };

      if (data.temperature_c > 30.0f) {
        esp_event_post(SENSOR_EVENTS, SENSOR_EVENT_TEMP_HIGH, &event_data,
                      sizeof(event_data), 0);
      } else if (data.temperature_c < 15.0f) {
        esp_event_post(SENSOR_EVENTS, SENSOR_EVENT_TEMP_LOW, &event_data,
                      sizeof(event_data), 0);
      }

      if (data.humidity_rh > 80.0f) {
        esp_event_post(SENSOR_EVENTS, SENSOR_EVENT_HUMIDITY_HIGH, &event_data,
                      sizeof(event_data), 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void event_driven_example(void)
{
  esp_event_loop_create_default();
  esp_event_handler_register(SENSOR_EVENTS, ESP_EVENT_ANY_ID, priv_sensor_event_handler, NULL);

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Event_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);

  static mpu6050_handle_t imu;
  static dht22_handle_t dht;

  mpu6050_config_t imu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  dht22_config_t dht_cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };

  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_dht22_init(&dht, &dht_cfg);

  xTaskCreate(priv_imu_monitor_task, "imu_mon", 4096, &imu, 5, NULL);
  xTaskCreate(priv_dht_monitor_task, "dht_mon", 4096, &dht, 4, NULL);

  ESP_LOGI(s_tag, "Event-driven sensor system running");

  /* Main task just monitors */
  for (int i = 0; i < 100; i++) {
    ESP_LOGI(s_tag, "System running... %d", i);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void app_main(void) { event_driven_example(); }
