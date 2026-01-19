/* tests/mocks/mock_adc_hal.c */

/**
 * @file mock_adc_hal.c
 * @brief Mock ADC HAL Function Implementation
 *
 * Provides mock implementations of ADC HAL functions for unit testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_adc_hal.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief ADC reference voltage in millivolts */
typedef enum : uint16_t {
  k_mock_adc_vref_mv = 3300, /**< 3.3V reference */
} mock_adc_voltage_t;

/** @brief Valid ADC resolution values in bits */
typedef enum : uint8_t {
  k_mock_adc_resolution_8bit  = 8,
  k_mock_adc_resolution_10bit = 10,
  k_mock_adc_resolution_12bit = 12,
} mock_adc_resolution_t;

/** @brief ADC bits parameter constants */
typedef enum : uint8_t {
  k_mock_adc_bits_unused = 0, /**< Bits parameter not applicable for this call type */
} mock_adc_bits_t;

/** @brief Bit manipulation constants for ADC calculations */
typedef enum : uint8_t {
  k_mock_adc_bit_shift_base   = 1, /**< Base value for 2^n calculation */
  k_mock_adc_max_value_offset = 1, /**< Offset to get max value from 2^n */
} mock_adc_bit_constants_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_adc_state_t g_mock_adc;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the history
 */
static void
internal_record_call(mock_adc_call_type_t type, uint8_t unit, uint8_t channel, uint8_t bits)
{
  if (g_mock_adc.call_count < k_mock_adc_call_history_size) {
    mock_adc_call_t* call = &g_mock_adc.call_history[g_mock_adc.call_count];
    call->type            = type;
    call->unit            = unit;
    call->channel         = channel;
    call->bits            = bits;
    g_mock_adc.call_count++;
  }
}

/**
 * @brief Check and consume error injection
 */
static rx_err_t internal_check_error(void)
{
  if (g_mock_adc.error_set) {
    rx_err_t err         = g_mock_adc.next_error;
    g_mock_adc.error_set = false;
    return err;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_adc_init(void)
{
  memset(&g_mock_adc, 0, sizeof(g_mock_adc));
}

void mock_adc_reset(void)
{
  mock_adc_init();
}

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

void mock_adc_set_value(uint8_t unit, uint8_t channel, uint16_t value)
{
  if (unit < k_mock_adc_max_units && channel < k_mock_adc_max_channels) {
    g_mock_adc.units[unit].values[channel] = value;
  }
}

void mock_adc_simulate_timeout(bool simulate)
{
  g_mock_adc.simulate_timeout = simulate;
}

void mock_adc_set_next_error(rx_err_t err)
{
  g_mock_adc.next_error = err;
  g_mock_adc.error_set  = true;
}

void mock_adc_clear_error(void)
{
  g_mock_adc.error_set = false;
}

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

bool mock_adc_is_initialized(uint8_t unit)
{
  if (unit < k_mock_adc_max_units) {
    return g_mock_adc.units[unit].initialized;
  }
  return false;
}

uint8_t mock_adc_get_resolution(uint8_t unit)
{
  if (unit < k_mock_adc_max_units) {
    return g_mock_adc.units[unit].resolution;
  }
  return 0;
}

bool mock_adc_is_channel_enabled(uint8_t unit, uint8_t channel)
{
  if (unit < k_mock_adc_max_units && channel < k_mock_adc_max_channels) {
    return g_mock_adc.units[unit].channel_enabled[channel];
  }
  return false;
}

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

const mock_adc_call_t* mock_adc_get_call(uint16_t index)
{
  if (index < g_mock_adc.call_count) {
    return &g_mock_adc.call_history[index];
  }
  return NULL;
}

uint16_t mock_adc_get_call_count(void)
{
  return g_mock_adc.call_count;
}

void mock_adc_clear_history(void)
{
  g_mock_adc.call_count = 0;
}

/* =============================================================================
 * Mock ADC HAL Functions
 * =============================================================================
 */

rx_err_t adc_init(uint8_t unit, uint8_t channel, uint8_t bits)
{
  internal_record_call(k_mock_adc_call_init, unit, channel, bits);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate unit */
  if (unit >= k_mock_adc_max_units) {
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel >= k_mock_adc_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Validate resolution */
  if (bits != k_mock_adc_resolution_8bit && bits != k_mock_adc_resolution_10bit &&
      bits != k_mock_adc_resolution_12bit) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize unit if not already initialized */
  if (!g_mock_adc.units[unit].initialized) {
    g_mock_adc.units[unit].initialized = true;
    g_mock_adc.units[unit].resolution  = bits;
  }

  /* Enable channel */
  g_mock_adc.units[unit].channel_enabled[channel] = true;

  return k_rx_ok;
}

rx_err_t adc_read(uint8_t unit, uint8_t channel, uint16_t* value)
{
  internal_record_call(k_mock_adc_call_read, unit, channel, k_mock_adc_bits_unused);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (value == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Validate unit */
  if (unit >= k_mock_adc_max_units) {
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel >= k_mock_adc_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_adc.units[unit].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check timeout simulation */
  if (g_mock_adc.simulate_timeout) {
    return k_rx_err_timeout;
  }

  /* Return simulated value */
  *value = g_mock_adc.units[unit].values[channel];

  return k_rx_ok;
}

rx_err_t adc_read_voltage_mv(uint8_t unit, uint8_t channel, uint8_t bits, uint32_t* voltage_mv)
{
  rx_err_t err;
  uint16_t raw_value;
  uint32_t max_value;

  internal_record_call(k_mock_adc_call_read_voltage_mv, unit, channel, bits);

  err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (voltage_mv == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Validate resolution */
  if (bits != k_mock_adc_resolution_8bit && bits != k_mock_adc_resolution_10bit &&
      bits != k_mock_adc_resolution_12bit) {
    return k_rx_err_invalid_arg;
  }

  /* Read raw ADC value */
  err = adc_read(unit, channel, &raw_value);
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate voltage */
  max_value   = ((uint32_t)k_mock_adc_bit_shift_base << bits) - k_mock_adc_max_value_offset;
  *voltage_mv = ((uint32_t)raw_value * k_mock_adc_vref_mv) / max_value;

  return k_rx_ok;
}
