/* lib/star_encoder/include/star_encoder.h */

#ifndef STAR_ENCODER_H
#define STAR_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file star_encoder.h
 * @brief Quadrature encoder driver using ESP32 PCNT peripheral
 *
 * This library provides a simple interface for reading quadrature encoders
 * using the ESP32's Pulse Counter (PCNT) peripheral. It supports position
 * tracking and velocity calculation for motor control applications.
 *
 * Key Features:
 * - Hardware-accelerated quadrature decoding
 * - Position tracking with signed 32-bit counter
 * - Velocity calculation
 * - Configurable glitch filter
 * - Counter limits and overflow detection
 *
 * Example Usage:
 * @code
 * // === Initialize Encoder ===
 *
 * #include "star_encoder.h"
 *
 * star_encoder_handle_t encoder;
 * star_encoder_config_t config = {
 *     .pin_a = GPIO_NUM_25,
 *     .pin_b = GPIO_NUM_26,
 *     .filter_value = 1000,     // 1000 APB clocks glitch filter
 *     .high_limit = 10000,      // Upper count limit
 *     .low_limit = -10000,      // Lower count limit
 * };
 *
 * esp_err_t ret = star_encoder_init(&encoder, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init encoder: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Read Position ===
 *
 * int32_t position;
 * star_encoder_get_count(&encoder, &position);
 * ESP_LOGI(TAG, "Encoder position: %ld", position);
 *
 *
 * // === Calculate Velocity ===
 *
 * // Read position at regular intervals
 * float velocity_rpm = star_encoder_get_velocity_rpm(&encoder, 10.0f, 500);
 * ESP_LOGI(TAG, "Motor speed: %.2f RPM", velocity_rpm);
 *
 *
 * // === Reset Position ===
 *
 * star_encoder_reset_count(&encoder);
 *
 *
 * // === Cleanup ===
 *
 * star_encoder_deinit(&encoder);
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief Encoder configuration structure
 */
typedef struct {
  gpio_num_t pin_a;        /**< Encoder A phase pin */
  gpio_num_t pin_b;        /**< Encoder B phase pin */
  uint32_t   filter_value; /**< Glitch filter (APB clock cycles, 0=disabled) */
  int16_t    high_limit;   /**< Upper count limit before overflow */
  int16_t    low_limit;    /**< Lower count limit before underflow */
} star_encoder_config_t;

/**
 * @brief Encoder handle structure (opaque)
 */
typedef struct {
  pcnt_unit_handle_t    unit_handle;    /**< PCNT unit handle */
  pcnt_channel_handle_t channel_a;      /**< PCNT channel A handle */
  pcnt_channel_handle_t channel_b;      /**< PCNT channel B handle */
  gpio_num_t            pin_a;          /**< Encoder A phase pin */
  gpio_num_t            pin_b;          /**< Encoder B phase pin */
  int16_t               high_limit;     /**< Upper count limit */
  int16_t               low_limit;      /**< Lower count limit */
  int32_t               last_count;     /**< Last count value for velocity calculation */
  int64_t               last_time_us;   /**< Last timestamp in microseconds */
  int32_t               overflow_count; /**< Overflow counter for extended range */
  bool                  initialized;    /**< Initialization flag */
} star_encoder_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize a quadrature encoder instance
 *
 * Configures the PCNT peripheral to decode quadrature encoder signals from
 * two GPIO pins (A and B phases).
 *
 * @param[out] handle Pointer to encoder handle structure. Must not be NULL.
 * @param[in]  config Pointer to encoder configuration. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_encoder_init(star_encoder_handle_t* handle, const star_encoder_config_t* config);

/**
 * @brief Deinitialize encoder and release resources
 *
 * @param[in] handle Pointer to initialized encoder handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_encoder_deinit(star_encoder_handle_t* handle);

/**
 * @brief Get current encoder count
 *
 * Reads the current position count from the PCNT peripheral. The count includes
 * overflow tracking for extended range beyond the 16-bit hardware counter.
 *
 * @param[in]  handle    Pointer to initialized encoder handle. Must not be NULL.
 * @param[out] out_count Pointer to store the count value. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_encoder_get_count(star_encoder_handle_t* handle, int32_t* out_count);

/**
 * @brief Reset encoder count to zero
 *
 * @param[in] handle Pointer to initialized encoder handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_encoder_reset_count(star_encoder_handle_t* handle);

/**
 * @brief Calculate encoder velocity in RPM
 *
 * Calculates the rotational velocity based on count changes over time.
 * This function should be called at regular intervals for accurate velocity measurement.
 *
 * @param[in]  handle            Pointer to initialized encoder handle. Must not be NULL.
 * @param[in]  dt_ms             Time delta in milliseconds since last call.
 * @param[in]  counts_per_rev    Encoder counts per revolution (4x resolution for quadrature).
 * @param[out] out_velocity_rpm  Pointer to store velocity in RPM. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_encoder_get_velocity_rpm(star_encoder_handle_t* handle,
                                        float                  dt_ms,
                                        uint32_t               counts_per_rev,
                                        float*                 out_velocity_rpm);

#ifdef __cplusplus
}
#endif

#endif /* STAR_ENCODER_H */
