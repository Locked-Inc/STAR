/**
 * @file 211_ring_buffer_logging.c
 * @brief Ring buffer for continuous sensor logging
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <string.h>

static const char *s_tag = "RING_LOG";

typedef struct {
  uint32_t timestamp;
  float accel_x, accel_y, accel_z;
  float gyro_x, gyro_y, gyro_z;
} imu_sample_t;

#define RING_BUF_SIZE (sizeof(imu_sample_t) * 100)

static RingbufHandle_t ring_buf;

static void priv_producer_task(void *arg)
{
  mpu6050_handle_t *imu = (mpu6050_handle_t*)arg;

  while (1) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    star_sensor_mpu6050_read_accel(imu, &accel);
    star_sensor_mpu6050_read_gyro(imu, &gyro);

    imu_sample_t sample = {
      .timestamp = xTaskGetTickCount(),
      .accel_x = accel.x_g, .accel_y = accel.y_g, .accel_z = accel.z_g,
      .gyro_x = gyro.x_dps, .gyro_y = gyro.y_dps, .gyro_z = gyro.z_dps
    };

    if (xRingbufferSend(ring_buf, &sample, sizeof(sample), 0) != pdTRUE) {
      ESP_LOGI(s_tag, "Ring buffer full - oldest data overwritten");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void priv_consumer_task(void *arg)
{
  while (1) {
    size_t item_size;
    imu_sample_t *sample = (imu_sample_t*)xRingbufferReceive(ring_buf, &item_size, pdMS_TO_TICKS(100));

    if (sample) {
      ESP_LOGI(s_tag, "[%lu] A:%.2f,%.2f,%.2f G:%.1f,%.1f,%.1f",
               sample->timestamp,
               sample->accel_x, sample->accel_y, sample->accel_z,
               sample->gyro_x, sample->gyro_y, sample->gyro_z);

      vRingbufferReturnItem(ring_buf, sample);
    }

    vTaskDelay(pdMS_TO_TICKS(50));  /* Process slower than produce */
  }
}

void ring_buffer_example(void)
{
  ring_buf = xRingbufferCreate(RING_BUF_SIZE, RINGBUF_TYPE_NOSPLIT);

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Ring_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  static mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  xTaskCreate(priv_producer_task, "producer", 4096, &imu, 5, NULL);
  xTaskCreate(priv_consumer_task, "consumer", 4096, NULL, 4, NULL);

  ESP_LOGI(s_tag, "Ring buffer logging started");

  vTaskDelay(pdMS_TO_TICKS(30000));

  vRingbufferDelete(ring_buf);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { ring_buffer_example(); }
