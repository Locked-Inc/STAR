/**
 * @file rx_poeg.c
 * @brief POEG Motor Fault Protection Driver Implementation
 *
 * @details
 * Implements hardware-level emergency PWM output disable for the STAR
 * motor controller. Each of the 4 DRV8263H motor drivers has its nFAULT
 * output connected to a GTETRG pin, which feeds a POEG group. When a
 * fault is detected, POEG immediately disables GPTW PWM output (Hi-Z).
 *
 * POEGGA/B/C/D are sub-sources of the shared GROUPBL2 vector (107) on
 * RX72N (HW manual R01UH0824EJ0111 Section 15.3, Table 15.4). A single
 * GROUPBL2 ISR reads GRPBL2, dispatches to each faulted group, and:
 * 1. Clears the ICU GROUPBL2 IR flag
 * 2. Logs which POEG groups asserted
 * 3. Triggers e-stop via shared_data
 *
 * @par Implementation Notes
 * - POEG PIDE (pin detection enable) is write-once after reset
 * - nFAULT is active-low, so INV bit is set to detect low-level
 * - Noise filter enabled with PCLKB/8 sampling (~400ns at 60MHz)
 * - GTINTAD links each GPTW channel to its corresponding POEG group
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_poeg.h"

#include "rx72n_gptw_regs.h"
#include "rx72n_icu_regs.h"
#include "rx72n_poeg_regs.h"
#include "rx_check.h"
#include "rx_err.h"
#include "rx_log.h"
#include "shared_data.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Module log tag */
static const char* s_tag = "POEG";

/**
 * @enum poeg_icu_constants_t
 * @brief ICU configuration constants for POEG interrupts
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_icu_ir_clear        = 0, /**< Write 0 to clear IR flag */
  k_ier_bit_enable_base = 1, /**< Base for IER bit shift */
  k_ier_bits_per_reg    = 8, /**< 8 bits per IER register */
} poeg_icu_constants_t;

/**
 * @enum poeg_init_config_t
 * @brief POEG initialization configuration values
 *
 * @details
 * Combined register value for each POEG group initialization:
 * - PIDE = 1 (enable pin detection)
 * - INV = 1 (invert for active-low nFAULT)
 * - NFEN = 1 (enable noise filter)
 * - NFCS = 01 (PCLKB/8, ~400ns sampling)
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_poeg_init_value =
    (k_poeg_pide_enable | k_poeg_inv_invert | k_poeg_nfen_enable | k_poeg_nfcs_pclkb_div8),
} poeg_init_config_t;

/**
 * @enum poeg_gtintad_values_t
 * @brief Pre-computed GTINTAD values for each GPTW-POEG link
 *
 * @details
 * Each GPTW channel's GTINTAD register is configured to:
 * - Select the corresponding POEG group (A/B/C/D)
 * - Enable dead-time error detection (GRPDTE)
 * - Enable simultaneous low output detection (GRPABL)
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_gtintad_motor_0 = (k_gptw_gtintad_grp_a | k_gptw_gtintad_grpdte | k_gptw_gtintad_grpabl),
  k_gtintad_motor_1 = (k_gptw_gtintad_grp_b | k_gptw_gtintad_grpdte | k_gptw_gtintad_grpabl),
  k_gtintad_motor_2 = (k_gptw_gtintad_grp_c | k_gptw_gtintad_grpdte | k_gptw_gtintad_grpabl),
  k_gtintad_motor_3 = (k_gptw_gtintad_grp_d | k_gptw_gtintad_grpdte | k_gptw_gtintad_grpabl),
} poeg_gtintad_values_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief POEG group register accessor lookup table (indexed by motor) */
static volatile rx_poegg_regs_t* const s_poeg_groups[k_poeg_motor_count] = {
  (volatile rx_poegg_regs_t*)k_poegga_base_addr,
  (volatile rx_poegg_regs_t*)k_poeggb_base_addr,
  (volatile rx_poegg_regs_t*)k_poeggc_base_addr,
  (volatile rx_poegg_regs_t*)k_poeggd_base_addr,
};

/** @brief GPTW channel register accessor lookup table (indexed by motor) */
static volatile rx_gptw_channel_regs_t* const s_gptw_channels[k_poeg_motor_count] = {
  (volatile rx_gptw_channel_regs_t*)k_gptw0_base_addr,
  (volatile rx_gptw_channel_regs_t*)k_gptw1_base_addr,
  (volatile rx_gptw_channel_regs_t*)k_gptw2_base_addr,
  (volatile rx_gptw_channel_regs_t*)k_gptw3_base_addr,
};

/** @brief GTINTAD values lookup table (indexed by motor) */
static const uint32_t s_gtintad_values[k_poeg_motor_count] = {
  k_gtintad_motor_0,
  k_gtintad_motor_1,
  k_gtintad_motor_2,
  k_gtintad_motor_3,
};

/** @brief GRPBL2 sub-source bit for each POEG group (indexed by motor) */
static const uint8_t s_poeg_grpbl2_bits[k_poeg_motor_count] = {
  k_poeg_grpbl2_bit_poeggai,
  k_poeg_grpbl2_bit_poeggbi,
  k_poeg_grpbl2_bit_poeggci,
  k_poeg_grpbl2_bit_poeggdi,
};

/** @brief Accessors for the GROUPBL2 status/enable registers */
static inline volatile uint32_t* poeg_grpbl2_reg(void)
{
  return (volatile uint32_t*)k_poeg_grpbl2_addr;
}

static inline volatile uint32_t* poeg_genbl2_reg(void)
{
  return (volatile uint32_t*)k_poeg_genbl2_addr;
}

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Configure ICU for the shared POEG GROUPBL2 vector
 *
 * @param[in] priority IPR priority level (1-15)
 *
 * @pre ICU registers accessible
 * @post GROUPBL2 vector (107) enabled at specified priority with all four
 *       POEGG sub-sources unmasked in GENBL2.
 *
 * @since Version 1.0.0
 */
static void internal_enable_poeg_groupbl2_irq(uint8_t priority)
{
  volatile rx_icu_regs_t* icu_regs = icu();
  const uint16_t          vector   = k_poeg_irq_groupbl2_vector;

  /* Enable all four POEGG sub-sources in GENBL2 */
  *poeg_genbl2_reg() |=
    (uint32_t)((1UL << k_poeg_grpbl2_bit_poeggai) | (1UL << k_poeg_grpbl2_bit_poeggbi) |
               (1UL << k_poeg_grpbl2_bit_poeggci) | (1UL << k_poeg_grpbl2_bit_poeggdi));

  /* Clear any pending interrupt */
  icu_regs->ir[vector] = k_icu_ir_clear;

  /* Set interrupt priority */
  icu_regs->ipr[vector] = priority;

  /* Enable in IER register */
  const uint8_t ier_idx  = (uint8_t)(vector / k_ier_bits_per_reg);
  const uint8_t ier_bit  = (uint8_t)(vector % k_ier_bits_per_reg);
  const uint8_t ier_mask = (uint8_t)(k_ier_bit_enable_base << ier_bit);
  icu_regs->ier[ier_idx] |= ier_mask;
}

/**
 * @brief Configure GPTW GTINTAD for POEG group linkage
 *
 * @param[in] motor_index Motor index (0-3)
 *
 * @pre GPTW channel initialized and outputs not active during GRP write
 * @post GTINTAD configured with POEG group and detection enables
 *
 * @since Version 1.0.0
 */
static void internal_configure_gtintad(uint8_t motor_index)
{
  volatile rx_gptw_channel_regs_t* gptw = s_gptw_channels[motor_index];

  /* Unlock write protection */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Set GTINTAD: POEG group + dead-time error + simultaneous low detection */
  gptw->gtintad = s_gtintad_values[motor_index];

  /* Lock write protection */
  gptw->gtwp = k_gptw_gtwp_lock;
}

/* =============================================================================
 * ISR Handler - Single GROUPBL2 dispatcher for POEGGA/B/C/D
 * =============================================================================
 */

/**
 * @brief GROUPBL2 ISR - dispatches POEGGA/B/C/D fault interrupts
 *
 * @details
 * Handles the shared GROUPBL2 vector (107). Reads GRPBL2 to identify which
 * POEG sub-sources asserted, logs each, and triggers a single e-stop.
 *
 * @pre POEG groups initialized via rx_poeg_init()
 * @post For every POEGG bit set in GRPBL2, the fault is logged
 * @post Global e-stop triggered once if any POEG source fired
 *
 * @note Thread Safety: ISR context, runs at priority 14
 * @since Version 1.0.0
 */
void __attribute__((interrupt)) poeg_groupbl2_isr(void);
void __attribute__((interrupt)) poeg_groupbl2_isr(void)
{
  const uint32_t grpbl2  = *poeg_grpbl2_reg();
  bool           faulted = false;

  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if ((grpbl2 & (1UL << s_poeg_grpbl2_bits[i])) == 0U) {
      continue;
    }

    volatile rx_poegg_regs_t* poeg   = s_poeg_groups[i];
    const uint32_t            status = poeg->poeggn;
    if ((status & k_poeg_pidf_detected) != 0U) {
      rx_log_error_val(s_tag, "nFAULT motor", (uint32_t)i);
    }
    if ((status & k_poeg_iocf_detected) != 0U) {
      rx_log_error_val(s_tag, "GPTW output err motor", (uint32_t)i);
    }
    faulted = true;
  }

  /* Clear the shared GROUPBL2 request flag */
  icu()->ir[k_poeg_irq_groupbl2_vector] = k_icu_ir_clear;

  if (faulted) {
    shared_data_trigger_estop_isr_safe(k_estop_reason_driver_fault);
  }
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_poeg_init(void)
{
  rx_log_info(s_tag, "Initializing POEG fault protection");

  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    /* Configure POEG group: enable pin detection, invert, noise filter */
    volatile rx_poegg_regs_t* poeg = s_poeg_groups[i];
    poeg->poeggn                   = k_poeg_init_value;

    /* Verify write-once enable bits were accepted */
    if ((poeg->poeggn & k_poeg_pide_enable) == 0) {
      rx_log_error_val(s_tag, "POEG PIDE write failed motor", (uint32_t)i);
      return k_rx_err_hw_init_failed;
    }

    /* Link GPTW channel to POEG group via GTINTAD */
    internal_configure_gtintad(i);
  }

  /* Enable shared GROUPBL2 ICU vector (one dispatch for all POEG groups) */
  internal_enable_poeg_groupbl2_irq(k_poeg_isr_priority);

  rx_log_info(s_tag, "POEG fault protection active (4 groups on GROUPBL2, priority 14)");
  return k_rx_ok;
}

rx_err_t rx_poeg_get_fault_status(uint8_t motor_index, bool* fault_active)
{
  RX_CHECK_NULL_PTR(fault_active, s_tag, "fault_active is nullptr");

  if (motor_index >= k_poeg_motor_count) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t status = s_poeg_groups[motor_index]->poeggn;
  *fault_active =
    ((status & (k_poeg_pidf_detected | k_poeg_iocf_detected | k_poeg_ostpf_detected)) != 0);

  return k_rx_ok;
}

rx_err_t rx_poeg_clear_fault(uint8_t motor_index)
{
  if (motor_index >= k_poeg_motor_count) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_poegg_regs_t* poeg = s_poeg_groups[motor_index];

  /* Check if fault source is still active (ST bit shows pin state) */
  /* With INV=1, ST=0 means nFAULT is still asserted (fault active) */
  if ((poeg->poeggn & k_poeg_st_high) == 0) {
    rx_log_warn_val(s_tag, "Cannot clear: nFAULT still active motor", (uint32_t)motor_index);
    return k_rx_err_busy;
  }

  /* Clear PIDF and IOCF flags by writing 0 to them */
  uint32_t reg_val = poeg->poeggn;
  reg_val &= ~(k_poeg_pidf_detected | k_poeg_iocf_detected | k_poeg_ostpf_detected);
  poeg->poeggn = reg_val;

  /* Clear e-stop if this was the cause */
  if (shared_data_get_estop_reason() == k_estop_reason_driver_fault) {
    (void)shared_data_clear_estop();
  }

  rx_log_info_val(s_tag, "Fault cleared motor", (uint32_t)motor_index);
  return k_rx_ok;
}

rx_err_t rx_poeg_software_stop(uint8_t motor_index)
{
  if (motor_index >= k_poeg_motor_count) {
    return k_rx_err_invalid_arg;
  }

  s_poeg_groups[motor_index]->poeggn |= k_poeg_ssf_stop;
  return k_rx_ok;
}

rx_err_t rx_poeg_clear_software_stop(uint8_t motor_index)
{
  if (motor_index >= k_poeg_motor_count) {
    return k_rx_err_invalid_arg;
  }

  uint32_t reg_val = s_poeg_groups[motor_index]->poeggn;
  reg_val &= ~k_poeg_ssf_stop;
  s_poeg_groups[motor_index]->poeggn = reg_val;
  return k_rx_ok;
}
