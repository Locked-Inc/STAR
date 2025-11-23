/**
 * @file 184_freertos_sensor_tasks.c
 * @brief Multi-threaded sensor reading with FreeRTOS
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char *s_tag = "RTOS_SENSORS";

static star_bus_manager_t s_manager;
static SemaphoreHandle_t s_bus_mutex;

typedef struct {
  float accel_x, accel_y, accel_z;
  float temperature, humidity;
} sensor_data_t;

static sensor_data_t s_shared_data;
static SemaphoreHandle_t s_data_mutex;

static void imu_task(void *arg)
{
  mpu6050_handle_t* imu = (mpu6050_handle_t*)arg;

  while (1) {
    mpu6050_accel_t accel;
    xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
    esp_err_t ret = star_sensor_mpu6050_read_accel(imu, &accel);
    xSemaphoreGive(s_bus_mutex);

    if (ret == ESP_OK) {
      xSemaphoreTake(s_data_mutex, portMAX_DELAY);
      s_shared_data.accel_x = accel.x_g;
      s_shared_data.accel_y = accel.y_g;
      s_shared_data.accel_z = accel.z_g;
      xSemaphoreGive(s_data_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void dht_task(void *arg)
{
  dht22_handle_t* dht = (dht22_handle_t*)arg;

  while (1) {
    dht22_data_t data;
    xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
    esp_err_t ret = star_sensor_dht22_read(dht, &data);
    xSemaphoreGive(s_bus_mutex);

    if (ret == ESP_OK) {
      xSemaphoreTake(s_data_mutex, portMAX_DELAY);
      s_shared_data.temperature = data.temperature_c;
      s_shared_data.humidity = data.humidity_rh;
      xSemaphoreGive(s_data_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

static void logger_task(void *arg)
{
  while (1) {
    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    ESP_LOGI(s_tag, "Accel: %.2f,%.2f,%.2f | Temp: %.1f C, Hum: %.1f%%",
             s_shared_data.accel_x, s_shared_data.accel_y, s_shared_data.accel_z,
             s_shared_data.temperature, s_shared_data.humidity);
    xSemaphoreGive(s_data_mutex);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void freertos_sensors_example(void)
{
  s_bus_mutex = xSemaphoreCreateMutex();
  s_data_mutex = xSemaphoreCreateMutex();

  star_bus_manager_init(&s_manager, "RTOS_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&s_manager, i2c);
  star_bus_config_init(i2c, &s_manager);

  static mpu6050_handle_t s_imu;
  static dht22_handle_t s_dht;

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

  star_sensor_mpu6050_init(&s_imu, &s_manager, "mpu6050", &imu_cfg);
  star_sensor_dht22_init(&s_dht, &dht_cfg);

  xTaskCreate(imu_task, "imu", 4096, &s_imu, 5, NULL);
  xTaskCreate(dht_task, "dht", 4096, &s_dht, 4, NULL);
  xTaskCreate(logger_task, "logger", 4096, NULL, 3, NULL);

  ESP_LOGI(s_tag, "Multi-threaded sensor tasks running");
}

void app_main(void) { freertos_sensors_example(); }
