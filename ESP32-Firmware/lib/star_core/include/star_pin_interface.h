/**
 * @file star_pin_interface.h
 * @brief Pin validator interface for dependency inversion
 */

#ifndef STAR_PIN_INTERFACE_H
#define STAR_PIN_INTERFACE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pin validator interface - abstract operations for pin management
 */
typedef struct star_pin_interface {
  /**
   * @brief Register a pin
   * @param ctx Implementation context
   * @param pin GPIO pin number
   * @param description Pin description
   * @param shared Whether pin can be shared
   * @return ESP_OK on success
   */
  esp_err_t (*register_pin)(void* ctx, int pin, const char* description, bool shared);

  /**
   * @brief Unregister a pin
   * @param ctx Implementation context
   * @param pin GPIO pin number
   * @param description Pin description (for matching)
   * @return ESP_OK on success
   */
  esp_err_t (*unregister_pin)(void* ctx, int pin, const char* description);

  /**
   * @brief Validate all registered pins for conflicts
   * @param ctx Implementation context
   * @return ESP_OK if no conflicts
   */
  esp_err_t (*validate)(void* ctx);

  /**
   * @brief Implementation context (opaque pointer to actual validator)
   */
  void* ctx;
} star_pin_interface_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_PIN_INTERFACE_H */
