/**
 * @file star_error_interface.h
 * @brief Abstract error handler interface for dependency inversion
 * @details
 * This interface defines the contract for error handling in the STAR framework.
 * It follows the Dependency Inversion Principle (DIP), allowing high-level
 * components to depend on this abstraction rather than concrete error handler
 * implementations. Supports retry logic with exponential backoff for transient errors.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_ERROR_INTERFACE_H
#define STAR_ERROR_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * It follows the Dependency Inversion Principle (DIP), allowing high-level
 * components to depend on this abstract interface rather than concrete implementations.
 *
 * The concrete implementation is provided by star_error_handler.h.
 *
 * Example Usage:
 * @code
 * // === Creating and Using Error Interface ===
 *
 * #include "star_error_interface.h"
 * #include "star_error_handler.h"
 *
 * // 1. Create concrete error handler
 * error_handler_t error_handler;
 * error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);
 *
 * // 2. Get interface from implementation
 * star_error_interface_t error_iface;
 * error_handler_get_interface(&error_iface, &error_handler);
 *
 * // 3. Inject into components that need error handling
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 * // Components use error interface for error handling and retry logic
 *
 *
 * // === Using the Interface Directly ===
 *
 * // Record an error through the interface
 * if (some_operation_failed) {
 *     error_iface.record_error(
 *         error_iface.ctx,
 *         ESP_ERR_TIMEOUT,
 *         "Operation timed out",
 *         __FILE__,
 *         __LINE__,
 *         __func__
 *     );
 * }
 *
 * // Or use the convenience macro
 * if (ret != ESP_OK) {
 *     STAR_IFACE_RECORD_ERROR(&error_iface, ret, "Sensor read failed");
 * }
 *
 * // Check if retry is possible
 * if (error_iface.can_retry(error_iface.ctx)) {
 *     // Attempt retry...
 * }
 *
 * // Reset error state after successful recovery
 * error_iface.reset_state(error_iface.ctx);
 *
 *
 * // === Using Default Interface (Simplified) ===
 *
 * // Create interface with default error handler in one call
 * star_error_interface_t* iface = star_error_interface_create_default();
 *
 * // Use with components
 * star_bus_manager_init(&bus_manager, "main", iface, NULL);
 *
 * // Cleanup
 * star_error_interface_destroy(iface);
 *
 *
 * // === Implementing Custom Error Handler ===
 *
 * // You can create custom error handlers by implementing the interface
 * esp_err_t my_record_error(void* ctx, esp_err_t error, const char* message,
 *                           const char* file, int line, const char* func) {
 *     // Custom logging, telemetry, etc.
 *     ESP_LOGE("MY_HANDLER", "[%s:%d] %s: %s",
 *              file, line, func, message);
 *     return error;
 * }
 *
 * bool my_can_retry(void* ctx) {
 *     my_context_t* my_ctx = (my_context_t*)ctx;
 *     return my_ctx->retry_count < my_ctx->max_retries;
 * }
 *
 * esp_err_t my_reset_state(void* ctx) {
 *     my_context_t* my_ctx = (my_context_t*)ctx;
 *     my_ctx->retry_count = 0;
 *     return ESP_OK;
 * }
 *
 * // Create interface with custom implementation
 * my_context_t my_ctx = { .max_retries = 5, .retry_count = 0 };
 * star_error_interface_t custom_iface = {
 *     .record_error = my_record_error,
 *     .can_retry = my_can_retry,
 *     .reset_state = my_reset_state,
 *     .ctx = &my_ctx
 * };
 *
 *
 * // === NULL Interface Handling ===
 *
 * // Many components accept NULL error_iface and create default internally
 * // sensor_init(&sensor, &bus_manager, "sensor_bus", NULL, &config);
 * // ^-- This creates a default error handler internally
 *
 * // The STAR_IFACE_RECORD_ERROR macro safely handles NULL interfaces
 * star_error_interface_t* maybe_null = NULL;
 * esp_err_t result = STAR_IFACE_RECORD_ERROR(maybe_null, ESP_ERR_TIMEOUT, "msg");
 * // ^-- Returns ESP_ERR_TIMEOUT without crashing
 * @endcode
 */

/**
 * @brief Error handler interface - abstract operations for error handling
 */
typedef struct star_error_interface {
  /**
   * @brief Record an error
   * @param ctx Implementation context
   * @param error Error code
   * @param message Error message
   * @param file Source file
   * @param line Line number
   * @param func Function name
   * @return ESP_OK on success
   */
  esp_err_t (*record_error)(void*       ctx,
                            esp_err_t   error,
                            const char* message,
                            const char* file,
                            int         line,
                            const char* func);

  /**
   * @brief Check if retry is possible
   * @param ctx Implementation context
   * @return true if retry is allowed
   */
  bool (*can_retry)(void* ctx);

  /**
   * @brief Reset error state
   * @param ctx Implementation context
   * @return ESP_OK on success
   */
  esp_err_t (*reset_state)(void* ctx);

  /**
   * @brief Implementation context (opaque pointer to actual handler)
   */
  void* ctx;
} star_error_interface_t;

/**
 * @brief Convenience macro to record error through interface
 */
#define STAR_IFACE_RECORD_ERROR(iface, err, msg)                                                   \
  ((iface) && (iface)->record_error                                                                \
     ? (iface)->record_error((iface)->ctx, (err), (msg), __FILE__, __LINE__, __func__)             \
     : (err))

#ifdef __cplusplus
}
#endif

#endif /* STAR_ERROR_INTERFACE_H */
