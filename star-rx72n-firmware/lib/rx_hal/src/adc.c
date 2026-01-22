/* lib/rx_hal/src/adc.c */

/**
 * @file adc.c
 * @brief ADC Driver for RX72N S12ADFa
 *
 * Provides 12-bit A/D conversion on RX72N using S12ADFa peripheral.
 * Supports ADC unit 0 and 1 with multiple channels.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief ADC hardware limits */
typedef enum : uint8_t {
  k_adc_max_units    = 2,   /**< S12AD0, S12AD1 */
  k_adc_max_channels = 8,   /**< Channels 0-7 per unit */
  k_adc_timeout_ms   = 100, /**< ADC conversion timeout (ms) */
} adc_constants_t;

/** @brief ADC unit and bit constants */
typedef enum : uint8_t {
  k_adc_unit_0  = 0U, /**< ADC unit 0 */
  k_adc_unit_1  = 1U, /**< ADC unit 1 */
  k_adc_bit_one = 1U, /**< Single-bit value */
} adc_unit_constants_t;

/** @brief ADC resolution options */
typedef enum : uint8_t {
  k_adc_resolution_8bit  = 8,  /**< 8-bit ADC resolution */
  k_adc_resolution_10bit = 10, /**< 10-bit ADC resolution */
  k_adc_resolution_12bit = 12, /**< 12-bit ADC resolution (default) */
} adc_resolution_bits_t;

/** @brief ADC module stop bit positions in MSTPCRA */
typedef enum : uint8_t {
  k_adc_mstpra_s12ad1 = 16, /**< S12AD1 module stop bit */
  k_adc_mstpra_s12ad0 = 17, /**< S12AD0 module stop bit */
} adc_module_stop_bits_t;

/** @brief ADC Control Extended Register (ADCER) bit masks and values */
typedef enum : uint16_t {
  k_adc_adcer_adprc_mask  = 3U, /**< ADPRC bit mask (bits 1:0) */
  k_adc_adcer_adprc_shift = 0U, /**< ADPRC bit shift position */
  k_adc_adcer_adprc_12bit = 0U, /**< 12-bit resolution */
  k_adc_adcer_adprc_10bit = 1U, /**< 10-bit resolution */
  k_adc_adcer_adprc_8bit  = 2U, /**< 8-bit resolution */
} adc_adcer_bits_t;

/** @brief ADC Control/Status Register (ADCSR) bit masks */
typedef enum : uint16_t {
  k_adc_adcsr_idle = 0U,    /**< ADC idle state (ADST bit cleared, conversion complete) */
  k_adc_adcsr_adst = 4096U, /**< A/D Conversion Start (bit 12) */
} adc_adcsr_bits_t;

/** @brief ADC channel register boundaries */
typedef enum : uint8_t {
  k_adc_channel_adansa0_max  = 15, /**< Maximum channel for ADANSA0 (channels 0-15) */
  k_adc_channel_adansa1_base = 16, /**< Base channel for ADANSA1 (channels 16+) */
} adc_channel_boundaries_t;

/** @brief ADC channel indices */
typedef enum : uint8_t {
  k_adc_channel_0 = 0U, /**< ADC channel 0 */
  k_adc_channel_1 = 1U, /**< ADC channel 1 */
  k_adc_channel_2 = 2U, /**< ADC channel 2 */
  k_adc_channel_3 = 3U, /**< ADC channel 3 */
  k_adc_channel_4 = 4U, /**< ADC channel 4 */
  k_adc_channel_5 = 5U, /**< ADC channel 5 */
  k_adc_channel_6 = 6U, /**< ADC channel 6 */
  k_adc_channel_7 = 7U, /**< ADC channel 7 */
} adc_channel_index_t;

/** @brief ADC timeout and voltage constants */
typedef enum : uint16_t {
  k_adc_timeout_multiplier   = 1000, /**< Timeout loop multiplier */
  k_adc_timeout_expired      = 0,    /**< Timeout counter expired */
  k_adc_reference_voltage_mv = 3300, /**< ADC reference voltage (3.3V) */
} adc_misc_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* s_tag = "ADC";

/* Track initialized ADC units */
static bool s_adc_unit_initialized[k_adc_max_units] = {false, false};

/* Track resolution for each ADC unit (initialized to 12-bit default) */
static adc_resolution_t s_adc_unit_resolution[k_adc_max_units] = {
    k_adc_resolution_12bit,
    k_adc_resolution_12bit
};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get ADC base address from unit number
 *
 * @param[in] unit ADC unit (0 or 1)
 *
 * @return Pointer to ADC register base, or NULL if invalid unit
 */
static volatile rx_s12ad_regs_t* internal_get_adc_base(const uint8_t unit)
{
  switch (unit) {
    case k_adc_unit_0: {
      return s12ad0();
    }
    case k_adc_unit_1: {
      return s12ad1();
    }
    default: {
      return NULL;
    }
  }
}

/**
 * @brief Configure ADC unit control registers and enable module clock
 *
 * @param[in] unit ADC unit number (k_adc_unit_0 or k_adc_unit_1)
 * @param[in] adc Pointer to ADC register base
 * @param[in] bits Resolution setting (k_adc_resolution_8bit/10bit/12bit)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if adc is NULL
 * @return k_rx_err_invalid_arg if unit or bits are invalid
 */
static rx_err_t internal_configure_adc_unit(const uint8_t             unit,
                                            volatile rx_s12ad_regs_t* adc,
                                            const uint8_t             bits)
{
  uint16_t adcer;

  if (adc == NULL) {
    return k_rx_err_null_ptr;
  }
  if (unit < k_adc_unit_0 || unit > k_adc_unit_1) {
    return k_rx_err_invalid_arg;
  }
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    return k_rx_err_invalid_arg;
  }

  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;

  if (unit == k_adc_unit_0) {
    system_regs()->mstpcra &= ~((uint32_t)k_adc_bit_one << k_adc_mstpra_s12ad0);
  } else {
    system_regs()->mstpcra &= ~((uint32_t)k_adc_bit_one << k_adc_mstpra_s12ad1);
  }

  system_regs()->prcr = k_rx_prcr_lock;

  adcer = adc->adcer;
  adcer &= ~(k_adc_adcer_adprc_mask << k_adc_adcer_adprc_shift);
  if (bits == k_adc_resolution_8bit) {
    adcer |= k_adc_adcer_adprc_8bit;
  } else if (bits == k_adc_resolution_10bit) {
    adcer |= k_adc_adcer_adprc_10bit;
  } else {
    adcer |= k_adc_adcer_adprc_12bit;
  }
  adc->adcer = adcer;

  s_adc_unit_initialized[unit] = true;
  rx_log_debug(s_tag, "ADC unit initialized");

  return k_rx_ok;
}

/**
 * @brief Validate ADC unit and channel
 *
 * @param[in] unit ADC unit number
 * @param[in] channel ADC channel number
 *
 * @return k_rx_ok if valid, error code otherwise
 */
static rx_err_t internal_validate_unit_channel(const uint8_t unit, uint8_t channel)
{
  /* Validate unit */
  if (unit < k_adc_unit_0 || unit > k_adc_unit_1) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel < k_adc_channel_0 || channel > k_adc_channel_7) {
    rx_log_error(s_tag, "Invalid ADC channel");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Read raw ADC value from a specific channel data register
 *
 * Helper function to extract channel-specific register access from adc_read().
 * This supports NASA Power of 10 Rule 4 by keeping functions under 60 lines.
 *
 * @param[in] adc Pointer to ADC register base (must not be NULL)
 * @param[in] channel ADC channel to read from (0-7)
 * @param[out] value Pointer to store the raw ADC value (must not be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is out of range
 *
 * @pre adc and value must be valid non-NULL pointers (caller responsibility)
 * @pre ADC conversion must be complete before calling this function
 */
static rx_err_t internal_read_channel_value(volatile rx_s12ad_regs_t* adc,
                                            const adc_channel_t       channel,
                                            uint16_t*                 value)
{
  RX_CHECK_NULL_PTR(adc, s_tag, "ADC register pointer is NULL");
  RX_CHECK_NULL_PTR(value, s_tag, "value pointer is NULL");

  switch (channel) {
    case k_adc_channel_0:
      *value = adc->addr0;
      break;
    case k_adc_channel_1:
      *value = adc->addr1;
      break;
    case k_adc_channel_2:
      *value = adc->addr2;
      break;
    case k_adc_channel_3:
      *value = adc->addr3;
      break;
    case k_adc_channel_4:
      *value = adc->addr4;
      break;
    case k_adc_channel_5:
      *value = adc->addr5;
      break;
    case k_adc_channel_6:
      *value = adc->addr6;
      break;
    case k_adc_channel_7:
      *value = adc->addr7;
      break;
    default:
      rx_log_error(s_tag, "Unsupported channel");
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize ADC unit and enable a specific channel
 *
 * Configures the S12ADFa peripheral for A/D conversion. The ADC unit
 * is initialized on first use with the specified resolution. Subsequent
 * calls enable additional channels on the same unit.
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to enable (0-7)
 * @param[in] bits Resolution setting (8, 10, or 12 bits)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if unit, channel, or bits is invalid
 *
 * @note Resolution is set only on first initialization of each unit.
 *       Subsequent calls ignore the bits parameter for that unit.
 *
 * @code{.c}
 * // Initialize ADC unit 0, channel 2 with 12-bit resolution
 * rx_err_t err = adc_init(0, 2, 12);
 * if (err != k_rx_ok) {
 *     // Handle error
 * }
 * @endcode
 */
rx_err_t adc_init(const adc_unit_t unit, const adc_channel_t channel, const adc_resolution_t bits)
{
  volatile rx_s12ad_regs_t* adc = NULL;
  rx_err_t                  err;

  /* Validate parameters */
  err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Validate resolution */
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return k_rx_err_invalid_arg;
  }

  /* Get ADC base */
  adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize ADC unit if not already initialized */
  if (!s_adc_unit_initialized[unit]) {
    /* Store resolution for validation on subsequent calls */
    s_adc_unit_resolution[unit] = bits;

    err = internal_configure_adc_unit(unit, adc, bits);
    if (err != k_rx_ok) {
      return err;
    }
  } else {
    /* Unit already initialized - verify resolution matches */
    if (bits != s_adc_unit_resolution[unit]) {
      rx_log_error(s_tag, "ADC resolution mismatch on subsequent init");
      return k_rx_err_invalid_arg;
    }
  }

  /* Enable the specified channel */
  if (channel <= k_adc_channel_adansa0_max) {
    adc->adansa0 |= ((uint16_t)k_adc_bit_one << channel);
  } else {
    adc->adansa1 |= ((uint16_t)k_adc_bit_one << (channel - k_adc_channel_adansa1_base));
  }

  rx_log_debug(s_tag, "ADC channel enabled");

  return k_rx_ok;
}

/**
 * @brief Read raw ADC conversion value from specified channel
 *
 * Performs a single-scan A/D conversion and returns the raw digital value.
 * The conversion is synchronous (blocking) with a configurable timeout.
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to read (0-7)
 * @param[out] value Pointer to store the raw ADC value (12/10/8-bit depending on init)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if value is NULL
 * @return k_rx_err_invalid_arg if unit or channel is invalid
 * @return k_rx_err_invalid_state if ADC unit not initialized
 * @return k_rx_err_timeout if conversion times out
 *
 * @pre ADC unit must be initialized via adc_init()
 *
 * @code{.c}
 * uint16_t raw_value;
 * rx_err_t err = adc_read(0, 2, &raw_value);
 * if (err == k_rx_ok) {
 *     // Use raw_value (0-4095 for 12-bit)
 * }
 * @endcode
 */
rx_err_t adc_read(const adc_unit_t unit, const adc_channel_t channel, uint16_t* value)
{
  volatile rx_s12ad_regs_t* adc;
  rx_err_t                  err;
  uint32_t                  timeout;

  /* Validate parameters */
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is NULL");

  err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Check if unit is initialized */
  if (!s_adc_unit_initialized[unit]) {
    rx_log_error(s_tag, "ADC unit not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get ADC base */
  adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Start single-scan conversion */
  adc->adcsr = k_adc_adcsr_adst;

  /* Wait for conversion to complete (poll ADCSR.ADST bit) */
  timeout = k_adc_timeout_ms * k_adc_timeout_multiplier;
  while ((adc->adcsr & k_adc_adcsr_adst) != k_adc_adcsr_idle && timeout > k_adc_timeout_expired) {
    timeout--;
  }

  if (timeout == k_adc_timeout_expired) {
    rx_log_error(s_tag, "ADC conversion timeout");
    return k_rx_err_timeout;
  }

  /* Read conversion result from appropriate data register */
  return internal_read_channel_value(adc, channel, value);
}

/**
 * @brief Read ADC value and convert to millivolts
 *
 * Performs an A/D conversion and scales the result to millivolts based on
 * the reference voltage (3.3V) and configured resolution.
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to read (0-7)
 * @param[in] bits Resolution used during initialization (8, 10, or 12)
 * @param[out] voltage_mv Pointer to store voltage in millivolts (0-3300)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if voltage_mv is NULL
 * @return k_rx_err_invalid_arg if unit, channel, or bits is invalid
 * @return k_rx_err_invalid_state if ADC unit not initialized
 * @return k_rx_err_timeout if conversion times out
 *
 * @pre ADC unit and channel must be initialized via adc_init()
 * @pre Resolution (bits) must match the value used in adc_init()
 *
 * @note Uses 3.3V reference voltage (k_adc_reference_voltage_mv = 3300)
 *
 * @code{.c}
 * uint32_t voltage;
 * rx_err_t err = adc_read_voltage_mv(0, 2, 12, &voltage);
 * if (err == k_rx_ok) {
 *     // voltage is in millivolts (0-3300)
 * }
 * @endcode
 */
rx_err_t adc_read_voltage_mv(const adc_unit_t unit,
                             adc_channel_t    channel,
                             adc_resolution_t bits,
                             uint32_t*        voltage_mv)
{
  uint16_t raw_value;
  rx_err_t err;
  uint32_t max_value;

  /* Parameter order: unit, channel, resolution bits. */
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "Voltage pointer is NULL");

  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return k_rx_err_invalid_arg;
  }

  /* Validate that passed resolution matches unit's configured resolution */
  if (unit < k_adc_unit_0 || unit > k_adc_unit_1) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  if (bits != s_adc_unit_resolution[unit]) {
    rx_log_error(s_tag, "ADC resolution mismatch in voltage read");
    return k_rx_err_invalid_arg;
  }

  /* Read raw ADC value */
  err = adc_read(unit, channel, &raw_value);
  RX_RETURN_ON_ERROR(err, s_tag, "ADC read failed");

  /* Calculate voltage (using ADC reference voltage) */
  max_value   = ((uint32_t)k_adc_bit_one << bits) - k_adc_bit_one;
  *voltage_mv = ((uint32_t)raw_value * k_adc_reference_voltage_mv) / max_value;

  return k_rx_ok;
}
