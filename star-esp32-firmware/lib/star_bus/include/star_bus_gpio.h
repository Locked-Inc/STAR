/* lib/star_bus/include/star_bus_gpio.h */

#ifndef STAR_COMPONENT_BUS_GPIO_H
#define STAR_COMPONENT_BUS_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_gpio.h
 * @brief GPIO bus operations through the unified bus manager
 *
 * This module provides GPIO digital read/write operations that work through the
 * bus manager. GPIO bus is used for controlling digital logic such as
 * multiplexer select lines.
 *
 * Key Features:
 * - Digital write operations (set pin high/low)
 * - Digital read operations (read pin state)
 * - Thread-safe through bus manager mutex
 *
 * Example Usage:
 * @code
 * // === Setup ===
 *
 * // Create GPIO bus for multiplexer control
 * gpio_num_t mux_pins[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6};
 * star_bus_config_t* mux_bus = star_bus_config_create_gpio(
 *     "mux_gpio",        // Unique bus name
 *     mux_pins,          // Pin array
 *     3                  // Number of pins
 * );
 * star_bus_manager_add_bus(&bus_manager, mux_bus);
 *
 *
 * // === Digital Write ===
 *
 * // Set pin 0 (GPIO_NUM_4) to HIGH
 * star_bus_gpio_write_digital(&bus_manager, "mux_gpio", 0, 1);
 *
 * // Set pin 1 (GPIO_NUM_5) to LOW
 * star_bus_gpio_write_digital(&bus_manager, "mux_gpio", 1, 0);
 *
 *
 * // === Digital Read ===
 *
 * // Read state of pin 2 (GPIO_NUM_6)
 * uint8_t pin_state;
 * star_bus_gpio_read_digital(&bus_manager, "mux_gpio", 2, &pin_state);
 * ESP_LOGI(TAG, "Pin state: %d", pin_state);
 *
 * @endcode
 */

/**
 * @brief Get default GPIO operations structure
 *
 * Returns a structure containing function pointers to the default
 * implementations for GPIO bus operations.
 *
 * @return star_gpio_ops_t Default operations structure
 */
star_gpio_ops_t star_bus_gpio_get_default_ops(void);

/**
 * @brief Write digital value to GPIO pin
 *
 * @param[in] manager   Bus manager instance
 * @param[in] bus_name  Name of GPIO bus
 * @param[in] pin_index Index into pin array (0-based)
 * @param[in] value     Value to write (0=LOW, non-zero=HIGH)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_gpio_write_digital(star_bus_manager_t* manager,
                                       const char*         bus_name,
                                       uint8_t             pin_index,
                                       uint8_t             value);

/**
 * @brief Read digital value from GPIO pin
 *
 * @param[in]  manager   Bus manager instance
 * @param[in]  bus_name  Name of GPIO bus
 * @param[in]  pin_index Index into pin array (0-based)
 * @param[out] value     Pointer to store pin state (0 or 1)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_gpio_read_digital(star_bus_manager_t* manager,
                                      const char*         bus_name,
                                      uint8_t             pin_index,
                                      uint8_t*            value);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_GPIO_H */
