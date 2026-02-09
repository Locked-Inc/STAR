/* tests/mocks/mock_rx_bq4050.h */

/**
 * @file mock_rx_bq4050.h
 * @brief Mock BQ4050 battery fuel gauge driver for power monitoring tests
 *
 * @details
 * Provides test double for BQ4050 battery management IC driver (SMBus interface).
 * Enables testing of power monitoring, battery state estimation, and safety
 * shutdown logic without actual BQ4050 hardware.
 *
 * Enables testing of: Battery voltage/current/capacity readings, Cell balancing
 * monitoring, State-of-charge (SoC) estimation, Low battery detection, SMBus
 * communication error handling
 *
 * @par Mock Capabilities: Configurable battery parameters (voltage, current, SoC),
 * Error injection (SMBus timeout, PEC errors), Call history tracking
 *
 * @par Usage: tests/test_rx_bq4050.c
 * @see rx_bq4050.h Real BQ4050 driver
 * @see mock_rx_bus_smbus.h SMBus mock (used by BQ4050)
 * @par NASA Power of 10: ✓ Static allocation
 * @par SOLID: D - Power monitoring depends on BQ4050 interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#ifndef STAR_MOCK_RX_BQ4050_H
#define STAR_MOCK_RX_BQ4050_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_bq4050_max_cells = 4, /**< Maximum number of cells */
} bq4050_constants_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/* Forward declaration for bus manager */
struct rx_bus_manager_s;
typedef struct rx_bus_manager_s rx_bus_manager_t;

/**
 * @struct rx_bq4050_status_t
 * @brief Battery status structure
 */
typedef struct {
  uint16_t voltage_mv;                    /**< Pack voltage (mV) */
  uint16_t cell_voltages_mv[k_bq4050_max_cells]; /**< Cell voltages (mV) */
  int16_t  current_ma;                    /**< Current (mA) */
  int16_t  average_current_ma;            /**< Average current (mA) */
  uint8_t  relative_soc;                  /**< Relative SoC (%) */
  uint8_t  absolute_soc;                  /**< Absolute SoC (%) */
  int16_t  temperature_c;                 /**< Temperature (°C) */
  uint16_t remaining_capacity_mah;        /**< Remaining capacity (mAh) */
  uint16_t full_capacity_mah;             /**< Full charge capacity (mAh) */
  uint16_t design_capacity_mah;           /**< Design capacity (mAh) */
  uint16_t cycle_count;                   /**< Charge cycles */
  uint16_t time_to_empty_min;             /**< Time to empty (min) */
  uint16_t time_to_full_min;              /**< Time to full (min) */
  bool     is_charging;                   /**< Charging flag */
  bool     is_fully_charged;              /**< Fully charged flag */
  bool     is_fully_discharged;           /**< Fully discharged flag */
  bool     is_low_capacity;               /**< Low capacity flag */
} rx_bq4050_status_t;

/**
 * @struct rx_bq4050_config_t
 * @brief BQ4050 configuration
 */
typedef struct {
  uint8_t num_cells; /**< Number of cells */
} rx_bq4050_config_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_bq4050_reset(void);
void mock_bq4050_set_init_return(rx_err_t err);
void mock_bq4050_set_status(uint16_t voltage_mv, int16_t current_ma, uint8_t soc);
void mock_bq4050_set_full_status(const rx_bq4050_status_t* status);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_bq4050_get_init_count(void);
uint32_t mock_bq4050_get_voltage_count(void);
uint32_t mock_bq4050_get_current_count(void);
uint32_t mock_bq4050_get_soc_count(void);
uint32_t mock_bq4050_get_status_count(void);
bool     mock_bq4050_was_initialized(void);

/* =============================================================================
 * Mock BQ4050 API
 * =============================================================================
 */

rx_err_t rx_bq4050_init(rx_bus_manager_t*         manager,
                        const char*               bus_name,
                        const rx_bq4050_config_t* config);

rx_err_t rx_bq4050_read_voltage(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                uint16_t*         voltage_mv);

rx_err_t rx_bq4050_read_current(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                int16_t*          current_ma);

rx_err_t rx_bq4050_read_soc(rx_bus_manager_t* manager,
                            const char*       bus_name,
                            uint8_t*          soc);

rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* STAR_MOCK_RX_BQ4050_H */
