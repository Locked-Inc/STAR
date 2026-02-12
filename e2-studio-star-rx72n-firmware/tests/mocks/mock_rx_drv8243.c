/* tests/mocks/mock_rx_drv8243.c */

/**
 * @file mock_rx_drv8243.c
 * @brief Mock DRV8243 Motor Driver Implementation
 *
 * @details
 * Implements mock rx_drv8243 functions for unit testing.
 * Tracks calls, stores parameters, and allows configurable return values.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_rx_drv8243.h"

#include <string.h>

/* =============================================================================
 * Static Variables - Return Values
 * =============================================================================
 */

/** @brief Return value for rx_drv8243_init() */
static rx_err_t s_init_return = k_rx_ok;

/** @brief Return value for rx_drv8243_deinit() */
static rx_err_t s_deinit_return = k_rx_ok;

/** @brief Return value for rx_drv8243_set_speed() */
static rx_err_t s_set_speed_return = k_rx_ok;

/** @brief Return value for rx_drv8243_stop() */
static rx_err_t s_stop_return = k_rx_ok;

/** @brief Return value for rx_drv8243_read_current() */
static rx_err_t s_read_current_return = k_rx_ok;

/** @brief Return value for rx_drv8243_get_fault_status() */
static rx_err_t s_fault_status_return = k_rx_ok;

/** @brief Return value for rx_drv8243_get_speed() */
static rx_err_t s_get_speed_return = k_rx_ok;

/** @brief Return value for rx_drv8243_set_current_limit() */
static rx_err_t s_set_current_limit_return = k_rx_ok;

/* =============================================================================
 * Static Variables - Call Counts
 * =============================================================================
 */

/** @brief Call count for rx_drv8243_init() */
static uint32_t s_init_count = 0;

/** @brief Call count for rx_drv8243_deinit() */
static uint32_t s_deinit_count = 0;

/** @brief Call count for rx_drv8243_set_speed() */
static uint32_t s_set_speed_count = 0;

/** @brief Call count for rx_drv8243_stop() */
static uint32_t s_stop_count = 0;

/** @brief Call count for rx_drv8243_read_current() */
static uint32_t s_read_current_count = 0;

/** @brief Call count for rx_drv8243_get_fault_status() */
static uint32_t s_fault_status_count = 0;

/** @brief Call count for rx_drv8243_get_speed() */
static uint32_t s_get_speed_count = 0;

/** @brief Call count for rx_drv8243_set_current_limit() */
static uint32_t s_set_current_limit_count = 0;

/* =============================================================================
 * Static Variables - Per-Motor State
 * =============================================================================
 */

/** @brief Simulated fault status per motor */
static bool s_fault_active[k_mock_drv8243_max_motors] = {false};

/** @brief Simulated current reading per motor (mA) */
static float s_current_ma[k_mock_drv8243_max_motors] = {0.0f};

/** @brief Last speed set per motor */
static float s_last_speed[k_mock_drv8243_max_motors] = {0.0f};

/** @brief Track which motor index we're operating on */
static uint8_t s_motor_index = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_drv8243_reset(void)
{
  /* Reset return values to defaults */
  s_init_return              = k_rx_ok;
  s_deinit_return            = k_rx_ok;
  s_set_speed_return         = k_rx_ok;
  s_stop_return              = k_rx_ok;
  s_read_current_return      = k_rx_ok;
  s_fault_status_return      = k_rx_ok;
  s_get_speed_return         = k_rx_ok;
  s_set_current_limit_return = k_rx_ok;

  /* Reset call counts */
  s_init_count              = 0;
  s_deinit_count            = 0;
  s_set_speed_count         = 0;
  s_stop_count              = 0;
  s_read_current_count      = 0;
  s_fault_status_count      = 0;
  s_get_speed_count         = 0;
  s_set_current_limit_count = 0;

  /* Reset per-motor state */
  for (uint8_t i = 0; i < k_mock_drv8243_max_motors; i++) {
    s_fault_active[i] = false;
    s_current_ma[i]   = 0.0f;
    s_last_speed[i]   = 0.0f;
  }

  s_motor_index = 0;
}

void mock_drv8243_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_drv8243_set_deinit_return(rx_err_t err)
{
  s_deinit_return = err;
}

void mock_drv8243_set_speed_return(rx_err_t err)
{
  s_set_speed_return = err;
}

void mock_drv8243_set_read_current_return(rx_err_t err)
{
  s_read_current_return = err;
}

void mock_drv8243_set_fault_status_return(rx_err_t err)
{
  s_fault_status_return = err;
}

void mock_drv8243_set_fault_status(uint8_t motor_idx, bool fault)
{
  if (motor_idx < k_mock_drv8243_max_motors) {
    s_fault_active[motor_idx] = fault;
  }
}

void mock_drv8243_set_current_reading(uint8_t motor_idx, float current_ma)
{
  if (motor_idx < k_mock_drv8243_max_motors) {
    s_current_ma[motor_idx] = current_ma;
  }
}

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_drv8243_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_drv8243_get_deinit_count(void)
{
  return s_deinit_count;
}

uint32_t mock_drv8243_get_set_speed_count(void)
{
  return s_set_speed_count;
}

uint32_t mock_drv8243_get_read_current_count(void)
{
  return s_read_current_count;
}

uint32_t mock_drv8243_get_fault_status_count(void)
{
  return s_fault_status_count;
}

float mock_drv8243_get_last_speed(uint8_t motor_idx)
{
  if (motor_idx < k_mock_drv8243_max_motors) {
    return s_last_speed[motor_idx];
  }
  return 0.0f;
}

bool mock_drv8243_was_initialized(void)
{
  return s_init_count > 0;
}

/* =============================================================================
 * Mock Implementation
 * =============================================================================
 */

rx_err_t rx_drv8243_init(rx_drv8243_handle_t* handle, const rx_drv8243_config_t* config)
{
  uint8_t idx;

  s_init_count++;

  if (handle == nullptr || config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_init_return == k_rx_ok) {
    /* Assign motor index based on init order */
    idx = (uint8_t)((s_init_count - 1) % k_mock_drv8243_max_motors);
    (void)idx; /* Not directly used, but documents motor index tracking */

    handle->current_speed    = 0.0f;
    handle->current_ma       = 0.0f;
    handle->fault_active     = false;
    handle->initialized      = true;
    handle->spi_enabled      = config->use_spi_variant;
    handle->current_limit_ma = config->current_limit_ma;

    /* Store index in handle for later queries */
    /* Using a simple approach: handle pointer difference to track which motor */
  }

  return s_init_return;
}

rx_err_t rx_drv8243_deinit(rx_drv8243_handle_t* handle)
{
  s_deinit_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_deinit_return == k_rx_ok) {
    (void)memset(handle, 0, sizeof(*handle));
    handle->initialized = false;
  }

  return s_deinit_return;
}

rx_err_t rx_drv8243_set_speed(rx_drv8243_handle_t* handle, float speed)
{
  s_set_speed_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_set_speed_return == k_rx_ok) {
    handle->current_speed = speed;

    /* Track last speed (use set_speed_count to track which motor in iteration) */
    uint8_t idx       = (s_set_speed_count - 1) % k_mock_drv8243_max_motors;
    s_last_speed[idx] = speed;
  }

  return s_set_speed_return;
}

rx_err_t rx_drv8243_stop(rx_drv8243_handle_t* handle, bool brake)
{
  (void)brake;

  s_stop_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_stop_return == k_rx_ok) {
    handle->current_speed = 0.0f;
  }

  return s_stop_return;
}

rx_err_t rx_drv8243_read_current(const rx_drv8243_handle_t* handle, float* out_current)
{
  uint8_t idx;

  s_read_current_count++;

  if (handle == nullptr || out_current == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_current_return == k_rx_ok) {
    /* Use read_current_count to determine motor index */
    idx          = (s_read_current_count - 1) % k_mock_drv8243_max_motors;
    *out_current = s_current_ma[idx];
  }

  return s_read_current_return;
}

rx_err_t rx_drv8243_get_fault_status(const rx_drv8243_handle_t* handle, bool* out_fault)
{
  uint8_t idx;

  s_fault_status_count++;

  if (handle == nullptr || out_fault == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_fault_status_return == k_rx_ok) {
    /* Use fault_status_count to determine motor index */
    idx        = (s_fault_status_count - 1) % k_mock_drv8243_max_motors;
    *out_fault = s_fault_active[idx];
  }

  return s_fault_status_return;
}

rx_err_t rx_drv8243_get_speed(const rx_drv8243_handle_t* handle, float* out_speed)
{
  s_get_speed_count++;

  if (handle == nullptr || out_speed == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_get_speed_return == k_rx_ok) {
    *out_speed = handle->current_speed;
  }

  return s_get_speed_return;
}

rx_err_t rx_drv8243_set_current_limit(rx_drv8243_handle_t* handle, uint16_t limit_ma)
{
  s_set_current_limit_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_set_current_limit_return == k_rx_ok) {
    handle->current_limit_ma = limit_ma;
  }

  return s_set_current_limit_return;
}
