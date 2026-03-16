/**
 * @file mock_rx_ds18b20.h
 * @brief Mock DS18B20 temperature sensor driver for thermal monitoring tests
 *
 * @details
 * Provides test double for DS18B20 digital temperature sensor (1-Wire interface).
 * Enables testing of temperature monitoring, multi-sensor management, and
 * thermal protection without actual DS18B20 hardware.
 *
 * Enables testing of: Temperature readings (-55degC to +125degC), Multi-sensor
 * addressing (64-bit ROM codes), Resolution configuration (9-12 bit), CRC
 * validation, 1-Wire error handling
 *
 * @par Mock Capabilities: Configurable temperature values, Multi-device
 * simulation (up to 8 sensors), Error injection (CRC errors, conversion timeout),
 * Call tracking
 *
 * @par Usage: tests/test_rx_ds18b20.c
 * @see rx_ds18b20.h Real DS18B20 driver
 * @see mock_rx_onewire_hw.h 1-Wire hardware mock
 * @par NASA Power of 10: [OK] Static allocation, bounded loops
 * @par SOLID: D - Thermal monitoring depends on DS18B20 interface
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum ds18b20_resolution_t
 * @brief DS18B20 resolution settings
 */
typedef enum : uint8_t {
  k_ds18b20_resolution_9bit  = 0, /**< 9-bit resolution */
  k_ds18b20_resolution_10bit = 1, /**< 10-bit resolution */
  k_ds18b20_resolution_11bit = 2, /**< 11-bit resolution */
  k_ds18b20_resolution_12bit = 3, /**< 12-bit resolution */
} ds18b20_resolution_t;

/* Forward declaration for bus manager */
struct rx_bus_manager_s;
typedef struct rx_bus_manager_s rx_bus_manager_t;

/**
 * @struct rx_ds18b20_config_t
 * @brief DS18B20 configuration
 */
typedef struct {
  rx_bus_manager_t*    bus_manager;      /**< Bus manager */
  const char*          bus_name;         /**< 1-Wire bus name */
  ds18b20_resolution_t resolution;       /**< Temperature resolution */
  bool                 use_rom_matching; /**< Use ROM matching */
} rx_ds18b20_config_t;

/**
 * @struct rx_ds18b20_handle_t
 * @brief DS18B20 driver handle
 */
typedef struct {
  ds18b20_resolution_t resolution;  /**< Configured resolution */
  bool                 initialized; /**< Init flag */
} rx_ds18b20_handle_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_ds18b20_reset(void);
void mock_ds18b20_set_init_return(rx_err_t err);
void mock_ds18b20_set_resolution_return(rx_err_t err);
void mock_ds18b20_set_trigger_return(rx_err_t err);
void mock_ds18b20_set_read_return(rx_err_t err);
void mock_ds18b20_set_temperature(float temp_celsius);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_ds18b20_get_init_count(void);
uint32_t mock_ds18b20_get_trigger_count(void);
uint32_t mock_ds18b20_get_read_count(void);
bool     mock_ds18b20_was_initialized(void);

/* =============================================================================
 * Mock DS18B20 API
 * =============================================================================
 */

rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle, const rx_ds18b20_config_t* config);
rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle);
rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle, ds18b20_resolution_t resolution);
rx_err_t rx_ds18b20_trigger_conversion(rx_ds18b20_handle_t* handle);
rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temp_celsius);

#ifdef __cplusplus
}
#endif
