/**
 * @file bno055_driver.h
 * @brief BNO055 9-DOF IMU Driver Header for ESP32-IDF
 *
 * Comprehensive driver for Bosch Sensortec BNO055 absolute orientation sensor
 * with support for quaternion output, Euler angles, calibration management,
 * and thread-safe I2C operations.
 *
 * Author: Sensor Driver Implementation
 * Date: 2024
 * Target: ESP32-IDF v4.4+ with FreeRTOS
 */

#ifndef BNO055_DRIVER_H
#define BNO055_DRIVER_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== I2C ADDRESS CONFIGURATION ===== */

#define BNO055_I2C_ADDR_DEFAULT     0x29    /**< Default I2C address (COM3 = 3.3V) */
#define BNO055_I2C_ADDR_ALT         0x28    /**< Alternative address (COM3 = GND) */
#define BNO055_CHIP_ID_VALUE        0xA0    /**< Expected chip ID */

/* ===== REGISTER MAP - PAGE 0 (Operational) ===== */

#define BNO055_CHIP_ID_ADDR         0x00    /**< Chip ID register */
#define BNO055_ACC_ID_ADDR          0x01    /**< Accelerometer ID */
#define BNO055_MAG_ID_ADDR          0x02    /**< Magnetometer ID */
#define BNO055_GYRO_ID_ADDR         0x03    /**< Gyroscope ID */
#define BNO055_SW_REV_ID_LSB_ADDR   0x04    /**< Software revision LSB */
#define BNO055_SW_REV_ID_MSB_ADDR   0x05    /**< Software revision MSB */
#define BNO055_BL_REV_ID_ADDR       0x06    /**< Bootloader revision */
#define BNO055_PAGE_ID_ADDR         0x07    /**< Page select register */

/* Sensor Data Registers */
#define BNO055_ACC_DATA_X_LSB_ADDR  0x08
#define BNO055_ACC_DATA_X_MSB_ADDR  0x09
#define BNO055_ACC_DATA_Y_LSB_ADDR  0x0A
#define BNO055_ACC_DATA_Y_MSB_ADDR  0x0B
#define BNO055_ACC_DATA_Z_LSB_ADDR  0x0C
#define BNO055_ACC_DATA_Z_MSB_ADDR  0x0D

#define BNO055_MAG_DATA_X_LSB_ADDR  0x0E
#define BNO055_MAG_DATA_X_MSB_ADDR  0x0F
#define BNO055_MAG_DATA_Y_LSB_ADDR  0x10
#define BNO055_MAG_DATA_Y_MSB_ADDR  0x11
#define BNO055_MAG_DATA_Z_LSB_ADDR  0x12
#define BNO055_MAG_DATA_Z_MSB_ADDR  0x13

#define BNO055_GYRO_DATA_X_LSB_ADDR 0x14
#define BNO055_GYRO_DATA_X_MSB_ADDR 0x15
#define BNO055_GYRO_DATA_Y_LSB_ADDR 0x16
#define BNO055_GYRO_DATA_Y_MSB_ADDR 0x17
#define BNO055_GYRO_DATA_Z_LSB_ADDR 0x18
#define BNO055_GYRO_DATA_Z_MSB_ADDR 0x19

/* Euler Angles */
#define BNO055_EUL_HEADING_LSB_ADDR 0x1A
#define BNO055_EUL_HEADING_MSB_ADDR 0x1B
#define BNO055_EUL_ROLL_LSB_ADDR    0x1C
#define BNO055_EUL_ROLL_MSB_ADDR    0x1D
#define BNO055_EUL_PITCH_LSB_ADDR   0x1E
#define BNO055_EUL_PITCH_MSB_ADDR   0x1F

/* Quaternion */
#define BNO055_QUAT_W_LSB_ADDR      0x20
#define BNO055_QUAT_W_MSB_ADDR      0x21
#define BNO055_QUAT_X_LSB_ADDR      0x22
#define BNO055_QUAT_X_MSB_ADDR      0x23
#define BNO055_QUAT_Y_LSB_ADDR      0x24
#define BNO055_QUAT_Y_MSB_ADDR      0x25
#define BNO055_QUAT_Z_LSB_ADDR      0x26
#define BNO055_QUAT_Z_MSB_ADDR      0x27

/* Linear Acceleration */
#define BNO055_LIN_ACCEL_X_LSB_ADDR 0x28
#define BNO055_LIN_ACCEL_X_MSB_ADDR 0x29
#define BNO055_LIN_ACCEL_Y_LSB_ADDR 0x2A
#define BNO055_LIN_ACCEL_Y_MSB_ADDR 0x2B
#define BNO055_LIN_ACCEL_Z_LSB_ADDR 0x2C
#define BNO055_LIN_ACCEL_Z_MSB_ADDR 0x2D

/* Gravity Vector */
#define BNO055_GRAVITY_X_LSB_ADDR   0x2E
#define BNO055_GRAVITY_X_MSB_ADDR   0x2F
#define BNO055_GRAVITY_Y_LSB_ADDR   0x30
#define BNO055_GRAVITY_Y_MSB_ADDR   0x31
#define BNO055_GRAVITY_Z_LSB_ADDR   0x32
#define BNO055_GRAVITY_Z_MSB_ADDR   0x33

/* Temperature, Calibration Status */
#define BNO055_TEMP_ADDR            0x34
#define BNO055_CALIB_STAT_ADDR      0x35
#define BNO055_SELFTEST_RESULT_ADDR 0x36
#define BNO055_INTR_STAT_ADDR       0x37
#define BNO055_SYS_CLK_STATUS_ADDR  0x38
#define BNO055_SYS_STAT_ADDR        0x39
#define BNO055_SYS_ERR_ADDR         0x3A

/* Control Registers */
#define BNO055_OPR_MODE_ADDR        0x3D
#define BNO055_PWR_MODE_ADDR        0x3E
#define BNO055_SYS_TRIGGER_ADDR     0x3F
#define BNO055_TEMP_SOURCE_ADDR     0x40
#define BNO055_AXIS_MAP_CONFIG_ADDR 0x42
#define BNO055_AXIS_MAP_SIGN_ADDR   0x43

/* ===== REGISTER MAP - PAGE 1 (Calibration) ===== */

#define BNO055_ACC_OFFSET_X_LSB_ADDR    0x00
#define BNO055_ACC_OFFSET_X_MSB_ADDR    0x01
#define BNO055_ACC_OFFSET_Y_LSB_ADDR    0x02
#define BNO055_ACC_OFFSET_Y_MSB_ADDR    0x03
#define BNO055_ACC_OFFSET_Z_LSB_ADDR    0x04
#define BNO055_ACC_OFFSET_Z_MSB_ADDR    0x05

#define BNO055_MAG_OFFSET_X_LSB_ADDR    0x06
#define BNO055_MAG_OFFSET_X_MSB_ADDR    0x07
#define BNO055_MAG_OFFSET_Y_LSB_ADDR    0x08
#define BNO055_MAG_OFFSET_Y_MSB_ADDR    0x09
#define BNO055_MAG_OFFSET_Z_LSB_ADDR    0x0A
#define BNO055_MAG_OFFSET_Z_MSB_ADDR    0x0B

#define BNO055_GYRO_OFFSET_X_LSB_ADDR   0x0C
#define BNO055_GYRO_OFFSET_X_MSB_ADDR   0x0D
#define BNO055_GYRO_OFFSET_Y_LSB_ADDR   0x0E
#define BNO055_GYRO_OFFSET_Y_MSB_ADDR   0x0F
#define BNO055_GYRO_OFFSET_Z_LSB_ADDR   0x10
#define BNO055_GYRO_OFFSET_Z_MSB_ADDR   0x11

#define BNO055_ACCEL_RADIUS_LSB_ADDR    0x12
#define BNO055_ACCEL_RADIUS_MSB_ADDR    0x13
#define BNO055_MAG_RADIUS_LSB_ADDR      0x1A
#define BNO055_MAG_RADIUS_MSB_ADDR      0x1B

/* ===== OPERATION MODE ENUMERATION ===== */

typedef enum {
    BNO055_MODE_CONFIGMODE          = 0x0,      /**< Configuration mode */
    BNO055_MODE_ACCONLY             = 0x2,      /**< Accelerometer only */
    BNO055_MODE_MAGONLY             = 0x4,      /**< Magnetometer only */
    BNO055_MODE_ACCMAG              = 0x7,      /**< Accelerometer + Magnetometer */
    BNO055_MODE_ACCGYRO             = 0x8,      /**< Accelerometer + Gyroscope */
    BNO055_MODE_MAGGYRO             = 0xA,      /**< Magnetometer + Gyroscope */
    BNO055_MODE_AMG                 = 0xB,      /**< All three sensors (no fusion) */
    BNO055_MODE_IMU                 = 0xC,      /**< 6-DOF IMU (Accel + Gyro fusion) */
    BNO055_MODE_COMPASS             = 0xD,      /**< Compass mode (Accel + Mag fusion) */
    BNO055_MODE_M4G                 = 0xE,      /**< Magnetometer for Gyroscope */
    BNO055_MODE_NDOF_FMC_OFF        = 0xF,      /**< 9-DOF without fast mag calibration */
    BNO055_MODE_NDOF                = 0x1C,     /**< 9-DOF with fast mag calibration (standard) */
} bno055_operation_mode_t;

/* ===== POWER MODE ENUMERATION ===== */

typedef enum {
    BNO055_PWRMODE_NORMAL           = 0x0,      /**< Normal power mode */
    BNO055_PWRMODE_LOWPOWER         = 0x1,      /**< Low power mode */
    BNO055_PWRMODE_SUSPEND          = 0x2,      /**< Suspend mode */
} bno055_power_mode_t;

/* ===== DATA STRUCTURES ===== */

/**
 * @brief Raw quaternion data (16-bit signed integers)
 */
typedef struct {
    int16_t w;  /**< W component (real part) */
    int16_t x;  /**< X component (imaginary i) */
    int16_t y;  /**< Y component (imaginary j) */
    int16_t z;  /**< Z component (imaginary k) */
} __attribute__((packed)) bno055_quaternion_raw_t;

/**
 * @brief Floating-point quaternion (normalized)
 * Range: -1.0 to +1.0 for each component
 * Magnitude: sqrt(w^2 + x^2 + y^2 + z^2) = 1.0 (unit quaternion)
 */
typedef struct {
    float w;    /**< W component (real part), range [-1, 1] */
    float x;    /**< X component (imaginary i), range [-1, 1] */
    float y;    /**< Y component (imaginary j), range [-1, 1] */
    float z;    /**< Z component (imaginary k), range [-1, 1] */
} bno055_quaternion_t;

/**
 * @brief Euler angles in degrees
 * Convention: Z-Y-X (Heading-Roll-Pitch)
 */
typedef struct {
    float heading;  /**< Heading (rotation around Z-axis), range [-180, 180] or [0, 360] */
    float roll;     /**< Roll (rotation around X-axis), range [-180, 180] */
    float pitch;    /**< Pitch (rotation around Y-axis), range [-90, 90] */
} bno055_euler_angles_t;

/**
 * @brief Calibration status for each sensor
 */
typedef struct {
    uint8_t sys_calib;      /**< System calibration status (0-3) */
    uint8_t gyro_calib;     /**< Gyroscope calibration status (0-3) */
    uint8_t accel_calib;    /**< Accelerometer calibration status (0-3) */
    uint8_t mag_calib;      /**< Magnetometer calibration status (0-3) */
    bool is_stable;         /**< True if all sensors >= level 2 */
    bool is_fully_calibrated;   /**< True if all sensors at level 3 (may be unstable!) */
} bno055_calib_status_t;

/**
 * @brief Calibration offset data (for Page 1 registers)
 */
typedef struct {
    int16_t acc_offset_x;   /**< Accelerometer X offset */
    int16_t acc_offset_y;   /**< Accelerometer Y offset */
    int16_t acc_offset_z;   /**< Accelerometer Z offset */
    int16_t mag_offset_x;   /**< Magnetometer X offset */
    int16_t mag_offset_y;   /**< Magnetometer Y offset */
    int16_t mag_offset_z;   /**< Magnetometer Z offset */
    int16_t gyro_offset_x;  /**< Gyroscope X offset */
    int16_t gyro_offset_y;  /**< Gyroscope Y offset */
    int16_t gyro_offset_z;  /**< Gyroscope Z offset */
} bno055_calibration_offsets_t;

/**
 * @brief Driver configuration structure
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;    /**< I2C bus handle from i2c_new_master_bus() */
    uint8_t i2c_addr;                   /**< I2C slave address (0x28 or 0x29) */
    uint32_t i2c_clock_hz;              /**< I2C clock frequency (100k, 400k, etc.) */
    bool use_mutex;                     /**< Enable mutex protection for multi-task safety */
} bno055_config_t;

/**
 * @brief Driver handle (opaque to user)
 */
typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t i2c_addr;
    SemaphoreHandle_t mutex;
    bool initialized;
    bno055_operation_mode_t current_mode;
} bno055_driver_handle_t;

/* ===== FUNCTION DECLARATIONS ===== */

/**
 * @brief Initialize BNO055 driver
 *
 * @param[out] handle      Driver handle to be filled
 * @param[in]  config      Initialization configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Blocks for 650ms during initialization for device startup
 */
esp_err_t bno055_init(bno055_driver_handle_t *handle, const bno055_config_t *config);

/**
 * @brief Deinitialize driver and release resources
 *
 * @param[in] handle   Driver handle
 *
 * @return ESP_OK on success
 */
esp_err_t bno055_deinit(bno055_driver_handle_t *handle);

/**
 * @brief Set operation mode with validation and proper timing
 *
 * @param[in] handle   Driver handle
 * @param[in] mode     Target operation mode
 *
 * @return ESP_OK on success
 *
 * @note Automatically handles CONFIGMODE transitions
 * @note Inserts 25ms delays for firmware safety (exceeds datasheet minimums)
 * @note Validates mode transition validity
 */
esp_err_t bno055_set_operation_mode(bno055_driver_handle_t *handle,
                                   bno055_operation_mode_t mode);

/**
 * @brief Set power mode (must call from CONFIG mode)
 *
 * @param[in] handle   Driver handle
 * @param[in] pwrmode  Target power mode
 *
 * @return ESP_OK on success
 */
esp_err_t bno055_set_power_mode(bno055_driver_handle_t *handle,
                               bno055_power_mode_t pwrmode);

/**
 * @brief Read calibration status for all sensors
 *
 * @param[in]  handle   Driver handle
 * @param[out] status   Calibration status structure
 *
 * @return ESP_OK on success
 */
esp_err_t bno055_read_calib_status(bno055_driver_handle_t *handle,
                                  bno055_calib_status_t *status);

/**
 * @brief Read quaternion output (raw 16-bit values)
 *
 * @param[in]  handle   Driver handle
 * @param[out] quat     Raw quaternion data
 *
 * @return ESP_OK on success
 *
 * @note Requires mode with quaternion output (IMU, COMPASS, M4G, NDOF, etc.)
 * @note Values range from -16384 to +16383 (Q14 format)
 * @note Use bno055_quaternion_to_float() to normalize
 */
esp_err_t bno055_read_quaternion_raw(bno055_driver_handle_t *handle,
                                    bno055_quaternion_raw_t *quat);

/**
 * @brief Read quaternion output with consistency checking
 *
 * @param[in]  handle   Driver handle
 * @param[out] quat     Normalized floating-point quaternion (unit quaternion)
 *
 * @return ESP_OK on success
 *
 * @note Automatically detects mid-read sensor updates and retries
 * @note Normalizes output to unit quaternion (magnitude = 1.0)
 * @note Slower than raw read but guarantees data consistency
 */
esp_err_t bno055_read_quaternion(bno055_driver_handle_t *handle,
                                bno055_quaternion_t *quat);

/**
 * @brief Read Euler angles (not recommended due to firmware bugs)
 *
 * @param[in]  handle   Driver handle
 * @param[out] euler    Euler angles in degrees
 *
 * @return ESP_OK on success
 *
 * @warning Firmware bug: Accuracy degrades significantly > 20-30° from horizontal
 * @note Better approach: Use quaternions and convert locally if needed
 */
esp_err_t bno055_read_euler_angles(bno055_driver_handle_t *handle,
                                   bno055_euler_angles_t *euler);

/**
 * @brief Save current calibration offsets to structure
 *
 * @param[in]  handle   Driver handle
 * @param[out] offsets  Calibration offset structure
 *
 * @return ESP_OK on success
 */
esp_err_t bno055_save_calibration(bno055_driver_handle_t *handle,
                                 bno055_calibration_offsets_t *offsets);

/**
 * @brief Load previously saved calibration offsets
 *
 * @param[in] handle   Driver handle
 * @param[in] offsets  Calibration offset structure
 *
 * @return ESP_OK on success
 *
 * @note Must be in CONFIGMODE to load offsets
 * @note Automatic calibration may overwrite offsets in fusion modes
 */
esp_err_t bno055_load_calibration(bno055_driver_handle_t *handle,
                                 const bno055_calibration_offsets_t *offsets);

/**
 * @brief Wait for calibration to reach stable state
 *
 * @param[in] handle       Driver handle
 * @param[in] timeout_ms   Maximum wait time in milliseconds
 *
 * @return true if stable calibration achieved, false on timeout
 *
 * @note "Stable" = all sensors at level >= 2 (not fully at dangerous level 3)
 */
bool bno055_wait_calibration_stable(bno055_driver_handle_t *handle,
                                   uint32_t timeout_ms);

/**
 * @brief Check overall system health
 *
 * @param[in] handle   Driver handle
 *
 * @return true if system is healthy (running state, no errors)
 */
bool bno055_check_system_health(bno055_driver_handle_t *handle);

/**
 * @brief Perform soft reset (returns to CONFIGMODE)
 *
 * @param[in] handle   Driver handle
 *
 * @return ESP_OK on success
 *
 * @note Blocks for 650ms during reset sequence
 */
esp_err_t bno055_soft_reset(bno055_driver_handle_t *handle);

/**
 * @brief Convert raw quaternion to normalized floating-point
 *
 * @param[in]  raw     Raw quaternion from sensor
 * @param[out] quat    Normalized floating-point quaternion
 */
void bno055_quaternion_to_float(const bno055_quaternion_raw_t *raw,
                               bno055_quaternion_t *quat);

/**
 * @brief Normalize quaternion to unit magnitude
 *
 * @param[in,out] quat   Quaternion to normalize in-place
 *
 * @note Essential after quaternion operations to maintain precision
 */
void bno055_quaternion_normalize(bno055_quaternion_t *quat);

/**
 * @brief Convert quaternion to Euler angles (Z-Y-X convention)
 *
 * @param[in]  quat    Input quaternion
 * @param[out] euler   Output Euler angles in degrees
 *
 * @note Avoids gimbal lock issues inherent to Euler angles
 * @note More numerically stable than direct sensor Euler output
 */
void bno055_quaternion_to_euler(const bno055_quaternion_t *quat,
                               bno055_euler_angles_t *euler);

#ifdef __cplusplus
}
#endif

#endif // BNO055_DRIVER_H
