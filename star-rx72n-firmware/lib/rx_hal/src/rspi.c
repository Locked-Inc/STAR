/* src/hardware/rspi.c */

/**
 * @file rspi.c
 * @brief RSPI (SPI) Driver for RX72N
 * @details
 * Provides SPI peripheral mode communication for RPi5 interface.
 * RX72N acts as SPI peripheral, RPi5 acts as SPI controller.
 *
 * Supports:
 * - Full-duplex communication
 * - 8-bit and 16-bit data frames
 * - SPI modes 0-3
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_rspi_max_channels = 3,     /* RSPI0, RSPI1, RSPI2 */
  k_rspi_timeout_us   = 10000, /* 10ms timeout for operations */
} rspi_constants_t;

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
static volatile RSPI_Type* internal_get_rspi_base(uint8_t channel)
{
  switch (channel) {
    case 0: {
      return &RSPI0;
    }
    case 1: {
      return &RSPI1;
    }
    case 2: {
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
    star_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate mode (0-3) */
  if (mode > 3) {
    star_log_error(s_tag, "Invalid SPI mode (must be 0-3)");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile RSPI_Type* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RSPI module (clear module stop bit) */
  SYSTEM.PRCR = 0xA50B; /* Enable writes to MSTPCR */

  if (channel == 0) {
    SYSTEM.MSTPCRB &= ~(1 << 17); /* RSPI0 */
  } else if (channel == 1) {
    SYSTEM.MSTPCRB &= ~(1 << 16); /* RSPI1 */
  } else {
    SYSTEM.MSTPCRB &= ~(1 << 15); /* RSPI2 */
  }

  SYSTEM.PRCR = 0xA500; /* Lock MSTPCR */

  /* Disable SPI before configuration */
  rspi->SPCR = 0;

  /* Configure SPI mode (CPOL and CPHA) */
  uint16_t spcmd = 0;
  if (mode & 0x02) {
    spcmd |= (1 << 1); /* CPOL = 1 */
  }
  if (mode & 0x01) {
    spcmd |= (1 << 0); /* CPHA = 1 */
  }

  /* Configure data length */
  if (use_16bit) {
    spcmd |= (0x0F << 8);   /* 16-bit data */
    rspi->SPDCR = (1 << 4); /* Word access mode */
  } else {
    spcmd |= (0x07 << 8); /* 8-bit data */
    rspi->SPDCR = 0;      /* Byte access mode */
  }

  rspi->SPCMD0 = spcmd;

  /* Configure peripheral mode */
  rspi->SPCR = k_rspi_spcr_spe; /* Enable SPI in peripheral mode (MSTR=0) */

  /* Configure pin control (no loopback) */
  rspi->SPPCR = 0;

  /* Mark channel as initialized */
  s_rspi_channel_initialized[channel] = true;

  star_log_debug(s_tag, "RSPI peripheral mode initialized");

  return k_rx_ok;
}

rx_err_t
rspi_peripheral_transfer(uint8_t channel, const uint8_t* tx_data, uint8_t* rx_data, uint16_t length)
{
  RX_CHECK_NULL_PTR(tx_data, s_tag, "TX data pointer is NULL");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    star_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile RSPI_Type* rspi = internal_get_rspi_base(channel);

  for (uint16_t i = 0; i < length; i++) {
    /* Wait for transmit buffer empty */
    uint32_t timeout = k_rspi_timeout_us;
    while (!(rspi->SPSR & k_rspi_spsr_sptef) && timeout > 0) {
      timeout--;
    }

    if (timeout == 0) {
      star_log_error(s_tag, "SPI transmit timeout");
      return k_rx_err_timeout;
    }

    /* Write transmit data */
    rspi->SPDR = tx_data[i];

    /* Wait for receive buffer full */
    timeout = k_rspi_timeout_us;
    while (!(rspi->SPSR & k_rspi_spsr_sprf) && timeout > 0) {
      timeout--;
    }

    if (timeout == 0) {
      star_log_error(s_tag, "SPI receive timeout");
      return k_rx_err_timeout;
    }

    /* Read receive data */
    rx_data[i] = (uint8_t)rspi->SPDR;

    /* Clear status flags */
    rspi->SPSR &= ~(k_rspi_spsr_sprf | k_rspi_spsr_ovrf);
  }

  return k_rx_ok;
}

rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available)
{
  RX_CHECK_NULL_PTR(available, s_tag, "Available pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    star_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile RSPI_Type* rspi = internal_get_rspi_base(channel);

  /* Check if receive buffer has data */
  *available = (rspi->SPSR & k_rspi_spsr_sprf) != 0;

  return k_rx_ok;
}

rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready)
{
  RX_CHECK_NULL_PTR(ready, s_tag, "Ready pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    star_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile RSPI_Type* rspi = internal_get_rspi_base(channel);

  /* Check if transmit buffer is empty */
  *ready = (rspi->SPSR & k_rspi_spsr_sptef) != 0;

  return k_rx_ok;
}

rx_err_t rspi_deinit(uint8_t channel)
{
  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    star_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile RSPI_Type* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Disable SPI */
  rspi->SPCR = 0;

  /* Disable RSPI module (set module stop bit) */
  SYSTEM.PRCR = 0xA50B; /* Enable writes to MSTPCR */

  if (channel == 0) {
    SYSTEM.MSTPCRB |= (1 << 17); /* RSPI0 */
  } else if (channel == 1) {
    SYSTEM.MSTPCRB |= (1 << 16); /* RSPI1 */
  } else {
    SYSTEM.MSTPCRB |= (1 << 15); /* RSPI2 */
  }

  SYSTEM.PRCR = 0xA500; /* Lock MSTPCR */

  /* Mark channel as uninitialized */
  s_rspi_channel_initialized[channel] = false;

  star_log_debug(s_tag, "RSPI deinitialized");

  return k_rx_ok;
}
