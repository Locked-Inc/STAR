/* lib/rx_drv8243/inc/rx_drv8243.h */

/**
 * @file rx_drv8243.h
 * @brief DRV8243 H-bridge motor driver integration API for RX72N
 * @details
 * Provides an integration layer for the Texas Instruments DRV8243 H-bridge motor driver.
 * Combines rx_motor for PWM control with the bus manager for current sensing via ADC
 * and fault detection via GPIO. Includes current limiting and comprehensive protection features.
 *
 * Port of star_drv8243 from ESP32 to RX72N platform.
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
 * - RX72N MTU PWM_A -> DRV8243 PH (Phase/Direction)
 * - RX72N MTU PWM_B -> DRV8243 EN (Enable/Speed)
 * - DRV8243 IPROPI -> RX72N ADC (Current sense)
 * - DRV8243 nFAULT -> RX72N GPIO (Fault detect)
 *
 * Example Usage:
 * @code
 * // Initialize DRV8243 driver
 * rx_drv8243_handle_t motor_driver;
 * rx_drv8243_config_t config = {
 *     .bus_manager = &bus_manager,
 *     .gpio_bus_name = "gpio_bus",
 *     .adc_bus_name = "adc_bus",
 *     .mtu_channel = k_mtu_channel_3,
 *     .output_ph = k_mtu_output_a,
 *     .output_en = k_mtu_output_b,
 *     .pin_ipropi = k_adc_channel_0,
 *     .port_nfault = 3, .pin_nfault = 2,  // PORT3.2
 *     .pwm_freq_hz = 20000,
 *     .current_limit_ma = 2000,
 *     .ki_propi = 525,
 * };
 * rx_drv8243_init(&motor_driver, &config);
 *
 * // Control motor
 * rx_drv8243_set_speed(&motor_driver, 50.0f);   // 50% forward
 * rx_drv8243_set_speed(&motor_driver, -75.0f);  // 75% reverse
 * rx_drv8243_stop(&motor_driver, true);         // Brake
 *
 * // Monitor current and faults
 * float current_ma;
 * rx_drv8243_read_current(&motor_driver, &current_ma);
 *
 * bool fault;
 * rx_drv8243_get_fault_status(&motor_driver, &fault);
 *
 * // Cleanup
 * rx_drv8243_deinit(&motor_driver);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX_DRV8243_H
#define STAR_RX_DRV8243_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"
#include "rx_motor.h"

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief DRV8243 fault type enumeration
 */
typedef enum {
  k_rx_drv8243_fault_none         = 0, /**< No fault detected */
  k_rx_drv8243_fault_overcurrent  = 1, /**< Overcurrent protection triggered */
  k_rx_drv8243_fault_thermal      = 2, /**< Thermal shutdown (TSD) */
  k_rx_drv8243_fault_undervoltage = 3, /**< Undervoltage lockout (UVLO) */
  k_rx_drv8243_fault_overvoltage  = 4, /**< Overvoltage protection (OVP) */
  k_rx_drv8243_fault_unknown      = 5, /**< Unknown/unidentified fault */
} rx_drv8243_fault_t;

/**
 * @brief DRV8243 configuration structure
 */
typedef struct {
  rx_bus_manager_t* bus_manager;   /**< Bus manager instance (required) */
  const char*       gpio_bus_name; /**< GPIO bus name for fault pin (required) */
  const char*       adc_bus_name;  /**< ADC bus name for current sense (required) */

  /* Motor control configuration */
  rx_mtu_channel_t mtu_channel; /**< MTU channel for PWM */
  rx_mtu_output_t  output_ph;   /**< PWM output for phase/direction (MTIOC) */
  rx_mtu_output_t  output_en;   /**< PWM output for enable/speed (MTIOC) */

  /* Monitoring pins */
  uint8_t pin_ipropi;  /**< Current sense ADC channel (0-7) */
  uint8_t port_nfault; /**< Fault detect GPIO port number */
  uint8_t pin_nfault;  /**< Fault detect GPIO pin number */

  /* Motor control parameters */
  uint32_t pwm_freq_hz;  /**< PWM frequency in Hz (max 25 kHz recommended) */
  uint32_t dead_time_ns; /**< H-bridge dead-time (prevents shoot-through) */

  /* Current sensing parameters */
  uint16_t current_limit_ma; /**< Software current limit in mA (0 = disabled) */
  uint16_t ki_propi;         /**< IPROPI ratio in A/V (0 = default 525) */
} rx_drv8243_config_t;

/**
 * @brief DRV8243 driver handle structure
 */
typedef struct {
  rx_bus_manager_t* bus_manager;   /**< Bus manager reference (not owned) */
  const char*       gpio_bus_name; /**< GPIO bus name (not owned) */
  const char*       adc_bus_name;  /**< ADC bus name (not owned) */

  rx_motor_handle_t motor; /**< Underlying MTU motor control handle */

  /* Pin assignments */
  uint8_t pin_ipropi;  /**< Current sense ADC channel (0-7) */
  uint8_t port_nfault; /**< Fault detect GPIO port number */
  uint8_t pin_nfault;  /**< Fault detect GPIO pin number */

  /* Configuration */
  uint16_t current_limit_ma; /**< Software current limit in mA */
  uint16_t ki_propi;         /**< IPROPI current sense ratio (A/V) */

  /* Runtime state */
  float current_speed; /**< Last commanded speed (-100.0 to +100.0 percent) */
  bool  fault_active;  /**< Last known fault status from nFAULT pin */
  bool  initialized;   /**< True after successful init, false after deinit */
} rx_drv8243_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize DRV8243 motor driver
 *
 * @param[out] handle Pointer to DRV8243 handle structure. Must not be NULL.
 * @param[in]  config Pointer to DRV8243 configuration. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or config is NULL
 * @return k_rx_err_invalid_arg if configuration is invalid
 * @return k_rx_err_invalid_state if motor initialization fails
 */
rx_err_t rx_drv8243_init(rx_drv8243_handle_t* handle, const rx_drv8243_config_t* config);

/**
 * @brief Deinitialize DRV8243 motor driver
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_deinit(rx_drv8243_handle_t* handle);

/**
 * @brief Set motor speed and direction
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] speed  Speed percentage (-100.0 to +100.0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized or fault active
 */
rx_err_t rx_drv8243_set_speed(rx_drv8243_handle_t* handle, float speed);

/**
 * @brief Stop motor
 *
 * @param[in] handle Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] brake  True for brake, false for coast
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_stop(rx_drv8243_handle_t* handle, bool brake);

/**
 * @brief Read motor current
 *
 * @param[in]  handle      Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_current Pointer to store current in milliamps. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or out_current is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_read_current(rx_drv8243_handle_t* handle, float* out_current);

/**
 * @brief Get fault status
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_fault Pointer to store fault status. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or out_fault is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_get_fault_status(rx_drv8243_handle_t* handle, bool* out_fault);

/**
 * @brief Get current motor speed
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[out] out_speed Pointer to store speed percentage. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or out_speed is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_get_speed(const rx_drv8243_handle_t* handle, float* out_speed);

/**
 * @brief Set current limit
 *
 * @param[in] handle   Pointer to initialized DRV8243 handle. Must not be NULL.
 * @param[in] limit_ma Current limit in milliamps (0 = disabled)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_drv8243_set_current_limit(rx_drv8243_handle_t* handle, uint16_t limit_ma);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_DRV8243_H */
