/**
 * @file bmp280_driver.h
 * @brief BMP280 Pressure & Temperature Sensor Driver Header for ESP32-IDF
 *
 * Comprehensive driver for Bosch Sensortec BMP280 barometric pressure and
 * temperature sensor with pressure/temperature compensation, altitude calculation,
 * and thread-safe I2C operations on shared sensor bus.
 *
 * Author: Sensor Driver Implementation
 * Date: 2024
 * Target: ESP32-IDF v4.4+ with FreeRTOS
 */

#ifndef BMP280_DRIVER_H
#define BMP280_DRIVER_H

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

#define BMP280_I2C_ADDR_DEFAULT     0x77    /**< Default I2C address (SDO = 3.3V) */
#define BMP280_I2C_ADDR_ALT         0x76    /**< Alternative address (SDO = GND) */
#define BMP280_CHIP_ID_VALUE        0x58    /**< Expected chip ID */

/* ===== REGISTER MAP ===== */

#define BMP280_ID_ADDR              0xD0    /**< Chip ID register */
#define BMP280_RESET_ADDR           0xE0    /**< Soft reset register */
#define BMP280_STATUS_ADDR          0xF3    /**< Status register */
#define BMP280_CTRL_MEAS_ADDR       0xF4    /**< Control and measurement settings */
#define BMP280_CONFIG_ADDR          0xF5    /**< Configuration register */
#define BMP280_PRESS_MSB_ADDR       0xF7    /**< Pressure data (MSB) */
#define BMP280_PRESS_LSB_ADDR       0xF8    /**< Pressure data (LSB) */
#define BMP280_PRESS_XLSB_ADDR      0xF9    /**< Pressure data (XLSB) */
#define BMP280_TEMP_MSB_ADDR        0xFA    /**< Temperature data (MSB) */
#define BMP280_TEMP_LSB_ADDR        0xFB    /**< Temperature data (LSB) */
#define BMP280_TEMP_XLSB_ADDR       0xFC    /**< Temperature data (XLSB) */

/* Calibration Coefficients (Burst read 0x88-0xA0) */
#define BMP280_CALIB_START_ADDR     0x88    /**< Start of calibration data */
#define BMP280_CALIB_SIZE           24      /**< Size of calibration data */

#define BMP280_T1_ADDR              0x88    /**< Temperature calibration T1 */
#define BMP280_T2_ADDR              0x8A    /**< Temperature calibration T2 */
#define BMP280_T3_ADDR              0x8C    /**< Temperature calibration T3 */
#define BMP280_P1_ADDR              0x8E    /**< Pressure calibration P1 */
#define BMP280_P2_ADDR              0x90    /**< Pressure calibration P2 */
#define BMP280_P3_ADDR              0x92    /**< Pressure calibration P3 */
#define BMP280_P4_ADDR              0x94    /**< Pressure calibration P4 */
#define BMP280_P5_ADDR              0x96    /**< Pressure calibration P5 */
#define BMP280_P6_ADDR              0x98    /**< Pressure calibration P6 */
#define BMP280_P7_ADDR              0x9A    /**< Pressure calibration P7 */
#define BMP280_P8_ADDR              0x9C    /**< Pressure calibration P8 */
#define BMP280_P9_ADDR              0x9E    /**< Pressure calibration P9 */

/* ===== CONTROL REGISTER BIT DEFINITIONS ===== */

/**
 * @brief Oversampling settings
 * Values for both temperature and pressure oversampling (CTRL_MEAS register)
 */
typedef enum {
    BMP280_OVERSAMPLE_X0    = 0x0,      /**< No oversampling (skipped) */
    BMP280_OVERSAMPLE_X1    = 0x1,      /**< 1x oversampling */
    BMP280_OVERSAMPLE_X2    = 0x2,      /**< 2x oversampling */
    BMP280_OVERSAMPLE_X4    = 0x3,      /**< 4x oversampling */
    BMP280_OVERSAMPLE_X8    = 0x4,      /**< 8x oversampling */
    BMP280_OVERSAMPLE_X16   = 0x5,      /**< 16x oversampling */
} bmp280_oversampling_t;

/**
 * @brief Power modes
 */
typedef enum {
    BMP280_MODE_SLEEP       = 0x0,      /**< Sleep mode (no measurements) */
    BMP280_MODE_FORCED      = 0x1,      /**< Forced mode (single measurement) */
    BMP280_MODE_NORMAL      = 0x3,      /**< Normal mode (continuous) */
} bmp280_mode_t;

/**
 * @brief Standby duration in NORMAL mode
 */
typedef enum {
    BMP280_STANDBY_0P5MS    = 0x0,      /**< 0.5 ms standby */
    BMP280_STANDBY_62P5MS   = 0x1,      /**< 62.5 ms standby */
    BMP280_STANDBY_125MS    = 0x2,      /**< 125 ms standby */
    BMP280_STANDBY_250MS    = 0x3,      /**< 250 ms standby */
    BMP280_STANDBY_500MS    = 0x4,      /**< 500 ms standby */
    BMP280_STANDBY_1000MS   = 0x5,      /**< 1000 ms standby */
    BMP280_STANDBY_2000MS   = 0x6,      /**< 2000 ms standby */
    BMP280_STANDBY_4000MS   = 0x7,      /**< 4000 ms standby */
} bmp280_standby_duration_t;

/**
 * @brief IIR filter coefficient
 */
typedef enum {
    BMP280_FILTER_OFF       = 0x0,      /**< Filter off */
    BMP280_FILTER_2         = 0x1,      /**< IIR filter coefficient 2 */
    BMP280_FILTER_4         = 0x2,      /**< IIR filter coefficient 4 */
    BMP280_FILTER_8         = 0x3,      /**< IIR filter coefficient 8 */
    BMP280_FILTER_16        = 0x4,      /**< IIR filter coefficient 16 */
} bmp280_filter_t;

/* ===== DATA STRUCTURES ===== */

/**
 * @brief Calibration coefficients (unique per sensor)
 * Retrieved from manufacturer EEPROM at addresses 0x88-0xA0
 */
typedef struct {
    /* Temperature calibration (3 coefficients) */
    uint16_t T1;        /**< Temperature calibration T1 (unsigned) */
    int16_t T2;         /**< Temperature calibration T2 (signed) */
    int16_t T3;         /**< Temperature calibration T3 (signed) */

    /* Pressure calibration (9 coefficients) */
    uint16_t P1;        /**< Pressure calibration P1 (unsigned) */
    int16_t P2;         /**< Pressure calibration P2 (signed) */
    int16_t P3;         /**< Pressure calibration P3 (signed) */
    int16_t P4;         /**< Pressure calibration P4 (signed) */
    int16_t P5;         /**< Pressure calibration P5 (signed) */
    int16_t P6;         /**< Pressure calibration P6 (signed) */
    int16_t P7;         /**< Pressure calibration P7 (signed) */
    int16_t P8;         /**< Pressure calibration P8 (signed) */
    int16_t P9;         /**< Pressure calibration P9 (signed) */
} bmp280_calibration_t;

/**
 * @brief Raw ADC measurement data (20-bit)
 */
typedef struct {
    int32_t adc_temperature;    /**< Raw temperature ADC value (20-bit) */
    int32_t adc_pressure;       /**< Raw pressure ADC value (20-bit) */
    int32_t t_fine;             /**< Fine resolution temperature (used in compensation) */
} bmp280_raw_measurement_t;

/**
 * @brief Compensated measurement data
 */
typedef struct {
    float temperature_c;        /**< Temperature in Celsius */
    float pressure_pa;          /**< Pressure in Pascals */
    float pressure_hpa;         /**< Pressure in hectoPascals */
    float altitude_m;           /**< Calculated altitude in meters (if reference pressure provided) */
} bmp280_measurement_t;

/**
 * @brief Driver configuration structure
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;        /**< I2C bus handle from i2c_new_master_bus() */
    uint8_t i2c_addr;                       /**< I2C slave address (0x76 or 0x77) */
    uint32_t i2c_clock_hz;                  /**< I2C clock frequency (100k, 400k) */
    bmp280_oversampling_t temp_oversample;  /**< Temperature oversampling */
    bmp280_oversampling_t press_oversample; /**< Pressure oversampling */
    bmp280_filter_t filter_coeff;           /**< IIR filter coefficient */
    bmp280_standby_duration_t standby_dur;  /**< Standby duration in NORMAL mode */
    bool use_mutex;                         /**< Enable mutex for multi-task safety */
    float sea_level_pressure_hpa;           /**< Reference sea level pressure for altitude calc */
} bmp280_config_t;

/**
 * @brief Driver handle (opaque to user)
 */
typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t i2c_addr;
    SemaphoreHandle_t mutex;
    bool initialized;
    bmp280_calibration_t calib;
    float sea_level_pressure_hpa;
} bmp280_driver_handle_t;

/* ===== FUNCTION DECLARATIONS ===== */

/**
 * @brief Initialize BMP280 driver
 *
 * @param[out] handle      Driver handle to be filled
 * @param[in]  config      Initialization configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Reads and stores calibration coefficients automatically
 * @note Does not start measurements; call bmp280_set_mode() to begin
 */
esp_err_t bmp280_init(bmp280_driver_handle_t *handle, const bmp280_config_t *config);

/**
 * @brief Deinitialize driver and release resources
 *
 * @param[in] handle   Driver handle
 *
 * @return ESP_OK on success
 */
esp_err_t bmp280_deinit(bmp280_driver_handle_t *handle);

/**
 * @brief Set measurement mode
 *
 * @param[in] handle   Driver handle
 * @param[in] mode     Measurement mode (SLEEP, FORCED, NORMAL)
 *
 * @return ESP_OK on success
 */
esp_err_t bmp280_set_mode(bmp280_driver_handle_t *handle, bmp280_mode_t mode);

/**
 * @brief Read raw ADC measurements
 *
 * @param[in]  handle   Driver handle
 * @param[out] raw      Raw measurement data
 *
 * @return ESP_OK on success
 *
 * @note Verifies STATUS.measuring bit before reading (waits if still measuring)
 * @warning Temperature MUST be read first for pressure compensation algorithm
 */
esp_err_t bmp280_read_raw(bmp280_driver_handle_t *handle,
                         bmp280_raw_measurement_t *raw);

/**
 * @brief Read and compensate temperature
 *
 * @param[in]  handle              Driver handle
 * @param[out] temperature_c       Compensated temperature in Celsius
 * @param[out] t_fine_for_pressure Intermediate value needed for pressure compensation
 *
 * @return ESP_OK on success
 *
 * @note MUST call this before reading pressure (pressure compensation depends on t_fine)
 * @note If only pressure is needed, still call this function first
 */
esp_err_t bmp280_read_temperature(bmp280_driver_handle_t *handle,
                                 float *temperature_c,
                                 int32_t *t_fine_for_pressure);

/**
 * @brief Read and compensate pressure
 *
 * @param[in]  handle       Driver handle
 * @param[in]  t_fine       Fine resolution temperature from bmp280_read_temperature()
 * @param[out] pressure_pa  Compensated pressure in Pascals
 * @param[out] pressure_hpa Compensated pressure in hectoPascals
 *
 * @return ESP_OK on success
 *
 * @warning Must call bmp280_read_temperature() first to obtain t_fine parameter
 */
esp_err_t bmp280_read_pressure(bmp280_driver_handle_t *handle,
                              int32_t t_fine,
                              float *pressure_pa,
                              float *pressure_hpa);

/**
 * @brief Read complete measurement (temperature + pressure + altitude)
 *
 * @param[in]  handle       Driver handle
 * @param[out] measurement  Complete compensated measurement data
 *
 * @return ESP_OK on success
 *
 * @note Convenience function that reads temperature, pressure, and calculates altitude
 * @note Handles t_fine internally
 */
esp_err_t bmp280_read_measurement(bmp280_driver_handle_t *handle,
                                 bmp280_measurement_t *measurement);

/**
 * @brief Calculate altitude from pressure
 *
 * @param[in] pressure_hpa              Measured pressure in hectoPascals
 * @param[in] sea_level_pressure_hpa    Reference sea level pressure
 *
 * @return Altitude in meters, or NAN if input invalid
 *
 * @note Formula: h = 44330 * (1 - (p/p0)^0.1903)
 * @note Accuracy highly dependent on accurate reference pressure
 * @note Typical error: ±5-10 meters without precise reference pressure
 *
 * @warning Reference pressure MUST match your location and current day weather
 * @warning Standard sea level pressure (1013.25 hPa) may be ±30+ meters off
 */
float bmp280_calculate_altitude(float pressure_hpa, float sea_level_pressure_hpa);

/**
 * @brief Validate pressure measurement is within valid range
 *
 * @param[in] pressure_hpa   Pressure value to validate
 *
 * @return true if pressure is in valid range (300-1100 hPa), false otherwise
 *
 * @note Valid range: 300-1100 hPa
 */
bool bmp280_validate_pressure(float pressure_hpa);

/**
 * @brief Validate altitude calculation (detect unrealistic values)
 *
 * @param[in] pressure_hpa           Current measured pressure
 * @param[in] reference_pressure_hpa Reference sea level pressure
 *
 * @return Calculated altitude if valid, NAN if inputs are unrealistic
 *
 * @note Additional validation beyond simple altitude formula
 * @note Checks pressure ratio sanity and altitude magnitude bounds
 */
float bmp280_calculate_altitude_safe(float pressure_hpa,
                                    float reference_pressure_hpa);

/**
 * @brief Update sea level reference pressure
 *
 * @param[in] handle                Driver handle
 * @param[in] new_reference_pressure New reference pressure in hPa
 *
 * @note Used for subsequent altitude calculations
 */
void bmp280_set_sea_level_pressure(bmp280_driver_handle_t *handle,
                                  float new_reference_pressure);

/**
 * @brief Read status register
 *
 * @param[in]  handle    Driver handle
 * @param[out] status    Status register value
 *
 * @return ESP_OK on success
 *
 * @note Bit 3: measuring (1 = measurement ongoing)
 * @note Bit 0: im_update (1 = NVM write in progress)
 */
esp_err_t bmp280_read_status(bmp280_driver_handle_t *handle, uint8_t *status);

/**
 * @brief Check if measurement is in progress
 *
 * @param[in] handle   Driver handle
 *
 * @return true if measurement is ongoing, false otherwise
 *
 * @note Can be used to poll before reading to ensure fresh data
 */
bool bmp280_is_measuring(bmp280_driver_handle_t *handle);

/**
 * @brief Perform soft reset (returns to default state)
 *
 * @param[in] handle   Driver handle
 *
 * @return ESP_OK on success
 *
 * @note Soft reset value is 0xB6 (write to 0xE0)
 * @note After reset, must re-initialize driver configuration
 */
esp_err_t bmp280_soft_reset(bmp280_driver_handle_t *handle);

/**
 * @brief Compensate temperature using 32-bit algorithm
 *
 * @param[in]  adc_temp   Raw 20-bit temperature ADC value
 * @param[in]  calib      Calibration coefficients
 * @param[out] t_fine     Fine resolution temperature for pressure compensation
 *
 * @return Compensated temperature in Celsius * 100 (e.g., 2560 = 25.60°C)
 *
 * @note Internal function used by bmp280_read_temperature()
 * @note t_fine output MUST be used for pressure compensation
 */
float bmp280_compensate_temperature_32bit(int32_t adc_temp,
                                         const bmp280_calibration_t *calib,
                                         int32_t *t_fine);

/**
 * @brief Compensate pressure using 32-bit algorithm
 *
 * @param[in] adc_press   Raw 20-bit pressure ADC value
 * @param[in] t_fine      Fine resolution temperature from temperature compensation
 * @param[in] calib       Calibration coefficients
 *
 * @return Compensated pressure in Pascals
 *
 * @note 32-bit algorithm: Accuracy typically 1 Pa (1-sigma)
 * @note Internal function used by bmp280_read_pressure()
 * @warning t_fine MUST come from bmp280_compensate_temperature_32bit()
 */
uint32_t bmp280_compensate_pressure_32bit(int32_t adc_press,
                                         int32_t t_fine,
                                         const bmp280_calibration_t *calib);

#ifdef __cplusplus
}
#endif

#endif // BMP280_DRIVER_H
