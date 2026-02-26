/* tests/mocks/mock_adc_hal.c */

/**
 * @file mock_adc_hal.c
 * @brief Mock ADC HAL Function Implementation
 *
 * @details
 * Provides mock implementations of the ADC HAL public API (adc_init, adc_read,
 * adc_read_voltage_mv) for use in unit tests that run on the host without
 * RX72N hardware.  The mock records all calls in a circular call-history buffer,
 * supports single-shot error injection, and allows per-channel raw-value
 * pre-programming via g_mock_adc.
 *
 * @par SOLID Principles:
 * - **S:** Single responsibility -- mock state management only, no production logic
 * - **L:** Liskov Substitution -- drop-in replacement for adc_hal.c functions
 * - **D:** Dependency Inversion -- tests depend on mock interface, not hardware
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: [OK] No dynamic allocation; all state in static g_mock_adc
 * - Rule 5: [OK] All public functions validate inputs (null pointer, range checks)
 * - Rule 6: [OK] Minimize variable scope; variables declared near first use
 *           (see g_mock_adc, local variables in mock_adc_read, mock_adc_init)
 * - Rule 7: [OK] All internal function returns checked by callers
 *
 * @author STAR Project
 * @version 1.0.0
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
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
  k_mock_adc_resolution_8bit  = 8,  /**< 8-bit resolution: 256 discrete levels (0-255) */
  k_mock_adc_resolution_10bit = 10, /**< 10-bit resolution: 1024 discrete levels (0-1023) */
  k_mock_adc_resolution_12bit = 12, /**< 12-bit resolution: 4096 discrete levels (0-4095) */
} mock_adc_resolution_t;

/** @brief ADC bits parameter constants */
typedef enum : uint8_t {
  k_mock_adc_bits_unused = 0, /**< Bits parameter not applicable for this call type */
} mock_adc_bits_t;

/** @brief Bit manipulation constants for ADC calculations */
typedef enum : uint8_t {
  k_mock_adc_bit_shift_base    = 1, /**< Base value for 2^n calculation */
  k_mock_adc_max_value_offset  = 1, /**< Offset to get max value from 2^n */
  k_mock_adc_raw_value_default = 0, /**< Default raw value returned when no error injected */
} mock_adc_bit_constants_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/**
 * @var g_mock_adc
 * @brief Global mock ADC state shared across all mock HAL functions
 *
 * @details
 * Contains per-unit initialization flags, per-channel pre-programmed raw values,
 * call history ring buffer, error injection slot, and timeout simulation flag.
 * Callers must invoke mock_adc_init() to zero this structure before each test.
 *
 * @note Access from test code only -- never modify directly from production code
 * @warning Direct modification outside of mock helpers bypasses call recording
 *          and may cause tests to observe inconsistent state
 * @since Version 1.0.0
 */
mock_adc_state_t g_mock_adc;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the mock call-history ring buffer
 *
 * @details
 * Appends a new call record to g_mock_adc.call_history if capacity remains.
 * When the buffer is full (call_count >= k_mock_adc_call_history_size),
 * the call is silently dropped to prevent buffer overrun.
 *
 * @param[in] type    Call type identifier (k_mock_adc_call_init, _read, _read_voltage_mv)
 * @param[in] unit    ADC unit number passed by the caller
 * @param[in] channel ADC channel number passed by the caller
 * @param[in] bits    Resolution in bits passed by the caller (0 when not applicable)
 *
 * @pre g_mock_adc has been initialized via mock_adc_init()
 * @pre type is a valid mock_adc_call_type_t enumerator
 *
 * @post g_mock_adc.call_count incremented by 1 (if buffer not full)
 * @post New call appended to g_mock_adc.call_history[call_count - 1]
 *
 * @note Not thread-safe; unit tests are single-threaded by convention
 * @since Version 1.0.0
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
 * @brief Check and consume a pending injected error
 *
 * @details
 * If g_mock_adc.error_set is true, clears the flag and returns the injected
 * error code stored in g_mock_adc.next_error.  If no error is pending,
 * returns k_rx_ok immediately.  Each injected error is consumed exactly once
 * (single-shot semantics).
 *
 * @return rx_err_t Consumed error code or k_rx_ok if none pending
 * @retval k_rx_ok No error was injected; caller should proceed normally
 * @retval (any rx_err_t) The injected error code; caller should propagate it
 *
 * @pre g_mock_adc has been initialized via mock_adc_init()
 * @pre g_mock_adc.next_error is a valid rx_err_t if error_set is true
 *
 * @post g_mock_adc.error_set cleared to false (error consumed)
 * @post g_mock_adc.next_error is undefined after consumption
 *
 * @note Not thread-safe; unit tests are single-threaded by convention
 * @since Version 1.0.0
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
  return nullptr;
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

/**
 * @brief Mock implementation of adc_init() -- initialize ADC unit/channel
 *
 * @details
 * Records the call in the call-history buffer, checks for injected errors,
 * validates unit, channel, and resolution parameters, and marks the unit
 * as initialized in g_mock_adc.  Resolution is stored for later validation
 * by adc_read_voltage_mv().
 *
 * @param[in] unit    ADC unit number (0-based; must be < k_mock_adc_max_units)
 * @param[in] channel ADC channel number (0-based; must be < k_mock_adc_max_channels)
 * @param[in] bits    ADC resolution: 8, 10, or 12 (must match mock_adc_resolution_t)
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Initialization successful
 * @retval (injected) Any error code pre-set via g_mock_adc.next_error
 * @retval k_rx_err_invalid_arg unit >= k_mock_adc_max_units, channel >= k_mock_adc_max_channels,
 *                               or bits not in {8, 10, 12}
 *
 * @pre g_mock_adc has been zeroed via mock_adc_init() before the test
 * @pre unit is a valid adc_unit_t value
 *
 * @post Call recorded in g_mock_adc.call_history
 * @post g_mock_adc.units[unit].initialized == true on k_rx_ok
 * @post g_mock_adc.units[unit].channel_enabled[channel] == true on k_rx_ok
 *
 * @note Not thread-safe; unit tests are single-threaded by convention
 *
 * @code{.c}
 * mock_adc_init();
 * rx_err_t err = adc_init(k_adc_unit_0, k_adc_channel_2, k_adc_resolution_12bit);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_TRUE(g_mock_adc.units[0].initialized);
 * @endcode
 *
 * @see adc_read() Read after successful initialization
 * @see mock_adc_init() Reset mock state between tests
 * @since Version 1.0.0
 */
rx_err_t adc_init(adc_unit_t unit, adc_channel_t channel, adc_resolution_t bits)
{
  internal_record_call(k_mock_adc_call_init, unit, channel, bits);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate unit */
  if ((uint8_t)unit >= k_mock_adc_max_units) {
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if ((uint8_t)channel >= k_mock_adc_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Validate resolution */
  if ((uint8_t)bits != (uint8_t)k_mock_adc_resolution_8bit &&
      (uint8_t)bits != (uint8_t)k_mock_adc_resolution_10bit &&
      (uint8_t)bits != (uint8_t)k_mock_adc_resolution_12bit) {
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

/**
 * @brief Mock implementation of adc_read() -- return pre-programmed raw ADC value
 *
 * @details
 * Records the call, checks for injected errors, validates parameters and
 * initialization state, and copies the pre-programmed raw value from
 * g_mock_adc.units[unit].values[channel] into *value.
 * If g_mock_adc.simulate_timeout is true, returns k_rx_err_timeout immediately.
 *
 * @param[in]  unit    ADC unit number (0-based; must be < k_mock_adc_max_units)
 * @param[in]  channel ADC channel number (0-based; must be < k_mock_adc_max_channels)
 * @param[out] value   Pointer to store raw ADC reading (must not be nullptr)
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok Raw value written to *value
 * @retval (injected) Any error code pre-set via g_mock_adc.next_error
 * @retval k_rx_err_null_ptr value is nullptr
 * @retval k_rx_err_invalid_arg unit >= k_mock_adc_max_units or
 *                               channel >= k_mock_adc_max_channels
 * @retval k_rx_err_invalid_state unit not initialized via adc_init()
 * @retval k_rx_err_timeout g_mock_adc.simulate_timeout is true
 *
 * @pre adc_init() called successfully for this unit/channel
 * @pre value points to valid uint16_t storage
 *
 * @post *value contains g_mock_adc.units[unit].values[channel] on k_rx_ok
 * @post Call recorded in g_mock_adc.call_history
 *
 * @note Not thread-safe; unit tests are single-threaded by convention
 *
 * @code{.c}
 * mock_adc_init();
 * (void)adc_init(k_adc_unit_0, k_adc_channel_2, k_adc_resolution_12bit);
 * g_mock_adc.units[0].values[2] = 2048U;
 * uint16_t raw = 0U;
 * rx_err_t err = adc_read(k_adc_unit_0, k_adc_channel_2, &raw);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL(2048U, raw);
 * @endcode
 *
 * @see adc_init() Must be called before adc_read()
 * @see adc_read_voltage_mv() Convenience function that scales to millivolts
 * @since Version 1.0.0
 */
rx_err_t adc_read(adc_unit_t unit, adc_channel_t channel, uint16_t* value)
{
  internal_record_call(k_mock_adc_call_read, unit, channel, k_mock_adc_bits_unused);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (value == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate unit */
  if ((uint8_t)unit >= k_mock_adc_max_units) {
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if ((uint8_t)channel >= k_mock_adc_max_channels) {
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

/**
 * @brief Mock implementation of adc_read_voltage_mv() -- return pre-programmed value in mV
 *
 * @details
 * Records the call, checks for injected errors, validates parameters,
 * then calls adc_read() to obtain the raw value and scales it to millivolts:
 *
 *   voltage_mv = (raw * k_mock_adc_vref_mv) / ((2^bits) - 1)
 *
 * Resolution must match the resolution stored during adc_init() for this unit.
 *
 * @param[in]  unit       ADC unit number (0-based; must be < k_mock_adc_max_units)
 * @param[in]  channel    ADC channel number (0-based; must be < k_mock_adc_max_channels)
 * @param[in]  bits       ADC resolution: 8, 10, or 12 (must match unit's configured resolution)
 * @param[out] voltage_mv Pointer to store computed millivolt value (must not be nullptr)
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok Millivolt value written to *voltage_mv
 * @retval (injected) Any error code pre-set via g_mock_adc.next_error
 * @retval k_rx_err_null_ptr voltage_mv is nullptr
 * @retval k_rx_err_invalid_arg bits not in {8, 10, 12}, or bits mismatches unit resolution
 * @retval k_rx_err_invalid_state unit not initialized via adc_init()
 * @retval k_rx_err_timeout g_mock_adc.simulate_timeout is true (via adc_read)
 *
 * @pre adc_init() called successfully with the same bits value for this unit
 * @pre voltage_mv points to valid uint32_t storage
 *
 * @post *voltage_mv contains scaled millivolt reading on k_rx_ok
 * @post g_mock_adc.call_history gains two entries:
 *       k_mock_adc_call_read_voltage_mv (outer) and k_mock_adc_call_read (inner via adc_read())
 *
 * @note Not thread-safe; unit tests are single-threaded by convention
 *
 * @code{.c}
 * mock_adc_init();
 * (void)adc_init(k_adc_unit_0, k_adc_channel_0, k_adc_resolution_12bit);
 * g_mock_adc.units[0].values[0] = 2048U;  // ~1.65 V at 12-bit, 3.3 V ref
 * uint32_t mv = 0U;
 * rx_err_t err = adc_read_voltage_mv(k_adc_unit_0, k_adc_channel_0,
 *                                    k_adc_resolution_12bit, &mv);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_UINT32_WITHIN(2U, 1650U, mv);
 * @endcode
 *
 * @see adc_read() Raw-count read used internally
 * @see adc_init() Must be called before this function
 * @since Version 1.0.0
 */
rx_err_t adc_read_voltage_mv(adc_unit_t       unit,
                             adc_channel_t    channel,
                             adc_resolution_t bits,
                             uint32_t*        voltage_mv)
{
  internal_record_call(k_mock_adc_call_read_voltage_mv, unit, channel, bits);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (voltage_mv == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate resolution */
  if ((uint8_t)bits != (uint8_t)k_mock_adc_resolution_8bit &&
      (uint8_t)bits != (uint8_t)k_mock_adc_resolution_10bit &&
      (uint8_t)bits != (uint8_t)k_mock_adc_resolution_12bit) {
    return k_rx_err_invalid_arg;
  }

  /* Validate resolution matches unit's configured resolution (matches production behavior) */
  if ((uint8_t)unit < k_mock_adc_max_units && g_mock_adc.units[unit].initialized &&
      g_mock_adc.units[unit].resolution != (uint8_t)bits) {
    return k_rx_err_invalid_arg;
  }

  /* Read raw ADC value */
  uint16_t raw_value = k_mock_adc_raw_value_default;
  err                = adc_read(unit, channel, &raw_value);
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate voltage */
  const uint32_t max_value =
    ((uint32_t)k_mock_adc_bit_shift_base << bits) - k_mock_adc_max_value_offset;
  *voltage_mv = ((uint32_t)raw_value * k_mock_adc_vref_mv) / max_value;

  return k_rx_ok;
}
