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
 * @return RX_OK if valid, RX_ERR_GPIO_INVALID_PORT if invalid
 */
static rx_err_t internal_validate_port(uint8_t port)
{
  uint8_t index = internal_port_to_index(port);
  if (index == 0xFF) {
    return RX_ERR_GPIO_INVALID_PORT;
  }
  return RX_OK;
}

/**
 * @brief Validate pin number
 *
 * @param[in] pin Pin number to validate
 *
 * @return RX_OK if valid, RX_ERR_GPIO_INVALID_PIN if invalid
 */
static rx_err_t internal_validate_pin(uint8_t pin)
{
  if (pin >= RX_PIN_VALIDATOR_MAX_PINS) {
    return RX_ERR_GPIO_INVALID_PIN;
  }
  return RX_OK;
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
    return RX_ERR_NULL_POINTER;
  }

  /* Validate port */
  rx_err_t err = internal_validate_port(port);
  if (err != RX_OK) {
    return err;
  }

  /* Validate pin */
  err = internal_validate_pin(pin);
  if (err != RX_OK) {
    return err;
  }

  return RX_OK;
}

/**
 * @brief Reserve pin implementation
 */
static rx_err_t impl_reserve_pin(void* ctx, uint8_t port, uint8_t pin, const char* function)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL || function == NULL) {
    return RX_ERR_NULL_POINTER;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != RX_OK) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return RX_ERR_RTOS_MUTEX;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if already reserved */
  if (reservation->reserved) {
    /* Release mutex before returning error */
    tx_mutex_put(&validator->mutex);

    RX_LOG_WARN("PIN_VALIDATOR", "Pin already reserved");
    return RX_ERR_GPIO_CONFLICT;
  }

  /* Reserve the pin */
  reservation->reserved = true;
  strncpy(reservation->function, function, RX_PIN_FUNCTION_NAME_MAX_LEN - 1);
  reservation->function[RX_PIN_FUNCTION_NAME_MAX_LEN - 1] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  RX_LOG_DEBUG("PIN_VALIDATOR", "Pin reserved");

  return RX_OK;
}

/**
 * @brief Release pin implementation
 */
static rx_err_t impl_release_pin(void* ctx, uint8_t port, uint8_t pin)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return RX_ERR_NULL_POINTER;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != RX_OK) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return RX_ERR_RTOS_MUTEX;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if pin was reserved */
  if (!reservation->reserved) {
    /* Release mutex before returning error */
    tx_mutex_put(&validator->mutex);

    RX_LOG_WARN("PIN_VALIDATOR", "Pin was not reserved");
    return RX_ERR_INVALID_STATE;
  }

  /* Release the pin */
  reservation->reserved    = false;
  reservation->function[0] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  RX_LOG_DEBUG("PIN_VALIDATOR", "Pin released");

  return RX_OK;
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
  if (impl_validate_pin(ctx, port, pin) != RX_OK) {
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
static rx_err_t impl_get_pin_function(void* ctx, uint8_t port, uint8_t pin, char* function_out,
                                      size_t function_len)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL || function_out == NULL) {
    return RX_ERR_NULL_POINTER;
  }

  if (function_len < RX_PIN_FUNCTION_NAME_MAX_LEN) {
    return RX_ERR_INVALID_SIZE;
  }

  /* Validate port/pin */
  rx_err_t err = impl_validate_pin(ctx, port, pin);
  if (err != RX_OK) {
    return err;
  }

  uint8_t port_index = internal_port_to_index(port);

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return RX_ERR_RTOS_MUTEX;
  }

  pin_reservation_t* reservation = &validator->reservations[port_index][pin];

  /* Check if pin is reserved */
  if (!reservation->reserved) {
    tx_mutex_put(&validator->mutex);
    return RX_ERR_INVALID_STATE;
  }

  /* Copy function name */
  strncpy(function_out, reservation->function, function_len - 1);
  function_out[function_len - 1] = '\0';

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  return RX_OK;
}

/**
 * @brief Clear all reservations implementation
 */
static rx_err_t impl_clear_all_reservations(void* ctx)
{
  pin_validator_t* validator = (pin_validator_t*)ctx;

  if (validator == NULL) {
    return RX_ERR_NULL_POINTER;
  }

  /* Acquire mutex */
  UINT status = tx_mutex_get(&validator->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return RX_ERR_RTOS_MUTEX;
  }

  /* Clear all reservations */
  for (uint32_t port_idx = 0; port_idx < RX_PIN_VALIDATOR_MAX_PORTS; port_idx++) {
    for (uint32_t pin_idx = 0; pin_idx < RX_PIN_VALIDATOR_MAX_PINS; pin_idx++) {
      validator->reservations[port_idx][pin_idx].reserved    = false;
      validator->reservations[port_idx][pin_idx].function[0] = '\0';
    }
  }

  /* Release mutex */
  tx_mutex_put(&validator->mutex);

  RX_LOG_DEBUG("PIN_VALIDATOR", "All reservations cleared");

  return RX_OK;
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
    RX_LOG_ERROR("PIN_VALIDATOR", "Failed to create mutex");
    return RX_ERR_RTOS_MUTEX;
  }

  validator->initialized = true;

  RX_LOG_INFO("PIN_VALIDATOR", "Pin validator initialized");

  return RX_OK;
}

rx_err_t pin_validator_get_interface(rx_pin_interface_t* iface, pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(iface, "PIN_VALIDATOR", "Interface pointer is NULL");
  RX_CHECK_NULL_PTR(validator, "PIN_VALIDATOR", "Validator pointer is NULL");

  if (!validator->initialized) {
    RX_LOG_ERROR("PIN_VALIDATOR", "Validator not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Fill interface */
  iface->ctx                    = validator;
  iface->validate_pin           = impl_validate_pin;
  iface->reserve_pin            = impl_reserve_pin;
  iface->release_pin            = impl_release_pin;
  iface->is_pin_reserved        = impl_is_pin_reserved;
  iface->get_pin_function       = impl_get_pin_function;
  iface->clear_all_reservations = impl_clear_all_reservations;

  return RX_OK;
}

rx_err_t pin_validator_deinit(pin_validator_t* validator)
{
  RX_CHECK_NULL_PTR(validator, "PIN_VALIDATOR", "Validator pointer is NULL");

  if (!validator->initialized) {
    return RX_OK; /* Already deinitialized */
  }

  /* Delete mutex */
  tx_mutex_delete(&validator->mutex);

  /* Clear state */
  validator->initialized = false;

  RX_LOG_INFO("PIN_VALIDATOR", "Pin validator deinitialized");

  return RX_OK;
}
