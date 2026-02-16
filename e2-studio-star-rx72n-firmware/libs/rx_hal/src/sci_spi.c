/* libs/rx_hal/src/sci_spi.c */

/**
 * @file sci_spi.c
 * @brief SCI SPI Controller Mode Driver Implementation
 *
 * @details
 * Implements SPI controller functionality using the RX72N SCI peripheral in
 * clock-synchronous mode. Uses 8-bit TDR/RDR registers; 16-bit frames are
 * composed of two consecutive 8-bit transfers with CS held throughout.
 *
 * @see rx_sci_spi.h Public API header
 * @see rx72n_sci_regs.h SCI register definitions
 *
 * @author STAR Team
 * @date 2026-02-11
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "hardware.h"
#include "rx72n_sci_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_sci_spi.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Log tag for this module */
static const char* const s_tag = "SCI_SPI";

/** @brief SCI SPI register constants */
typedef enum : uint8_t {
  k_sci_spi_smr_sync    = 0x80, /**< SMR.CM=1: clock-synchronous mode, CKS=00 */
  k_sci_spi_scr_disable = 0x00, /**< SCR: all disabled */
  k_sci_spi_scr_enable  = 0x33, /**< SCR: TE=1, RE=1, CKE=11 (SCK output) */
  k_sci_spi_scmr_msb    = 0x08, /**< SCMR.SDIR=1: MSB-first */
  k_sci_spi_ssr_clear   = 0x84, /**< SSR: clear error flags, keep TDRE */
  k_sci_spi_ssr_tdre    = 0x80, /**< SSR.TDRE: transmit data register empty */
  k_sci_spi_ssr_rdrf    = 0x40, /**< SSR.RDRF: receive data register full */
  k_sci_spi_ssr_errors  = 0x38, /**< SSR error mask: ORER|FER|PER */
} sci_spi_reg_constants_t;

/** @brief SPI mode to SPMR register value mapping */
typedef enum : uint8_t {
  k_spmr_mode_0 = 0x00, /**< CPOL=0, CPHA=0: CKPOL=0, CKPH=0 */
  k_spmr_mode_1 = 0x80, /**< CPOL=0, CPHA=1: CKPOL=0, CKPH=1 */
  k_spmr_mode_2 = 0x40, /**< CPOL=1, CPHA=0: CKPOL=1, CKPH=0 */
  k_spmr_mode_3 = 0xC0, /**< CPOL=1, CPHA=1: CKPOL=1, CKPH=1 */
} spmr_mode_values_t;

/** @brief Bit shift and timeout constants */
typedef enum : uint32_t {
  k_sci_spi_byte_shift    = 8,        /**< Shift for high byte of 16-bit value */
  k_sci_spi_byte_mask     = 0xFF,     /**< Mask for extracting a single byte */
  k_sci_spi_timeout_count = 100000,   /**< Polling loop timeout (NASA Rule 2) */
  k_sci_spi_pclkb_hz      = 60000000, /**< PCLKB frequency (60 MHz) */
  k_sci_spi_brr_divisor   = 4,        /**< BRR formula: rate = PCLKB/(4*(BRR+1)) */
  k_sci_spi_max_channels  = 13,       /**< Support all SCI channels 0-12 */
  k_sci_spi_cs_setup_nops = 10,       /**< NOP count for CS setup delay (~300ns) */
  k_sci_spi_cs_hold_nops  = 10,       /**< NOP count for CS hold delay (~300ns) */
  k_sci_spi_max_spi_mode  = 3,        /**< Maximum valid SPI mode value */
} sci_spi_constants_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/** @brief Per-channel initialization tracking */
typedef struct {
  bool          initialized; /**< True if channel is initialized */
  rx_port_pin_t cs_pin;      /**< Chip select GPIO pin */
} sci_spi_state_t;

/** @brief Channel state for all SCI channels (0-12) */
static sci_spi_state_t s_channels[k_sci_spi_max_channels];

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Map external channel number to internal state index
 * @details
 * Performs a direct 1:1 mapping from SCI channel number (0–12) to
 * the corresponding index in the s_channels[] state array.
 *
 * @param[in]  channel External SCI channel number
 * @param[out] idx     Pointer to store internal index
 *
 * @return k_rx_ok on success, error code otherwise
 * @retval k_rx_ok            Channel mapped successfully
 * @retval k_rx_err_invalid_arg Channel out of range or idx is NULL
 *
 * @pre idx != NULL
 * @pre channel is a hardware-valid SCI channel number
 * @post On k_rx_ok, *idx < k_sci_spi_max_channels
 * @post On error, *idx is unchanged
 *
 * @note Thread-safe. Pure function with no shared state access.
 *
 * @since 1.1.0
 */
static rx_err_t internal_channel_to_index(uint8_t channel, uint8_t* idx)
{
  if (idx == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (channel < k_sci_spi_max_channels) { /* SCI0 through SCI12 */
    *idx = channel;                       /* Direct mapping: channel number = array index */
    return k_rx_ok;
  }
  return k_rx_err_invalid_arg;
}

/**
 * @brief Get SPMR register value for a given SPI mode
 * @param[in] mode SPI mode (0-3)
 * @return SPMR register value
 */
static uint8_t internal_spmr_for_mode(uint8_t mode)
{
  switch (mode) {
    case k_sci_spi_mode_0:
      return k_spmr_mode_0;
    case k_sci_spi_mode_1:
      return k_spmr_mode_1;
    case k_sci_spi_mode_2:
      return k_spmr_mode_2;
    case k_sci_spi_mode_3:
      return k_spmr_mode_3;
    default:
      return k_spmr_mode_0;
  }
}

/**
 * @brief Assert chip select (drive LOW)
 * @param[in] cs_pin GPIO pin for chip select
 */
static void internal_cs_assert(rx_port_pin_t cs_pin)
{
  (void)gpio_write_low(cs_pin);

  /* CS setup delay (~300ns at 240 MHz) */
  for (uint8_t i = 0; i < k_sci_spi_cs_setup_nops; i++) {
    __asm__ volatile("nop");
  }
}

/**
 * @brief Deassert chip select (drive HIGH)
 * @param[in] cs_pin GPIO pin for chip select
 */
static void internal_cs_deassert(rx_port_pin_t cs_pin)
{
  /* CS hold delay (~300ns at 240 MHz) */
  for (uint8_t i = 0; i < k_sci_spi_cs_hold_nops; i++) {
    __asm__ volatile("nop");
  }

  (void)gpio_write_high(cs_pin);
}

/**
 * @brief Transfer a single byte via SCI SPI
 * @param[in]  sci     SCI register pointer
 * @param[in]  tx_byte Byte to transmit
 * @param[out] rx_byte Pointer to store received byte
 * @return k_rx_ok on success, k_rx_err_timeout on timeout
 */
static rx_err_t
internal_transfer_byte(volatile rx_sci_regs_t* sci, uint8_t tx_byte, uint8_t* rx_byte)
{
  uint32_t timeout;

  /* Wait for TDRE=1 (transmit buffer empty) */
  timeout = k_sci_spi_timeout_count;
  while ((sci->ssr & k_sci_spi_ssr_tdre) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Write TX byte */
  sci->tdr = tx_byte;

  /* Wait for RDRF=1 (receive data ready) */
  timeout = k_sci_spi_timeout_count;
  while ((sci->ssr & k_sci_spi_ssr_rdrf) == 0) {
    if (--timeout == 0) {
      return k_rx_err_timeout;
    }
  }

  /* Read RX byte */
  *rx_byte = sci->rdr;

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t sci_spi_init_controller(uint8_t channel, const sci_spi_controller_config_t* config)
{
  uint8_t idx;
  uint8_t brr_value;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Unsupported SCI channel for SPI");
    return k_rx_err_invalid_arg;
  }

  if (s_channels[idx].initialized) {
    rx_log_error(s_tag, "SCI SPI channel already initialized");
    return k_rx_err_invalid_state;
  }

  if (config->freq_hz == 0 || config->spi_mode > k_sci_spi_max_spi_mode) {
    rx_log_error(s_tag, "Invalid SPI configuration");
    return k_rx_err_invalid_arg;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    rx_log_error(s_tag, "SCI register base is NULL");
    return k_rx_err_hw_init_failed;
  }

  /* 1. Disable TE/RE during configuration */
  sci->scr = k_sci_spi_scr_disable;

  /* 2. Set clock-synchronous mode (SMR.CM=1), PCLKB/1 */
  sci->smr = k_sci_spi_smr_sync;

  /* 3. Set MSB-first transfer order (SCMR.SDIR=1) */
  sci->scmr = k_sci_spi_scmr_msb;

  /* 4. Set SPI mode via SPMR register */
  sci->spmr = internal_spmr_for_mode(config->spi_mode);

  /* 5. Calculate and set BRR: bit_rate = PCLKB / (4 * (BRR + 1)) */
  brr_value = (uint8_t)((k_sci_spi_pclkb_hz / (k_sci_spi_brr_divisor * config->freq_hz)) - 1);
  sci->brr  = brr_value;

  /* 6. Clear error flags */
  sci->ssr = k_sci_spi_ssr_clear;

  /* 7. Configure CS GPIO: output, initial HIGH (deselected) */
  err = gpio_set_output(config->cs);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to configure CS GPIO as output");
    return err;
  }
  err = gpio_write_high(config->cs);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to deassert CS GPIO");
    return err;
  }

  /* 8. Enable TE + RE + internal clock output */
  sci->scr = k_sci_spi_scr_enable;

  /* 9. Track init state */
  s_channels[idx].initialized = true;
  s_channels[idx].cs_pin      = config->cs;

  rx_log_info(s_tag, "SCI SPI controller initialized");
  return k_rx_ok;
}

rx_err_t sci_spi_controller_transfer_16bit(uint8_t channel, uint16_t tx_data, uint16_t* rx_data)
{
  uint8_t idx;
  uint8_t tx_msb;
  uint8_t tx_lsb;
  uint8_t rx_msb = 0;
  uint8_t rx_lsb = 0;

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channels[idx].initialized) {
    return k_rx_err_invalid_state;
  }

  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == nullptr) {
    return k_rx_err_hw_init_failed;
  }

  /* Split 16-bit frame into two bytes */
  tx_msb = (uint8_t)(tx_data >> k_sci_spi_byte_shift);
  tx_lsb = (uint8_t)(tx_data & k_sci_spi_byte_mask);

  /* Assert CS */
  internal_cs_assert(s_channels[idx].cs_pin);

  /* Transfer MSB */
  err = internal_transfer_byte(sci, tx_msb, &rx_msb);
  if (err != k_rx_ok) {
    internal_cs_deassert(s_channels[idx].cs_pin);
    return err;
  }

  /* Transfer LSB */
  err = internal_transfer_byte(sci, tx_lsb, &rx_lsb);
  if (err != k_rx_ok) {
    internal_cs_deassert(s_channels[idx].cs_pin);
    return err;
  }

  /* Deassert CS */
  internal_cs_deassert(s_channels[idx].cs_pin);

  /* Combine response bytes */
  if (rx_data != nullptr) {
    *rx_data = (uint16_t)((uint16_t)rx_msb << k_sci_spi_byte_shift) | rx_lsb;
  }

  return k_rx_ok;
}

rx_err_t sci_spi_controller_deinit(uint8_t channel)
{
  uint8_t idx;

  rx_err_t err = internal_channel_to_index(channel, &idx);
  if (err != k_rx_ok) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channels[idx].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Disable TE/RE */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci != nullptr) {
    sci->scr = k_sci_spi_scr_disable;
  }

  /* Clear init state */
  s_channels[idx].initialized = false;

  rx_log_info(s_tag, "SCI SPI controller deinitialized");
  return k_rx_ok;
}
