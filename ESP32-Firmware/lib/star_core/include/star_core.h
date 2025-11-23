/**
 * @file star_core.h
 * @brief Unified header for star_core interfaces
 *
 * This header provides all interface definitions and validation utilities
 * for the STAR firmware's dependency inversion system.
 */

#ifndef STAR_CORE_H
#define STAR_CORE_H

#include "star_error_interface.h"
#include "star_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

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
  return (iface != NULL && iface->record_error != NULL && iface->can_retry != NULL &&
          iface->reset_state != NULL);
}

/**
 * @brief Check if a pin interface is valid (all function pointers set)
 * @param iface Interface to validate
 * @return true if interface is valid and usable
 */
static inline bool star_pin_interface_is_valid(const star_pin_interface_t* iface)
{
  return (iface != NULL && iface->register_pin != NULL && iface->unregister_pin != NULL &&
          iface->validate != NULL);
}

/**
 * @brief Convenience macro to check if retry is possible through interface
 * Returns false if interface is NULL or invalid
 */
#define STAR_IFACE_CAN_RETRY(iface)                                                                \
  ((iface)->can_retry ? (iface)->can_retry((iface)->ctx) : false)

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
  ((iface)->register_pin ? (iface)->register_pin((iface)->ctx, (pin), (desc), (shared))             \
                         : ESP_OK)

/**
 * @brief Convenience macro to unregister a pin through interface
 * Returns ESP_OK if interface is NULL (no-op)
 */
#define STAR_IFACE_UNREGISTER_PIN(iface, pin, desc)                                                \
  ((iface)->unregister_pin ? (iface)->unregister_pin((iface)->ctx, (pin), (desc))                   \
                           : ESP_OK)

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
