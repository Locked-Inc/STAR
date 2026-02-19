/* libs/rx_bno055/inc/rx_bno055_constants.h */

/**
 * @file rx_bno055_constants.h
 * @brief BNO055 9-DOF IMU Register Addresses, Mode Constants, and Scale Factors
 *
 * @details
 * Defines all register addresses, operational mode codes, calibration
 * status masks, and scale factors for the Bosch BNO055 9-axis IMU.
 * The BNO055 combines accelerometer, gyroscope, and magnetometer with
 * an integrated sensor fusion processor running NDOF (Nine Degrees Of
 * Freedom) algorithm on-chip.
 *
 * @par Register Map (Page 0 — selected by PAGE_ID=0x00)
 * All data registers used by this driver are on Register Page 0.
 * Page 1 contains configuration registers not needed at runtime.
 *
 * @par Data Output Registers (fusion mode output)
 * | Address | Register | Description |
 * |---------|----------|-------------|
 * | 0x08 | ACC_Data_X_LSB | Accelerometer X axis, LSB |
 * | 0x09 | ACC_Data_X_MSB | Accelerometer X axis, MSB |
 * | 0x0A | ACC_Data_Y_LSB | Accelerometer Y axis, LSB |
 * | 0x0B | ACC_Data_Y_MSB | Accelerometer Y axis, MSB |
 * | 0x0C | ACC_Data_Z_LSB | Accelerometer Z axis, LSB |
 * | 0x0D | ACC_Data_Z_MSB | Accelerometer Z axis, MSB |
 * | 0x14 | GYR_Data_X_LSB | Gyroscope X axis, LSB |
 * | 0x15 | GYR_Data_X_MSB | Gyroscope X axis, MSB |
 * | 0x16 | GYR_Data_Y_LSB | Gyroscope Y axis, LSB |
 * | 0x17 | GYR_Data_Y_MSB | Gyroscope Y axis, MSB |
 * | 0x18 | GYR_Data_Z_LSB | Gyroscope Z axis, LSB |
 * | 0x19 | GYR_Data_Z_MSB | Gyroscope Z axis, MSB |
 * | 0x1A | EUL_Heading_LSB | Euler heading (yaw) angle, LSB |
 * | 0x1B | EUL_Heading_MSB | Euler heading (yaw) angle, MSB |
 * | 0x1C | EUL_Roll_LSB | Euler roll angle, LSB |
 * | 0x1D | EUL_Roll_MSB | Euler roll angle, MSB |
 * | 0x1E | EUL_Pitch_LSB | Euler pitch angle, LSB |
 * | 0x1F | EUL_Pitch_MSB | Euler pitch angle, MSB |
 * | 0x35 | CALIB_STAT | Calibration status (sys/gyro/accel/mag) |
 *
 * @see BNO055 Data Sheet (Bosch BST-BNO055-DS000-14)
 * @see rx_bno055.h Public driver API
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdint.h>

/* =============================================================================
 * I2C Address
 * =============================================================================
 */

/**
 * @enum bno055_addr_t
 * @brief BNO055 I2C device address options
 *
 * @details
 * The 7-bit I2C address is determined by the COM3 pin state:
 * - COM3 tied to GND (default): address = 0x28
 * - COM3 tied to VDDIO: address = 0x29
 * STAR Project: COM3 is tied to GND on the schematic.
 *
 * @see BNO055 Data Sheet Table 4-7 (I2C address selection)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_addr_default = 0x28, /**< COM3=GND: default STAR hardware address */
  k_bno055_addr_alt     = 0x29, /**< COM3=VDDIO: alternate address */
} bno055_addr_t;

/* =============================================================================
 * Chip ID
 * =============================================================================
 */

/**
 * @enum bno055_chip_id_t
 * @brief BNO055 chip identification constants
 *
 * @details
 * Used to verify communication with the correct device during init.
 * CHIP_ID register (0x00) always reads 0xA0 on genuine BNO055.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_chip_id_reg   = 0x00, /**< Register address of CHIP_ID */
  k_bno055_chip_id_value = 0xA0, /**< Expected CHIP_ID value for BNO055 */
} bno055_chip_id_t;

/* =============================================================================
 * System Registers
 * =============================================================================
 */

/**
 * @enum bno055_sys_regs_t
 * @brief BNO055 system configuration register addresses
 *
 * @details
 * System registers used during initialization to set operating mode,
 * power mode, and register page. All on page 0.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_page_id_reg  = 0x07, /**< PAGE_ID: register page select (0 or 1) */
  k_bno055_opr_mode_reg = 0x3D, /**< OPR_MODE: operating mode register */
  k_bno055_pwr_mode_reg = 0x3E, /**< PWR_MODE: power mode register */
  k_bno055_sys_trigger  = 0x3F, /**< SYS_TRIGGER: system trigger (reset, clk_sel) */
  k_bno055_calib_stat   = 0x35, /**< CALIB_STAT: calibration status register */
} bno055_sys_regs_t;

/* =============================================================================
 * Operating Modes
 * =============================================================================
 */

/**
 * @enum bno055_opr_mode_t
 * @brief BNO055 operating mode values for OPR_MODE register (0x3D)
 *
 * @details
 * The BNO055 has multiple operating modes. This driver uses:
 * - CONFIG mode: Required before changing any configuration
 * - NDOF mode: Full 9-DOF sensor fusion (accelerometer + gyro + magnetometer)
 *
 * Mode switch timing requirements:
 * - Any mode → CONFIG: 7 ms
 * - CONFIG → Any fusion mode: 7 ms
 * - CONFIG → NDOF: 7 ms + internal calibration startup (~650 ms from power-on)
 *
 * @see BNO055 Data Sheet Table 3-3 (Operating modes)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_opr_config = 0x00, /**< CONFIG mode: all sensors suspended, safe to configure */
  k_bno055_opr_ndof   = 0x0C, /**< NDOF mode: full 9-DOF absolute orientation fusion */
} bno055_opr_mode_t;

/* =============================================================================
 * Power Modes
 * =============================================================================
 */

/**
 * @enum bno055_pwr_mode_t
 * @brief BNO055 power mode values for PWR_MODE register (0x3E)
 *
 * @details
 * Normal power mode keeps all sensors active continuously.
 * This is required for NDOF fusion mode operation.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_pwr_normal  = 0x00, /**< Normal: all sensors active, required for NDOF */
  k_bno055_pwr_low     = 0x01, /**< Low power: only accelerometer active */
  k_bno055_pwr_suspend = 0x02, /**< Suspend: all sensors off, SCI active */
} bno055_pwr_mode_t;

/* =============================================================================
 * Page Select
 * =============================================================================
 */

/**
 * @enum bno055_page_t
 * @brief BNO055 register page select values for PAGE_ID register (0x07)
 *
 * @details
 * BNO055 has two register pages. All data registers are on page 0.
 * After power-on or reset, page 0 is selected by default. This driver
 * always ensures page 0 is selected during init.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_page_0 = 0x00, /**< Page 0: data and status registers (default) */
  k_bno055_page_1 = 0x01, /**< Page 1: configuration registers */
} bno055_page_t;

/* =============================================================================
 * Data Registers (Page 0)
 * =============================================================================
 */

/**
 * @enum bno055_data_regs_t
 * @brief BNO055 sensor data register addresses (page 0)
 *
 * @details
 * All sensor output registers on page 0. Data is 16-bit two's complement,
 * stored in little-endian byte order (LSB at lower address, MSB at higher).
 * Multiple consecutive registers can be read in one I2C burst transaction.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_accel_x_lsb = 0x08, /**< ACC_Data_X_LSB: accelerometer X axis (read 6 bytes for X,Y,Z) */
  k_bno055_gyro_x_lsb  = 0x14, /**< GYR_Data_X_LSB: gyroscope X axis (read 6 bytes for X,Y,Z) */
  k_bno055_euler_h_lsb = 0x1A, /**< EUL_Heading_LSB: Euler heading/yaw (read 6 bytes for H,R,P) */
} bno055_data_regs_t;

/* =============================================================================
 * Data Read Lengths
 * =============================================================================
 */

/**
 * @enum bno055_read_len_t
 * @brief Byte counts for burst reads of BNO055 sensor data
 *
 * @details
 * All BNO055 vector data is 3 axes × 2 bytes = 6 bytes per sensor.
 * Burst reads capture all axes in a single I2C transaction.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_vector_bytes  = 6, /**< Bytes for a 3-axis vector (2 bytes per axis) */
  k_bno055_calib_bytes   = 1, /**< Bytes for calibration status register */
  k_bno055_chip_id_bytes = 1, /**< Bytes for chip ID register */
} bno055_read_len_t;

/* =============================================================================
 * Byte Indices Within Vector Reads
 * =============================================================================
 */

/**
 * @enum bno055_vec_idx_t
 * @brief Byte array indices for parsing 6-byte vector reads
 *
 * @details
 * BNO055 stores vectors as: [X_LSB, X_MSB, Y_LSB, Y_MSB, Z_LSB, Z_MSB].
 * Each axis is a 16-bit two's complement value (little-endian).
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_idx_x_lsb = 0, /**< buf[0]: X axis low byte */
  k_bno055_idx_x_msb = 1, /**< buf[1]: X axis high byte */
  k_bno055_idx_y_lsb = 2, /**< buf[2]: Y axis low byte */
  k_bno055_idx_y_msb = 3, /**< buf[3]: Y axis high byte */
  k_bno055_idx_z_lsb = 4, /**< buf[4]: Z axis low byte */
  k_bno055_idx_z_msb = 5, /**< buf[5]: Z axis high byte */
} bno055_vec_idx_t;

/* =============================================================================
 * Calibration Status Masks
 * =============================================================================
 */

/**
 * @enum bno055_calib_mask_t
 * @brief Bit masks for CALIB_STAT register (0x35)
 *
 * @details
 * Each 2-bit field encodes calibration level: 0=uncalibrated, 3=fully calibrated.
 * SYS calibration requires all three sensors to be individually calibrated first.
 *
 * | Bits | Sensor | Fully calibrated value |
 * |------|--------|------------------------|
 * | [7:6] | SYS   | 0xC0 |
 * | [5:4] | GYR   | 0x30 |
 * | [3:2] | ACC   | 0x0C |
 * | [1:0] | MAG   | 0x03 |
 *
 * @see BNO055 Data Sheet Section 3.10 (Calibration)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_calib_sys_mask = 0xC0, /**< SYS calibration bits [7:6]: 0xC0 = fully calibrated */
  k_bno055_calib_gyr_mask = 0x30, /**< GYR calibration bits [5:4]: 0x30 = fully calibrated */
  k_bno055_calib_acc_mask = 0x0C, /**< ACC calibration bits [3:2]: 0x0C = fully calibrated */
  k_bno055_calib_mag_mask = 0x03, /**< MAG calibration bits [1:0]: 0x03 = fully calibrated */
  k_bno055_calib_all_full = 0xFF, /**< All sensors fully calibrated (SYS+GYR+ACC+MAG=3) */
} bno055_calib_mask_t;

/* =============================================================================
 * Timing Constants
 * =============================================================================
 */

/**
 * @enum bno055_timing_t
 * @brief Timing delays for BNO055 mode switches and initialization
 *
 * @details
 * All delays in ThreadX ticks (1 tick = 1 ms at STAR firmware configuration).
 * The 650ms power-on delay should be handled by the system before calling
 * rx_bno055_init() — typically by the app_main_task startup sequence.
 *
 * @since 1.0.0
 */
typedef enum : uint16_t {
  k_bno055_config_mode_delay_ms = 25, /**< Delay after entering CONFIG mode (ms) */
  k_bno055_fusion_mode_delay_ms = 10, /**< Delay after entering fusion mode (ms) */
  k_bno055_power_on_delay_ms    = 650, /**< Power-on stabilization delay (ms); caller must wait */
} bno055_timing_t;

/* =============================================================================
 * Bit Manipulation
 * =============================================================================
 */

/**
 * @enum bno055_bitops_t
 * @brief Constants for combining LSB/MSB bytes into 16-bit signed values
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_byte_shift = 8, /**< Left shift count to position MSB in a 16-bit word */
} bno055_bitops_t;
