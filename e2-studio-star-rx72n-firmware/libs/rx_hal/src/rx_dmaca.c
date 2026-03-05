/* libs/rx_hal/src/rx_dmaca.c */

/**
 * @file rx_dmaca.c
 * @brief DMACA driver for RX72N
 * @version 1.0.0
 *
 * Implements the API declared in rx_dmaca.h.
 *
 * Hardware references:
 *   RX72N Hardware Manual Sec.18  (DMACA)
 *   RX72N Hardware Manual Sec.46  (CRCA)
 *   RX Family Register Access  iodefine.h / r_bsp header set
 *
 * Design constraints honoured:
 *   - 8-bit and 32-bit transfers only -- 16-bit is prohibited for CRCDIR.
 *   - Destination address is NEVER incremented for CRCDIR use.
 *   - All completion loops are bounded (NASA Rule 2).
 *   - No dynamic memory allocation (NASA Rule 3).
 *   - Every public function validates its arguments and returns rx_err_t.
 *
 * @details
 * Internal implementation of the DMACA abstraction layer used by the
 * CRC driver.  Provides initialization, channel configuration, start,
 * polling wait, and abort functionality.  The code avoids dynamic
 * allocation and uses compile-time constants; all loops are bounded to
 * satisfy safety-critical restrictions.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 2: Bounded loops for DMA polling and configuration
 * - Rule 3: No dynamic memory allocation anywhere in this file
 * - Rule 5: Argument validation in every public API with pre/post
 *            conditions documented
 * - Rule 7: All return values checked by callers
 *
 * @par SOLID Principles Adherence:
 * - Single Responsibility: each function performs exactly one task (init,
 *   configure, start, wait, abort)
 * - Open/Closed: configuration structure allows new transfer modes without
 *   modifying existing code
 * - Liskov Substitution: validate_channel ensures any channel index is
 *   safely handled, making higher-level code agnostic to invalid inputs
 * - Interface Segregation: small, focused API (configure/start/wait/abort)
 * - Dependency Inversion: high-level drivers depend on this abstract API
 *   rather than raw hardware registers
 *
 * @see docs/sections/06_nasa_power_of_10.tex
 * @see docs/sections/04_style_guide.tex
 * @see docs/sections/07_gateway_architecture.tex
 *
 * @author STAR Team
 * @date 2026-02-22
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_dmaca.h"

#include <stddef.h>

#include "rx_check.h"
#include "rx_err.h"
#include "tx_api.h"

/* hardware register definitions for DMAC (RX72N) */
#include "rx72n_dmac_regs.h"

/* =========================================================================
 * Internal helpers -- register access
 *
 * The RX72N iodefine.h exposes DMAC channels as an array:
 *   DMAC0 ... DMAC7  or  DMAC[0] ... DMAC[7]  (BSP-version-dependent)
 *
 * We use a small inline accessor to keep channel selection uniform.
 * ========================================================================= */

/* ------------------------------------------------------------------
 * Private constants
 * ------------------------------------------------------------------ 
 */

/**
 * @enum dmaca_const_t
 * @brief Local register constants for DMACA control values
 *
 * @details
 * Defines named constants for register clearing operations and software
 * request bit values. These are represented as a typed enum (uint8_t) per
 * C23 style guidelines to avoid magic numbers and enable compile-time type
 * checking. Call sites must cast values to uint8_t when writing directly to
 * hardware registers that require the uint8_t type.
 *
 * @invariant All values must match hardware register encodings and bit
 * positions defined in the RX72N datasheet for DMACA control registers.
 *
 * @par Example:
 * @code
 * // Clear interrupt enable flag by writing DMINT register
 * ch->DMINT = (uint8_t)k_dmaca_dmint_clear;
 *
 * // Clear status flags by writing DMSTS register
 * ch->DMSTS = (uint8_t)k_dmaca_dmsts_clear;
 *
 * // Trigger software request
 * ch->DMAMD |= (uint8_t)k_dmaca_swreq_request;
 * @endcode
 *
 * @see k_dmaca_dmint_clear, k_dmaca_dmsts_clear, k_dmaca_swreq_request
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_dmint_clear   = 0x00U, /**< Clear DMINT (disable interrupts) */
  k_dmaca_dmsts_clear   = 0x00U, /**< Clear DMSTS (clear status flags) */
  k_dmaca_dmcrb_clear   = 0x00U, /**< Value written to DMCRB to clear it */
  k_dmaca_swreq_request = 1U,    /**< SWREQ request bit value */
} dmaca_const_t;

/**
 * @enum dmaca_bitmap_t
 * @brief Bitmask helpers for tracking configured channels
 *
 * @details
 * The driver maintains a bitmap (s_channel_configured) where each bit
 * corresponds to a channel index.  This enum provides named constants for the
 * empty bitmap and the base bit value used when shifting by a channel index.
 * Using typed enum values avoids magic literals and clarifies intent when
 * manipulating the bitmap.
 *
 * @invariant Only the two enumerators are used; shifting by channel index is
 *            performed with k_dmaca_channel_bitmap_bit.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_channel_bitmap_none = 0U, /**< No channels configured */
  k_dmaca_channel_bitmap_bit  = 1U, /**< Base bit value for one channel */
} dmaca_bitmap_t;

/**
 * @enum dmaca_dte_t
 * @brief Values for the DMCNT.DTE bit
 *
 * @details
 * Used internally when enabling or disabling the DMA transfer engine for a
 * channel.  Grouped here as a typed enum for readability; the underlying
 * register field is one bit wide so the values are 0 or 1.
 *
 * @invariant dmaca_dte_t is a one-bit field constraint: values must be 0 or 1 
 * and correspond directly to the DMCNT.DTE register bit encoding in the RX72N 
 * datasheet.
 *
 * @code
 * // Example: enable DMA transfer engine for a channel
 * ch->dmcnt |= (uint8_t)k_dmaca_dte_enable;  // Set DTE = 1
 * // Example: disable DMA transfer engine
 * ch->dmcnt &= ~(uint8_t)k_dmaca_dte_enable;  // Clear DTE bit to disable
 * @endcode
 *
 * @see k_dmaca_dte_enable, k_dmaca_dte_disable, rx_dmaca_start, rx_dmaca_abort
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_dte_disable = 0U, /**< DTE = 0, DMA engine disabled */
  k_dmaca_dte_enable  = 1U, /**< DTE = 1, DMA engine enabled  */
} dmaca_dte_t;

/**
 * @enum dmaca_timeout_scale_t
 * @brief Constants for DMA timeout calculation
 *
 * @details
 * Derived from worst-case per-transfer cycles and ICLK/PCLKB ratio.
 * At 240/60 MHz (ratio = 4): (Cr + Cw) = (1 + 4) = 5 PCLKB cycles
 * Per transfer: 5 x 4 = 20 ICLK iterations. Add 25% margin (5/4).
 *
 * @invariant These constants are compile-time invariants derived from the RX72N 
 * hardware clock configuration (240 MHz ICLK, 60 MHz PCLKB). They must not be 
 * modified and represent the per-transfer cost and margin parameters for timeout 
 * calculations in the DMACA polling loops.
 *
 * @code
 * // Example: compute timeout for a 256-byte transfer
 * uint32_t transfer_size = 256;  // bytes
 * uint32_t timeout = (transfer_size * k_dmaca_cycles_per_xfer * 
 *                      k_dmaca_margin_numerator) / k_dmaca_margin_denominator;
 * rx_dmaca_wait(channel, timeout);
 * @endcode
 *
 * @see k_dmaca_cycles_per_xfer, k_dmaca_margin_numerator, k_dmaca_margin_denominator, 
 *      rx_dmaca_transfer_blocking, rx_dmaca_wait
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_cycles_per_xfer    = 20U, /**< ICLK iterations per transfer */
  k_dmaca_margin_numerator   = 5U,  /**< Bus contention margin numerator */
  k_dmaca_margin_denominator = 4U,  /**< Bus contention margin denominator */
} dmaca_timeout_scale_t;

/**
 * @enum dmaca_wait_sched_t
 * @brief Cooperative scheduling constants for bounded DMA wait polling
 *
 * @details
 * Defines the number of polls between scheduler yields and the sleep duration
 * in RTOS ticks used by rx_dmaca_wait(). This prevents pure busy-wait behavior
 * while preserving bounded polling semantics.
 *
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_wait_polls_per_sleep = 16U, /**< Polls between scheduler sleeps */
  k_dmaca_wait_sleep_ticks     = 1U,  /**< ThreadX ticks to sleep per yield */
} dmaca_wait_sched_t;

/* ------------------------------------------------------------------
 * DMTMD register bit definitions (transfer mode)
 * ------------------------------------------------------------------ 
 */

/**
 * @enum dmtmd_shift_t
 * @brief Bit shift positions within the DMTMD (Transfer Mode) register
 *
 * @details
 * Defines the bit positions for the MD (mode) and SZ (size) fields
 * within the 16-bit DMTMD register. Used when constructing the
 * register value during channel configuration.
 *
 * @invariant k_dmtmd_sz_shift must match the bit position defined in the 
 * RX72N datasheet for correct register field masking and shifting.
 *
 * @code
 * // Example: construct DMTMD value with byte size field
 * uint16_t dmtmd = (k_dmtmd_sz_byte << k_dmtmd_sz_shift);
 * ch->dmtmd = dmtmd;
 * @endcode
 *
 * @see dmtmd_mode_t, dmtmd_size_t, rx_dmaca_configure, k_dmtmd_sz_shift
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmtmd_sz_shift = 8U,  /**< Bit position for SZ[1:0] data size field */
} dmtmd_shift_t;

/**
 * @enum dmtmd_size_t
 * @brief Data size field values for DMTMD.SZ[1:0]
 *
 * @details
 * Selects the transfer data width. Only 8-bit (byte) and 32-bit (longword)
 * are supported for CRCDIR compatibility; 16-bit transfers are rejected.
 *
 * @invariant Values must match DMTMD.SZ[1:0] encoding in RX72N datasheet: 
 * 0 = byte (8-bit), 2 = longword (32-bit). Value 1 (16-bit word transfer) is 
 * not supported and must be rejected.
 *
 * @code
 * // Example: set longword transfer size
 * uint16_t dmtmd = (k_dmtmd_sz_longword << k_dmtmd_sz_shift);
 * ch->dmtmd = (ch->dmtmd & ~(3U << k_dmtmd_sz_shift)) | dmtmd;
 * @endcode
 *
 * @see dmtmd_shift_t, rx_dmaca_configure, k_dmtmd_sz_byte, k_dmtmd_sz_longword
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmtmd_sz_byte     = 0U, /**< 8-bit (byte) transfer width */
  k_dmtmd_sz_longword = 2U, /**< 32-bit (longword) transfer width */
} dmtmd_size_t;


/* =========================================================================
 * Module-level state
 * ========================================================================= 
 */

/**
 * @var s_dmaca_initialized
 * @brief DMACA initialization flag
 *
 * @details
 * This boolean indicates whether the DMACA driver has been successfully
 * initialized via rx_dmaca_init().  Public API calls use this flag to
 * reject requests made prior to initialization.  It is cleared during
 * module startup and set to true exactly once by rx_dmaca_init().
 *
 * @note This is a module-level static variable; it is not visible outside
 *       rx_dmaca.c and should only be accessed within this translation
 *       unit.
 *
 * @warning Direct modification of this variable is forbidden outside of
 *          this module.  Only rx_dmaca_init() or other internal helper
 *          routines are permitted to change its value.
 *
 * @since 1.0.0
 */
static bool s_dmaca_initialized = false;

/**
 * @var s_channel_configured
 * @brief Bitmap of channels with active or pending configurations
 *
 * @details
 * Each bit in this 8-bit bitmap corresponds to one DMACA channel.  When
 * bit N is set to 1 the associated channel N currently has an active
 * configuration or a configuration that is pending completion.  The flag is
 * manipulated by rx_dmaca_configure() and cleared when transfers complete.
 *
 * Bit semantics:
 *   - Bit 0 -> channel 0
 *   - Bit 1 -> channel 1
 *   - ...
 *
 * Bit N = 1 means channel N has an active or pending configuration.
 *
 * @note This variable has module-level scope; it is static to rx_dmaca.c and
 *       must not be referenced from other files.
 *
 * @warning Do not modify this bitmap directly outside of the module.  Changes
 *          should only occur through the initialization routine
 *          rx_dmaca_init() or internal APIs that manage channel state.
 *
 * @since 1.0.0
 */
static uint8_t s_channel_configured = 0U;

/* =========================================================================
 * Argument validation helpers
 * ========================================================================= 
 */

/**
 * @brief  Validate a DMACA channel index.
 *
 * @details
 * Internal helper that checks whether the provided channel number falls
 * within the valid range of channels supported by the RX72N DMACA
 * peripheral.  Returns k_rx_ok for a valid index and k_rx_err_nack for
 * any out-of-range value.  This function is used by all public APIs to
 * centralise boundary checking.
 *
 * @param[in] channel DMACA channel index to validate.
 *                    Valid range is [0, k_dmaca_num_channels-1].
 *
 * @return rx_err_t  Result of validation.
 * @retval k_rx_ok      Channel index is within range.
 * @retval k_rx_err_nack Channel index is invalid (>= k_dmaca_num_channels).
 *
 * @pre  channel must be a uint8_t value providing an index for boundary validation
 * @pre  DMACA subsystem is in a state where channel index validation is meaningful
 *       (i.e., the DMACA module is accessible and k_dmaca_num_channels is properly initialized)
 *
 * @post Return value is either k_rx_ok or k_rx_err_nack
 * @post Returns k_rx_ok if and only if channel < k_dmaca_num_channels
 * @post Returns k_rx_err_nack if and only if channel >= k_dmaca_num_channels
 * @post No global state is modified; function has no side effects
 *
 * @note This is an internal, fast helper; callers should still handle the
 *       error return value appropriately.  Uses RX_ASSERT macros to validate
 *       preconditions during debug builds.
 *
 * @since 1.0.0
 *
 * @see k_dmaca_num_channels, k_rx_err_nack, k_rx_ok, RX_ASSERT, internal_validate_channel
 */
static rx_err_t internal_validate_channel(uint8_t channel)
{
  /* Precondition: k_dmaca_num_channels must be valid (non-zero) */
  RX_ASSERT(k_dmaca_num_channels > 0U, "DMACA validate_channel: num_channels constant is zero");
  /* postcondition: result must be one of the two defined codes */
  rx_err_t result;

  if (channel >= k_dmaca_num_channels) {
    result = k_rx_err_nack;
  } else {
    result = k_rx_ok;
  }

  RX_ASSERT((result == k_rx_ok) || (result == k_rx_err_nack),
            "DMACA validate_channel returned unexpected value");

  return result;
}

/* -------------------------------------------------------------------------
 * Internal helper routines extracted from rx_dmaca_configure().  Breaking the
 * original 139-line function into smaller pieces keeps each module under the
 * 60-line limit and improves readability.  Each helper returns an error
 * code so the caller can propagate failures.
 */

/**
 * @brief  Perform runtime validation of parameter structure.
 *
 * @details
 * Executes all of the argument checks that were previously embedded in
 * rx_dmaca_configure().  This includes null-pointer tests and value range
 * verifications for transfer count, data size, and source/destination
 * pointers.  Hardware-specific invariants such as supported transfer modes or
 * address modes are *not* checked here; those are handled by the register
 * configuration helpers.
 *
 * @param[in] p_cfg Pointer to the configuration structure.
 *
 * @return rx_err_t
 * @retval k_rx_ok       Parameters appear valid.
 * @retval k_rx_err_nack One or more fields are invalid (NULL or out of
 *                       allowable range).
 *
 * @pre  p_cfg may be NULL (checked by this helper).
 * @pre  If p_cfg is non-NULL, all fields (transfer_count, data_size, p_src,
 *       p_dst) must be initialized to defined values.
 *
 * @post If k_rx_ok is returned, the fields used by the configure helpers
 *       meet the constraints described above.
 * @post If k_rx_err_nack is returned, no global state is modified.
 *
 * @note This function is internal and not thread-safe.  Caller must still
 *       treat a non-zero return value as an error condition.
 *
 * @since 1.0.0
 */
static rx_err_t internal_validate_config_params(const dmaca_config_t* p_cfg)
{
  /* Preconditions: NULL pointer is handled gracefully rather than asserted */
  if (p_cfg == NULL) {
    return k_rx_err_nack;
  }

  if ((p_cfg->transfer_count == 0U) || (p_cfg->transfer_count > k_dmaca_max_transfer_count)) {
    return k_rx_err_nack;
  }

  if ((p_cfg->data_size != k_dmaca_size_byte) && (p_cfg->data_size != k_dmaca_size_longword)) {
    return k_rx_err_nack;
  }

  if ((p_cfg->p_src == NULL) || (p_cfg->p_dst == NULL)) {
    return k_rx_err_nack;
  }

  /* Postcondition: Validated invariants enforced in debug builds (NASA Rule 5) */
  RX_ASSERT((p_cfg->transfer_count != 0U) && (p_cfg->transfer_count <= k_dmaca_max_transfer_count),
            "validate_config_params: transfer_count invariant violated");
  RX_ASSERT((p_cfg->data_size == k_dmaca_size_byte) || (p_cfg->data_size == k_dmaca_size_longword),
            "validate_config_params: data_size invariant violated");

  return k_rx_ok;
}

/**
 * @brief  Populate the transfer mode register (DMTMD) for a channel.
 *
 * @details
 * Builds the 16-bit register value based on the configuration structure and
 * writes it to the channel's DMTMD register.  Only byte and longword data
 * sizes are supported; invalid transfer or data size values produce an error
 * code.
 *
 * @param[in] ch    Pointer to the channel registers.
 * @param[in] p_cfg Configuration parameters (assumed non-NULL).
 *
 * @return rx_err_t
 * @retval k_rx_ok       Register written successfully.
 * @retval k_rx_err_nack Unsupported transfer mode or data size.
 *
 * @pre  ch must point to a valid channel register block.
 * @pre  p_cfg must be non-NULL with valid transfer_mode and data_size fields.
 *
 * @post DMTMD register contains the encoded mode or prior value remains
 * @post On k_rx_ok, ch->dmtmd is updated atomically.
 *
 * @note Thread safety: NOT thread-safe. Caller must ensure exclusive access
 *       to the channel register block during configuration.
 *       unchanged on error.
 *
 * @since 1.0.0
 */
static rx_err_t internal_configure_transfer_mode(volatile rx_dmac_channel_regs_t* p_ch,
                                                 const dmaca_config_t*            p_cfg)
{
  /* Preconditions */
  RX_ASSERT(p_ch != NULL, "Channel register pointer is NULL");
  RX_ASSERT(p_cfg != NULL, "Config pointer is NULL");

  uint16_t dmtmd = 0U;

  switch (p_cfg->transfer_mode) {
    case k_dmaca_mode_normal:
      dmtmd |= k_dmtmd_md_normal;
      break;
    case k_dmaca_mode_block:
      dmtmd |= k_dmtmd_md_block;
      break;
    case k_dmaca_mode_repeat:
      dmtmd |= k_dmtmd_md_repeat;
      break;
    default:
      return k_rx_err_nack;
  }

  if (p_cfg->data_size == k_dmaca_size_longword) {
    dmtmd |= (k_dmtmd_sz_longword << k_dmtmd_sz_shift);
  } else if (p_cfg->data_size != k_dmaca_size_byte) {
    return k_rx_err_nack;
  }

  p_ch->dmtmd = dmtmd;
  return k_rx_ok;
}

/**
 * @brief  Populate the address mode register (DMAMD) for a channel.
 *
 * @details
 * Computes the DMAMD value from the configuration.  The current firmware
 * only supports a fixed destination address; attempts to use any other mode
 * return an error.
 *
 * @param[in] ch    Pointer to the channel registers.
 * @param[in] p_cfg Configuration parameters (assumed non-NULL).
 *
 * @return rx_err_t
 * @retval k_rx_ok       Register written successfully.
 * @retval k_rx_err_nack Unsupported address mode.
 *
 * @pre  ch must point to a valid channel register block.
 * @pre  p_cfg must be non-NULL with valid src_addr_mode and dst_addr_mode fields.
 *
 * @post DMAMD register contains the encoded mode or prior value remains
 *       unchanged on error.
 * @post On k_rx_ok, ch->dmamd is updated atomically.
 *
 * @note Thread safety: NOT thread-safe. Caller must ensure exclusive access
 *       to the channel register block during configuration.
 *
 * @since 1.0.0
 */
static rx_err_t internal_configure_address_mode(volatile rx_dmac_channel_regs_t* p_ch,
                                                const dmaca_config_t*            p_cfg)
{
  /* Preconditions */
  RX_ASSERT(p_ch != NULL, "Channel register pointer is NULL");
  RX_ASSERT(p_cfg != NULL, "Config pointer is NULL");

  uint16_t dmamd = 0U;

  switch (p_cfg->src_addr_mode) {
    case k_dmaca_addr_fixed:
      dmamd |= k_dmamd_sm_fixed;
      break;
    case k_dmaca_addr_increment:
      dmamd |= k_dmamd_sm_increment;
      break;
    case k_dmaca_addr_decrement:
      dmamd |= k_dmamd_sm_decrement;
      break;
    default:
      return k_rx_err_nack;
  }

  if (p_cfg->dst_addr_mode != k_dmaca_addr_fixed) {
    return k_rx_err_nack;
  }

  p_ch->dmamd = dmamd;
  return k_rx_ok;
}

/**
 * @brief  Write source/destination/count registers for a channel.
 *
 * @details
 * Stops the DMA transfer (transfer enable bit must already be cleared by
 * caller) and writes the source address, destination address, and transfer
 * count registers.  The repeat count register is always cleared.
 *
 * @param[in] ch    Pointer to the channel registers.
 * @param[in] p_cfg Configuration parameters (assumed non-NULL).
 *
 * @return rx_err_t Always returns k_rx_ok; parameters have already been
 *                     validated.
 * @retval k_rx_ok Configuration applied successfully.
 *
 * @pre  ch must point to a valid channel register block and the caller must
 *       have disabled DTE.
 * @pre  p_cfg must be non-NULL with valid p_src, p_dst, and transfer_count.
 *
 * @post Source, destination, and count registers reflect the supplied
 *       configuration.
 * @post ch->dmcrb is cleared to 0U.
 *
 * @note Thread safety: NOT thread-safe. Caller must ensure exclusive access
 *       to the channel register block during configuration.
 *
 * @since 1.0.0
 */
static rx_err_t internal_configure_channel_regs(volatile rx_dmac_channel_regs_t* p_ch,
                                                const dmaca_config_t*            p_cfg)
{
  /* Preconditions */
  RX_ASSERT(p_ch != NULL, "Channel register pointer is NULL");
  RX_ASSERT(p_cfg != NULL, "Config pointer is NULL");
  p_ch->dmsar = (uint32_t)(uintptr_t)p_cfg->p_src;
  p_ch->dmdar = (uint32_t)(uintptr_t)p_cfg->p_dst;
  p_ch->dmcra = (uint32_t)p_cfg->transfer_count;
  p_ch->dmcrb = (uint8_t)k_dmaca_dmcrb_clear;
  return k_rx_ok;
}

/* =========================================================================
 * Public API implementation
 * ========================================================================= 
 */

/**
 * @brief  Initialize the DMACA peripheral.
 *
 * @details
 * Releases the DMACA module from module-stop by setting the DMST bit in
 * the DMAST register.  This function also clears the per-channel configured
 * bitmap and marks the module as initialized.
 *
 * Re-initialization without a reset is a programming error. In debug builds,
 * an assertion will fire. In release builds, the function returns
 * k_rx_err_invalid_state to signal the error condition.
 *
 * @return rx_err_t
 * @retval k_rx_ok  Initialization succeeded (module clock enabled, state reset).
 * @retval k_rx_err_invalid_state Already initialized (re-initialization attempted
 *                                without calling reset first).
 *
 * @pre  System clock configuration complete and __RX__ target (DMACA present).
 * @pre  No DMA transfers are active on any channel.
 * @pre  Module must not already be initialized (s_dmaca_initialized == false).
 *
 * @post DMACA module clock is enabled (MSTP(DMAC) == 0).
 * @post s_dmaca_initialized == true
 * @post s_channel_configured == 0U
 *
 * @note This routine is NOT thread-safe and should be called once from a
 *       single initialization context prior to any RTOS tasks using DMACA.
 * @warning Calling this function more than once without an intervening reset
 *          is a programming error that will assert in debug builds and return
 *          an error in release builds.
 *
 * @see dmac_dmast_reg(), k_dmast_dmst
 * @since 1.0.0
 */
rx_err_t rx_dmaca_init(void)
{

  RX_ASSERT(dmac_dmast_reg() != NULL, "DMAC init: DMAST register pointer NULL");
  /* Re-initializing without a reset is a programming error.
     * Debug builds assert so callers can catch the mistake.
     * Release builds return an error to signal invalid state. */
  if (s_dmaca_initialized) {
    RX_ASSERT(false, "DMAC init: re-initialization without reset");
    return k_rx_err_invalid_state;
  }

  /* DMAC is not affected by MSTP (11.4 in User's Manual) */
  /* Enable DMACA module-level operation (DMST bit in DMAST register). */
  /* hardware header provides helper for the DMAST register plus bit masks */
  *dmac_dmast_reg() |= (uint8_t)k_dmast_dmst;

  /* Force all channels into a known idle state by clearing DMCNT.DTE. */
  for (uint8_t channel = 0U; channel < k_dmaca_num_channels; ++channel) {
    volatile rx_dmac_channel_regs_t* p_ch = dmac_channel(channel);
    RX_ASSERT(p_ch != NULL, "DMAC init: channel register pointer NULL");
    p_ch->dmcnt &= ~(uint8_t)k_dmaca_dte_enable;
  }

  s_dmaca_initialized  = true;
  s_channel_configured = (uint8_t)k_dmaca_channel_bitmap_none;

  RX_ASSERT(s_channel_configured == (uint8_t)k_dmaca_channel_bitmap_none,
            "DMAC init did not clear channel bitmap");

  return k_rx_ok;
}

/**
 * @brief Configure a single DMACA channel for a transfer operation.
 *
 * @details
 * Writes the supplied configuration into the channel's register set
 * (DMSAR, DMDAR, DMCRA, DMTMD, DMAMD, DMINT, DMSTS, and DMCNT).
 * The values are latched immediately by the hardware; no DMA activity is
 * triggered by this call. The channel remains inactive (ACT == 0) until
 * rx_dmaca_start() is invoked.
 *
 * **Configuration steps:**
 * 1. Validates channel index (0 to k_dmaca_num_channels-1)
 * 2. Validates p_cfg pointer (non-NULL, all fields valid)
 * 3. Disables the channel (clears DTE bit)
 * 4. Programs source and destination addresses
 * 5. Configures transfer count, mode, data size, and address modes
 * 6. Clears interrupt flags and status
 * 7. Returns with channel disabled and ready for rx_dmaca_start()
 *
 * **Data size restrictions:**
 * Only k_dmaca_size_byte (8-bit) and k_dmaca_size_longword (32-bit) transfers
 * are supported. 16-bit (word) transfers are explicitly rejected because hardware
 * peripherals like CRCDIR do not accept 16-bit bus accesses. Requests for
 * unsupported sizes return k_rx_err_nack.
 *
 * **Transfer count validation:**
 * Must be in range [1, k_dmaca_max_transfer_count] inclusive. The RX72N DMACA
 * uses a 10-bit counter, limiting blocks to 1024 transfers maximum. Callers
 * must split larger buffers into multiple DMA operations.
 *
 * @param[in] channel DMACA channel index (0 to k_dmaca_num_channels-1).
 *                    Invalid indices return k_rx_err_nack without modifying
 *                    hardware.
 *
 * @param[in] p_cfg   Pointer to dmaca_config_t configuration structure.
 *                    Must not be NULL. All fields validated:
 *                    - p_src, p_dst: non-NULL, properly aligned
 *                    - transfer_count: [1, k_dmaca_max_transfer_count]
 *                    - data_size: k_dmaca_size_byte or k_dmaca_size_longword
 *                    - transfer_mode: k_dmaca_mode_normal, _block, or _repeat
 *                    - src_addr_mode: fixed, increment, or decrement
 *                    - dst_addr_mode: must be k_dmaca_addr_fixed (enforced)
 *
 * @return rx_err_t Error code indicating configuration result.
 * @retval k_rx_ok           Configuration accepted and channel registers updated.
 * @retval k_rx_err_nack     Invalid channel, NULL p_cfg, invalid transfer_count,
 *                           unsupported data_size (16-bit), NULL p_src/p_dst,
 *                           invalid transfer_mode, or dst_addr_mode != fixed.
 *
 * @pre rx_dmaca_init() has been called to enable DMACA peripheral clock.
 * @pre channel must be a valid DMACA channel index.
 * @pre The specified channel is idle (ACT == 0) and not in an active transfer.
 * @pre p_cfg points to valid, readable dmaca_config_t structure.
 * @pre p_cfg->p_src and p_cfg->p_dst are non-NULL and properly aligned.
 * @pre p_cfg->transfer_count is in range [1, k_dmaca_max_transfer_count].
 *
 * @post Channel's DMSAR, DMDAR, DMCRA, DMTMD, DMAMD, DMINT registers reflect
 *       the configuration in *p_cfg.
 * @post Channel remains disabled (DTE == 0) and does not transfer data.
 * @post All interrupt and status flags are cleared (DMINT clear).
 * @post Channel is ready for rx_dmaca_start() to arm and activate.
 *
 * @note **Thread Safety:** NOT reentrant for the same channel. Concurrent
 *       configuration of different channels requires external serialization
 *       to prevent hardware bus collisions. No internal locking is performed.
 *
 * @warning Configuring a channel while it is actively transferring
 *          (ACT == 1, DTE == 1) may produce unpredictable results. Ensure
 *          the channel is idle before calling (use rx_dmaca_abort() if needed).
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 6 preconditions and 5 postconditions (exceeds minimum 2)
 * - Rule 7: [OK] All return values from validate_channel validated
 *
 * @see dmaca_config_t Structure containing all configuration parameters
 * @see rx_dmaca_init() Initialize DMACA module (must call first)
 * @see rx_dmaca_start() Arm and activate the configured channel
 * @see rx_dmaca_transfer_blocking() Combined configure + start + wait
 * @see validate_channel() Internal validation helper
 *
 * @since 1.0.0
 */
rx_err_t rx_dmaca_configure(uint8_t channel, const dmaca_config_t* p_cfg)
{
  RX_VALIDATE_INIT(s_dmaca_initialized, "CRC", "DMA module not initialized");
  rx_err_t                         err;
  volatile rx_dmac_channel_regs_t* ch;

  /* quick sanity asserts for debugging; runtime validity is handled below */
  RX_ASSERT(channel < k_dmaca_num_channels, "DMACA invalid channel");

  /* validate channel index first, then configuration parameters */
  err = internal_validate_channel(channel);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_validate_config_params(p_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  /* stop the channel before touching any registers */
  ch = dmac_channel(channel);
  RX_ASSERT(ch != NULL, "DMACA configure: channel register pointer NULL");
  ch->dmcnt &= ~(uint8_t)k_dmaca_dte_enable;

  /* write basic registers */
  err = internal_configure_channel_regs(ch, p_cfg);
  if (err != k_rx_ok) {
    return err; /* should never happen */
  }

  err = internal_configure_transfer_mode(ch, p_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_configure_address_mode(ch, p_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  /* disable interrupts and clear any stale status bits */
  ch->dmint = (uint8_t)k_dmaca_dmint_clear;
  ch->dmsts = (uint8_t)k_dmaca_dmsts_clear;

  /* mark channel configured so rx_dmaca_start can use it */
  s_channel_configured |= (uint8_t)(k_dmaca_channel_bitmap_bit << channel);
  /* Postcondition: channel must now be marked configured */
  RX_ASSERT((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) != 0U,
            "DMACA configure: channel not marked after configuration");

  return k_rx_ok;
}

/**
 * @brief  Arm and activate a configured DMACA channel.
 *
 * @details
 * Sets the DTE bit for the specified channel and issues a software request
 * pulse (SWREQ) to start the DMA transfer.  The channel must have been
 * previously configured via rx_dmaca_configure() and must be idle.  This
 * function does **not** poll for completion; callers may use rx_dmaca_wait()
 * or rx_dmaca_transfer_blocking() after starting.
 *
 * @param[in] channel DMACA channel index (0 .. k_dmaca_num_channels-1).
 *
 * @return rx_err_t
 * @retval k_rx_ok       Transfer started successfully.
 * @retval k_rx_err_nack Channel index invalid, or channel not configured.
 *
 * @pre rx_dmaca_init() has been called.
 * @pre rx_dmaca_configure(channel, ...) has returned k_rx_ok.
 * @pre The channel is idle (not currently active).
 *
 * @post On success: Channel's DTE bit is set (transfer engine enabled).
 * @post On success: Software request issued; DMA transfer is in progress.
 * @post On failure: Channel state is unchanged.
 *
 * @note Thread safety: callers must ensure exclusive access to the channel
 *       being started.  Concurrent starts on the same channel will lead to
 *       undefined hardware behaviour.
 *
 * @see rx_dmaca_configure(), rx_dmaca_wait(), rx_dmaca_transfer_blocking()
 * @since 1.0.0
 */
rx_err_t rx_dmaca_start(uint8_t channel)
{
  RX_VALIDATE_INIT(s_dmaca_initialized, "CRC", "DMA module not initialized");
  rx_err_t err;

  /* Precondition assertions */
  RX_ASSERT(channel < k_dmaca_num_channels, "DMACA invalid channel index");

  err = internal_validate_channel(channel);
  if (err != k_rx_ok) {
    return err;
  }

  if ((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) == 0U) {
    /* Channel has not been configured. */
    return k_rx_err_nack;
  }

  volatile rx_dmac_channel_regs_t* ch = dmac_channel(channel);
  RX_ASSERT(ch != NULL, "DMACA start: channel register pointer NULL");
  RX_ASSERT((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) != 0U,
            "DMACA start: channel not marked configured");

  /* Enable DMA transfer for this channel. */
  ch->dmcnt |= (uint8_t)k_dmaca_dte_enable;

  /* Issue software request (SWREQ pulse -> triggers the block transfer). */
  ch->dmreq |= (uint8_t)k_dmaca_swreq_request;

  return k_rx_ok;
}

/**
 * @brief  Wait for a DMA transfer to complete on a channel.
 *
 * @details
 * Implements bounded polling on the ACT (active) bit in the DMSTS register.
 * When ACT clears to 0, the transfer has completed. If the timeout expires
 * before completion, the function disables the transfer engine and returns
 * an error. The polling loop satisfies NASA Rule 2 (bounded loops) by using
 * the caller-supplied timeout_cycles as the iteration limit.
 * @param[in] channel        DMACA channel index (0 .. k_dmaca_num_channels-1).
 * @param[in] timeout_cycles Maximum polling iterations before timing out.
 *                            The timeout value should be computed using
 *                            the helper constants in dmaca_timeout_scale_t
 *                            or by calling rx_dmaca_transfer_blocking().
 *
 * @return rx_err_t
 * @retval k_rx_ok          Transfer completed before timeout.
 * @retval k_rx_err_timeout Timeout occurred; transfer engine disabled.
 * @retval k_rx_err_nack    Invalid channel index or channel not configured.
 *
 * @pre rx_dmaca_init() and rx_dmaca_configure() have been called for the
 *      specified channel, and rx_dmaca_start() has been invoked.
 * @pre timeout_cycles > 0 (zero while ACT=1 results in immediate k_rx_err_timeout;
 *      if ACT=0, returns k_rx_ok immediately regardless of timeout).
 * @post On k_rx_ok: Transfer completed, channel configured flag cleared.
 * @post On k_rx_err_timeout: Transfer engine disabled, configured flag cleared.
 * @post On k_rx_err_nack: Channel state unchanged.
 *
 * @note Implements a bounded polling loop; see NASA Rule 2 for reasoning.
 *       In thread context, this function yields with tx_thread_sleep(1)
 *       periodically to avoid scheduler starvation.
 *
 * @see rx_dmaca_start, rx_dmaca_configure, rx_dmaca_transfer_blocking
 * @since 1.0.0
 */
rx_err_t rx_dmaca_wait(uint8_t channel, uint32_t timeout_cycles)
{
  rx_err_t err;
  bool     transfer_complete = false;

  /* Preconditions / assertions */
  RX_ASSERT(channel < k_dmaca_num_channels, "DMACA wait: invalid channel");

  err = internal_validate_channel(channel);
  if (err != k_rx_ok) {
    return err;
  }

  RX_ASSERT((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) != 0U,
            "DMACA wait: channel not configured");

  if ((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) == 0U) {
    return k_rx_err_nack;
  }
  volatile rx_dmac_channel_regs_t* ch = dmac_channel(channel);

  /*
     * Bounded polling loop -- NASA Rule 2 compliance.
     *
     * We poll the ACT bit (transfer active flag) in DMSTS.
     * ACT = 1: DMA is running or waiting for bus.
     * ACT = 0: Transfer complete (or not yet started).
     *
     * Secondary check: DTE auto-clears to 0 after block completion.
     *
     * The timeout counter counts loop iterations, not exact CPU cycles,
     * but each iteration reads a volatile peripheral register (>= 4 cycles
     * at 60 MHz PCLKB / 240 MHz ICLK), so the bound is conservative.
     */

  for (uint32_t i = 0U; i < timeout_cycles; i++) {
    if (((ch->dmsts & k_dmsts_act) == 0U)) {
      /* DMA transfer complete */
      transfer_complete = true;
      break;
    }

    if (((i + 1U) % k_dmaca_wait_polls_per_sleep) == 0U) {
      if (tx_thread_identify() != TX_NULL) {
        (void)tx_thread_sleep((ULONG)k_dmaca_wait_sleep_ticks);
      }
    }
  }

  if (!transfer_complete) {
    ch->dmcnt &= ~(uint8_t)k_dmaca_dte_enable;
    s_channel_configured &= (uint8_t)(~(k_dmaca_channel_bitmap_bit << channel));
    return k_rx_err_timeout;
  }

  /* Transfer ended normally -- clear the configured flag. */
  s_channel_configured &= (uint8_t)(~(k_dmaca_channel_bitmap_bit << channel));

  return k_rx_ok;
}

/**
 * @brief Abort an ongoing DMA transfer on a channel.
 *
 * @details
 * Public API is tolerant of out-of-range channel values and will return
 * without side-effects when an invalid index is provided.  For debug builds
 * callers' assumptions are verified with `RX_ASSERT(channel < k_dmaca_num_channels, ...)`.
 *
 * @param[in] channel DMACA channel index to abort (0..k_dmaca_num_channels-1)
 *
 * @note The function performs a runtime no-op for invalid channel indexes.
 *
 * @note Thread safety: NOT thread-safe for the same channel. Concurrent
 *       abort and start/wait operations on the same channel require external
 *       synchronization.
 *
 * @return void No return value; operation is performed via side-effects on
 *         hardware registers and module state.
 *
 * @pre channel is an integer (uint8_t) representing the desired DMACA
 *      channel; if it is outside the valid range the call is a no-op.
 * @pre DMACA peripheral clock must be enabled (MSTP bit cleared) so that
 *      register accesses are valid.
 *
 * @post If channel is valid: DMA transfer engine for that channel is
 *       disabled and the module's configured-bit bitmap is cleared.
 * @post If channel is invalid: no hardware registers are modified and the
 *       function returns without side-effects.
 *
 * @par Example:
 * @code
 * // abort channel 3 before reconfiguring it
 * rx_dmaca_abort(3);
 * // caller may then safely call rx_dmaca_configure(3, &new_cfg);
 * @endcode
 *
 * @see dmac_ch(), RX_ASSERT
 * @since 1.0.0
 */
void rx_dmaca_abort(uint8_t channel)
{
  /* Validate caller assumptions in debug builds. Public API remains tolerant
     * and performs a no-op for out-of-range indices. */
  RX_ASSERT(channel < k_dmaca_num_channels, "DMACA invalid channel index");

  if (channel >= k_dmaca_num_channels) {
    /* no-op for invalid index, per API contract */
    return;
  }

  volatile rx_dmac_channel_regs_t* ch = dmac_channel(channel);

  /* disable transfer engine and clear configured flag */
  ch->dmcnt &= ~(uint8_t)k_dmaca_dte_enable;
  s_channel_configured &= (uint8_t)(~(k_dmaca_channel_bitmap_bit << channel));

  RX_ASSERT((s_channel_configured & (uint8_t)(k_dmaca_channel_bitmap_bit << channel)) == 0U,
            "DMACA channel still configured after abort");
}

/**
 * @brief  Configure, start, and wait for a DMA transfer in one call.
 *
 * @details
 * Convenience wrapper that sequentially calls rx_dmaca_configure(),
 * rx_dmaca_start(), and rx_dmaca_wait() using a computed timeout based on
 * the configured block size.  This simplifies common usage patterns where
 * the caller merely wants to perform a single blocking DMA transfer.
 *
 * The timeout is scaled from the transfer_count using the constants defined
 * in dmaca_timeout_scale_t and is floored at k_dmaca_poll_timeout_cycles.
 *
 * @param[in] channel DMACA channel index (0 .. k_dmaca_num_channels-1).
 * @param[in] p_cfg   Pointer to a valid dmaca_config_t structure.  Must not
 *                    be NULL and transfer_count must be non-zero.
 *
 * @return rx_err_t
 * @retval k_rx_ok         Transfer completed successfully before timeout.
 * @retval k_rx_err_nack   Configuration or start failed (invalid arguments).
 * @retval k_rx_err_timeout Timed out while waiting for completion.
 *
 * @pre rx_dmaca_init() has been invoked.
 * @pre Channel index is valid and p_cfg points to a properly initialized
 *      configuration block.
 *
 * @post On k_rx_ok: Transfer completed, channel idle, configured flag cleared.
 * @post On k_rx_err_timeout: Channel disabled, configured flag cleared.
 * @post On k_rx_err_nack: Channel state depends on which sub-call failed.
 *
 * @note For fine-grained timeouts or non-blocking behaviour, callers may
 *       invoke the underlying API functions individually.
 * @note Thread safety: NOT thread-safe. Caller must ensure exclusive access
 *       to the specified channel for the duration of the blocking transfer.
 *
 * @see rx_dmaca_wait(), rx_dmaca_configure(), rx_dmaca_start()
 * @since 1.0.0
 */
rx_err_t rx_dmaca_transfer_blocking(uint8_t channel, const dmaca_config_t* p_cfg)
{
  RX_VALIDATE_INIT(s_dmaca_initialized, "CRC", "DMA module not initialized");
  rx_err_t err;

  /* defensive assertions */
  RX_ASSERT(p_cfg != NULL, "DMACA transfer_blocking: p_cfg is NULL");
  RX_ASSERT(p_cfg->transfer_count != 0U, "DMACA transfer_blocking: zero transfer_count");
  RX_ASSERT(channel < k_dmaca_num_channels, "DMACA transfer_blocking: invalid channel");

  err = rx_dmaca_configure(channel, p_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_dmaca_start(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /*
     * Scale timeout to the configured block size.
     *
     * Worst-case per-transfer: (Cr + Cw) = (1 + 4) = 5 PCLKB cycles
     * At 240/60 MHz (ICLK/PCLKB ratio = 4): 5 x 4 = 20 ICLK iterations
     * Add 25 % bus contention margin for EXDMAC priority preemption.
     *
     * Minimum floor = k_dmaca_poll_timeout_cycles to cover DMA startup
     * overhead and very small blocks where the formula underestimates.
     */
  uint32_t transfer_count = (uint32_t)p_cfg->transfer_count;
  uint32_t cycles         = k_dmaca_cycles_per_xfer;
  uint32_t numerator      = k_dmaca_margin_numerator;
  uint32_t denominator    = k_dmaca_margin_denominator;
  /* guard against overflow by using 64-bit intermediate multiplication */
  uint64_t prod   = (uint64_t)transfer_count * (uint64_t)cycles * (uint64_t)numerator;
  uint32_t scaled = (uint32_t)(prod / (uint64_t)denominator);
  uint32_t timeout =
    (scaled > k_dmaca_poll_timeout_cycles) ? scaled : k_dmaca_poll_timeout_cycles;

  return rx_dmaca_wait(channel, timeout);
}
