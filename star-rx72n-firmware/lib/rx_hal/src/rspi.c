/* lib/rx_hal/src/rspi.c */

/**
 * @file rspi.c
 * @brief RSPI (SPI) Driver for RX72N
 *
 * Provides SPI peripheral mode communication for RPi5 interface.
 * RX72N acts as SPI peripheral, RPi5 acts as SPI controller.
 *
 * Supports:
 * - Full-duplex communication
 * - 8-bit and 16-bit data frames
 * - SPI modes 0-3
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief RSPI channel and timeout constants */
typedef enum {
  k_rspi_max_channels = 3,     /**< RSPI0, RSPI1, RSPI2 */
  k_rspi_timeout_us   = 10000, /**< 10ms timeout for operations */
  k_rspi_timeout_zero = 0,     /**< Timeout expired value */
} rspi_constants_t;

/** @brief RSPI channel numbers for switch statements */
typedef enum {
  k_rspi_channel_0 = 0, /**< RSPI0 */
  k_rspi_channel_1 = 1, /**< RSPI1 */
  k_rspi_channel_2 = 2, /**< RSPI2 */
} rspi_channel_num_t;

/** @brief System protection register values */
typedef enum {
  k_rspi_prcr_unlock = 0xA50B, /**< Enable writes to MSTPCR */
  k_rspi_prcr_lock   = 0xA500, /**< Disable writes to MSTPCR */
} rspi_prcr_values_t;

/** @brief RSPI module stop bit positions in MSTPCRB */
typedef enum {
  k_rspi_mstpb_rspi0 = 17, /**< RSPI0 module stop bit */
  k_rspi_mstpb_rspi1 = 16, /**< RSPI1 module stop bit */
  k_rspi_mstpb_rspi2 = 15, /**< RSPI2 module stop bit */
} rspi_module_stop_bits_t;

/** @brief SPI mode limits */
typedef enum {
  k_rspi_mode_max = 3, /**< Maximum valid SPI mode (0-3) */
} rspi_mode_limits_t;

/** @brief SPCMD register bit positions and masks */
typedef enum {
  k_rspi_spcmd_cpha_mask = 0x01, /**< CPHA bit mask (bit 0) */
  k_rspi_spcmd_cpol_mask = 0x02, /**< CPOL bit mask (bit 1) */
  k_rspi_spcmd_cpol_pos  = 1,    /**< CPOL bit position */
  k_rspi_spcmd_cpha_pos  = 0,    /**< CPHA bit position */
  k_rspi_spcmd_spl_shift = 8,    /**< SPL (data length) bit shift */
  k_rspi_spcmd_8bit      = 0x07, /**< 8-bit data length value */
  k_rspi_spcmd_16bit     = 0x0F, /**< 16-bit data length value */
} rspi_spcmd_bits_t;

/** @brief SPDCR register bit positions (local) */
typedef enum {
  k_rspi_spdcr_splw_pos  = 4,    /**< SPLW bit position (word access) */
  k_rspi_spdcr_byte_mode = 0x00, /**< Byte access mode */
} rspi_spdcr_local_t;

/** @brief SPPCR register values */
typedef enum {
  k_rspi_sppcr_no_loopback = 0x00, /**< No loopback mode */
} rspi_sppcr_values_t;

/** @brief SPCR register disabled value */
typedef enum {
  k_rspi_spcr_disabled = 0x00, /**< SPI disabled */
} rspi_spcr_values_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* s_tag = "RSPI";

/* Track initialized RSPI channels */
static bool s_rspi_channel_initialized[k_rspi_max_channels] = {false, false, false};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get RSPI base address from channel number
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return Pointer to RSPI register base, or NULL if invalid channel
 */
static volatile rx_rspi_regs_t* internal_get_rspi_base(uint8_t channel)
{
  switch (channel) {
    case k_rspi_channel_0: {
      return &RSPI0;
    }
    case k_rspi_channel_1: {
      return &RSPI1;
    }
    case k_rspi_channel_2: {
      return &RSPI2;
    }
    default: {
      return NULL;
    }
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rspi_init_peripheral(uint8_t channel, uint8_t mode, bool use_16bit)
{
  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate mode (0-3) */
  if (mode > k_rspi_mode_max) {
    rx_log_error(s_tag, "Invalid SPI mode (must be 0-3)");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RSPI module (clear module stop bit) */
  SYSTEM.prcr = k_rspi_prcr_unlock;

  if (channel == k_rspi_channel_0) {
    SYSTEM.mstpcrb &= ~(1UL << k_rspi_mstpb_rspi0);
  } else if (channel == k_rspi_channel_1) {
    SYSTEM.mstpcrb &= ~(1UL << k_rspi_mstpb_rspi1);
  } else {
    SYSTEM.mstpcrb &= ~(1UL << k_rspi_mstpb_rspi2);
  }

  SYSTEM.prcr = k_rspi_prcr_lock;

  /* Disable SPI before configuration */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Configure SPI mode (CPOL and CPHA) */
  uint16_t spcmd = 0;
  if (mode & k_rspi_spcmd_cpol_mask) {
    spcmd |= (1 << k_rspi_spcmd_cpol_pos); /* CPOL = 1 */
  }
  if (mode & k_rspi_spcmd_cpha_mask) {
    spcmd |= (1 << k_rspi_spcmd_cpha_pos); /* CPHA = 1 */
  }

  /* Configure data length */
  if (use_16bit) {
    spcmd |= (k_rspi_spcmd_16bit << k_rspi_spcmd_spl_shift); /* 16-bit data */
    rspi->spdcr = (1 << k_rspi_spdcr_splw_pos);              /* Word access mode */
  } else {
    spcmd |= (k_rspi_spcmd_8bit << k_rspi_spcmd_spl_shift); /* 8-bit data */
    rspi->spdcr = k_rspi_spdcr_byte_mode;                   /* Byte access mode */
  }

  rspi->spcmd0 = spcmd;

  /* Configure peripheral mode */
  rspi->spcr = k_rspi_spcr_spe; /* Enable SPI in peripheral mode (MSTR=0) */

  /* Configure pin control (no loopback) */
  rspi->sppcr = k_rspi_sppcr_no_loopback;

  /* Mark channel as initialized */
  s_rspi_channel_initialized[channel] = true;

  rx_log_debug(s_tag, "RSPI peripheral mode initialized");

  return k_rx_ok;
}

rx_err_t
rspi_peripheral_transfer(uint8_t channel, const uint8_t* tx_data, uint8_t* rx_data, uint16_t length)
{
  RX_CHECK_NULL_PTR(tx_data, s_tag, "TX data pointer is NULL");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);

  for (uint16_t i = 0; i < length; i++) {
    /* Wait for transmit buffer empty */
    uint32_t timeout = k_rspi_timeout_us;
    while (!(rspi->spsr & k_rspi_spsr_sptef) && timeout > k_rspi_timeout_zero) {
      timeout--;
    }

    if (timeout == k_rspi_timeout_zero) {
      rx_log_error(s_tag, "SPI transmit timeout");
      return k_rx_err_timeout;
    }

    /* Write transmit data */
    rspi->spdr = tx_data[i];

    /* Wait for receive buffer full */
    timeout = k_rspi_timeout_us;
    while (!(rspi->spsr & k_rspi_spsr_sprf) && timeout > k_rspi_timeout_zero) {
      timeout--;
    }

    if (timeout == k_rspi_timeout_zero) {
      rx_log_error(s_tag, "SPI receive timeout");
      return k_rx_err_timeout;
    }

    /* Read receive data */
    rx_data[i] = (uint8_t)rspi->spdr;

    /* Clear status flags */
    rspi->spsr &= ~(k_rspi_spsr_sprf | k_rspi_spsr_ovrf);
  }

  return k_rx_ok;
}

rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available)
{
  RX_CHECK_NULL_PTR(available, s_tag, "Available pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);

  /* Check if receive buffer has data */
  *available = (rspi->spsr & k_rspi_spsr_sprf) != 0;

  return k_rx_ok;
}

rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready)
{
  RX_CHECK_NULL_PTR(ready, s_tag, "Ready pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);

  /* Check if transmit buffer is empty */
  *ready = (rspi->spsr & k_rspi_spsr_sptef) != 0;

  return k_rx_ok;
}

rx_err_t rspi_deinit(uint8_t channel)
{
  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Disable SPI */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Disable RSPI module (set module stop bit) */
  SYSTEM.prcr = k_rspi_prcr_unlock;

  if (channel == k_rspi_channel_0) {
    SYSTEM.mstpcrb |= (1UL << k_rspi_mstpb_rspi0);
  } else if (channel == k_rspi_channel_1) {
    SYSTEM.mstpcrb |= (1UL << k_rspi_mstpb_rspi1);
  } else {
    SYSTEM.mstpcrb |= (1UL << k_rspi_mstpb_rspi2);
  }

  SYSTEM.prcr = k_rspi_prcr_lock;

  /* Mark channel as uninitialized */
  s_rspi_channel_initialized[channel] = false;

  rx_log_debug(s_tag, "RSPI deinitialized");

  return k_rx_ok;
}
