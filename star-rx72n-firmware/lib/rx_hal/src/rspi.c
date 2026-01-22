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

#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief RSPI channel and timeout constants */
typedef enum : uint32_t {
  k_rspi_max_channels = 3,     /**< RSPI0, RSPI1, RSPI2 */
  k_rspi_timeout_us   = 10000, /**< 10ms timeout for operations */
  k_rspi_timeout_zero = 0,     /**< Timeout expired value */
  k_rspi_len_zero     = 0,     /**< Zero-length transfer */
} rspi_constants_t;

/** @brief RSPI channel numbers for switch statements */
typedef enum : uint8_t {
  k_rspi_channel_0 = 0, /**< RSPI0 */
  k_rspi_channel_1 = 1, /**< RSPI1 */
  k_rspi_channel_2 = 2, /**< RSPI2 */
} rspi_channel_num_t;

/** @brief RSPI module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_rspi_mstpb_rspi0 = 17, /**< RSPI0 module stop bit */
  k_rspi_mstpb_rspi1 = 16, /**< RSPI1 module stop bit */
  k_rspi_mstpb_rspi2 = 15, /**< RSPI2 module stop bit */
} rspi_module_stop_bits_t;

/** @brief SPI mode limits */
typedef enum : uint8_t {
  k_rspi_mode_min = 0, /**< Minimum valid SPI mode (0) */
  k_rspi_mode_max = 3, /**< Maximum valid SPI mode (0-3) */
} rspi_mode_limits_t;

/** @brief SPCMD register bit positions and masks */
typedef enum : uint16_t {
  k_rspi_spcmd_cpha_mask = 1U,  /**< CPHA bit mask (bit 0) */
  k_rspi_spcmd_cpol_mask = 2U,  /**< CPOL bit mask (bit 1) */
  k_rspi_spcmd_cpol_pos  = 1,   /**< CPOL bit position */
  k_rspi_spcmd_cpha_pos  = 0,   /**< CPHA bit position */
  k_rspi_spcmd_spl_shift = 8,   /**< SPL (data length) bit shift */
  k_rspi_spcmd_8bit      = 7U,  /**< 8-bit data length value */
  k_rspi_spcmd_16bit     = 15U, /**< 16-bit data length value */
} rspi_spcmd_bits_t;

/** @brief SPDCR register bit positions (local) */
typedef enum : uint8_t {
  k_rspi_spdcr_splw_pos  = 4,  /**< SPLW bit position (word access) */
  k_rspi_spdcr_byte_mode = 0U, /**< Byte access mode */
} rspi_spdcr_local_t;

/** @brief SPPCR register values */
static const uint8_t s_rspi_sppcr_no_loopback = 0U; /**< No loopback mode */

/** @brief SPCR register disabled value */
static const uint8_t s_rspi_spcr_disabled = 0U; /**< SPI disabled */

/** @brief Bit manipulation constants */
static const uint32_t s_rspi_bit_set = 1UL;

/** @brief RSPI transfer limits */
static const uint16_t s_rspi_transfer_len_max = 65535U;

/** @brief SPCMD register initial value (all mode bits clear) */
static const uint16_t s_rspi_spcmd_init = 0U;

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
static volatile rx_rspi_regs_t* internal_get_rspi_base(const uint8_t channel)
{
  switch (channel) {
    case k_rspi_channel_0: {
      return rspi0();
    }
    case k_rspi_channel_1: {
      return rspi1();
    }
    case k_rspi_channel_2: {
      return rspi2();
    }
    default: {
      return NULL;
    }
  }
}

/**
 * @brief Set MSTPCRB module stop bit for an RSPI channel
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] enable True to enable module, false to stop module
 */
static void internal_set_mstpcrb_for_channel(const uint8_t channel, const bool enable)
{
  uint32_t mask = 0U;

  RX_ASSERT(system_regs() != NULL, "system_regs is NULL");
  RX_ASSERT(s_rspi_bit_set != 0U, "RSPI bit constant is zero");
  RX_ASSERT((channel == k_rspi_channel_0) || (channel == k_rspi_channel_1) ||
              (channel == k_rspi_channel_2),
            "Invalid RSPI channel");

  if (channel == k_rspi_channel_0) {
    mask = (s_rspi_bit_set << k_rspi_mstpb_rspi0);
  } else if (channel == k_rspi_channel_1) {
    mask = (s_rspi_bit_set << k_rspi_mstpb_rspi1);
  } else if (channel == k_rspi_channel_2) {
    mask = (s_rspi_bit_set << k_rspi_mstpb_rspi2);
  }
  if (enable) {
    system_regs()->mstpcrb &= ~mask;
  } else {
    system_regs()->mstpcrb |= mask;
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize RSPI peripheral mode for RPi5 SPI communication
 *
 * Configures the specified RSPI channel as a SPI peripheral (slave) to communicate
 * with the Raspberry Pi 5 (acting as SPI controller/master). Enables the peripheral
 * in the configured SPI mode (0-3) with optional 8-bit or 16-bit data frames.
 * Uses register protection unlock/lock for module stop control.
 *
 * @param[in] channel RSPI channel number (0-2, corresponding to RSPI0/RSPI1/RSPI2)
 * @param[in] config Pointer to rspi_config_t configuration structure
 *                   @c config must contain:
 *                   - @c spi_mode: SPI mode (0-3) controlling CPOL and CPHA bits
 *                   - @c use_16bit: True for 16-bit data frames, false for 8-bit
 *
 * @return k_rx_ok if initialization successful and channel marked as initialized
 * @return k_rx_err_invalid_arg if:
 *         - @c config pointer is NULL
 *         - @c channel is out of range (>= 3)
 *         - @c config->spi_mode exceeds maximum (> 3)
 *         - RSPI base address lookup fails for the channel
 *
 * @pre config pointer must be non-NULL
 * @pre channel must be in range [0, k_rspi_max_channels)
 * @pre config->spi_mode must be in range [0, 3]
 *
 * @post If successful:
 *       - RSPI module is enabled (module stop bit cleared via register protection)
 *       - SPI mode (CPOL/CPHA) is configured per config->spi_mode
 *       - Data frame length (8-bit or 16-bit) is set per config->use_16bit
 *       - SPI is enabled in peripheral (slave) mode (SPCR.MSTR = 0)
 *       - s_rspi_channel_initialized[channel] is set to true
 *       - Peripheral is ready to receive transfers from SPI controller
 */
rx_err_t rspi_init_peripheral(const uint8_t channel, const rspi_config_t* config)
{
  uint16_t spcmd = s_rspi_spcmd_init;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate mode (0-3) */
  if (config->spi_mode < k_rspi_mode_min || config->spi_mode > k_rspi_mode_max) {
    rx_log_error(s_tag, "Invalid SPI mode (must be 0-3)");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);

  /* Enable RSPI module (clear module stop bit) */
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, true);

  system_regs()->prcr = k_rx_prcr_lock;

  /* Disable SPI before configuration */
  rspi->spcr = s_rspi_spcr_disabled;

  /* Configure SPI mode (CPOL and CPHA) */
  if (config->spi_mode & k_rspi_spcmd_cpol_mask) {
    spcmd |= (uint16_t)(s_rspi_bit_set << k_rspi_spcmd_cpol_pos); /* CPOL = 1 */
  }
  if (config->spi_mode & k_rspi_spcmd_cpha_mask) {
    spcmd |= (uint16_t)(s_rspi_bit_set << k_rspi_spcmd_cpha_pos); /* CPHA = 1 */
  }

  /* Configure data length */
  if (config->use_16bit) {
    spcmd |= (k_rspi_spcmd_16bit << k_rspi_spcmd_spl_shift);          /* 16-bit data */
    rspi->spdcr = (uint8_t)(s_rspi_bit_set << k_rspi_spdcr_splw_pos); /* Word access mode */
  } else {
    spcmd |= (k_rspi_spcmd_8bit << k_rspi_spcmd_spl_shift); /* 8-bit data */
    rspi->spdcr = k_rspi_spdcr_byte_mode;                   /* Byte access mode */
  }

  rspi->spcmd0 = spcmd;

  /* Configure peripheral mode */
  rspi->spcr = k_rspi_spcr_spe; /* Enable SPI in peripheral mode (MSTR=0) */

  /* Configure pin control (no loopback) */
  rspi->sppcr = s_rspi_sppcr_no_loopback;

  /* Mark channel as initialized */
  s_rspi_channel_initialized[channel] = true;

  rx_log_debug(s_tag, "RSPI peripheral mode initialized");

  return k_rx_ok;
}

/**
 * @brief Perform full-duplex SPI transfer in peripheral mode
 *
 * Executes a full-duplex (simultaneous transmit and receive) SPI transfer
 * on the specified RSPI channel operating in peripheral (slave) mode.
 * Transfers data byte-by-byte with timeout protection on both transmit
 * buffer ready and receive buffer full operations.
 *
 * @param[in]  channel RSPI channel number (0-2)
 * @param[in]  tx_data Pointer to transmit data buffer (not NULL)
 * @param[out] rx_data Pointer to receive data buffer (not NULL)
 * @param[in]  length Number of bytes to transfer (1 to 65535)
 *
 * @return k_rx_ok if transfer completed successfully
 * @return k_rx_err_invalid_arg if:
 *         - @c tx_data or @c rx_data pointer is NULL
 *         - @c length is zero or exceeds maximum (65535 bytes)
 *         - RSPI base address lookup fails for the channel
 * @return k_rx_err_invalid_state if:
 *         - @c channel is out of range (>= 3)
 *         - @c channel is not initialized via rspi_init_peripheral()
 * @return k_rx_err_timeout if:
 *         - Transmit buffer does not become ready within timeout
 *         - Receive buffer does not fill within timeout
 *
 * @pre channel must be initialized via rspi_init_peripheral() before calling
 * @pre tx_data pointer must be non-NULL
 * @pre rx_data pointer must be non-NULL
 * @pre length must be in range [1, 65535]
 *
 * @post If successful, rx_data buffer is filled with received bytes
 *       corresponding to transmitted data at each byte offset
 */
rx_err_t rspi_peripheral_transfer(const uint8_t  channel,
                                  const uint8_t* tx_data,
                                  uint8_t*       rx_data,
                                  const uint16_t length)
{
  uint32_t timeout;

  RX_CHECK_NULL_PTR(tx_data, s_tag, "TX data pointer is NULL");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is NULL");

  if (length == k_rspi_len_zero || length > s_rspi_transfer_len_max) {
    rx_log_error(s_tag, "Invalid transfer length");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    rx_log_error(s_tag, "Failed to get RSPI base address");
    return k_rx_err_invalid_arg;
  }

  for (uint16_t i = 0; i < length; i++) {
    /* Wait for transmit buffer empty */
    timeout = k_rspi_timeout_us;
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

/**
 * @brief Check if data is available in the RSPI receive buffer
 *
 * Polls the RSPI status register to determine if the receive buffer contains
 * data ready for reading. Returns the status without blocking.
 *
 * @param[in] channel RSPI channel number (0, 1, or 2)
 * @param[out] available Pointer to bool flag set to true if data is available, false otherwise
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid or base address retrieval fails
 * @return k_rx_err_invalid_state if channel is not initialized
 *
 * @pre Channel must be initialized via rspi_init_peripheral() before calling this function
 * @pre available must be a valid non-NULL pointer
 */
rx_err_t rspi_peripheral_read_available(const uint8_t channel, bool* available)
{
  RX_CHECK_NULL_PTR(available, s_tag, "Available pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  const volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    *available = false;
    return k_rx_err_invalid_arg;
  }

  /* Check if receive buffer has data */
  *available = (rspi->spsr & k_rspi_spsr_sprf) != 0;

  return k_rx_ok;
}

/**
 * @brief Check if transmit buffer is ready for data
 *
 * Polls the RSPI status register to determine if the transmit buffer is empty
 * and ready to accept new data. Returns the status without blocking.
 *
 * @param[in] channel RSPI channel number (0-2)
 * @param[out] ready Set to true if ready to transmit, false otherwise
 *
 * @return k_rx_ok on success, error code otherwise
 * @return k_rx_err_invalid_arg if channel is invalid or base address retrieval fails
 * @return k_rx_err_invalid_state if channel is not initialized
 *
 * @pre Channel must be initialized via rspi_init_peripheral()
 * @pre ready must be a valid non-NULL pointer
 */
rx_err_t rspi_peripheral_write_ready(const uint8_t channel, bool* ready)
{
  RX_CHECK_NULL_PTR(ready, s_tag, "Ready pointer is NULL");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  const volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == NULL) {
    *ready = false;
    return k_rx_err_invalid_arg;
  }

  /* Check if transmit buffer is empty */
  *ready = (rspi->spsr & k_rspi_spsr_sptef) != 0;

  return k_rx_ok;
}

/**
 * @brief Deinitialize and disable RSPI peripheral
 *
 * Disables the specified RSPI channel, clears the initialization flag, and
 * optionally disables the module (via module stop bit) to reduce power consumption.
 *
 * @param[in] channel RSPI channel number (0, 1, or 2)
 *
 * @return k_rx_ok if deinitialization successful
 * @return k_rx_err_invalid_arg if:
 *         - @c channel is out of range (>= 3)
 *         - RSPI base address lookup fails for the channel
 *
 * @pre Channel should have been initialized via rspi_init_peripheral() previously
 *
 * @post If successful:
 *       - RSPI peripheral is disabled (SPCR.SPE = 0)
 *       - s_rspi_channel_initialized[channel] is set to false
 *       - Channel is no longer available for SPI operations
 */
rx_err_t rspi_deinit(const uint8_t channel)
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
  rspi->spcr = s_rspi_spcr_disabled;

  /* Disable RSPI module (set module stop bit) */
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, false);

  system_regs()->prcr = k_rx_prcr_lock;

  /* Mark channel as uninitialized */
  s_rspi_channel_initialized[channel] = false;

  rx_log_debug(s_tag, "RSPI deinitialized");

  return k_rx_ok;
}
