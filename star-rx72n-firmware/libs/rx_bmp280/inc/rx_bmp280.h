/* libs/rx_bmp280/inc/rx_bmp280.h */

/**
 * @file rx_bmp280.h
 * @brief BMP280 Digital Barometric Pressure and Temperature Sensor Driver API
 *
 * @details
 * # Overview
 *
 * Platform-independent driver for the Bosch BMP280 digital barometric
 * pressure sensor. The BMP280 measures atmospheric pressure and ambient
 * temperature using a MEMS capacitive pressure element with integrated
 * temperature compensation.
 *
 * This driver uses forced mode operation: each measurement is explicitly
 * triggered, the sensor performs one conversion, and returns to sleep.
 * This minimizes self-heating and provides one-shot measurement semantics
 * suitable for periodic IMU task reads.
 *
 * # Key Specifications
 *
 * | Parameter | Value |
 * |-----------|-------|
 * | Pressure range | 300-1100 hPa |
 * | Pressure resolution | 0.0016 hPa |
 * | Temperature range | -40 to +85 degC |
 * | Temperature resolution | 0.01 degC |
 * | I2C speed | Up to 400 kHz (fast mode) |
 *
 * # Hardware Configuration (STAR Project)
 *
 * - **Module**: DFRobot BMP280 breakout
 * - **I2C Address**: 0x76 (SDO pulled LOW)
 * - **I2C Bus**: RIIC1 (SCL1=P2.1, SDA1=P2.0), bus name "i2c1_baro"
 *
 * # Operating Mode
 *
 * The driver uses forced mode (trigger -> measure -> sleep) with:
 * - Temperature oversampling: 2x
 * - Pressure oversampling: 16x
 * - IIR filter: coefficient 2
 *
 * # Integer Compensation Formulas
 *
 * The BMP280 uses factory-calibrated trimming parameters to compensate
 * raw ADC output. The driver implements the Bosch-provided integer-only
 * compensation formulas (no floating-point math in driver).
 *
 * Output values:
 * - temp_centi_degc: temperature * 100 (e.g. 2523 = 25.23 degC)
 * - press_pa_256: pressure * 256 (e.g. 26624000 = 104000 Pa = 1040 hPa)
 *
 * @see rx_bmp280_regs.h Register map and bit-field constants
 * @see rx_bmp280.c Implementation with Bosch compensation formulas
 * @see BMP280 datasheet v1.19 section 4.2.3 for compensation algorithms
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1** (No goto): [PASS] Structured control flow only
 * - **Rule 3** (No heap): [PASS] Zero dynamic allocation
 * - **Rule 5** (Assertions): [PASS] 2+ preconditions per function
 * - **Rule 7** (Return checks): [PASS] All I2C returns validated
 * - **Rule 8** (Preprocessor): [PASS] C23 typed enums only
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright MIT License
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_bmp280_regs.h"
#include "rx_bus_manager.h"
#include "rx_err.h"

/* =============================================================================
 * Data Types
 * =============================================================================
 */

/**
 * @struct bmp280_calib_t
 * @brief BMP280 factory calibration coefficients
 *
 * @details
 * Trimming parameters read from on-chip OTP during initialization.
 * Used by the Bosch integer compensation algorithm to convert raw
 * ADC values to calibrated temperature and pressure.
 *
 * All coefficients are little-endian in the register map and stored
 * as parsed C types here. dig_T1 and dig_P1 are unsigned; all others
 * are signed two's complement.
 *
 * @invariant All values loaded once from BMP280 OTP during init
 * @invariant dig_T1, dig_P1 are positive (unsigned)
 *
 * @see BMP280 datasheet section 4.2.2 for coefficient descriptions
 * @see rx_bmp280_init() Reads and parses these coefficients
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint16_t dig_T1; /**< Temperature compensation coefficient T1 (unsigned) */
  int16_t  dig_T2; /**< Temperature compensation coefficient T2 (signed) */
  int16_t  dig_T3; /**< Temperature compensation coefficient T3 (signed) */
  uint16_t dig_P1; /**< Pressure compensation coefficient P1 (unsigned) */
  int16_t  dig_P2; /**< Pressure compensation coefficient P2 (signed) */
  int16_t  dig_P3; /**< Pressure compensation coefficient P3 (signed) */
  int16_t  dig_P4; /**< Pressure compensation coefficient P4 (signed) */
  int16_t  dig_P5; /**< Pressure compensation coefficient P5 (signed) */
  int16_t  dig_P6; /**< Pressure compensation coefficient P6 (signed) */
  int16_t  dig_P7; /**< Pressure compensation coefficient P7 (signed) */
  int16_t  dig_P8; /**< Pressure compensation coefficient P8 (signed) */
  int16_t  dig_P9; /**< Pressure compensation coefficient P9 (signed) */
} bmp280_calib_t;

/**
 * @struct bmp280_data_t
 * @brief BMP280 compensated measurement output
 *
 * @details
 * Integer-scaled output values for temperature and pressure.
 * Avoids floating-point in the driver; convert to SI units in application:
 *
 * @code
 * float temp_celsius   = (float)data.temp_centi_degc / 100.0F;
 * float pressure_pa    = (float)data.press_pa_256    / 256.0F;
 * float pressure_hpa   = pressure_pa / 100.0F;
 * @endcode
 *
 * @invariant temp_centi_degc in [-4000, 8500] (operating range -40 to +85 degC)
 * @invariant press_pa_256 > 0 (absolute pressure always positive)
 *
 * @see rx_bmp280_read() Populates this structure
 *
 * @since Version 1.0.0
 */
typedef struct {
  int32_t  temp_centi_degc; /**< Temperature * 100 (e.g. 2523 = 25.23 degC) */
  uint32_t press_pa_256;    /**< Pressure * 256 in Pa (divide by 256.0 for Pa) */
} bmp280_data_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BMP280 sensor and read factory calibration coefficients
 *
 * @details
 * Reads the 24-byte calibration block from OTP (k_bmp280_reg_calib_start (0x88) through k_bmp280_reg_calib_end (0x9F)) and parses
 * the 12 trimming parameters into module-static storage.
 * Then writes the IIR filter configuration to register k_bmp280_reg_config (0xF5).
 *
 * The ctrl_meas register (0xF4) is NOT written during init; it is written
 * at the start of each rx_bmp280_read() call to trigger a forced measurement.
 * The bus manager must have "i2c1_baro" registered as the named I2C bus.
 *
 * @param[in] manager Pointer to initialized bus manager with "i2c1_baro" registered.
 *                    Must not be NULL.
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Calibration read, sensor ready for reads
 * @retval k_rx_err_null_ptr manager is NULL
 * @retval k_rx_err_nack I2C NACK (device not found, check address/wiring)
 * @retval k_rx_err_timeout I2C timeout
 *
 * @pre manager non-NULL, "i2c1_baro" bus registered and initialized
 * @pre BMP280 powered (3.3V) on same I2C bus as BNO055
 *
 * @post Calibration coefficients loaded into module-static storage
 * @post Driver initialized successfully (s_initialized == true on success)
 * @post IIR filter configured with coefficient 2
 *
 * @note Not thread-safe; call during single-threaded initialization
 * @note Blocks ~2 ms (I2C calibration burst read)
 *
 * @see rx_bmp280_read() Triggered forced measurements after init
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bmp280_init(rx_bus_manager_t* manager);

/**
 * @brief Trigger a forced measurement and read compensated pressure and temperature
 *
 * @details
 * Measurement sequence:
 * 1. Write ctrl_meas = k_bmp280_ctrl_meas_val (0x55) to k_bmp280_reg_ctrl_meas (0xF4) (osrs_t=2x, osrs_p=16x, forced mode)
 * 2. Poll status register k_bmp280_reg_status (0xF3) until measuring bit (k_bmp280_status_meas_mask, bit 3) clears
 *    - Bounded by k_bmp280_poll_max iterations
 * 3. Read 6 bytes from k_bmp280_reg_press_msb (0xF7): pressure MSB/LSB/XLSB + temperature MSB/LSB/XLSB
 * 4. Assemble 20-bit ADC values:
 *    adc_P = (buf[k_bmp280_press_msb_idx] << k_bmp280_shift_msb) |
 *            (buf[k_bmp280_press_lsb_idx] << k_bmp280_shift_lsb_left) |
 *            (buf[k_bmp280_press_xlsb_idx] >> k_bmp280_shift_xlsb_right)
 *    adc_T = (buf[k_bmp280_temp_msb_idx] << k_bmp280_shift_msb) |
 *            (buf[k_bmp280_temp_lsb_idx] << k_bmp280_shift_lsb_left) |
 *            (buf[k_bmp280_temp_xlsb_idx] >> k_bmp280_shift_xlsb_right)
 * 5. Apply Bosch integer compensation algorithm (BMP280 datasheet section 4.2.3)
 *
 * @param[out] out Output data structure. Must not be NULL.
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok Measurement complete, out populated
 * @retval k_rx_err_null_ptr out is NULL
 * @retval k_rx_err_not_initialized init not called
 * @retval k_rx_err_nack I2C communication failure
 * @retval k_rx_err_timeout Status poll exceeded k_bmp280_poll_max iterations
 *
 * @pre rx_bmp280_init() succeeded
 * @pre out non-NULL
 *
 * @post out->temp_centi_degc contains calibrated temperature * 100
 * @post out->press_pa_256 contains calibrated pressure * 256
 * @post Sensor returns to sleep mode (forced measurement complete)
 *
 * @note Not thread-safe; call from single task
 * @note Measurement takes ~40 ms with 16x pressure oversampling
 * @warning Returns k_rx_err_timeout if status bit does not clear within
 *          k_bmp280_poll_max polls (indicates hardware fault)
 *
 * @see rx_bmp280_init() Must be called successfully before rx_bmp280_read()
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bmp280_read(bmp280_data_t* out);

#ifdef __cplusplus
}
#endif
