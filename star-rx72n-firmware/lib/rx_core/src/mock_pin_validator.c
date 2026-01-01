/* lib/rx_core/src/mock_pin_validator.c */

/**
 * @file mock_pin_validator.c
 * @brief Mock Pin Validator Implementation
 *
 * Implements the rx_pin_interface_t by tracking pin operations in memory.
 * Used for testing and validating the DIP pattern.
 */

#include "mock_pin_validator.h"

#include <string.h>

#include "rx_check.h"

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
 * @return Array index (0-255), or 0xFFFF if invalid
 */
static uint16_t internal_pin_to_index(uint8_t port, uint8_t pin)
{
  /* Validate pin first (0-7) */
  if (pin >= 8) {
    return 0xFFFF;
  }

  /* Handle decimal ports 0-9 */
  if (port <= 9) {
    uint16_t index = (uint16_t)(port * 8 + pin);
    return (index < k_mock_pin_validator_max_pins) ? index : 0xFFFF;
  }

  /* Handle hex ports A-G (0xA-0x10, which is 10-16 decimal) */
  if (port >= 0xA && port <= 0x10) {
    uint16_t index = (uint16_t)(((port - 0xA) + 10) * 8 + pin);
    return (index < k_mock_pin_validator_max_pins) ? index : 0xFFFF;
  }

  return 0xFFFF; /* Invalid port */
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
  if (pin >= 8) {
    return k_rx_err_gpio_invalid_pin;
  }

  /* Check port range */
  if (port <= 9) {
    return k_rx_ok; /* Decimal ports 0-9 */
  }

  if (port >= 0xA && port <= 0x10) {
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
static rx_err_t impl_validate_pin(void* ctx, uint8_t port, uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  /* Track that this pin was validated */
  uint16_t index = internal_pin_to_index(port, pin);
  if (index != 0xFFFF) {
    validator->validated_pins[index] = true;
    validator->validate_call_count++;
  }

  return k_rx_ok;
}

/**
 * @brief Reserve pin implementation
 */
static rx_err_t impl_reserve_pin(void* ctx, uint8_t port, uint8_t pin, const char* function)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == NULL || function == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
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
static rx_err_t impl_release_pin(void* ctx, uint8_t port, uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port/pin */
  rx_err_t err = internal_validate_port_pin(port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
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
static bool impl_is_pin_reserved(void* ctx, uint8_t port, uint8_t pin)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == NULL) {
    return false;
  }

  /* Validate port/pin */
  if (internal_validate_port_pin(port, pin) != k_rx_ok) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
    return false;
  }

  return validator->reserved_pins[index];
}

/**
 * @brief Get pin function implementation
 */
static rx_err_t
impl_get_pin_function(void* ctx, uint8_t port, uint8_t pin, char* function_out, size_t function_len)
{
  mock_pin_validator_t* validator = (mock_pin_validator_t*)ctx;

  if (validator == NULL || function_out == NULL) {
    return k_rx_err_null_pointer;
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
  if (index == 0xFFFF) {
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

  if (validator == NULL) {
    return k_rx_err_null_pointer;
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

bool mock_pin_validator_was_validated(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == NULL) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
    return false;
  }

  return validator->validated_pins[index];
}

bool mock_pin_validator_is_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == NULL) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
    return false;
  }

  return validator->reserved_pins[index];
}

bool mock_pin_validator_was_reserved(mock_pin_validator_t* validator, uint8_t port, uint8_t pin)
{
  if (validator == NULL) {
    return false;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
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
  if (validator == NULL) {
    return NULL;
  }

  uint16_t index = internal_pin_to_index(port, pin);
  if (index == 0xFFFF) {
    return NULL;
  }

  if (!validator->reserved_pins[index]) {
    return NULL;
  }

  return validator->function_names[index];
}

uint32_t mock_pin_validator_get_validate_call_count(mock_pin_validator_t* validator)
{
  if (validator == NULL) {
    return 0;
  }

  return validator->validate_call_count;
}

uint32_t mock_pin_validator_get_reserve_call_count(mock_pin_validator_t* validator)
{
  if (validator == NULL) {
    return 0;
  }

  return validator->reserve_call_count;
}

uint32_t mock_pin_validator_get_release_call_count(mock_pin_validator_t* validator)
{
  if (validator == NULL) {
    return 0;
  }

  return validator->release_call_count;
}

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
