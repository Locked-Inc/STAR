/* src/core/rx_error_handler.c */

/**
 * @file rx_error_handler.c
 * @brief Error Handler Concrete Implementation
 *
 * Implements the rx_error_interface_t with error tracking, retry logic,
 * and exponential backoff.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_error_handler.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Find component by name
 *
 * @param[in] handler Handler instance
 * @param[in] component Component name to find
 *
 * @return Pointer to component state, or NULL if not found
 */
static error_component_state_t* internal_find_component(error_handler_t* handler,
                                                        const char*      component)
{
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    if (handler->components[i].in_use &&
        strncmp(handler->components[i].name, component, k_error_handler_component_name_max) == 0) {
      return &handler->components[i];
    }
  }
  return NULL;
}

/**
 * @brief Find or create component entry
 *
 * @param[in] handler Handler instance
 * @param[in] component Component name
 *
 * @return Pointer to component state, or NULL if no slots available
 */
static error_component_state_t* internal_find_or_create_component(error_handler_t* handler,
                                                                  const char*      component)
{
  /* Try to find existing component */
  error_component_state_t* comp = internal_find_component(handler, component);
  if (comp != NULL) {
    return comp;
  }

  /* Find empty slot */
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    if (!handler->components[i].in_use) {
      /* Initialize new component entry */
      strncpy(handler->components[i].name, component, k_error_handler_component_name_max - 1);
      handler->components[i].name[k_error_handler_component_name_max - 1] = '\0';
      handler->components[i].error_count                                  = 0;
      handler->components[i].retry_count                                  = 0;
      handler->components[i].in_use                                       = true;
      return &handler->components[i];
    }
  }

  /* No slots available */
  return NULL;
}

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Report error implementation
 */
static rx_err_t
impl_report_error(void* ctx, rx_err_t err, const char* component, const char* message)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL || component == NULL || message == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  /* Increment total error count */
  handler->total_error_count++;

  /* Find or create component */
  error_component_state_t* comp = internal_find_or_create_component(handler, component);
  if (comp != NULL) {
    comp->error_count++;
    comp->retry_count++;
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  /* Log the error */
  rx_log_error(component, message);
  rx_log_error_val(component, "Error", err);

  return k_rx_ok;
}

/**
 * @brief Get total error count implementation
 */
static uint32_t impl_get_error_count(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  const uint32_t count = handler->total_error_count;

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return count;
}

/**
 * @brief Get component error count implementation
 */
static uint32_t impl_get_component_error_count(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL || component == NULL) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  uint32_t                 count = 0;
  error_component_state_t* comp  = internal_find_component(handler, component);
  if (comp != NULL) {
    count = comp->error_count;
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return count;
}

/**
 * @brief Clear all errors implementation
 */
static rx_err_t impl_clear_errors(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  /* Clear total count */
  handler->total_error_count = 0;

  /* Clear all component counts */
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    handler->components[i].error_count = 0;
    handler->components[i].retry_count = 0;
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return k_rx_ok;
}

/**
 * @brief Check if retry limit reached implementation
 */
static bool impl_is_retry_limit_reached(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL || component == NULL) {
    return true; /* Fail-safe: consider limit reached if invalid params */
  }

  /* If max_retries is 0, no limit */
  if (handler->max_retries == 0) {
    return false;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return true; /* Fail-safe */
  }

  bool                     limit_reached = false;
  error_component_state_t* comp          = internal_find_component(handler, component);
  if (comp != NULL) {
    limit_reached = (comp->retry_count >= handler->max_retries);
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return limit_reached;
}

/**
 * @brief Reset retry counter implementation
 */
static rx_err_t impl_reset_retry_counter(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL || component == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  error_component_state_t* comp = internal_find_component(handler, component);
  if (comp != NULL) {
    comp->retry_count = 0;
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return k_rx_ok;
}

/**
 * @brief Get backoff delay implementation (exponential backoff)
 */
static uint32_t impl_get_backoff_delay(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == NULL || component == NULL) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  uint32_t                 delay_ms = 0;
  error_component_state_t* comp     = internal_find_component(handler, component);
  if (comp != NULL && comp->retry_count > 0) {
    /* Exponential backoff: delay = initial * 2^(retry_count - 1)
     * Capped at max_backoff_ms */
    delay_ms = handler->initial_backoff_ms;
    for (uint32_t i = 1; i < comp->retry_count; i++) {
      delay_ms *= 2;
      if (delay_ms >= handler->max_backoff_ms) {
        delay_ms = handler->max_backoff_ms;
        break;
      }
    }
  }

  /* Release mutex */
  tx_mutex_put(&handler->mutex);

  return delay_ms;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t error_handler_init(error_handler_t* handler,
                            uint32_t         max_retries,
                            uint32_t         initial_backoff_ms,
                            uint32_t         max_backoff_ms)
{
  RX_CHECK_NULL_PTR(handler, "ERROR_HANDLER", "Handler pointer is NULL");

  /* Clear all state */
  memset(handler, 0, sizeof(error_handler_t));

  /* Initialize configuration */
  handler->max_retries        = max_retries;
  handler->initial_backoff_ms = initial_backoff_ms;
  handler->max_backoff_ms     = max_backoff_ms;

  /* Create mutex */
  const UINT status = tx_mutex_create(&handler->mutex, "ErrorHandlerMutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    rx_log_error("ERROR_HANDLER", "Failed to create mutex");
    return k_rx_err_rtos_mutex;
  }

  handler->initialized = true;

  rx_log_info("ERROR_HANDLER", "Error handler initialized");

  return k_rx_ok;
}

rx_err_t error_handler_get_interface(rx_error_interface_t* iface, error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(iface, "ERROR_HANDLER", "Interface pointer is NULL");
  RX_CHECK_NULL_PTR(handler, "ERROR_HANDLER", "Handler pointer is NULL");

  if (!handler->initialized) {
    rx_log_error("ERROR_HANDLER", "Handler not initialized");
    return k_rx_err_invalid_state;
  }

  /* Fill interface */
  iface->ctx                       = handler;
  iface->report_error              = impl_report_error;
  iface->get_error_count           = impl_get_error_count;
  iface->get_component_error_count = impl_get_component_error_count;
  iface->clear_errors              = impl_clear_errors;
  iface->is_retry_limit_reached    = impl_is_retry_limit_reached;
  iface->reset_retry_counter       = impl_reset_retry_counter;
  iface->get_backoff_delay         = impl_get_backoff_delay;

  return k_rx_ok;
}

rx_err_t error_handler_deinit(error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(handler, "ERROR_HANDLER", "Handler pointer is NULL");

  if (!handler->initialized) {
    return k_rx_ok; /* Already deinitialized */
  }

  /* Delete mutex */
  tx_mutex_delete(&handler->mutex);

  /* Clear state */
  handler->initialized = false;

  rx_log_info("ERROR_HANDLER", "Error handler deinitialized");

  return k_rx_ok;
}
