/* lib/rx_bus/inc/rx_bus_adc.h */

/**
 * @file rx_bus_adc.h
 * @brief ADC bus abstraction for RX72N
 * @details
 * Provides bus manager integration for S12ADFa ADC operations.
 * Wraps the low-level ADC HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BUS_ADC_H
#define STAR_RX72N_BUS_ADC_H

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ADC Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize ADC channel through bus manager
 *
 * Configures ADC unit, channel, and resolution.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name ADC bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not ADC type
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_adc_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Read ADC raw value through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name ADC bus name
 * @param[out] value Pointer to store raw ADC value
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or value is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if conversion or mutex timeout
 */
rx_err_t rx_bus_adc_read(rx_bus_manager_t* manager, const char* bus_name, uint16_t* value);

/**
 * @brief Read ADC voltage in millivolts through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name ADC bus name
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or voltage_mv is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if conversion or mutex timeout
 */
rx_err_t
rx_bus_adc_read_voltage_mv(rx_bus_manager_t* manager, const char* bus_name, uint32_t* voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_ADC_H */
