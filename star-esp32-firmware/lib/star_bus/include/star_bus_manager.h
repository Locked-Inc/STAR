/**
 * @file star_bus_manager.h
 * @brief Unified bus manager API for I2C, SPI, UART, GPIO, ADC, and 1-Wire protocols
 * @details
 * The bus manager provides a central registry for managing multiple bus configurations
 * with thread-safe operations. Supports dependency injection of error handlers and pin
 * validators for loose coupling following the Dependency Inversion Principle.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_MANAGER_H
#define STAR_COMPONENT_BUS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "star_bus_manager_types.h"

/* --- Configuration Constants --- */

/**
 * @brief Bus manager configuration constants
 */
typedef enum {
  k_bus_manager_mutex_timeout_ms = 1000, /**< Default mutex timeout (ms) for bus operations */
} bus_manager_config_t;

/**
 * Key Features:
 * - Thread-safe bus management with mutex protection
 * - Support for multiple bus types (I2C, SPI, GPIO, DHT22, etc.)
 * - Automatic pin registration and conflict detection
 * - Safe bus access via callback pattern (prevents use-after-free)
 *
 * Example Usage:
 * @code
 * // === Basic Bus Manager Setup ===
 *
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 * #include "star_bus_i2c.h"
 * #include "star_error_handler.h"
 * #include "star_pin_validator.h"
 *
 * // 1. Create error handler and pin validator interfaces
 * error_handler_t error_handler;
 * error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);
 *
 * star_error_interface_t error_iface;
 * star_pin_interface_t pin_iface;
 * error_handler_get_interface(&error_iface, &error_handler);
 * pin_validator_get_interface(&pin_iface);
 *
 * // 2. Initialize bus manager with dependency injection
 * star_bus_manager_t bus_manager;
 * esp_err_t ret = star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init bus manager: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 * // 3. Create and add an I2C bus configuration
 * star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
 *     "sensor_i2c",      // Bus name
 *     I2C_NUM_0,         // I2C port
 *     0x68,              // Device address (MPU6050)
 *     GPIO_NUM_21,       // SDA pin
 *     GPIO_NUM_22,       // SCL pin
 *     400000             // Clock speed (400kHz)
 * );
 * star_bus_manager_add_bus(&bus_manager, i2c_bus);
 *
 * // 4. Create and add an SPI bus configuration
 * spi_device_interface_config_t spi_dev_cfg = {
 *     .clock_speed_hz = 1000000,
 *     .mode = 0,
 *     .spics_io_num = GPIO_NUM_5,
 *     .queue_size = 7,
 * };
 * star_bus_config_t* spi_bus = star_bus_config_create_spi_device(
 *     "flash_spi",       // Bus name
 *     SPI2_HOST,         // SPI host
 *     GPIO_NUM_23,       // COPI
 *     GPIO_NUM_19,       // CIPO
 *     GPIO_NUM_18,       // SCLK
 *     SPI_DMA_CH_AUTO,   // DMA channel
 *     &spi_dev_cfg
 * );
 * star_bus_manager_add_bus(&bus_manager, spi_bus);
 *
 * // 5. Use the buses through protocol-specific functions
 * uint8_t reg_data[2];
 * star_bus_i2c_read(&bus_manager, "sensor_i2c", reg_data, 2, 0x3B, NULL);
 *
 *
 * // === Safe Bus Access Pattern ===
 *
 * // Use star_bus_manager_with_bus() for safe access (prevents use-after-free)
 * esp_err_t my_callback(star_bus_config_t* bus, void* ctx) {
 *     // Safely access bus while mutex is held
 *     ESP_LOGI(TAG, "Bus type: %s", star_bus_type_to_string(bus->type));
 *     return ESP_OK;
 * }
 *
 * star_bus_manager_with_bus(&bus_manager, "sensor_i2c", my_callback, NULL);
 *
 *
 * // === Pin Registration for Conflict Detection ===
 *
 * // Callback to register pins with your tracking system
 * esp_err_t register_pin_cb(gpio_num_t pin, const char* bus, const char* usage, void* ctx) {
 *     ESP_LOGI(TAG, "Registering pin %d for %s (%s)", pin, bus, usage);
 *     return star_register_pin(pin, usage, false);
 * }
 *
 * star_bus_manager_register_all_pins(&bus_manager, register_pin_cb, NULL);
 * star_validate_pins();  // Check for conflicts
 *
 *
 * // === Cleanup ===
 *
 * // Remove specific bus
 * star_bus_manager_remove_bus(&bus_manager, "sensor_i2c");
 *
 * // Or deinit all (removes all buses and cleans up)
 * star_bus_manager_deinit(&bus_manager);
 * error_handler_deinit(&error_handler);
 * @endcode
 */

/* --- Public Functions --- */

/**
 * @brief Initialize an I2C bus manager instance.
 *
 * Allocates resources (like a mutex) for the manager. Must be called before
 * any other manager functions.
 *
 * @param[out] manager     Pointer to a bus manager structure to initialize. Must not be NULL.
 * @param[in]  tag         Optional tag string for logging associated with this manager instance. If NULL or empty, a default tag is used.
 * @param[in]  error_iface Optional error handler interface (can be NULL).
 * @param[in]  pin_iface   Optional pin validator interface (can be NULL).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if manager is NULL, ESP_ERR_NO_MEM if mutex creation fails.
 */
esp_err_t star_bus_manager_init(star_bus_manager_t*     manager,
                                const char*             tag,
                                star_error_interface_t* error_iface,
                                star_pin_interface_t*   pin_iface);

/**
 * @brief Add a new I2C bus/device configuration to the manager.
 *
 * Adds a previously created configuration (via star_bus_config_create_i2c)
 * to the manager's internal list. The manager takes ownership conceptually,
 * but the configuration still needs to be destroyed eventually (typically via
 * star_bus_manager_remove_bus or star_bus_manager_deinit).
 * The configuration must have a unique name.
 * This function is thread-safe.
 *
 * @param[in] manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] config  Pointer to the I2C bus configuration structure to add. Must not be NULL and must have a valid name.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if manager or config is invalid, ESP_ERR_INVALID_STATE if a bus with the same name already exists or mutex error, ESP_ERR_TIMEOUT if mutex times out.
 */
esp_err_t star_bus_manager_add_bus(star_bus_manager_t* manager, star_bus_config_t* config);

/**
 * @brief Find an I2C bus configuration by its unique name.
 *
 * Searches the manager's list for a configuration matching the given name.
 * This function is thread-safe.
 *
 * @warning The returned pointer can become invalid if another thread removes the bus.
 *          For safe access, use star_bus_manager_with_bus() instead.
 *
 * @param[in] manager Pointer to the initialized bus manager. Can be NULL (returns NULL).
 * @param[in] name    The unique name of the bus configuration to find. Can be NULL (returns NULL).
 * @return star_bus_config_t* Pointer to the found configuration structure, or NULL if not found or an error occurs (e.g., mutex timeout). The returned pointer is still managed by the bus manager.
 */
star_bus_config_t* star_bus_manager_find_bus(const star_bus_manager_t* manager, const char* name);

/**
 * @brief Callback function type for star_bus_manager_with_bus
 */
typedef esp_err_t (*star_bus_callback_t)(star_bus_config_t* bus, void* user_ctx);

/**
 * @brief Safely access a bus configuration through a callback.
 *
 * This function finds a bus by name and invokes the provided callback
 * while holding the manager's mutex, preventing use-after-free issues
 * that can occur with star_bus_manager_find_bus().
 *
 * The callback is invoked with the mutex held, so operations within
 * the callback are thread-safe with respect to bus removal.
 *
 * @param[in] manager  Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name     The unique name of the bus configuration. Must not be NULL.
 * @param[in] callback Function to invoke with the found bus. Must not be NULL.
 * @param[in] user_ctx User context passed to the callback.
 * @return esp_err_t ESP_OK if callback executed successfully, ESP_ERR_NOT_FOUND if bus not found,
 *                   ESP_ERR_INVALID_ARG if arguments invalid, ESP_ERR_TIMEOUT on mutex timeout,
 *                   or the error code returned by the callback.
 */
esp_err_t star_bus_manager_with_bus(star_bus_manager_t* manager,
                                    const char*         name,
                                    star_bus_callback_t callback,
                                    void*               user_ctx);

/**
 * @brief Remove an I2C bus configuration from the manager and destroy it.
 *
 * Finds the configuration by name, removes it from the manager's list,
 * deinitializes the associated I2C driver (if initialized), and frees the
 * configuration structure memory (via star_bus_config_destroy).
 * This function is thread-safe (acquires mutex initially, releases before deinit/destroy).
 *
 * @param[in] manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name    The unique name of the bus configuration to remove and destroy. Must not be NULL.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if manager or name is invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_TIMEOUT if mutex times out, or an error code from the underlying deinitialization/destroy process.
 */
esp_err_t star_bus_manager_remove_bus(star_bus_manager_t* manager, const char* name);

/**
 * @brief Deinitialize the bus manager and all managed I2C bus configurations.
 *
 * Iterates through all I2C bus configurations currently managed, deinitializes
 * each one (if initialized), destroys each configuration structure (freeing memory),
 * and finally releases the manager's resources (mutex).
 * After this call, the manager and all its previously managed configurations are invalid.
 *
 * @param[in] manager Pointer to the bus manager to deinitialize. Must not be NULL.
 * @return esp_err_t ESP_OK if all deinitializations and cleanup were successful, ESP_ERR_INVALID_ARG if manager is NULL, or the first error code encountered during the deinitialization/destroy process of any bus.
 */
esp_err_t star_bus_manager_deinit(star_bus_manager_t* manager);

/**
 * @brief Iterates through all I2C buses/devices managed by the manager and calls a
 *        user-provided function to register each configured pin (SDA, SCL).
 *
 * This allows decoupling the bus manager from a specific pin validation/tracking system.
 * The user provides a callback function that conforms to `pin_registration_function_t`.
 * This function will be called for the SDA and SCL pins associated with each I2C
 * configuration in the manager.
 * This function is thread-safe (acquires mutex during iteration).
 *
 * @param[in] manager  Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] reg_func The user's pin registration callback function. Must not be NULL.
 * @param[in] user_ctx A user context pointer to be passed to the registration function.
 * @return esp_err_t ESP_OK if iteration completed (doesn't guarantee all registrations succeeded, check reg_func's return), ESP_ERR_INVALID_ARG if manager or reg_func is NULL, ESP_ERR_TIMEOUT if mutex times out. Returns the first error reported by the callback.
 */
esp_err_t star_bus_manager_register_all_pins(const star_bus_manager_t*   manager,
                                             pin_registration_function_t reg_func,
                                             void*                       user_ctx);

/* --- Unified Bus Interface --- */

/**
 * @brief Write a digital value to a GPIO bus pin (unified interface)
 *
 * This is a convenience wrapper that dispatches to the appropriate GPIO bus function.
 * The bus must be of type k_star_bus_type_gpio.
 *
 * @param[in] manager   Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] bus_name  Name of the GPIO bus. Must not be NULL.
 * @param[in] pin_index Index of the pin in the GPIO bus configuration.
 * @param[in] value     Digital value to write (0 or non-zero).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if bus type is not GPIO,
 *                   or error codes from the underlying GPIO operation.
 */
esp_err_t star_bus_write_digital(star_bus_manager_t* manager,
                                 const char*         bus_name,
                                 uint8_t             pin_index,
                                 uint8_t             value);

/**
 * @brief Read a digital value from a GPIO bus pin (unified interface)
 *
 * This is a convenience wrapper that dispatches to the appropriate GPIO bus function.
 * The bus must be of type k_star_bus_type_gpio.
 *
 * @param[in]  manager   Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  bus_name  Name of the GPIO bus. Must not be NULL.
 * @param[in]  pin_index Index of the pin in the GPIO bus configuration.
 * @param[out] value     Pointer to store the read digital value.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if bus type is not GPIO,
 *                   or error codes from the underlying GPIO operation.
 */
esp_err_t star_bus_read_digital(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             pin_index,
                                uint8_t*            value);

/**
 * @brief Read raw ADC value from an ADC bus (unified interface)
 *
 * This is a convenience wrapper that dispatches to the appropriate ADC bus function.
 * The bus must be of type k_star_bus_type_adc.
 *
 * @param[in]  manager  Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  bus_name Name of the ADC bus. Must not be NULL.
 * @param[out] out_raw  Pointer to store the raw ADC value (int32_t for explicit typing).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if bus type is not ADC,
 *                   or error codes from the underlying ADC operation.
 */
esp_err_t
star_bus_read_analog_raw(star_bus_manager_t* manager, const char* bus_name, int32_t* out_raw);

/**
 * @brief Read calibrated voltage in millivolts from an ADC bus (unified interface)
 *
 * This is a convenience wrapper that dispatches to the appropriate ADC bus function.
 * The bus must be of type k_star_bus_type_adc.
 *
 * @param[in]  manager        Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  bus_name       Name of the ADC bus. Must not be NULL.
 * @param[out] out_voltage_mv Pointer to store the voltage in millivolts (int32_t for explicit typing).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if bus type is not ADC,
 *                   or error codes from the underlying ADC operation.
 */
esp_err_t
star_bus_read_analog_mv(star_bus_manager_t* manager, const char* bus_name, int32_t* out_voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_MANAGER_H */
