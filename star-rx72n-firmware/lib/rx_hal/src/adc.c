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
typedef enum {
  k_adc_max_units    = 2,   /**< S12AD0, S12AD1 */
  k_adc_max_channels = 8,   /**< Channels 0-7 per unit */
  k_adc_timeout_ms   = 100, /**< ADC conversion timeout (ms) */
} adc_constants_t;

/** @brief ADC resolution options */
typedef enum {
  k_adc_resolution_8bit  = 8,  /**< 8-bit ADC resolution */
  k_adc_resolution_10bit = 10, /**< 10-bit ADC resolution */
  k_adc_resolution_12bit = 12, /**< 12-bit ADC resolution (default) */
} adc_resolution_t;

/** @brief ADC module stop bit positions in MSTPCRA */
typedef enum {
  k_adc_mstpra_s12ad1 = 16, /**< S12AD1 module stop bit */
  k_adc_mstpra_s12ad0 = 17, /**< S12AD0 module stop bit */
} adc_module_stop_bits_t;

/** @brief ADC Control Extended Register (ADCER) bit masks and values */
typedef enum {
  k_adc_adcer_adprc_mask  = 0x03,        /**< ADPRC bit mask (bits 1:0) */
  k_adc_adcer_adprc_shift = 0,           /**< ADPRC bit shift position */
  k_adc_adcer_adprc_12bit = (0x00 << 0), /**< 12-bit resolution */
  k_adc_adcer_adprc_10bit = (0x01 << 0), /**< 10-bit resolution */
  k_adc_adcer_adprc_8bit  = (0x02 << 0), /**< 8-bit resolution */
} adc_adcer_bits_t;

/** @brief ADC Control/Status Register (ADCSR) bit masks */
typedef enum {
  k_adc_adcsr_adst = 0x1000, /**< A/D Conversion Start (bit 12) */
} adc_adcsr_bits_t;

/** @brief ADC channel register boundaries */
typedef enum {
  k_adc_channel_adansa0_max  = 15, /**< Maximum channel for ADANSA0 (channels 0-15) */
  k_adc_channel_adansa1_base = 16, /**< Base channel for ADANSA1 (channels 16+) */
} adc_channel_boundaries_t;

/** @brief ADC timeout and voltage constants */
typedef enum {
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
static volatile rx_s12ad_regs_t* internal_get_adc_base(uint8_t unit)
{
  switch (unit) {
    case 0: {
      return s12ad0();
    }
    case 1: {
      return s12ad1();
    }
    default: {
      return NULL;
    }
  }
}

/**
 * @brief Validate ADC unit and channel
 *
 * @param[in] unit ADC unit number
 * @param[in] channel ADC channel number
 *
 * @return k_rx_ok if valid, error code otherwise
 */
static rx_err_t internal_validate_unit_channel(uint8_t unit, uint8_t channel)
{
  /* Validate unit */
  if (unit >= k_adc_max_units) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel >= k_adc_max_channels) {
    rx_log_error(s_tag, "Invalid ADC channel");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t adc_init(uint8_t unit, uint8_t channel, uint8_t bits)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Validate resolution */
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return k_rx_err_invalid_arg;
  }

  /* Get ADC base */
  volatile rx_s12ad_regs_t* adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize ADC unit if not already initialized */
  if (!s_adc_unit_initialized[unit]) {
    /* Unlock module stop control */
    system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;

    /* Enable ADC module (clear module stop bit) */
    if (unit == 0) {
      system_regs()->mstpcra &= ~(1UL << k_adc_mstpra_s12ad0);
    } else {
      system_regs()->mstpcra &= ~(1UL << k_adc_mstpra_s12ad1);
    }

    /* Lock module stop control */
    system_regs()->prcr = k_rx_prcr_lock;

    /* Configure ADC resolution */
    uint16_t adcer = adc->adcer;
    adcer &= ~(k_adc_adcer_adprc_mask << k_adc_adcer_adprc_shift);
    if (bits == k_adc_resolution_8bit) {
      adcer |= k_adc_adcer_adprc_8bit;
    } else if (bits == k_adc_resolution_10bit) {
      adcer |= k_adc_adcer_adprc_10bit;
    } else {
      adcer |= k_adc_adcer_adprc_12bit;
    }
    adc->adcer = adcer;

    /* Mark unit as initialized */
    s_adc_unit_initialized[unit] = true;

    rx_log_debug(s_tag, "ADC unit initialized");
  }

  /* Enable the specified channel */
  if (channel <= k_adc_channel_adansa0_max) {
    adc->adansa0 |= (1 << channel);
  } else {
    adc->adansa1 |= (1 << (channel - k_adc_channel_adansa1_base));
  }

  rx_log_debug(s_tag, "ADC channel enabled");

  return k_rx_ok;
}

rx_err_t adc_read(uint8_t unit, uint8_t channel, uint16_t* value)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is NULL");

  rx_err_t err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Check if unit is initialized */
  if (!s_adc_unit_initialized[unit]) {
    rx_log_error(s_tag, "ADC unit not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get ADC base */
  volatile rx_s12ad_regs_t* adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Start single-scan conversion */
  adc->adcsr = k_adc_adcsr_adst;

  /* Wait for conversion to complete (poll ADCSR.ADST bit) */
  uint32_t timeout = k_adc_timeout_ms * k_adc_timeout_multiplier;
  while ((adc->adcsr & k_adc_adcsr_adst) != 0 && timeout > k_adc_timeout_expired) {
    timeout--;
  }

  if (timeout == k_adc_timeout_expired) {
    rx_log_error(s_tag, "ADC conversion timeout");
    return k_rx_err_timeout;
  }

  /* Read conversion result from appropriate data register */
  switch (channel) {
    case 0:
      *value = adc->addr0;
      break;
    case 1:
      *value = adc->addr1;
      break;
    case 2:
      *value = adc->addr2;
      break;
    case 3:
      *value = adc->addr3;
      break;
    case 4:
      *value = adc->addr4;
      break;
    case 5:
      *value = adc->addr5;
      break;
    case 6:
      *value = adc->addr6;
      break;
    case 7:
      *value = adc->addr7;
      break;
    default:
      rx_log_error(s_tag, "Unsupported channel");
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t adc_read_voltage_mv(uint8_t unit, uint8_t channel, uint8_t bits, uint32_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "Voltage pointer is NULL");

  /* Read raw ADC value */
  uint16_t raw_value;
  rx_err_t err = adc_read(unit, channel, &raw_value);
  RX_RETURN_ON_ERROR(err, s_tag, "ADC read failed");

  /* Calculate voltage (using ADC reference voltage) */
  uint32_t max_value = (1 << bits) - 1;
  *voltage_mv        = ((uint32_t)raw_value * k_adc_reference_voltage_mv) / max_value;

  return k_rx_ok;
}
