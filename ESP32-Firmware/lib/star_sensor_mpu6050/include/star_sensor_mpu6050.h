/* lib/star_sensor_mpu6050/include/star_sensor_mpu6050.h */

#ifndef STAR_SENSOR_MPU6050_H
#define STAR_SENSOR_MPU6050_H

/* System headers */
#include <stdbool.h>
#include <stdint.h>

/* FreeRTOS headers */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ESP-IDF headers */
#include "esp_err.h"

/* Project headers */
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
 *     s_mpu6050_i2c_addr_low,   // 0x68 (AD0 pin low)
 *     GPIO_NUM_21,            // SDA
 *     GPIO_NUM_22,            // SCL
 *     400000                  // 400kHz fast mode
 * );
 * star_bus_manager_add_bus(&bus_manager, i2c_bus);
 *
 * // 3. Configure and initialize MPU6050
 * mpu6050_handle_t imu;
 * mpu6050_config_t config = {
 *     .i2c_addr = s_mpu6050_i2c_addr_low,
 *     .accel_range = k_mpu6050_accel_range_4g,   // ±4g
 *     .gyro_range = k_mpu6050_gyro_range_500,    // ±500 °/s
 *     .dlpf = k_mpu6050_dlpf_44hz,               // 44Hz low-pass filter
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

/* Type-safe I2C addresses */
static const uint8_t s_mpu6050_i2c_addr_low  = 0x68; /* AD0 pin pulled low */
static const uint8_t s_mpu6050_i2c_addr_high = 0x69; /* AD0 pin pulled high */
static const uint8_t s_mpu6050_who_am_i_val  = 0x68; /* Expected WHO_AM_I value */

/* MPU6050 Register Map */
typedef enum {
  k_mpu6050_reg_self_test_x  = 0x0D,
  k_mpu6050_reg_smplrt_div   = 0x19,
  k_mpu6050_reg_config       = 0x1A,
  k_mpu6050_reg_gyro_config  = 0x1B,
  k_mpu6050_reg_accel_config = 0x1C,
  k_mpu6050_reg_fifo_en      = 0x23,
  k_mpu6050_reg_int_enable   = 0x38,
  k_mpu6050_reg_int_status   = 0x3A,
  k_mpu6050_reg_accel_xout_h = 0x3B,
  k_mpu6050_reg_temp_out_h   = 0x41,
  k_mpu6050_reg_gyro_xout_h  = 0x43,
  k_mpu6050_reg_user_ctrl    = 0x6A,
  k_mpu6050_reg_pwr_mgmt_1   = 0x6B,
  k_mpu6050_reg_pwr_mgmt_2   = 0x6C,
  k_mpu6050_reg_fifo_count_h = 0x72,
  k_mpu6050_reg_fifo_r_w     = 0x74,
  k_mpu6050_reg_who_am_i     = 0x75
} mpu6050_register_t;

/* Power Management constants */
static const uint8_t s_mpu6050_pwr1_device_reset = (1 << 7);
static const uint8_t s_mpu6050_pwr1_sleep        = (1 << 6);
static const uint8_t s_mpu6050_pwr1_cycle        = (1 << 5);
static const uint8_t s_mpu6050_pwr1_temp_dis     = (1 << 3);
static const uint8_t s_mpu6050_pwr1_clksel_mask  = 0x07;

/* FIFO Enable bits */
static const uint8_t s_mpu6050_fifo_en_temp  = (1 << 7);
static const uint8_t s_mpu6050_fifo_en_xg    = (1 << 6);
static const uint8_t s_mpu6050_fifo_en_yg    = (1 << 5);
static const uint8_t s_mpu6050_fifo_en_zg    = (1 << 4);
static const uint8_t s_mpu6050_fifo_en_accel = (1 << 3);

/* User Control bits */
static const uint8_t s_mpu6050_userctrl_fifo_en        = (1 << 6);
static const uint8_t s_mpu6050_userctrl_fifo_reset     = (1 << 2);
static const uint8_t s_mpu6050_userctrl_sig_cond_reset = (1 << 0);

/* FIFO size constant */
static const uint16_t s_mpu6050_fifo_size = 1024;

/**
 * @brief Temperature sensor conversion constants
 *
 * The MPU6050 temperature sensor raw output is converted to degrees Celsius using:
 *   Temperature (C) = (TEMP_OUT / TEMP_SENSITIVITY) + TEMP_OFFSET
 *
 * Where TEMP_OUT is the 16-bit signed raw temperature value from registers 0x41-0x42.
 * These values are from the MPU6050 Register Map and Descriptions document, Section 4.18.
 */
static const float s_mpu6050_temp_sensitivity = 340.0f; /* LSB per degree C */
static const float s_mpu6050_temp_offset      = 36.53f; /* Offset in degrees C */

/**
 * @brief Accelerometer sensitivity values (LSB/g)
 *
 * These values convert raw 16-bit accelerometer readings to g-units.
 * From MPU6050 Register Map, Section 4.2 (ACCEL_CONFIG register):
 *   AFS_SEL=0 (+/-2g):  16384 LSB/g
 *   AFS_SEL=1 (+/-4g):   8192 LSB/g
 *   AFS_SEL=2 (+/-8g):   4096 LSB/g
 *   AFS_SEL=3 (+/-16g):  2048 LSB/g
 */
static const float s_mpu6050_accel_sens_2g  = 16384.0f;
static const float s_mpu6050_accel_sens_4g  = 8192.0f;
static const float s_mpu6050_accel_sens_8g  = 4096.0f;
static const float s_mpu6050_accel_sens_16g = 2048.0f;

/**
 * @brief Gyroscope sensitivity values (LSB per degree/s)
 *
 * These values convert raw 16-bit gyroscope readings to degrees per second.
 * From MPU6050 Register Map, Section 4.3 (GYRO_CONFIG register):
 *   FS_SEL=0 (+/-250 deg/s):  131.0 LSB per deg/s
 *   FS_SEL=1 (+/-500 deg/s):   65.5 LSB per deg/s
 *   FS_SEL=2 (+/-1000 deg/s):  32.8 LSB per deg/s
 *   FS_SEL=3 (+/-2000 deg/s):  16.4 LSB per deg/s
 */
static const float s_mpu6050_gyro_sens_250  = 131.0f;
static const float s_mpu6050_gyro_sens_500  = 65.5f;
static const float s_mpu6050_gyro_sens_1000 = 32.8f;
static const float s_mpu6050_gyro_sens_2000 = 16.4f;

/**
 * @brief Accelerometer full-scale range selection
 *
 * Determines the measurement range and sensitivity of the accelerometer.
 * Larger ranges allow measuring higher accelerations but with lower resolution.
 */
typedef enum {
  k_mpu6050_accel_range_2g  = 0, /**< +/- 2g range (16384 LSB/g) */
  k_mpu6050_accel_range_4g  = 1, /**< +/- 4g range (8192 LSB/g) */
  k_mpu6050_accel_range_8g  = 2, /**< +/- 8g range (4096 LSB/g) */
  k_mpu6050_accel_range_16g = 3  /**< +/- 16g range (2048 LSB/g) */
} mpu6050_accel_range_t;

/**
 * @brief Gyroscope full-scale range selection
 *
 * Determines the measurement range and sensitivity of the gyroscope.
 * Larger ranges allow measuring higher angular velocities but with lower resolution.
 */
typedef enum {
  k_mpu6050_gyro_range_250  = 0, /**< +/- 250 deg/s (131.0 LSB per deg/s) */
  k_mpu6050_gyro_range_500  = 1, /**< +/- 500 deg/s (65.5 LSB per deg/s) */
  k_mpu6050_gyro_range_1000 = 2, /**< +/- 1000 deg/s (32.8 LSB per deg/s) */
  k_mpu6050_gyro_range_2000 = 3  /**< +/- 2000 deg/s (16.4 LSB per deg/s) */
} mpu6050_gyro_range_t;

/**
 * @brief Digital Low Pass Filter (DLPF) configuration
 *
 * Controls the bandwidth of the internal digital low-pass filter.
 * Lower bandwidth reduces noise but increases latency.
 * The gyroscope output rate is 8kHz when DLPF is disabled (260Hz),
 * or 1kHz when DLPF is enabled (any other setting).
 */
typedef enum {
  k_mpu6050_dlpf_260hz = 0, /**< Accel: 260Hz, Gyro: 256Hz, 8kHz output */
  k_mpu6050_dlpf_184hz = 1, /**< Accel: 184Hz, Gyro: 188Hz */
  k_mpu6050_dlpf_94hz  = 2, /**< Accel: 94Hz, Gyro: 98Hz */
  k_mpu6050_dlpf_44hz  = 3, /**< Accel: 44Hz, Gyro: 42Hz */
  k_mpu6050_dlpf_21hz  = 4, /**< Accel: 21Hz, Gyro: 20Hz */
  k_mpu6050_dlpf_10hz  = 5, /**< Accel: 10Hz, Gyro: 10Hz */
  k_mpu6050_dlpf_5hz   = 6  /**< Accel: 5Hz, Gyro: 5Hz (most filtering) */
} mpu6050_dlpf_t;

/**
 * @brief Raw accelerometer data (16-bit signed integers)
 *
 * Contains the unprocessed sensor readings directly from the registers.
 * Use the appropriate sensitivity value to convert to g-units.
 */
typedef struct {
  int16_t x; /**< X-axis raw value */
  int16_t y; /**< Y-axis raw value */
  int16_t z; /**< Z-axis raw value */
} mpu6050_raw_accel_t;

/**
 * @brief Raw gyroscope data (16-bit signed integers)
 *
 * Contains the unprocessed sensor readings directly from the registers.
 * Use the appropriate sensitivity value to convert to degrees/second.
 */
typedef struct {
  int16_t x; /**< X-axis raw value */
  int16_t y; /**< Y-axis raw value */
  int16_t z; /**< Z-axis raw value */
} mpu6050_raw_gyro_t;

/**
 * @brief Processed accelerometer data in g-units
 *
 * Contains accelerometer readings converted to gravitational units (g).
 * 1g = 9.81 m/s^2
 */
typedef struct {
  float x_g; /**< X-axis acceleration in g */
  float y_g; /**< Y-axis acceleration in g */
  float z_g; /**< Z-axis acceleration in g */
} mpu6050_accel_t;

/**
 * @brief Processed gyroscope data in degrees per second
 *
 * Contains gyroscope readings converted to angular velocity units.
 */
typedef struct {
  float x_dps; /**< X-axis angular velocity in deg/s */
  float y_dps; /**< Y-axis angular velocity in deg/s */
  float z_dps; /**< Z-axis angular velocity in deg/s */
} mpu6050_gyro_t;

/**
 * @brief MPU6050 configuration structure
 *
 * Used during initialization to configure the sensor parameters.
 */
typedef struct {
  uint8_t               i2c_addr;        /**< I2C address (0x68 or 0x69) */
  mpu6050_accel_range_t accel_range;     /**< Accelerometer full-scale range */
  mpu6050_gyro_range_t  gyro_range;      /**< Gyroscope full-scale range */
  mpu6050_dlpf_t        dlpf;            /**< Digital low-pass filter setting */
  uint8_t               sample_rate_div; /**< Sample rate = 1kHz / (1 + div) */
  bool                  enable_fifo;     /**< Enable hardware FIFO buffer */
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

/**
 * @brief Deinitialize MPU6050 and release resources
 *
 * Releases the mutex and cleans up error handler if owned internally.
 * After calling this function, the handle is no longer valid for operations.
 *
 * @param[in,out] handle Pointer to initialized handle
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle is NULL or not initialized
 */
esp_err_t star_sensor_mpu6050_deinit(mpu6050_handle_t* handle);

/**
 * @brief Perform software reset of the MPU6050
 *
 * Resets all registers to their default values and restores the configured
 * settings. The device will be ready for use after this function returns.
 * Thread-safe: Protected by internal mutex.
 *
 * @param[in,out] handle Pointer to initialized handle
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if handle is NULL or not initialized
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 */
esp_err_t star_sensor_mpu6050_reset(mpu6050_handle_t* handle);

/**
 * @brief Read raw accelerometer data
 *
 * Reads the raw 16-bit signed values from the accelerometer registers.
 * Use the sensitivity values (MPU6050_ACCEL_SENS_*) to convert to g-units.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] accel  Pointer to structure to receive raw accelerometer data
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or accel is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_read_accel_raw(const mpu6050_handle_t* handle,
                                             mpu6050_raw_accel_t*    accel);

/**
 * @brief Read raw gyroscope data
 *
 * Reads the raw 16-bit signed values from the gyroscope registers.
 * Use the sensitivity values (MPU6050_GYRO_SENS_*) to convert to degrees/second.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] gyro   Pointer to structure to receive raw gyroscope data
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or gyro is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_read_gyro_raw(const mpu6050_handle_t* handle,
                                            mpu6050_raw_gyro_t*     gyro);

/**
 * @brief Read accelerometer data in g-units
 *
 * Reads accelerometer data and converts to floating-point g-units
 * based on the configured accelerometer range.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] accel  Pointer to structure to receive accelerometer data (in g)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or accel is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_read_accel(const mpu6050_handle_t* handle,
                                         mpu6050_accel_t* const  accel);

/**
 * @brief Read gyroscope data in degrees per second
 *
 * Reads gyroscope data and converts to floating-point degrees/second
 * based on the configured gyroscope range.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] gyro   Pointer to structure to receive gyroscope data (in deg/s)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or gyro is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_read_gyro(const mpu6050_handle_t* handle, mpu6050_gyro_t* const gyro);

/**
 * @brief Read temperature sensor
 *
 * Reads the on-chip temperature sensor and converts to degrees Celsius.
 * Note: This measures die temperature, not ambient temperature.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] temp_c Pointer to receive temperature in degrees Celsius
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or temp_c is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_read_temperature(const mpu6050_handle_t* handle, float* const temp_c);

/**
 * @brief Read all sensor data in a single burst
 *
 * Reads accelerometer, gyroscope, and temperature data in one I2C transaction.
 * This is more efficient than calling individual read functions when all data
 * is needed, as it performs a single burst read of all sensor registers.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] accel  Pointer to receive accelerometer data (in g), may be NULL
 * @param[out] gyro   Pointer to receive gyroscope data (in deg/s), may be NULL
 * @param[out] temp_c Pointer to receive temperature (in C), may be NULL
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle is NULL or not initialized
 *
 * @note At least one output pointer must be non-NULL
 */
esp_err_t star_sensor_mpu6050_read_all(const mpu6050_handle_t* handle,
                                       mpu6050_accel_t* const  accel,
                                       mpu6050_gyro_t* const   gyro,
                                       float* const            temp_c);

/**
 * @brief Enable or disable the hardware FIFO buffer
 *
 * When enabled, the FIFO stores accelerometer, gyroscope, and temperature
 * data at the sample rate. This allows burst reading of accumulated samples.
 * Thread-safe: Protected by internal mutex.
 *
 * @param[in,out] handle Pointer to initialized handle
 * @param[in]     enable true to enable FIFO, false to disable
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if handle is NULL or not initialized
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 *
 * @note FIFO must be enabled in config during init for this to have effect
 * @note FIFO size is 1024 bytes (MPU6050_FIFO_SIZE)
 */
esp_err_t star_sensor_mpu6050_fifo_enable(mpu6050_handle_t* const handle, const bool enable);

/**
 * @brief Reset the hardware FIFO buffer
 *
 * Clears all data in the FIFO and resets the read/write pointers.
 * Thread-safe: Protected by internal mutex.
 *
 * @param[in,out] handle Pointer to initialized handle
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if handle is NULL or not initialized
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 */
esp_err_t star_sensor_mpu6050_fifo_reset(mpu6050_handle_t* handle);

/**
 * @brief Get the number of bytes currently in the FIFO
 *
 * Returns the number of bytes available for reading from the FIFO buffer.
 * Each sample consists of 14 bytes (6 accel + 2 temp + 6 gyro).
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] count  Pointer to receive FIFO byte count
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or count is NULL, or handle not initialized
 */
esp_err_t star_sensor_mpu6050_fifo_get_count(const mpu6050_handle_t* handle, uint16_t* const count);

/**
 * @brief Read data from the FIFO buffer
 *
 * Reads the specified number of bytes from the FIFO. The caller must ensure
 * the buffer is large enough and that sufficient data is available (use
 * star_sensor_mpu6050_fifo_get_count() to check).
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Buffer to receive FIFO data
 * @param[in]  len    Number of bytes to read
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle or data is NULL, or handle not initialized
 *
 * @note Each complete sample is 14 bytes (accel + temp + gyro)
 * @note Reading more bytes than available may return stale or undefined data
 */
esp_err_t star_sensor_mpu6050_fifo_read(const mpu6050_handle_t* handle,
                                        uint8_t* const          data,
                                        const uint16_t          len);

/**
 * @brief Set device sleep mode
 *
 * Enables or disables low-power sleep mode. In sleep mode, the device
 * consumes minimal power but cannot perform measurements.
 * Thread-safe: Protected by internal mutex.
 *
 * @param[in,out] handle Pointer to initialized handle
 * @param[in]     sleep  true to enter sleep mode, false to wake up
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if handle is NULL or not initialized
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 *
 * @note After waking from sleep, allow a few milliseconds for readings to stabilize
 */
esp_err_t star_sensor_mpu6050_set_sleep(mpu6050_handle_t* const handle, const bool sleep);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_MPU6050_H */
