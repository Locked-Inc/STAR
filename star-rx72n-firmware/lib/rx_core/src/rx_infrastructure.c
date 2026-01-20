/* src/core/rx_infrastructure.c */

/**
 * @file rx_infrastructure.c
 * @brief Global Infrastructure Initialization Implementation
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_infrastructure.h"

#include "rx_check.h"
#include "rx_error_handler.h"
#include "rx_gpio_constants.h"
#include "rx_log.h"
#include "rx_pin_validator.h"

/* =============================================================================
 * Global Infrastructure Instances
 * =============================================================================
 */

/**
 * @brief Global error handler instance
 */
static error_handler_t s_global_error_handler;

/**
 * @brief Global pin validator instance
 */
static pin_validator_t s_global_pin_validator;

/**
 * @brief Global error handler interface
 */
static rx_error_interface_t s_global_error_interface;

/**
 * @brief Global pin validator interface
 */
static rx_pin_interface_t s_global_pin_interface;

/**
 * @brief Is infrastructure initialized?
 */
static bool s_infrastructure_initialized = false;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_infrastructure_init(void)
{
  if (s_infrastructure_initialized) {
    rx_log_warn("INFRA", "Infrastructure already initialized");
    return k_rx_ok;
  }

  rx_log_info("INFRA", "Initializing global infrastructure");

  /* Initialize error handler */
  const error_handler_config_t config = {
    .max_retries        = k_error_handler_default_max_retries,
    .initial_backoff_ms = k_error_handler_default_initial_backoff_ms,
    .max_backoff_ms     = k_error_handler_default_max_backoff_ms,
  };

  rx_err_t err = error_handler_init(&s_global_error_handler, &config);
  if (err != k_rx_ok) {
    rx_log_error("INFRA", "Failed to initialize error handler");
    return err;
  }

  /* Get error handler interface */
  err = error_handler_get_interface(&s_global_error_interface, &s_global_error_handler);
  if (err != k_rx_ok) {
    rx_log_error("INFRA", "Failed to get error handler interface");
    error_handler_deinit(&s_global_error_handler);
    return err;
  }

  /* Initialize pin validator */
  err = pin_validator_init(&s_global_pin_validator);
  if (err != k_rx_ok) {
    rx_log_error("INFRA", "Failed to initialize pin validator");
    error_handler_deinit(&s_global_error_handler);
    return err;
  }

  /* Get pin validator interface */
  err = pin_validator_get_interface(&s_global_pin_interface, &s_global_pin_validator);
  if (err != k_rx_ok) {
    rx_log_error("INFRA", "Failed to get pin validator interface");
    pin_validator_deinit(&s_global_pin_validator);
    error_handler_deinit(&s_global_error_handler);
    return err;
  }

  s_infrastructure_initialized = true;

  rx_log_info("INFRA", "Global infrastructure initialized successfully");

  return k_rx_ok;
}

rx_err_t rx_infrastructure_deinit(void)
{
  if (!s_infrastructure_initialized) {
    return k_rx_ok;
  }

  rx_log_info("INFRA", "Deinitializing global infrastructure");

  /* Deinitialize pin validator */
  pin_validator_deinit(&s_global_pin_validator);

  /* Deinitialize error handler */
  error_handler_deinit(&s_global_error_handler);

  s_infrastructure_initialized = false;

  rx_log_info("INFRA", "Global infrastructure deinitialized");

  return k_rx_ok;
}

rx_error_interface_t* rx_infrastructure_get_error_interface(void)
{
  if (!s_infrastructure_initialized) {
    return NULL;
  }

  return &s_global_error_interface;
}

rx_pin_interface_t* rx_infrastructure_get_pin_interface(void)
{
  if (!s_infrastructure_initialized) {
    return NULL;
  }

  return &s_global_pin_interface;
}
