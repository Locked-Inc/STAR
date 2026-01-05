/* lib/rx_hcsr04/inc/rx_hcsr04.h */

/**
 * @file rx_hcsr04.h
 * @brief HC-SR04 Ultrasonic Distance Sensor Driver for RX72N
 *
 * @details
 * GPIO-based driver for HC-SR04 ultrasonic distance sensors. Provides blocking
 * and non-blocking measurement APIs for obstacle detection and collision
 * avoidance applications.
 *
 * Sensor Specifications:
 * - Range: 2cm to 400cm (effective 2cm to 400cm)
 * - Accuracy: +/-3mm
 * - Trigger: 10us HIGH pulse initiates measurement
 * - Echo: Pulse width proportional to distance (58us per cm roundtrip)
 * - Supply: 5V (level shifting required for RX72N 3.3V GPIO)
 *
 * Hardware Setup:
 * - TRIG pin: GPIO output (10us pulse triggers measurement)
 * - ECHO pin: GPIO input (pulse width indicates distance)
 * - VCC: 5V supply
 * - GND: Common ground with RX72N
 *
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_HCSR04_H
#define STAR_RX72N_HCSR04_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware_pinout.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief HC-SR04 timing constants
 *
 * Timing values based on speed of sound at 20C (343 m/s).
 * Distance = (echo_us * 0.0343 cm/us) / 2 = echo_us / 58.3 cm
 */
typedef enum {
  k_hcsr04_trigger_pulse_us    = 10,    /**< Minimum trigger pulse width */
  k_hcsr04_echo_timeout_us     = 30000, /**< Max echo wait (400cm + margin) */
  k_hcsr04_min_echo_us         = 116,   /**< Min valid echo (2cm) */
  k_hcsr04_max_echo_us         = 23200, /**< Max valid echo (400cm) */
  k_hcsr04_us_per_cm_roundtrip = 58,    /**< Microseconds per cm (roundtrip) */
  k_hcsr04_measurement_gap_ms  = 60,    /**< Minimum gap between measurements */
} rx_hcsr04_timing_t;

/**
 * @brief HC-SR04 distance range constants
 */
typedef enum {
  k_hcsr04_min_distance_cm = 2,   /**< Minimum measurable distance */
  k_hcsr04_max_distance_cm = 400, /**< Maximum measurable distance */
} rx_hcsr04_range_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief HC-SR04 sensor configuration
 *
 * Specifies trigger and echo pin assignments using type-safe GPIO pins.
 * For board-specific default configurations, see the application-layer
 * config headers.
 */
typedef struct {
  gpio_pin_t trigger_pin; /**< Trigger pin (type-safe GPIO enum) */
  gpio_pin_t echo_pin;    /**< Echo pin (type-safe GPIO enum) */
  uint32_t   timeout_us;  /**< Measurement timeout (default: 30000us) */
} rx_hcsr04_config_t;

/**
 * @brief HC-SR04 sensor handle
 *
 * Caller allocates this structure and passes to init. Driver manages
 * internal state. Do not modify fields directly after initialization.
 */
typedef struct {
  /* Configuration (set during init) */
  gpio_pin_t trigger_pin; /**< Trigger pin (type-safe GPIO enum) */
  gpio_pin_t echo_pin;    /**< Echo pin (type-safe GPIO enum) */
  uint32_t   timeout_us;  /**< Measurement timeout in microseconds */

  /* State */
  bool initialized;        /**< True if handle is initialized */
  bool measurement_active; /**< True if async measurement in progress */

  /* Statistics */
  uint32_t measurement_count; /**< Total measurements attempted */
  uint32_t timeout_count;     /**< Measurements that timed out */
  uint32_t range_error_count; /**< Out-of-range readings (too close/far) */
} rx_hcsr04_t;

/**
 * @brief Measurement result structure
 *
 * Contains distance in multiple units and raw timing data.
 */
typedef struct {
  float    distance_cm;  /**< Distance in centimeters */
  float    distance_in;  /**< Distance in inches */
  uint32_t echo_time_us; /**< Raw echo pulse duration in microseconds */
  rx_err_t status;       /**< Measurement status (k_rx_ok or error code) */
} rx_hcsr04_result_t;

/**
 * @brief Async measurement callback function type
 *
 * Called when async measurement completes or times out.
 *
 * @param[in] handle    Sensor handle
 * @param[in] result    Measurement result
 * @param[in] user_data User context passed to rx_hcsr04_measure_async()
 */
typedef void (*rx_hcsr04_callback_t)(rx_hcsr04_t*              handle,
                                     const rx_hcsr04_result_t* result,
                                     void*                     user_data);

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize HC-SR04 sensor
 *
 * Configures GPIO pins for trigger (output) and echo (input).
 * Reserves pins via pin validator to prevent conflicts.
 *
 * @param[out] handle Handle to initialize (caller-allocated)
 * @param[in]  config Configuration with pin assignments
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or config is NULL
 * @return k_rx_err_invalid_arg if port/pin values are invalid
 * @return k_rx_err_gpio_conflict if pins already reserved
 * @return k_rx_err_invalid_state if handle already initialized
 */
rx_err_t rx_hcsr04_init(rx_hcsr04_t* handle, const rx_hcsr04_config_t* config);

/**
 * @brief Deinitialize HC-SR04 sensor
 *
 * Releases GPIO pin reservations and resets handle state.
 *
 * @param[in,out] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_hcsr04_deinit(rx_hcsr04_t* handle);

/* =============================================================================
 * Public API - Measurement
 * =============================================================================
 */

/**
 * @brief Measure distance (blocking)
 *
 * Performs a complete measurement cycle:
 * 1. Send 10us trigger pulse
 * 2. Wait for echo pulse start
 * 3. Measure echo pulse duration
 * 4. Convert to distance
 *
 * Blocks for up to timeout_us (default 30ms).
 *
 * @param[in]  handle      Sensor handle
 * @param[out] distance_cm Measured distance in centimeters
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or distance_cm is NULL
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_timeout if no echo received (object >400cm or absent)
 * @return k_rx_err_out_of_range if distance <2cm (too close)
 */
rx_err_t rx_hcsr04_measure_blocking(rx_hcsr04_t* handle, float* distance_cm);

/**
 * @brief Measure distance with full result (blocking)
 *
 * Same as rx_hcsr04_measure_blocking() but returns complete result
 * including both distance units and raw timing.
 *
 * @param[in]  handle Sensor handle
 * @param[out] result Complete measurement result
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or result is NULL
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_timeout if no echo received
 * @return k_rx_err_out_of_range if distance out of bounds
 */
rx_err_t rx_hcsr04_measure(rx_hcsr04_t* handle, rx_hcsr04_result_t* result);

/**
 * @brief Start asynchronous measurement
 *
 * Initiates measurement and invokes callback with result.
 *
 * @note Current implementation is synchronous - the callback is invoked
 *       before this function returns. True non-blocking operation with
 *       ThreadX worker thread is a planned future enhancement.
 *       See: https://github.com/Locked-Inc/STAR/issues/70
 *
 * @param[in] handle    Sensor handle
 * @param[in] callback  Completion callback (required)
 * @param[in] user_data User context passed to callback (can be NULL)
 *
 * @return k_rx_ok if measurement completed and callback invoked
 * @return k_rx_err_null_pointer if handle or callback is NULL
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_busy if measurement already in progress
 */
rx_err_t
rx_hcsr04_measure_async(rx_hcsr04_t* handle, rx_hcsr04_callback_t callback, void* user_data);

/**
 * @brief Check if async measurement is in progress
 *
 * @param[in] handle Sensor handle
 *
 * @return true if measurement in progress
 * @return false if idle or handle is NULL
 */
bool rx_hcsr04_is_busy(const rx_hcsr04_t* handle);

/**
 * @brief Cancel async measurement
 *
 * Cancels in-progress async measurement. Callback will NOT be invoked.
 *
 * @param[in] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if no measurement in progress
 */
rx_err_t rx_hcsr04_cancel(rx_hcsr04_t* handle);

/* =============================================================================
 * Public API - Utilities
 * =============================================================================
 */

/**
 * @brief Convert distance from centimeters to inches
 *
 * @param[in] distance_cm Distance in centimeters
 *
 * @return Distance in inches
 */
float rx_hcsr04_cm_to_inches(float distance_cm);

/**
 * @brief Convert echo time to distance in centimeters
 *
 * Uses formula: distance = (echo_us * speed_of_sound) / 2
 * Speed of sound at 20C = 343 m/s = 0.0343 cm/us
 *
 * @note This function assumes 20°C. For temperature compensation, use
 *       rx_hcsr04_echo_to_cm_with_temp() instead.
 *
 * @param[in] echo_time_us Echo pulse duration in microseconds
 *
 * @return Distance in centimeters
 */
float rx_hcsr04_echo_to_cm(uint32_t echo_time_us);

/**
 * @brief Convert echo time to distance with temperature compensation
 *
 * Calculates distance using temperature-compensated speed of sound.
 * Speed of sound varies with temperature: v = 331.3 + (0.606 * temp_c) m/s
 *
 * Temperature compensation improves accuracy by ~0.17% per °C deviation from 20°C.
 *
 * Example: At 10°C, speed is ~337 m/s vs 343 m/s at 20°C (~1.75% difference)
 *
 * @param[in] echo_time_us  Echo pulse duration in microseconds
 * @param[in] temp_celsius  Ambient temperature in degrees Celsius
 *
 * @return Distance in centimeters (temperature-compensated)
 */
float rx_hcsr04_echo_to_cm_with_temp(uint32_t echo_time_us, float temp_celsius);

/**
 * @brief Calculate speed of sound at given temperature
 *
 * Uses formula: v = 331.3 + (0.606 * temp_c) m/s
 *
 * Valid temperature range: -40°C to +85°C (DS18B20 operating range)
 *
 * @param[in] temp_celsius Ambient temperature in degrees Celsius
 *
 * @return Speed of sound in m/s
 */
float rx_hcsr04_get_speed_of_sound(float temp_celsius);

/**
 * @brief Get sensor statistics
 *
 * @param[in]  handle           Sensor handle
 * @param[out] measurement_count Total measurements (can be NULL)
 * @param[out] timeout_count     Timeout count (can be NULL)
 * @param[out] range_error_count Range error count (can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_hcsr04_get_stats(const rx_hcsr04_t* handle,
                             uint32_t*          measurement_count,
                             uint32_t*          timeout_count,
                             uint32_t*          range_error_count);

/**
 * @brief Reset sensor statistics
 *
 * @param[in,out] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_hcsr04_reset_stats(rx_hcsr04_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HCSR04_H */
