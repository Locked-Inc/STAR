/* libs/rx_bmp280/inc/rx_bmp280_constants.h */

/**
 * @file rx_bmp280_constants.h
 * @brief Bosch BMP280 Barometric Pressure Sensor Register Addresses and Constants
 *
 * @details
 * Defines all register addresses, initialization values, and bit-field
 * indices for the Bosch BMP280 barometric pressure and temperature sensor.
 * The BMP280 provides compensated pressure (Pa) and temperature (0.01°C)
 * via 24-byte trim calibration parameters stored in non-volatile memory.
 *
 * @par Register Map (key registers)
 * | Address | Register | Description |
 * |---------|----------|-------------|
 * | 0xD0 | id | Chip ID (reads 0x58 for BMP280) |
 * | 0xE0 | reset | Soft reset (write 0xB6) |
 * | 0x88-0x9F | calib00..calib23 | Trim calibration parameters (24 bytes) |
 * | 0xF3 | status | Measuring and NVM update flags |
 * | 0xF4 | ctrl_meas | Oversampling + power mode control |
 * | 0xF5 | config | Standby time, IIR filter, SPI3w mode |
 * | 0xF7 | press_msb | Pressure data MSB [7:0] |
 * | 0xF8 | press_lsb | Pressure data LSB [7:0] |
 * | 0xF9 | press_xlsb | Pressure data XLSB [7:4], bits [3:0] unused |
 * | 0xFA | temp_msb | Temperature data MSB [7:0] |
 * | 0xFB | temp_lsb | Temperature data LSB [7:0] |
 * | 0xFC | temp_xlsb | Temperature data XLSB [7:4], bits [3:0] unused |
 *
 * @see BMP280 Data Sheet (Bosch BST-BMP280-DS001-19)
 * @see rx_bmp280.h Public driver API
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
 * @enum bmp280_addr_t
 * @brief BMP280 I2C device address options
 *
 * @details
 * 7-bit address determined by SDO pin state:
 * - SDO tied to GND: address = 0x76 (default STAR hardware)
 * - SDO tied to VDDIO: address = 0x77
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_addr_default = 0x76, /**< SDO=GND: default STAR hardware address */
  k_bmp280_addr_alt     = 0x77, /**< SDO=VDDIO: alternate address */
} bmp280_addr_t;

/* =============================================================================
 * Chip ID
 * =============================================================================
 */

/**
 * @enum bmp280_chip_id_t
 * @brief BMP280 chip identification constants
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_id_reg   = 0xD0, /**< Register address of chip ID */
  k_bmp280_id_value = 0x58, /**< Expected chip ID for BMP280 */
} bmp280_chip_id_t;

/* =============================================================================
 * System Register Addresses
 * =============================================================================
 */

/**
 * @enum bmp280_sys_regs_t
 * @brief BMP280 system and configuration register addresses
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_reset_reg    = 0xE0, /**< Soft-reset register: write 0xB6 to reset */
  k_bmp280_status_reg   = 0xF3, /**< Status: bit3=measuring, bit0=im_update */
  k_bmp280_ctrl_meas    = 0xF4, /**< Control measurement: osrs_t, osrs_p, mode */
  k_bmp280_config_reg   = 0xF5, /**< Config: t_sb (standby time), filter, spi3w_en */
} bmp280_sys_regs_t;

/* =============================================================================
 * Calibration Registers
 * =============================================================================
 */

/**
 * @enum bmp280_calib_regs_t
 * @brief BMP280 calibration trim parameter register addresses
 *
 * @details
 * 24 bytes of trim data starting at 0x88. All trim words are little-endian.
 * Must be read once during init and stored for use in Bosch compensation formulas.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_calib_base = 0x88, /**< First calibration register (dig_T1 LSB) */
  k_bmp280_calib_len  = 24,   /**< Total calibration data: 12 trim params × 2 bytes */
} bmp280_calib_regs_t;

/* =============================================================================
 * ADC Data Registers
 * =============================================================================
 */

/**
 * @enum bmp280_data_regs_t
 * @brief BMP280 raw ADC output register addresses
 *
 * @details
 * All 6 data bytes (pressure + temperature) can be read in a single burst
 * starting at 0xF7. The 20-bit ADC values are sign-extended and compensated
 * using the Bosch integer compensation formulas.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_data_base = 0xF7, /**< First data register: press_msb (read 6 bytes for P+T) */
  k_bmp280_data_len  = 6,    /**< Total data bytes: 3 pressure + 3 temperature */
} bmp280_data_regs_t;

/* =============================================================================
 * Byte Indices Within 6-Byte Data Read
 * =============================================================================
 */

/**
 * @enum bmp280_data_idx_t
 * @brief Array indices for parsing the 6-byte pressure+temperature burst read
 *
 * @details
 * Register layout: [press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb]
 * ADC value assembly: adc = (msb << 12) | (lsb << 4) | (xlsb >> 4)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_idx_press_msb  = 0, /**< buf[0]: press_msb[7:0] */
  k_bmp280_idx_press_lsb  = 1, /**< buf[1]: press_lsb[7:0] */
  k_bmp280_idx_press_xlsb = 2, /**< buf[2]: press_xlsb[7:4], bits[3:0]=unused */
  k_bmp280_idx_temp_msb   = 3, /**< buf[3]: temp_msb[7:0] */
  k_bmp280_idx_temp_lsb   = 4, /**< buf[4]: temp_lsb[7:0] */
  k_bmp280_idx_temp_xlsb  = 5, /**< buf[5]: temp_xlsb[7:4], bits[3:0]=unused */
} bmp280_data_idx_t;

/* =============================================================================
 * Initialization Values
 * =============================================================================
 */

/**
 * @enum bmp280_init_vals_t
 * @brief Register values written during BMP280 initialization
 *
 * @details
 * ctrl_meas = 0x2F breakdown:
 *   bits[7:5] = 001 → osrs_t ×1 (temperature oversampling ×1)
 *   bits[4:2] = 011 → osrs_p ×4 (pressure oversampling ×4)
 *   bits[1:0] = 11  → normal mode (continuous measurement)
 *
 * config = 0x70 breakdown:
 *   bits[7:5] = 011 → t_sb = 250 ms standby time between measurements
 *   bits[4:2] = 100 → filter coefficient = 16 (IIR filter)
 *   bit[0]    = 0   → SPI 3-wire disabled (I2C interface)
 *
 * @see BMP280 Data Sheet Table 24 (ctrl_meas register)
 * @see BMP280 Data Sheet Table 27 (config register)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_reset_value    = 0xB6, /**< Soft-reset trigger value for reset register */
  k_bmp280_ctrl_meas_val  = 0x2F, /**< osrs_t=×1, osrs_p=×4, mode=normal */
  k_bmp280_config_val     = 0x70, /**< t_sb=250ms, filter=16, spi3w=off */
} bmp280_init_vals_t;

/* =============================================================================
 * Bit Manipulation for ADC Assembly
 * =============================================================================
 */

/**
 * @enum bmp280_adc_shifts_t
 * @brief Bit shift counts for assembling 20-bit ADC values from 3 bytes
 *
 * @details
 * BMP280 stores 20-bit ADC as: msb[7:0] || lsb[7:0] || xlsb[7:4]
 * Assembled as: adc20 = (msb << 12) | (lsb << 4) | (xlsb >> 4)
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_shift_msb  = 12, /**< Left-shift count for MSB byte */
  k_bmp280_shift_lsb  = 4,  /**< Left-shift count for LSB byte */
  k_bmp280_shift_xlsb = 4,  /**< Right-shift count for XLSB byte (discard bits 3:0) */
} bmp280_adc_shifts_t;

/* =============================================================================
 * Calibration Data Byte Indices
 * =============================================================================
 */

/**
 * @enum bmp280_calib_idx_t
 * @brief Byte indices within the 24-byte calibration burst read
 *
 * @details
 * Calibration registers 0x88-0x9F store 12 trim words in little-endian order:
 * dig_T1[0:1], dig_T2[2:3], dig_T3[4:5],
 * dig_P1[6:7], dig_P2[8:9], dig_P3[10:11], dig_P4[12:13],
 * dig_P5[14:15], dig_P6[16:17], dig_P7[18:19], dig_P8[20:21], dig_P9[22:23]
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_calib_t1_lsb = 0,  /**< dig_T1 LSB (uint16) */
  k_bmp280_calib_t1_msb = 1,  /**< dig_T1 MSB (uint16) */
  k_bmp280_calib_t2_lsb = 2,  /**< dig_T2 LSB (int16) */
  k_bmp280_calib_t2_msb = 3,  /**< dig_T2 MSB (int16) */
  k_bmp280_calib_t3_lsb = 4,  /**< dig_T3 LSB (int16) */
  k_bmp280_calib_t3_msb = 5,  /**< dig_T3 MSB (int16) */
  k_bmp280_calib_p1_lsb = 6,  /**< dig_P1 LSB (uint16) */
  k_bmp280_calib_p1_msb = 7,  /**< dig_P1 MSB (uint16) */
  k_bmp280_calib_p2_lsb = 8,  /**< dig_P2 LSB (int16) */
  k_bmp280_calib_p2_msb = 9,  /**< dig_P2 MSB (int16) */
  k_bmp280_calib_p3_lsb = 10, /**< dig_P3 LSB (int16) */
  k_bmp280_calib_p3_msb = 11, /**< dig_P3 MSB (int16) */
  k_bmp280_calib_p4_lsb = 12, /**< dig_P4 LSB (int16) */
  k_bmp280_calib_p4_msb = 13, /**< dig_P4 MSB (int16) */
  k_bmp280_calib_p5_lsb = 14, /**< dig_P5 LSB (int16) */
  k_bmp280_calib_p5_msb = 15, /**< dig_P5 MSB (int16) */
  k_bmp280_calib_p6_lsb = 16, /**< dig_P6 LSB (int16) */
  k_bmp280_calib_p6_msb = 17, /**< dig_P6 MSB (int16) */
  k_bmp280_calib_p7_lsb = 18, /**< dig_P7 LSB (int16) */
  k_bmp280_calib_p7_msb = 19, /**< dig_P7 MSB (int16) */
  k_bmp280_calib_p8_lsb = 20, /**< dig_P8 LSB (int16) */
  k_bmp280_calib_p8_msb = 21, /**< dig_P8 MSB (int16) */
  k_bmp280_calib_p9_lsb = 22, /**< dig_P9 LSB (int16) */
  k_bmp280_calib_p9_msb = 23, /**< dig_P9 MSB (int16) */
} bmp280_calib_idx_t;

/* =============================================================================
 * Altitude Computation Constants
 * =============================================================================
 */

/**
 * @enum bmp280_altitude_t
 * @brief Constants for integer altitude approximation
 *
 * @details
 * Integer altitude formula: h_m ≈ (P0 - P) × 84 / 1000
 * where P0 = 101325 Pa (ISA standard atmosphere at sea level, 15°C).
 *
 * Derivation: At sea level, dP/dh ≈ −11.9 Pa/m, so 1 Pa ≈ 0.084 m.
 * Thus: Δh_m = ΔP_Pa × 0.084 ≈ ΔP × 84 / 1000.
 * Accuracy: ±10% for altitudes below 1000m above sea level.
 *
 * @since 1.0.0
 */
typedef enum : uint32_t {
  k_bmp280_sea_level_pa    = 101325, /**< Standard atmosphere pressure at sea level (Pa) */
  k_bmp280_alt_numerator   = 84,     /**< Altitude formula numerator: P×84/1000 gives meters */
  k_bmp280_alt_denominator = 1000,   /**< Altitude formula denominator */
} bmp280_altitude_t;

/* =============================================================================
 * Timing
 * =============================================================================
 */

/**
 * @enum bmp280_timing_t
 * @brief Timing delays for BMP280 initialization
 *
 * @details
 * The BMP280 requires a brief startup delay after soft reset before
 * it will ACK I2C transactions. Normal power-on delay is 2ms.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_bmp280_reset_delay_ms  = 5,  /**< Wait after soft reset before first read (ms) */
  k_bmp280_startup_delay_ms = 2, /**< Minimum startup time after power-on (ms) */
} bmp280_timing_t;
