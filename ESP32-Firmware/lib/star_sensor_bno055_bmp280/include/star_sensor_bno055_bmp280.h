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
 */

#define BNO055_I2C_ADDR (0x28)
#define BMP280_I2C_ADDR (0x76)

typedef enum {
  BNO055_POWER_MODE_NORMAL  = 0x00,
  BNO055_POWER_MODE_LOW     = 0x01,
  BNO055_POWER_MODE_SUSPEND = 0x02,
} bno055_power_mode_t;

typedef enum {
  BNO055_OP_MODE_CONFIG       = 0x00,
  BNO055_OP_MODE_ACCONLY      = 0x01,
  BNO055_OP_MODE_MAGONLY      = 0x02,
  BNO055_OP_MODE_GYROONLY     = 0x03,
  BNO055_OP_MODE_ACCMAG       = 0x04,
  BNO055_OP_MODE_ACCGYRO      = 0x05,
  BNO055_OP_MODE_MAGGYRO      = 0x06,
  BNO055_OP_MODE_AMG          = 0x07,
  BNO055_OP_MODE_NDOF_FMC_OFF = 0x0B,
  BNO055_OP_MODE_NDOF         = 0x0C, // 9-DOF sensor fusion
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
