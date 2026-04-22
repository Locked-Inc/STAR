/**
 * @file rx_bmp280_regs.h
 * @brief BMP280 Barometric Pressure and Temperature Sensor Register Map
 *
 * @details
 * # Overview
 *
 * Provides all register address definitions and configuration constants for the
 * Bosch BMP280 digital pressure sensor. All constants are defined as C23 typed
 * enumerations following STAR project conventions.
 *
 * The BMP280 measures atmospheric pressure (300-1100 hPa) and temperature
 * (-40 to +85 degC) using a combined MEMS capacitive pressure cell and digital
 * temperature sensor. Compensation coefficients are trimmed at the factory and
 * stored in on-chip OTP memory (registers 0x88-0x9F).
 *
 * # Register Map Summary
 *
 * | Address | Content |
 * |---------|---------|
 * | 0x88-0x9F | Calibration data (24 bytes) |
 * | 0xD0 | Chip ID (0x60) |
 * | 0xF3 | Status register |
 * | 0xF4 | ctrl_meas (osrs_t, osrs_p, mode) |
 * | 0xF5 | config (t_sb, filter, spi3w_en) |
 * | 0xF7-0xF9 | Pressure ADC (20-bit, MSB/LSB/XLSB) |
 * | 0xFA-0xFC | Temperature ADC (20-bit, MSB/LSB/XLSB) |
 *
 * # DFRobot Module Hardware
 *
 * - I2C Address: 0x76 (SDO pulled LOW)
 * - Connected to RIIC1 (SCL1=P2.1, SDA1=P2.0)
 * - Power: 3.3V
 *
 * @see rx_bmp280.h Public driver API
 * @see rx_bmp280.c Driver implementation
 * @see BMP280 datasheet v1.19 for complete register descriptions
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
 * @enum bmp280_reg_t
 * @brief BMP280 register addresses
 *
 * @details
 * Register addresses for all BMP280 control, configuration, and output
 * registers. Calibration data (0x88-0x9F) is accessed as a 24-byte burst
 * read starting at k_bmp280_reg_calib_start.
 *
 * ADC output registers (0xF7-0xFC) contain 20-bit raw values:
 * - Pressure: bits[19:4] in PRESS_MSB[7:0] and PRESS_LSB[7:0] and
 *   bits[3:0] in PRESS_XLSB[7:4]
 * - Temperature: same layout at TEMP_MSB/LSB/XLSB
 *
 * @invariant k_bmp280_reg_calib_start contains factory-calibrated coefficients
 * @invariant 0xF7 output is valid only after measurement completes
 *
 * @pre BMP280 has completed power-on reset (POR) sequence
 * @pre I2C bus interface is initialized and BMP280 is addressable
 *
 * @post Registers accessible after BMP280 power-on reset sequence completes
 * @post All addresses remain constant for the lifetime of the device
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_reg_chip_id     = 0xD0, /**< Chip ID register: contains device ID 0x60 for BMP280 */
  k_bmp280_reg_calib_start = 0x88, /**< Start of 24-byte calibration coefficient block */
  k_bmp280_reg_calib_end =
    0x9F, /**< Last byte of 24-byte calibration coefficient block (0x88-0x9F) */
  k_bmp280_reg_status     = 0xF3, /**< Status: bit 3 = measuring, bit 0 = im_update */
  k_bmp280_reg_ctrl_meas  = 0xF4, /**< Control: osrs_t[7:5] osrs_p[4:2] mode[1:0] */
  k_bmp280_reg_config     = 0xF5, /**< Config: t_sb[7:5] filter[4:2] spi3w_en[0] */
  k_bmp280_reg_press_msb  = 0xF7, /**< Pressure MSB: press_raw[19:12] */
  k_bmp280_reg_press_lsb  = 0xF8, /**< Pressure LSB: press_raw[11:4] */
  k_bmp280_reg_press_xlsb = 0xF9, /**< Pressure XLSB: press_raw[3:0] in bits [7:4] */
  k_bmp280_reg_temp_msb   = 0xFA, /**< Temperature MSB: temp_raw[19:12] */
  k_bmp280_reg_temp_lsb   = 0xFB, /**< Temperature LSB: temp_raw[11:4] */
  k_bmp280_reg_temp_xlsb  = 0xFC, /**< Temperature XLSB: temp_raw[3:0] in bits [7:4] */
} bmp280_reg_t;

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum bmp280_i2c_addr_t
 * @brief BMP280 I2C device address
 *
 * @details
 * The I2C address is selected by the SDO pin:
 * - SDO = LOW (DFRobot module default): 0x76
 * - SDO = HIGH: 0x77
 *
 * @invariant k_bmp280_i2c_addr is a valid 7-bit I2C address (0x00..0x7F)
 * @invariant Value is constant and determined solely by the SDO pin strapping
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_i2c_addr = 0x76, /**< I2C device address (SDO=LOW, DFRobot module) */
} bmp280_i2c_addr_t;

/**
 * @enum bmp280_chip_id_t
 * @brief BMP280 expected chip ID value from register 0xD0
 *
 * @details
 * The BMP280 always reads 0x60 from register k_bmp280_reg_chip_id (0xD0).
 * This constant is used during initialization to verify that the correct
 * device is present on the I2C bus before reading calibration data.
 *
 * @invariant The BMP280 chip ID register always reads 0x58 on authentic BMP280
 *            devices; 0x60 is the BME280 (pressure + humidity) ID and must not
 *            be accepted here.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_chip_id_expected = 0x58, /**< BMP280 chip ID at reg 0xD0 (DS 5.2.1) */
} bmp280_chip_id_t;

/**
 * @enum bmp280_ctrl_meas_t
 * @brief BMP280 ctrl_meas register value (0xF4)
 *
 * @details
 * Pre-computed register value for the required measurement configuration:
 *
 * **ctrl_meas (0xF4) = 0x55:**
 * - osrs_t[7:5] = 010 = 2x oversampling for temperature
 * - osrs_p[4:2] = 101 = 16x oversampling for pressure
 * - mode[1:0]   = 01  = Forced mode (one measurement, then sleep)
 *
 * @invariant k_bmp280_ctrl_meas_val encodes osrs_t=2x, osrs_p=16x, forced mode exactly
 * @invariant Forced mode bits ([1:0] = 01) ensure sensor returns to sleep after each measurement
 *
 * @see bmp280_config_t IIR filter and standby configuration register value
 * @see bmp280_status_mask_t Status register measuring-bit mask
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_ctrl_meas_val = 0x55, /**< ctrl_meas: osrs_t=2x, osrs_p=16x, forced mode */
} bmp280_ctrl_meas_t;

/**
 * @enum bmp280_config_t
 * @brief BMP280 config register value (0xF5)
 *
 * @details
 * Pre-computed register value for IIR filter and standby configuration:
 *
 * **config (0xF5) = 0x04:**
 * - t_sb[7:5]   = 000 = 0.5 ms standby time (not used in forced mode)
 * - filter[4:2] = 001 = IIR filter coefficient 2
 * - spi3w_en[0] = 0   = SPI disabled
 *
 * @invariant k_bmp280_config_val sets filter coefficient 2 to reduce noise on pressure readings
 * @invariant SPI disabled bit ([0] = 0) ensures I2C mode remains active
 *
 * @see bmp280_ctrl_meas_t Oversampling and mode configuration register value
 * @see bmp280_status_mask_t Status register measuring-bit mask
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_config_val =
    0x04, /**< config: filter coef=2 (filter[4:2]=001), standby=0.5ms, SPI disabled */
} bmp280_config_t;

/**
 * @enum bmp280_status_mask_t
 * @brief BMP280 status register measuring-bit mask (0xF3)
 *
 * @details
 * Bitmask for the measuring bit in the BMP280 status register:
 *
 * **status (0xF3) measuring bit:**
 * - Bit 3 = 1 while measurement in progress
 * - Bit 3 = 0 when measurement complete (ADC data ready)
 *
 * @invariant k_bmp280_status_meas_mask isolates exactly bit 3 of the status register
 * @invariant Value is read-only; writing this mask to the sensor has no effect
 *
 * @see bmp280_ctrl_meas_t Oversampling and mode configuration register value
 * @see bmp280_config_t IIR filter and standby configuration register value
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_status_meas_mask =
    0x08, /**< Status measuring bit: bit 3 set while measurement in progress */
} bmp280_status_mask_t;

/**
 * @enum bmp280_calib_reg_count_t
 * @brief BMP280 calibration data size
 *
 * @details
 * The calibration block at 0x88 contains 24 bytes encoding three temperature
 * coefficients and nine pressure coefficients:
 * - T1 (uint16), T2 (int16), T3 (int16) at 0x88-0x8D
 * - P1 (uint16)..P9 (int16) at 0x8E-0x9F
 *
 * @invariant Calibration bytes always valid (factory programmed in OTP)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_calib_byte_count = 24, /**< Number of calibration coefficient bytes (0x88-0x9F) */
} bmp280_calib_reg_count_t;

/**
 * @enum bmp280_poll_t
 * @brief BMP280 measurement polling loop bound and sleep constant
 *
 * @details
 * Maximum number of status register polls when waiting for a forced
 * measurement to complete. With 16x oversampling, maximum measurement
 * time is approximately 40 ms. Each poll sleeps k_bmp280_poll_sleep_ticks
 * between reads to yield CPU time. At 1 tick (10 ms) per poll, 10
 * iterations provides ~100 ms maximum poll time, well under the 900 ms
 * IMU task IWDT watchdog timeout.
 *
 * @invariant k_bmp280_poll_max > 0 (loop must terminate)
 * @invariant k_bmp280_poll_sleep_ticks >= 1 (must yield at least one tick)
 * @invariant k_bmp280_poll_max * k_bmp280_poll_sleep_ticks * 10 < 900 (must not exceed IWDT timeout)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_poll_max         = 10, /**< Maximum status poll iterations (~100 ms at 100 Hz tick) */
  k_bmp280_poll_sleep_ticks = 1,  /**< Ticks to sleep between polls (1 tick = 10 ms at 100 Hz) */
} bmp280_poll_t;

/* =============================================================================
 * ADC Assembly Constants
 * =============================================================================
 */

/**
 * @enum bmp280_adc_shift_t
 * @brief BMP280 ADC value bit-shift constants
 *
 * @details
 * Shift amounts used to assemble 20-bit ADC values from three 8-bit registers:
 * adc_val = ((int32_t)msb << 12) | ((int32_t)lsb << k_bmp280_shift_lsb_left) | ((int32_t)xlsb >> k_bmp280_shift_xlsb_right)
 *
 * Note: k_bmp280_shift_lsb_left and k_bmp280_shift_xlsb_right have the same numeric
 * value (4) but represent different operations: left-shift of the LSB byte and
 * right-shift of the XLSB byte respectively.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_shift_msb = 12, /**< Left-shift MSB byte 12 bits for 20-bit ADC assembly (bits[19:12]) */
  k_bmp280_shift_lsb_left =
    4, /**< Left-shift LSB byte 4 bits for 20-bit ADC assembly (bits[11:4]) */
  k_bmp280_shift_xlsb_right =
    4, /**< Right-shift XLSB byte 4 bits to extract lower nibble (bits[3:0]) */
} bmp280_adc_shift_t;

/**
 * @enum bmp280_adc_buf_t
 * @brief BMP280 ADC read buffer size
 *
 * @details
 * Buffer size for the 6-byte burst read from register 0xF7:
 * buf[0..2] = pressure MSB/LSB/XLSB, buf[3..5] = temperature MSB/LSB/XLSB
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_adc_buf_size = 6, /**< Byte count for 6-byte burst read (press + temp) */
} bmp280_adc_buf_t;

/**
 * @enum bmp280_adc_idx_t
 * @brief BMP280 ADC buffer byte indices
 *
 * @details
 * Byte indices into the 6-byte ADC read buffer returned from address 0xF7.
 * Buffer layout: buf[0]=press_msb, buf[1]=press_lsb, buf[2]=press_xlsb,
 * buf[3]=temp_msb, buf[4]=temp_lsb, buf[5]=temp_xlsb
 *
 * @invariant All index values in [0, k_bmp280_adc_buf_size - 1]
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_press_msb_idx  = 0, /**< Index of pressure MSB in 6-byte buffer */
  k_bmp280_press_lsb_idx  = 1, /**< Index of pressure LSB in 6-byte buffer */
  k_bmp280_press_xlsb_idx = 2, /**< Index of pressure XLSB in 6-byte buffer */
  k_bmp280_temp_msb_idx   = 3, /**< Index of temperature MSB in 6-byte buffer */
  k_bmp280_temp_lsb_idx   = 4, /**< Index of temperature LSB in 6-byte buffer */
  k_bmp280_temp_xlsb_idx  = 5, /**< Index of temperature XLSB in 6-byte buffer */
} bmp280_adc_idx_t;

/**
 * @enum bmp280_calib_idx_t
 * @brief BMP280 calibration byte indices in 24-byte calib buffer
 *
 * @details
 * Maps the 24-byte calibration buffer to the BMP280's trimming parameter
 * layout. All coefficients are stored little-endian (LSB first).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_calib_t1_lsb = 0,  /**< dig_T1 low byte (uint16_t) */
  k_bmp280_calib_t1_msb = 1,  /**< dig_T1 high byte */
  k_bmp280_calib_t2_lsb = 2,  /**< dig_T2 low byte (int16_t) */
  k_bmp280_calib_t2_msb = 3,  /**< dig_T2 high byte */
  k_bmp280_calib_t3_lsb = 4,  /**< dig_T3 low byte (int16_t) */
  k_bmp280_calib_t3_msb = 5,  /**< dig_T3 high byte */
  k_bmp280_calib_p1_lsb = 6,  /**< dig_P1 low byte (uint16_t) */
  k_bmp280_calib_p1_msb = 7,  /**< dig_P1 high byte */
  k_bmp280_calib_p2_lsb = 8,  /**< dig_P2 low byte (int16_t) */
  k_bmp280_calib_p2_msb = 9,  /**< dig_P2 high byte */
  k_bmp280_calib_p3_lsb = 10, /**< dig_P3 low byte (int16_t) */
  k_bmp280_calib_p3_msb = 11, /**< dig_P3 high byte */
  k_bmp280_calib_p4_lsb = 12, /**< dig_P4 low byte (int16_t) */
  k_bmp280_calib_p4_msb = 13, /**< dig_P4 high byte */
  k_bmp280_calib_p5_lsb = 14, /**< dig_P5 low byte (int16_t) */
  k_bmp280_calib_p5_msb = 15, /**< dig_P5 high byte */
  k_bmp280_calib_p6_lsb = 16, /**< dig_P6 low byte (int16_t) */
  k_bmp280_calib_p6_msb = 17, /**< dig_P6 high byte */
  k_bmp280_calib_p7_lsb = 18, /**< dig_P7 low byte (int16_t) */
  k_bmp280_calib_p7_msb = 19, /**< dig_P7 high byte */
  k_bmp280_calib_p8_lsb = 20, /**< dig_P8 low byte (int16_t) */
  k_bmp280_calib_p8_msb = 21, /**< dig_P8 high byte */
  k_bmp280_calib_p9_lsb = 22, /**< dig_P9 low byte (int16_t) */
  k_bmp280_calib_p9_msb = 23, /**< dig_P9 high byte */
} bmp280_calib_idx_t;

/**
 * @enum bmp280_calib_shift_t
 * @brief Bit shift for little-endian 16-bit calibration coefficient assembly
 *
 * @details
 * Calibration coefficients are stored little-endian (LSB first). The MSB
 * must be shifted left by 8 bits to form a 16-bit value.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_byte_shift = 8, /**< Shift MSB byte left 8 bits for 16-bit assembly */
} bmp280_calib_shift_t;

/**
 * @enum bmp280_write_size_t
 * @brief I2C write/read buffer size constants for BMP280
 *
 * @details
 * BMP280 register write sends [register_address, data_byte] as 2 bytes.
 * Size constants for determining transaction lengths.
 *
 * @invariant k_bmp280_write_buf_size == k_bmp280_write_idx_val + 1 (buffer exactly fits register + value)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_write_buf_size = 2, /**< Size of [reg, val] write buffer */
  k_bmp280_read_cmd_size  = 1, /**< Size of register address read command */
  k_bmp280_single_byte    = 1, /**< Size for single byte reads */
} bmp280_write_size_t;

/**
 * @enum bmp280_write_idx_t
 * @brief I2C write buffer index constants for BMP280 register writes
 *
 * @details
 * Index constants for accessing fields within the 2-byte I2C write buffer
 * [register_address, data_byte] used by internal_write_reg().
 *
 * @see bmp280_write_size_t Size constants for the same buffer
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_write_idx_reg = 0, /**< Index of register address in write buffer */
  k_bmp280_write_idx_val = 1, /**< Index of register value in write buffer */
} bmp280_write_idx_t;

#ifdef __cplusplus
}
#endif
