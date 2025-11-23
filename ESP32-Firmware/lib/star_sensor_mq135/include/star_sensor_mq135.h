/* lib/star_sensor_mq135/include/star_sensor_mq135.h */

#ifndef STAR_SENSOR_MQ135_H
#define STAR_SENSOR_MQ135_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/adc.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_mq135.h
 * @brief MQ135 air quality sensor driver
 *
 * This driver provides interface to the MQ135 gas sensor using ADC.
 * The MQ135 detects:
 * - CO2, NH3, NOx, Alcohol, Benzene, Smoke
 * - Operating range: 10-1000 ppm (various gases)
 * - Requires 24-48h preheat for accurate readings
 *
 * Calibration:
 * - R0 must be calibrated in clean air (approximately 397 ppm CO2)
 * - Rs/R0 ratio is used to calculate gas concentration
 * - Temperature and humidity compensation recommended
 *
 * Conversion formulas:
 * - Rs = [(VC * RL) / VRL] - RL
 * - PPM = a * (Rs/R0)^b  (where a, b are gas-specific constants)
 */

#define MQ135_CLEAN_AIR_RATIO (3.6f)    // Rs/R0 in clean air (397 ppm CO2)
#define MQ135_RL_VALUE (10000.0f)       // Load resistance in ohms (10kΩ typical)
#define MQ135_VCC (3.3f)                // Supply voltage
#define MQ135_ADC_RESOLUTION (4095.0f)  // 12-bit ADC

// Gas curve parameters (a, b for PPM = a * (Rs/R0)^b)
#define MQ135_CO2_A (110.47f)
#define MQ135_CO2_B (-2.862f)
#define MQ135_NH3_A (102.2f)
#define MQ135_NH3_B (-2.473f)
#define MQ135_ALCOHOL_A (77.255f)
#define MQ135_ALCOHOL_B (-3.18f)

// Temperature/humidity compensation
#define MQ135_TEMP_REFERENCE (20.0f)     // Reference temperature (°C)
#define MQ135_HUMIDITY_REFERENCE (65.0f) // Reference humidity (%RH)

typedef enum {
  MQ135_GAS_CO2 = 0,
  MQ135_GAS_NH3,
  MQ135_GAS_ALCOHOL,
} mq135_gas_type_t;

typedef struct {
  adc1_channel_t adc_channel;
  adc_atten_t    attenuation;  // ADC attenuation (0-3.3V typically ADC_ATTEN_DB_11)
  float          r0;            // Sensor resistance in clean air (must be calibrated)
  float          rl;            // Load resistance in ohms (default 10kΩ)
} mq135_config_t;

typedef struct {
  float rs;        // Sensor resistance
  float ratio;     // Rs/R0 ratio
  float ppm_co2;   // CO2 concentration in ppm
  float ppm_nh3;   // NH3 concentration in ppm
  float ppm_alcohol; // Alcohol concentration in ppm
} mq135_data_t;

typedef struct mq135_handle {
  adc1_channel_t  adc_channel;
  adc_atten_t     attenuation;
  float           r0;
  float           rl;
  error_handler_t error_handler;
  bool            initialized;
  uint32_t        warmup_start_time;  // For tracking preheat period
} mq135_handle_t;

/**
 * @brief Initialize MQ135 sensor
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_init(mq135_handle_t* handle, const mq135_config_t* config);

/**
 * @brief Deinitialize MQ135 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_deinit(mq135_handle_t* handle);

/**
 * @brief Calibrate sensor in clean air to determine R0
 *
 * This should be performed in clean air (approximately 397 ppm CO2).
 * Average multiple samples over 30-60 seconds for best results.
 *
 * @param[in]  handle  Pointer to initialized handle
 * @param[in]  samples Number of samples to average (recommended 50-100)
 * @param[out] r0      Calculated R0 value
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_calibrate(mq135_handle_t* handle, uint16_t samples, float* r0);

/**
 * @brief Set R0 calibration value
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] r0     Calibrated R0 value
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_set_r0(mq135_handle_t* handle, float r0);

/**
 * @brief Read raw ADC value
 *
 * @param[in]  handle    Pointer to initialized handle
 * @param[out] adc_value Raw ADC reading (0-4095)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_read_raw(const mq135_handle_t* handle, uint16_t* adc_value);

/**
 * @brief Calculate sensor resistance from ADC value
 *
 * @param[in]  handle    Pointer to initialized handle
 * @param[in]  adc_value Raw ADC reading
 * @param[out] rs        Calculated sensor resistance in ohms
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_calculate_rs(const mq135_handle_t* handle,
                                         uint16_t              adc_value,
                                         float*                rs);

/**
 * @brief Convert Rs/R0 ratio to gas concentration
 *
 * @param[in]  ratio    Rs/R0 ratio
 * @param[in]  gas_type Type of gas to calculate
 * @param[out] ppm      Gas concentration in parts per million
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_calculate_ppm(float             ratio,
                                          mq135_gas_type_t  gas_type,
                                          float*            ppm);

/**
 * @brief Read and calculate all gas concentrations
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Sensor resistance, ratio, and gas concentrations
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_read(const mq135_handle_t* handle, mq135_data_t* data);

/**
 * @brief Apply temperature and humidity compensation
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[in]  temperature Current temperature in °C
 * @param[in]  humidity    Current relative humidity in %
 * @param[in]  rs          Uncorrected sensor resistance
 * @param[out] rs_corrected Corrected sensor resistance
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_compensate(const mq135_handle_t* handle,
                                       float                 temperature,
                                       float                 humidity,
                                       float                 rs,
                                       float*                rs_corrected);

/**
 * @brief Check if sensor is warmed up
 *
 * MQ135 requires 24-48h preheat for stable readings.
 * This tracks time since initialization.
 *
 * @param[in]  handle    Pointer to initialized handle
 * @param[out] warmed_up true if >24h since init
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_mq135_is_warmed_up(const mq135_handle_t* handle, bool* warmed_up);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_MQ135_H */
