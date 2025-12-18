/* lib/star_core/include/star_pin_interface.h */

#ifndef STAR_PIN_INTERFACE_H
#define STAR_PIN_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_pin_interface.h
 * @brief Pin validator interface for dependency inversion
 *
 * This interface defines the contract for GPIO pin management in the STAR framework.
 * It follows the Dependency Inversion Principle (DIP), allowing high-level
 * components to depend on this abstract interface rather than concrete implementations.
 *
 * The concrete implementation is provided by star_pin_validator.h.
 *
 * Example Usage:
 * @code
 * // === Creating and Using Pin Interface ===
 *
 * #include "star_pin_interface.h"
 * #include "star_pin_validator.h"
 *
 * // 1. Get interface from global pin validator
 * star_pin_interface_t pin_iface;
 * pin_validator_get_interface(&pin_iface);
 *
 * // 2. Inject into components that manage GPIO pins
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 *
 * // === Using the Interface Directly ===
 *
 * // Register a pin as exclusive (cannot be shared)
 * pin_iface.register_pin(
 *     pin_iface.ctx,
 *     GPIO_NUM_4,
 *     "DHT22 Data Pin",
 *     false  // Not shared
 * );
 *
 * // Register a pin as shared (e.g., I2C bus pins)
 * pin_iface.register_pin(pin_iface.ctx, GPIO_NUM_21, "I2C SDA - IMU", true);
 * pin_iface.register_pin(pin_iface.ctx, GPIO_NUM_21, "I2C SDA - EEPROM", true);
 * pin_iface.register_pin(pin_iface.ctx, GPIO_NUM_22, "I2C SCL - shared", true);
 *
 * // Validate all pins for conflicts
 * esp_err_t ret = pin_iface.validate(pin_iface.ctx);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Pin conflict detected!");
 * }
 *
 * // Unregister when done
 * pin_iface.unregister_pin(pin_iface.ctx, GPIO_NUM_4, "DHT22 Data Pin");
 *
 *
 * // === Using with Bus Manager ===
 *
 * // The bus manager uses pin interface to track pins
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // When you add buses, pins are automatically registered
 * star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
 *     "imu_i2c", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, i2c_bus);
 * // ^-- This registers GPIO_NUM_21 and GPIO_NUM_22 through pin_iface
 *
 * // Validate all registered pins
 * star_validate_pins();  // Uses global validator
 *
 *
 * // === NULL Interface Handling ===
 *
 * // Bus manager accepts NULL pin_iface (pin validation disabled)
 * star_bus_manager_init(&bus_manager, "main", &error_iface, NULL);
 * // ^-- Works fine, but no pin conflict detection
 *
 *
 * // === Implementing Custom Pin Validator ===
 *
 * // You can create custom validators by implementing the interface
 * typedef struct {
 *     uint32_t used_pins;  // Simple bitmask for pins 0-31
 * } simple_validator_t;
 *
 * esp_err_t simple_register(void* ctx, int pin, const char* desc, bool shared) {
 *     simple_validator_t* v = (simple_validator_t*)ctx;
 *     if (!shared && (v->used_pins & (1 << pin))) {
 *         return ESP_ERR_INVALID_STATE;  // Conflict!
 *     }
 *     v->used_pins |= (1 << pin);
 *     return ESP_OK;
 * }
 *
 * esp_err_t simple_unregister(void* ctx, int pin, const char* desc) {
 *     simple_validator_t* v = (simple_validator_t*)ctx;
 *     v->used_pins &= ~(1 << pin);
 *     return ESP_OK;
 * }
 *
 * esp_err_t simple_validate(void* ctx) {
 *     return ESP_OK;  // Already checked in register
 * }
 *
 * // Create custom interface
 * simple_validator_t my_validator = { .used_pins = 0 };
 * star_pin_interface_t custom_iface = {
 *     .register_pin = simple_register,
 *     .unregister_pin = simple_unregister,
 *     .validate = simple_validate,
 *     .ctx = &my_validator
 * };
 *
 *
 * // === Detecting Pin Conflicts ===
 *
 * // This will fail because GPIO_NUM_4 is registered twice as exclusive
 * pin_iface.register_pin(pin_iface.ctx, GPIO_NUM_4, "Sensor A", false);
 * ret = pin_iface.register_pin(pin_iface.ctx, GPIO_NUM_4, "Sensor B", false);
 * // ^-- Returns ESP_ERR_INVALID_STATE (conflict!)
 *
 * // Or validate after all registrations
 * star_register_pin(GPIO_NUM_4, "Sensor A", false);
 * star_register_pin(GPIO_NUM_4, "Sensor B", false);
 * ret = star_validate_pins();  // Detects conflict here
 * @endcode
 */

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
