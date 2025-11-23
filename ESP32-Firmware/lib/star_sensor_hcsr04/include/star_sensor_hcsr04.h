/* lib/star_sensor_hcsr04/include/star_sensor_hcsr04.h */

#ifndef STAR_SENSOR_HCSR04_H
#define STAR_SENSOR_HCSR04_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_hcsr04.h
 * @brief HC-SR04 ultrasonic distance sensor driver
 *
 * This driver provides interface to the HC-SR04 ultrasonic sensor.
 * It supports:
 * - Distance measurement (2-400 cm)
 * - Temperature compensation
 * - Timeout protection
 * - ISR-based echo timing
 */

#define HCSR04_MIN_DISTANCE_CM (2)
#define HCSR04_MAX_DISTANCE_CM (400)
#define HCSR04_SPEED_OF_SOUND_CM_US (0.0343f)
#define HCSR04_TIMEOUT_US (23200)  // Max echo time for 400cm

typedef enum {
  HCSR04_OK = 0,
  HCSR04_ERR_TIMEOUT,
  HCSR04_ERR_OUT_OF_RANGE_MIN,
  HCSR04_ERR_OUT_OF_RANGE_MAX,
  HCSR04_ERR_INVALID_PULSE,
  HCSR04_ERR_NOT_READY,
} hcsr04_error_t;

typedef struct {
  gpio_num_t trigger_pin;
  gpio_num_t echo_pin;
  float      temperature_c;  // For speed of sound correction
} hcsr04_config_t;

typedef struct hcsr04_handle {
  gpio_num_t        trigger_pin;
  gpio_num_t        echo_pin;
  float             temperature_c;
  error_handler_t   error_handler;
  bool              initialized;
  volatile uint32_t echo_start_time;
  volatile uint32_t echo_end_time;
  volatile bool     measurement_complete;
  volatile bool     timeout_occurred;
} hcsr04_handle_t;

/**
 * @brief Initialize HC-SR04 sensor
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_init(hcsr04_handle_t* handle, const hcsr04_config_t* config);

/**
 * @brief Deinitialize HC-SR04 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_deinit(hcsr04_handle_t* handle);

/**
 * @brief Trigger measurement and read distance
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] distance_cm Distance in centimeters
 *
 * @return ESP_OK on success, HCSR04_ERR_* on measurement error
 */
esp_err_t star_sensor_hcsr04_read_distance(hcsr04_handle_t* handle, float* distance_cm);

/**
 * @brief Trigger measurement (non-blocking)
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_trigger(hcsr04_handle_t* handle);

/**
 * @brief Check if measurement is complete
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[out] complete true if measurement done
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_is_complete(const hcsr04_handle_t* handle, bool* complete);

/**
 * @brief Get measurement result (non-blocking)
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] distance_cm Distance in centimeters
 *
 * @return ESP_OK on success, HCSR04_ERR_NOT_READY if not complete
 */
esp_err_t star_sensor_hcsr04_get_result(hcsr04_handle_t* handle, float* distance_cm);

/**
 * @brief Set temperature for compensation
 *
 * @param[in] handle        Pointer to initialized handle
 * @param[in] temperature_c Temperature in Celsius
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_set_temperature(hcsr04_handle_t* handle, float temperature_c);

/**
 * @brief Calculate distance with temperature compensation
 *
 * @param[in]  echo_time_us  Echo pulse width in microseconds
 * @param[in]  temperature_c Temperature in Celsius
 * @param[out] distance_cm   Distance in centimeters
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_calculate_distance(uint32_t echo_time_us,
                                                float    temperature_c,
                                                float*   distance_cm);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_HCSR04_H */
