/* lib/rx_core/inc/rx_infrastructure.h */

/**
 * @file rx_infrastructure.h
 * @brief Global Infrastructure Initialization
 * @details
 * Provides global instances of error handler and pin validator.
 * Centralizes initialization of core infrastructure components.
 *
 * This module owns the concrete implementations and exposes interfaces
 * to the rest of the system. Other modules should use the interfaces,
 * not the concrete types.
 *
 * Usage:
 * @code
 *   // In main.c, before any other initialization:
 *   rx_err_t err = rx_infrastructure_init();
 *   RX_ERROR_CHECK(err);
 *
 *   // In other modules, get interfaces:
 *   rx_error_interface_t* error_iface = rx_infrastructure_get_error_interface();
 *   rx_pin_interface_t* pin_iface = rx_infrastructure_get_pin_interface();
 *
 *   // Use through interfaces:
 *   error_iface->report_error(error_iface->ctx, err, "MODULE", "Operation failed");
 *   pin_iface->reserve_pin(pin_iface->ctx, 0xA, 5, "SPI_MOSI");
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_INFRASTRUCTURE_H
#define STAR_RX72N_INFRASTRUCTURE_H

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Global Infrastructure Initialization
 * =============================================================================
 */

/**
 * @brief Initialize global infrastructure
 *
 * Initializes:
 * - Global error handler (max_retries=3, initial_backoff=100ms, max_backoff=5000ms)
 * - Global pin validator
 *
 * This must be called once during system initialization, before any
 * other modules are initialized.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_infrastructure_init(void);

/**
 * @brief Deinitialize global infrastructure
 *
 * Cleans up all infrastructure resources.
 * Should be called during shutdown (if ever).
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_infrastructure_deinit(void);

/* =============================================================================
 * Interface Accessors
 * =============================================================================
 */

/**
 * @brief Get global error handler interface
 *
 * @return Pointer to global error interface, or NULL if not initialized
 */
rx_error_interface_t* rx_infrastructure_get_error_interface(void);

/**
 * @brief Get global pin validator interface
 *
 * @return Pointer to global pin interface, or NULL if not initialized
 */
rx_pin_interface_t* rx_infrastructure_get_pin_interface(void);

/* =============================================================================
 * Convenience Macros
 * =============================================================================
 */

/**
 * @brief Report error through global error handler
 *
 * @param[in] err Error code
 * @param[in] component Component name
 * @param[in] message Error message
 */
#define RX_REPORT_ERROR(err, component, message)                                                   \
  do {                                                                                             \
    rx_error_interface_t* iface_ = rx_infrastructure_get_error_interface();                        \
    if (iface_ != NULL) {                                                                          \
      iface_->report_error(iface_->ctx, err, component, message);                                  \
    }                                                                                              \
  } while (0)

/**
 * @brief Reserve pin through global pin validator
 *
 * @param[in] port Port number
 * @param[in] pin Pin number
 * @param[in] function Function name
 *
 * @return rx_err_t error code
 */
#define RX_RESERVE_PIN(port, pin, function)                                                        \
  ({                                                                                               \
    rx_err_t            err_   = k_rx_err_invalid_state;                                           \
    rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();                            \
    if (iface_ != NULL) {                                                                          \
      err_ = iface_->reserve_pin(iface_->ctx, port, pin, function);                                \
    }                                                                                              \
    err_;                                                                                          \
  })

/**
 * @brief Release pin through global pin validator
 *
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return rx_err_t error code
 */
#define RX_RELEASE_PIN(port, pin)                                                                  \
  ({                                                                                               \
    rx_err_t            err_   = k_rx_err_invalid_state;                                           \
    rx_pin_interface_t* iface_ = rx_infrastructure_get_pin_interface();                            \
    if (iface_ != NULL) {                                                                          \
      err_ = iface_->release_pin(iface_->ctx, port, pin);                                          \
    }                                                                                              \
    err_;                                                                                          \
  })

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_INFRASTRUCTURE_H */
