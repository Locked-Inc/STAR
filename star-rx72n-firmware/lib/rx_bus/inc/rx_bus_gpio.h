/* include/rx_bus_gpio.h */

/**
 * @file rx_bus_gpio.h
 * @brief GPIO bus abstraction for RX72N
 * @details
 * Provides bus manager integration for GPIO digital I/O operations.
 * Wraps the low-level GPIO HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BUS_GPIO_H
#define STAR_RX72N_BUS_GPIO_H

#include <stdbool.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize GPIO pin through bus manager
 *
 * Configures pin direction and reserves pin through pin validator.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name GPIO bus name
 * @param[in] output True for output, false for input
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_ARG if bus is not GPIO type
 * @return RX_ERR_CONFLICT if pin already reserved (via pin validator)
 */
rx_err_t rx_bus_gpio_init(rx_bus_manager_t* manager, const char* bus_name, bool output);

/**
 * @brief Write GPIO output through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name GPIO bus name
 * @param[in] value True for high, false for low
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 */
rx_err_t rx_bus_gpio_write(rx_bus_manager_t* manager, const char* bus_name, bool value);

/**
 * @brief Read GPIO input through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name GPIO bus name
 * @param[out] value True if high, false if low
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or value is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 */
rx_err_t rx_bus_gpio_read(rx_bus_manager_t* manager, const char* bus_name, bool* value);

/**
 * @brief Toggle GPIO output through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name GPIO bus name
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 */
rx_err_t rx_bus_gpio_toggle(rx_bus_manager_t* manager, const char* bus_name);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_GPIO_H */
