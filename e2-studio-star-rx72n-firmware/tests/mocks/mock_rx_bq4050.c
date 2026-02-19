/* tests/mocks/mock_rx_bq4050.c */

/**
 * @file mock_rx_bq4050.c
 * @brief Mock BQ4050 Battery Fuel Gauge Implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_rx_bq4050.h"

#include <string.h>

/* Static return values */
static rx_err_t s_init_return = k_rx_ok;
static rx_err_t s_read_return = k_rx_ok;

/* Static call counts */
static uint32_t s_init_count    = 0;
static uint32_t s_voltage_count = 0;
static uint32_t s_current_count = 0;
static uint32_t s_soc_count     = 0;
static uint32_t s_status_count  = 0;

/* Configured status */
static rx_bq4050_status_t s_status = {0};

void mock_bq4050_reset(void)
{
  s_init_return   = k_rx_ok;
  s_read_return   = k_rx_ok;
  s_init_count    = 0;
  s_voltage_count = 0;
  s_current_count = 0;
  s_soc_count     = 0;
  s_status_count  = 0;

  (void)memset(&s_status, 0, sizeof(s_status));
  s_status.voltage_mv   = 12000;
  s_status.current_ma   = 0;
  s_status.relative_soc = 100;
}

void mock_bq4050_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_bq4050_set_read_return(rx_err_t err)
{
  s_read_return = err;
}

void mock_bq4050_set_status(uint16_t voltage_mv, int16_t current_ma, uint8_t soc)
{
  s_status.voltage_mv   = voltage_mv;
  s_status.current_ma   = current_ma;
  s_status.relative_soc = soc;
}

void mock_bq4050_set_full_status(const rx_bq4050_status_t* status)
{
  if (status != nullptr) {
    (void)memcpy(&s_status, status, sizeof(s_status));
  }
}

uint32_t mock_bq4050_get_init_count(void)
{
  return s_init_count;
}
uint32_t mock_bq4050_get_voltage_count(void)
{
  return s_voltage_count;
}
uint32_t mock_bq4050_get_current_count(void)
{
  return s_current_count;
}
uint32_t mock_bq4050_get_soc_count(void)
{
  return s_soc_count;
}
uint32_t mock_bq4050_get_status_count(void)
{
  return s_status_count;
}
bool mock_bq4050_was_initialized(void)
{
  return s_init_count > 0;
}

rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config)
{
  (void)manager;
  (void)bus_name;
  (void)config;

  s_init_count++;

  return s_init_return;
}

rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv)
{
  (void)manager;
  (void)bus_name;

  s_voltage_count++;

  if (voltage_mv == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    *voltage_mv = s_status.voltage_mv;
  }

  return s_read_return;
}

rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma)
{
  (void)manager;
  (void)bus_name;

  s_current_count++;

  if (current_ma == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    *current_ma = s_status.current_ma;
  }

  return s_read_return;
}

rx_err_t rx_bq4050_read_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc)
{
  (void)manager;
  (void)bus_name;

  s_soc_count++;

  if (soc == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    *soc = s_status.relative_soc;
  }

  return s_read_return;
}

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
 * @param[in]  num_cells Number of cells to read voltages for (ignored by mock)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok         Status written successfully
 * @retval k_rx_err_null_ptr status pointer is NULL
 * @retval (injected)      Any error set via mock_bq4050_set_read_return()
 *
 * @pre mock_bq4050_reset() or mock_bq4050_set_full_status() called to
 *      configure desired return values
 * @pre status != NULL
 * @post mock_bq4050_get_status_count() incremented by one
 * @post *status populated with mock battery state if return is k_rx_ok
 *
 * @note Thread-unsafe; single-threaded test use only
 *
 * @code
 * // Inject a read error and verify the task handles it:
 * mock_bq4050_set_read_return(k_rx_err_i2c_nack);
 * rx_bq4050_status_t status;
 * rx_err_t err = rx_bq4050_read_status(nullptr, "i2c0", &status, 4);
 * TEST_ASSERT_EQUAL(k_rx_err_i2c_nack, err);
 *
 * // Normal read with pre-configured status:
 * mock_bq4050_reset();
 * mock_bq4050_set_full_status(&expected_status);
 * err = rx_bq4050_read_status(nullptr, "i2c0", &status, 4);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL_UINT32(1, mock_bq4050_get_status_count());
 * @endcode
 *
 * @see mock_bq4050_set_full_status() Configure full status response
 * @see mock_bq4050_set_read_return() Inject read errors
 * @see mock_bq4050_get_status_count() Verify call count
 *
 * @since Version 1.1.0
 */
rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status,
                               uint8_t             num_cells)
{
  (void)manager;
  (void)bus_name;
  (void)num_cells;

  s_status_count++;

  if (status == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    (void)memcpy(status, &s_status, sizeof(*status));
  }

  return s_read_return;
}
