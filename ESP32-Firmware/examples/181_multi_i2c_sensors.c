/**
 * @file 181_multi_i2c_sensors.c
 * @brief Multiple I2C sensors on same bus
 *
 * Demonstrates:
 * - Multiple I2C sensors communicating on the same I2C bus
 * - Different I2C addresses for different sensors
 * - Using bus manager to manage sensor access
 * - Reading from MPU6050 (6-axis IMU), BH1750 (light), and QMC5883L (magnetometer)
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_bh1750.h"
#include "star_sensor_qmc5883l.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "MULTI_I2C";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define I2C_FREQ        (400000)

#define MPU6050_ADDR    (0x68)
#define BH1750_ADDR     (0x23)
#define QMC5883L_ADDR   (0x0D)

void multi_i2c_example(void)
{
  esp_err_t ret;

  /* Initialize error handler for retry logic */
  error_handler_t error_handler;
  error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &error_handler);

  /* Initialize bus manager with error interface */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "Multi_I2C", &error_iface, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager: %s", esp_err_to_name(ret));
    error_handler_deinit(&error_handler);
    return;
  }

  /* Create single I2C bus that all sensors will share
   * All sensors are on the same physical I2C bus with different addresses
   */
  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "sensor_bus", I2C_NUM_0, 0x00, I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  if (i2c == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    error_handler_deinit(&error_handler);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize MPU6050 sensor (6-axis IMU) */
  mpu6050_handle_t mpu;
  mpu6050_config_t mpu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };

  ret = star_sensor_mpu6050_init(&mpu, &manager, "sensor_bus", &mpu_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init MPU6050: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    error_handler_deinit(&error_handler);
    return;
  }
  ESP_LOGI(s_tag, "MPU6050 initialized at address 0x%02X", MPU6050_ADDR);

  /* Initialize BH1750 sensor (ambient light) */
  bh1750_handle_t bh;
  bh1750_config_t bh_cfg = {
    .i2c_addr = BH1750_ADDR_LOW,
    .mode = BH1750_MODE_CONTINUOUS,
    .resolution = BH1750_RES_HIGH,
    .mtreg = BH1750_DEFAULT_MTREG
  };

  ret = star_sensor_bh1750_init(&bh, &manager, "sensor_bus", &bh_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init BH1750: %s", esp_err_to_name(ret));
    star_sensor_mpu6050_deinit(&mpu);
    star_bus_manager_deinit(&manager);
    error_handler_deinit(&error_handler);
    return;
  }
  ESP_LOGI(s_tag, "BH1750 initialized at address 0x%02X", BH1750_ADDR);

  /* Initialize QMC5883L sensor (magnetometer) */
  qmc5883l_handle_t mag;
  qmc5883l_config_t mag_cfg = {
    .mode = QMC5883L_MODE_CONTINUOUS_VAL,
    .odr = QMC5883L_ODR_200HZ_VAL,
    .range = QMC5883L_RNG_8G_VAL,
    .osr = QMC5883L_OSR_512_VAL
  };

  ret = star_sensor_qmc5883l_init(&mag, &manager, "sensor_bus", &mag_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init QMC5883L: %s", esp_err_to_name(ret));
    star_sensor_mpu6050_deinit(&mpu);
    star_sensor_bh1750_deinit(&bh);
    star_bus_manager_deinit(&manager);
    error_handler_deinit(&error_handler);
    return;
  }
  ESP_LOGI(s_tag, "QMC5883L initialized at address 0x%02X", QMC5883L_ADDR);

  ESP_LOGI(s_tag, "All sensors initialized successfully");

  /* Main reading loop */
  for (int i = 0; i < 100; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temp;
    float lux;
    qmc5883l_mag_data_t mag_data;

    /* Read from all sensors */
    ret = star_sensor_mpu6050_read_all(&mpu, &accel, &gyro, &temp);
    if (ret == ESP_OK) {
      float accel_magnitude = sqrtf(
        accel.x_g * accel.x_g +
        accel.y_g * accel.y_g +
        accel.z_g * accel.z_g);

      ESP_LOGI(s_tag, "MPU6050 - Accel: X=%.2f Y=%.2f Z=%.2f (mag=%.2f g) | Temp: %.1f C",
               accel.x_g, accel.y_g, accel.z_g, accel_magnitude, temp);
    } else {
      ESP_LOGW(s_tag, "Failed to read MPU6050: %s", esp_err_to_name(ret));
    }

    ret = star_sensor_bh1750_read_light(&bh, &lux);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "BH1750 - Light: %.1f lux", lux);
    } else {
      ESP_LOGW(s_tag, "Failed to read BH1750: %s", esp_err_to_name(ret));
    }

    ret = star_sensor_qmc5883l_read_mag(&mag, &mag_data);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "QMC5883L - Mag: X=%.2f Y=%.2f Z=%.2f gauss",
               mag_data.x_gauss, mag_data.y_gauss, mag_data.z_gauss);
    } else {
      ESP_LOGW(s_tag, "Failed to read QMC5883L: %s", esp_err_to_name(ret));
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  /* Cleanup */
  star_sensor_mpu6050_deinit(&mpu);
  star_sensor_bh1750_deinit(&bh);
  star_sensor_qmc5883l_deinit(&mag);
  star_bus_manager_deinit(&manager);
  error_handler_deinit(&error_handler);

  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { multi_i2c_example(); }
