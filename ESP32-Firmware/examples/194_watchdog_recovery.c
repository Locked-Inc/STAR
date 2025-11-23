/**
 * @file 194_watchdog_recovery.c
 * @brief Watchdog timer for system recovery
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "WATCHDOG";

#define WDT_TIMEOUT_S (10)

void watchdog_example(void)
{
  /* Configure watchdog */
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  ESP_LOGI(s_tag, "Watchdog configured: %d second timeout", WDT_TIMEOUT_S);

  /* Initialize sensor */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IMU_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  int consecutive_failures = 0;

  for (int i = 0; i < 200; i++) {
    /* Feed watchdog */
    esp_task_wdt_reset();

    mpu6050_accel_t accel;
    esp_err_t ret = star_sensor_mpu6050_read_accel(&imu, &accel);

    if (ret == ESP_OK) {
      consecutive_failures = 0;
      ESP_LOGI(s_tag, "Accel: %.2f,%.2f,%.2f", accel.x_g, accel.y_g, accel.z_g);
    } else {
      consecutive_failures++;
      ESP_LOGW(s_tag, "Read failed (%d consecutive)", consecutive_failures);

      if (consecutive_failures >= 5) {
        ESP_LOGE(s_tag, "Too many failures, attempting recovery...");
        star_sensor_mpu6050_deinit(&imu);
        vTaskDelay(pdMS_TO_TICKS(100));
        star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);
        consecutive_failures = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  esp_task_wdt_delete(NULL);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { watchdog_example(); }
