/* lib/star_sensor_bno055_bmp280/include/star_sensor_bno055_bmp280.h */

#ifndef STAR_SENSOR_BNO055_BMP280_H
#define STAR_SENSOR_BNO055_BMP280_H

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
 * @file star_sensor_bno055_bmp280.h
 * @brief BNO055 + BMP280 10-DOF IMU driver
 *
 * This driver combines:
 * - BNO055: 9-axis sensor fusion (accelerometer, gyroscope, magnetometer)
 *   with on-chip Cortex M0+ running sensor fusion algorithms
 * - BMP280: Barometric pressure and temperature sensor
 *
 * Together providing 10 degrees of freedom:
 * - 3-axis acceleration
 * - 3-axis angular velocity
 * - 3-axis magnetic field
 * - Pressure/altitude
 *
 * BNO055 provides:
 * - Quaternion output
 * - Euler angles (heading, roll, pitch)
 * - Linear acceleration (gravity-compensated)
 * - Automatic calibration
 *
 * Example Usage:
 * @code
 * // === Basic Setup ===
 *
 * #include "star_sensor_bno055_bmp280.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // 1. Setup bus manager (see star_bus_manager.h for full setup)
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Create I2C bus for sensors (both BNO055 and BMP280 on same bus)
 * star_bus_config_t* imu_bus = star_bus_config_create_i2c(
 *     "imu_i2c",
 *     I2C_NUM_0,
 *     BNO055_I2C_ADDR,        // 0x28 (or 0x29 if ADR pin high)
 *     GPIO_NUM_21,            // SDA
 *     GPIO_NUM_22,            // SCL
 *     400000                  // 400kHz
 * );
 * star_bus_manager_add_bus(&bus_manager, imu_bus);
 *
 * // 3. Configure and initialize 10-DOF IMU
 * imu_10dof_handle_t imu;
 * imu_10dof_config_t config = {
 *     .manager = &bus_manager,
 *     .bus_name = "imu_i2c",
 *     .bno055_addr = BNO055_I2C_ADDR,   // 0x28
 *     .bmp280_addr = BMP280_I2C_ADDR,   // 0x76
 *     .operation_mode = k_bno055_op_mode_ndof  // Full 9-DOF fusion
 * };
 *
 * esp_err_t ret = star_sensor_imu_10dof_init(&imu, NULL, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init 10-DOF IMU: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Reading All Sensor Data ===
 *
 * imu_10dof_data_t data;
 * ret = star_sensor_imu_10dof_read(&imu, &data);
 * if (ret == ESP_OK) {
 *     // Euler angles (heading/yaw, roll, pitch)
 *     ESP_LOGI(TAG, "Heading: %.1f°, Roll: %.1f°, Pitch: %.1f°",
 *              data.euler.heading, data.euler.roll, data.euler.pitch);
 *
 *     // Quaternion (for 3D orientation)
 *     ESP_LOGI(TAG, "Quaternion: w=%d, x=%d, y=%d, z=%d",
 *              data.quaternion.w, data.quaternion.x,
 *              data.quaternion.y, data.quaternion.z);
 *
 *     // Linear acceleration (gravity removed)
 *     ESP_LOGI(TAG, "Linear Accel: X=%d, Y=%d, Z=%d",
 *              data.linear_accel.x, data.linear_accel.y, data.linear_accel.z);
 *
 *     // Gravity vector
 *     ESP_LOGI(TAG, "Gravity: X=%d, Y=%d, Z=%d",
 *              data.gravity.x, data.gravity.y, data.gravity.z);
 *
 *     // Raw sensor data
 *     ESP_LOGI(TAG, "Accel: X=%d, Y=%d, Z=%d",
 *              data.accelerometer.x, data.accelerometer.y, data.accelerometer.z);
 *     ESP_LOGI(TAG, "Gyro: X=%d, Y=%d, Z=%d",
 *              data.gyroscope.x, data.gyroscope.y, data.gyroscope.z);
 *     ESP_LOGI(TAG, "Mag: X=%d, Y=%d, Z=%d",
 *              data.magnetometer.x, data.magnetometer.y, data.magnetometer.z);
 *
 *     // BMP280 pressure and altitude
 *     ESP_LOGI(TAG, "Pressure: %.2f Pa, Altitude: %.2f m",
 *              data.pressure_pa, data.altitude_m);
 *
 *     // Temperature (from both sensors)
 *     ESP_LOGI(TAG, "BNO055 temp: %d°C, BMP280 temp: %.2f°C",
 *              data.temperature_c, data.bmp_temp_c);
 *
 *     // Calibration status
 *     ESP_LOGI(TAG, "Calibration: sys=%d, gyro=%d, accel=%d, mag=%d",
 *              data.calibration.sys, data.calibration.gyro,
 *              data.calibration.accel, data.calibration.mag);
 * }
 *
 *
 * // === Calibration Status ===
 *
 * // Check if system is fully calibrated
 * bool calibrated;
 * star_sensor_imu_10dof_is_calibrated(&imu, &calibrated);
 * if (calibrated) {
 *     ESP_LOGI(TAG, "IMU fully calibrated!");
 * }
 *
 * // Get detailed calibration status (0-3 for each sensor)
 * calibration_status_t cal_status;
 * star_sensor_imu_10dof_get_calibration(&imu, &cal_status);
 * ESP_LOGI(TAG, "Cal levels: System=%d, Gyro=%d, Accel=%d, Mag=%d",
 *          cal_status.sys, cal_status.gyro, cal_status.accel, cal_status.mag);
 *
 * // Wait for calibration (BNO055 auto-calibrates)
 * while (!calibrated) {
 *     star_sensor_imu_10dof_is_calibrated(&imu, &calibrated);
 *     star_sensor_imu_10dof_get_calibration(&imu, &cal_status);
 *     ESP_LOGI(TAG, "Calibrating... sys=%d, gyro=%d, accel=%d, mag=%d",
 *              cal_status.sys, cal_status.gyro, cal_status.accel, cal_status.mag);
 *     vTaskDelay(pdMS_TO_TICKS(500));
 * }
 *
 *
 * // === Operation Modes ===
 *
 * // NDOF mode - Full 9-DOF sensor fusion (default)
 * star_sensor_imu_10dof_set_mode(&imu, k_bno055_op_mode_ndof);
 *
 * // IMU mode - Accel + Gyro only (no magnetometer)
 * star_sensor_imu_10dof_set_mode(&imu, k_bno055_op_mode_accgyro);
 *
 * // Config mode - For configuration changes
 * star_sensor_imu_10dof_set_mode(&imu, k_bno055_op_mode_config);
 *
 * // Available modes:
 * // - k_bno055_op_mode_config:       Configuration mode
 * // - k_bno055_op_mode_acconly:      Accelerometer only
 * // - k_bno055_op_mode_magonly:      Magnetometer only
 * // - k_bno055_op_mode_gyroonly:     Gyroscope only
 * // - k_bno055_op_mode_accmag:       Accel + Mag
 * // - k_bno055_op_mode_accgyro:      Accel + Gyro
 * // - k_bno055_op_mode_maggyro:      Mag + Gyro
 * // - k_bno055_op_mode_amg:          All sensors, no fusion
 * // - k_bno055_op_mode_ndof_fmc_off: 9-DOF fusion, mag cal off
 * // - k_bno055_op_mode_ndof:         9-DOF fusion (recommended)
 *
 *
 * // === Reset BNO055 ===
 *
 * star_sensor_imu_10dof_reset_bno055(&imu);
 * vTaskDelay(pdMS_TO_TICKS(650));  // Wait for reset complete
 *
 *
 * // === Continuous Reading Task ===
 *
 * void imu_task(void* arg) {
 *     imu_10dof_handle_t* imu = (imu_10dof_handle_t*)arg;
 *     imu_10dof_data_t data;
 *
 *     while (1) {
 *         if (star_sensor_imu_10dof_read(imu, &data) == ESP_OK) {
 *             // Use euler angles for orientation
 *             float heading = data.euler.heading;
 *             float roll = data.euler.roll;
 *             float pitch = data.euler.pitch;
 *
 *             // Use altitude for height
 *             float altitude = data.altitude_m;
 *
 *             // Process data...
 *         }
 *         vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz update rate
 *     }
 * }
 *
 * xTaskCreate(imu_task, "imu_task", 4096, &imu, 5, NULL);
 *
 *
 * // === Cleanup ===
 *
 * star_sensor_imu_10dof_deinit(&imu);
 * star_bus_manager_remove_bus(&bus_manager, "imu_i2c");
 * @endcode
 */

#define BNO055_I2C_ADDR (0x28)
#define BMP280_I2C_ADDR (0x76)

typedef enum {
  k_bno055_power_mode_normal  = 0x00,
  k_bno055_power_mode_low     = 0x01,
  k_bno055_power_mode_suspend = 0x02,
} bno055_power_mode_t;

typedef enum {
  k_bno055_op_mode_config       = 0x00,
  k_bno055_op_mode_acconly      = 0x01,
  k_bno055_op_mode_magonly      = 0x02,
  k_bno055_op_mode_gyroonly     = 0x03,
  k_bno055_op_mode_accmag       = 0x04,
  k_bno055_op_mode_accgyro      = 0x05,
  k_bno055_op_mode_maggyro      = 0x06,
  k_bno055_op_mode_amg          = 0x07,
  k_bno055_op_mode_ndof_fmc_off = 0x0B,
  k_bno055_op_mode_ndof         = 0x0C, // 9-DOF sensor fusion
} bno055_op_mode_t;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} vec3_t;

typedef struct {
  int16_t w;
  int16_t x;
  int16_t y;
  int16_t z;
} quaternion_t;

typedef struct {
  float heading; // Yaw (0-359.99°)
  float roll;    // Roll (-180 to +180°)
  float pitch;   // Pitch (-90 to +90°)
} euler_t;

typedef struct {
  uint8_t sys;   // System calibration (0-3)
  uint8_t gyro;  // Gyroscope calibration (0-3)
  uint8_t accel; // Accelerometer calibration (0-3)
  uint8_t mag;   // Magnetometer calibration (0-3)
} calibration_status_t;

typedef struct {
  vec3_t               accelerometer; // m/s² (raw)
  vec3_t               gyroscope;     // dps (raw)
  vec3_t               magnetometer;  // μT (raw)
  vec3_t               linear_accel;  // m/s² (gravity compensated)
  vec3_t               gravity;       // m/s²
  quaternion_t         quaternion;    // Unit quaternion
  euler_t              euler;         // Euler angles
  int8_t               temperature_c; // BNO055 temperature
  float                pressure_pa;   // BMP280 pressure in pascals
  float                altitude_m;    // Calculated altitude
  float                bmp_temp_c;    // BMP280 temperature
  calibration_status_t calibration;
} imu_10dof_data_t;

typedef struct {
  star_bus_manager_t* manager;
  const char*         bus_name;
  uint8_t             bno055_addr;
  uint8_t             bmp280_addr;
  bno055_op_mode_t    operation_mode;
} imu_10dof_config_t;

/**
 * @brief 10-DOF IMU device handle
 *
 * Maintains state for BNO055 + BMP280 sensor fusion.
 * Thread-safe: All operations are protected by an internal mutex.
 */
typedef struct imu_10dof_handle {
  star_bus_manager_t*     manager;            /**< Pointer to bus manager */
  const char*             bus_name;           /**< Name of I2C bus */
  uint8_t                 bno055_addr;        /**< BNO055 I2C address */
  uint8_t                 bmp280_addr;        /**< BMP280 I2C address */
  star_error_interface_t* error_iface;        /**< Injected error interface (NULL = default) */
  SemaphoreHandle_t       mutex;              /**< Mutex for thread-safe operations */
  bool                    initialized;        /**< Initialization state */
  bool                    owns_error_handler; /**< True if we created default error handler */
  bno055_op_mode_t        operation_mode;     /**< BNO055 operation mode */
  // BMP280 calibration data
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2;
  int16_t  dig_P3;
  int16_t  dig_P4;
  int16_t  dig_P5;
  int16_t  dig_P6;
  int16_t  dig_P7;
  int16_t  dig_P8;
  int16_t  dig_P9;
  int32_t  t_fine; /**< Temperature compensation value */
} imu_10dof_handle_t;

/**
 * @brief Initialize 10-DOF IMU (BNO055 + BMP280)
 *
 * Performs device detection, calibration loading, and initialization.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle      Pointer to handle structure (must be allocated by caller)
 * @param[in]  error_iface Error interface for error handling (NULL = create default internally)
 * @param[in]  config      Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_sensor_imu_10dof_deinit() when done
 * @note If error_iface is NULL, a default error handler will be created internally
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_sensor_imu_10dof_init(imu_10dof_handle_t*       handle,
                                     star_error_interface_t*   error_iface,
                                     const imu_10dof_config_t* config);

/**
 * @brief Deinitialize 10-DOF IMU
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_deinit(imu_10dof_handle_t* handle);

/**
 * @brief Read all sensor data
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Complete sensor data
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_read(const imu_10dof_handle_t* handle, imu_10dof_data_t* data);

/**
 * @brief Get calibration status
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] status Calibration status for all sensors
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_get_calibration(const imu_10dof_handle_t* handle,
                                                calibration_status_t*     status);

/**
 * @brief Check if system is fully calibrated
 *
 * @param[in]  handle     Pointer to initialized handle
 * @param[out] calibrated true if all sensors at calibration level 3
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_is_calibrated(const imu_10dof_handle_t* handle, bool* calibrated);

/**
 * @brief Set operation mode
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] mode   Operation mode
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_set_mode(imu_10dof_handle_t* handle, bno055_op_mode_t mode);

/**
 * @brief Reset BNO055 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_imu_10dof_reset_bno055(imu_10dof_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_BNO055_BMP280_H */
