/* libs/rx_bno055/inc/rx_bno055.h */

/**
 * @file rx_bno055.h
 * @brief Bosch BNO055 9-DOF IMU Driver API
 *
 * @details
 * High-level driver for the Bosch BNO055 9-axis Absolute Orientation Sensor.
 * The BNO055 combines a 3-axis accelerometer, 3-axis gyroscope, and 3-axis
 * magnetometer with an ARM Cortex-M0 processor running Bosch's sensor fusion
 * algorithm on-chip.
 *
 * **Key Features Used:**
 * - NDOF (Nine Degrees of Freedom) absolute orientation mode
 * - Euler angle output: heading (yaw), roll, pitch in radians
 * - Linear acceleration output in m/s² (gravity compensated)
 * - Angular velocity (gyroscope) output in rad/s
 * - Calibration status monitoring
 *
 * **Interface:**
 * - Protocol: I2C via SCI2 SIIC (Simple IIC) on P52/P50
 * - Address: 0x28 (default, COM3=GND); 0x29 (COM3=VDDIO)
 * - Speed: 400 kHz fast mode
 *
 * **Initialization Timing:**
 * The BNO055 requires 650ms after power-on before it can be communicated with.
 * The caller (imu_task) must ensure this delay has passed before calling
 * rx_bno055_init(). Use tx_thread_sleep(650) in the task entry function.
 *
 * **NDOF Calibration:**
 * After entering NDOF mode, the sensors self-calibrate over time:
 * - Gyroscope: place stationary for a few seconds
 * - Accelerometer: place at several known orientations
 * - Magnetometer: perform figure-8 motion in the air
 * Calibration persists in BNO055 internal flash but resets on power cycle.
 * Check calib_status field to monitor calibration progress.
 *
 * @par Thread Safety
 * NOT thread-safe. The IMU task (imu_task.c) is the sole caller and provides
 * implicit serialization by accessing this driver from a single task context.
 *
 * @see rx_bno055_constants.h Register addresses and constants
 * @see rx_sci_iic.h SCI SIIC bus driver (underlying transport)
 * @see imu_task.c Consumer of this driver
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @struct rx_bno055_config_t
 * @brief Configuration for BNO055 driver initialization
 *
 * @details
 * Specifies which SCI SIIC channel and I2C address to use for the BNO055.
 * The underlying SIIC channel must already be initialized via sci_iic_init()
 * before calling rx_bno055_init().
 *
 * @invariant i2c_addr must be k_bno055_addr_default (0x28) or k_bno055_addr_alt (0x29)
 *
 * @code
 * rx_bno055_config_t config = {
 *     .channel  = k_imu_iic_channel,  // SCI2
 *     .i2c_addr = 0x28,               // COM3 tied to GND
 * };
 * @endcode
 *
 * @since 1.0.0
 */
typedef struct {
  uint8_t channel;  /**< SCI SIIC channel number (e.g., 2 for SCI2) */
  uint8_t i2c_addr; /**< 7-bit I2C address: 0x28 (default) or 0x29 (alt) */
} rx_bno055_config_t;

/**
 * @struct rx_bno055_data_t
 * @brief Complete IMU data from one BNO055 read cycle
 *
 * @details
 * All angles are in radians (SI units), acceleration in m/s², angular
 * velocity in rad/s. Fusion output is in absolute orientation (NDOF mode),
 * referenced to magnetic North.
 *
 * @invariant All float fields are valid when rx_bno055_read() returns k_rx_ok
 * @invariant calib_status: 0x00 = all uncalibrated, 0xFF = all fully calibrated
 *
 * @code
 * rx_bno055_data_t imu;
 * if (rx_bno055_read(&imu) == k_rx_ok) {
 *     // heading is yaw (0 to 2π), roll is ±π, pitch is ±π/2
 *     float yaw_deg = imu.heading_rad * 180.0f / 3.14159265f;
 * }
 * @endcode
 *
 * @since 1.0.0
 */
typedef struct {
  float   heading_rad;   /**< Euler heading (yaw) in radians. Range: 0 to 2π (North reference) */
  float   roll_rad;      /**< Euler roll in radians. Range: -π to π */
  float   pitch_rad;     /**< Euler pitch in radians. Range: -π/2 to π/2 */
  float   accel_x_mps2;  /**< Linear acceleration X in m/s² (gravity removed by BNO055 fusion) */
  float   accel_y_mps2;  /**< Linear acceleration Y in m/s² */
  float   accel_z_mps2;  /**< Linear acceleration Z in m/s² */
  float   gyro_x_rad_s;  /**< Angular velocity X (roll rate) in rad/s */
  float   gyro_y_rad_s;  /**< Angular velocity Y (pitch rate) in rad/s */
  float   gyro_z_rad_s;  /**< Angular velocity Z (yaw rate) in rad/s */
  uint8_t calib_status;  /**< Raw CALIB_STAT byte (0x35). Use bno055_calib_mask_t to decode. */
} rx_bno055_data_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BNO055 and enter NDOF fusion mode
 *
 * @details
 * Performs the full BNO055 initialization sequence:
 * 1. Verify CHIP_ID register (0x00) reads 0xA0
 * 2. Enter CONFIG mode (OPR_MODE=0x00), wait 25 ms
 * 3. Set normal power mode (PWR_MODE=0x00)
 * 4. Ensure page 0 selected (PAGE_ID=0x00)
 * 5. Enter NDOF fusion mode (OPR_MODE=0x0C), wait 10 ms
 *
 * @warning The caller must wait at least 650 ms after BNO055 power-on before
 * calling this function. Use tx_thread_sleep(650) at task startup.
 *
 * @param[in] config BNO055 configuration (channel + I2C address)
 *
 * @return rx_err_t Status
 * @retval k_rx_ok              Initialization successful; NDOF mode active
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_not_found   CHIP_ID read failed or returned wrong value
 * @retval k_rx_err_timeout     I2C communication timeout during init
 * @retval k_rx_err_nack          NACK from BNO055 (device not responding)
 *
 * @pre config != NULL
 * @pre sci_iic_init(config->channel) has been called successfully
 * @pre At least 650 ms has elapsed since BNO055 power-on
 * @post BNO055 in NDOF mode; rx_bno055_read() is now safe to call
 * @post s_config stores the provided config for subsequent reads
 *
 * @note Not thread-safe. Call once during IMU task startup.
 * @warning Do not call before 650 ms power-on delay (returns k_rx_err_not_found)
 *
 * @see rx_bno055_read() Read sensor data after successful init
 *
 * @since 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 3 preconditions, 2 postconditions
 */
[[nodiscard]] rx_err_t rx_bno055_init(const rx_bno055_config_t* config);

/**
 * @brief Read Euler angles, accelerometer, gyroscope, and calibration status
 *
 * @details
 * Performs three I2C burst reads:
 * 1. Euler angles: 6 bytes from register 0x1A (H_LSB, H_MSB, R_LSB, R_MSB, P_LSB, P_MSB)
 * 2. Accelerometer: 6 bytes from register 0x08 (X_LSB..Z_MSB)
 * 3. Gyroscope: 6 bytes from register 0x14 (X_LSB..Z_MSB)
 * 4. Calibration: 1 byte from register 0x35
 *
 * All raw 16-bit signed integers are converted to floats with proper scaling:
 * - Euler: 1 LSB = 1/16 degree → multiply by (π/2880) to get radians
 * - Accel: 1 LSB = 0.01 m/s²
 * - Gyro: 1 LSB = 1/16 degree/s → multiply by (π/2880) to get rad/s
 *
 * @param[out] data Pointer to structure to fill with sensor data
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         All data read and converted successfully
 * @retval k_rx_err_null_ptr data is NULL
 * @retval k_rx_err_invalid_state rx_bno055_init() has not been called
 * @retval k_rx_err_timeout I2C polling timeout
 * @retval k_rx_err_nack      NACK from BNO055
 *
 * @pre rx_bno055_init() returned k_rx_ok
 * @pre data != NULL
 * @post data->heading_rad, roll_rad, pitch_rad, accel_*, gyro_* filled on k_rx_ok
 * @post data->calib_status filled on k_rx_ok
 *
 * @note Not thread-safe.
 * @note Execution time: ~4 I2C transactions × ~0.25ms each ≈ 1ms at 400kHz
 *
 * @see rx_bno055_init() Must be called first
 * @see bno055_calib_mask_t Decode calib_status field
 *
 * @since 1.0.0
 */
[[nodiscard]] rx_err_t rx_bno055_read(rx_bno055_data_t* data);

#ifdef __cplusplus
}
#endif
