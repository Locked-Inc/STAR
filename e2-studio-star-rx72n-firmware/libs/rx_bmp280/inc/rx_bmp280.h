/* libs/rx_bmp280/inc/rx_bmp280.h */

/**
 * @file rx_bmp280.h
 * @brief Bosch BMP280 Barometric Pressure Sensor Driver API
 *
 * @details
 * High-level driver for the Bosch BMP280 barometric pressure and temperature
 * sensor. Uses the Bosch-recommended 32-bit integer compensation formulas to
 * convert raw 20-bit ADC values to calibrated pressure (Pa) and temperature
 * (0.01°C). An integer altitude approximation is also provided.
 *
 * **Key Features Used:**
 * - Compensated pressure output in Pascals (uint32_t, 1 Pa resolution)
 * - Compensated temperature output in 0.01°C units (int32_t)
 * - Integer altitude estimate in meters (int32_t, ±10% below 1000 m ASL)
 * - One-time calibration trim read during init (24 bytes, non-volatile on chip)
 *
 * **Interface:**
 * - Protocol: I2C via SCI2 SIIC (Simple IIC) on P52/P50
 * - Address: 0x76 (default, SDO=GND); 0x77 (SDO=VDDIO)
 * - Speed: 400 kHz fast mode
 *
 * **Operating Mode After Init:**
 * - Temperature oversampling ×1, pressure oversampling ×4
 * - Normal mode (continuous measurement every 250 ms standby)
 * - IIR filter coefficient = 16 (reduces pressure noise from door slams etc.)
 *
 * **Measurement Rate:**
 * Typical ODR = 1 / (t_standby + t_measure) ≈ 1 / (250 ms + ~11 ms) ≈ 3.8 Hz.
 * The imu_task reads BMP280 at 4 Hz (every 25 × 10 ms ticks), which matches.
 *
 * @par Thread Safety
 * NOT thread-safe. The IMU task (imu_task.c) is the sole caller and provides
 * implicit serialization by accessing this driver from a single task context.
 *
 * @see rx_bmp280_constants.h Register addresses and initialization constants
 * @see rx_sci_iic.h SCI SIIC bus driver (underlying transport)
 * @see imu_task.c Consumer of this driver
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

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
 * @struct rx_bmp280_config_t
 * @brief Configuration for BMP280 driver initialization
 *
 * @details
 * Specifies which SCI SIIC channel and I2C address to use for the BMP280.
 * The underlying SIIC channel must already be initialized via sci_iic_init()
 * before calling rx_bmp280_init().
 *
 * @invariant i2c_addr must be k_bmp280_addr_default (0x76) or k_bmp280_addr_alt (0x77)
 *
 * @code
 * rx_bmp280_config_t config = {
 *     .channel  = k_imu_iic_channel,  // SCI2
 *     .i2c_addr = 0x76,               // SDO tied to GND
 * };
 * @endcode
 *
 * @see rx_bmp280_init() Consumes this config
 *
 * @since 1.0.0
 */
typedef struct {
  uint8_t channel;  /**< SCI SIIC channel number (e.g., 2 for SCI2) */
  uint8_t i2c_addr; /**< 7-bit I2C address: 0x76 (default) or 0x77 (alt) */
} rx_bmp280_config_t;

/**
 * @struct rx_bmp280_data_t
 * @brief Compensated sensor output from one BMP280 read cycle
 *
 * @details
 * All values are the result of applying the Bosch 32-bit integer compensation
 * formulas to the raw 20-bit ADC samples. Temperature must always be computed
 * before pressure because the compensation shares an intermediate variable
 * (t_fine) internally in the driver.
 *
 * @invariant pressure_pa is valid when rx_bmp280_read() returns k_rx_ok
 * @invariant temperature_cdegc is valid when rx_bmp280_read() returns k_rx_ok
 * @invariant altitude_m is valid when rx_bmp280_read() returns k_rx_ok
 *
 * @code
 * rx_bmp280_data_t baro;
 * if (rx_bmp280_read(&baro) == k_rx_ok) {
 *     float temp_c = (float)baro.temperature_cdegc / 100.0f;
 *     float press_hpa = (float)baro.pressure_pa / 100.0f;
 * }
 * @endcode
 *
 * @see rx_bmp280_read() Fills this struct on success
 *
 * @since 1.0.0
 */
typedef struct {
  uint32_t pressure_pa;       /**< Compensated pressure in Pascals (Pa). Range: ~87000–108000 Pa at sea level */
  int32_t  temperature_cdegc; /**< Compensated temperature in 0.01°C. Range: -4000 to +8500 (−40°C to +85°C) */
  int32_t  altitude_m;        /**< Integer altitude estimate in meters. Formula: (P0−P)×84/1000. ±10% below 1000 m */
} rx_bmp280_data_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BMP280 and configure for continuous pressure measurement
 *
 * @details
 * Performs the full BMP280 initialization sequence:
 * 1. Verify chip ID register (0xD0) reads 0x58
 * 2. Issue soft reset (write 0xB6 to register 0xE0), wait 5 ms
 * 3. Read 24-byte calibration trim parameters (registers 0x88–0x9F)
 *    and store them in internal static state for all subsequent compensation calls
 * 4. Write ctrl_meas = 0x2F: osrs_t=×1, osrs_p=×4, mode=normal (continuous)
 * 5. Write config   = 0x70: t_sb=250 ms, IIR filter coefficient=16, SPI3w=off
 *
 * @param[in] config BMP280 configuration (channel + I2C address)
 *
 * @return rx_err_t Status
 * @retval k_rx_ok              Initialization successful; sensor in normal mode
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_not_found   Chip ID mismatch (sensor not present or wrong device)
 * @retval k_rx_err_timeout     I2C communication timeout
 * @retval k_rx_err_nack          NACK from BMP280 (device not responding)
 *
 * @pre config != NULL
 * @pre sci_iic_init(config->channel) has been called successfully
 * @pre config->i2c_addr is 0x76 or 0x77
 * @post Calibration trim parameters loaded into driver static state on k_rx_ok
 * @post BMP280 in normal measurement mode; rx_bmp280_read() safe to call on k_rx_ok
 *
 * @note Not thread-safe. Call once during IMU task startup.
 * @warning BMP280 first measurement may be incomplete; allow one ODR cycle (~270 ms) before first read
 *
 * @see rx_bmp280_read() Read compensated data after successful init
 *
 * @since 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 3 preconditions, 2 postconditions
 */
[[nodiscard]] rx_err_t rx_bmp280_init(const rx_bmp280_config_t* config);

/**
 * @brief Read and compensate pressure, temperature, and altitude from BMP280
 *
 * @details
 * Performs one I2C burst read of 6 bytes from register 0xF7 (press_msb through
 * temp_xlsb), assembles the two 20-bit ADC values, and applies the Bosch
 * 32-bit integer compensation formulas:
 *
 * 1. Assemble raw ADC values:
 *    - adc_T = (temp_msb << 12) | (temp_lsb << 4) | (temp_xlsb >> 4)
 *    - adc_P = (press_msb << 12) | (press_lsb << 4) | (press_xlsb >> 4)
 *
 * 2. Temperature compensation (Bosch BMP280 datasheet 8.2, returns t_fine):
 *    - Uses dig_T1, dig_T2, dig_T3 trim parameters
 *    - Output: temperature_cdegc = t_fine / 5120 (in 0.01°C units: divide by 512/100)
 *
 * 3. Pressure compensation (Bosch BMP280 datasheet 8.2, 32-bit integer):
 *    - Uses dig_P1..dig_P9 trim parameters and t_fine from step 2
 *    - Output: pressure_pa in Pa
 *
 * 4. Altitude estimate (linear approximation valid below 1000 m ASL):
 *    - altitude_m = (101325 - pressure_pa) * 84 / 1000
 *
 * @param[out] data Pointer to structure to fill with compensated sensor data
 *
 * @return rx_err_t Status
 * @retval k_rx_ok              All data read and compensated successfully
 * @retval k_rx_err_null_ptr    data is NULL
 * @retval k_rx_err_invalid_state rx_bmp280_init() has not been called
 * @retval k_rx_err_timeout     I2C polling timeout
 * @retval k_rx_err_nack          NACK from BMP280
 *
 * @pre rx_bmp280_init() returned k_rx_ok
 * @pre data != NULL
 * @post data->pressure_pa filled with compensated pressure on k_rx_ok
 * @post data->temperature_cdegc filled with compensated temperature on k_rx_ok
 * @post data->altitude_m filled with altitude estimate on k_rx_ok
 *
 * @note Not thread-safe.
 * @note Execution time: ~1 I2C transaction × ~0.15 ms (6 bytes at 400 kHz) + compensation math
 *
 * @see rx_bmp280_init() Must be called first
 * @see rx_bmp280_data_t Output data structure
 *
 * @since 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 2 preconditions, 3 postconditions
 */
[[nodiscard]] rx_err_t rx_bmp280_read(rx_bmp280_data_t* data);

#ifdef __cplusplus
}
#endif
