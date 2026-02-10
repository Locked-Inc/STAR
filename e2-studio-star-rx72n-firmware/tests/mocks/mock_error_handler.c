/* tests/mocks/mock_error_handler.c */

/**
 * @file mock_error_handler.c
 * @brief Mock Error Handler Implementation
 *
 * Implements the rx_error_interface_t by recording errors in memory.
 * Used for testing and validating the DIP pattern.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_error_handler.h"

#include <string.h>

#include "rx_check.h"

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Count errors for a specific component
 *
 * @param[in] handler Handler instance
 * @param[in] component Component name
 *
 * @return Number of errors for the component
 */
static uint32_t internal_count_component_errors(mock_error_handler_t* handler,
                                                const char*           component)
{
  uint32_t count = 0;

  for (uint32_t i = 0; i < handler->stored_error_count; i++) {
    if (strcmp(handler->errors[i].component, component) == 0) {
      count++;
    }
  }

  return count;
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
  mock_error_handler_t* handler = (mock_error_handler_t*)ctx;

  if (handler == nullptr || component == nullptr || message == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Get next write position (circular buffer) */
  uint32_t             index  = handler->write_index;
  mock_error_record_t* record = &handler->errors[index];

  /* Record the error */
  record->error_code = err;
  strncpy(record->component, component, k_mock_error_handler_component_name_max - 1);
  record->component[k_mock_error_handler_component_name_max - 1] = '\0';
  strncpy(record->message, message, k_mock_error_handler_message_max - 1);
  record->message[k_mock_error_handler_message_max - 1] = '\0';

  /* Update indices */
  handler->write_index = (handler->write_index + 1) % handler->max_errors;
  handler->total_error_count++;

  if (handler->stored_error_count < handler->max_errors) {
    handler->stored_error_count++;
  }

  return k_rx_ok;
}

/**
 * @brief Get total error count implementation
 */
static uint32_t impl_get_error_count(void* ctx)
{
  mock_error_handler_t* handler = (mock_error_handler_t*)ctx;

  if (handler == nullptr) {
    return 0;
  }

  return handler->total_error_count;
}

/**
 * @brief Get component error count implementation
 */
static uint32_t impl_get_component_error_count(void* ctx, const char* component)
{
  mock_error_handler_t* handler = (mock_error_handler_t*)ctx;

  if (handler == nullptr || component == nullptr) {
    return 0;
  }

  return internal_count_component_errors(handler, component);
}

/**
 * @brief Clear all errors implementation
 */
static rx_err_t impl_clear_errors(void* ctx)
{
  mock_error_handler_t* handler = (mock_error_handler_t*)ctx;

  if (handler == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Clear all error records */
  memset(handler->errors, 0, sizeof(handler->errors));
  handler->total_error_count  = 0;
  handler->stored_error_count = 0;
  handler->write_index        = 0;

  return k_rx_ok;
}

/**
 * @brief Check if retry limit reached implementation
 *
 * Note: Mock handler has no retry limit concept, always returns false
 */
static bool impl_is_retry_limit_reached(void* ctx, const char* component)
{
  (void)ctx;
  (void)component;

  /* Mock handler has no retry limit */
  return false;
}

/**
 * @brief Reset retry counter implementation
 *
 * Note: Mock handler has no retry counter, this is a no-op
 */
static rx_err_t impl_reset_retry_counter(void* ctx, const char* component)
{
  (void)ctx;
  (void)component;

  /* Mock handler has no retry counter */
  return k_rx_ok;
}

/**
 * @brief Get backoff delay implementation
 *
 * Note: Mock handler has no backoff logic, always returns 0
 */
static uint32_t impl_get_backoff_delay(void* ctx, const char* component)
{
  (void)ctx;
  (void)component;

  /* Mock handler has no backoff delay */
  return 0;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t mock_error_handler_init(mock_error_handler_t* handler, uint32_t max_errors)
{
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  if (max_errors == 0 || max_errors > k_mock_error_handler_default_max_errors) {
    return k_rx_err_invalid_arg;
  }

  /* Clear all state */
  memset(handler, 0, sizeof(mock_error_handler_t));

  /* Initialize configuration */
  handler->max_errors  = max_errors;
  handler->initialized = true;

  return k_rx_ok;
}

rx_err_t mock_error_handler_get_interface(rx_error_interface_t* iface,
                                          mock_error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(iface, "MOCK_ERROR", "Interface pointer is NULL");
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  if (!handler->initialized) {
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

/* =============================================================================
 * Testing Helper Functions Implementation
 * =============================================================================
 */

rx_err_t mock_error_handler_get_last_error(mock_error_handler_t* handler,
                                           rx_err_t*             out_error,
                                           const char**          out_component,
                                           const char**          out_message)
{
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  if (handler->stored_error_count == 0) {
    return k_rx_err_not_found;
  }

  /* Get most recent error (last stored) */
  uint32_t last_index =
    (handler->write_index == 0) ? (handler->stored_error_count - 1) : (handler->write_index - 1);
  mock_error_record_t* record = &handler->errors[last_index];

  /* Fill output parameters if provided */
  if (out_error != nullptr) {
    *out_error = record->error_code;
  }

  if (out_component != nullptr) {
    *out_component = record->component;
  }

  if (out_message != nullptr) {
    *out_message = record->message;
  }

  return k_rx_ok;
}

rx_err_t mock_error_handler_get_error_at(mock_error_handler_t* handler,
                                         uint32_t              index,
                                         rx_err_t*             out_error,
                                         const char**          out_component,
                                         const char**          out_message)
{
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  if (index >= handler->stored_error_count) {
    return k_rx_err_invalid_arg;
  }

  mock_error_record_t* record = &handler->errors[index];

  /* Fill output parameters if provided */
  if (out_error != nullptr) {
    *out_error = record->error_code;
  }

  if (out_component != nullptr) {
    *out_component = record->component;
  }

  if (out_message != nullptr) {
    *out_message = record->message;
  }

  return k_rx_ok;
}

bool mock_error_handler_has_error(mock_error_handler_t* handler,
                                  rx_err_t              error_code,
                                  const char*           component)
{
  if (handler == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < handler->stored_error_count; i++) {
    if (handler->errors[i].error_code == error_code) {
      /* If component specified, check it matches */
      if (component != nullptr) {
        if (strcmp(handler->errors[i].component, component) == 0) {
          return true;
        }
      } else {
        /* No component specified, error code match is enough */
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Clear all recorded errors
 *
 * @param[in] handler Pointer to mock error handler instance
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if handler is NULL
 */
rx_err_t mock_error_handler_clear(mock_error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  /* Reuse interface implementation */
  return impl_clear_errors(handler);
}

/**
 * @brief Get number of currently stored errors (circular buffer size)
 *
 * @param[in] handler Pointer to mock error handler instance
 *
 * @return Number of stored errors (0 if handler is NULL)
 */
uint32_t mock_error_handler_get_stored_count(mock_error_handler_t* handler)
{
  if (handler == nullptr) {
    return 0;
  }

  return handler->stored_error_count;
}

/**
 * @brief Deinitialize mock error handler
 *
 * @param[in] handler Pointer to mock error handler instance
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if handler is NULL
 */
rx_err_t mock_error_handler_deinit(mock_error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(handler, "MOCK_ERROR", "Handler pointer is NULL");

  if (!handler->initialized) {
    return k_rx_ok; /* Already deinitialized */
  }

  /* Clear state */
  handler->initialized = false;

  return k_rx_ok;
}
