/* lib/star_bus/include/star_bus_adc.h */

#ifndef STAR_COMPONENT_BUS_ADC_H
#define STAR_COMPONENT_BUS_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_adc.h
 * @brief ADC bus operations through the unified bus manager
 *
 * This module provides ADC analog read operations that work through the
 * bus manager. ADC bus is used for reading analog voltages such as
 * motor current sense lines.
 *
 * Key Features:
 * - Raw ADC value reading
 * - Calibrated voltage reading (in millivolts)
 * - Thread-safe through bus manager mutex
 *
 * Example Usage:
 * @code
 * // === Setup ===
 *
 * // Create ADC bus for motor current sense
 * star_bus_config_t* adc_bus = star_bus_config_create_adc(
 *     "motor_current",   // Unique bus name
 *     ADC_UNIT_1,        // ADC unit
 *     ADC_CHANNEL_0,     // ADC channel (GPIO36 on ESP32)
 *     ADC_BITWIDTH_12,   // 12-bit resolution
 *     ADC_ATTEN_DB_11    // 0-3.3V range
 * );
 * star_bus_manager_add_bus(&bus_manager, adc_bus);
 *
 *
 * // === Read Raw ADC Value ===
 *
 * // Read raw ADC counts (0-4095 for 12-bit)
 * int raw_value;
 * star_bus_adc_read_raw(&bus_manager, "motor_current", &raw_value);
 * ESP_LOGI(TAG, "Raw ADC: %d", raw_value);
 *
 *
 * // === Read Calibrated Voltage ===
 *
 * // Read voltage in millivolts
 * int voltage_mv;
 * star_bus_adc_read_voltage(&bus_manager, "motor_current", &voltage_mv);
 * ESP_LOGI(TAG, "Voltage: %d mV (%.2f V)", voltage_mv, voltage_mv / 1000.0f);
 *
 * @endcode
 */

/**
 * @brief Get default ADC operations structure
 *
 * Returns a structure containing function pointers to the default
 * implementations for ADC bus operations.
 *
 * @return star_adc_ops_t Default operations structure
 */
star_adc_ops_t star_bus_adc_get_default_ops(void);

/**
 * @brief Read raw ADC value
 *
 * @param[in]  manager  Bus manager instance
 * @param[in]  bus_name Name of ADC bus
 * @param[out] out_raw  Pointer to store raw ADC value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_adc_read_raw(star_bus_manager_t* manager,
                                 const char*         bus_name,
                                 int*                out_raw);

/**
 * @brief Read calibrated voltage in millivolts
 *
 * @param[in]  manager        Bus manager instance
 * @param[in]  bus_name       Name of ADC bus
 * @param[out] out_voltage_mv Pointer to store voltage in mV
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_adc_read_voltage(star_bus_manager_t* manager,
                                     const char*         bus_name,
                                     int*                out_voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_ADC_H */
