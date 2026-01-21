/* lib/rx_core/inc/rx_pin_interface.h */

/**
 * @file rx_pin_interface.h
 * @brief Pin Validator Interface (Dependency Inversion Principle)
 * @details
 * Defines the abstract interface for GPIO pin validation and reservation.
 * Follows ESP32 star_pin_interface_t pattern for architectural consistency.
 *
 * This is a pure interface with no implementation details.
 * Concrete implementations (like rx_pin_validator.c) will provide the actual
 * pin validation and tracking logic.
 *
 * Pin reservation prevents conflicts when multiple peripherals or drivers
 * attempt to use the same GPIO pin. This is especially important for:
 * - Peripheral function selection (UART TX/RX, SPI COPI/CIPO, etc.)
 * - GPIO output conflicts (two drivers trying to control same pin)
 * - Debugging pin usage conflicts
 *
 * RX72N has 182 GPIO pins across multiple ports (0-9, A-G).
 * Ports are identified by number: 0-9, 0xA-0xG (10-16 decimal).
 * Pins within each port are numbered 0-7 (not all ports have all 8 pins).
 *
 * Usage:
 * @code
 *   // 1. Create concrete implementation
 *   pin_validator_t validator;
 *   pin_validator_init(&validator);
 *
 *   // 2. Get interface
 *   rx_pin_interface_t iface;
 *   pin_validator_get_interface(&iface, &validator);
 *
 *   // 3. Inject into dependent component
 *   bus_manager_init(&bus_mgr, &error_iface, &iface);
 *
 *   // 4. Use through interface
 *   rx_err_t err = iface.reserve_pin(iface.ctx, k_rx_port_a, k_rx_pin_5, "SPI_COPI");
 *   if (err != k_rx_ok) {
 *       // Pin already reserved or invalid
 *   }
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PIN_INTERFACE_H
#define STAR_RX72N_PIN_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of interface struct */
typedef struct rx_pin_interface rx_pin_interface_t;

/* =============================================================================
 * Pin Function Type
 * =============================================================================
 */

/**
 * @brief Pin interface configuration constants
 */
typedef enum : uint8_t {
  k_pin_function_name_max_len =
    32, /**< Maximum length of pin function name (e.g., "SPI_COPI", "UART_TX") */
} pin_interface_limits_t;

/* =============================================================================
 * Pin Interface Function Pointers
 * =============================================================================
 */

/**
 * @brief Validate if a pin exists and is accessible
 *
 * @param[in] ctx Implementation context
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return k_rx_ok if pin is valid,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
typedef rx_err_t (*rx_pin_validate_fn)(void* ctx, const uint8_t port, const uint8_t pin);

/**
 * @brief Reserve a pin for a specific function
 *
 * @param[in] ctx Implementation context
 * @param[in] port Port number
 * @param[in] pin Pin number
 * @param[in] function Function name (e.g., "SPI_MOSI", "GPIO_OUT")
 *
 * @return k_rx_ok if reserved successfully,
 *         k_rx_err_gpio_conflict if pin already reserved,
 *         k_rx_err_gpio_invalid_port if port invalid,
 *         k_rx_err_gpio_invalid_pin if pin invalid,
 *         k_rx_err_invalid_arg if function is NULL
 */
typedef rx_err_t (*rx_pin_reserve_fn)(void*             ctx,
                                      const uint8_t     port,
                                      const uint8_t     pin,
                                      const char*       function);

/**
 * @brief Release a previously reserved pin
 *
 * @param[in] ctx Implementation context
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return k_rx_ok if released successfully,
 *         k_rx_err_invalid_state if pin was not reserved,
 *         k_rx_err_gpio_invalid_port if port invalid,
 *         k_rx_err_gpio_invalid_pin if pin invalid
 */
typedef rx_err_t (*rx_pin_release_fn)(void* ctx, const uint8_t port, const uint8_t pin);

/**
 * @brief Check if a pin is currently reserved
 *
 * @param[in] ctx Implementation context
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return true if pin is reserved, false if available
 */
typedef bool (*rx_pin_is_reserved_fn)(void* ctx, const uint8_t port, const uint8_t pin);

/**
 * @brief Get the function name for a reserved pin
 *
 * @param[in] ctx Implementation context
 * @param[in] port Port number
 * @param[in] pin Pin number
 * @param[out] function_out Buffer to store function name
 * @param[in] function_len Length of function_out buffer
 *
 * @return k_rx_ok if function retrieved,
 *         k_rx_err_invalid_state if pin not reserved,
 *         k_rx_err_null_ptr if function_out is NULL,
 *         k_rx_err_invalid_size if buffer too small,
 *         k_rx_err_gpio_invalid_port if port invalid,
 *         k_rx_err_gpio_invalid_pin if pin invalid
 */
typedef rx_err_t (*rx_pin_get_function_fn)(void*          ctx,
                                           const uint8_t  port,
                                           const uint8_t  pin,
                                           char*          function_out,
                                           const uint32_t function_len);

/**
 * @brief Clear all pin reservations
 *
 * @param[in] ctx Implementation context
 *
 * @return k_rx_ok on success
 */
typedef rx_err_t (*rx_pin_clear_all_fn)(void* ctx);

/* =============================================================================
 * Pin Interface Structure
 * =============================================================================
 */

/**
 * @brief Pin validator interface
 *
 * This structure defines the contract that all pin validator implementations
 * must fulfill. Dependent components use this interface without knowing the
 * concrete implementation details.
 *
 * The ctx field holds a pointer to the concrete implementation's private data.
 * All function pointers receive this context as their first parameter.
 */
struct rx_pin_interface {
  void*                  ctx; /**< Implementation context (opaque pointer to concrete validator) */
  rx_pin_validate_fn     validate_pin;           /**< Validate pin existence */
  rx_pin_reserve_fn      reserve_pin;            /**< Reserve pin for function */
  rx_pin_release_fn      release_pin;            /**< Release reserved pin */
  rx_pin_is_reserved_fn  is_pin_reserved;        /**< Check if pin is reserved */
  rx_pin_get_function_fn get_pin_function;       /**< Get pin's function name */
  rx_pin_clear_all_fn    clear_all_reservations; /**< Clear all reservations */
};

/* =============================================================================
 * Interface Validation
 * =============================================================================
 */

/**
 * @brief Validate that a pin interface is properly initialized
 *
 * @param[in] iface Interface to validate
 *
 * @return k_rx_ok if valid, k_rx_err_null_ptr if NULL,
 *         k_rx_err_invalid_state if missing function pointers
 */
static inline rx_err_t rx_pin_interface_validate(const rx_pin_interface_t* iface)
{
  if (iface == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Check required function pointers */
  if (iface->validate_pin == NULL || iface->reserve_pin == NULL || iface->release_pin == NULL ||
      iface->is_pin_reserved == NULL || iface->get_pin_function == NULL ||
      iface->clear_all_reservations == NULL) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PIN_INTERFACE_H */
