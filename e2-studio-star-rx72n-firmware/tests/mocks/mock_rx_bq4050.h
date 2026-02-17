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
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - Power monitoring depends on BQ4050 interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#pragma once

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
  uint16_t voltage_mv;                           /**< Pack voltage (mV) */
  uint16_t cell_voltages_mv[k_bq4050_max_cells]; /**< Cell voltages (mV) */
  int16_t  current_ma;                           /**< Current (mA) */
  int16_t  average_current_ma;                   /**< Average current (mA) */
  uint8_t  relative_soc;                         /**< Relative SoC (%) */
  uint8_t  absolute_soc;                         /**< Absolute SoC (%) */
  int16_t  temperature_c;                        /**< Temperature (degC) */
  uint16_t remaining_capacity_mah;               /**< Remaining capacity (mAh) */
  uint16_t full_capacity_mah;                    /**< Full charge capacity (mAh) */
  uint16_t design_capacity_mah;                  /**< Design capacity (mAh) */
  uint16_t cycle_count;                          /**< Charge cycles */
  uint16_t time_to_empty_min;                    /**< Time to empty (min) */
  uint16_t time_to_full_min;                     /**< Time to full (min) */
  bool     is_charging;                          /**< Charging flag */
  bool     is_fully_charged;                     /**< Fully charged flag */
  bool     is_fully_discharged;                  /**< Fully discharged flag */
  bool     is_low_capacity;                      /**< Low capacity flag */
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

/**
 * @brief Inject an error return value for all BQ4050 read operations
 *
 * @details
 * Configures the static s_read_return variable so that all subsequent calls to
 * rx_bq4050_read_voltage(), rx_bq4050_read_current(), rx_bq4050_read_soc(),
 * and rx_bq4050_read_status() return @p err instead of k_rx_ok.
 * Use this to simulate I2C communication failures in tests.
 *
 * Call mock_bq4050_reset() to restore the default return value (k_rx_ok).
 *
 * @param[in] err Error code to inject; typically k_rx_err_i2c_nack,
 *                k_rx_err_timeout, or k_rx_err_bus_busy
 *
 * @pre mock_bq4050_reset() has been called at least once (setUp) to establish
 *      a known baseline
 * @pre err is a valid rx_err_t value
 *
 * @post s_read_return == err
 * @post All subsequent read calls return err until mock_bq4050_reset() is called
 *
 * @note Not thread-safe; single-threaded test use only
 *
 * @code
 * // Simulate I2C NACK and verify error handling:
 * mock_bq4050_set_read_return(k_rx_err_i2c_nack);
 * rx_bq4050_status_t out;
 * (void)memset(&out, 0, sizeof(out));
 * rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &out, 4);
 * TEST_ASSERT_EQUAL(k_rx_err_i2c_nack, err);
 * // Restore normal operation
 * mock_bq4050_reset();
 * @endcode
 *
 * @see mock_bq4050_reset()           Restore default return value (k_rx_ok)
 * @see mock_bq4050_set_init_return() Inject errors into rx_bq4050_init()
 * @see rx_bq4050_read_status()       Main read function affected by this setting
 *
 * @since Version 1.0.0
 */
void mock_bq4050_set_read_return(rx_err_t err);
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

rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config);

rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv);

rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma);

rx_err_t rx_bq4050_read_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc);

/**
 * @brief Read full battery status from mock BQ4050 fuel gauge
 *
 * @details
 * Returns the full battery status struct from the mock's internal state.
 * Increments the status call counter. If the injected return code is not
 * k_rx_ok, the status struct is left unmodified.
 *
 * @param[in]  manager   Bus manager handle (ignored by mock, may be NULL)
 * @param[in]  bus_name  SMBus channel name (ignored by mock)
 * @param[out] status    Pointer to status struct to populate; must not be NULL
 * @param[in]  num_cells Number of cells to read voltages for; must be in
 *                       [1, k_bq4050_max_cells] (same constraint as production driver)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Status written successfully
 * @retval k_rx_err_null_ptr    status pointer is NULL
 * @retval k_rx_err_invalid_arg num_cells is 0 or greater than k_bq4050_max_cells (4)
 * @retval (injected)           Any error set via mock_bq4050_set_read_return()
 *
 * @pre mock_bq4050_reset() or mock_bq4050_set_full_status() called to
 *      configure desired return values
 * @pre status != NULL
 * @pre num_cells in [1, k_bq4050_max_cells]
 * @post mock_bq4050_get_status_count() incremented by one (only on valid num_cells)
 * @post *status populated with mock battery state if return is k_rx_ok
 *
 * @note Thread-unsafe; single-threaded test use only
 *
 * @code
 * // Configure mock and read status:
 * mock_bq4050_reset();
 * mock_bq4050_set_full_status(&expected_status);
 * rx_bq4050_status_t out;
 * rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &out, 4);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_one,
 *                          mock_bq4050_get_status_count());
 *
 * // Inject a read error:
 * mock_bq4050_set_read_return(k_rx_err_i2c_nack);
 * err = rx_bq4050_read_status(nullptr, "i2c0", &out, 4);
 * TEST_ASSERT_EQUAL(k_rx_err_i2c_nack, err);
 * @endcode
 *
 * @see mock_bq4050_set_full_status() Configure full status response
 * @see mock_bq4050_set_read_return() Inject read errors into this function
 * @see mock_bq4050_get_status_count() Verify call count
 *
 * @since Version 1.1.0
 */
[[nodiscard]] rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                                             const char*         bus_name,
                                             rx_bq4050_status_t* status,
                                             uint8_t             num_cells);

#ifdef __cplusplus
}
#endif
