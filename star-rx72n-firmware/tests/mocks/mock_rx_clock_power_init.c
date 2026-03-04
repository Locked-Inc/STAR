/* tests/mocks/mock_rx_clock_power_init.c */

/**
 * @file mock_rx_clock_power_init.c
 * @brief Mock Clock/Power Initialization Implementation
 *
 * @details
 * Implements mock rx_clock_power_init() for unit testing.
 * Tracks calls and allows configurable return values.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_rx_clock_power_init.h"

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Configured return value for rx_clock_power_init() */
static rx_err_t s_return_value = k_rx_ok;

/** @brief Number of times rx_clock_power_init() was called */
static uint32_t s_call_count = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_clock_power_init_reset(void)
{
  s_return_value = k_rx_ok;
  s_call_count   = 0;
}

void mock_clock_power_init_set_return(rx_err_t err)
{
  s_return_value = err;
}

uint32_t mock_clock_power_init_get_call_count(void)
{
  return s_call_count;
}

bool mock_clock_power_init_was_called(void)
{
  return s_call_count > 0;
}

/* =============================================================================
 * Mock Implementation
 * =============================================================================
 */

/**
 * @brief Mock rx_clock_power_init() - replaces real implementation for testing
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_clock_power_init(void)
{
  s_call_count++;
  return s_return_value;
}
