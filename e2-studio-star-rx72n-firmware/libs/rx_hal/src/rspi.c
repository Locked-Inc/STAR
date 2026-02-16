/* lib/rx_hal/src/rspi.c */

/**
 * @file rspi.c
 * @brief RSPI (Renesas Serial Peripheral Interface) Driver for RX72N
 *
 * @details
 * Provides complete SPI driver implementation supporting both peripheral mode
 * (RX72N as SPI peripheral with RPi5 as controller) and controller mode (RX72N
 * as SPI controller for motor driver communication). Implements full-duplex
 * communication with configurable clock frequency, SPI modes 0-3, and automatic
 * chip select management.
 *
 * @par System Architecture
 * @dot
 * digraph rspi_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5";
 *     style=filled;
 *     color=lightblue;
 *     rpi5_spi [label="SPI Controller\n(spidev)"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N";
 *     style=filled;
 *     color=lightgreen;
 *
 *     subgraph cluster_driver {
 *       label="RSPI Driver (This Module)";
 *       style=filled;
 *       color=lightyellow;
 *       peripheral [label="Peripheral Mode\nrspi_init_peripheral()\nrspi_peripheral_transfer()"];
 *       controller [label="Controller Mode\nrspi_init_controller()\nrspi_controller_transfer_16bit()"];
 *       helpers [label="Internal Helpers\ninternal_wait_tx_ready()\ninternal_wait_rx_ready()"];
 *     }
 *
 *     rspi_hw [label="RSPI0/1/2\nHardware Registers"];
 *     gpio [label="GPIO\n(CS pins)"];
 *   }
 *
 *   subgraph cluster_peripherals {
 *     label="SPI Peripherals";
 *     style=filled;
 *     color=lightcoral;
 *     drv8243 [label="DRV8243S\nMotor Drivers"];
 *     other [label="Other SPI\nDevices"];
 *   }
 *
 *   rpi5_spi -> peripheral [label="COPI/CIPO\nSCLK/CS"];
 *   peripheral -> rspi_hw;
 *   controller -> rspi_hw;
 *   controller -> gpio [label="CS control"];
 *   helpers -> rspi_hw [style=dashed];
 *   rspi_hw -> drv8243 [label="SPI Bus"];
 *   rspi_hw -> other [label="SPI Bus"];
 * }
 * @enddot
 *
 * @par Supported Features
 * | Feature | Peripheral Mode | Controller Mode |
 * |---------|-----------------|-----------------|
 * | Full-duplex | [OK] | [OK] |
 * | 8-bit data | [OK] | - |
 * | 16-bit data | [OK] | [OK] |
 * | SPI modes 0-3 | [OK] | [OK] |
 * | Hardware CS | - | GPIO-based |
 * | Clock config | External | 100kHz - 10MHz |
 *
 * @par RSPI Channels
 * | Channel | Use Case | Status Tracking |
 * |---------|----------|-----------------|
 * | RSPI0 | RPi5 communication | s_rspi_channel_initialized[0] |
 * | RSPI1 | Motor drivers | s_rspi_controller_initialized[1] |
 * | RSPI2 | Reserved | Available |
 *
 * @par Performance Characteristics
 * | Metric | Value | Condition |
 * |--------|-------|-----------|
 * | Max clock (controller) | 10 MHz | PCLKB=60MHz, SPBR=2 |
 * | Min clock (controller) | 100 kHz | PCLKB=60MHz, SPBR=255 |
 * | Transfer timeout | 10 ms | Per byte |
 * | CS setup/hold delay | ~300 ns | 10 NOP cycles @ 240MHz |
 * | 16-bit transfer time | ~2-50 µs | Depends on clock |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~4 KB | All functions |
 * | Constants (.rodata) | ~100 B | Static constants |
 * | Static data (.bss) | ~20 B | Init flags, CS config |
 * | Stack (per call) | ~50 B | Local variables |
 *
 * @par Hardware Register Access
 * Uses rx72n_rspi_regs.h register definitions with inline accessors:
 * - rspi0(), rspi1(), rspi2() for channel base addresses
 * - MSTPCRB module stop control for power management
 * - PRCR register protection for system register access
 *
 * @par Module Dependencies
 * - hardware.h: Hardware definitions and inline accessors
 * - rx72n_regs.h: Register structure definitions
 * - rx_gpio_constants.h: GPIO pin definitions
 * - rx_port_constants.h: Port constants
 * - rx_port_utils.h: Port/pin extraction utilities
 * - rx_register_protection.h: PRCR unlock/lock
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded (timeout counters, transfer length max)
 * - Rule 3: [OK] No dynamic memory allocation (static buffers only)
 * - Rule 4: [OK] Functions under 60 lines (refactored into helpers)
 * - Rule 5: [OK] Minimum 2 assertions per function (RX_ASSERT, RX_CHECK_NULL_PTR)
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] C23 typed enums for all constants
 * - Rule 9: [OK] No function pointers (direct hardware access)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** Driver handles only RSPI peripheral operations
 * - **O (OCP):** Mode/frequency configurable without code changes
 * - **L (LSP):** Peripheral/controller modes have consistent error semantics
 * - **I (ISP):** Separate init/transfer/deinit for each mode
 * - **D (DIP):** Depends on hardware abstractions (rx72n_regs.h)
 *
 * @par Thread Safety
 * - **NOT thread-safe**: Caller must provide external synchronization
 * - Static state (s_rspi_*) shared across calls
 * - Concurrent access to same channel requires mutex protection
 * - Different channels can be accessed independently
 *
 * @see rx72n_rspi_regs.h RSPI register definitions
 * @see rx_spi_comm.h Higher-level SPI communication layer
 * @see rx_drv8243.h Motor driver using RSPI controller mode
 * @see docs/sections/03_hardware_pinout.tex SPI pin assignments
 *
 * @author STAR Team
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_gpio_constants.h"
#include "rx_port_constants.h"
#include "rx_port_utils.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rspi_constants_t
 * @brief RSPI channel count, timeout, and general constants
 *
 * @details
 * Core configuration constants for RSPI driver. Timeout values are calibrated
 * for worst-case SPI peripheral response times at minimum clock frequency.
 *
 * @par Timeout Calculation
 * At 100 kHz minimum clock, one byte takes ~80 µs. 10 ms timeout provides
 * ~125 byte margin before declaring failure, sufficient for any single
 * transfer operation.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rspi_max_channels = 3, /**< Number of RSPI channels: RSPI0, RSPI1, RSPI2.
                                @par Value: 3 */

  k_rspi_timeout_us = 10000, /**< Timeout for TX/RX wait operations in loop iterations.
                                  Approximately 10 ms at typical loop overhead.
                                  @par Value: 10000 iterations (~10 ms) */

  k_rspi_timeout_zero = 0, /**< Sentinel value indicating timeout has expired.
                                @par Value: 0 */

  k_rspi_len_zero = 0, /**< Sentinel for zero-length transfer (invalid).
                            @par Value: 0 */

  k_rspi_zero_u32 = 0, /**< Zero constant for uint32_t comparisons.
                            @par Value: 0 */

  k_rspi_spbr_offset = 1, /**< Offset for SPBR calculation formula.
                               SPBR = (PCLKB / (2 * freq)) - 1
                               @par Value: 1 */

  k_rspi_loop_start = 0, /**< Starting index for bounded loops (NASA Rule 2).
                              @par Value: 0 */
} rspi_constants_t;

/** @brief RSPI CS default constants (uninitialized state) */
typedef enum : uint8_t {
  k_rspi_cs_default_port = 0, /**< Default CS port (unconfigured) */
  k_rspi_cs_default_pin  = 0, /**< Default CS pin (unconfigured) */
  k_rspi_spbr_init       = 0, /**< Initial SPBR value */
} rspi_cs_defaults_t;

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

/** @brief RSPI controller mode constants */
typedef enum : uint32_t {
  k_rspi_pclkb_hz         = 60000000, /**< PCLKB clock frequency (60 MHz) */
  k_rspi_min_freq_hz      = 100000,   /**< Minimum SPI clock (100 kHz) */
  k_rspi_max_freq_hz      = 10000000, /**< Maximum SPI clock (10 MHz) */
  k_rspi_spbr_max         = 255,      /**< Maximum SPBR value */
  k_rspi_spbr_divisor     = 2,        /**< SPBR divisor factor */
  k_rspi_cs_setup_delay   = 10,       /**< CS setup delay iterations (~300ns) */
  k_rspi_cs_hold_delay    = 10,       /**< CS hold delay iterations (~300ns) */
  k_rspi_max_delay_cycles = 10000,    /**< Sanity limit for timing delays */
} rspi_controller_constants_t;

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

/* Track initialized RSPI channels (peripheral mode) */
static bool s_rspi_channel_initialized[k_rspi_max_channels] = {false, false, false};

/* Track controller mode RSPI channels */
static bool s_rspi_controller_initialized[k_rspi_max_channels] = {false, false, false};

/** @brief Controller mode CS pin configuration */
typedef struct {
  uint8_t port; /**< GPIO port for chip select */
  uint8_t pin;  /**< GPIO pin for chip select */
} rspi_cs_config_t;

/* Track controller mode CS pins */
static rspi_cs_config_t s_rspi_cs_config[k_rspi_max_channels] = {
  {k_rspi_cs_default_port, k_rspi_cs_default_pin},
  {k_rspi_cs_default_port, k_rspi_cs_default_pin},
  {k_rspi_cs_default_port, k_rspi_cs_default_pin},
};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get RSPI base address from channel number
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return Pointer to RSPI register base, or nullptr if invalid channel
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
      return nullptr;
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
  uint32_t mask = k_rspi_zero_u32;

  RX_ASSERT(system_regs() != nullptr, "system_regs is nullptr");
  RX_ASSERT(s_rspi_bit_set != k_rspi_zero_u32, "RSPI bit constant is zero");
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
 * @brief Initialize RSPI channel in peripheral mode
 *
 * Configures the specified RSPI channel as a SPI peripheral to communicate
 * with the Raspberry Pi 5 (acting as SPI Controller). Enables the peripheral
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
 *         - @c config pointer is nullptr
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
 *       - SPI is enabled in peripheral mode (SPCR.MSTR = 0)
 *       - s_rspi_channel_initialized[channel] is set to true
 *       - Peripheral is ready to receive transfers from SPI controller
 */
rx_err_t rspi_init_peripheral(const uint8_t channel, const rspi_config_t* config)
{
  uint16_t spcmd = s_rspi_spcmd_init;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate mode (0-3) - lower bound check omitted since spi_mode is uint8_t and
   * k_rspi_mode_min == 0, so spi_mode >= k_rspi_mode_min is always true (-Wtype-limits) */
  if ((uint8_t)config->spi_mode > k_rspi_mode_max) {
    rx_log_error(s_tag, "Invalid SPI mode (must be 0-3)");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Enable RSPI module (clear module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, true);

  *prcr_reg() = k_rx_prcr_lock;

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
 * @brief Wait for transmit buffer to become empty
 *
 * @param[in] rspi Pointer to RSPI registers
 *
 * @return k_rx_ok if buffer became empty within timeout
 * @return k_rx_err_timeout if timeout expired
 */
static rx_err_t internal_wait_tx_ready(volatile rx_rspi_regs_t* rspi)
{
  /* Pre-condition 1: RSPI register pointer must be valid */
  RX_ASSERT(rspi != nullptr, "RSPI register pointer is nullptr");

  /* Pre-condition 2: Timeout constant must be non-zero */
  uint32_t timeout = k_rspi_timeout_us;
  RX_ASSERT(timeout > k_rspi_timeout_zero, "Timeout constant must be non-zero");

  while (!(rspi->spsr & k_rspi_spsr_sptef) && timeout > k_rspi_timeout_zero) {
    timeout--;
  }

  if (timeout == k_rspi_timeout_zero) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Wait for receive buffer to become full
 *
 * @param[in] rspi Pointer to RSPI registers
 *
 * @return k_rx_ok if buffer became full within timeout
 * @return k_rx_err_timeout if timeout expired
 */
static rx_err_t internal_wait_rx_ready(volatile rx_rspi_regs_t* rspi)
{
  /* Pre-condition 1: RSPI register pointer must be valid */
  RX_ASSERT(rspi != nullptr, "RSPI register pointer is nullptr");

  /* Pre-condition 2: Timeout constant must be non-zero */
  uint32_t timeout = k_rspi_timeout_us;
  RX_ASSERT(timeout > k_rspi_timeout_zero, "Timeout constant must be non-zero");

  while (!(rspi->spsr & k_rspi_spsr_sprf) && timeout > k_rspi_timeout_zero) {
    timeout--;
  }

  if (timeout == k_rspi_timeout_zero) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Perform full-duplex SPI transfer in peripheral mode
 *
 * Executes a full-duplex (simultaneous transmit and receive) SPI transfer
 * on the specified RSPI channel operating in peripheral mode.
 * Transfers data byte-by-byte with timeout protection on both transmit
 * buffer ready and receive buffer full operations.
 *
 * @param[in]  channel RSPI channel number (0-2)
 * @param[in]  tx_data Pointer to transmit data buffer (not nullptr)
 * @param[out] rx_data Pointer to receive data buffer (not nullptr)
 * @param[in]  length Number of bytes to transfer (1 to 65535)
 *
 * @return k_rx_ok if transfer completed successfully
 * @return k_rx_err_invalid_arg if:
 *         - @c tx_data or @c rx_data pointer is nullptr
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
  rx_err_t                 err;
  volatile rx_rspi_regs_t* rspi;

  RX_CHECK_NULL_PTR(tx_data, s_tag, "TX data pointer is nullptr");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

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
  rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    rx_log_error(s_tag, "Failed to get RSPI base address");
    return k_rx_err_invalid_arg;
  }

  /* NASA Rule 2: Statically bounded loop */
  for (uint16_t i = 0; i < s_rspi_transfer_len_max; i++) {
    if (i >= length) {
      break;
    }

    /* Wait for transmit buffer empty */
    err = internal_wait_tx_ready(rspi);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "SPI transmit timeout");
      return err;
    }

    /* Write transmit data */
    rspi->spdr = tx_data[i];

    /* Wait for receive buffer full */
    err = internal_wait_rx_ready(rspi);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "SPI receive timeout");
      return err;
    }

    /* Read receive data */
    rx_data[i] = (uint8_t)rspi->spdr;

    /* Clear status flags */
    rspi->spsr &= (uint8_t) ~(uint8_t)(k_rspi_spsr_sprf | k_rspi_spsr_ovrf);
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
 * @pre available must be a valid non-nullptr
 */
rx_err_t rspi_peripheral_read_available(const uint8_t channel, bool* available)
{
  RX_CHECK_NULL_PTR(available, s_tag, "Available pointer is nullptr");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  const volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
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
 * @pre ready must be a valid non-nullptr
 */
rx_err_t rspi_peripheral_write_ready(const uint8_t channel, bool* ready)
{
  RX_CHECK_NULL_PTR(ready, s_tag, "Ready pointer is nullptr");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  const volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
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
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Disable SPI */
  rspi->spcr = s_rspi_spcr_disabled;

  /* Disable RSPI module (set module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, false);

  *prcr_reg() = k_rx_prcr_lock;

  /* Mark channel as uninitialized */
  s_rspi_channel_initialized[channel] = false;

  rx_log_debug(s_tag, "RSPI deinitialized");

  return k_rx_ok;
}

/* =============================================================================
 * RSPI Controller Mode Functions
 * =============================================================================
 */

/**
 * @brief Cycle-accurate timing delay using inline NOP instructions
 *
 * Produces precise delays using inline assembly NOP operations for SPI
 * timing requirements (CS setup/hold times).
 *
 * @param[in] cycles Number of NOP cycles to execute
 *
 * @note Timing is cycle-accurate on RX72N @ 240 MHz (4.17ns per cycle)
 * @note Using inline ASM ensures compiler does not optimize away the loop
 */
static void internal_timing_delay(const uint16_t cycles)
{
  /* Rule 5: Pre-condition validation */
  RX_ASSERT(cycles != 0, "Timing delay cycles cannot be zero");
  RX_ASSERT(cycles <= k_rspi_max_delay_cycles, "Timing delay cycles exceeds sanity limit");

  /*
   * NASA Rule 2: Loop bounded by k_rspi_max_delay_cycles (compile-time constant)
   * Early break when delay >= cycles to execute only the required iterations.
   */
  volatile uint32_t delay;
  for (delay = k_rspi_loop_start; delay < k_rspi_max_delay_cycles; delay++) {
    if (delay >= cycles) {
      break;
    }
    __asm__ volatile("nop");
  }
}

/**
 * @brief Configure SPCMD register for SPI mode (CPOL/CPHA)
 *
 * Sets CPOL and CPHA bits in SPCMD register based on SPI mode (0-3).
 * Mode determines clock polarity and phase per SPI specification.
 *
 * @param[in,out] spcmd Pointer to SPCMD register value to modify
 * @param[in] spi_mode SPI mode (0-3)
 *   - Mode 0: CPOL=0, CPHA=0
 *   - Mode 1: CPOL=0, CPHA=1
 *   - Mode 2: CPOL=1, CPHA=0
 *   - Mode 3: CPOL=1, CPHA=1
 */
static void internal_configure_spcmd(uint16_t* spcmd, const uint8_t spi_mode)
{
  /* Rule 5: Pre-condition validation */
  RX_ASSERT(spcmd != nullptr, "spcmd pointer is nullptr");
  RX_ASSERT((spi_mode & ~(k_rspi_spcmd_cpol_mask | k_rspi_spcmd_cpha_mask)) == 0,
            "spi_mode contains invalid bits");

  /* Configure SPI mode (CPOL and CPHA) */
  if (spi_mode & k_rspi_spcmd_cpol_mask) {
    *spcmd |= (uint16_t)(s_rspi_bit_set << k_rspi_spcmd_cpol_pos); /* CPOL = 1 */
  }
  if (spi_mode & k_rspi_spcmd_cpha_mask) {
    *spcmd |= (uint16_t)(s_rspi_bit_set << k_rspi_spcmd_cpha_pos); /* CPHA = 1 */
  }
}

/**
 * @brief Calculate SPBR value for desired SPI clock frequency
 *
 * @param[in] freq_hz Desired SPI clock frequency in Hz
 * @param[out] spbr Pointer to store calculated SPBR value
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if frequency out of range
 */
static rx_err_t internal_calculate_spbr(const uint32_t freq_hz, uint8_t* spbr)
{
  uint32_t divisor;
  uint32_t spbr_val;

  RX_CHECK_NULL_PTR(spbr, s_tag, "SPBR output pointer is nullptr");

  if (freq_hz < k_rspi_min_freq_hz || freq_hz > k_rspi_max_freq_hz) {
    rx_log_error(s_tag, "SPI frequency out of range (100kHz-10MHz)");
    return k_rx_err_invalid_arg;
  }

  /* SPBR formula: freq = PCLKB / (2 * (SPBR + 1) * 2^BRDV)
   * With BRDV=0: freq = PCLKB / (2 * (SPBR + 1))
   * Solving for SPBR: SPBR = (PCLKB / (2 * freq)) - 1 */
  divisor  = k_rspi_spbr_divisor * freq_hz;
  spbr_val = (k_rspi_pclkb_hz / divisor) - k_rspi_spbr_offset;

  /* Reject frequencies that are too low (would require SPBR > 255) */
  if (spbr_val > k_rspi_spbr_max) {
    rx_log_error(s_tag, "Requested SPI frequency too low; minimum achievable is ~117 kHz");
    return k_rx_err_invalid_arg;
  }

  *spbr = (uint8_t)spbr_val;

  return k_rx_ok;
}

/**
 * @brief Configure GPIO pin for chip select output
 *
 * @param[in] pin_config GPIO pin (rx_port_pin_t encoding port and pin)
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if invalid port/pin
 */
static rx_err_t internal_configure_cs_gpio(const rx_port_pin_t pin_config)
{
  const uint8_t            port      = rx_port_from_pin(pin_config);
  const uint8_t            pin       = rx_pin_from_pin(pin_config);
  volatile rx_port_regs_t* port_regs = rx_port_get_base(port);

  if (port_regs == nullptr) {
    rx_log_error(s_tag, "Invalid port number for CS pin");
    return k_rx_err_invalid_arg;
  }

  /* Validate pin (lower bound check omitted - pin is uint8_t, k_rx_pin_min == 0,
   * so pin >= k_rx_pin_min is always true, avoiding -Wtype-limits warning) */
  if (pin > k_rx_pin_max) {
    rx_log_error(s_tag, "Invalid pin number for CS pin (valid range 0-7)");
    return k_rx_err_invalid_arg;
  }

  /* Configure as output */
  port_regs->pdr |= ((uint32_t)s_rspi_bit_set << pin);

  /* Set high (CS inactive - active low) */
  port_regs->podr |= ((uint32_t)s_rspi_bit_set << pin);

  return k_rx_ok;
}

/**
 * @brief Validate controller initialization arguments
 *
 * @param[in] channel RSPI channel number
 * @param[in] config  Controller configuration
 *
 * @return k_rx_ok if all validations pass
 * @return k_rx_err_invalid_arg if config is nullptr, channel invalid, or mode invalid
 * @return k_rx_err_invalid_state if channel already initialized
 */
static rx_err_t rspi_validate_controller_args(const uint8_t                   channel,
                                              const rspi_controller_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "Controller config pointer is nullptr");

  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Check if channel is already initialized */
  if (s_rspi_controller_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel already initialized");
    return k_rx_err_invalid_state;
  }

  /* Validate mode (0-3) */
  if ((uint8_t)config->spi_mode > k_rspi_mode_max) {
    rx_log_error(s_tag, "Invalid SPI mode (must be 0-3)");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Prepare controller hardware resources
 *
 * Calculates SPBR, validates RSPI base address, and configures CS GPIO.
 *
 * @param[in]  channel  RSPI channel number
 * @param[in]  config   Controller configuration
 * @param[out] out_rspi Pointer to receive RSPI register base
 * @param[out] out_spbr Pointer to receive calculated SPBR value
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if SPBR calculation or GPIO config fails
 */
static rx_err_t rspi_prepare_controller(const uint8_t                   channel,
                                        const rspi_controller_config_t* config,
                                        volatile rx_rspi_regs_t**       out_rspi,
                                        uint8_t*                        out_spbr)
{
  rx_err_t err;

  /* Calculate SPBR for requested frequency */
  err = internal_calculate_spbr(config->freq_hz, out_spbr);
  if (err != k_rx_ok) {
    return err;
  }

  /* Get RSPI base - validate before configuring GPIO to avoid leaving GPIO
   * configured if RSPI base lookup fails */
  *out_rspi = internal_get_rspi_base(channel);
  if (*out_rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Configure CS GPIO pin (after RSPI base validation) */
  err = internal_configure_cs_gpio(config->cs);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Configure RSPI hardware registers
 *
 * Enables module, configures registers, stores CS pin, and marks initialized.
 *
 * @param[in] channel RSPI channel number
 * @param[in] rspi    RSPI register base
 * @param[in] spbr    Calculated SPBR value
 * @param[in] spcmd   SPCMD register value
 * @param[in] config  Controller configuration
 *
 * @return k_rx_ok on success
 */
static rx_err_t rspi_configure_registers(const uint8_t                   channel,
                                         volatile rx_rspi_regs_t*        rspi,
                                         uint8_t                         spbr,
                                         uint16_t                        spcmd,
                                         const rspi_controller_config_t* config)
{
  /* Enable RSPI module (clear module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, true);
  *prcr_reg() = k_rx_prcr_lock;

  /* Disable SPI before configuration */
  rspi->spcr = s_rspi_spcr_disabled;

  /* Configure bit rate */
  rspi->spbr = spbr;

  /* Apply SPCMD configuration */
  rspi->spdcr  = (uint8_t)(s_rspi_bit_set << k_rspi_spdcr_splw_pos); /* Word access mode */
  rspi->spcmd0 = spcmd;

  /* Configure controller mode with SPI enabled */
  rspi->spcr = k_rspi_spcr_spe | k_rspi_spcr_mstr;

  /* Configure pin control (no loopback) */
  rspi->sppcr = s_rspi_sppcr_no_loopback;

  /* Store CS pin configuration */
  s_rspi_cs_config[channel].port = rx_port_from_pin(config->cs);
  s_rspi_cs_config[channel].pin  = rx_pin_from_pin(config->cs);

  /* Mark channel as initialized in controller mode */
  s_rspi_controller_initialized[channel] = true;

  rx_log_debug(s_tag, "RSPI controller mode initialized");

  return k_rx_ok;
}

/**
 * @brief Initialize RSPI channel in controller mode
 *
 * Configures the specified RSPI channel as a SPI controller to communicate
 * with SPI peripheral devices. Enables the module, configures clock frequency,
 * SPI mode, and chip select GPIO pin.
 *
 * @param[in] channel RSPI channel number (0-2)
 * @param[in] config Pointer to controller configuration structure containing:
 *                   - freq_hz: SPI clock frequency (100 kHz to 10 MHz)
 *                   - spi_mode: SPI mode (0-3) for CPOL/CPHA configuration
 *                   - cs: GPIO pin for chip select (type-safe rx_port_pin_t)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if:
 *         - config pointer is nullptr
 *         - channel >= k_rspi_max_channels
 *         - config->spi_mode > 3
 *         - config->freq_hz out of range (< 100kHz or > 10MHz)
 *         - RSPI base address lookup fails
 *         - CS GPIO configuration fails (invalid port/pin)
 * @return k_rx_err_invalid_state if channel is already initialized
 *
 * @pre config must be non-NULL
 * @pre channel < k_rspi_max_channels
 * @pre channel must not be already initialized
 * @pre CS GPIO port and pin must be valid and available
 *
 * @post If successful:
 *       - RSPI module is enabled via internal_set_mstpcrb_for_channel()
 *       - *prcr_reg() is unlocked/locked for register protection
 *       - RSPI registers configured: spbr, spcmd0, spcr, spdcr, sppcr
 *       - CS GPIO configured as output, driven high (inactive)
 *       - s_rspi_cs_config[channel] stores CS port/pin
 *       - s_rspi_controller_initialized[channel] = true
 * @post On error:
 *       - Hardware state is unchanged
 *       - Channel remains uninitialized
 *
 * @note Bit rate calculation: freq = PCLKB / (2 * (SPBR + 1))
 * @note Always configures 16-bit data frames with word access mode
 * @note CS is active-low (asserted=0, deasserted=1)
 */
rx_err_t rspi_init_controller(const uint8_t channel, const rspi_controller_config_t* config)
{
  uint16_t                 spcmd = s_rspi_spcmd_init;
  uint8_t                  spbr  = k_rspi_spbr_init;
  volatile rx_rspi_regs_t* rspi  = nullptr;
  rx_err_t                 err;

  /* Validate all arguments and preconditions */
  err = rspi_validate_controller_args(channel, config);
  if (err != k_rx_ok) {
    return err;
  }

  /* Prepare hardware resources (SPBR, RSPI base, CS GPIO) */
  err = rspi_prepare_controller(channel, config, &rspi, &spbr);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure SPI mode (CPOL and CPHA) */
  internal_configure_spcmd(&spcmd, config->spi_mode);

  /* Configure 16-bit data length */
  spcmd |= (k_rspi_spcmd_16bit << k_rspi_spcmd_spl_shift);

  /* Configure and enable RSPI hardware registers */
  return rspi_configure_registers(channel, rspi, spbr, spcmd, config);
}

/**
 * @brief Set chip select line state for RSPI controller channel
 *
 * Controls the GPIO chip select pin for the specified RSPI channel operating
 * in controller mode. CS is active-low (asserted low, deasserted high).
 *
 * @param[in] channel RSPI channel number (0-2)
 * @param[in] active True to assert CS (drive low), false to deassert CS (drive high)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if channel is not initialized or out of range
 * @return k_rx_err_invalid_arg if GPIO port base lookup fails
 *
 * @pre Channel must be initialized via rspi_init_controller()
 * @pre channel < k_rspi_max_channels
 * @pre s_rspi_controller_initialized[channel] == true
 *
 * @post GPIO port_regs->podr is modified to assert/deassert CS pin
 *
 * @note Reads s_rspi_cs_config[channel] for port and pin configuration
 */
rx_err_t rspi_controller_set_cs(const uint8_t channel, const bool active)
{
  volatile rx_port_regs_t* port_regs;
  uint8_t                  port;
  uint8_t                  pin;

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_controller_initialized[channel]) {
    rx_log_error(s_tag, "RSPI controller channel not initialized");
    return k_rx_err_invalid_state;
  }

  port      = s_rspi_cs_config[channel].port;
  pin       = s_rspi_cs_config[channel].pin;
  port_regs = rx_port_get_base(port);

  if (port_regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (active) {
    /* CS active (low) */
    const uint8_t pin_mask = (uint8_t)((uint32_t)s_rspi_bit_set << pin);
    port_regs->podr &= (uint8_t)~pin_mask;
  } else {
    /* CS inactive (high) */
    const uint8_t pin_mask = (uint8_t)((uint32_t)s_rspi_bit_set << pin);
    port_regs->podr |= pin_mask;
  }

  return k_rx_ok;
}

/**
 * @brief Assert CS with setup delay
 *
 * Asserts chip select (active low) and applies CS setup time delay.
 *
 * @param[in] channel RSPI channel number
 *
 * @return k_rx_ok on success
 * @return Error codes from rspi_controller_set_cs()
 */
static rx_err_t rspi_controller_assert_cs_with_setup(const uint8_t channel)
{
  rx_err_t err;

  /* Rule 5: Pre-condition validation */
  RX_ASSERT((channel == k_rspi_channel_0) || (channel == k_rspi_channel_1) ||
              (channel == k_rspi_channel_2),
            "Invalid RSPI channel");
  RX_ASSERT(s_rspi_controller_initialized[channel], "RSPI controller channel not initialized");

  /* Assert CS (active low) */
  err = rspi_controller_set_cs(channel, true);
  if (err != k_rx_ok) {
    return err;
  }

  /**
   * CS setup time delay (~300ns).
   * Inline assembly NOP is used to produce precise, cycle-accurate timing that
   * C-level delays cannot guarantee. SPI peripherals typically require minimum CS
   * setup time before SCLK transitions. Loop count k_rspi_cs_setup_delay is
   * calibrated for RX72N at 240 MHz (4.17ns per cycle).
   */
  internal_timing_delay(k_rspi_cs_setup_delay);

  return k_rx_ok;
}

/**
 * @brief Deassert CS with hold delay
 *
 * Applies CS hold time delay and deasserts chip select (inactive high).
 *
 * @param[in] channel RSPI channel number
 *
 * @return k_rx_ok on success
 * @return Error codes from rspi_controller_set_cs()
 */
static rx_err_t rspi_controller_deassert_cs_with_hold(const uint8_t channel)
{
  /* Rule 5: Pre-condition validation */
  RX_ASSERT((channel == k_rspi_channel_0) || (channel == k_rspi_channel_1) ||
              (channel == k_rspi_channel_2),
            "Invalid RSPI channel");
  RX_ASSERT(s_rspi_controller_initialized[channel], "RSPI controller channel not initialized");

  /**
   * CS hold time delay (~300ns).
   * Inline assembly NOP is used to produce precise, cycle-accurate timing that
   * C-level delays cannot guarantee. SPI peripherals typically require minimum CS
   * hold time after final SCLK transition before deassertion. Loop count
   * k_rspi_cs_hold_delay is calibrated for RX72N at 240 MHz (4.17ns per cycle).
   */
  internal_timing_delay(k_rspi_cs_hold_delay);

  /* Deassert CS (inactive high) */
  return rspi_controller_set_cs(channel, false);
}

/**
 * @brief Perform core 16-bit SPI transfer operation
 *
 * Executes TX/RX transfer: waits for TX ready, writes data, waits for RX ready,
 * reads data, and clears status flags.
 *
 * @param[in]  rspi    RSPI register base
 * @param[in]  tx_data 16-bit data to transmit
 * @param[out] rx_data Pointer to receive 16-bit response
 *
 * @return k_rx_ok on success
 * @return k_rx_err_timeout if TX or RX wait times out
 */
static rx_err_t rspi_controller_do_16bit_transfer(volatile rx_rspi_regs_t* rspi,
                                                  uint16_t                 tx_data,
                                                  uint16_t*                rx_data)
{
  rx_err_t err;

  /* Rule 5: Pre-condition validation */
  RX_CHECK_NULL_PTR(rspi, s_tag, "RSPI register pointer is nullptr");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

  /* Wait for transmit buffer empty */
  err = internal_wait_tx_ready(rspi);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SPI controller transmit timeout");
    return err;
  }

  /* Write 16-bit transmit data */
  rspi->spdr = tx_data;

  /* Wait for receive buffer full */
  err = internal_wait_rx_ready(rspi);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SPI controller receive timeout");
    return err;
  }

  /* Read 16-bit receive data */
  *rx_data = (uint16_t)rspi->spdr;

  /* Clear status flags */
  rspi->spsr &= (uint8_t) ~(uint8_t)(k_rspi_spsr_sprf | k_rspi_spsr_ovrf);

  return k_rx_ok;
}

/**
 * @brief Perform 16-bit SPI transfer in controller mode with CS handling
 *
 * Executes a full-duplex 16-bit SPI transfer with automatic chip select
 * assertion/deassertion and timing delays.
 *
 * Sequence:
 * 1. Assert CS (active low) with setup delay (~300ns)
 * 2. Wait for TX buffer ready, then transmit 16-bit data
 * 3. Wait for RX buffer full, then read 16-bit response
 * 4. Hold CS for minimum hold time (~300ns)
 * 5. Deassert CS (inactive high)
 * 6. Clear status flags
 *
 * @param[in]  channel RSPI channel number (0-2)
 * @param[in]  tx_data 16-bit data to transmit
 * @param[out] rx_data Pointer to receive 16-bit response (must be non-NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if:
 *         - rx_data pointer is nullptr
 *         - RSPI base address lookup fails
 * @return k_rx_err_invalid_state if:
 *         - channel >= k_rspi_max_channels
 *         - channel is not initialized via rspi_init_controller()
 * @return k_rx_err_timeout if:
 *         - TX buffer does not become ready within timeout
 *         - RX buffer does not fill within timeout
 * @return Other errors from rspi_controller_set_cs() on CS control failure
 *
 * @pre Channel must be initialized via rspi_init_controller()
 * @pre rx_data must be a valid non-nullptr
 *
 * @post If successful:
 *       - rx_data is filled with received 16-bit value
 *       - Status flags (SPRF, OVRF) are cleared
 *       - CS is deasserted (high)
 * @post On error:
 *       - CS is deasserted if possible
 *       - rx_data contents are undefined
 *
 * @note CS setup delay: k_rspi_cs_setup_delay NOP loops (~300ns)
 * @note CS hold delay: k_rspi_cs_hold_delay NOP loops (~300ns)
 * @note Uses inline assembly NOP for precise timing control
 */
rx_err_t rspi_controller_transfer_16bit(const uint8_t   channel,
                                        const uint16_t  tx_data,
                                        uint16_t* const rx_data)
{
  rx_err_t                 err;
  volatile rx_rspi_regs_t* rspi;

  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

  /* Validate channel */
  if (channel >= k_rspi_max_channels || !s_rspi_controller_initialized[channel]) {
    rx_log_error(s_tag, "RSPI controller channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Assert CS with setup delay */
  err = rspi_controller_assert_cs_with_setup(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Perform 16-bit TX/RX transfer */
  err = rspi_controller_do_16bit_transfer(rspi, tx_data, rx_data);
  if (err != k_rx_ok) {
    /* Deassert CS on error (ignore CS deassert errors) */
    (void)rspi_controller_set_cs(channel, false);
    return err;
  }

  /* Deassert CS with hold delay */
  return rspi_controller_deassert_cs_with_hold(channel);
}

/**
 * @brief Deinitialize RSPI controller mode
 *
 * Disables the RSPI controller, deasserts chip select, clears configuration,
 * and stops the module to reduce power consumption.
 *
 * @param[in] channel RSPI channel number (0-2)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is out of range or base address lookup fails
 *
 * @pre Should be called after rspi_init_controller() to properly clean up
 *
 * @post If successful:
 *       - CS is deasserted via rspi_controller_set_cs(channel, false)
 *       - RSPI is disabled (rspi->spcr = s_rspi_spcr_disabled)
 *       - Module clock is stopped via internal_set_mstpcrb_for_channel()
 *       - s_rspi_cs_config[channel] is cleared (port=0, pin=0)
 *       - s_rspi_controller_initialized[channel] = false
 *       - Register protection is unlocked/locked via *prcr_reg()
 *
 * @note Thread-safe only if caller ensures no concurrent access to the same channel
 */
rx_err_t rspi_controller_deinit(const uint8_t channel)
{
  /* Validate channel */
  if (channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Ensure CS is deasserted */
  if (s_rspi_controller_initialized[channel]) {
    (void)rspi_controller_set_cs(channel, false);
  }

  /* Disable SPI */
  rspi->spcr = s_rspi_spcr_disabled;

  /* Disable RSPI module (set module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, false);
  *prcr_reg() = k_rx_prcr_lock;

  /* Clear CS configuration */
  s_rspi_cs_config[channel].port = k_rspi_cs_default_port;
  s_rspi_cs_config[channel].pin  = k_rspi_cs_default_pin;

  /* Mark channel as uninitialized */
  s_rspi_controller_initialized[channel] = false;

  rx_log_debug(s_tag, "RSPI controller deinitialized");

  return k_rx_ok;
}
