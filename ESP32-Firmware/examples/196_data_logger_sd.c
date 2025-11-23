/**
 * @file 196_data_logger_sd.c
 * @brief SD card data logger
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <string.h>

static const char *s_tag = "SD_LOGGER";

#define MOUNT_POINT "/sdcard"

void sd_logger_example(void)
{
  /* Mount SD card */
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t* card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to mount SD card");
    return;
  }

  ESP_LOGI(s_tag, "SD card mounted: %lluMB", card->csd.capacity / (1024 * 1024));

  /* Initialize sensors */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Logger_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  dht22_handle_t dht;
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

  /* Create log file */
  char filepath[64];
  snprintf(filepath, sizeof(filepath), "%s/log_%lu.csv", MOUNT_POINT, xTaskGetTickCount());

  FILE* f = fopen(filepath, "w");
  if (!f) {
    ESP_LOGE(s_tag, "Failed to create log file");
    return;
  }

  fprintf(f, "timestamp,ax,ay,az,gx,gy,gz,temp,humidity\n");
  ESP_LOGI(s_tag, "Logging to: %s", filepath);

  for (int i = 0; i < 1000; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    dht22_data_t dht_data = {0};

    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_mpu6050_read_gyro(&imu, &gyro);
    if (i % 20 == 0) {  /* DHT22 every 2 seconds */
      star_sensor_dht22_read(&dht, &dht_data);
    }

    fprintf(f, "%lu,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
            xTaskGetTickCount(),
            accel.x_g, accel.y_g, accel.z_g,
            gyro.x_dps, gyro.y_dps, gyro.z_dps,
            dht_data.temperature_c, dht_data.humidity_rh);

    if (i % 100 == 0) {
      fflush(f);
      ESP_LOGI(s_tag, "Logged %d samples", i);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  fclose(f);
  ESP_LOGI(s_tag, "Logging complete");

  star_sensor_mpu6050_deinit(&imu);
  star_sensor_dht22_deinit(&dht);
  star_bus_manager_deinit(&manager);
  esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
}

void app_main(void) { sd_logger_example(); }
