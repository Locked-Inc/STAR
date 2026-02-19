/* libs/rx_bno055/src/rx_bno055.c */

/**
 * @file rx_bno055.c
 * @brief Bosch BNO055 9-DOF IMU Driver Implementation
 *
 * @details
 * Implements initialization and data reading for the BNO055 IMU in
 * NDOF (Nine Degrees of Freedom) absolute orientation fusion mode.
 * All register access is via the SCI2 SIIC HAL (sci_iic_write_read).
 *
 * Scale factors for output conversion:
 * - Euler angles and gyroscope: 1 LSB = 1/16 degree
 *   → in radians: multiply by π / (16 × 180) = π / 2880
 * - Linear acceleration: 1 LSB = 0.01 m/s² (100 LSB per m/s²)
 *   → multiply by 0.01
 *
 * @see rx_bno055.h Public API header
 * @see rx_bno055_constants.h Register addresses and constants
 * @see rx_sci_iic.h SCI SIIC bus driver
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdbool.h>
#include <stdint.h>

#include "rx_bno055.h"
#include "rx_bno055_constants.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_sci_iic.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Log tag for this module */
static const char* const s_tag = "BNO055";

/**
 * @brief Scale factor: Euler/gyro raw integer → radians
 * @details 1 LSB = 1/16 degree. 1 degree = π/180 radians.
 *          Combined: 1 LSB = π/(16×180) = π/2880 radians ≈ 0.00109083 rad
 */
static const float s_angle_scale = 0.00109083f;

/**
 * @brief Scale factor: accelerometer raw integer → m/s²
 * @details 1 LSB = 0.01 m/s² in BNO055 NDOF fusion output (linear acceleration)
 */
static const float s_accel_scale = 0.01f;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @var s_config
 * @brief Saved configuration from last successful rx_bno055_init() call
 *
 * @note Updated only on successful init. Channel/address stored for reads.
 * @warning Do not access before rx_bno055_init() returns k_rx_ok.
 *
 * @since 1.0.0
 */
static rx_bno055_config_t s_config;

/**
 * @var s_initialized
 * @brief True if rx_bno055_init() has completed successfully
 *
 * @note Guards rx_bno055_read() against use before init.
 *
 * @since 1.0.0
 */
static bool s_initialized = false;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BNO055 register
 *
 * @param[in] reg   Register address to write
 * @param[in] value Byte value to write
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         Write successful
 * @retval k_rx_err_nack     NACK received
 * @retval k_rx_err_timeout Polling timeout
 *
 * @pre sci_iic_init() has been called for s_config.channel
 * @post Register reg contains value on k_rx_ok
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t buf[2];

  buf[0] = reg;
  buf[1] = value;
  return sci_iic_write(s_config.channel, s_config.i2c_addr, buf, 2);
}

/**
 * @brief Read one or more bytes from a BNO055 register address
 *
 * @param[in]  reg  Starting register address
 * @param[out] data Buffer to store read bytes
 * @param[in]  len  Number of bytes to read
 *
 * @return rx_err_t Status
 * @retval k_rx_ok         Read successful
 * @retval k_rx_err_nack     NACK received
 * @retval k_rx_err_timeout Polling timeout
 *
 * @pre data != NULL, len > 0
 * @pre sci_iic_init() has been called for s_config.channel
 * @post data[0..len-1] filled with register bytes on k_rx_ok
 *
 * @note Not thread-safe.
 *
 * @since 1.0.0
 */
static rx_err_t internal_read_reg(uint8_t reg, uint8_t* data, uint8_t len)
{
  return sci_iic_write_read(s_config.channel, s_config.i2c_addr, &reg, 1, data, len);
}

/**
 * @brief Combine LSB and MSB bytes into a signed 16-bit integer
 *
 * @param[in] lsb Low byte (bits 7:0)
 * @param[in] msb High byte (bits 15:8)
 *
 * @return Signed 16-bit value in two's complement representation
 *
 * @pre No preconditions (pure function)
 * @post Return value = (int16_t)((msb << 8) | lsb)
 *
 * @note Thread-safe (pure function, no shared state).
 *
 * @since 1.0.0
 */
static int16_t internal_combine_bytes(uint8_t lsb, uint8_t msb)
{
  return (int16_t)((uint16_t)msb << k_bno055_byte_shift | (uint16_t)lsb);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BNO055 and enter NDOF fusion mode
 *
 * @details Full implementation. See rx_bno055.h for API contract.
 *
 * @param[in] config BNO055 configuration
 *
 * @return rx_err_t Status
 *
 * @pre config != NULL
 * @pre sci_iic_init(config->channel) called
 * @post s_initialized = true on success; NDOF mode active
 *
 * @since 1.0.0
 */
rx_err_t rx_bno055_init(const rx_bno055_config_t* config)
{
  uint8_t  chip_id = 0;
  rx_err_t err;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  s_config = *config;

  /* 1. Verify CHIP_ID — confirms device is present and powered */
  err = internal_read_reg(k_bno055_chip_id_reg, &chip_id, k_bno055_chip_id_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "CHIP_ID read failed — check power and I2C wiring");
    return err;
  }
  if (chip_id != k_bno055_chip_id_value) {
    rx_log_error(s_tag, "CHIP_ID mismatch — not a BNO055");
    return k_rx_err_not_found;
  }

  /* 2. Enter CONFIG mode before changing any settings */
  err = internal_write_reg(k_bno055_opr_mode_reg, k_bno055_opr_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to enter CONFIG mode");
    return err;
  }
  (void)tx_thread_sleep(k_bno055_config_mode_delay_ms);

  /* 3. Set normal power mode */
  err = internal_write_reg(k_bno055_pwr_mode_reg, k_bno055_pwr_normal);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set PWR_MODE normal");
    return err;
  }

  /* 4. Ensure register page 0 is selected */
  err = internal_write_reg(k_bno055_page_id_reg, k_bno055_page_0);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to select register page 0");
    return err;
  }

  /* 5. Enter NDOF fusion mode */
  err = internal_write_reg(k_bno055_opr_mode_reg, k_bno055_opr_ndof);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to enter NDOF mode");
    return err;
  }
  (void)tx_thread_sleep(k_bno055_fusion_mode_delay_ms);

  s_initialized = true;
  rx_log_info(s_tag, "BNO055 initialized, NDOF mode active");
  return k_rx_ok;
}

/**
 * @brief Read Euler angles, accelerometer, gyroscope, and calibration status
 *
 * @details Full implementation. See rx_bno055.h for API contract.
 *
 * @param[out] data Destination for sensor readings
 *
 * @return rx_err_t Status
 *
 * @pre rx_bno055_init() returned k_rx_ok
 * @pre data != NULL
 * @post data filled with current sensor readings on k_rx_ok
 *
 * @since 1.0.0
 */
rx_err_t rx_bno055_read(rx_bno055_data_t* data)
{
  uint8_t euler_buf[k_bno055_vector_bytes];
  uint8_t accel_buf[k_bno055_vector_bytes];
  uint8_t gyro_buf[k_bno055_vector_bytes];
  uint8_t calib_buf[k_bno055_calib_bytes];

  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Read Euler angles: heading, roll, pitch (6 bytes from 0x1A) */
  rx_err_t err = internal_read_reg(k_bno055_euler_h_lsb, euler_buf, k_bno055_vector_bytes);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read accelerometer: X, Y, Z (6 bytes from 0x08) */
  err = internal_read_reg(k_bno055_accel_x_lsb, accel_buf, k_bno055_vector_bytes);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read gyroscope: X, Y, Z (6 bytes from 0x14) */
  err = internal_read_reg(k_bno055_gyro_x_lsb, gyro_buf, k_bno055_vector_bytes);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read calibration status (1 byte from 0x35) */
  err = internal_read_reg(k_bno055_calib_stat, calib_buf, k_bno055_calib_bytes);
  if (err != k_rx_ok) {
    return err;
  }

  /* Convert Euler angles from raw 1/16-degree units to radians */
  data->heading_rad = (float)internal_combine_bytes(euler_buf[k_bno055_idx_x_lsb],
                                                    euler_buf[k_bno055_idx_x_msb]) *
                      s_angle_scale;
  data->roll_rad    = (float)internal_combine_bytes(euler_buf[k_bno055_idx_y_lsb],
                                                    euler_buf[k_bno055_idx_y_msb]) *
                      s_angle_scale;
  data->pitch_rad   = (float)internal_combine_bytes(euler_buf[k_bno055_idx_z_lsb],
                                                    euler_buf[k_bno055_idx_z_msb]) *
                      s_angle_scale;

  /* Convert accelerometer from 1/100 m/s² units to m/s² */
  data->accel_x_mps2 = (float)internal_combine_bytes(accel_buf[k_bno055_idx_x_lsb],
                                                      accel_buf[k_bno055_idx_x_msb]) *
                       s_accel_scale;
  data->accel_y_mps2 = (float)internal_combine_bytes(accel_buf[k_bno055_idx_y_lsb],
                                                      accel_buf[k_bno055_idx_y_msb]) *
                       s_accel_scale;
  data->accel_z_mps2 = (float)internal_combine_bytes(accel_buf[k_bno055_idx_z_lsb],
                                                      accel_buf[k_bno055_idx_z_msb]) *
                       s_accel_scale;

  /* Convert gyroscope from 1/16-degrees/s units to rad/s */
  data->gyro_x_rad_s = (float)internal_combine_bytes(gyro_buf[k_bno055_idx_x_lsb],
                                                      gyro_buf[k_bno055_idx_x_msb]) *
                       s_angle_scale;
  data->gyro_y_rad_s = (float)internal_combine_bytes(gyro_buf[k_bno055_idx_y_lsb],
                                                      gyro_buf[k_bno055_idx_y_msb]) *
                       s_angle_scale;
  data->gyro_z_rad_s = (float)internal_combine_bytes(gyro_buf[k_bno055_idx_z_lsb],
                                                      gyro_buf[k_bno055_idx_z_msb]) *
                       s_angle_scale;

  data->calib_status = calib_buf[0];

  return k_rx_ok;
}
