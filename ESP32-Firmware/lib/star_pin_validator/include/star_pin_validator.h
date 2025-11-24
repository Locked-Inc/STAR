/* esp32-firmware/components/star_pin_validator/include/star_pin_validator.h */

#ifndef STAR_PIN_VALIDATOR_H
#define STAR_PIN_VALIDATOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "soc/gpio_num.h"
#include "star_pin_interface.h"

/**
 * @file star_pin_validator.h
 * @brief GPIO pin conflict detection and tracking
 *
 * This module tracks GPIO pin usage across your application to detect conflicts
 * where multiple components try to use the same pin. It supports both exclusive
 * pins and shared pins (like I2C bus lines used by multiple devices).
 *
 * Key Features:
 * - Register pins with descriptions for tracking
 * - Detect conflicts between exclusive pins
 * - Support shared pins (like I2C SDA/SCL)
 * - Thread-safe with mutex protection
 * - Interface wrapper for dependency injection
 *
 * Example Usage:
 * @code
 * // === Basic Pin Registration ===
 *
 * #include "star_pin_validator.h"
 *
 * // Register exclusive pins (cannot be shared)
 * star_register_pin(GPIO_NUM_4, "DHT22 Data", false);
 * star_register_pin(GPIO_NUM_5, "Trigger Pin HCSR04", false);
 * star_register_pin(GPIO_NUM_18, "Echo Pin HCSR04", false);
 *
 * // Register shared pins (like I2C bus shared by multiple devices)
 * star_register_pin(GPIO_NUM_21, "I2C SDA - MPU6050", true);
 * star_register_pin(GPIO_NUM_21, "I2C SDA - BMP280", true);   // OK: shared pin
 * star_register_pin(GPIO_NUM_22, "I2C SCL - shared", true);
 *
 * // Validate all pins for conflicts
 * esp_err_t ret = star_validate_pins();
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Pin conflict detected!");
 *     // Handle conflict - check logs for details
 * }
 *
 *
 * // === Detecting Conflicts ===
 *
 * // This will cause a conflict (same pin registered twice as non-shared)
 * star_register_pin(GPIO_NUM_4, "DHT22 Data", false);
 * star_register_pin(GPIO_NUM_4, "LED Output", false);  // Conflict!
 *
 * ret = star_validate_pins();  // Returns ESP_ERR_INVALID_STATE
 *
 *
 * // === Unregistering Pins ===
 *
 * // When a component is done with a pin, unregister it
 * star_unregister_pin(GPIO_NUM_4, "DHT22 Data");
 *
 *
 * // === Using with Bus Manager ===
 *
 * // Get interface for dependency injection
 * star_pin_interface_t pin_iface;
 * pin_validator_get_interface(&pin_iface);
 *
 * // Inject into bus manager
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // Bus manager can now auto-register pins for buses it manages
 * star_bus_manager_register_all_pins(&bus_manager, register_pin_callback, NULL);
 * star_validate_pins();
 *
 *
 * // === Manual Integration Callback ===
 *
 * // Define callback for bus manager pin registration
 * esp_err_t register_bus_pin(gpio_num_t pin, const char* bus_name,
 *                            const char* usage, void* ctx) {
 *     char desc[64];
 *     snprintf(desc, sizeof(desc), "%s: %s", bus_name, usage);
 *     return star_register_pin(pin, desc, true);  // Shared for I2C/SPI
 * }
 *
 * star_bus_manager_register_all_pins(&bus_manager, register_bus_pin, NULL);
 *
 *
 * // === Cleanup ===
 *
 * star_free_pin_validator();  // Free all resources when shutting down
 * @endcode
 */

#define PIN_VALIDATOR_DESC_MAX_LEN (64)

/**
 * @brief Info for each GPIO pin
 */
typedef struct {
  bool    can_be_shared; /* Whether this pin can be shared */
  uint8_t usage_count;   /* Number of components using this pin */
  char**  users;         /* Dynamically allocated array of descriptions */
} star_pin_info_t;

/**
 * @brief The main validator containing all pin usage information
 */
typedef struct {
  star_pin_info_t   pins[GPIO_NUM_MAX]; /* Every pin on the MCU */
  bool              initialized;        /* Whether the validator has been initialized */
  SemaphoreHandle_t mutex;              /* Mutex for thread safety */
} star_pin_validator_t;

/**
 * @brief Register a pin on the validator
 * @param gpio_num GPIO pin number
 * @param desc Description of what's using this pin
 * @param can_be_shared Whether this pin can be shared with other users
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_register_pin(gpio_num_t gpio_num, const char* desc, bool can_be_shared);

/**
 * @brief Unregister a pin from the validator
 * @param gpio_num GPIO pin number
 * @param desc Description that was used when registering (must match)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_unregister_pin(gpio_num_t gpio_num, const char* desc);

/**
 * @brief Validate all registered pins for conflicts
 * @return ESP_OK if no conflicts, ESP_ERR_INVALID_STATE if conflicts found
 */
esp_err_t star_validate_pins(void);

/**
 * @brief Free the pin validator resources
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_free_pin_validator(void);

/**
 * @brief Create an interface adapter for the pin validator
 *
 * Fills in a star_pin_interface_t structure that wraps the global pin validator
 * implementation. This allows the pin validator to be used through the abstract interface.
 *
 * @param[out] iface Pointer to interface structure to fill in
 */
void pin_validator_get_interface(star_pin_interface_t* iface);

/**
 * @brief Check if the pin validator is initialized (thread-safe).
 *
 * This function safely checks the initialized flag by acquiring the mutex.
 * Useful for components that want to verify validator state before operations.
 *
 * @return true if initialized, false otherwise or if mutex unavailable
 */
bool star_pin_validator_is_initialized(void);

#endif /* STAR_PIN_VALIDATOR_H */
