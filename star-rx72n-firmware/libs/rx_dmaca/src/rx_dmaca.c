/**
 * @file rx_dmaca.c
 * @brief RX72N DMAC Polling-Mode Driver Implementation
 *
 * @details
 * Implements polling-mode DMA transfers for the RX72N DMAC peripheral.
 * Used by rx_crc to offload byte-stream writes to CRCDIR.
 *
 * Hardware access uses the register accessors from rx72n_dmac_regs.h.
 * In test builds (UNIT_TEST), these accessors return pointers to mock state
 * (tests/mocks/rx72n_dmac_regs.h shadows the real header), allowing full
 * logic testing without RX72N hardware.
 *
 * @see rx_dmaca.h Public API
 * @see rx72n_dmac_regs.h Register definitions
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_dmaca.h"

#include <stddef.h>

#ifdef UNIT_TEST
#include "mock_rx72n_regs.h"
#else
#include "rx72n_regs.h"
#endif
#include "rx_check.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/**
 * @enum rx_dmaca_internal_constants_t
 * @brief Internal constants for DMAC driver
 *
 * @details Abort timeout and DMA length constraints.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Abort poll iterations before timeout (fixed internal limit) */
  k_dmaca_abort_timeout_cycles = 10000U,

  /** @brief Maximum DMA transfer length (DMCRA is 16-bit) */
  k_dmaca_len_max = 65535U,

  /**
   * @brief Maximum caller-supplied timeout_cycles for internal_poll_act()
   *
   * @details
   * Provides a statically-provable upper bound on the internal_poll_act() loop
   * so that NASA Rule 2 (bounded loops) is satisfied even though the loop bound
   * is a runtime parameter. Callers are validated against this limit in
   * rx_dmaca_transfer_poll() before the loop executes.
   */
  k_dmaca_timeout_cycles_max = 10000000U,

} rx_dmaca_internal_constants_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

/** @brief Log tag for this module */
static const char* const s_tag = "DMAC";

/** @brief Module initialization flag */
static bool s_dmaca_initialized = false;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Configure DMAC channel registers for a transfer
 *
 * @details
 * Sets DMSAR, DMDAR, DMCRA, DMTMD, DMAMD, and DMINT for one polling transfer.
 * Transfer size is always 8-bit (DMTMD.SZ=00b) for CRCDIR compatibility.
 *
 *
 * @pre config is not NULL and all fields are valid
 * @pre rx_dmaca_init() has been called
 * @post Channel registers configured for transfer
 * @post DMINT = 0 (no interrupts in polling mode)
 *
 * @note Not thread-safe on the same channel
 * @since Version 1.0.0
 */
static void internal_configure_channel(const rx_dmaca_config_t* config)
{
  volatile rx_dmac_channel_regs_t* ch = dmac_channel(config->channel);

  /* Source: increment after each read; Destination: fixed (CRCDIR) */
  ch->dmsar = (uint32_t)(uintptr_t)config->src;
  ch->dmdar = (uint32_t)config->dst_addr;
  ch->dmcra = config->len; /* Normal mode: lower 16 bits = transfer count */
  ch->dmtmd = (uint16_t)(k_dmtmd_md_normal | k_dmtmd_dctg_software | k_dmtmd_sz_8bit);
  ch->dmamd = (uint16_t)(k_dmamd_sm_increment | k_dmamd_dm_fixed);
  ch->dmint = k_dmint_disabled; /* No interrupts in polling mode */
}

/**
 * @brief Poll DMSTS.ACT until clear or timeout
 *
 * @details
 * Loops up to timeout_cycles times checking if the DMA ACT flag
 * (transfer in progress) is clear. Returns k_rx_ok on success,
 * k_rx_err_timeout if the loop exhausts without ACT clearing.
 *
 *
 *
 * @pre channel < k_dmac_channel_count
 * @pre timeout_cycles > 0 and <= k_dmaca_timeout_cycles_max (enforced by caller)
 * @post On k_rx_ok: DMSTS.ACT = 0
 * @post On k_rx_err_timeout: DMSTS.ACT may still be set; caller must clear DTE
 *
 * @par Activity Diagram:
 * @startuml
 * start
 * :internal_poll_act(channel, timeout_cycles);
 * note right
 *   timeout_cycles pre-validated in [1, k_dmaca_timeout_cycles_max]
 *   -- provides NASA Rule 2 statically-bounded loop
 * end note
 * :i = 0;
 * while (i < timeout_cycles?) is (yes)
 *   :read DMSTS.ACT for channel;
 *   if (ACT == 0?) then (yes)
 *     :write DMSTS = 0 (clear DTIF flag);
 *     :return k_rx_ok;
 *     stop
 *   endif
 *   :i++;
 * endwhile (no -- exhausted)
 * :return k_rx_err_timeout;
 * stop
 * @enduml
 *
 * @note Not thread-safe on the same channel
 * @since Version 1.0.0
 */
static rx_err_t internal_poll_act(uint8_t channel, uint32_t timeout_cycles)
{
  for (uint32_t i = 0U; i < timeout_cycles; i++) {
    if ((dmac_channel(channel)->dmsts & k_dmsts_act) == 0U) {
      dmac_channel(channel)->dmsts = 0U; /* Clear DTIF flag after ACT=0 */
      return k_rx_ok;
    }
  }
  return k_rx_err_timeout;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the RX72N DMAC peripheral
 *
 * @details
 * Unlocks PRCR, enables the DMAC module clock (clears MSTPCRA bit 28), starts
 * the DMAC module (sets DMAST.DMST = 1), and marks the driver as initialized.
 * Returns k_rx_err_invalid_state on double-init without touching hardware.
 *
 *
 * @pre DMAC must not already be initialized (s_dmaca_initialized == false)
 * @pre System clock (PCLKA) must be configured and running
 * @post s_dmaca_initialized set to true
 * @post DMAC module clock enabled and DMAST.DMST = 1
 *
 * @note Not thread-safe; call during single-threaded system initialization
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_init(void)
{
  /* Pre-condition: detect double-init (NASA Rule 5) */
  if (s_dmaca_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Enable DMAC module clock (clear MSTPCRA bit 28) */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcra &= ~k_dmac_mstpcra_bit;
  *prcr_reg() = k_rx_prcr_lock;

  /* Start DMAC module (DMAST.DMST = 1) */
  *dmac_dmast_reg() = k_dmast_dmst;

  /* Post-condition: module operational */
  s_dmaca_initialized = true;
  return k_rx_ok;
}

/**
 * @brief Perform a polled DMA transfer and block until complete or timeout
 *
 * @details
 * Validates all inputs, configures the DMA channel registers via
 * internal_configure_channel(), starts the transfer (DTE=1, SWREQ=1),
 * and polls the ACT bit until completion or timeout. Clears DTE on exit
 * regardless of outcome.
 *
 *
 *
 * @pre rx_dmaca_init() must have been called successfully
 * @pre config->src must point to at least config->len bytes of readable memory
 * @post DMA channel DTE cleared (channel idle) regardless of return value
 * @post On k_rx_ok, destination memory (config->dst_addr) contains transferred data
 *
 * @par Activity Diagram:
 * @startuml
 * start
 * :rx_dmaca_transfer_poll(&config);
 * if (config == NULL?) then (yes)
 *   :return k_rx_err_null_ptr;
 *   stop
 * endif
 * if (not initialized?) then (yes)
 *   :return k_rx_err_not_initialized;
 *   stop
 * endif
 * if (channel >= 8?) then (yes)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endif
 * if (len == 0 or len > 65535?) then (yes)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endif
 * if (timeout == 0 or timeout > max?) then (yes)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endif
 * if (dst_addr == 0?) then (yes)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endif
 * if (src == NULL?) then (yes)
 *   :return k_rx_err_null_ptr;
 *   stop
 * endif
 * :internal_configure_channel(&config);
 * :DMCNT.DTE = 1 (enable channel);
 * :DMREQ = SWREQ|CLRS (trigger + auto-clear);
 * :internal_poll_act(channel, timeout_cycles);
 * :DMCNT.DTE = 0 (always clear on exit);
 * :return poll result (k_rx_ok or k_rx_err_timeout);
 * stop
 * @enduml
 *
 * @note Not thread-safe; concurrent calls on the same channel corrupt the transfer
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config)
{
  /* Pre-conditions: validate all inputs (NASA Rule 5) */
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if (!s_dmaca_initialized) {
    return k_rx_err_not_initialized;
  }

  if (config->channel >= k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (config->len == 0U || config->len > k_dmaca_len_max) {
    return k_rx_err_invalid_arg;
  }

  if (config->timeout_cycles == 0U || config->timeout_cycles > k_dmaca_timeout_cycles_max) {
    return k_rx_err_invalid_arg;
  }

  if (config->dst_addr == 0U) {
    return k_rx_err_invalid_arg;
  }

  RX_CHECK_NULL_PTR(config->src, s_tag, "src pointer is NULL");

  /* Configure channel registers */
  internal_configure_channel(config);

  /* DMA start sequence (mandatory order per HW manual section 18): */
  /* 1. DTE = 1 (channel enable) */
  dmac_channel(config->channel)->dmcnt = k_dmcnt_dte;

  /* 2. SWREQ = 1 + CLRS = 1 (trigger and auto-clear) */
  dmac_channel(config->channel)->dmreq = (uint8_t)(k_dmreq_swreq | k_dmreq_clrs);

  /* Poll until complete or timeout */
  rx_err_t ret = internal_poll_act(config->channel, config->timeout_cycles);

  /* Post-condition: clear DTE regardless of outcome (NASA Rule 5) */
  dmac_channel(config->channel)->dmcnt = 0U;

  return ret;
}

/**
 * @brief Abort an in-progress DMA transfer on the specified channel
 *
 * @details
 * Immediately clears DTE to disable the channel, then polls the ACT bit
 * until the active transfer drains or the internal abort timeout expires.
 * Returns k_rx_ok when the channel is confirmed idle.
 *
 *
 *
 * @pre rx_dmaca_init() must have been called successfully
 * @pre channel must be in [0, k_dmac_channel_count - 1]
 * @post DTE cleared for the specified channel
 * @post Channel ACT bit is 0 on k_rx_ok return
 *
 * @note Not thread-safe; do not call concurrently on the same channel
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_abort(uint8_t channel)
{
  /* Pre-conditions: validate inputs (NASA Rule 5) */
  if (!s_dmaca_initialized) {
    rx_log_error(s_tag, "DMAC not initialized");
    return k_rx_err_not_initialized;
  }

  if (channel >= k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }

  /* Clear DTE to disable channel immediately */
  dmac_channel(channel)->dmcnt = 0U;

  /* Wait for active transfer to drain (polls ACT bit) */
  return internal_poll_act(channel, k_dmaca_abort_timeout_cycles);
}

/**
 * @brief Deinitialize the RX72N DMAC peripheral
 *
 * @details
 * Clears DMAST.DMST (suspends all channels), unlocks PRCR, sets MSTPCRA
 * bit 28 to gate the DMAC module clock, re-locks PRCR, and marks the
 * driver as uninitialized. Returns k_rx_err_not_initialized if called
 * without a prior rx_dmaca_init().
 *
 *
 * @pre rx_dmaca_init() must have been called successfully
 * @pre No DMA transfers should be in progress (all channels idle)
 * @post s_dmaca_initialized set to false
 * @post DMAC module clock disabled (MSTPCRA bit 28 set)
 *
 * @note Not thread-safe; call during single-threaded system teardown
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_deinit(void)
{
  /* Pre-condition: must be initialized (NASA Rule 5) */
  if (!s_dmaca_initialized) {
    rx_log_error(s_tag, "DMAC not initialized");
    return k_rx_err_not_initialized;
  }

  /* Clear DMST to suspend all channels */
  *dmac_dmast_reg() = 0U;

  /* Re-enable module stop to gate DMAC clock (power saving) */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcra |= k_dmac_mstpcra_bit;
  *prcr_reg() = k_rx_prcr_lock;

  /* Post-condition: module stopped */
  s_dmaca_initialized = false;
  return k_rx_ok;
}
