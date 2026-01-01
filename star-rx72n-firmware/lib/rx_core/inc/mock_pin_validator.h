/* lib/rx_core/inc/mock_pin_validator.h */

/**
 * @file mock_pin_validator.h
 * @brief Mock Pin Validator for Testing DIP Pattern
 * @details
 * Mock implementation of the rx_pin_interface_t for testing and validation.
 * Tracks pin operations in memory instead of validating against real hardware.
 *
 * This demonstrates the power of Dependency Inversion Principle:
 * - Same interface as pin_validator_t
 * - Different implementation (tracking vs hardware validation)
 * - Can be swapped in any code that uses rx_pin_interface_t
 *
 * Features:
 * - Tracks which pins were validated (via validate_pin calls)
 * - Tracks which pins were reserved (via reserve_pin calls)
 * - Tracks which pins were released (via release_pin calls)
 * - No ThreadX dependencies (no mutex, simpler for testing)
 * - Supports up to 256 pins (adequate for RX72N's 182 pins)
 * - Query functions to verify pin operations in tests
 *
 * Memory usage (static allocation):
 * - ~512 bytes for validated_pins array (256 bools)
 * - ~512 bytes for reserved_pins array (256 bools)
 * - ~8KB for function names (256 pins * 32 bytes)
 * - ~50 bytes for mock_pin_validator_t struct
 * Total: ~9KB RAM
 *
 * Usage Example (Swapping Implementations):
 * @code
 * // Production code uses real pin validator:
 * pin_validator_t real_validator;
 * pin_validator_init(&real_validator);
 * rx_pin_interface_t pin_iface;
 * pin_validator_get_interface(&pin_iface, &real_validator);
 *
 * // Test code uses mock validator (same interface!):
 * mock_pin_validator_t mock_validator;
 * mock_pin_validator_init(&mock_validator);
 * rx_pin_interface_t test_iface;
 * mock_pin_validator_get_interface(&test_iface, &mock_validator);
 *
 * // Both can be used identically by dependent code:
 * bus_manager_init(&bus_mgr, &error_iface, &pin_iface);  // or &test_iface
 * @endcode
 *
 * Usage Example (Verifying Pin Operations in Tests):
 * @code
 * // 1. Initialize mock
 * mock_pin_validator_t mock;
 * mock_pin_validator_init(&mock);
 * rx_pin_interface_t iface;
 * mock_pin_validator_get_interface(&iface, &mock);
 *
 * // 2. Pass to code under test
 * spi_driver_init(&spi_drv, &iface);  // Will reserve MOSI, MISO, SCK pins
 *
 * // 3. Verify pins were reserved
 * assert(mock_pin_validator_was_reserved(&mock, 0xA, 5));  // MOSI
 * assert(mock_pin_validator_was_reserved(&mock, 0xA, 6));  // MISO
 * assert(mock_pin_validator_was_reserved(&mock, 0xA, 7));  // SCK
 *
 * // 4. Check reservation count
 * uint32_t count = mock_pin_validator_get_reserve_call_count(&mock);
 * assert(count == 3);
 *
 * // 5. Cleanup
 * spi_driver_deinit(&spi_drv);  // Should release pins
 * assert(!mock_pin_validator_is_reserved(&mock, 0xA, 5));
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_MOCK_PIN_VALIDATOR_H
#define STAR_RX72N_MOCK_PIN_VALIDATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/**
 * @brief Mock pin validator configuration constants
 */
typedef enum {
  k_mock_pin_validator_max_pins          = 256, /**< Maximum number of pins to track (covers RX72N's 182 pins) */
  k_mock_pin_validator_function_name_max = 32,  /**< Maximum function name length */
} mock_pin_validator_limits_t;

/* =============================================================================
 * Mock Pin Validator Structure
 * =============================================================================
 */

/**
 * @brief Mock pin validator implementation
 *
 * Tracks pin operations in memory for testing. Implements rx_pin_interface_t.
 *
 * @note Array indexing formula: port * 8 + pin
 *       The factor 8 represents the number of pins per GPIO port on RX72N (pins 0-7).
 *       This linearizes the 2D port/pin space into a flat array for efficient tracking.
 *       Example: Port A (0xA), Pin 5 = 0xA * 8 + 5 = 85
 */
typedef struct {
  bool validated_pins[k_mock_pin_validator_max_pins]; /**< Track validated pins (indexed: port*8+pin, where 8 = pins per port) */
  bool reserved_pins[k_mock_pin_validator_max_pins];  /**< Track currently reserved pins (indexed: port*8+pin) */
  char function_names[k_mock_pin_validator_max_pins]
                     [k_mock_pin_validator_function_name_max]; /**< Function names for reserved pins (indexed: port*8+pin) */
  uint32_t validate_call_count; /**< Total number of validate_pin calls */
  uint32_t reserve_call_count;  /**< Total number of reserve_pin calls */
  uint32_t release_call_count;  /**< Total number of release_pin calls */
  bool     initialized;         /**< Is the validator initialized? */
} mock_pin_validator_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize mock pin validator
 *
 * @param[in,out] validator Validator instance to initialize
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if validator is NULL
 */
rx_err_t mock_pin_validator_init(mock_pin_validator_t* validator);

/**
 * @brief Get interface from mock validator
 *
 * @param[out] iface Interface to fill
 * @param[in,out] validator Mock validator instance
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if either parameter is NULL,
 *         RX_ERR_INVALID_STATE if validator not initialized
 */
rx_err_t mock_pin_validator_get_interface(rx_pin_interface_t*   iface,
                                          mock_pin_validator_t* validator);

/* =============================================================================
 * Testing Helper Functions
 * =============================================================================
 */

/**
 * @brief Check if a pin was validated (validate_pin was called)
 *
 * @param[in] validator Validator instance
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return true if validate_pin was called for this pin, false otherwise
 */
bool mock_pin_validator_was_validated(mock_pin_validator_t* validator, uint8_t port, uint8_t pin);

/**
 * @brief Check if a pin is currently reserved
 *
 * @param[in] validator Validator instance
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return true if pin is currently reserved, false otherwise
 */
bool mock_pin_validator_is_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin);

/**
 * @brief Check if a pin was ever reserved (even if later released)
 *
 * This is different from is_reserved which only checks current state.
 * This checks if reserve_pin was ever called for this pin.
 *
 * @param[in] validator Validator instance
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return true if reserve_pin was called for this pin, false otherwise
 *
 * @note This returns true even if the pin was later released.
 *       To check current reservation status, use mock_pin_validator_is_reserved.
 */
bool mock_pin_validator_was_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin);

/**
 * @brief Get the function name for a reserved pin
 *
 * @param[in] validator Validator instance
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return Function name if pin is reserved, NULL if not reserved or invalid
 *
 * @note Returns a pointer to internal buffer. Do not modify or free.
 */
const char*
mock_pin_validator_get_function(mock_pin_validator_t* validator, uint8_t port, uint8_t pin);

/**
 * @brief Get total number of validate_pin calls
 *
 * @param[in] validator Validator instance
 *
 * @return Total number of validate_pin calls
 */
uint32_t mock_pin_validator_get_validate_call_count(mock_pin_validator_t* validator);

/**
 * @brief Get total number of reserve_pin calls
 *
 * @param[in] validator Validator instance
 *
 * @return Total number of reserve_pin calls
 */
uint32_t mock_pin_validator_get_reserve_call_count(mock_pin_validator_t* validator);

/**
 * @brief Get total number of release_pin calls
 *
 * @param[in] validator Validator instance
 *
 * @return Total number of release_pin calls
 */
uint32_t mock_pin_validator_get_release_call_count(mock_pin_validator_t* validator);

/**
 * @brief Clear all tracking data
 *
 * Resets all tracking arrays and counters. Does not deinitialize.
 *
 * @param[in,out] validator Validator to clear
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if validator is NULL
 */
rx_err_t mock_pin_validator_clear(mock_pin_validator_t* validator);

/**
 * @brief Deinitialize mock pin validator (cleanup resources)
 *
 * @param[in,out] validator Validator to deinitialize
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if validator is NULL
 */
rx_err_t mock_pin_validator_deinit(mock_pin_validator_t* validator);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MOCK_PIN_VALIDATOR_H */
