/**
 * @file star_sensor_ds18b20.h
 * @brief DS18B20 temperature sensor driver API over 1-Wire
 * @details
 * Provides interface for Dallas/Maxim DS18B20 digital temperature sensor using 1-Wire protocol
 * via the bus manager. Supports configurable resolution (9-12 bit), temperature conversion, and
 * both single-device and multi-device configurations with ROM addressing.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_SENSOR_DS18B20_H
#define STAR_SENSOR_DS18B20_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_bus_manager.h"

/**
 * @file star_sensor_ds18b20.h
 * @brief DS18B20 digital temperature sensor driver using 1-Wire bus
 *
 * This library provides an interface for the DS18B20 temperature sensor
 * through the STAR bus manager's 1-Wire implementation. The DS18B20 is
 * a digital thermometer with programmable resolution (9-12 bits).
 *
 * Key Features:
 * - Temperature range: -55°C to +125°C
 * - Programmable resolution: 9, 10, 11, or 12 bits
 * - CRC error checking
 * - Parasitic power support
 * - Multiple sensors on same bus
 *
 * Example Usage:
 * @code
 * // === Initialize DS18B20 ===
 *
 * #include "star_sensor_ds18b20.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Create 1-Wire bus
 * star_bus_config_t* onewire_bus = star_bus_config_create_onewire(
 *     "temp_onewire",    // Bus name
 *     GPIO_NUM_4,        // Data pin
 *     false              // External power (not parasitic)
 * );
 * star_bus_manager_add_bus(&bus_manager, onewire_bus);
 *
 * // Initialize DS18B20 sensor
 * star_ds18b20_handle_t temp_sensor;
 * star_ds18b20_config_t config = {
 *     .bus_manager = &bus_manager,
 *     .bus_name = "temp_onewire",
 *     .resolution = k_star_ds18b20_resolution_12_bit,
 *     .use_rom = false,  // Skip ROM (single sensor on bus)
 * };
 *
 * esp_err_t ret = star_sensor_ds18b20_init(&temp_sensor, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init DS18B20: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Read Temperature ===
 *
 * float temperature_c;
 * ret = star_sensor_ds18b20_read_temp(&temp_sensor, &temperature_c);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Temperature: %.2f °C", temperature_c);
 * }
 *
 *
 * // === Change Resolution ===
 *
 * star_sensor_ds18b20_set_resolution(&temp_sensor, k_star_ds18b20_resolution_10_bit);
 *
 *
 * // === Cleanup ===
 *
 * star_sensor_ds18b20_deinit(&temp_sensor);
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief DS18B20 temperature resolution options
 */
typedef enum {
  k_star_ds18b20_resolution_9_bit  = 0, /**< 9-bit resolution (0.5°C, 93.75ms conversion) */
  k_star_ds18b20_resolution_10_bit = 1, /**< 10-bit resolution (0.25°C, 187.5ms conversion) */
  k_star_ds18b20_resolution_11_bit = 2, /**< 11-bit resolution (0.125°C, 375ms conversion) */
  k_star_ds18b20_resolution_12_bit = 3, /**< 12-bit resolution (0.0625°C, 750ms conversion) */
} star_ds18b20_resolution_t;

/**
 * @brief DS18B20 configuration structure
 */
typedef struct {
  star_bus_manager_t*       bus_manager; /**< Bus manager instance */
  const char*               bus_name;    /**< 1-Wire bus name */
  star_ds18b20_resolution_t resolution;  /**< Temperature resolution */
  bool                      use_rom;     /**< Use ROM address (true for multiple sensors) */
  uint64_t                  rom_code;    /**< 64-bit ROM code (if use_rom is true) */
} star_ds18b20_config_t;

/**
 * @brief DS18B20 sensor handle structure
 */
typedef struct {
  star_bus_manager_t*       bus_manager; /**< Bus manager reference */
  const char*               bus_name;    /**< 1-Wire bus name */
  star_ds18b20_resolution_t resolution;  /**< Current resolution */
  bool                      use_rom;     /**< Use ROM addressing */
  uint64_t                  rom_code;    /**< Device ROM code */
  bool                      initialized; /**< Initialization flag */
} star_ds18b20_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize DS18B20 temperature sensor
 *
 * @param[out] handle Pointer to DS18B20 handle structure. Must not be NULL.
 * @param[in]  config Pointer to DS18B20 configuration. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_ds18b20_init(star_ds18b20_handle_t*       handle,
                                   const star_ds18b20_config_t* config);

/**
 * @brief Deinitialize DS18B20 sensor
 *
 * @param[in] handle Pointer to initialized DS18B20 handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_ds18b20_deinit(star_ds18b20_handle_t* handle);

/**
 * @brief Read temperature from DS18B20 sensor
 *
 * Triggers a temperature conversion and reads the result. The conversion
 * time depends on the configured resolution (93.75ms to 750ms).
 *
 * @param[in]  handle          Pointer to initialized DS18B20 handle. Must not be NULL.
 * @param[out] out_temperature Pointer to store temperature in Celsius. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_ds18b20_read_temp(star_ds18b20_handle_t* handle, float* out_temperature);

/**
 * @brief Set DS18B20 temperature resolution
 *
 * Changes the sensor's measurement resolution. Higher resolution provides
 * more precision but takes longer to convert.
 *
 * @param[in] handle     Pointer to initialized DS18B20 handle. Must not be NULL.
 * @param[in] resolution New resolution setting.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_ds18b20_set_resolution(star_ds18b20_handle_t*    handle,
                                             star_ds18b20_resolution_t resolution);

/**
 * @brief Read DS18B20 ROM code (64-bit unique ID)
 *
 * Reads the device's unique 64-bit ROM code. Useful for identifying multiple
 * sensors on the same bus.
 *
 * @param[in]  handle      Pointer to initialized DS18B20 handle. Must not be NULL.
 * @param[out] out_rom     Pointer to store ROM code. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_ds18b20_read_rom(star_ds18b20_handle_t* handle, uint64_t* out_rom);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_DS18B20_H */
