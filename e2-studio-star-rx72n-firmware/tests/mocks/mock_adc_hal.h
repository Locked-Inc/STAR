/* tests/mocks/mock_adc_hal.h */

/**
 * @file mock_adc_hal.h
 * @brief Mock ADC HAL for unit testing analog input code without hardware
 *
 * @details
 * Provides test double for ADC (Analog-to-Digital Converter) HAL to enable
 * unit testing of analog sensor modules without RX72N hardware. Supports
 * multi-channel ADC value simulation, conversion timing, and error injection.
 *
 * Enables testing of:
 * - ADC initialization and configuration
 * - Single-channel analog readings (0-4095, 12-bit)
 * - Multi-channel scanning
 * - Conversion timeout handling
 * - Sensor calibration algorithms
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_adc {
 *   rankdir=LR;
 *   test [label="Sensor Tests"];
 *   mock [label="Mock ADC\n(simulated values)"];
 *   real [label="Real ADC\nHardware", style=dashed];
 *   test -> mock;
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature           | Supported | Description |
 * |-------------------|-----------|-------------|
 * | Multi-channel     | Yes       | 21 ADC channels (AN000-AN020) |
 * | Value simulation  | Yes       | Set expected ADC values |
 * | Error injection   | Yes       | Timeout, conversion errors |
 * | Resolution        | Yes       | 12-bit (0-4095) |
 *
 * @par Mock vs Real:
 * | Aspect | Mock | Real |
 * |--------|------|------|
 * | Hardware | Simulated values | Actual analog voltage |
 * | Timing | Instant | Conversion time (~µs) |
 *
 * @par Usage: tests/test_adc_hal.c
 *
 * @see adc_hal.h Real ADC HAL
 * @see hardware.h Hardware definitions
 *
 * @par NASA Power of 10: ✓ Static allocation, bounded loops
 * @par SOLID: D - Dependency Inversion
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock ADC constants */
typedef enum : uint8_t {
  k_mock_adc_max_units         = 2,  /**< ADC units (0, 1) */
  k_mock_adc_max_channels      = 8,  /**< Channels per unit (0-7) */
  k_mock_adc_call_history_size = 64, /**< Call history buffer size */
} mock_adc_constants_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief ADC HAL function call types */
typedef enum : uint8_t {
  k_mock_adc_call_init,
  k_mock_adc_call_read,
  k_mock_adc_call_read_voltage_mv,
} mock_adc_call_type_t;

/** @brief ADC HAL function call record */
typedef struct {
  mock_adc_call_type_t type;    /**< Call type */
  uint8_t              unit;    /**< ADC unit */
  uint8_t              channel; /**< ADC channel */
  uint8_t              bits;    /**< Resolution (for init) */
} mock_adc_call_t;

/** @brief Per-unit ADC state */
typedef struct {
  bool     initialized;                              /**< Unit initialized */
  uint8_t  resolution;                               /**< Configured resolution */
  uint16_t values[k_mock_adc_max_channels];          /**< Simulated ADC values */
  bool     channel_enabled[k_mock_adc_max_channels]; /**< Channel enable state */
} mock_adc_unit_state_t;

/** @brief Global mock ADC state */
typedef struct {
  mock_adc_unit_state_t units[k_mock_adc_max_units];                /**< Per-unit state */
  mock_adc_call_t       call_history[k_mock_adc_call_history_size]; /**< Call history */
  uint16_t              call_count;                                 /**< Number of calls recorded */
  rx_err_t              next_error;       /**< Error to return on next call */
  bool                  error_set;        /**< Whether error injection is active */
  bool                  simulate_timeout; /**< Simulate ADC timeout */
} mock_adc_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock ADC state instance */
extern mock_adc_state_t g_mock_adc;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock ADC state
 *
 * Resets all unit states and clears call history.
 */
void mock_adc_init(void);

/**
 * @brief Reset mock ADC state (alias for init)
 */
void mock_adc_reset(void);

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

/**
 * @brief Set simulated ADC value for a channel
 *
 * @param[in] unit ADC unit (0-1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] value Simulated ADC value
 */
void mock_adc_set_value(uint8_t unit, uint8_t channel, uint16_t value);

/**
 * @brief Enable ADC timeout simulation
 *
 * @param[in] simulate True to simulate timeout on reads
 */
void mock_adc_simulate_timeout(bool simulate);

/**
 * @brief Set error to return on next ADC HAL call
 *
 * @param[in] err Error code to return
 */
void mock_adc_set_next_error(rx_err_t err);

/**
 * @brief Clear any pending error injection
 */
void mock_adc_clear_error(void);

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

/**
 * @brief Check if ADC unit is initialized
 *
 * @param[in] unit ADC unit (0-1)
 *
 * @return True if initialized
 */
bool mock_adc_is_initialized(uint8_t unit);

/**
 * @brief Get configured resolution for unit
 *
 * @param[in] unit ADC unit (0-1)
 *
 * @return Resolution in bits, or 0 if not initialized
 */
uint8_t mock_adc_get_resolution(uint8_t unit);

/**
 * @brief Check if channel is enabled
 *
 * @param[in] unit ADC unit (0-1)
 * @param[in] channel ADC channel (0-7)
 *
 * @return True if channel is enabled
 */
bool mock_adc_is_channel_enabled(uint8_t unit, uint8_t channel);

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

/**
 * @brief Get call history entry
 *
 * @param[in] index Index in call history
 *
 * @return Pointer to call record, or NULL if index out of range
 */
const mock_adc_call_t* mock_adc_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @return Number of calls in history
 */
uint16_t mock_adc_get_call_count(void);

/**
 * @brief Clear call history
 */
void mock_adc_clear_history(void);

/* =============================================================================
 * ADC HAL Function Declarations (for test linking)
 * =============================================================================
 */

rx_err_t adc_init(adc_unit_t unit, adc_channel_t channel, adc_resolution_t bits);
rx_err_t adc_read(adc_unit_t unit, adc_channel_t channel, uint16_t* value);
rx_err_t adc_read_voltage_mv(adc_unit_t       unit,
                             adc_channel_t    channel,
                             adc_resolution_t bits,
                             uint32_t*        voltage_mv);

#ifdef __cplusplus
}
#endif
