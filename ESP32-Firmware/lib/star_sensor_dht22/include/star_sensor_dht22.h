/* lib/star_sensor_dht22/include/star_sensor_dht22.h */

#ifndef STAR_SENSOR_DHT22_H
#define STAR_SENSOR_DHT22_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_dht22.h
 * @brief DHT22 (AM2302) temperature and humidity sensor driver
 *
 * This driver provides interface to the DHT22 sensor using single-wire protocol.
 * It supports:
 * - Temperature measurement (-40 to 80°C, ±0.5°C accuracy)
 * - Humidity measurement (0-100% RH, ±2% accuracy)
 * - 16-bit resolution for both readings
 * - Checksum validation
 * - Timing-critical bit-level protocol
 *
 * Protocol timing:
 * - Start signal: Pull low 1ms, pull high 20-40us
 * - Response: Sensor pulls low 80us, high 80us
 * - Data: 40 bits (16-bit humidity, 16-bit temp, 8-bit checksum)
 * - Bit encoding: 26-28us = 0, 70us = 1
 * - Sampling rate: Max 0.5 Hz (2 second intervals)
 */

#define DHT22_DATA_BITS (40)
#define DHT22_MIN_SAMPLE_PERIOD_MS (2000)
#define DHT22_TIMEOUT_US (85)

typedef enum {
  DHT22_OK = 0,
  DHT22_ERR_TIMEOUT,
  DHT22_ERR_CHECKSUM,
  DHT22_ERR_NO_RESPONSE,
  DHT22_ERR_TOO_SOON,
} dht22_error_t;

typedef struct {
  gpio_num_t data_pin;
  bool       enable_pullup;  // Internal pull-up (typically external 4.7k-10k used)
} dht22_config_t;

typedef struct {
  float temperature_c;
  float humidity_rh;
} dht22_data_t;

typedef struct dht22_handle {
  gpio_num_t        data_pin;
  error_handler_t   error_handler;
  bool              initialized;
  uint32_t          last_read_time;  // For enforcing minimum sample period
  dht22_data_t      last_data;
} dht22_handle_t;

/**
 * @brief Initialize DHT22 sensor
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_dht22_init(dht22_handle_t* handle, const dht22_config_t* config);

/**
 * @brief Deinitialize DHT22 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_dht22_deinit(dht22_handle_t* handle);

/**
 * @brief Read temperature and humidity
 *
 * This function performs the complete DHT22 protocol sequence and validates
 * the checksum. It enforces a minimum 2-second interval between readings.
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Temperature and humidity readings
 *
 * @return ESP_OK on success, DHT22_ERR_* on protocol/checksum error
 */
esp_err_t star_sensor_dht22_read(dht22_handle_t* handle, dht22_data_t* data);

/**
 * @brief Get last successful reading without triggering new measurement
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Last successful reading
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if no reading available
 */
esp_err_t star_sensor_dht22_get_last_reading(const dht22_handle_t* handle, dht22_data_t* data);

/**
 * @brief Check if enough time has elapsed since last reading
 *
 * @param[in]  handle     Pointer to initialized handle
 * @param[out] can_read   true if ready to read
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_dht22_can_read(const dht22_handle_t* handle, bool* can_read);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_DHT22_H */
