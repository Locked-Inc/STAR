/**
 * @file mock_time.c
 * @brief Mock Time Implementation for Testing
 *
 * Mock implementation of rx_time_interface_t for testing.
 * Allows tests to control time progression without real delays.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "mock_time.h"

#include <string.h>

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

static mock_time_t g_mock_time;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

static mock_time_t* internal_get_mock(mock_time_t* mock)
{
  return (mock != NULL) ? mock : &g_mock_time;
}

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

static void impl_sleep_ms(void* ctx, uint32_t ms)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == NULL) {
    m = &g_mock_time;
  }

  m->sleep_call_count++;
  m->total_sleep_ms += ms;

  if (m->auto_advance) {
    m->current_time_ms += ms;
  }
}

static uint32_t impl_get_ms(void* ctx)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == NULL) {
    m = &g_mock_time;
  }

  return m->current_time_ms;
}

static bool impl_is_elapsed(void* ctx, uint32_t start_ms, uint32_t timeout_ms)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == NULL) {
    m = &g_mock_time;
  }

  /* Handle wrap-around correctly */
  uint32_t elapsed = m->current_time_ms - start_ms;

  return elapsed >= timeout_ms;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

rx_err_t mock_time_init(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_time_t));
  m->initialized  = true;
  m->auto_advance = false; /* Default: no auto-advance */

  return RX_OK;
}

rx_err_t mock_time_deinit(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_time_t));

  return RX_OK;
}

rx_err_t mock_time_get_interface(rx_time_interface_t* iface, mock_time_t* mock)
{
  if (iface == NULL) {
    return RX_ERR_NULL_POINTER;
  }

  mock_time_t* m = internal_get_mock(mock);

  iface->ctx        = m;
  iface->sleep_ms   = impl_sleep_ms;
  iface->get_ms     = impl_get_ms;
  iface->is_elapsed = impl_is_elapsed;

  return RX_OK;
}

/* =============================================================================
 * Time Control Functions
 * =============================================================================
 */

void mock_time_advance(mock_time_t* mock, uint32_t ms)
{
  mock_time_t* m = internal_get_mock(mock);

  m->current_time_ms += ms;
}

void mock_time_set(mock_time_t* mock, uint32_t ms)
{
  mock_time_t* m = internal_get_mock(mock);

  m->current_time_ms = ms;
}

void mock_time_set_auto_advance(mock_time_t* mock, bool enable)
{
  mock_time_t* m = internal_get_mock(mock);

  m->auto_advance = enable;
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

uint32_t mock_time_get_sleep_count(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  return m->sleep_call_count;
}

uint32_t mock_time_get_total_sleep(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  return m->total_sleep_ms;
}

void mock_time_reset_counters(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  m->sleep_call_count = 0;
  m->total_sleep_ms   = 0;
}
