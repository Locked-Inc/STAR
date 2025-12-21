/* src/hardware/adc.c */

/**
 * @file adc.c
 * @brief ADC Driver for RX72N S12ADFa
 * @details
 * Provides 12-bit A/D conversion on RX72N using S12ADFa peripheral.
 * Supports ADC unit 0 and 1 with multiple channels.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "hardware.h"
#include "rx72n_regs.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_adc_max_units    = 2,  /* S12AD0, S12AD1 */
  k_adc_max_channels = 8,  /* Channels 0-7 per unit */
  k_adc_timeout_ms   = 100, /* ADC conversion timeout */
} adc_constants_t;

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
static volatile S12AD_Type* internal_get_adc_base(uint8_t unit)
{
  switch (unit) {
    case 0: {
      return &S12AD0;
    }
    case 1: {
      return &S12AD1;
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
 * @return RX_OK if valid, error code otherwise
 */
static rx_err_t internal_validate_unit_channel(uint8_t unit, uint8_t channel)
{
  /* Validate unit */
  if (unit >= k_adc_max_units) {
    RX_LOG_ERROR(s_tag, "Invalid ADC unit");
    return RX_ERR_INVALID_ARG;
  }

  /* Validate channel */
  if (channel >= k_adc_max_channels) {
    RX_LOG_ERROR(s_tag, "Invalid ADC channel");
    return RX_ERR_INVALID_ARG;
  }

  return RX_OK;
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
  if (bits != 8 && bits != 10 && bits != 12) {
    RX_LOG_ERROR(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return RX_ERR_INVALID_ARG;
  }

  /* Get ADC base */
  volatile S12AD_Type* adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Initialize ADC unit if not already initialized */
  if (!s_adc_unit_initialized[unit]) {
    /* Unlock module stop control */
    SYSTEM.PRCR = 0xA50B; /* Enable writes to MSTPCR */

    /* Enable ADC module (clear module stop bit) */
    if (unit == 0) {
      SYSTEM.MSTPCRA &= ~(1 << 17); /* S12AD0 */
    } else {
      SYSTEM.MSTPCRA &= ~(1 << 16); /* S12AD1 */
    }

    /* Lock module stop control */
    SYSTEM.PRCR = 0xA500;

    /* Configure ADC resolution */
    uint16_t adcer = adc->ADCER;
    adcer &= ~(0x03 << 0); /* Clear ADPRC bits */
    if (bits == 8) {
      adcer |= (0x02 << 0); /* 8-bit mode */
    } else if (bits == 10) {
      adcer |= (0x01 << 0); /* 10-bit mode */
    } else {
      adcer |= (0x00 << 0); /* 12-bit mode */
    }
    adc->ADCER = adcer;

    /* Mark unit as initialized */
    s_adc_unit_initialized[unit] = true;

    RX_LOG_DEBUG(s_tag, "ADC unit initialized");
  }

  /* Enable the specified channel */
  if (channel < 16) {
    adc->ADANSA0 |= (1 << channel);
  } else {
    adc->ADANSA1 |= (1 << (channel - 16));
  }

  RX_LOG_DEBUG(s_tag, "ADC channel enabled");

  return RX_OK;
}

rx_err_t adc_read(uint8_t unit, uint8_t channel, uint16_t* value)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is NULL");

  rx_err_t err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Check if unit is initialized */
  if (!s_adc_unit_initialized[unit]) {
    RX_LOG_ERROR(s_tag, "ADC unit not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Get ADC base */
  volatile S12AD_Type* adc = internal_get_adc_base(unit);
  if (adc == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Start single-scan conversion */
  adc->ADCSR = 0x1000; /* ADST=1, start conversion */

  /* Wait for conversion to complete (poll ADCSR.ADST bit) */
  uint32_t timeout = k_adc_timeout_ms * 1000; /* Convert to loops */
  while ((adc->ADCSR & 0x1000) != 0 && timeout > 0) {
    timeout--;
  }

  if (timeout == 0) {
    RX_LOG_ERROR(s_tag, "ADC conversion timeout");
    return RX_ERR_TIMEOUT;
  }

  /* Read conversion result from appropriate data register */
  switch (channel) {
    case 0:
      *value = adc->ADDR0;
      break;
    case 1:
      *value = adc->ADDR1;
      break;
    case 2:
      *value = adc->ADDR2;
      break;
    case 3:
      *value = adc->ADDR3;
      break;
    case 4:
      *value = adc->ADDR4;
      break;
    case 5:
      *value = adc->ADDR5;
      break;
    case 6:
      *value = adc->ADDR6;
      break;
    case 7:
      *value = adc->ADDR7;
      break;
    default:
      RX_LOG_ERROR(s_tag, "Unsupported channel");
      return RX_ERR_INVALID_ARG;
  }

  return RX_OK;
}

rx_err_t adc_read_voltage_mv(uint8_t unit, uint8_t channel, uint8_t bits, uint32_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "Voltage pointer is NULL");

  /* Read raw ADC value */
  uint16_t raw_value;
  rx_err_t err = adc_read(unit, channel, &raw_value);
  RX_RETURN_ON_ERROR(err, s_tag, "ADC read failed");

  /* Calculate voltage (assuming 3.3V reference) */
  uint32_t max_value = (1 << bits) - 1;
  *voltage_mv        = ((uint32_t)raw_value * 3300) / max_value;

  return RX_OK;
}
