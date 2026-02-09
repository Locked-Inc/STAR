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

void mock_bq4050_set_init_return(rx_err_t err) { s_init_return = err; }

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

uint32_t mock_bq4050_get_init_count(void)    { return s_init_count; }
uint32_t mock_bq4050_get_voltage_count(void) { return s_voltage_count; }
uint32_t mock_bq4050_get_current_count(void) { return s_current_count; }
uint32_t mock_bq4050_get_soc_count(void)     { return s_soc_count; }
uint32_t mock_bq4050_get_status_count(void)  { return s_status_count; }
bool     mock_bq4050_was_initialized(void)   { return s_init_count > 0; }

rx_err_t rx_bq4050_init(rx_bus_manager_t*         manager,
                        const char*               bus_name,
                        const rx_bq4050_config_t* config)
{
  (void)manager;
  (void)bus_name;
  (void)config;

  s_init_count++;

  return s_init_return;
}

rx_err_t rx_bq4050_read_voltage(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                uint16_t*         voltage_mv)
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

rx_err_t rx_bq4050_read_current(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                int16_t*          current_ma)
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

rx_err_t rx_bq4050_read_soc(rx_bus_manager_t* manager,
                            const char*       bus_name,
                            uint8_t*          soc)
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

rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status)
{
  (void)manager;
  (void)bus_name;

  s_status_count++;

  if (status == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    (void)memcpy(status, &s_status, sizeof(*status));
  }

  return s_read_return;
}
