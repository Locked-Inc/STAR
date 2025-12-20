/**
 * @file star_drv8243.h
 * @brief DRV8243 H-bridge motor driver integration API
 * @details
 * Provides an integration layer for the Texas Instruments DRV8243 H-bridge motor driver.
 * Combines star_motor for PWM control with the bus manager for current sensing via ADC
 * and fault detection via GPIO. Includes current limiting and comprehensive protection features.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_DRV8243_H
#define STAR_DRV8243_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_bus_manager.h"
#include "star_motor.h"

/**
 * @file star_drv8243.h
 * @brief DRV8243 H-Bridge Motor Driver Integration Library
 *
 * This library provides integration between the ESP32 MCPWM peripheral
 * (via star_motor), current sensing via ADC, and fault detection via GPIO
 * for the Texas Instruments DRV8243 H-bridge motor driver.
 *
 * Key Features:
 * - H-bridge motor control (one brushed DC motor)
 * - Current sensing via IPROPI pin (ADC)
 * - Fault detection via nFAULT pin (GPIO)
 * - Configurable current limiting
 * - PH/EN control mode (PWM + direction)
 * - PWM frequency: up to 25 kHz
 *
 * Hardware Connections:
 * - ESP32 PWM_A → DRV8243 PH (Phase/Direction)
 * - ESP32 PWM_B → DRV8243 EN (Enable/Speed)
 * - DRV8243 IPROPI → ESP32 ADC (Current sense)
 * - DRV8243 nFAULT → ESP32 GPIO (Fault detect)
 *
 * Example Usage:
 * @code
 * // === Initialize DRV8243 Motor Driver ===
 *
 * #include "star_drv8243.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Create GPIO bus for fault detection
 * star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
 * star_bus_manager_add_bus(&bus_manager, gpio_bus);
 *
 * // Create ADC bus for current sensing
 * star_bus_config_t* adc_bus = star_bus_config_create_adc(
 *     "adc_bus",
 *     ADC_UNIT_1,
 *     ADC_ATTEN_DB_12,
 *     ADC_BITWIDTH_12
 * );
 * star_bus_manager_add_bus(&bus_manager, adc_bus);
 *
 * // Initialize DRV8243 driver
 * star_drv8243_handle_t motor_driver;
 * star_drv8243_config_t config = {
 *     .bus_manager = &bus_manager,
 *     .gpio_bus_name = "gpio_bus",
 *     .adc_bus_name = "adc_bus",
 *     .pin_pwm_ph = GPIO_NUM_25,      // Phase control
 *     .pin_pwm_en = GPIO_NUM_26,      // Enable/Speed control
 *     .pin_ipropi = ADC_CHANNEL_6,    // Current sense (GPIO34)
 *     .pin_nfault = GPIO_NUM_35,      // Fault detect
 *     .pwm_freq_hz = 20000,           // 20 kHz PWM
 *     .current_limit_ma = 2000,       // 2A current limit
 *     .ki_propi = 525,                // IPROPI ratio (525 A/V typical)
 * };
 *
 * esp_err_t ret = star_drv8243_init(&motor_driver, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init DRV8243: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Control Motor ===
 *
 * // Set motor speed: 50% forward
 * star_drv8243_set_speed(&motor_driver, 50.0f);
 *
 * // Set motor speed: 75% reverse
 * star_drv8243_set_speed(&motor_driver, -75.0f);
 *
 * // Stop motor (coast)
 * star_drv8243_stop(&motor_driver, false);
 *
 * // Stop motor (brake)
 * star_drv8243_stop(&motor_driver, true);
 *
 *
 * // === Monitor Current and Faults ===
 *
 * // Read motor current
 * float current_ma;
 * ret = star_drv8243_read_current(&motor_driver, &current_ma);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Motor current: %.1f mA", current_ma);
 * }
 *
 * // Check fault status
 * bool fault_active;
 * ret = star_drv8243_get_fault_status(&motor_driver, &fault_active);
 * if (fault_active) {
 *     ESP_LOGW(TAG, "Motor fault detected!");
 * }
 *
 *
 * // === Cleanup ===
 *
 * star_drv8243_deinit(&motor_driver);
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief DRV8243 fault type enumeration
 *
 * These fault types correspond to DRV8243 fault conditions as indicated
 * by the nFAULT pin. The DRV8243 uses a single nFAULT output (active low)
 * that asserts on any fault condition. Without SPI diagnostic register
 * access, specific fault type identification requires external diagnostics.
 */
typedef enum {
  k_star_drv8243_fault_none         = 0, /**< No fault detected */
  k_star_drv8243_fault_overcurrent  = 1, /**< Overcurrent protection triggered */
  k_star_drv8243_fault_thermal      = 2, /**< Thermal shutdown (TSD) */
  k_star_drv8243_fault_undervoltage = 3, /**< Undervoltage lockout (UVLO) */
  k_star_drv8243_fault_overvoltage  = 4, /**< Overvoltage protection (OVP) */
  k_star_drv8243_fault_unknown      = 5, /**< Unknown/unidentified fault */
} star_drv8243_fault_t;

/**
 * @brief DRV8243 configuration structure
 *
 * Hardware Assumptions:
 * - DRV8243 operates in PH/EN (Phase/Enable) control mode
 * - PH pin controls direction, EN pin controls speed via PWM duty cycle
 * - nFAULT is active-low with internal pull-up on DRV8243; external pull-up
 *   on ESP32 GPIO is configured for reliable fault detection
 * - IPROPI outputs a voltage proportional to motor current (typical 525 A/V)
 * - The ESP32 ADC reference voltage affects current measurement accuracy
 *
 * Timing Constraints:
 * - PWM frequency should not exceed 25 kHz for optimal efficiency
 * - Dead-time prevents H-bridge shoot-through; typical 1us is safe
 * - Current sensing requires time for ADC conversion (typically 10-50us)
 */
typedef struct {
  star_bus_manager_t* bus_manager;   /**< Bus manager instance (required) */
  const char*         gpio_bus_name; /**< GPIO bus name for fault pin (required) */
  const char*         adc_bus_name;  /**< ADC bus name for current sense (required) */

  /* Motor control pins (directly connected to DRV8243) */
  int pin_pwm_ph; /**< Phase/Direction pin -> DRV8243 PH (controls H-bridge polarity) */
  int pin_pwm_en; /**< Enable/Speed pin -> DRV8243 EN (PWM duty = motor speed) */

  /* Monitoring pins (directly connected to DRV8243) */
  int pin_ipropi; /**< Current sense -> DRV8243 IPROPI (ADC channel, not GPIO number) */
  int pin_nfault; /**< Fault detect <- DRV8243 nFAULT (active-low, GPIO number) */

  /* Motor control parameters */
  uint32_t pwm_freq_hz;         /**< PWM frequency in Hz (max 25 kHz recommended) */
  uint32_t timer_resolution_hz; /**< MCPWM timer resolution (0 = default 10 MHz) */
  uint32_t dead_time_ns;        /**< H-bridge dead-time (prevents shoot-through) */

  /* Current sensing parameters */
  uint16_t current_limit_ma; /**< Software current limit in mA (0 = disabled) */
  uint16_t ki_propi;         /**< IPROPI ratio in A/V (0 = default 525 from datasheet) */

  /* MCPWM hardware selection */
  int group_id; /**< MCPWM group ID (0 or 1, ESP32 has 2 groups) */
} star_drv8243_config_t;

/**
 * @brief DRV8243 driver handle structure
 *
 * This structure maintains the runtime state of a DRV8243 motor driver instance.
 * It is allocated by the caller (typically on stack or as static storage) and
 * initialized via star_drv8243_init(). Zero dynamic allocation is used.
 *
 * Thread Safety:
 * - The handle is not inherently thread-safe
 * - Callers must provide external synchronization if accessed from multiple tasks
 * - Bus manager operations are internally mutex-protected
 */
typedef struct {
  star_bus_manager_t* bus_manager;   /**< Bus manager reference (not owned) */
  const char*         gpio_bus_name; /**< GPIO bus name (not owned) */
  const char*         adc_bus_name;  /**< ADC bus name (not owned) */

  star_motor_handle_t motor; /**< Underlying MCPWM motor control handle */

  /* Pin assignments (cached from config) */
  int pin_ipropi; /**< Current sense ADC channel number */
  int pin_nfault; /**< Fault detect GPIO pin number */

  /* Configuration (cached from config) */
  uint16_t current_limit_ma; /**< Software current limit in mA */
  uint16_t ki_propi;         /**< IPROPI current sense ratio (A/V) */

  /* Runtime state */
  float current_speed; /**< Last commanded speed (-100.0 to +100.0 percent) */
  bool  fault_active;  /**< Last known fault status from nFAULT pin */
  bool  initialized;   /**< True after successful init, false after deinit */
} star_drv8243_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize DRV8243 motor driver
 *
 * Initializes the motor driver with MCPWM for PWM generation, ADC for
 * current sensing, and GPIO for fault detection.
 *
 * @param[out] handle Pointer to DRV8243 handle structure. Must not be NULL.
 * @param[in]  config Pointer to DRV8243 configuration. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_init(star_drv8243_handle_t* handle, const star_drv8243_config_t* config);

/**
 * @brief Deinitialize DRV8243 motor driver
 *
 * Stops the motor and releases all resources.
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_deinit(star_drv8243_handle_t* handle);

/**
 * @brief Set motor speed and direction
 *
 * Sets the motor speed as a percentage from -100 (full reverse) to
 * +100 (full forward). The function will enforce current limiting if
 * configured.
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] speed  Speed percentage (-100.0 to +100.0)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_set_speed(star_drv8243_handle_t* handle, float speed);

/**
 * @brief Stop motor
 *
 * Stops the motor with either coast (low-side off) or brake (short circuit).
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] brake  True for brake, false for coast
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_stop(star_drv8243_handle_t* handle, bool brake);

/**
 * @brief Read motor current
 *
 * Reads the motor current from the IPROPI pin via ADC.
 *
 * @param[in]  handle      Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_current Pointer to store current in milliamps. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_read_current(star_drv8243_handle_t* handle, float* out_current);

/**
 * @brief Get fault status
 *
 * Reads the nFAULT pin to check for fault conditions.
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_fault Pointer to store fault status. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_get_fault_status(star_drv8243_handle_t* handle, bool* out_fault);

/**
 * @brief Get current motor speed
 *
 * Returns the currently set motor speed.
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_speed Pointer to store speed percentage. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_get_speed(const star_drv8243_handle_t* handle, float* out_speed);

/**
 * @brief Set current limit
 *
 * Updates the current limit threshold. Set to 0 to disable current limiting.
 *
 * @param[in] handle       Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] limit_ma     Current limit in milliamps (0 = disabled)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_drv8243_set_current_limit(star_drv8243_handle_t* handle, uint16_t limit_ma);

#ifdef __cplusplus
}
#endif

#endif /* STAR_DRV8243_H */
