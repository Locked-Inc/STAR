// SPDX-License-Identifier: MIT
/* tests/mocks/mock_pin_validator.c */

/**
 * @file mock_pin_validator.c
 * @brief Mock Pin Validator Implementation
 *
 * Implements the rx_pin_interface_t by tracking pin operations in memory.
 * Used for testing and validating the DIP pattern.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include "mock_pin_validator.h"

#include <string.h>

#include "rx_check.h"
#include "rx_gpio_constants.h"

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert port/pin to flat array index
 *
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return Array index (0-255), or k_invalid_pin_index if invalid
 */
static uint16_t internal_pin_to_index(uint8_t port, uint8_t pin)
{
  /* Validate pin first (0-7) */
  if (pin >= k_pins_per_port) {
    return k_invalid_pin_index;
  }

  /* Handle decimal ports 0-9 */
  if (port <= k_max_decimal_port) {
    uint16_t index = (uint16_t)(port * k_pins_per_port + pin);
    return (index < k_mock_pin_validator_max_pins) ? index : k_invalid_pin_index;
  }

  /* Handle hex ports A-G (0xA-0x10, which is 10-16 decimal) */
  if (port >= k_hex_port_start && port <= k_hex_port_end) {
    uint16_t index =
      (uint16_t)(((port - k_hex_port_start) + k_hex_port_offset) * k_pins_per_port + pin);
    return (index < k_mock_pin_validator_max_pins) ? index : k_invalid_pin_index;
  }

  return k_invalid_pin_index; /* Invalid port */
}

/**
 * @brief Validate port/pin combination
 *
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return k_rx_ok if valid, error code otherwise
 */
static rx_err_t internal_validate_port_pin(uint8_t port, uint8_t pin)
{
  if (pin >= k_pins_per_port) {
    return k_rx_err_gpio_invalid_pin;
  }

  /* Check port range */
  if (port <= k_max_decimal_port) {
    return k_rx_ok; /* Decimal ports 0-9 */
  }

  if (port >= k_hex_port_start && port <= k_hex_port_end) {
    return k_rx_ok; /* Hex ports A-G */
  }

  return k_rx_err_gpio_invalid_port;
}

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Validate pin implementation
 */
static rx_err_t impl_validate_pin(void* ctx, const uint8_t port, const uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  /* Track that this pin was validated */
  uint16_t index = internal_pin_to_index(port, pin);
  if (index != k_invalid_pin_index) {
    validator->validated_pins[index] = true;
    validator->validate_call_count++;
  }

  return k_rx_ok;
}

/**
 * @brief Reserve pin implementation
 */
static rx_err_t
impl_reserve_pin(void* ctx, const uint8_t port, const uint8_t pin, const char* function)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr || function == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return k_rx_err_gpio_invalid_pin;
  }

  /* Check if already reserved */
  if (validator->reserved_pins[index]) {
    return k_rx_err_gpio_conflict;
  }

  /* Reserve the pin */
  validator->reserved_pins[index] = true;
  strncpy(validator->function_names[index], function, k_mock_pin_validator_function_name_max - 1);
  validator->function_names[index][k_mock_pin_validator_function_name_max - 1] = '\0';
  validator->reserve_call_count++;

  return k_rx_ok;
}

/**
 * @brief Release pin implementation
 */
static rx_err_t impl_release_pin(void* ctx, const uint8_t port, const uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return k_rx_err_gpio_invalid_pin;
  }

  /* Check if pin was reserved */
  if (!validator->reserved_pins[index]) {
    return k_rx_err_invalid_state;
  }

  /* Release the pin */
  validator->reserved_pins[index]     = false;
  validator->function_names[index][0] = '\0';
  validator->release_call_count++;

  return k_rx_ok;
}

/**
 * @brief Check if pin is reserved implementation
 */
static bool impl_is_pin_reserved(void* ctx, const uint8_t port, const uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr) {
    return false;
  }

  /* Validate port/pin */
  if (internal_validate_port_pin(port, pin) != k_rx_ok) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return false;
  }

  return validator->reserved_pins[index];
}

/**
 * @brief Get pin function implementation
 */
static rx_err_t impl_get_pin_function(void*          ctx,
                                      const uint8_t  port,
                                      const uint8_t  pin,
                                      char*          function_out,
                                      const uint32_t function_len)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr || function_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (function_len < k_pin_function_name_max_len) {
    return k_rx_err_invalid_size;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return k_rx_err_gpio_invalid_pin;
  }

  /* Check if pin is reserved */
  if (!validator->reserved_pins[index]) {
    return k_rx_err_invalid_state;
  }

  /* Copy function name */
  strncpy(function_out, validator->function_names[index], function_len - 1);
  function_out[function_len - 1] = '\0';

  return k_rx_ok;
}

/**
 * @brief Clear all reservations implementation
 */
static rx_err_t impl_clear_all_reservations(void* ctx)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Clear all reservations (but keep validation and call count tracking) */
  for (uint32_t i = 0; i < k_mock_pin_validator_max_pins; i++) {
    validator->reserved_pins[i]     = false;
    validator->function_names[i][0] = '\0';
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t mock_pin_validator_init(mock_pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "MOCK_PIN", "Validator pointer is NULL");

  /* Clear all state */
  memset(validator, 0, sizeof(mock_pin_validator_t));

  validator->initialized = true;

  return k_rx_ok;
}

rx_err_t mock_pin_validator_get_interface(rx_pin_interface_t*   iface,
                                          mock_pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(iface, "MOCK_PIN", "Interface pointer is NULL");
  RX_CHECK_NULL_PTR(validator, "MOCK_PIN", "Validator pointer is NULL");

  if (!validator->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Fill interface */
  iface->ctx                    = validator;
  iface->validate_pin           = impl_validate_pin;
  iface->reserve_pin            = impl_reserve_pin;
  iface->release_pin            = impl_release_pin;
  iface->is_pin_reserved        = impl_is_pin_reserved;
  iface->get_pin_function       = impl_get_pin_function;
  iface->clear_all_reservations = impl_clear_all_reservations;

  return k_rx_ok;
}

/* =============================================================================
 * Testing Helper Functions Implementation
 * =============================================================================
 */

/**
 * @brief Check if a pin was previously validated
 *
 * @param[in] validator Pointer to mock pin validator instance
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return true if pin was validated, false otherwise
 */
bool mock_pin_validator_was_validated(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == nullptr) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return false;
  }

  return validator->validated_pins[index];
}

/**
 * @brief Check if a pin is currently reserved
 *
 * @param[in] validator Pointer to mock pin validator instance
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return true if pin is reserved, false otherwise
 */
bool mock_pin_validator_is_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == nullptr) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return false;
  }

  return validator->reserved_pins[index];
}

/**
 * @brief Check if a pin was ever reserved (even if later released)
 *
 * @param[in] validator Pointer to mock pin validator instance
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return true if pin was reserved at any point, false otherwise
 */
bool mock_pin_validator_was_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == nullptr) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return false;
  }

  /* Check if function name was ever set (even if later cleared) */
  /* Note: This implementation only tracks current state. For full history tracking,
   * we would need additional arrays. For now, this is equivalent to is_reserved. */
  return validator->reserved_pins[index] || (validator->function_names[index][0] != '\0');
}

const char*
mock_pin_validator_get_function(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == nullptr) {
    return nullptr;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == k_invalid_pin_index) {
    return nullptr;
  }

  if (!validator->reserved_pins[index]) {
    return nullptr;
  }

  return validator->function_names[index];
}

/**
 * @brief Get total number of validate calls
 *
 * @param[in] validator Pointer to mock pin validator instance
 *
 * @return Number of times validate_pin was called
 */
uint32_t mock_pin_validator_get_validate_call_count(mock_pin_validator_t* validator)
{
  if (validator == nullptr) {
    return 0;
  }

  return validator->validate_call_count;
}

/**
 * @brief Get total number of reserve calls
 *
 * @param[in] validator Pointer to mock pin validator instance
 *
 * @return Number of times reserve_pin was called
 */
uint32_t mock_pin_validator_get_reserve_call_count(mock_pin_validator_t* validator)
{
  if (validator == nullptr) {
    return 0;
  }

  return validator->reserve_call_count;
}

/**
 * @brief Get total number of release calls
 *
 * @param[in] validator Pointer to mock pin validator instance
 *
 * @return Number of times release_pin was called
 */
uint32_t mock_pin_validator_get_release_call_count(mock_pin_validator_t* validator)
{
  if (validator == nullptr) {
    return 0;
  }

  return validator->release_call_count;
}

/**
 * @brief Clear all tracking data (validated pins, reservations, call counts)
 *
 * @param[in] validator Pointer to mock pin validator instance
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if validator is NULL
 */
rx_err_t mock_pin_validator_clear(mock_pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "MOCK_PIN", "Validator pointer is NULL");

  /* Clear all tracking data */
  memset(validator->validated_pins, 0, sizeof(validator->validated_pins));
  memset(validator->reserved_pins, 0, sizeof(validator->reserved_pins));
  memset(validator->function_names, 0, sizeof(validator->function_names));

  validator->validate_call_count = 0;
  validator->reserve_call_count  = 0;
  validator->release_call_count  = 0;

  return k_rx_ok;
}

/**
 * @brief Deinitialize mock pin validator
 *
 * @param[in] validator Pointer to mock pin validator instance
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if validator is NULL
 */
rx_err_t mock_pin_validator_deinit(mock_pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "MOCK_PIN", "Validator pointer is NULL");

  if (!validator->initialized) {
    return k_rx_ok; /* Already deinitialized */
  }

  /* Clear state */
  validator->initialized = false;

  return k_rx_ok;
}
