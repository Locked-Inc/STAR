/**
 * @file star_error_interface.h
 * @brief Error handler interface for dependency inversion
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
