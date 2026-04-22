/**
 * @file rx_bno055_regs.h
 * @brief BNO055 9-DOF Absolute Orientation Sensor Register Map and Configuration Enumerations
 *
 * @details
 * # Overview
 *
 * This file provides all register address definitions and configuration constants for the
 * Bosch BNO055 9-DOF absolute orientation sensor. All constants are defined as C23 typed
 * enumerations following STAR project conventions with zero magic numbers.
 *
 * The BNO055 integrates a triaxial accelerometer, triaxial gyroscope, and triaxial
 * geomagnetic sensor with an on-chip ARM Cortex-M0 that performs sensor fusion to
 * produce absolute orientation quaternions and Euler angles in hardware.
 *
 * # Register Map Summary
 *
 * | Address Range | Contents |
 * |---------------|----------|
 * | 0x00 | Chip ID (0xA0) |
 * | 0x08-0x0D | Accelerometer XYZ (raw) |
 * | 0x14-0x19 | Gyroscope XYZ (raw) |
 * | 0x1A-0x1F | Euler heading/roll/pitch |
 * | 0x20-0x27 | Quaternion W/X/Y/Z |
 * | 0x28-0x2D | Linear acceleration XYZ |
 * | 0x2E-0x33 | Gravity vector XYZ |
 * | 0x34 | Temperature |
 * | 0x35 | Calibration status |
 * | 0x3B | Unit selection |
 * | 0x3D | Operating mode |
 * | 0x3E | Power mode |
 * | 0x3F | System trigger |
 * | 0x41-0x42 | Axis map config/sign |
 *
 * # DFRobot Module Hardware
 *
 * - I2C Address: 0x28 (COM3/ADR pulled LOW)
 * - Connected to RIIC1 (SCL1=P2.1, SDA1=P2.0)
 * - Power: 3.3V via J5 connector
 * - Reset: Active-low via P8.3
 *
 * # Scale Factors
 *
 * | Output | Scale | Unit |
 * |--------|-------|------|
 * | Euler angles | 1/16 deg/LSB | degrees |
 * | Quaternion | 1/16384/LSB | unit quaternion |
 * | Linear accel | 1/100 m/s^2/LSB | m/s^2 |
 * | Gyroscope | 1/16 dps/LSB | deg/s |
 *
 * @see rx_bno055.h Public driver API
 * @see rx_bno055.c Driver implementation
 * @see BNO055 datasheet section 3.6 for complete register descriptions
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 3:** [PASS] No dynamic allocation (constants only)
 * - **Rule 8:** [PASS] C23 typed enums for all constants, no macros
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Register Address Map
 * =============================================================================
 */

/**
 * @enum bno055_reg_t
 * @brief BNO055 register addresses (page 0)
 *
 * @details
 * Complete register address map for the BNO055 sensor. All register
 * addresses reference page 0 (default page after reset).
 *
 * The sensor organizes its output registers as follows:
 * - Raw sensor data: accelerometer (0x08-0x0D), gyroscope (0x14-0x19)
 * - Fusion output: Euler (0x1A-0x1F), quaternion (0x20-0x27),
 *   linear accel (0x28-0x2D), gravity (0x2E-0x33)
 * - System: temperature (0x34), calibration (0x35)
 * - Configuration: unit select (0x3B), operating mode (0x3D),
 *   power mode (0x3E), sys trigger (0x3F), axis map (0x41-0x42)
 *
 * @invariant All values are 8-bit I2C register addresses
 * @invariant k_bno055_reg_chip_id reads back k_bno055_chip_id_expected on power-up
 *
 * @note Enum constants use the STAR project k_ prefix convention for typed enum
 *       constants (as opposed to SCREAMING_SNAKE_CASE which applies to #define macros).
 *       This is intentional per STAR coding standards (CLAUDE.md).
 *
 * @see bno055_chip_id_t Expected chip ID value
 * @see BNO055 datasheet table 4-2 for complete page 0 register map
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_reg_chip_id    = 0x00, /**< Chip ID register - reads 0xA0 */
  k_bno055_reg_acc_x_lsb  = 0x08, /**< Raw accel X low byte */
  k_bno055_reg_acc_x_msb  = 0x09, /**< Raw accel X high byte */
  k_bno055_reg_acc_y_lsb  = 0x0A, /**< Raw accel Y low byte */
  k_bno055_reg_acc_y_msb  = 0x0B, /**< Raw accel Y high byte */
  k_bno055_reg_acc_z_lsb  = 0x0C, /**< Raw accel Z low byte */
  k_bno055_reg_acc_z_msb  = 0x0D, /**< Raw accel Z high byte */
  k_bno055_reg_gyr_x_lsb  = 0x14, /**< Raw gyro X low byte */
  k_bno055_reg_gyr_x_msb  = 0x15, /**< Raw gyro X high byte */
  k_bno055_reg_gyr_y_lsb  = 0x16, /**< Raw gyro Y low byte */
  k_bno055_reg_gyr_y_msb  = 0x17, /**< Raw gyro Y high byte */
  k_bno055_reg_gyr_z_lsb  = 0x18, /**< Raw gyro Z low byte */
  k_bno055_reg_gyr_z_msb  = 0x19, /**< Raw gyro Z high byte */
  k_bno055_reg_eul_h_lsb  = 0x1A, /**< Euler heading low byte (0-359.9375 deg) */
  k_bno055_reg_eul_h_msb  = 0x1B, /**< Euler heading high byte */
  k_bno055_reg_eul_r_lsb  = 0x1C, /**< Euler roll low byte (-90 to +90 deg) */
  k_bno055_reg_eul_r_msb  = 0x1D, /**< Euler roll high byte */
  k_bno055_reg_eul_p_lsb  = 0x1E, /**< Euler pitch low byte (-180 to +180 deg) */
  k_bno055_reg_eul_p_msb  = 0x1F, /**< Euler pitch high byte */
  k_bno055_reg_qua_w_lsb  = 0x20, /**< Quaternion W low byte */
  k_bno055_reg_qua_w_msb  = 0x21, /**< Quaternion W high byte */
  k_bno055_reg_qua_x_lsb  = 0x22, /**< Quaternion X low byte */
  k_bno055_reg_qua_x_msb  = 0x23, /**< Quaternion X high byte */
  k_bno055_reg_qua_y_lsb  = 0x24, /**< Quaternion Y low byte */
  k_bno055_reg_qua_y_msb  = 0x25, /**< Quaternion Y high byte */
  k_bno055_reg_qua_z_lsb  = 0x26, /**< Quaternion Z low byte */
  k_bno055_reg_qua_z_msb  = 0x27, /**< Quaternion Z high byte */
  k_bno055_reg_lia_x_lsb  = 0x28, /**< Linear accel X low byte (m/s^2, gravity-free) */
  k_bno055_reg_lia_x_msb  = 0x29, /**< Linear accel X high byte */
  k_bno055_reg_lia_y_lsb  = 0x2A, /**< Linear accel Y low byte */
  k_bno055_reg_lia_y_msb  = 0x2B, /**< Linear accel Y high byte */
  k_bno055_reg_lia_z_lsb  = 0x2C, /**< Linear accel Z low byte */
  k_bno055_reg_lia_z_msb  = 0x2D, /**< Linear accel Z high byte */
  k_bno055_reg_grv_x_lsb  = 0x2E, /**< Gravity vector X low byte */
  k_bno055_reg_grv_x_msb  = 0x2F, /**< Gravity vector X high byte */
  k_bno055_reg_grv_y_lsb  = 0x30, /**< Gravity vector Y low byte */
  k_bno055_reg_grv_y_msb  = 0x31, /**< Gravity vector Y high byte */
  k_bno055_reg_grv_z_lsb  = 0x32, /**< Gravity vector Z low byte */
  k_bno055_reg_grv_z_msb  = 0x33, /**< Gravity vector Z high byte */
  k_bno055_reg_temp       = 0x34, /**< Temperature in deg C (1 degC/LSB) */
  k_bno055_reg_calib_stat = 0x35, /**< Calibration status: SYS[7:6] GYR[5:4] ACC[3:2] MAG[1:0] */
  k_bno055_reg_int_sta =
    0x37, /**< Interrupt Status (Page 0) -- reading clears INT pin per DS 3.6 */
  k_bno055_reg_unit_sel     = 0x3B, /**< Unit selection register */
  k_bno055_reg_opr_mode     = 0x3D, /**< Operating mode register */
  k_bno055_reg_pwr_mode     = 0x3E, /**< Power mode register */
  k_bno055_reg_sys_trigger  = 0x3F, /**< System trigger (reset, self-test) */
  k_bno055_reg_axis_map_cfg = 0x41, /**< Axis remapping configuration */
  k_bno055_reg_axis_map_sgn = 0x42, /**< Axis remapping sign */
} bno055_reg_t;

/* =============================================================================
 * Configuration Enumerations
 * =============================================================================
 */

/**
 * @enum bno055_opr_mode_t
 * @brief BNO055 operating mode selection
 *
 * @details
 * Controls sensor fusion algorithm. CONFIG mode is required for configuration
 * changes. NDOF (Nine Degrees of Freedom) mode runs full sensor fusion with
 * all three sensors (accelerometer, gyroscope, magnetometer).
 *
 * @par Transition times:
 * - CONFIG -> any fusion mode: 7 ms minimum
 * - Any fusion mode -> CONFIG: 19 ms minimum
 *
 * @see BNO055 datasheet table 3-5 for all operating modes
 * @see bno055_delay_ms_t Timing constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_opr_config = 0x00, /**< Configuration mode (required for register writes) */
  k_bno055_opr_ndof   = 0x0C, /**< NDOF fusion: accel + gyro + mag absolute orientation */
} bno055_opr_mode_t;

/**
 * @enum bno055_pwr_mode_t
 * @brief BNO055 power management modes
 *
 * @details
 * Normal mode keeps all sensors active. Low-power and suspend modes
 * reduce power consumption at the cost of responsiveness.
 *
 * @see BNO055 datasheet section 3.2 for power management details
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_pwr_normal = 0x00, /**< Normal power mode: all sensors active */
} bno055_pwr_mode_t;

/**
 * @enum bno055_unit_sel_t
 * @brief BNO055 unit selection register values
 *
 * @details
 * Selects measurement units for each sensor output. Default configuration
 * uses degrees for Euler angles, m/s^2 for acceleration, dps for gyroscope,
 * and Celsius for temperature.
 *
 * @see BNO055 datasheet section 3.6.5 for unit selection details
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_unit_sel_default = 0x00, /**< Degrees, m/s^2, dps, Celsius (factory default) */
} bno055_unit_sel_t;

/**
 * @enum bno055_sys_trigger_t
 * @brief BNO055 system trigger register values
 *
 * @details
 * Controls system reset and self-test initiation. Writing k_bno055_sys_trigger_rst
 * issues a software reset; the sensor reboots and takes ~650 ms to restart.
 *
 * @see BNO055 datasheet section 3.6.7 for SYS_TRIGGER details
 * @see bno055_delay_ms_t Boot delay constant
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_sys_trigger_rst   = 0x20, /**< Software reset command (bit 5) */
  k_bno055_sys_trigger_clear = 0x00, /**< Clear trigger register (normal operation) */
} bno055_sys_trigger_t;

/**
 * @enum bno055_axis_map_t
 * @brief BNO055 axis remapping configuration values
 *
 * @details
 * Controls axis remapping for mounting orientation compensation. Default
 * values match the standard orientation (X=0, Y=1, Z=2) with positive signs
 * on all axes.
 *
 * @see BNO055 datasheet section 3.4 for axis remapping details
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_axis_map_default  = 0x24, /**< Default axis map: X=0,Y=1,Z=2 (0b00100100) */
  k_bno055_axis_map_sign_pos = 0x00, /**< All axes positive sign */
} bno055_axis_map_t;

/**
 * @enum bno055_chip_id_t
 * @brief BNO055 chip identification constant
 *
 * @details
 * Fixed chip ID read from register 0x00 after power-on. Used to verify
 * successful I2C communication and device identity during initialization.
 *
 * @invariant CHIP_ID register always reads k_bno055_chip_id_expected on genuine BNO055
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_chip_id_expected = 0xA0, /**< Factory-programmed chip ID for BNO055 */
} bno055_chip_id_t;

/**
 * @enum bno055_i2c_addr_t
 * @brief BNO055 I2C device address
 *
 * @details
 * The I2C address is determined by the COM3/ADR pin state:
 * - ADR = LOW (default/DFRobot module): 0x28
 * - ADR = HIGH: 0x29
 *
 * The STAR project uses the DFRobot module with ADR pulled LOW.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_i2c_addr = 0x28, /**< I2C device address (COM3/ADR=LOW, DFRobot module) */
} bno055_i2c_addr_t;

/**
 * @enum bno055_delay_ms_t
 * @brief BNO055 required startup and mode-change delay constants
 *
 * @details
 * Timing requirements from BNO055 datasheet for power-on reset and
 * operating mode transitions. Violating these delays will cause
 * unreliable register reads and communication failures.
 *
 * | Transition | Required Delay |
 * |------------|----------------|
 * | Power-on / software reset | 650 ms minimum |
 * | CONFIG -> NDOF (fusion modes) | 7 ms minimum |
 * | Any fusion -> CONFIG | 19 ms minimum |
 *
 * @see BNO055 datasheet table 0-2 (timing characteristics)
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_bno055_delay_por_ms    = 650, /**< Power-on reset / software reset boot time (ms) */
  k_bno055_delay_config_ms = 19,  /**< Fusion -> CONFIG mode transition delay (ms) */
  k_bno055_delay_ndof_ms   = 7,   /**< CONFIG -> NDOF mode transition delay (ms) */
} bno055_delay_ms_t;

/**
 * @enum bno055_delay_ticks_t
 * @brief BNO055 ThreadX tick delay constants for mode transitions
 *
 * @details
 * Pre-computed tick counts for tx_thread_sleep() calls during the BNO055
 * initialization sequence. All values are rounded up to the next 10 ms tick
 * boundary to ensure minimum required delays are met.
 *
 * | Transition | Min Delay | Ticks Used | Actual Delay |
 * |------------|-----------|------------|--------------|
 * | POR/reset  | 650 ms    | 65 ticks   | 650 ms exact |
 * | -> CONFIG  | 19 ms     | 2 ticks    | 20 ms (round-up) |
 * | -> NDOF    | 7 ms      | 1 tick     | 10 ms (round-up) |
 *
 * @see bno055_delay_ms_t Delay values in milliseconds
 * @see rx_bno055_init() Uses these for tx_thread_sleep() calls
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_delay_ndof_ticks = 1, /**< CONFIG->NDOF transition: 7 ms rounds up to 1 tick (10 ms) */
  k_bno055_delay_por_ticks = 65, /**< POR/software reset boot: 650 ms / 10 ms per tick = 65 ticks */
  k_bno055_delay_config_ticks =
    2, /**< Fusion->CONFIG transition: 19 ms rounds up to 2 ticks (20 ms) */
} bno055_delay_ticks_t;

/**
 * @enum bno055_tick_round_t
 * @brief Extra tick for rounding fractional delay values
 *
 * @details
 * When a delay in milliseconds does not divide evenly by the tick period
 * (k_bno055_ms_per_tick = 10 ms), add one extra tick to guarantee the
 * minimum required delay is met. Used in the CONFIG mode transition
 * (19 ms rounds up to 20 ms = 2 ticks).
 *
 * @see bno055_delay_ticks_t Delay tick constants
 * @see rx_bno055_init() Uses this in tx_thread_sleep() calls
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_tick_round_up =
    1, /**< Extra tick added to round fractional delays up to next tick boundary */
} bno055_tick_round_t;

/**
 * @enum bno055_regs_scale_t
 * @brief BNO055 register-level output data scale factors (internal register naming)
 *
 * @details
 * Scale factors to convert raw 16-bit integer output to physical units.
 * Divide the raw register value by the corresponding scale constant.
 * For the public API scale constants used with bno055_data_t, see bno055_scale_t in rx_bno055.h.
 *
 * | Output Register | Scale | Physical Unit |
 * |-----------------|-------|---------------|
 * | Euler angles | 16 | degrees |
 * | Quaternion W/X/Y/Z | 16384 | dimensionless (unit quaternion) |
 * | Linear acceleration | 100 | m/s^2 |
 * | Gyroscope | 16 | deg/s |
 *
 * @see bno055_scale_t Public API scale constants in rx_bno055.h
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_bno055_scale_euler_lsb_per_deg  = 16,    /**< Euler angle: 16 LSB per degree */
  k_bno055_scale_accel_lsb_per_mps2 = 100,   /**< Linear accel: 100 LSB per m/s^2 */
  k_bno055_scale_gyro_lsb_per_dps   = 16,    /**< Gyroscope: 16 LSB per deg/s */
  k_bno055_scale_quat_lsb           = 16384, /**< Quaternion: 16384 LSB per unit */
} bno055_regs_scale_t;

/* NOTE: Calibration bit-shift and mask constants have been moved to rx_bno055.h
 * as bno055_calib_shift_t and bno055_calib_mask_t (public API types). */

/* =============================================================================
 * Read Buffer Size Constants
 * =============================================================================
 */

/**
 * @enum bno055_read_size_t
 * @brief BNO055 burst read buffer size constants
 *
 * @details
 * Defines byte counts for burst I2C reads of contiguous register blocks.
 * Used to pre-size stack buffers in driver functions.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_euler_bytes = 6U, /**< Euler angles: heading(2) + roll(2) + pitch(2) */
  k_bno055_quat_bytes  = 8U, /**< Quaternion: W(2) + X(2) + Y(2) + Z(2) */
  k_bno055_lia_bytes   = 6U, /**< Linear acceleration: X(2) + Y(2) + Z(2) */
  k_bno055_gyro_bytes  = 6U, /**< Gyroscope: X(2) + Y(2) + Z(2) */
} bno055_read_size_t;

/**
 * @enum bno055_byte_idx_t
 * @brief Byte index constants for little-endian 16-bit assembly
 *
 * @details
 * The BNO055 outputs all multi-byte values in little-endian format (LSB first).
 * These indices reference the LSB and MSB positions within a 2-byte register pair.
 *
 * @code
 * int16_t val = (int16_t)((uint16_t)buf[k_bno055_idx_lsb] |
 *                         ((uint16_t)buf[k_bno055_idx_msb] << k_bno055_shift_msb));
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_idx_lsb = 0U, /**< Byte index of LSB in a little-endian 2-byte value */
  k_bno055_idx_msb = 1U, /**< Byte index of MSB in a little-endian 2-byte value */
} bno055_byte_idx_t;

/**
 * @enum bno055_shift_t
 * @brief Bit-shift constant for assembling 16-bit little-endian values
 *
 * @details
 * The MSB of a 16-bit little-endian pair must be shifted left by 8 bits
 * to place it in the upper byte position when assembling the full value.
 *
 * @see bno055_byte_idx_t Byte index constants used with this shift
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_shift_msb = 8U, /**< Bit-shift to place MSB into upper byte position */
} bno055_shift_t;

/**
 * @enum bno055_tick_rate_t
 * @brief ThreadX tick rate constant for delay conversion
 *
 * @details
 * The ThreadX RTOS is configured at 100 Hz tick rate (10 ms per tick).
 * Delay values in bno055_delay_ms_t (milliseconds) must be divided by
 * k_bno055_ms_per_tick to compute the correct tx_thread_sleep() argument.
 *
 * Conversion: ticks = delay_ms / k_bno055_ms_per_tick
 *
 * @invariant k_bno055_ms_per_tick > 0 (must be a valid divisor)
 *
 * @warning Assumes TX_TIMER_TICKS_PER_SECOND == 100. Must be updated if
 *          ThreadX tick rate changes (e.g., to 1000 Hz for 1 ms ticks).
 *
 * @see bno055_delay_ms_t Delay constants in milliseconds
 * @see rx_bno055_init() Uses these tick conversions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_ms_per_tick = 10, /**< ThreadX tick period: 10 ms at 100 Hz tick rate */
} bno055_tick_rate_t;

/**
 * @enum bno055_time_const_t
 * @brief Time conversion constants for tick-rate validation
 *
 * @details
 * Named constant for milliseconds per second, used in the compile-time
 * assertion that verifies k_bno055_ms_per_tick matches TX_TIMER_TICKS_PER_SECOND.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_bno055_ms_per_second = 1000U, /**< Milliseconds per second (for tick-rate conversion) */
} bno055_time_const_t;

/* =============================================================================
 * Page 1 Interrupt Engine Registers
 * =============================================================================
 */

/**
 * @enum bno055_page_t
 * @brief BNO055 register page selection values for the PAGE_ID register
 *
 * @details
 * The BNO055 organizes registers into two pages selected by writing to PAGE_ID
 * (address 0x07 on both pages). Page 0 is the default after reset and contains
 * all sensor output registers. Page 1 contains the interrupt engine configuration
 * registers used to enable the INT output pin.
 *
 * @see bno055_int_reg_t Page 1 interrupt register addresses
 * @see rx_bno055.c internal_init_enable_interrupt() Uses PAGE_ID to switch pages
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_page0 = 0x00U, /**< Page 0 (default after reset): sensor output, system registers */
  k_bno055_page1 = 0x01U, /**< Page 1: interrupt engine configuration registers */
} bno055_page_t;

/**
 * @enum bno055_int_reg_t
 * @brief BNO055 interrupt engine register addresses (PAGE_ID and Page 1 registers)
 *
 * @details
 * These register addresses control the BNO055 interrupt output (INT pin).
 * PAGE_ID (0x07) exists on both pages and selects the active page.
 * All other addresses in this enum are Page 1 registers.
 *
 * Interrupt configuration sequence (while in CONFIG mode):
 * 1. Write PAGE_ID=1 to switch to Page 1
 * 2. Write INT_MSK (0x0F): route ACC_BSX_DRDY to INT pin
 * 3. Write INT_EN (0x10): enable ACC_BSX_DRDY interrupt source
 * 4. Write PAGE_ID=0 to return to Page 0 for normal operation
 *
 * @warning All addresses in this enum (except PAGE_ID) are Page 1 addresses.
 *          Writing them without first setting PAGE_ID=1 will corrupt Page 0
 *          sensor output registers (e.g., 0x0F = MAG_RADIUS_MSB on Page 0).
 *
 * @invariant All addresses are 8-bit I2C register addresses
 * @see bno055_page_t Page selection values
 * @see rx_bno055.c internal_init_enable_interrupt()
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_reg_page_id = 0x07U, /**< PAGE_ID register (both pages): 0=Page0, 1=Page1 */
  k_bno055_reg_int_msk = 0x0FU, /**< INT_MSK (Page 1 0x0F): routes interrupt sources to INT pin */
  k_bno055_reg_int_en  = 0x10U, /**< INT_EN (Page 1 0x10): enables interrupt sources */
} bno055_int_reg_t;

/**
 * @enum bno055_int_en_t
 * @brief INT_EN register bit values for enabling interrupt sources (Page 1, 0x10)
 *
 * @details
 * Each bit in INT_EN enables a specific interrupt source.
 * Bit 0 (k_bno055_int_en_acc_bsx_drdy) enables the accelerometer BSX
 * data-ready interrupt, which asserts on every new accelerometer sample.
 *
 * INT_EN bit map (BNO055 datasheet Table 4-16):
 * - Bit 7: Reserved
 * - Bit 6: ACC_AM_EN -- accelerometer any-motion
 * - Bit 5: ACC_HIGH_G_EN -- accelerometer high-g
 * - Bit 4: Reserved
 * - Bit 3: GYR_HIGH_RATE_EN
 * - Bit 2: GYR_AM_EN
 * - Bit 1: Reserved
 * - Bit 0: ACC_BSX_DRDY -- accelerometer data-ready (this constant)
 *
 * @see bno055_int_reg_t k_bno055_reg_int_en register address
 * @see bno055_int_msk_t INT_MSK mask values (same bit layout as INT_EN)
 * @see rx_bno055.c internal_init_enable_interrupt()
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_int_en_acc_bsx_drdy =
    0x01U, /**< INT_EN bit 0: enable accelerometer data-ready interrupt */
} bno055_int_en_t;

/**
 * @enum bno055_int_msk_t
 * @brief INT_MSK register bit values for routing interrupt sources to INT pin (Page 1, 0x0F)
 *
 * @details
 * INT_MSK has the same bit layout as INT_EN (Table 4-15). Setting a bit here
 * routes the corresponding enabled interrupt source to the physical INT pin.
 * Both INT_EN and INT_MSK must be set for the INT pin to assert.
 *
 * INT_MSK bit 0 (k_bno055_int_msk_acc_bsx_drdy) routes the accelerometer BSX
 * data-ready interrupt to the INT pin so it asserts on every new sample.
 *
 * @see bno055_int_reg_t k_bno055_reg_int_msk register address
 * @see bno055_int_en_t INT_EN enable values (same bit positions)
 * @see rx_bno055.c internal_init_enable_interrupt()
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_int_msk_acc_bsx_drdy = 0x01U, /**< INT_MSK bit 0: route ACC_BSX_DRDY to INT pin */
} bno055_int_msk_t;

#ifdef __cplusplus
}
#endif
