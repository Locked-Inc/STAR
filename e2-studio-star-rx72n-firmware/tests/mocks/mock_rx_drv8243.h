/* tests/mocks/mock_rx_drv8243.h */

/**
 * @file mock_rx_drv8243.h
 * @brief Mock DRV8243 H-bridge driver for motor control testing
 *
 * @details
 * Provides test double for DRV8243 dual H-bridge motor driver (high-level interface)
 * to enable unit testing of motor control logic without actual motor hardware.
 *
 * Enables testing of: Motor speed/direction control, Fault detection and recovery,
 * Current limiting, Braking logic, Multi-motor coordination
 *
 * @par Mock Capabilities: Configurable return values, Call tracking, Fault injection
 * (per motor), Current simulation
 * @par Usage: tests/test_rx_drv8243.c, tests/test_rx_motor.c
 * @see rx_drv8243.h Real DRV8243 driver
 * @see mock_rx_drv8243_hw.h DRV8243 hardware layer mock
 * @par NASA Power of 10: ✓ Static allocation
 * @par SOLID: D - Motor control depends on driver interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_drv8243_constants_t
 * @brief Mock DRV8243 driver constants
 */
typedef enum : uint8_t {
  k_mock_drv8243_max_motors = 4, /**< Maximum number of motors to track */
} mock_drv8243_constants_t;

/* =============================================================================
 * Type Definitions (simplified from rx_drv8243.h)
 * =============================================================================
 */

/**
 * @enum rx_drv8243_fault_t
 * @brief DRV8243 fault types
 */
typedef enum : uint8_t {
  k_rx_drv8243_fault_none         = 0, /**< No fault */
  k_rx_drv8243_fault_overcurrent  = 1, /**< Overcurrent protection */
  k_rx_drv8243_fault_thermal      = 2, /**< Thermal shutdown */
  k_rx_drv8243_fault_undervoltage = 3, /**< Undervoltage lockout */
  k_rx_drv8243_fault_overvoltage  = 4, /**< Overvoltage protection */
  k_rx_drv8243_fault_open_load    = 5, /**< Open load (SPI only) */
  k_rx_drv8243_fault_spi_error    = 6, /**< SPI communication error */
  k_rx_drv8243_fault_unknown      = 7, /**< Unknown fault */
} rx_drv8243_fault_t;

/**
 * @struct rx_drv8243_detailed_fault_t
 * @brief Detailed fault status (SPI variant)
 */
typedef struct {
  bool spi_error;     /**< SPI frame error */
  bool por;           /**< Power-on reset */
  bool vmov;          /**< VM overvoltage */
  bool vmuv;          /**< VM undervoltage */
  bool ocp;           /**< Overcurrent protection */
  bool tsd;           /**< Thermal shutdown */
  bool ola;           /**< Open load */
  bool ocp_h1;        /**< OCP high-side OUT1 */
  bool ocp_l1;        /**< OCP low-side OUT1 */
  bool ocp_h2;        /**< OCP high-side OUT2 */
  bool ocp_l2;        /**< OCP low-side OUT2 */
  bool ola1;          /**< Open load OUT1 */
  bool ola2;          /**< Open load OUT2 */
  bool itrip_active;  /**< ITRIP active */
  bool device_active; /**< Device active */
} rx_drv8243_detailed_fault_t;

/* Forward declarations for mock - minimal config/handle */
struct rx_bus_manager_s;
typedef struct rx_bus_manager_s rx_bus_manager_t;

/**
 * @struct rx_drv8243_config_t
 * @brief DRV8243 configuration (simplified for mock)
 */
typedef struct {
  rx_bus_manager_t* bus_manager;   /**< Bus manager (unused in mock) */
  const char*       gpio_bus_name; /**< GPIO bus name */
  const char*       adc_bus_name;  /**< ADC bus name */
  uint32_t          pwm_freq_hz;   /**< PWM frequency */
  uint16_t          current_limit_ma; /**< Current limit */
  bool              use_spi_variant;  /**< Enable SPI variant */
} rx_drv8243_config_t;

/**
 * @struct rx_drv8243_handle_t
 * @brief DRV8243 driver handle (simplified for mock)
 */
typedef struct {
  float    current_speed;    /**< Last commanded speed */
  float    current_ma;       /**< Simulated current reading */
  bool     fault_active;     /**< Fault status */
  bool     initialized;      /**< Initialization flag */
  bool     spi_enabled;      /**< SPI variant enabled */
  uint16_t current_limit_ma; /**< Current limit */
} rx_drv8243_handle_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock DRV8243 state
 *
 * @details
 * Clears all call counts, resets return values to defaults (k_rx_ok),
 * clears fault states and current readings. Call in test setUp().
 */
void mock_drv8243_reset(void);

/**
 * @brief Set return value for rx_drv8243_init()
 *
 * @param[in] err Error code to return from rx_drv8243_init()
 */
void mock_drv8243_set_init_return(rx_err_t err);

/**
 * @brief Set return value for rx_drv8243_deinit()
 *
 * @param[in] err Error code to return from rx_drv8243_deinit()
 */
void mock_drv8243_set_deinit_return(rx_err_t err);

/**
 * @brief Set return value for rx_drv8243_set_speed()
 *
 * @param[in] err Error code to return from rx_drv8243_set_speed()
 */
void mock_drv8243_set_speed_return(rx_err_t err);

/**
 * @brief Set return value for rx_drv8243_read_current()
 *
 * @param[in] err Error code to return from rx_drv8243_read_current()
 */
void mock_drv8243_set_read_current_return(rx_err_t err);

/**
 * @brief Set return value for rx_drv8243_get_fault_status()
 *
 * @param[in] err Error code to return from rx_drv8243_get_fault_status()
 */
void mock_drv8243_set_fault_status_return(rx_err_t err);

/**
 * @brief Set simulated fault status for a motor
 *
 * @param[in] motor_idx Motor index (0-3)
 * @param[in] fault     True if fault active, false otherwise
 */
void mock_drv8243_set_fault_status(uint8_t motor_idx, bool fault);

/**
 * @brief Set simulated current reading for a motor
 *
 * @param[in] motor_idx  Motor index (0-3)
 * @param[in] current_ma Current in milliamps
 */
void mock_drv8243_set_current_reading(uint8_t motor_idx, float current_ma);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Get total number of rx_drv8243_init() calls
 *
 * @return uint32_t Number of calls
 */
uint32_t mock_drv8243_get_init_count(void);

/**
 * @brief Get total number of rx_drv8243_deinit() calls
 *
 * @return uint32_t Number of calls
 */
uint32_t mock_drv8243_get_deinit_count(void);

/**
 * @brief Get total number of rx_drv8243_set_speed() calls
 *
 * @return uint32_t Number of calls
 */
uint32_t mock_drv8243_get_set_speed_count(void);

/**
 * @brief Get total number of rx_drv8243_read_current() calls
 *
 * @return uint32_t Number of calls
 */
uint32_t mock_drv8243_get_read_current_count(void);

/**
 * @brief Get total number of rx_drv8243_get_fault_status() calls
 *
 * @return uint32_t Number of calls
 */
uint32_t mock_drv8243_get_fault_status_count(void);

/**
 * @brief Get the last speed set on a motor
 *
 * @param[in] motor_idx Motor index (0-3)
 * @return float Last speed percentage (-100 to +100)
 */
float mock_drv8243_get_last_speed(uint8_t motor_idx);

/**
 * @brief Check if any motor was initialized
 *
 * @return bool True if rx_drv8243_init() was called
 */
bool mock_drv8243_was_initialized(void);

/* =============================================================================
 * Mock DRV8243 API (replaces real implementation)
 * =============================================================================
 */

/**
 * @brief Mock rx_drv8243_init() - Initialize DRV8243 motor driver
 *
 * @param[out] handle Pointer to DRV8243 handle structure
 * @param[in]  config Pointer to DRV8243 configuration
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_init(rx_drv8243_handle_t* handle, const rx_drv8243_config_t* config);

/**
 * @brief Mock rx_drv8243_deinit() - Deinitialize DRV8243 motor driver
 *
 * @param[in] handle Pointer to initialized DRV8243 handle
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_deinit(rx_drv8243_handle_t* handle);

/**
 * @brief Mock rx_drv8243_set_speed() - Set motor speed and direction
 *
 * @param[in] handle Pointer to initialized DRV8243 handle
 * @param[in] speed  Speed percentage (-100.0 to +100.0)
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_set_speed(rx_drv8243_handle_t* handle, float speed);

/**
 * @brief Mock rx_drv8243_stop() - Stop motor
 *
 * @param[in] handle Pointer to initialized DRV8243 handle
 * @param[in] brake  True for brake, false for coast
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_stop(rx_drv8243_handle_t* handle, bool brake);

/**
 * @brief Mock rx_drv8243_read_current() - Read motor current
 *
 * @param[in]  handle      Pointer to initialized DRV8243 handle
 * @param[out] out_current Pointer to store current in milliamps
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_read_current(const rx_drv8243_handle_t* handle, float* out_current);

/**
 * @brief Mock rx_drv8243_get_fault_status() - Get fault status
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle
 * @param[out] out_fault Pointer to store fault status
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_get_fault_status(const rx_drv8243_handle_t* handle, bool* out_fault);

/**
 * @brief Mock rx_drv8243_get_speed() - Get current motor speed
 *
 * @param[in]  handle    Pointer to initialized DRV8243 handle
 * @param[out] out_speed Pointer to store speed percentage
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_get_speed(const rx_drv8243_handle_t* handle, float* out_speed);

/**
 * @brief Mock rx_drv8243_set_current_limit() - Set current limit
 *
 * @param[in] handle   Pointer to initialized DRV8243 handle
 * @param[in] limit_ma Current limit in milliamps
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_drv8243_set_current_limit(rx_drv8243_handle_t* handle, uint16_t limit_ma);

#ifdef __cplusplus
}
#endif
