/* lib/rx_core/inc/rx_pin_validator.h */

/**
 * @file rx_pin_validator.h
 * @brief Pin Validator Concrete Implementation
 * @details
 * Concrete implementation of the rx_pin_interface_t.
 * Provides pin validation and reservation tracking for RX72N GPIO.
 *
 * Features:
 * - Validates port/pin combinations against RX72N hardware
 * - Tracks pin reservations to prevent conflicts
 * - Stores function name for each reserved pin
 * - Thread-safe operation (ThreadX mutex)
 * - Supports all RX72N ports (0-9, A-G) and pins (0-7)
 *
 * Memory usage (static allocation):
 * - ~4.5KB for pin reservation tracking (17 ports * 8 pins * ~33 bytes)
 * - ~100 bytes for ThreadX mutex
 * - ~50 bytes for pin_validator_t struct
 * Total: ~4.7KB RAM
 *
 * RX72N Port/Pin Layout:
 * - PORT0-PORT9: Decimal ports 0-9
 * - PORTA-PORTG: Hex ports 0xA-0x10 (decimal 10-16)
 * - Each port has up to 8 pins (0-7)
 * - Not all ports have all 8 pins available
 *
 * Usage:
 * @code
 *   // 1. Declare validator
 *   static pin_validator_t s_pin_validator;
 *
 *   // 2. Initialize
 *   rx_err_t err = pin_validator_init(&s_pin_validator);
 *   RX_ERROR_CHECK(err);
 *
 *   // 3. Get interface
 *   rx_pin_interface_t pin_iface;
 *   pin_validator_get_interface(&pin_iface, &s_pin_validator);
 *
 *   // 4. Reserve pins
 *   err = pin_iface.reserve_pin(pin_iface.ctx, 0xA, 5, "SPI_MOSI");
 *   if (err == k_rx_err_gpio_conflict) {
 *       // Pin already reserved
 *   }
 *
 *   // 5. Release when done
 *   pin_iface.release_pin(pin_iface.ctx, 0xA, 5);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_PIN_VALIDATOR_H
#define STAR_RX72N_PIN_VALIDATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_pin_interface.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/**
 * @brief Pin validator configuration constants
 */
typedef enum {
  k_pin_validator_max_ports = 17, /**< Maximum port number (0-16 for 0-9, A-G) */
  k_pin_validator_max_pins  = 8,  /**< Maximum pins per port */
} pin_validator_limits_t;

/* =============================================================================
 * Pin Reservation State
 * =============================================================================
 */

/**
 * @brief Pin reservation state for a single pin
 */
typedef struct {
  bool reserved;                              /**< Is this pin reserved? */
  char function[k_pin_function_name_max_len]; /**< Function name (e.g., "SPI_MOSI", "GPIO_OUT") */
} pin_reservation_t;

/* =============================================================================
 * Pin Validator Structure
 * =============================================================================
 */

/**
 * @brief Pin validator concrete implementation
 *
 * This structure contains all state for pin validation and tracking.
 * It implements the rx_pin_interface_t.
 */
typedef struct {
  TX_MUTEX          mutex; /**< ThreadX mutex for thread-safe operation */
  pin_reservation_t reservations
    [k_pin_validator_max_ports]
    [k_pin_validator_max_pins]; /**< Pin reservation tracking array (indexed: reservations[port][pin], Port 0-9: Decimal, Port 10-16: Hex A-G) */
  bool initialized;             /**< Is the validator initialized? */
} pin_validator_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize pin validator
 *
 * @param[in,out] validator Validator instance to initialize
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if validator is NULL,
 *         k_rx_err_rtos_mutex if mutex creation fails
 */
rx_err_t pin_validator_init(pin_validator_t* validator);

/**
 * @brief Get interface from concrete validator
 *
 * @param[out] iface Interface to fill
 * @param[in,out] validator Concrete validator instance
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if either parameter is NULL,
 *         k_rx_err_invalid_state if validator not initialized
 */
rx_err_t pin_validator_get_interface(rx_pin_interface_t* iface, pin_validator_t* validator);

/**
 * @brief Deinitialize pin validator (cleanup resources)
 *
 * @param[in,out] validator Validator to deinitialize
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if validator is NULL
 */
rx_err_t pin_validator_deinit(pin_validator_t* validator);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PIN_VALIDATOR_H */
