/* libs/rx_hal/inc/rx_dmaca.h */

/**
 * @file rx_dmaca.h
 * @brief DMACA driver for RX72N
 *
 * @details
 * Provides DMA transfer abstraction for the RX72N DMACA peripheral supporting
 * 8-bit and 32-bit block transfers only.  16-bit transfers are explicitly
 * prohibited because the hardware does not support them.  All operations are
 * bounded by configurable timeout values to satisfy NASA Power of 10 Rule 2,
 * and no dynamic memory is allocated (Rule 3 compliance).  Initialization is
 * idempotent, channel operations require serialized caller access, and all
 * errors are reported via structured rx_err_t return codes.
 *
 * **Supported Transfer Sizes:**
 * - 8-bit (byte) transfers: k_dmaca_size_byte
 * - 32-bit (word) transfers: k_dmaca_size_longword
 *
 * **Prohibited:**
 * - 16-bit transfers: Hardware does not support; requests are rejected with
 *   k_rx_err_nack during rx_dmaca_configure()
 *
 * **Usage Model:**
 * Typical sequence: rx_dmaca_init() -> rx_dmaca_configure() -> rx_dmaca_start()
 * -> rx_dmaca_wait(), or use rx_dmaca_transfer_blocking() for synchronous
 * transfers. The rx_dmaca_abort() function is always safe (returns void) and
 * may be called during cleanup without explicit channel validation.
 *
 * **Limitations:**
 * - Maximum transfer_count: 1024 transfer units per block
 * - No interrupt support; polling only via rx_dmaca_wait()
 * - All channel access must be serialized by the caller
 * - Timeout values are iterative cycle counts (not wall-clock time)
 *
 * @par NASA Power of 10 Compliance
 * | Rule | Status | Enforcement |
 * |------|--------|-----------|
 * | 1    | [PASS] | No goto, setjmp/longjmp, or recursion |
 * | 2    | [PASS] | All loops bounded by timeout_cycles parameter |
 * | 3    | [PASS] | Static allocation only; zero malloc/free |
 * | 5    | [PASS] | Preconditions and postconditions on all functions |
 * | 10   | [PASS] | Compiles cleanly with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles
 * - **Single Responsibility:** DMACA channel management and transfer control only
 * - **Dependency Inversion:** Abstracts error codes via rx_err_t enum
 *
 * @see rx_dmaca.tex
 * @author STAR Team
 * @date 2026-02-22
 * @version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_err.h"

/* =========================================================================
 * Public constants
 * ========================================================================= 
 */

/**
 * @enum dmaca_hw_limit_t
 * @brief Hardware-imposed limits for DMACA peripheral.
 *
 * @details The DMACA peripheral exposes 8 fixed channels and a 10-bit
 * transfer-count field. These enumerators provide named constants for those
 * hardware limits, preventing magic numbers in validation and loop bounds.
 *
 * @invariant Values are hardware-defined and must not be modified.
 *
 * @code
 * // Example: validate transfer count is within hardware limits
 * if (requested_count > k_dmaca_max_transfer_count) {
 *     return k_rx_err_nack;  // Request exceeds 1024-unit maximum
 * }
 * // Example: iterate over all valid DMACA channels
 * for (uint8_t ch = 0; ch < k_dmaca_num_channels; ch++) {
 *     rx_dmaca_abort(ch);
 * }
 * @endcode
 *
 * @see rx_dmaca_configure, dmaca_config_t, validate_channel
 * @since 1.0.0
 */
typedef enum : uint16_t {
  k_dmaca_max_transfer_count = 1024U, /**< 10-bit DMTC field max (1..1024) */
} dmaca_hw_limit_t;

/* =========================================================================
 * Types
 * ========================================================================= 
 */

/**
 * @enum dmaca_data_size_t
 * @brief DMA transfer data width selectors
 *
 * @details
 * Determines the bus width used for each transfer unit.  Only 8-bit and
 * 32-bit modes are supported; 16-bit mode is deliberately excluded because
 * the CRCDIR peripheral does not support half-word accesses.  Using a
 * 16-bit value results in undefined CRC results.
 *
 * The selected value maps directly to the DMTMD.SZ[1:0] field in the
 * channel's transfer mode register.
 *
 * @invariant The value must be either k_dmaca_size_byte or
 *            k_dmaca_size_longword; 16-bit width is disallowed.
 *
 * @code
 *   dmaca_config_t cfg = {0};
 *   cfg.data_size = k_dmaca_size_byte;           // 8-bit transfers
 *   cfg.data_size = k_dmaca_size_longword;       // 32-bit transfers
 *   // cfg.data_size = 1; // [compile-time error] not a named enumerator
 * @endcode
 *
 * @see RX72N Hardware Manual Sec.46.2, DMTMD register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_size_byte     = 0U, /**< 8-bit  -- DMTMD.SZ[1:0] = 00b */
  k_dmaca_size_longword = 2U, /**< 32-bit -- DMTMD.SZ[1:0] = 10b */
} dmaca_data_size_t;

/**
 * @enum dmaca_addr_mode_t
 * @brief Address update mode for source or destination pointer
 *
 * @details
 * Determines how the peripheral updates the source or destination address
 * register (DMAMDx/DMADDx) after each transfer unit.  The value is encoded
 * in the DMAMD.xM[1:0] bits of the channel control register.  Fixed mode is
 * used for peripherals or FIFO registers where the address must remain
 * constant; increment/decrement are used for memory buffers in normal or
 * reverse order.
 *
 * @invariant Enumerators map directly to the two-bit field; only the three
 *            listed values are valid (0,2,3).  The underlying type is
 *            uint8_t to match the register width.
 *
 * @code
 *   dmaca_config_t cfg = {0};
 *   cfg.src_addr_mode = k_dmaca_addr_increment;
 *   cfg.dst_addr_mode = k_dmaca_addr_fixed;  // writing to CRCDIR
 *   rx_dmaca_configure(0, &cfg);
 * @endcode
 *
 * @see DMAMD, rx_dmaca_configure(), dmaca_config_t
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_addr_fixed     = 0U, /**< DMAMD.xM[1:0] = 00b  (register, FIFO) */
  k_dmaca_addr_increment = 2U, /**< DMAMD.xM[1:0] = 10b  (normal buffer)  */
  k_dmaca_addr_decrement = 3U, /**< DMAMD.xM[1:0] = 11b  (reverse buffer) */
} dmaca_addr_mode_t;

/**
 * @enum   dmaca_transfer_mode_t
 * @brief  DMA transfer mode controlling activation granularity.
 *
 * @details Encodes DMTMD.MD[1:0].  Block mode (k_dmaca_mode_block) is the
 *          required setting for CRC computation: the entire buffer is pushed
 *          to CRCDIR in a single activation pulse.
 *
 * @invariant Value must be one of the three named enumerators.
 *
 * @code
 *   dmaca_config_t cfg = {
 *       .p_src          = (const void *)buffer,
 *       .p_dst          = (void *)rx_dmaca_crcdir_reg_ptr(),
 *       .transfer_count = 256U,
 *       .data_size      = k_dmaca_size_byte,
 *       .transfer_mode  = k_dmaca_mode_block,
 *       .src_addr_mode  = k_dmaca_addr_increment,
 *       .dst_addr_mode  = k_dmaca_addr_fixed,
 *   };
 * @endcode
 *
 * @see dmaca_config_t
 * @since 1.0.0
 */
typedef enum : uint8_t {
  k_dmaca_mode_normal = 0U, /**< Normal  (1 unit per activation) */
  k_dmaca_mode_block  = 1U, /**< Block   (full block per activation) -- use for CRC */
  k_dmaca_mode_repeat = 2U, /**< Repeat  (auto-reload source/dest) */
} dmaca_transfer_mode_t;

/**
 * @struct dmaca_config_t
 * @brief  Complete configuration for one DMA channel transfer.
 *
 * @details
 * Encapsulates all parameters required by rx_dmaca_configure() to program
 * a single DMACA channel.  Fill this structure, then pass it to
 * rx_dmaca_configure() followed by rx_dmaca_start().  The struct uses
 * static allocation only (no pointers to dynamic memory) to comply with
 * NASA Power of 10 Rule 3.
 *
 * @invariant transfer_count in range [1, k_dmaca_max_transfer_count].
 * @invariant data_size must be k_dmaca_size_byte or k_dmaca_size_longword.
 * @invariant p_src and p_dst must be non-NULL and properly aligned.
 *
 * @code
 *   dmaca_config_t cfg = {
 *       .p_src          = (const void *)my_buffer,
 *       .p_dst          = (void *)rx_dmaca_crcdir_reg_ptr(),
 *       .transfer_count = 256U,
 *       .data_size      = k_dmaca_size_byte,
 *       .transfer_mode  = k_dmaca_mode_block,
 *       .src_addr_mode  = k_dmaca_addr_increment,
 *       .dst_addr_mode  = k_dmaca_addr_fixed,
 *   };
 *   rx_err_t err = rx_dmaca_configure(0U, &cfg);
 * @endcode
 *
 * @see rx_dmaca_configure, rx_dmaca_transfer_blocking
 * @since 1.0.0
 */
typedef struct {
  const void*           p_src;          /**< Source buffer address (read-only by DMA) */
  void*                 p_dst;          /**< Destination address (fixed for CRCDIR) */
  uint32_t              transfer_count; /**< Number of data units (1-1024) */
  dmaca_data_size_t     data_size;      /**< k_dmaca_size_byte or k_dmaca_size_longword ONLY */
  dmaca_transfer_mode_t transfer_mode;  /**< Use k_dmaca_mode_block for CRC */
  dmaca_addr_mode_t     src_addr_mode;  /**< Typically k_dmaca_addr_increment */
  dmaca_addr_mode_t     dst_addr_mode;  /**< k_dmaca_addr_fixed for CRCDIR */
} dmaca_config_t;

/* =========================================================================
 * Timeout configuration  (NASA Rule 2 -- bounded loops)
 * ========================================================================= 
*/

/**
 * @def RX_DMACA_POLL_TIMEOUT_CYCLES
 * @brief Worst-case execution time budget for DMA completion polling.
 *
 * @details
 * Derivation (8-bit, 256-byte block):
 *   Per-transfer cost = (Cr + Cw) = (1 + 4) = 5 PCLKB cycles
 *   At 240 MHz ICLK / 60 MHz PCLKB: 5 x 4 = 20 ICLK cycles per byte
 *   256 bytes x 20 cycles = 5,120 CPU cycles
 *   Bus contention margin (EXDMAC priority preemption): +25 % -> ~6,400
 *   Round up to nearest power of two with margin: 8,192
 *
 * Override with -DRX_DMACA_POLL_TIMEOUT_CYCLES=<n> in your build if you
 * have longer blocks or tighter real-time budgets.
 * @note This macro can be overridden at compile time using
 *       -DRX_DMACA_POLL_TIMEOUT_CYCLES=<value> for custom timing requirements.
 *
 * @since 1.0.0
 */
#ifndef RX_DMACA_POLL_TIMEOUT_CYCLES
#define RX_DMACA_POLL_TIMEOUT_CYCLES (8192U)
#endif

/* =========================================================================
 * API
 * ========================================================================= \
 */

/**
 * @brief  Enable DMACA module clock and perform one-time hardware init.
 *
 * @details Releases the DMACA module from the module-stop state by clearing
 *          MSTP(DMAC) in MSTPCRB, then disables all 8 channels and clears
 *          pending activation flags.  Idempotent if called more than once.
 *
 * @pre  System clock and bus configuration are complete.
 * @pre  No DMA transfer is in progress on any channel.
 *
 * @post DMACA module clock is enabled (MSTP(DMAC) == 0).
 * @post All 8 channels are disabled (DMCNT.DTE == 0 for channels 0-7).
 *
 * @return rx_err_t Status code indicating success (k_rx_ok) or failure;
 *         callers should check for k_rx_ok before proceeding.
 *
 * @retval k_rx_ok  Module enabled and all channels reset successfully.
 *
 * @note Thread safety: NOT re-entrant.  Call once from a single initialization
 *       context before any RTOS tasks that use DMACA are started.
 *
 * @code
 * rx_err_t err = rx_dmaca_init();
 * if (err != k_rx_ok) return err;  // Check initialization succeeded
 * @endcode
 *
 * @see rx_dmaca_configure, rx_dmaca_abort
 * @since 1.0.0
 */
rx_err_t rx_dmaca_init(void);

/**
 * @brief  Configure a single DMACA channel for a transfer.
 *
 * @details
 * Writes the supplied configuration into the channel's register set
 * (DMAMDx, DMSARx, DMDARx, DMCNTx, DMTMDx, etc.).  The values are
 * latched immediately by the hardware; no DMA activity is triggered by this
 * call.  The channel remains inactive (ACT == 0) until rx_dmaca_start()
 * is invoked.
 *
 * Does not start the transfer; call rx_dmaca_start() afterwards.
 *
 * @param[in] channel  Channel index 0-7.  Values >= k_dmaca_num_channels
 *                     result in k_rx_err_nack.
 * @param[in] p_cfg    Pointer to a const dmaca_config_t structure containing
 *                     the desired settings.  Must not be NULL.  transfer_count
 *                     is checked to be between 1 and k_dmaca_max_transfer_count
 *                     inclusive; unsupported data sizes (such as 16-bit)
 *                     result in k_rx_err_nack.
 *
 * @return rx_err_t Error status; k_rx_ok on success, k_rx_err_nack on invalid
 *         parameters.
 *
 * @retval k_rx_ok           Configuration accepted.
 * @retval k_rx_err_nack     channel >= k_dmaca_num_channels, p_cfg is NULL,
 *                           transfer_count is 0 or > 1024, or an unsupported
 *                           data_size was requested (e.g. 16-bit).
 *
 * @pre  rx_dmaca_init() has been called to enable the DMACA peripheral.
 * @pre  The specified channel is idle (ACT == 0) and not currently
 *       participating in an active transfer.
 *
 * @post The channel's DMACA registers reflect the contents of *p_cfg.
 * @post The channel remains disabled/stopped (ACT == 0, DTE == 0).
 *
 * @note Thread safety: callers must serialize accesses to the same channel.
 *       Configuring different channels concurrently is safe provided the
 *       caller ensures the writes to each channel's registers are atomic; a
 *       hardware bus collision or concurrent modification of the same channel
 *       may produce undefined behavior.  No internal locking is performed.
 *
 * @code
 * dmaca_config_t cfg = {...};  // Initialize configuration
 * rx_err_t err = rx_dmaca_configure(0, &cfg);
 * if (err != k_rx_ok) return err;  // Check configuration accepted
 * @endcode
 *
 * @see dmaca_config_t, rx_dmaca_start(), rx_dmaca_init()
 * @since 1.0.0
 */
rx_err_t rx_dmaca_configure(uint8_t channel, const dmaca_config_t* p_cfg);
/**
 * @brief  Arm and activate a previously configured channel.
 *
 * @details Sets DMCNT.DTE = 1 to arm the channel, then writes DMREQ.SWREQ = 1
 *          to issue a software-trigger pulse.  The DMA controller begins
 *          transferring data immediately on the next bus cycle.
 *
 * @param[in] channel  Channel index 0-7.
 *
 * @pre  rx_dmaca_configure() has been called successfully for this channel.
 * @pre  The channel is idle (ACT == 0) -- do not re-start a running transfer.
 *
 * @post DMCNT.DTE == 1 and DMREQ.SWREQ has been pulsed.
 * @post DMA transfer is in progress on the specified channel.
 *
 * @return rx_err_t Status of the start operation; k_rx_ok if the transfer was
 *         successfully armed, or k_rx_err_nack if the channel was invalid or
 *         not configured.
 *
 * @retval k_rx_ok        Transfer started.
 * @retval k_rx_err_nack  Channel not previously configured or invalid index.
 *
 * @note Thread safety: NOT safe to call concurrently on the same channel.
 *
 * @code
 * rx_err_t err = rx_dmaca_start(0);
 * if (err != k_rx_ok) return err;  // Check transfer armed successfully
 * @endcode
 *
 * @see rx_dmaca_configure, rx_dmaca_wait, rx_dmaca_abort
 * @since 1.0.0
 */
rx_err_t rx_dmaca_start(uint8_t channel);

/**
 * @brief  Poll until the channel is idle or the timeout expires.
 *
 * @details
 * This routine repeatedly reads the channel status register until the
 * transfer active flag (ACT) and the transfer enable bit (DTE) both clear,
 * indicating the channel has become idle.  The loop is bounded by
 * `timeout_cycles` to satisfy NASA Rule 2; each iteration reads a volatile
 * register so the bound corresponds to a conservative CPU cycle count.
 *
 * @param[in] channel        Channel index 0-7.
 * @param[in] timeout_cycles Maximum number of polling iterations before
 *                           declaring a timeout.  Use
 *                           RX_DMACA_POLL_TIMEOUT_CYCLES or scale to your
 *                           block size.
 *
 * @return rx_err_t Status of the wait operation.  Callers receive k_rx_ok if
 *         the channel became idle before the timeout, or an error code
 *         otherwise (k_rx_err_timeout for a timeout, k_rx_err_nack for an
 *         invalid channel index).  See RX_DMACA_POLL_TIMEOUT_CYCLES for a
 *         recommended timeout value and refer to rx_dmaca_start and
 *         rx_dmaca_abort for related usage context.
 *
 * @retval k_rx_ok           Transfer completed normally (ACT == 0 && DTE == 0).
 * @retval k_rx_err_timeout  Timeout expired before transfer completed.
 * @retval k_rx_err_nack    Invalid channel index or channel not configured.
 *
 * @pre  rx_dmaca_start() has been called successfully for this channel.
 * @pre  timeout_cycles > 0.
 *
 * @post On success: channel idle (ACT == 0 && DTE == 0).
 * @post On timeout: channel is disabled and configured state is cleared
 *       before returning k_rx_err_timeout.
 *
 * @note Thread safety: NOT safe to call concurrently on the same channel.
 *
 * @code
 * rx_err_t err = rx_dmaca_wait(0, RX_DMACA_POLL_TIMEOUT_CYCLES);
 * if (err != k_rx_ok) rx_dmaca_abort(0);  // Abort on timeout or error
 * @endcode
 *
 * @see rx_dmaca_start, rx_dmaca_abort, RX_DMACA_POLL_TIMEOUT_CYCLES
 * @since 1.0.0
 */
rx_err_t rx_dmaca_wait(uint8_t channel, uint32_t timeout_cycles);
/**
 * @brief  Unconditionally abort and disable a channel.
 *
 * @details
 * Disables the DMA transfer engine (DTE bit) and clears the internal
 * configured-channel bitmap for the specified channel.  This function has a
 * **void** return type by design: it is always safe to call, even with an
 * idle or never-configured channel.  Passing an out-of-range channel index
 * results in a deliberate no-op rather than an error return, so callers may
 * invoke it during cleanup without additional range checks.
 *
 * @param[in] channel  Channel index 0-7.  Values >= k_dmaca_num_channels are
 *                     silently ignored.
 *
 * @pre  rx_dmaca_init() has been called and DMACA module clock is enabled.
 * @pre  Caller has exclusive access to the specified channel (no concurrent ops).
 *
 * @post The specified channel is idle and its configured flag is cleared.
 * @post DMCNT.DTE == 0 for the specified channel.
 *
 * @note Thread safety: NOT safe to call concurrently on the same channel.
 *
 * @warning Because the signature returns void, callers cannot detect misuse
 *          and should ensure the channel argument is valid if subsequent
 *          logic depends on the abort having taken effect.
 *
 * @code
 * rx_dmaca_abort(0);  // Always safe; no-op if channel invalid or idle
 * @endcode
 *
 * @see rx_dmaca_configure, rx_dmaca_wait, rx_dmaca_start
 * @since 1.0.0 (void signature and permissive no-op contract introduced)
 */
void rx_dmaca_abort(uint8_t channel);

/**
 * @brief  Configure, start, and wait for a DMA transfer in a single call.
 *
 * @details
 * This helper convenience function chains together the core routines:
 * rx_dmaca_configure(), rx_dmaca_start(), and rx_dmaca_wait().  It
 * configures the specified channel, immediately arms and triggers it, then
 * polls for completion using a timeout that is scaled to the configured
 * transfer_count to keep the polling loop bounded.  The channel is left
 * disabled on any error.
 *
 * @param[in] channel  Channel index 0-7.
 * @param[in] p_cfg    Pointer to transfer configuration; must not be NULL.
 * @return rx_err_t Status code for the combined configure/start/wait
 *         operation; see the @retval entries below for the specific meanings
 *         of each returned value.
 *
 * @retval k_rx_ok           Transfer completed successfully.
 * @retval k_rx_err_timeout  Timeout expired while waiting for completion.
 *                           Channel is disabled before returning.
 * @retval k_rx_err_nack     Invalid channel index, NULL/invalid p_cfg, or
 *                           configuration rejected (propagated from
 *                           rx_dmaca_configure()).
 *                           This value may propagate from rx_dmaca_configure().
 *
 * @pre  channel < k_dmaca_num_channels
 * @pre  p_cfg != NULL
 * @pre  rx_dmaca_init() has been called and DMACA is enabled.
 *
 * @post On success: channel idle (ACT == 0 && DTE == 0).
 * @post On error/timeout: channel disabled and configured flag cleared.
 *
 * @note Timeout scaling behaviour ensures a bounded loop by deriving timeout
 *       from p_cfg->transfer_count and capping it with
 *       RX_DMACA_POLL_TIMEOUT_CYCLES. Not reentrant for the same channel;
 *       callers must serialize access per channel.
 *
 * @code
 * dmaca_config_t cfg = {...};  // Initialize configuration
 * rx_err_t err = rx_dmaca_transfer_blocking(0, &cfg);
 * if (err != k_rx_ok) rx_dmaca_abort(0);  // Cleanup on any error
 * @endcode
 *
 * @see rx_dmaca_configure, rx_dmaca_start, rx_dmaca_wait, rx_err_t
 * @since 1.0.0
 */
rx_err_t rx_dmaca_transfer_blocking(uint8_t channel, const dmaca_config_t* p_cfg);

#ifdef __cplusplus
}
#endif