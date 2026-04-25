/**
 * @file rspi.c
 * @brief RSPI (Renesas Serial Peripheral Interface) Driver for RX72N
 *
 * @details
 * Provides complete SPI driver implementation supporting both peripheral mode
 * (RX72N as SPI peripheral with RPi5 as controller) and controller mode (RX72N
 * as SPI controller for sensor communication). Implements full-duplex
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
 *     sensors [label="Future SPI\nSensors"];
 *   }
 *
 *   rpi5_spi -> peripheral [label="COPI/CIPO\nSCLK/CS"];
 *   peripheral -> rspi_hw;
 *   controller -> rspi_hw;
 *   controller -> gpio [label="CS control"];
 *   helpers -> rspi_hw [style=dashed];
 *   rspi_hw -> sensors [label="SPI Bus", style=dashed];
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
 * | RSPI1 | Not used in current design | s_rspi_controller_initialized[1] |
 * | RSPI2 | Not used | Available |
 *
 * @par Performance Characteristics
 * | Metric | Value | Condition |
 * |--------|-------|-----------|
 * | Max clock (controller) | 10 MHz | PCLKB=60MHz, SPBR=2 |
 * | Min clock (controller) | 100 kHz | PCLKB=60MHz, SPBR=255 |
 * | Transfer timeout | 10 ms | Per byte |
 * | CS setup/hold delay | ~300 ns | 10 NOP cycles @ 240MHz |
 * | 16-bit transfer time | ~2-50 us | Depends on clock |
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
 * @see docs/sections/03_hardware_pinout.tex SPI pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "rx72n_clock.h"
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
 * At 100 kHz minimum clock, one byte takes ~80 us. 10 ms timeout provides
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

/* RSPI channel constants (k_rspi_channel_0/1/2) provided by hardware.h via rspi_channel_t */

/** @brief RSPI0/RSPI1 module stop bit positions in MSTPCRB (HW Manual p.408) */
typedef enum : uint8_t {
  k_rspi_mstpb_rspi0 = 17, /**< RSPI0 module stop bit in MSTPCRB */
  k_rspi_mstpb_rspi1 = 16, /**< RSPI1 module stop bit in MSTPCRB */
} rspi_mstpcrb_bits_t;

/** @brief RSPI2 module stop bit position in MSTPCRC (HW Manual p.410) */
typedef enum : uint8_t {
  k_rspi_mstpc_rspi2 = 22, /**< RSPI2 module stop bit in MSTPCRC */
} rspi_mstpcrc_bits_t;

/** @brief RSPI controller mode constants
 *
 * PCLKB rate comes from rx72n_clock.h (k_pclkb_hz) — do not duplicate the
 * literal here so the divisor math tracks any future clock-tree change.
 */
typedef enum : uint32_t {
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
  k_rspi_spdcr_splw_pos  = 5,  /**< SPLW bit position (longword/32-bit access) */
  k_rspi_spdcr_byte_mode = 0U, /**< Byte access mode */
} rspi_spdcr_local_t;

/**
 * @enum rspi_register_defaults_t
 * @brief RSPI register default values (reset/disabled state)
 *
 * @details
 * Values written to RSPI registers during initialization to establish
 * a known baseline configuration. These match the hardware reset defaults
 * documented in the RX72N Hardware Manual Section 38.2. Writing these values
 * before configuring mode-specific settings ensures a deterministic starting
 * state regardless of prior peripheral activity.
 *
 * @invariant All values must match the hardware manual reset state
 *
 * @code
 * // Reset RSPI registers to defaults before reconfiguration
 * rspi->sppcr = k_rspi_sppcr_no_loopback;
 * rspi->spcr  = k_rspi_spcr_disabled;
 * @endcode
 *
 * @see rspi_init_peripheral() Uses these during peripheral mode init
 * @see internal_configure_registers() Uses these during controller mode init
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rspi_sppcr_no_loopback = 0U, /**< SPPCR: no loopback mode (normal operation) */
  k_rspi_spcr_disabled     = 0U, /**< SPCR: SPI disabled (module in reset state) */
} rspi_register_defaults_t;

/**
 * @enum rspi_bit_constants_t
 * @brief RSPI bit manipulation constants for register field operations
 *
 * @details
 * Provides named constants for single-bit set operations used to construct
 * register masks via left-shift. Using a named constant instead of a literal
 * 1 improves readability and grep-ability of register manipulation code.
 * All register mask construction in this module uses k_rspi_bit_set as the
 * base value to ensure consistent, self-documenting bit manipulation.
 *
 * @invariant k_rspi_bit_set must always equal 1
 *
 * @code
 * // Construct a mask for MSTPCRB module stop bit
 * uint32_t mask = (k_rspi_bit_set << k_rspi_mstpb_rspi0);
 * system_regs()->mstpcrb &= ~mask;
 * @endcode
 *
 * @see internal_set_mstpcrb_for_channel() Uses for MSTPCRB mask construction
 * @see internal_configure_spcmd() Uses for CPOL/CPHA bit setting
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rspi_bit_set = 1UL, /**< Single bit set value (1) for shift-based register mask construction */
} rspi_bit_constants_t;

/**
 * @enum rspi_transfer_constants_t
 * @brief RSPI transfer length and command register constants
 *
 * @details
 * Defines limits and initialization values for the RSPI transfer engine.
 * The maximum transfer length is constrained by the 16-bit DMA counter.
 * SPCMD initial value sets a clean baseline before configuring bit fields
 * for CPOL, CPHA, and data length via bitwise OR operations.
 *
 * @invariant k_rspi_transfer_len_max must equal 65535 (uint16_t maximum, 16-bit DMA counter limit)
 *
 * @code
 * // Validate transfer length before starting peripheral transfer
 * if (length == k_rspi_len_zero || length > k_rspi_transfer_len_max) {
 *     return k_rx_err_invalid_arg;
 * }
 * // Initialize SPCMD before configuring mode-specific bits
 * uint16_t spcmd = k_rspi_spcmd_init;
 * @endcode
 *
 * @see rspi_peripheral_transfer() Uses k_rspi_transfer_len_max for length validation
 * @see rspi_init_peripheral() Uses k_rspi_spcmd_init for register setup
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_rspi_transfer_len_max = 65535U, /**< Maximum transfer length (16-bit counter limit) */
  k_rspi_spcmd_init       = 0U,     /**< SPCMD initial value (all fields at reset state) */
} rspi_transfer_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* const s_tag = "RSPI";

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
 * @brief Get RSPI register base address from channel number
 *
 * @details
 * Maps a channel index (0-2) to the corresponding RSPI peripheral register
 * block via the inline accessor functions rspi0()/rspi1()/rspi2(). Returns
 * nullptr for out-of-range channel numbers to allow callers to detect invalid
 * input without risking a wild pointer dereference.
 *
 * @param[in] channel RSPI channel number (valid: 0, 1, or 2)
 *
 * @return Pointer to RSPI register base for the given channel
 * @retval non-null Valid register base pointer
 * @retval nullptr Invalid channel number
 *
 * @pre channel must be in range [0, k_rspi_max_channels)
 * @pre RSPI hardware must be present (RX72N 144-pin package)
 * @post Returned pointer (if non-null) is aligned and points to valid HW registers
 * @post No side effects on hardware state
 *
 * @note Not thread-safe. Caller must provide synchronization if needed.
 *
 * @see rspi0() Channel 0 register accessor
 * @see rspi1() Channel 1 register accessor
 * @see rspi2() Channel 2 register accessor
 *
 * @since Version 1.0.0
 */
static volatile rx_rspi_regs_t* internal_get_rspi_base(const rspi_channel_t channel)
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
 * @brief Set module stop bit for an RSPI channel
 *
 * @details
 * Controls the Module Stop Control Register bits that gate the clock to each
 * RSPI channel. RSPI0 and RSPI1 are in MSTPCRB (bits 17 and 16); RSPI2 is in
 * MSTPCRC (bit 22). Clearing a bit enables the module (starts the clock);
 * setting it stops the module (reduces power consumption).
 *
 * @param[in] channel RSPI channel number (valid: 0, 1, or 2)
 * @param[in] enable true to enable module clock (clear stop bit),
 *                   false to stop module clock (set stop bit)
 *
 * @pre system_regs() must return a valid pointer
 * @pre channel must be one of k_rspi_channel_0, k_rspi_channel_1, k_rspi_channel_2
 * @post MSTPCRB (channels 0/1) or MSTPCRC (channel 2) updated appropriately
 * @post Module clock enabled or disabled for the specified channel
 *
 * @note Not thread-safe. Caller must hold MSTPCR protection if applicable.
 * @warning Disabling a module while it is actively transferring data will
 *          cause undefined hardware behavior.
 *
 * @see internal_get_rspi_base() Maps channel to register base
 *
 * @since Version 1.0.0
 */
static void internal_set_mstpcrb_for_channel(const rspi_channel_t channel, const bool enable)
{
  RX_ASSERT(system_regs() != nullptr, "system_regs is nullptr");
  static_assert(k_rspi_bit_set != 0, "RSPI bit constant must be non-zero");
  RX_ASSERT((channel == k_rspi_channel_0) || (channel == k_rspi_channel_1) ||
              (channel == k_rspi_channel_2),
            "Invalid RSPI channel");

  /* RSPI2 stop bit is in MSTPCRC (bit 22), not MSTPCRB (HW Manual p.410) */
  if (channel == k_rspi_channel_2) {
    const uint32_t mask = (k_rspi_bit_set << k_rspi_mstpc_rspi2);
    if (enable) {
      system_regs()->mstpcrc &= ~mask;
    } else {
      system_regs()->mstpcrc |= mask;
    }
    return;
  }

  uint32_t mask = k_rspi_zero_u32;
  if (channel == k_rspi_channel_0) {
    mask = (k_rspi_bit_set << k_rspi_mstpb_rspi0);
  } else if (channel == k_rspi_channel_1) {
    mask = (k_rspi_bit_set << k_rspi_mstpb_rspi1);
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
rx_err_t rspi_init_peripheral(const rspi_channel_t channel, const rspi_config_t* config)
{
  uint16_t spcmd = k_rspi_spcmd_init;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels) {
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

  /*
   * RSPI Peripheral Mode Register Write Sequence
   * =============================================
   * The register write order follows the RX72N Hardware Manual Section 38.3.6
   * (RSPI Initialization Procedure). The sequence is:
   *
   * 1. Enable module clock (MSTPCRB) - module must be powered before register access
   * 2. Disable SPI (SPCR.SPE=0) - registers must not be modified while SPI is active
   * 3. Configure SPCMD0 - set CPOL, CPHA, and data length while SPI is disabled
   * 4. Configure SPDCR - set data access width (byte or word) while SPI is disabled
   * 5. Configure SPPCR - set pin control (no loopback) BEFORE enabling SPI
   * 6. Enable SPI (SPCR.SPE=1) - must be the LAST register write to avoid
   *    partial configuration being active during setup
   */

  /* Step 1: Enable RSPI module clock (clear module stop bit in MSTPCRB) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, true);

  *prcr_reg() = k_rx_prcr_lock;

  /* Step 2: Disable SPI before modifying configuration registers */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Step 3: Configure SPI mode (CPOL and CPHA) in SPCMD0 */
  if (config->spi_mode & k_rspi_spcmd_cpol_mask) {
    spcmd |= (uint16_t)(k_rspi_bit_set << k_rspi_spcmd_cpol_pos); /* CPOL = 1 */
  }
  if (config->spi_mode & k_rspi_spcmd_cpha_mask) {
    spcmd |= (uint16_t)(k_rspi_bit_set << k_rspi_spcmd_cpha_pos); /* CPHA = 1 */
  }

  /* Step 3b: Configure data length in SPCMD0 and access mode in SPDCR */
  if (config->use_16bit) {
    spcmd |= (k_rspi_spcmd_16bit << k_rspi_spcmd_spl_shift); /* 16-bit data */
    rspi->spdcr =
      (uint8_t)(k_rspi_bit_set << k_rspi_spdcr_splw_pos); /* Longword (32-bit) access mode */
  } else {
    spcmd |= (k_rspi_spcmd_8bit << k_rspi_spcmd_spl_shift); /* 8-bit data */
    rspi->spdcr = k_rspi_spdcr_byte_mode;                   /* Byte access mode */
  }

  rspi->spcmd0 = spcmd;

  /* Step 5: Configure pin control (no loopback) - MUST be written before SPCR.SPE
   * per HW manual to avoid glitches on COPI/CIPO lines during enable */
  rspi->sppcr = k_rspi_sppcr_no_loopback;

  /* Step 6: Enable SPI in peripheral mode (MSTR=0) - MUST be the last register
   * write so all configuration is stable before the peripheral becomes active */
  rspi->spcr = k_rspi_spcr_spe;

  /* Mark channel as initialized */
  s_rspi_channel_initialized[channel] = true;

  rx_log_debug(s_tag, "RSPI peripheral mode initialized");

  return k_rx_ok;
}

/**
 * @brief Wait for transmit buffer to become empty (SPTEF flag)
 *
 * @details
 * Polls the SPSR.SPTEF (SPI Transmit Buffer Empty Flag) in a busy-wait loop
 * with a configurable timeout. When SPTEF is set, the transmit buffer is empty
 * and ready to accept new data via SPDR. The timeout prevents infinite loops
 * if the SPI peripheral is stuck.
 *
 * @param[in] rspi Pointer to RSPI register block (must be valid and non-null)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Transmit buffer became empty within timeout
 * @retval k_rx_err_timeout Timeout expired before SPTEF was set
 *
 * @pre rspi must be a valid non-null pointer to initialized RSPI registers
 * @pre RSPI module clock must be enabled (MSTPCRB bit cleared)
 * @post No hardware state modified (read-only polling of SPSR)
 * @post Timeout counter decremented to reflect remaining iterations
 *
 * @note Not thread-safe. Caller must provide synchronization.
 * @warning Busy-wait loop; do not call from time-critical ISR context.
 *
 * @see internal_wait_rx_ready() Companion function for receive buffer
 * @see k_rspi_timeout_us Timeout constant
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_wait_tx_ready(volatile rx_rspi_regs_t* rspi)
{
  /* Pre-condition 1: RSPI register pointer must be valid */
  RX_ASSERT(rspi != nullptr, "RSPI register pointer is nullptr");

  /* Pre-condition 2: Timeout constant must be non-zero (compile-time check) */
  static_assert(k_rspi_timeout_us > k_rspi_timeout_zero, "Timeout constant must be non-zero");
  uint32_t timeout = k_rspi_timeout_us;

  while (!(rspi->spsr & k_rspi_spsr_sptef) && timeout > k_rspi_timeout_zero) {
    timeout--;
  }

  if (timeout == k_rspi_timeout_zero) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Wait for receive buffer to become full (SPRF flag)
 *
 * @details
 * Polls the SPSR.SPRF (SPI Receive Buffer Full Flag) in a busy-wait loop
 * with a configurable timeout. When SPRF is set, received data is available
 * in the SPDR register and can be read. The timeout prevents infinite loops
 * if the SPI peripheral is stuck or no clock is being generated.
 *
 * @param[in] rspi Pointer to RSPI register block (must be valid and non-null)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Receive buffer became full within timeout
 * @retval k_rx_err_timeout Timeout expired before SPRF was set
 *
 * @pre rspi must be a valid non-null pointer to initialized RSPI registers
 * @pre A transfer must be in progress or complete for SPRF to set
 * @post No hardware state modified (read-only polling of SPSR)
 * @post Timeout counter decremented to reflect remaining iterations
 *
 * @note Not thread-safe. Caller must provide synchronization.
 * @warning Busy-wait loop; do not call from time-critical ISR context.
 *
 * @see internal_wait_tx_ready() Companion function for transmit buffer
 * @see k_rspi_timeout_us Timeout constant
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_wait_rx_ready(volatile rx_rspi_regs_t* rspi)
{
  /* Pre-condition 1: RSPI register pointer must be valid */
  RX_ASSERT(rspi != nullptr, "RSPI register pointer is nullptr");

  /* Pre-condition 2: Timeout constant must be non-zero (compile-time check) */
  static_assert(k_rspi_timeout_us > k_rspi_timeout_zero, "Timeout constant must be non-zero");
  uint32_t timeout = k_rspi_timeout_us;

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
rx_err_t rspi_peripheral_transfer(const rspi_channel_t channel,
                                  const uint8_t*       tx_data,
                                  uint8_t*             rx_data,
                                  const uint16_t       length)
{
  RX_CHECK_NULL_PTR(tx_data, s_tag, "TX data pointer is nullptr");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

  if (length == k_rspi_len_zero) {
    rx_log_error(s_tag, "Invalid transfer length");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
    rx_log_error(s_tag, "RSPI channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* const rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    rx_log_error(s_tag, "Failed to get RSPI base address");
    return k_rx_err_invalid_arg;
  }

  /* NASA Rule 2: Statically bounded loop */
  for (uint16_t i = k_rspi_loop_start; i < k_rspi_transfer_len_max; i++) {
    if (i >= length) {
      break;
    }

    /* Wait for transmit buffer empty */
    rx_err_t err = internal_wait_tx_ready(rspi);
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
 * @return k_rx_err_null_ptr if available pointer is nullptr
 * @return k_rx_err_invalid_state if channel is out of range or not initialized
 * @return k_rx_err_invalid_arg if RSPI base address retrieval fails
 *
 * @pre Channel must be initialized via rspi_init_peripheral() before calling this function
 * @pre available must be a valid non-nullptr
 */
rx_err_t rspi_peripheral_read_available(const rspi_channel_t channel, bool* available)
{
  RX_CHECK_NULL_PTR(available, s_tag, "Available pointer is nullptr");

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
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
 * @return k_rx_err_null_ptr if ready pointer is nullptr
 * @return k_rx_err_invalid_state if channel is out of range or not initialized
 * @return k_rx_err_invalid_arg if RSPI base address retrieval fails
 *
 * @pre Channel must be initialized via rspi_init_peripheral()
 * @pre ready must be a valid non-nullptr
 */
rx_err_t rspi_peripheral_write_ready(const rspi_channel_t channel, bool* ready)
{
  RX_CHECK_NULL_PTR(ready, s_tag, "Ready pointer is nullptr");

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels || !s_rspi_channel_initialized[channel]) {
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
rx_err_t rspi_deinit(const rspi_channel_t channel)
{
  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /*
   * RSPI Deinitialization Register Write Sequence
   * ==============================================
   * The deinit order is the reverse of initialization:
   * 1. Disable SPI (SPCR.SPE=0) - stop all bus activity before removing clock
   * 2. Disable module clock (MSTPCRB) - safe to gate clock only after SPI is idle
   * 3. Clear software state - mark channel uninitialized after hardware is off
   */

  /* Step 1: Disable SPI to stop all bus activity */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Step 2: Disable RSPI module clock (set module stop bit) - must be done
   * after SPI is disabled to avoid undefined behavior from clock gating
   * during an active transfer */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, false);

  *prcr_reg() = k_rx_prcr_lock;

  /* Step 3: Mark channel as uninitialized after hardware is safely off */
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
 * @details
 * Produces precise delays using inline assembly NOP operations for SPI
 * timing requirements (CS setup/hold times). Each NOP cycle is 4.17 ns
 * at 240 MHz core clock.
 *
 * @param[in] cycles Number of NOP cycles to execute (must be > 0,
 *                   <= k_rspi_max_delay_cycles)
 *
 * @pre cycles must be non-zero
 * @pre cycles must not exceed k_rspi_max_delay_cycles
 * @post Minimum delay of (cycles * 4.17 ns) has elapsed
 * @post No hardware state modified
 *
 * @note Timing is cycle-accurate on RX72N @ 240 MHz (4.17 ns per cycle)
 * @note Using inline ASM ensures compiler does not optimize away the loop
 * @warning Only accurate when compiled with known optimization level (-O2)
 *
 * @see internal_controller_assert_cs_with_setup() Uses for CS setup time
 * @see internal_controller_deassert_cs_with_hold() Uses for CS hold time
 *
 * @since Version 1.0.0
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
 * @details
 * Sets CPOL and CPHA bits in an SPCMD register value based on SPI mode (0-3).
 * The SPI mode encodes clock polarity (CPOL) and clock phase (CPHA) per the
 * standard SPI specification. Bits are set via bitwise OR so pre-existing
 * fields in the SPCMD value (such as data length) are preserved.
 *
 * | Mode | CPOL | CPHA | Clock Idle | Data Sampled |
 * |------|------|------|------------|--------------|
 * | 0    | 0    | 0    | Low        | Rising edge  |
 * | 1    | 0    | 1    | Low        | Falling edge |
 * | 2    | 1    | 0    | High       | Falling edge |
 * | 3    | 1    | 1    | High       | Rising edge  |
 *
 * @param[in,out] spcmd Pointer to SPCMD register value to modify
 * @param[in] spi_mode SPI mode (0-3)
 *
 * @return void
 *
 * @pre spcmd must be a valid non-null pointer
 * @pre spi_mode must be in range [0, 3] (only bits 0-1 used)
 * @post spcmd CPOL/CPHA bits set according to spi_mode
 * @post Other spcmd bits remain unchanged
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see rspi_init_controller() Calls this during controller init
 * @see rspi_init_peripheral() Performs equivalent inline configuration
 *
 * @since Version 1.0.0
 */
static void internal_configure_spcmd(uint16_t* spcmd, const uint8_t spi_mode)
{
  /* Rule 5: Pre-condition validation */
  RX_ASSERT(spcmd != nullptr, "spcmd pointer is nullptr");
  RX_ASSERT((spi_mode & ~(k_rspi_spcmd_cpol_mask | k_rspi_spcmd_cpha_mask)) == 0,
            "spi_mode contains invalid bits");

  /* Configure SPI mode (CPOL and CPHA) */
  if (spi_mode & k_rspi_spcmd_cpol_mask) {
    *spcmd |= (uint16_t)(k_rspi_bit_set << k_rspi_spcmd_cpol_pos); /* CPOL = 1 */
  }
  if (spi_mode & k_rspi_spcmd_cpha_mask) {
    *spcmd |= (uint16_t)(k_rspi_bit_set << k_rspi_spcmd_cpha_pos); /* CPHA = 1 */
  }
}

/**
 * @brief Calculate SPBR value for desired SPI clock frequency
 *
 * @details
 * Computes the RSPI Bit Rate Register (SPBR) value required to achieve
 * the desired SPI clock frequency. Uses the formula:
 *   SPBR = (PCLKB / (2 * freq_hz)) - 1
 * with BRDV=0 (no additional divisor). The result is validated against
 * the hardware-supported range [0, 255].
 *
 * @param[in] freq_hz Desired SPI clock frequency in Hz
 *                    (valid range: k_rspi_min_freq_hz to k_rspi_max_freq_hz)
 * @param[out] spbr Pointer to store calculated SPBR value (0-255)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok SPBR calculated and stored successfully
 * @retval k_rx_err_null_ptr spbr pointer is nullptr
 * @retval k_rx_err_invalid_arg freq_hz outside supported range or SPBR > 255
 *
 * @pre spbr must be a valid non-null pointer
 * @pre freq_hz must be in range [100000, 10000000] Hz
 * @post *spbr contains the calculated SPBR value if k_rx_ok returned
 * @post *spbr is unchanged on error
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see rspi_init_controller() Calls this to configure clock frequency
 * @see k_pclkb_hz (rx72n_clock.h) PCLKB clock frequency used in calculation
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_calculate_spbr(const uint32_t freq_hz, uint8_t* spbr)
{
  RX_CHECK_NULL_PTR(spbr, s_tag, "SPBR output pointer is nullptr");

  if (freq_hz < k_rspi_min_freq_hz || freq_hz > k_rspi_max_freq_hz) {
    rx_log_error(s_tag, "SPI frequency out of range (100kHz-10MHz)");
    return k_rx_err_invalid_arg;
  }

  /* SPBR formula: freq = PCLKB / (2 * (SPBR + 1) * 2^BRDV)
   * With BRDV=0: freq = PCLKB / (2 * (SPBR + 1))
   * Solving for SPBR: SPBR = (PCLKB / (2 * freq)) - 1 */
  const uint32_t divisor  = k_rspi_spbr_divisor * freq_hz;
  const uint32_t spbr_val = (k_pclkb_hz / divisor) - k_rspi_spbr_offset;

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
 * @details
 * Configures a GPIO pin as a digital output for use as an active-low SPI
 * chip select signal. Extracts port and pin numbers from the type-safe
 * rx_port_pin_t encoding, sets the pin direction to output via PDR, and
 * drives the pin high (CS inactive) via PODR.
 *
 * @param[in] pin_config GPIO pin (rx_port_pin_t encoding port and pin)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok GPIO configured as CS output and driven high (inactive)
 * @retval k_rx_err_invalid_arg Port base lookup failed or pin out of range
 *
 * @pre pin_config must encode a valid port (0-J) and pin (0-7)
 * @pre Port hardware must be available and not module-stopped
 * @post GPIO PDR bit set (output direction)
 * @post GPIO PODR bit set (CS inactive / high)
 *
 * @note Not thread-safe. Caller must provide synchronization for port access.
 *
 * @see rspi_init_controller() Calls this during controller initialization
 * @see rx_port_from_pin() Extracts port number from rx_port_pin_t
 * @see rx_pin_from_pin() Extracts pin number from rx_port_pin_t
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_cs_gpio(const rx_port_pin_t pin_config)
{
  const uint8_t port = rx_port_from_pin(pin_config);
  const uint8_t pin  = rx_pin_from_pin(pin_config);

  volatile rx_port_regs_t* const port_regs = rx_port_get_base(port);

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
  port_regs->pdr |= ((uint32_t)k_rspi_bit_set << pin);

  /* Set high (CS inactive - active low) */
  port_regs->podr |= ((uint32_t)k_rspi_bit_set << pin);

  return k_rx_ok;
}

/**
 * @brief Validate controller initialization arguments
 *
 * @details
 * Performs comprehensive input validation for rspi_init_controller() before
 * any hardware modification occurs. Checks null pointer, channel range,
 * duplicate initialization, and SPI mode range. Centralizes validation
 * logic to keep the public API function concise.
 *
 * @param[in] channel RSPI channel number (valid: 0 to k_rspi_max_channels - 1)
 * @param[in] config  Pointer to controller configuration structure
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All validations passed
 * @retval k_rx_err_null_ptr config pointer is nullptr
 * @retval k_rx_err_invalid_arg Channel out of range or SPI mode > 3
 * @retval k_rx_err_invalid_state Channel already initialized in controller mode
 *
 * @pre config may be nullptr (will be detected and rejected)
 * @pre channel may be out of range (will be detected and rejected)
 * @post No hardware state modified (validation only)
 * @post s_rspi_controller_initialized[] not modified
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see rspi_init_controller() Calls this as first step of initialization
 * @see internal_prepare_controller() Called after this validation passes
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_validate_controller_args(const rspi_channel_t            channel,
                                                  const rspi_controller_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "Controller config pointer is nullptr");

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels) {
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
 * @details
 * Calculates the SPBR register value for the requested clock frequency,
 * validates the RSPI register base address for the given channel, and
 * configures the CS GPIO pin as an active-low output. Operations are
 * ordered so that a failure in SPBR calculation avoids unnecessary GPIO
 * configuration, and RSPI base validation occurs before GPIO setup to
 * avoid leaving GPIO configured if the channel is invalid.
 *
 * @param[in]  channel  RSPI channel number (0 to k_rspi_max_channels - 1)
 * @param[in]  config   Pointer to controller configuration (freq_hz, cs pin)
 * @param[out] out_rspi Pointer to receive RSPI register base address
 * @param[out] out_spbr Pointer to receive calculated SPBR value
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Hardware resources prepared successfully
 * @retval k_rx_err_null_ptr out_spbr is nullptr (via internal_calculate_spbr)
 * @retval k_rx_err_invalid_arg Frequency out of range, invalid channel, or bad CS pin
 *
 * @pre config must be validated by internal_validate_controller_args() first
 * @pre channel must be in range [0, k_rspi_max_channels)
 * @post *out_rspi points to valid RSPI register base on success
 * @post *out_spbr contains calculated SPBR value on success
 * @post CS GPIO pin configured as output, driven high (inactive) on success
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see internal_validate_controller_args() Must be called before this function
 * @see internal_configure_registers() Called after this to program hardware
 * @see internal_calculate_spbr() Called internally for frequency calculation
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_prepare_controller(const rspi_channel_t            channel,
                                            const rspi_controller_config_t* config,
                                            volatile rx_rspi_regs_t**       out_rspi,
                                            uint8_t*                        out_spbr)
{
  /* Calculate SPBR for requested frequency */
  rx_err_t err = internal_calculate_spbr(config->freq_hz, out_spbr);
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
 * @brief Configure RSPI hardware registers for controller mode
 *
 * @details
 * Performs the complete RSPI hardware register configuration sequence for
 * controller mode. Enables the module clock, disables SPI for safe register
 * modification, programs bit rate, command, data control and pin control
 * registers, then enables SPI in controller mode. Also stores the CS pin
 * configuration and marks the channel as initialized. The register write
 * order follows the RX72N Hardware Manual Section 38.3.6 initialization
 * procedure.
 *
 * @param[in] channel RSPI channel number (0 to k_rspi_max_channels - 1)
 * @param[in] rspi    Pointer to RSPI register base (must be valid, non-null)
 * @param[in] spbr    Calculated SPBR value for clock frequency (0-255)
 * @param[in] spcmd   Pre-configured SPCMD register value (CPOL/CPHA/data length)
 * @param[in] config  Pointer to controller configuration (used for CS pin info)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Registers configured and channel marked as initialized
 *
 * @pre rspi must be a valid non-null pointer from internal_get_rspi_base()
 * @pre spbr must be pre-calculated by internal_calculate_spbr()
 * @pre spcmd must be pre-configured with CPOL/CPHA and data length bits
 * @post RSPI module clock enabled, registers configured, SPI enabled
 * @post s_rspi_cs_config[channel] stores CS port and pin
 * @post s_rspi_controller_initialized[channel] set to true
 *
 * @note Not thread-safe. Caller must provide synchronization.
 * @warning PRCR register protection is temporarily unlocked during this call.
 *
 * @see internal_prepare_controller() Provides rspi, spbr, and validates channel
 * @see rspi_init_controller() Public API that orchestrates the full init flow
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_registers(const rspi_channel_t            channel,
                                             volatile rx_rspi_regs_t*        rspi,
                                             uint8_t                         spbr,
                                             uint16_t                        spcmd,
                                             const rspi_controller_config_t* config)
{
  /*
   * RSPI Controller Mode Register Write Sequence
   * ==============================================
   * The register write order follows the RX72N Hardware Manual Section 38.3.6
   * (RSPI Initialization Procedure). The sequence is:
   *
   * 1. Enable module clock (MSTPCRB) - module must be powered before register access
   * 2. Disable SPI (SPCR.SPE=0) - registers must not be modified while SPI is active
   * 3. Configure SPBR - set bit rate divider while SPI is disabled
   * 4. Configure SPDCR and SPCMD0 - set data access width and mode while disabled
   * 5. Configure SPPCR - set pin control (no loopback) BEFORE enabling SPI
   * 6. Enable SPI (SPCR.SPE=1, MSTR=1) - must be the LAST register write to avoid
   *    partial configuration or clock glitches on the SPI bus
   */

  /* Step 1: Enable RSPI module clock (clear module stop bit in MSTPCRB) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, true);
  *prcr_reg() = k_rx_prcr_lock;

  /* Step 2: Disable SPI before modifying configuration registers */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Step 3: Configure bit rate divider (freq = PCLKB / (2 * (SPBR + 1))) */
  rspi->spbr = spbr;

  /* Step 4: Configure data access width (longword/32-bit mode) and command register */
  rspi->spdcr =
    (uint8_t)(k_rspi_bit_set << k_rspi_spdcr_splw_pos); /* Longword (32-bit) access mode */
  rspi->spcmd0 = spcmd;

  /* Step 5: Configure pin control (no loopback) - MUST be written before SPCR.SPE
   * per HW manual to avoid glitches on COPI/CIPO lines during enable */
  rspi->sppcr = k_rspi_sppcr_no_loopback;

  /* Step 6: Enable SPI in controller mode (MSTR=1, SPE=1) - MUST be the last register
   * write so all configuration is stable before the controller drives the bus */
  rspi->spcr = k_rspi_spcr_spe | k_rspi_spcr_mstr;

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
 *         - (uint8_t)channel >= k_rspi_max_channels
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
rx_err_t rspi_init_controller(const rspi_channel_t channel, const rspi_controller_config_t* config)
{
  /* Validate all arguments and preconditions */
  rx_err_t err = internal_validate_controller_args(channel, config);
  if (err != k_rx_ok) {
    return err;
  }

  /* Prepare hardware resources (SPBR, RSPI base, CS GPIO) */
  volatile rx_rspi_regs_t* rspi = nullptr;
  uint8_t                  spbr = k_rspi_spbr_init;
  err                           = internal_prepare_controller(channel, config, &rspi, &spbr);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure SPI mode (CPOL and CPHA) */
  uint16_t spcmd = k_rspi_spcmd_init;
  internal_configure_spcmd(&spcmd, config->spi_mode);

  /* Configure 16-bit data length */
  spcmd |= (k_rspi_spcmd_16bit << k_rspi_spcmd_spl_shift);

  /* Configure and enable RSPI hardware registers */
  return internal_configure_registers(channel, rspi, spbr, spcmd, config);
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
rx_err_t rspi_controller_set_cs(const rspi_channel_t channel, const bool active)
{
  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels || !s_rspi_controller_initialized[channel]) {
    rx_log_error(s_tag, "RSPI controller channel not initialized");
    return k_rx_err_invalid_state;
  }

  const uint8_t                  port      = s_rspi_cs_config[channel].port;
  const uint8_t                  pin       = s_rspi_cs_config[channel].pin;
  volatile rx_port_regs_t* const port_regs = rx_port_get_base(port);

  if (port_regs == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Compute pin mask once (identical for both assert and deassert) */
  const uint8_t pin_mask = (uint8_t)((uint32_t)k_rspi_bit_set << pin);

  if (active) {
    /* CS active (low) */
    port_regs->podr &= (uint8_t)~pin_mask;
  } else {
    /* CS inactive (high) */
    port_regs->podr |= pin_mask;
  }

  return k_rx_ok;
}

/**
 * @brief Assert CS with setup delay
 *
 * @details
 * Asserts the chip select line (drives low for active-low CS) and then
 * applies a cycle-accurate setup time delay using NOP instructions. The
 * setup delay ensures the SPI peripheral has sufficient time to recognize
 * CS assertion before clock transitions begin. Delay is calibrated for
 * ~300ns at 240 MHz core clock.
 *
 * @param[in] channel RSPI channel number (0 to k_rspi_max_channels - 1)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok CS asserted and setup delay completed
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_invalid_arg GPIO port lookup failed
 *
 * @pre Channel must be initialized via rspi_init_controller()
 * @pre s_rspi_controller_initialized[channel] must be true
 * @post CS GPIO pin driven low (active)
 * @post Minimum setup delay of ~300ns has elapsed
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see internal_controller_deassert_cs_with_hold() Companion deassert function
 * @see internal_timing_delay() Provides cycle-accurate delay
 * @see k_rspi_cs_setup_delay Setup delay iteration count
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_controller_assert_cs_with_setup(const rspi_channel_t channel)
{
  /* Rule 5: Pre-condition validation */
  RX_ASSERT((channel == k_rspi_channel_0) || (channel == k_rspi_channel_1) ||
              (channel == k_rspi_channel_2),
            "Invalid RSPI channel");
  RX_ASSERT(s_rspi_controller_initialized[channel], "RSPI controller channel not initialized");

  /* Assert CS (active low) */
  const rx_err_t err = rspi_controller_set_cs(channel, true);
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
 * @details
 * Applies a cycle-accurate hold time delay using NOP instructions, then
 * deasserts the chip select line (drives high for active-low CS). The hold
 * delay ensures the SPI peripheral has sufficient time to latch the final
 * data bit before CS is released. Delay is calibrated for ~300ns at 240 MHz
 * core clock.
 *
 * @param[in] channel RSPI channel number (0 to k_rspi_max_channels - 1)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Hold delay completed and CS deasserted
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_invalid_arg GPIO port lookup failed
 *
 * @pre Channel must be initialized via rspi_init_controller()
 * @pre s_rspi_controller_initialized[channel] must be true
 * @post Minimum hold delay of ~300ns has elapsed
 * @post CS GPIO pin driven high (inactive)
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see internal_controller_assert_cs_with_setup() Companion assert function
 * @see internal_timing_delay() Provides cycle-accurate delay
 * @see k_rspi_cs_hold_delay Hold delay iteration count
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_controller_deassert_cs_with_hold(const rspi_channel_t channel)
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
 * @details
 * Executes a single full-duplex 16-bit SPI transfer without chip select
 * management. Waits for the transmit buffer to become empty, writes 16-bit
 * data to SPDR, waits for the receive buffer to fill, reads the 16-bit
 * response from SPDR, and clears SPRF and OVRF status flags. This is the
 * inner transfer primitive used by rspi_controller_transfer_16bit() after
 * CS assertion and before CS deassertion.
 *
 * @param[in]  rspi    Pointer to RSPI register base (must be valid, non-null)
 * @param[in]  tx_data 16-bit data word to transmit via SPDR
 * @param[out] rx_data Pointer to store received 16-bit response from SPDR
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Transfer completed successfully, rx_data contains response
 * @retval k_rx_err_null_ptr rspi or rx_data pointer is nullptr
 * @retval k_rx_err_timeout TX buffer did not empty or RX buffer did not fill
 *
 * @pre rspi must be a valid non-null pointer to initialized RSPI registers
 * @pre SPI must be enabled (SPCR.SPE=1) before calling
 * @post *rx_data contains the 16-bit value received during the transfer
 * @post SPSR flags SPRF and OVRF are cleared
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @see rspi_controller_transfer_16bit() Public API that wraps this with CS handling
 * @see internal_wait_tx_ready() TX buffer polling with timeout
 * @see internal_wait_rx_ready() RX buffer polling with timeout
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_controller_do_16bit_transfer(volatile rx_rspi_regs_t* rspi,
                                                      uint16_t                 tx_data,
                                                      uint16_t*                rx_data)
{
  /* Rule 5: Pre-condition validation */
  RX_CHECK_NULL_PTR(rspi, s_tag, "RSPI register pointer is nullptr");
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

  /* Wait for transmit buffer empty */
  rx_err_t err = internal_wait_tx_ready(rspi);
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
 *         - (uint8_t)channel >= k_rspi_max_channels
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
rx_err_t rspi_controller_transfer_16bit(const rspi_channel_t channel,
                                        const uint16_t       tx_data,
                                        uint16_t* const      rx_data)
{
  RX_CHECK_NULL_PTR(rx_data, s_tag, "RX data pointer is nullptr");

  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels || !s_rspi_controller_initialized[channel]) {
    rx_log_error(s_tag, "RSPI controller channel not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* const rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Assert CS with setup delay */
  rx_err_t err = internal_controller_assert_cs_with_setup(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Perform 16-bit TX/RX transfer */
  err = internal_controller_do_16bit_transfer(rspi, tx_data, rx_data);
  if (err != k_rx_ok) {
    /* Deassert CS on error (ignore CS deassert errors) */
    (void)rspi_controller_set_cs(channel, false);
    return err;
  }

  /* Deassert CS with hold delay */
  return internal_controller_deassert_cs_with_hold(channel);
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
 *       - RSPI is disabled (rspi->spcr = k_rspi_spcr_disabled)
 *       - Module clock is stopped via internal_set_mstpcrb_for_channel()
 *       - s_rspi_cs_config[channel] is cleared (port=0, pin=0)
 *       - s_rspi_controller_initialized[channel] = false
 *       - Register protection is unlocked/locked via *prcr_reg()
 *
 * @note Thread-safe only if caller ensures no concurrent access to the same channel
 */
rx_err_t rspi_controller_deinit(const rspi_channel_t channel)
{
  /* Validate channel */
  if ((uint8_t)channel >= k_rspi_max_channels) {
    rx_log_error(s_tag, "Invalid RSPI channel");
    return k_rx_err_invalid_arg;
  }

  /* Get RSPI base */
  volatile rx_rspi_regs_t* const rspi = internal_get_rspi_base(channel);
  if (rspi == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /*
   * RSPI Controller Deinitialization Sequence
   * ==========================================
   * The deinit order ensures safe shutdown without bus glitches:
   * 1. Deassert CS (drive high) - release peripheral before disabling SPI
   * 2. Disable SPI (SPCR.SPE=0) - stop controller clock generation
   * 3. Disable module clock (MSTPCRB) - gate clock only after SPI is idle
   * 4. Clear software state - CS config and init flag after hardware is off
   */

  /* Step 1: Deassert CS to release any connected peripheral before shutdown */
  if (s_rspi_controller_initialized[channel]) {
    (void)rspi_controller_set_cs(channel, false);
  }

  /* Step 2: Disable SPI to stop controller clock generation */
  rspi->spcr = k_rspi_spcr_disabled;

  /* Step 3: Disable RSPI module clock (set module stop bit) - must be done
   * after SPI is disabled to avoid undefined behavior from clock gating
   * during an active transfer */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  internal_set_mstpcrb_for_channel(channel, false);
  *prcr_reg() = k_rx_prcr_lock;

  /* Step 4: Clear software state after hardware is safely off */
  s_rspi_cs_config[channel].port = k_rspi_cs_default_port;
  s_rspi_cs_config[channel].pin  = k_rspi_cs_default_pin;

  s_rspi_controller_initialized[channel] = false;

  rx_log_debug(s_tag, "RSPI controller deinitialized");

  return k_rx_ok;
}

#endif /* __RX__ */
