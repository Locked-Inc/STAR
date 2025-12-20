/**
 * @file star_encoder_mux.h
 * @brief Multiplexed quadrature encoder driver for 74HC4052-based encoder reading
 * @details
 * Provides an interface for reading 4 quadrature encoders that share common signal lines
 * through a 74HC4052 dual 4-to-1 multiplexer. The driver sequentially switches between
 * encoders using select lines and reads position/velocity data from each encoder.
 *
 * Hardware Configuration:
 * - 74HC4052 multiplexer with 2 select lines (SEL0, SEL1)
 * - 4 quadrature encoders sharing 2 output pins (OUT_A, OUT_B)
 * - Sequential reading with configurable settling time
 *
 * Key Features:
 * - Reads all 4 encoders sequentially (typically ~80μs total with 10μs settling)
 * - Hardware-accelerated quadrature decoding via ESP32 PCNT peripheral
 * - Independent position tracking for each encoder
 * - Velocity calculation for each encoder
 * - Configurable multiplexer settling time
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_ENCODER_MUX_H
#define STAR_ENCODER_MUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_encoder.h"

/**
 * @file star_encoder_mux.h
 * @brief Multiplexed quadrature encoder driver
 *
 * This library provides support for reading 4 quadrature encoders through a
 * 74HC4052 dual 4-to-1 multiplexer. The encoders share common signal lines
 * and are read sequentially by switching the multiplexer select lines.
 *
 * Example Usage:
 * @code
 * // === Initialize Encoder Multiplexer ===
 *
 * #include "star_encoder_mux.h"
 *
 * star_encoder_mux_handle_t encoder_mux;
 * star_encoder_mux_config_t config = {
 *     .pin_sel0 = GPIO_NUM_40,          // Multiplexer select line 0
 *     .pin_sel1 = GPIO_NUM_41,          // Multiplexer select line 1
 *     .pin_out_a = GPIO_NUM_47,         // Shared channel A output
 *     .pin_out_b = GPIO_NUM_42,         // Shared channel B output
 *     .settling_time_us = 10,           // 10μs settling time after switching
 *     .filter_value = 1000,             // Glitch filter (APB clock cycles)
 *     .high_limit = 10000,              // PCNT upper limit
 *     .low_limit = -10000,              // PCNT lower limit
 * };
 *
 * esp_err_t ret = star_encoder_mux_init(&encoder_mux, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init encoder mux: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Read All Encoders ===
 *
 * int32_t positions[4];
 * float velocities_rpm[4];
 *
 * // dt_ms = time since last call (e.g., 4.0ms for 250Hz control loop)
 * ret = star_encoder_mux_read_all(&encoder_mux, positions, velocities_rpm, 4.0f, 341);
 * if (ret != ESP_OK) {
 *     ESP_LOGW(TAG, "Failed to read encoders: %s", esp_err_to_name(ret));
 * }
 *
 * ESP_LOGI(TAG, "Motor 0: pos=%ld, vel=%.2f RPM", positions[0], velocities_rpm[0]);
 * ESP_LOGI(TAG, "Motor 1: pos=%ld, vel=%.2f RPM", positions[1], velocities_rpm[1]);
 * ESP_LOGI(TAG, "Motor 2: pos=%ld, vel=%.2f RPM", positions[2], velocities_rpm[2]);
 * ESP_LOGI(TAG, "Motor 3: pos=%ld, vel=%.2f RPM", positions[3], velocities_rpm[3]);
 *
 *
 * // === Reset Specific Encoder ===
 *
 * star_encoder_mux_reset(&encoder_mux, 0);  // Reset encoder 0
 *
 *
 * // === Cleanup ===
 *
 * star_encoder_mux_deinit(&encoder_mux);
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief Multiplexed encoder configuration structure
 */
typedef struct {
  gpio_num_t pin_sel0;         /**< Multiplexer select line 0 (controls bit 0) */
  gpio_num_t pin_sel1;         /**< Multiplexer select line 1 (controls bit 1) */
  gpio_num_t pin_out_a;        /**< Shared encoder channel A output */
  gpio_num_t pin_out_b;        /**< Shared encoder channel B output */
  uint32_t   settling_time_us; /**< Settling time after mux switch (μs, recommended: 10) */
  uint32_t   filter_value;     /**< Glitch filter (APB clock cycles, 0=disabled) */
  int16_t    high_limit;       /**< Upper count limit for PCNT */
  int16_t    low_limit;        /**< Lower count limit for PCNT */
} star_encoder_mux_config_t;

/**
 * @brief Number of encoders supported by the multiplexer
 */
typedef enum {
  k_encoder_count = 4, /**< 4 encoders (74HC4052 supports 4-to-1 mux) */
} encoder_count_t;

/**
 * @brief Multiplexed encoder handle structure
 *
 * Internal state for the multiplexed encoder driver. Tracks position
 * and timing for each of the 4 encoders independently.
 */
typedef struct {
  star_encoder_handle_t encoder;                      /**< Shared PCNT encoder instance */
  gpio_num_t            pin_sel0;                     /**< Select line 0 GPIO */
  gpio_num_t            pin_sel1;                     /**< Select line 1 GPIO */
  uint32_t              settling_time_us;             /**< Settling time (μs) */
  int32_t               positions[k_encoder_count];   /**< Accumulated position per encoder */
  int32_t               last_counts[k_encoder_count]; /**< Last PCNT count per encoder */
  int64_t               last_time_us;                 /**< Last read timestamp (μs) */
  bool                  initialized;                  /**< Initialization flag */
} star_encoder_mux_handle_t;

/* --- Public Function Prototypes --- */

/**
 * @brief Initialize the multiplexed encoder driver
 *
 * Configures the 74HC4052 multiplexer select lines as GPIO outputs and initializes
 * a shared PCNT encoder instance for reading the 4 multiplexed encoders.
 *
 * @param[out] handle Pointer to encoder mux handle (must not be NULL)
 * @param[in]  config Pointer to configuration structure (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid handle or config
 *   - ESP_ERR_INVALID_STATE: Already initialized
 *   - Other ESP-IDF error codes from GPIO or PCNT initialization
 */
esp_err_t star_encoder_mux_init(star_encoder_mux_handle_t*       handle,
                                const star_encoder_mux_config_t* config);

/**
 * @brief Deinitialize the multiplexed encoder driver
 *
 * Releases GPIO and PCNT resources and clears the handle.
 *
 * @param[in] handle Pointer to encoder mux handle (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid handle
 *   - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t star_encoder_mux_deinit(star_encoder_mux_handle_t* handle);

/**
 * @brief Read positions and velocities from all 4 encoders
 *
 * Sequentially switches through all 4 encoders, reading the PCNT count and
 * calculating velocity for each. This function performs the following for
 * each encoder:
 *   1. Set multiplexer select lines to choose encoder
 *   2. Wait for settling time (typically 10μs)
 *   3. Read PCNT count and update accumulated position
 *   4. Calculate velocity based on delta count and delta time
 *
 * Total execution time: ~20μs per encoder × 4 = ~80μs
 *
 * @param[in]  handle           Pointer to encoder mux handle (must not be NULL)
 * @param[out] out_positions    Array of 4 int32_t for absolute positions (must not be NULL)
 * @param[out] out_velocities_rpm Array of 4 float for velocities in RPM (must not be NULL)
 * @param[in]  dt_ms            Time since last call in milliseconds (must be > 0)
 * @param[in]  counts_per_rev   Encoder counts per revolution (must be > 0)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid handle, output arrays, or parameters
 *   - ESP_ERR_INVALID_STATE: Not initialized
 *   - Other ESP-IDF error codes from PCNT read
 */
esp_err_t star_encoder_mux_read_all(star_encoder_mux_handle_t* handle,
                                    int32_t                    out_positions[k_encoder_count],
                                    float                      out_velocities_rpm[k_encoder_count],
                                    float                      dt_ms,
                                    uint32_t                   counts_per_rev);

/**
 * @brief Reset position for a specific encoder
 *
 * Resets the accumulated position for the specified encoder to zero.
 * Useful for homing or resetting odometry.
 *
 * @param[in] handle      Pointer to encoder mux handle (must not be NULL)
 * @param[in] encoder_idx Encoder index (0-3)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid handle or encoder index
 *   - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t star_encoder_mux_reset(star_encoder_mux_handle_t* handle, uint8_t encoder_idx);

/**
 * @brief Reset positions for all encoders
 *
 * Resets the accumulated positions for all 4 encoders to zero.
 *
 * @param[in] handle Pointer to encoder mux handle (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid handle
 *   - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t star_encoder_mux_reset_all(star_encoder_mux_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_ENCODER_MUX_H */
