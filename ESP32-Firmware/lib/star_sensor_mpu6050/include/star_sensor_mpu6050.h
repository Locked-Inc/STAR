/* lib/star_sensor_mpu6050/include/star_sensor_mpu6050.h */

#ifndef STAR_SENSOR_MPU6050_H
#define STAR_SENSOR_MPU6050_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_error_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_mpu6050.h
 * @brief MPU6050 6-axis IMU driver (3-axis accelerometer + 3-axis gyroscope)
 *
 * This driver provides an interface to the InvenSense MPU6050 6-DoF IMU sensor.
 * It supports reading accelerometer, gyroscope, and temperature data with
 * configurable ranges and digital low-pass filter settings.
 *
 * Key Features:
 * - 3-axis accelerometer (±2g, ±4g, ±8g, ±16g ranges)
 * - 3-axis gyroscope (±250, ±500, ±1000, ±2000 °/s ranges)
 * - On-chip temperature sensor
 * - Configurable digital low-pass filter (DLPF)
 * - Hardware FIFO buffer support
 * - Sleep mode for power saving
 * - Thread-safe with mutex protection
 *
 * Example Usage:
 * @code
 * // === Basic Setup ===
 *
 * #include "star_sensor_mpu6050.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // 1. Setup bus manager (see star_bus_manager.h for full setup)
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Create I2C bus for MPU6050
 * star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
 *     "imu_i2c",
 *     I2C_NUM_0,
 *     MPU6050_I2C_ADDR_LOW,   // 0x68 (AD0 pin low)
 *     GPIO_NUM_21,            // SDA
 *     GPIO_NUM_22,            // SCL
 *     400000                  // 400kHz fast mode
 * );
 * star_bus_manager_add_bus(&bus_manager, i2c_bus);
 *
 * // 3. Configure and initialize MPU6050
 * mpu6050_handle_t imu;
 * mpu6050_config_t config = {
 *     .i2c_addr = MPU6050_I2C_ADDR_LOW,
 *     .accel_range = MPU6050_ACCEL_RANGE_4G,   // ±4g
 *     .gyro_range = MPU6050_GYRO_RANGE_500,    // ±500 °/s
 *     .dlpf = MPU6050_DLPF_44HZ,               // 44Hz low-pass filter
 *     .sample_rate_div = 9,                    // 100Hz sample rate
 *     .enable_fifo = false
 * };
 *
 * esp_err_t ret = star_sensor_mpu6050_init(
 *     &imu,
 *     &bus_manager,
 *     "imu_i2c",
 *     NULL,           // Use default error handler
 *     &config
 * );
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init MPU6050: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Reading Sensor Data ===
 *
 * // Read all data at once
 * mpu6050_accel_t accel;
 * mpu6050_gyro_t gyro;
 * float temperature;
 *
 * ret = star_sensor_mpu6050_read_all(&imu, &accel, &gyro, &temperature);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Accel: X=%.2fg Y=%.2fg Z=%.2fg",
 *              accel.x_g, accel.y_g, accel.z_g);
 *     ESP_LOGI(TAG, "Gyro: X=%.1f°/s Y=%.1f°/s Z=%.1f°/s",
 *              gyro.x_dps, gyro.y_dps, gyro.z_dps);
 *     ESP_LOGI(TAG, "Temperature: %.1f°C", temperature);
 * }
 *
 * // Or read individual components
 * star_sensor_mpu6050_read_accel(&imu, &accel);
 * star_sensor_mpu6050_read_gyro(&imu, &gyro);
 * star_sensor_mpu6050_read_temperature(&imu, &temperature);
 *
 *
 * // === Reading Raw Data ===
 *
 * // For custom processing, read raw 16-bit values
 * mpu6050_raw_accel_t raw_accel;
 * mpu6050_raw_gyro_t raw_gyro;
 *
 * star_sensor_mpu6050_read_accel_raw(&imu, &raw_accel);
 * star_sensor_mpu6050_read_gyro_raw(&imu, &raw_gyro);
 *
 * // Convert manually if needed
 * float ax = raw_accel.x / 8192.0f;  // For ±4g range
 *
 *
 * // === Using FIFO Buffer ===
 *
 * // Enable FIFO for burst reading
 * star_sensor_mpu6050_fifo_enable(&imu, true);
 *
 * // Wait for data to accumulate...
 * vTaskDelay(pdMS_TO_TICKS(100));
 *
 * // Check FIFO count
 * uint16_t fifo_count;
 * star_sensor_mpu6050_fifo_get_count(&imu, &fifo_count);
 * ESP_LOGI(TAG, "FIFO has %d bytes", fifo_count);
 *
 * // Read FIFO data
 * uint8_t fifo_data[512];
 * star_sensor_mpu6050_fifo_read(&imu, fifo_data, fifo_count);
 *
 * // Reset FIFO when done
 * star_sensor_mpu6050_fifo_reset(&imu);
 *
 *
 * // === Power Management ===
 *
 * // Put device to sleep to save power
 * star_sensor_mpu6050_set_sleep(&imu, true);
 *
 * // Wake up when needed
 * star_sensor_mpu6050_set_sleep(&imu, false);
 *
 * // Reset device to defaults
 * star_sensor_mpu6050_reset(&imu);
 *
 *
 * // === Cleanup ===
 *
 * star_sensor_mpu6050_deinit(&imu);
 * star_bus_manager_remove_bus(&bus_manager, "imu_i2c");
 * @endcode
 */

#define MPU6050_I2C_ADDR_LOW (0x68)
#define MPU6050_I2C_ADDR_HIGH (0x69)
#define MPU6050_WHO_AM_I_VAL (0x68)

/* Register Map */
#define MPU6050_REG_SELF_TEST_X (0x0D)
#define MPU6050_REG_SMPLRT_DIV (0x19)
#define MPU6050_REG_CONFIG (0x1A)
#define MPU6050_REG_GYRO_CONFIG (0x1B)
#define MPU6050_REG_ACCEL_CONFIG (0x1C)
#define MPU6050_REG_FIFO_EN (0x23)
#define MPU6050_REG_INT_ENABLE (0x38)
#define MPU6050_REG_INT_STATUS (0x3A)
#define MPU6050_REG_ACCEL_XOUT_H (0x3B)
#define MPU6050_REG_TEMP_OUT_H (0x41)
#define MPU6050_REG_GYRO_XOUT_H (0x43)
#define MPU6050_REG_USER_CTRL (0x6A)
#define MPU6050_REG_PWR_MGMT_1 (0x6B)
#define MPU6050_REG_PWR_MGMT_2 (0x6C)
#define MPU6050_REG_FIFO_COUNT_H (0x72)
#define MPU6050_REG_FIFO_R_W (0x74)
#define MPU6050_REG_WHO_AM_I (0x75)

/* Power Management */
#define MPU6050_PWR1_DEVICE_RESET (1 << 7)
#define MPU6050_PWR1_SLEEP (1 << 6)
#define MPU6050_PWR1_CYCLE (1 << 5)
#define MPU6050_PWR1_TEMP_DIS (1 << 3)
#define MPU6050_PWR1_CLKSEL_MASK (0x07)

/* FIFO Enable bits */
#define MPU6050_FIFO_EN_TEMP (1 << 7)
#define MPU6050_FIFO_EN_XG (1 << 6)
#define MPU6050_FIFO_EN_YG (1 << 5)
#define MPU6050_FIFO_EN_ZG (1 << 4)
#define MPU6050_FIFO_EN_ACCEL (1 << 3)

/* User Control bits */
#define MPU6050_USERCTRL_FIFO_EN (1 << 6)
#define MPU6050_USERCTRL_FIFO_RESET (1 << 2)
#define MPU6050_USERCTRL_SIG_COND_RESET (1 << 0)

#define MPU6050_FIFO_SIZE (1024)

typedef enum {
  MPU6050_ACCEL_RANGE_2G  = 0,
  MPU6050_ACCEL_RANGE_4G  = 1,
  MPU6050_ACCEL_RANGE_8G  = 2,
  MPU6050_ACCEL_RANGE_16G = 3
} mpu6050_accel_range_t;

typedef enum {
  MPU6050_GYRO_RANGE_250  = 0,
  MPU6050_GYRO_RANGE_500  = 1,
  MPU6050_GYRO_RANGE_1000 = 2,
  MPU6050_GYRO_RANGE_2000 = 3
} mpu6050_gyro_range_t;

typedef enum {
  MPU6050_DLPF_260HZ = 0,
  MPU6050_DLPF_184HZ = 1,
  MPU6050_DLPF_94HZ  = 2,
  MPU6050_DLPF_44HZ  = 3,
  MPU6050_DLPF_21HZ  = 4,
  MPU6050_DLPF_10HZ  = 5,
  MPU6050_DLPF_5HZ   = 6
} mpu6050_dlpf_t;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} mpu6050_raw_accel_t;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} mpu6050_raw_gyro_t;

typedef struct {
  float x_g;
  float y_g;
  float z_g;
} mpu6050_accel_t;

typedef struct {
  float x_dps;
  float y_dps;
  float z_dps;
} mpu6050_gyro_t;

typedef struct {
  uint8_t               i2c_addr;
  mpu6050_accel_range_t accel_range;
  mpu6050_gyro_range_t  gyro_range;
  mpu6050_dlpf_t        dlpf;
  uint8_t               sample_rate_div;
  bool                  enable_fifo;
} mpu6050_config_t;

/**
 * @brief MPU6050 device handle
 *
 * Maintains state for IMU operations.
 * Thread-safe: All operations are protected by an internal mutex.
 */
typedef struct mpu6050_handle {
  star_bus_manager_t*     manager;            /**< Pointer to bus manager */
  const char*             bus_name;           /**< Name of I2C bus for this device */
  uint8_t                 i2c_addr;           /**< I2C device address */
  mpu6050_config_t        config;             /**< Device configuration */
  star_error_interface_t* error_iface;        /**< Injected error interface (NULL = default) */
  SemaphoreHandle_t       mutex;              /**< Mutex for thread-safe operations */
  bool                    initialized;        /**< Initialization state */
  bool                    owns_error_handler; /**< True if we created default error handler */
  float                   accel_sensitivity;  /**< Accelerometer LSB/g sensitivity */
  float                   gyro_sensitivity;   /**< Gyroscope LSB/dps sensitivity */
} mpu6050_handle_t;

/**
 * @brief Initialize MPU6050 IMU sensor
 *
 * Performs device detection, configuration, and initialization.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle      Pointer to handle structure (must be allocated by caller)
 * @param[in]  manager     Pointer to initialized bus manager
 * @param[in]  bus_name    Name of I2C bus configured for this device
 * @param[in]  error_iface Error interface for error handling (NULL = create default internally)
 * @param[in]  config      Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_sensor_mpu6050_deinit() when done
 * @note If error_iface is NULL, a default error handler will be created internally
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_sensor_mpu6050_init(mpu6050_handle_t*       handle,
                                   star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   star_error_interface_t* error_iface,
                                   const mpu6050_config_t* config);

esp_err_t star_sensor_mpu6050_deinit(mpu6050_handle_t* handle);

esp_err_t star_sensor_mpu6050_reset(const mpu6050_handle_t* handle);

esp_err_t star_sensor_mpu6050_read_accel_raw(const mpu6050_handle_t* handle,
                                             mpu6050_raw_accel_t*    accel);

esp_err_t star_sensor_mpu6050_read_gyro_raw(const mpu6050_handle_t* handle,
                                            mpu6050_raw_gyro_t*     gyro);

esp_err_t star_sensor_mpu6050_read_accel(const mpu6050_handle_t* handle, mpu6050_accel_t* accel);

esp_err_t star_sensor_mpu6050_read_gyro(const mpu6050_handle_t* handle, mpu6050_gyro_t* gyro);

esp_err_t star_sensor_mpu6050_read_temperature(const mpu6050_handle_t* handle, float* temp_c);

esp_err_t star_sensor_mpu6050_read_all(const mpu6050_handle_t* handle,
                                       mpu6050_accel_t*        accel,
                                       mpu6050_gyro_t*         gyro,
                                       float*                  temp_c);

esp_err_t star_sensor_mpu6050_fifo_enable(mpu6050_handle_t* handle, bool enable);

esp_err_t star_sensor_mpu6050_fifo_reset(const mpu6050_handle_t* handle);

esp_err_t star_sensor_mpu6050_fifo_get_count(const mpu6050_handle_t* handle, uint16_t* count);

esp_err_t
star_sensor_mpu6050_fifo_read(const mpu6050_handle_t* handle, uint8_t* data, uint16_t len);

esp_err_t star_sensor_mpu6050_set_sleep(mpu6050_handle_t* handle, bool sleep);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_MPU6050_H */
