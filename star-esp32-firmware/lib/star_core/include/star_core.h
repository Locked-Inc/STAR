/* lib/star_core/include/star_core.h */

#ifndef STAR_CORE_H
#define STAR_CORE_H

#include <stddef.h>

#include "star_error_interface.h"
#include "star_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_core.h
 * @brief Unified header for star_core interfaces
 *
 * This header provides all interface definitions and validation utilities
 * for the STAR firmware's dependency inversion system.
 *
 * Example Usage:
 * @code
 * // === Include All Core Interfaces ===
 *
 * #include "star_core.h"  // Includes star_error_interface.h and star_pin_interface.h
 *
 *
 * // === Get Library Version ===
 *
 * const char* version = star_core_version();
 * ESP_LOGI(TAG, "STAR Core version: %s", version);
 *
 *
 * // === Validate Interfaces Before Use ===
 *
 * star_error_interface_t error_iface;
 * error_handler_get_interface(&error_iface, &error_handler);
 *
 * // Check if error interface is valid (all function pointers set)
 * if (star_error_interface_is_valid(&error_iface)) {
 *     ESP_LOGI(TAG, "Error interface is valid");
 * } else {
 *     ESP_LOGE(TAG, "Error interface is incomplete");
 *     return ESP_ERR_INVALID_STATE;
 * }
 *
 * star_pin_interface_t pin_iface;
 * pin_validator_get_interface(&pin_iface);
 *
 * // Check if pin interface is valid
 * if (star_pin_interface_is_valid(&pin_iface)) {
 *     ESP_LOGI(TAG, "Pin interface is valid");
 * }
 *
 *
 * // === Using Convenience Macros ===
 *
 * // Check if retry is possible (safely handles NULL)
 * if (STAR_IFACE_CAN_RETRY(&error_iface)) {
 *     ESP_LOGI(TAG, "Retry is possible");
 *     // Attempt retry...
 * }
 *
 * // Reset error state
 * esp_err_t ret = STAR_IFACE_RESET_STATE(&error_iface);
 *
 * // Register a GPIO pin
 * ret = STAR_IFACE_REGISTER_PIN(&pin_iface, GPIO_NUM_21, "I2C SDA", true);
 * ret = STAR_IFACE_REGISTER_PIN(&pin_iface, GPIO_NUM_22, "I2C SCL", true);
 *
 * // Unregister a pin when no longer needed
 * ret = STAR_IFACE_UNREGISTER_PIN(&pin_iface, GPIO_NUM_21, "I2C SDA");
 *
 * // Validate all registered pins for conflicts
 * ret = STAR_IFACE_VALIDATE_PINS(&pin_iface);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Pin conflict detected!");
 * }
 *
 *
 * // === NULL-Safe Macro Usage ===
 *
 * // All STAR_IFACE_* macros safely handle NULL interfaces
 * star_error_interface_t* maybe_null = NULL;
 *
 * // This won't crash - returns false for NULL
 * bool can_retry = STAR_IFACE_CAN_RETRY(maybe_null);
 *
 * // This won't crash - returns ESP_OK for NULL (no-op)
 * ret = STAR_IFACE_RESET_STATE(maybe_null);
 *
 *
 * // === Typical Initialization Pattern ===
 *
 * esp_err_t init_system(void) {
 *     // Create error handler
 *     error_handler_t error_handler;
 *     error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);
 *
 *     // Get error interface
 *     star_error_interface_t error_iface;
 *     error_handler_get_interface(&error_iface, &error_handler);
 *
 *     // Validate before use
 *     if (!star_error_interface_is_valid(&error_iface)) {
 *         return ESP_ERR_INVALID_STATE;
 *     }
 *
 *     // Get pin interface
 *     star_pin_interface_t pin_iface;
 *     pin_validator_get_interface(&pin_iface);
 *
 *     // Validate before use
 *     if (!star_pin_interface_is_valid(&pin_iface)) {
 *         return ESP_ERR_INVALID_STATE;
 *     }
 *
 *     // Initialize bus manager with validated interfaces
 *     star_bus_manager_t bus_manager;
 *     esp_err_t ret = star_bus_manager_init(&bus_manager, "main",
 *                                            &error_iface, &pin_iface);
 *     return ret;
 * }
 *
 *
 * // === Using STAR_IFACE_RECORD_ERROR ===
 *
 * // Record error through interface (from star_error_interface.h)
 * esp_err_t perform_operation(star_error_interface_t* error_iface) {
 *     esp_err_t ret = some_function();
 *
 *     if (ret != ESP_OK) {
 *         // Record error with file/line/function info
 *         STAR_IFACE_RECORD_ERROR(error_iface, ret, "Operation failed");
 *
 *         // Check if we should retry
 *         if (STAR_IFACE_CAN_RETRY(error_iface)) {
 *             // Retry logic...
 *             ret = some_function();
 *             if (ret == ESP_OK) {
 *                 STAR_IFACE_RESET_STATE(error_iface);
 *             }
 *         }
 *     }
 *     return ret;
 * }
 * @endcode
 */

/**
 * @brief Get the star_core library version
 * @return Version string
 */
const char* star_core_version(void);

/**
 * @brief Check if an error interface is valid (all function pointers set)
 * @param iface Interface to validate
 * @return true if interface is valid and usable
 */
static inline bool star_error_interface_is_valid(const star_error_interface_t* iface)
{
  if (iface == NULL) {
    return false;
  }

  if (iface->record_error == NULL) {
    return false;
  }

  if (iface->can_retry == NULL) {
    return false;
  }

  if (iface->reset_state == NULL) {
    return false;
  }

  return true;
}

/**
 * @brief Check if a pin interface is valid (all function pointers set)
 * @param iface Interface to validate
 * @return true if interface is valid and usable
 */
static inline bool star_pin_interface_is_valid(const star_pin_interface_t* iface)
{
  if (iface == NULL) {
    return false;
  }

  if (iface->register_pin == NULL) {
    return false;
  }

  if (iface->unregister_pin == NULL) {
    return false;
  }

  if (iface->validate == NULL) {
    return false;
  }

  return true;
}

/**
 * @brief Convenience macro to check if retry is possible through interface
 * Returns false if interface is NULL or invalid
 */
#define STAR_IFACE_CAN_RETRY(iface) ((iface)->can_retry ? (iface)->can_retry((iface)->ctx) : false)

/**
 * @brief Convenience macro to reset error state through interface
 * Returns ESP_OK if interface is NULL (no-op), otherwise calls reset_state
 */
#define STAR_IFACE_RESET_STATE(iface)                                                              \
  ((iface)->reset_state ? (iface)->reset_state((iface)->ctx) : ESP_OK)

/**
 * @brief Convenience macro to register a pin through interface
 * Returns ESP_OK if interface is NULL (no-op)
 */
#define STAR_IFACE_REGISTER_PIN(iface, pin, desc, shared)                                          \
  ((iface)->register_pin ? (iface)->register_pin((iface)->ctx, (pin), (desc), (shared)) : ESP_OK)

/**
 * @brief Convenience macro to unregister a pin through interface
 * Returns ESP_OK if interface is NULL (no-op)
 */
#define STAR_IFACE_UNREGISTER_PIN(iface, pin, desc)                                                \
  ((iface)->unregister_pin ? (iface)->unregister_pin((iface)->ctx, (pin), (desc)) : ESP_OK)

/**
 * @brief Convenience macro to validate pins through interface
 * Returns ESP_OK if interface is NULL (no-op)
 */
#define STAR_IFACE_VALIDATE_PINS(iface)                                                            \
  ((iface)->validate ? (iface)->validate((iface)->ctx) : ESP_OK)

#ifdef __cplusplus
}
#endif

#endif /* STAR_CORE_H */
