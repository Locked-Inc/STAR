/**
 * @file mock_rx_bmp280.h
 * @brief Mock BMP280 barometric pressure sensor driver for IMU task unit tests
 *
 * @details
 * Provides test double for BMP280 barometric pressure sensor.
 * Enables testing of IMU task initialization and data handling
 * without actual BMP280 hardware.
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* Forward declaration */
struct rx_bus_manager_s;
typedef struct rx_bus_manager_s rx_bus_manager_t;

/**
 * @struct bmp280_data_t
 * @brief BMP280 sensor data output structure (mock version)
 */
typedef struct {
  int32_t  temp_centi_degc; /**< Temperature in centi-degrees Celsius */
  uint32_t press_pa_256;    /**< Pressure in Pa * 256 */
} bmp280_data_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_bmp280_reset(void);
void mock_bmp280_set_init_return(rx_err_t err);
void mock_bmp280_set_read_return(rx_err_t err);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_bmp280_get_init_count(void);
uint32_t mock_bmp280_get_read_count(void);

/* =============================================================================
 * Mock BMP280 API (matches rx_bmp280.h signatures)
 * =============================================================================
 */

rx_err_t rx_bmp280_init(rx_bus_manager_t* manager);
rx_err_t rx_bmp280_read(bmp280_data_t* out);

#ifdef __cplusplus
}
#endif
