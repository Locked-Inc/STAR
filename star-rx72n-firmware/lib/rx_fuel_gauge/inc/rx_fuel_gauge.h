/* include/rx_fuel_gauge.h */

/**
 * @file rx_fuel_gauge.h
 * @brief Fuel Gauge Driver for RX72N
 * @details
 * Generic fuel gauge driver interface using SMBUS protocol.
 * Provides battery monitoring: voltage, current, state of charge, temperature.
 *
 * Designed to work with common fuel gauge ICs like:
 * - BQ27441 (Texas Instruments)
 * - MAX17048/MAX17049 (Maxim)
 * - LC709203F (On Semiconductor)
 *
 * Uses bus manager for thread-safe SMBUS communication.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_FUEL_GAUGE_H
#define STAR_RX72N_FUEL_GAUGE_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Fuel gauge battery status
 */
typedef struct {
  uint16_t voltage_mv;        /**< Battery voltage in millivolts */
  int16_t  current_ma;        /**< Battery current in milliamps (+ charging, - discharging) */
  uint8_t  state_of_charge;   /**< State of charge in percent (0-100) */
  int16_t  temperature_c;     /**< Battery temperature in degrees Celsius */
  uint16_t capacity_mah;      /**< Remaining capacity in milliamp-hours */
  uint16_t full_capacity_mah; /**< Full charge capacity in milliamp-hours */
  uint16_t time_to_empty_min; /**< Estimated time to empty in minutes (0xFFFF if charging) */
  uint16_t time_to_full_min;  /**< Estimated time to full in minutes (0xFFFF if discharging) */
  uint16_t cycle_count;       /**< Number of charge/discharge cycles */
  bool     is_charging;       /**< True if battery is charging */
  bool     is_full;           /**< True if battery is fully charged */
  bool     is_low;            /**< True if battery is critically low */
} rx_fuel_gauge_status_t;

/**
 * @brief Fuel gauge configuration
 */
typedef struct {
  uint16_t design_capacity_mah;  /**< Battery design capacity in mAh */
  uint16_t terminate_voltage_mv; /**< Termination voltage in mV */
  uint8_t  low_soc_threshold;    /**< Low SoC threshold in percent (0-100) */
} rx_fuel_gauge_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize fuel gauge
 *
 * Configures fuel gauge with battery parameters and enables monitoring.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name for fuel gauge
 * @param[in] config Fuel gauge configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or config is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device not responding
 */
rx_err_t rx_fuel_gauge_init(rx_bus_manager_t*             manager,
                            const char*                   bus_name,
                            const rx_fuel_gauge_config_t* config);

/**
 * @brief Read battery voltage
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or voltage_mv is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_fuel_gauge_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv);

/**
 * @brief Read battery current
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] current_ma Pointer to store current in milliamps
 *                        Positive = charging, negative = discharging
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or current_ma is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_fuel_gauge_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma);

/**
 * @brief Read state of charge
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] soc_percent Pointer to store state of charge (0-100%)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or soc_percent is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_fuel_gauge_read_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent);

/**
 * @brief Read battery temperature
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] temperature_c Pointer to store temperature in degrees Celsius
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or temperature_c is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_fuel_gauge_read_temperature(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        int16_t*          temperature_c);

/**
 * @brief Read complete battery status
 *
 * Reads all available battery parameters in a single call.
 * More efficient than calling individual read functions.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] status Pointer to status structure
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or status is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_fuel_gauge_read_status(rx_bus_manager_t*       manager,
                                   const char*             bus_name,
                                   rx_fuel_gauge_status_t* status);

/**
 * @brief Set low battery threshold
 *
 * Configures the state of charge threshold for low battery warning.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] threshold_percent Low battery threshold (0-100%)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_invalid_arg if threshold_percent > 100
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 */
rx_err_t rx_fuel_gauge_set_low_threshold(rx_bus_manager_t* manager,
                                         const char*       bus_name,
                                         uint8_t           threshold_percent);

/**
 * @brief Trigger fuel gauge learning cycle
 *
 * Initiates a full charge/discharge cycle to calibrate capacity estimation.
 * Battery should be charged to full, then discharged to empty.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 */
rx_err_t rx_fuel_gauge_start_learning(rx_bus_manager_t* manager, const char* bus_name);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_FUEL_GAUGE_H */
