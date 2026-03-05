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

#include <stdbool.h>
#include <stddef.h>

#include "rx72n_regs.h"
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

} rx_dmaca_internal_constants_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

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
 * @param[in] config Validated transfer configuration
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
  ch->dmtmd = (uint16_t)((uint16_t)k_dmtmd_md_normal | (uint16_t)k_dmtmd_dctg_software |
                         (uint16_t)k_dmtmd_sz_8bit);
  ch->dmamd = (uint16_t)((uint16_t)k_dmamd_sm_increment | (uint16_t)k_dmamd_dm_fixed);
  ch->dmint = (uint8_t)k_dmint_disabled; /* No interrupts in polling mode */
}

/**
 * @brief Poll DMSTS.ACT until clear or timeout
 *
 * @details
 * Loops up to timeout_cycles times checking if the DMA ACT flag
 * (transfer in progress) is clear. Returns k_rx_ok on success,
 * k_rx_err_timeout if the loop exhausts without ACT clearing.
 *
 * @param[in] channel Channel number (0-7, pre-validated)
 * @param[in] timeout_cycles Maximum iterations (> 0, pre-validated)
 *
 * @return rx_err_t
 * @retval k_rx_ok ACT cleared (transfer complete)
 * @retval k_rx_err_timeout ACT still set after timeout_cycles iterations
 *
 * @pre channel < k_dmac_channel_count
 * @pre timeout_cycles > 0
 * @post On k_rx_ok: DMSTS.ACT = 0
 *
 * @note Not thread-safe on the same channel
 * @since Version 1.0.0
 */
static rx_err_t internal_poll_act(uint8_t channel, uint32_t timeout_cycles)
{
  for (uint32_t i = 0U; i < timeout_cycles; i++) {
    if ((dmac_channel(channel)->dmsts & (uint8_t)k_dmsts_act) == 0U) {
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
  *dmac_dmast_reg() = (uint8_t)k_dmast_dmst;

  /* Post-condition: module operational */
  s_dmaca_initialized = true;
  return k_rx_ok;
}

rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config)
{
  /* Pre-conditions: validate all inputs (NASA Rule 5) */
  RX_CHECK_NULL_PTR(config, "DMAC", "config pointer is NULL");

  if (!s_dmaca_initialized) {
    return k_rx_err_not_initialized;
  }

  if (config->channel >= (uint8_t)k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (config->len == 0U || config->len > k_dmaca_len_max) {
    return k_rx_err_invalid_arg;
  }

  if (config->timeout_cycles == 0U) {
    return k_rx_err_invalid_arg;
  }

  RX_CHECK_NULL_PTR(config->src, "DMAC", "src pointer is NULL");

  /* Configure channel registers */
  internal_configure_channel(config);

  /* DMA start sequence (mandatory order per HW manual section 18): */
  /* 1. DTE = 1 (channel enable) */
  dmac_channel(config->channel)->dmcnt = (uint8_t)k_dmcnt_dte;

  /* 2. SWREQ = 1 + CLRS = 1 (trigger and auto-clear) */
  dmac_channel(config->channel)->dmreq = (uint8_t)((uint8_t)k_dmreq_swreq | (uint8_t)k_dmreq_clrs);

  /* Poll until complete or timeout */
  rx_err_t ret = internal_poll_act(config->channel, config->timeout_cycles);

  /* Post-condition: clear DTE regardless of outcome (NASA Rule 5) */
  dmac_channel(config->channel)->dmcnt = 0U;

  return ret;
}

rx_err_t rx_dmaca_abort(uint8_t channel)
{
  /* Pre-conditions: validate inputs (NASA Rule 5) */
  if (!s_dmaca_initialized) {
    rx_log_error("DMAC", "DMAC not initialized");
    return k_rx_err_not_initialized;
  }

  if (channel >= (uint8_t)k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }

  /* Clear DTE to disable channel immediately */
  dmac_channel(channel)->dmcnt = 0U;

  /* Wait for active transfer to drain (polls ACT bit) */
  return internal_poll_act(channel, k_dmaca_abort_timeout_cycles);
}

rx_err_t rx_dmaca_deinit(void)
{
  /* Pre-condition: must be initialized (NASA Rule 5) */
  if (!s_dmaca_initialized) {
    rx_log_error("DMAC", "DMAC not initialized");
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
