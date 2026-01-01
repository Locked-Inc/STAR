/* src/core/rx_pin_validator.c */

/**
 * @file rx_pin_validator.c
 * @brief Pin Validator Concrete Implementation
 *
 * Implements the rx_pin_interface_t with pin validation and reservation tracking.
 */

#include "rx_pin_validator.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert port number to internal array index
 *
 * @param[in] port Port number (0-9, 0xA-0x10 for A-G)
 *
 * @return Array index (0-16), or 0xFF if invalid
 */
static uint8_t internal_port_to_index(uint8_t port)
{
  if (port <= 9) {
    return port; /* Ports 0-9 map directly */
  }

  if (port >= 0xA && port <= 0x10) {
    return (port - 0xA) + 10; /* Ports A-G (0xA-0x10) map to 10-16 */
  }

  return 0xFF; /* Invalid port */
}

/**
 * @brief Validate port number
 *
 * @param[in] port Port number to validate
 *
 * @return k_rx_ok if valid, k_rx_err_gpio_invalid_port if invalid
 */
static rx_err_t internal_validate_port(uint8_t port)
{
  uint8_t index = internal_port_to_index(port);
  if (index == 0xFF) {
    return k_rx_err_gpio_invalid_port;
  }
  return k_rx_ok;
}

/**
 * @brief Validate pin number
 *
 * @param[in] pin Pin number to validate
 *
 * @return k_rx_ok if valid, k_rx_err_gpio_invalid_pin if invalid
 */
static rx_err_t internal_validate_pin(uint8_t pin)
{
  if (pin >= k_pin_validator_max_pins) {
    return k_rx_err_gpio_invalid_pin;
  }
  return k_rx_ok;
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
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port */
  rx_err_t err = internal_validate_port(port);
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate pin */
  err = internal_validate_pin(pin);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Reserve pin implementation
 */
static rx_err_t impl_reserve_pin(void* ctx, uint8_t port, uint8_t pin, const char* function)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL || function == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if already reserved */
  if (reservation->reserved) {
    /* Release mutex before returning error */
    tx_mutex_put(&validator->mutex);

    star_log_warn("PIN_VALIDATOR", "Pin already reserved");
    return k_rx_err_gpio_conflict;
  }

  /* Reserve the pin */
  reservation->reserved = true;
  strncpy(reservation->function, function, k_pin_function_name_max_len - 1);
  reservation->function[k_pin_function_name_max_len - 1] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  star_log_debug("PIN_VALIDATOR", "Pin reserved");

  return k_rx_ok;
}

/**
 * @brief Release pin implementation
 */
static rx_err_t impl_release_pin(void* ctx, uint8_t port, uint8_t pin)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if pin was reserved */
  if (!reservation->reserved) {
    /* Release mutex before returning error */
    tx_mutex_put(&validator->mutex);

    star_log_warn("PIN_VALIDATOR", "Pin was not reserved");
    return k_rx_err_invalid_state;
  }

  /* Release the pin */
  reservation->reserved    = false;
  reservation->function[0] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  star_log_debug("PIN_VALIDATOR", "Pin released");

  return k_rx_ok;
}

/**
 * @brief Check if pin is reserved implementation
 */
static bool impl_is_pin_reserved(void* ctx, uint8_t port, uint8_t pin)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return false;
  }

  /* Validate port/pin */
  if (impl_validate_pin(ctx, port, pin) != k_rx_ok) {
    return false;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return false;
  }

  bool reserved = validator->reservations[port_index][pin].reserved;

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  return reserved;
}

/**
 * @brief Get pin function implementation
 */
static rx_err_t
impl_get_pin_function(void* ctx, uint8_t port, uint8_t pin, char* function_out, size_t function_len)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL || function_out == NULL) {
    return k_rx_err_null_pointer;
  }

  if (function_len < k_pin_function_name_max_len) {
    return k_rx_err_invalid_size;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if pin is reserved */
  if (!reservation->reserved) {
    tx_mutex_put(&validator->mutex);
    return k_rx_err_invalid_state;
  }

  /* Copy function name */
  strncpy(function_out, reservation->function, function_len - 1);
  function_out[function_len - 1] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  return k_rx_ok;
}

/**
 * @brief Clear all reservations implementation
 */
static rx_err_t impl_clear_all_reservations(void* ctx)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  /* Clear all reservations */
  for (uint32_t port_idx = 0; port_idx < k_pin_validator_max_ports; port_idx++) {
    for (uint32_t pin_idx = 0; pin_idx < k_pin_validator_max_pins; pin_idx++) {
      validator->reservations[port_idx][pin_idx].reserved    = false;
      validator->reservations[port_idx][pin_idx].function[0] = '\0';
    }
  }

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  star_log_debug("PIN_VALIDATOR", "All reservations cleared");

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t pin_validator_init(pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "PIN_VALIDATOR", "Validator pointer is NULL");

  /* Clear all state */
  memset(validator, 0, sizeof(pin_validator_t));

  /* Create mutex */
  UINT status = tx_mutex_create(&validator->mutex, "PinValidatorMutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    star_log_error("PIN_VALIDATOR", "Failed to create mutex");
    return k_rx_err_rtos_mutex;
  }

  validator->initialized = true;

  star_log_info("PIN_VALIDATOR", "Pin validator initialized");

  return k_rx_ok;
}

rx_err_t pin_validator_get_interface(rx_pin_interface_t* iface, pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(iface, "PIN_VALIDATOR", "Interface pointer is NULL");
  RX_CHECK_NULL_PTR(validator, "PIN_VALIDATOR", "Validator pointer is NULL");

  if (!validator->initialized) {
    star_log_error("PIN_VALIDATOR", "Validator not initialized");
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

rx_err_t pin_validator_deinit(pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "PIN_VALIDATOR", "Validator pointer is NULL");

  if (!validator->initialized) {
    return k_rx_ok; /* Already deinitialized */
  }

  /* Delete mutex */
  tx_mutex_delete(&validator->mutex);

  /* Clear state */
  validator->initialized = false;

  star_log_info("PIN_VALIDATOR", "Pin validator deinitialized");

  return k_rx_ok;
}
