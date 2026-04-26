/**
 * @file rx_cmt.c
 * @brief CMT (Compare Match Timer) Driver Implementation - Periodic Interrupt Generation
 *
 * @details
 * # Overview
 *
 * Implements the CMT driver for periodic interrupt generation on the RX72N.
 * This file contains all internal helpers, interrupt handlers, and the public
 * API implementation for CMT channels 0-3.
 *
 * ## Implementation Architecture
 *
 * @dot
 * digraph implementation {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API";
 *     style=filled;
 *     color=lightgreen;
 *     init [label="rx_cmt_init()"];
 *     start [label="rx_cmt_start()"];
 *     stop [label="rx_cmt_stop()"];
 *     count [label="rx_cmt_get_count()"];
 *     deinit [label="rx_cmt_deinit()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal Helpers";
 *     style=filled;
 *     color=lightyellow;
 *     get_base [label="internal_get_cmt_base()"];
 *     calc [label="internal_calculate_cmt_params()"];
 *     enable [label="internal_enable_cmt_module_clock()"];
 *     configure [label="internal_configure_cmt_timer_registers()"];
 *     irq [label="internal_configure_cmt_interrupt()"];
 *     validate [label="internal_validate_cmt_init_params()"];
 *     save [label="internal_save_cmt_callback()"];
 *   }
 *
 *   subgraph cluster_isr {
 *     label="Interrupt Handlers";
 *     style=filled;
 *     color=lightblue;
 *     cmt1_isr [label="cmt1_isr()"];
 *     cmt2_isr [label="cmt2_isr()"];
 *     cmt3_isr [label="cmt3_isr()"];
 *   }
 *
 *   init -> validate -> calc -> enable -> configure -> irq -> save;
 *   init -> start;
 *   start -> get_base;
 *   stop -> get_base;
 *   count -> get_base;
 *   cmt1_isr -> s_cmt_callback [label="call"];
 * }
 * @enddot
 *
 * ## CMT Timer Operation
 *
 * The CMT operates as a 16-bit up-counter with compare match:
 *
 * @msc
 *   width=600;
 *   CMCNT, CMCOR, ISR, Callback;
 *
 *   CMCNT box CMCNT [label="Counter at 0"];
 *   CMCNT -> CMCNT [label="Count: 1, 2, 3..."];
 *   CMCNT => CMCOR [label="CMCNT reaches CMCOR"];
 *   CMCOR box CMCOR [label="Compare match!\nCMCNT = 0"];
 *   CMCOR => ISR [label="CMIn interrupt"];
 *   ISR => Callback [label="s_cmt_callback[n]()"];
 *   Callback >> ISR;
 *   ISR >> CMCNT [label="Return"];
 *   CMCNT -> CMCNT [label="Continue counting..."];
 * @endmsc
 *
 * ## Clock Divider Selection Algorithm
 *
 * The driver tries each divider in order (/8, /32, /128, /512) and selects
 * the first one that produces a period fitting in 16 bits:
 *
 * ```
 * period = (PCLKB / divider) / frequency
 * if (0 < period <= 65535): use this divider
 * ```
 *
 * | Divider | PCLKB/div | Min Period | Max Period |
 * |---------|-----------|------------|------------|
 * | /8 | 7.5 MHz | 1 (7.5 MHz) | 65535 (115 Hz) |
 * | /32 | 1.875 MHz | 1 (1.875 MHz) | 65535 (29 Hz) |
 * | /128 | 468.75 kHz | 1 (469 kHz) | 65535 (7.2 Hz) |
 * | /512 | 117.2 kHz | 1 (117 kHz) | 65535 (1.8 Hz) |
 *
 * ## Memory Map (Static State)
 *
 * | Symbol | Size | Description |
 * |--------|------|-------------|
 * | s_cmt_initialized[] | 4 bytes | Per-channel init flags |
 * | s_cmt_callback[] | 16 bytes | Callback function pointers |
 * | s_cmt_user_data[] | 16 bytes | User data pointers |
 *
 * **Total static allocation:** 36 bytes
 *
 * ## Register Access Pattern
 *
 * All register access uses inline accessor functions for type safety:
 * - `cmt0()`, `cmt1()`, `cmt2()`, `cmt3()` - Channel registers
 * - `cmt_ctrl()` - Control registers (CMSTR0, CMSTR1)
 * - `icu()` - Interrupt controller
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Implementation |
 * |------|----------------|
 * | Rule 1 | No goto/recursion, single return paths |
 * | Rule 2 | Bounded loop in internal_calculate_cmt_params() |
 * | Rule 3 | Static arrays only, no heap |
 * | Rule 4 | All functions under 60 lines |
 * | Rule 5 | Parameter validation in all public functions |
 * | Rule 6 | File-scope static with s_ prefix |
 * | Rule 7 | All return values checked |
 * | Rule 8 | C23 typed enums for all constants |
 * | Rule 9 | Single-level pointers only |
 * | Rule 10 | Clean with -Wall -Wextra -Werror |
 *
 * @see rx_cmt.h Public API declarations
 * @see rx72n_cmt_regs.h CMT register definitions
 * @see rx_register_protection.h PRCR unlock for module stop
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include "rx_cmt.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

/* Interrupt handler forward declarations (required for -Wmissing-declarations) */
void cmt1_isr(void);
void cmt2_isr(void);
void cmt3_isr(void);

static const char* const s_tag = "CMT";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum cmt_constants_t
 * @brief CMT channel and divider configuration constants
 *
 * @details
 * Defines the channel count, divider settings, and loop bounds for CMT
 * configuration. These values correspond to RX72N hardware capabilities.
 *
 * ## Divider Settings (CMCR.CKS field)
 *
 * | CKS Value | Divider | Counter Clock @60MHz |
 * |-----------|---------|----------------------|
 * | 0b00 | /8 | 7.5 MHz |
 * | 0b01 | /32 | 1.875 MHz |
 * | 0b10 | /128 | 468.75 kHz |
 * | 0b11 | /512 | 117.19 kHz |
 *
 * @note k_cmt_divider_start and k_cmt_num_dividers define loop bounds
 *       for NASA Power of 10 Rule 2 compliance.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cmt_max_channels  = 4, /**< Total CMT channels: CMT0, CMT1, CMT2, CMT3 */
  k_cmt_divider_start = 0, /**< Loop start index for divider iteration (Rule 2) */
  k_cmt_div_8         = 0, /**< CKS=0b00: PCLKB/8 divider (7.5 MHz @ 60 MHz PCLKB) */
  k_cmt_div_32        = 1, /**< CKS=0b01: PCLKB/32 divider (1.875 MHz @ 60 MHz PCLKB) */
  k_cmt_div_128       = 2, /**< CKS=0b10: PCLKB/128 divider (468.75 kHz @ 60 MHz PCLKB) */
  k_cmt_div_512       = 3, /**< CKS=0b11: PCLKB/512 divider (117.19 kHz @ 60 MHz PCLKB) */
  k_cmt_num_dividers  = 4, /**< Loop bound for divider iteration (Rule 2 compliance) */
} cmt_constants_t;

/** @brief CMT divider values (actual divisor values) */
typedef enum : uint16_t {
  k_cmt_divider_val_8   = 8,   /**< Divide by 8 */
  k_cmt_divider_val_32  = 32,  /**< Divide by 32 */
  k_cmt_divider_val_128 = 128, /**< Divide by 128 */
  k_cmt_divider_val_512 = 512, /**< Divide by 512 */
} cmt_divider_values_t;

/**
 * @brief CMT module stop bit positions in MSTPCRA (RX72N Manual Section 11.2.2, p.406)
 * @details
 * CMT is controlled by MSTPCRA, NOT MSTPCRB.
 * - MSTPA15: CMT unit 0 (CMT0, CMT1) module stop
 * - MSTPA14: CMT unit 1 (CMT2, CMT3) module stop
 */
typedef enum : uint8_t {
  k_cmt_mstpa_unit0 = 15, /**< MSTPCRA bit 15: CMT unit 0 (CMT0, CMT1) module stop */
  k_cmt_mstpa_unit1 = 14, /**< MSTPCRA bit 14: CMT unit 1 (CMT2, CMT3) module stop */
} cmt_module_stop_bits_t;

/**
 * @struct rx_cmt_interrupt_config_t
 * @brief CMT interrupt routing and priority configuration
 *
 * @details
 * Bundles the CMT channel identifier with the desired ICU interrupt priority
 * level. Passed by value to internal_configure_cmt_interrupt() so that the
 * caller does not need to keep the structure alive after the call returns.
 *
 * The channel field selects which ICU interrupt vector to program (CMI0-CMI3),
 * and the priority field is written directly into the corresponding IPR[]
 * register.
 *
 * @invariant channel must be a valid rx_cmt_channel_t value (0-3)
 * @invariant priority must be in [k_ipr_level_min, k_ipr_level_max]
 *
 * @par Example:
 * @code{.c}
 * rx_cmt_interrupt_config_t irq_cfg = {
 *   .channel  = k_cmt_channel_1,
 *   .priority = 5,
 * };
 * rx_err_t err = internal_configure_cmt_interrupt(irq_cfg);
 * @endcode
 *
 * @see internal_configure_cmt_interrupt() Sole consumer of this struct
 * @see rx_cmt_init() Constructs this struct inline before calling the above
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_cmt_channel_t channel; /**< CMT channel identifier (k_cmt_channel_0 ... k_cmt_channel_3) */
  uint8_t priority; /**< ICU interrupt priority level (k_ipr_level_min ... k_ipr_level_max) */
} rx_cmt_interrupt_config_t;

/** @brief CMCR register bit positions (RX72N Manual Ch31, p.1583) */
typedef enum : uint8_t {
  k_cmt_cmcr_cks_shift = 0, /**< CKS (clock select) bit shift */
  k_cmt_cmcr_cmie_pos  = 6, /**< CMIE (interrupt enable) bit position */
  k_cmt_cmcr_rsvd7_pos = 7, /**< Reserved bit 7: write value should be 1 */
} cmt_cmcr_bits_t;

/** @brief Period calculation constants */
typedef enum : uint16_t {
  k_cmt_period_min = 0,      /**< Minimum valid period */
  k_cmt_period_max = 0xFFFF, /**< Maximum valid period (16-bit) */
  k_cmt_period_adj = 1,      /**< Period adjustment (period - 1) */
} cmt_period_constants_t;

/** @brief CMSTR register bit positions */
typedef enum : uint8_t {
  k_cmt_cmstr_str0 = 0, /**< CMT0/CMT2 start bit */
  k_cmt_cmstr_str1 = 1, /**< CMT1/CMT3 start bit */
} cmt_cmstr_bits_t;

/** @brief CMT bit manipulation constants */
typedef enum : uint8_t {
  k_cmt_bit_mask_lsb = 1, /**< Single bit set for shifts */
  k_cmt_value_zero   = 0, /**< Zero value for comparisons */
} cmt_bit_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_cmt_initialized
 * @brief Per-channel initialization status flags
 *
 * @details
 * Tracks whether each CMT channel has been initialized. Set to true after
 * successful rx_cmt_init(), cleared on rx_cmt_deinit().
 *
 * | Index | Channel | Initial Value |
 * |-------|---------|---------------|
 * | 0 | CMT0 | false (ThreadX manages) |
 * | 1 | CMT1 | false |
 * | 2 | CMT2 | false |
 * | 3 | CMT3 | false |
 *
 * @note Used to prevent operations on uninitialized channels.
 * @warning Do not modify directly; managed by init/deinit functions.
 *
 * @since Version 1.0.0
 */
static bool s_cmt_initialized[k_cmt_max_channels] = {false};

/**
 * @var s_cmt_callback
 * @brief Per-channel interrupt callback function pointers
 *
 * @details
 * Stores user-provided callback functions for each CMT channel. Called from
 * the corresponding interrupt handler (cmt1_isr, cmt2_isr, cmt3_isr).
 *
 * @note NULL callbacks are valid - interrupt fires but no action taken.
 * @warning Callback executes in interrupt context. Keep it fast!
 *
 * @see rx_cmt_callback_t Callback function type
 *
 * @since Version 1.0.0
 */
static rx_cmt_callback_t s_cmt_callback[k_cmt_max_channels] = {nullptr};

/**
 * @var s_cmt_user_data
 * @brief Per-channel user data for callbacks
 *
 * @details
 * Stores user-provided context data passed to callbacks. Allows callbacks
 * to access application-specific state without global variables.
 *
 * @note May be NULL if callback doesn't need user data.
 *
 * @since Version 1.0.0
 */
static void* s_cmt_user_data[k_cmt_max_channels] = {nullptr};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Map a CMT channel enum to its hardware register base pointer
 *
 * @details
 * Uses a switch statement to convert a logical channel identifier into a
 * pointer to the corresponding volatile rx_cmt_channel_regs_t register block.
 * The four CMT channels (CMT0-CMT3) each have a distinct base address in the
 * RX72N memory map; the inline accessor functions cmt0()-cmt3() encode those
 * addresses in a type-safe manner.
 *
 * The default case returns nullptr, which every caller is expected to check
 * immediately after the call (NASA Rule 7 compliance).
 *
 *
 *
 * @pre channel is a member of rx_cmt_channel_t (0-3)
 * @pre Hardware register addresses must be accessible (clocks enabled before use)
 *
 * @post Return value, if non-null, points to the live hardware register block
 * @post Caller must validate the return value before dereferencing (Rule 7)
 *
 * @note Not thread-safe; the hardware registers are globally shared
 * @note This function never writes to hardware; it only computes an address
 *
 * @par Thread Safety:
 * Read-only address computation; no shared mutable state is accessed.
 *
 * @par Performance:
 * Execution time: ~1 cycle (single branch/address load with -O2)
 *
 * @par Example:
 * @code{.c}
 * volatile rx_cmt_channel_regs_t* cmt = internal_get_cmt_base(k_cmt_channel_2);
 * if (cmt == nullptr) {
 *   return k_rx_err_invalid_arg;
 * }
 * cmt->cmcnt = 0;
 * @endcode
 *
 * @see cmt0() Inline accessor for CMT0 base address
 * @see cmt1() Inline accessor for CMT1 base address
 * @see cmt2() Inline accessor for CMT2 base address
 * @see cmt3() Inline accessor for CMT3 base address
 *
 * @since Version 1.0.0
 */
static volatile rx_cmt_channel_regs_t* internal_get_cmt_base(const rx_cmt_channel_t channel)
{
  switch (channel) {
    case k_cmt_channel_0:
      return cmt0();
    case k_cmt_channel_1:
      return cmt1();
    case k_cmt_channel_2:
      return cmt2();
    case k_cmt_channel_3:
      return cmt3();
    default:
      return nullptr;
  }
}

/**
 * @brief Calculate the optimal CMT clock divider and compare-match value for a target frequency
 *
 * @details
 * Iterates the four available PCLKB clock dividers (/8, /32, /128, /512) in
 * ascending order and selects the first divider whose resulting 16-bit period
 * value satisfies:
 *
 * ```
 * period = (PCLKB / divider) / frequency_hz
 * 0 < period <= 65535
 * ```
 *
 * The chosen divider index is returned in *divider (corresponding to the CKS
 * field of CMCR: 0=/8, 1=/32, 2=/128, 3=/512).  The compare-match register
 * value is returned in *cmcor; the caller must subtract the adjustment constant
 * k_cmt_period_adj when writing to the hardware register.
 *
 * Algorithm steps:
 * 1. Validate output pointers are non-null
 * 2. Reject frequency_hz == 0 or frequency_hz > PCLKB/8 (maximum achievable)
 * 3. Loop i from k_cmt_divider_start to k_cmt_num_dividers - 1
 * 4.   Compute period_calc = (PCLKB / dividers[i]) / frequency_hz
 * 5.   If 0 < period_calc <= 65535, store i and period_calc, return k_rx_ok
 * 6. If no divider fits, return k_rx_err_invalid_arg
 *
 *
 *
 * @pre divider must point to a valid uint8_t storage location
 * @pre cmcor must point to a valid uint16_t storage location
 *
 * @post On k_rx_ok: *divider holds the CKS value (0-3) for CMCR
 * @post On k_rx_ok: *cmcor holds the period count; caller subtracts 1 before writing CMCOR
 *
 * @note Not thread-safe; pure computation with no shared state modification
 * @note Uses a bounded loop (k_cmt_num_dividers iterations) -- NASA Rule 2 compliant
 *
 * @par Thread Safety:
 * Pure computation with no global or static state; safe to call from any context.
 *
 * @par Performance:
 * Execution time: ~5 cycles (loop unrolled at -O2 for 4 divider steps)
 *
 * @par Example:
 * @code{.c}
 * uint8_t  divider = 0;
 * uint16_t cmcor   = 0;
 * rx_err_t err = internal_calculate_cmt_params(1000U, &divider, &cmcor);
 * if (err == k_rx_ok) {
 *   // divider and cmcor are ready for use in CMCR/CMCOR registers
 * }
 * @endcode
 *
 * @see internal_configure_cmt_timer_registers() Consumes the output of this function
 * @see cmt_constants_t  Divider index constants (k_cmt_div_8 ... k_cmt_div_512)
 * @see cmt_divider_values_t Actual divisor values (8, 32, 128, 512)
 *
 * @since Version 1.0.0
 */
static rx_err_t
internal_calculate_cmt_params(const uint32_t frequency_hz, uint8_t* divider, uint16_t* cmcor)
{
  static const uint16_t dividers[]    = {k_cmt_divider_val_8,
                                         k_cmt_divider_val_32,
                                         k_cmt_divider_val_128,
                                         k_cmt_divider_val_512};
  const uint32_t        pclkb         = k_pclkb_hz;
  const uint32_t        max_frequency = pclkb / k_cmt_divider_val_8;

  if (divider == nullptr || cmcor == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (frequency_hz <= (uint32_t)k_cmt_value_zero || frequency_hz > max_frequency) {
    return k_rx_err_invalid_arg;
  }

  /* Try each divider to find one that fits in 16-bit */
  for (uint8_t i = k_cmt_divider_start; i < k_cmt_num_dividers; i++) {
    const uint32_t period_calc = (pclkb / dividers[i]) / frequency_hz;

    /* Check if period fits in 16-bit and is reasonable */
    if (period_calc > k_cmt_period_min && period_calc <= k_cmt_period_max) {
      *divider = i;
      *cmcor   = (uint16_t)period_calc;
      return k_rx_ok;
    }
  }

  rx_log_error(s_tag, "Error occurred");
  return k_rx_err_invalid_arg;
}

/**
 * @brief Enable the CMT module clock by clearing the MSTPCRB module-stop bit
 *
 * @details
 * Removes the CMT0-CMT3 peripheral from module-stop state so that the module
 * clock is supplied and its registers become accessible. The sequence is:
 *
 * 1. Unlock the write-protect register (PRCR) for PRC1 and PRC3 bits
 * 2. Clear bit k_cmt_mstpa_unit0 (bit 15) in MSTPCRA to enable CMT unit 0 (CMT0/CMT1)
 * 3. Clear bit k_cmt_mstpa_unit1 (bit 14) in MSTPCRA to enable CMT unit 1 (CMT2/CMT3)
 * 4. Re-lock the PRCR to prevent accidental modification
 *
 * This function must be called before any CMT register access; reading or
 * writing CMT registers while the module clock is stopped produces undefined
 * hardware behaviour on the RX72N.
 *
 * The function is idempotent: calling it when the module is already enabled
 * simply clears an already-clear bit, which is harmless.
 *
 *
 * @pre The system clock (PCLKB) must be running before calling this function
 * @pre No other task or ISR must be concurrently modifying MSTPCRB
 *
 * @post MSTPCRA bits 15 and 14 are cleared; CMT module clocks for all 4 channels are active
 * @post PRCR is locked upon return (prevents accidental MSTPCRB changes)
 *
 * @note Not thread-safe; PRCR unlock/lock is a non-atomic read-modify-write
 * @note Called once per system lifetime; subsequent calls are benign
 *
 * @par Thread Safety:
 * Not thread-safe. Call during system initialization before RTOS scheduler
 * starts, or with interrupts disabled.
 *
 * @par Performance:
 * Execution time: ~4 register writes; negligible overhead
 *
 * @par Example:
 * @code{.c}
 * // Called internally by rx_cmt_init(); not intended for direct use
 * internal_enable_cmt_module_clock();
 * // CMT registers are now accessible
 * @endcode
 *
 * @see rx_register_protection.h  PRCR lock/unlock constants
 * @see rx_cmt_init()             Sole caller; invoked before register programming
 *
 * @since Version 1.0.0
 */
static void internal_enable_cmt_module_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  /* CMT is in MSTPCRA (not MSTPCRB): bit 15 = unit 0 (CMT0/1), bit 14 = unit 1 (CMT2/3) */
  system_regs()->mstpcra &= ~((uint32_t)k_cmt_bit_mask_lsb << k_cmt_mstpa_unit0);
  system_regs()->mstpcra &= ~((uint32_t)k_cmt_bit_mask_lsb << k_cmt_mstpa_unit1);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Write clock divider, interrupt-enable, compare-match, and counter registers
 *
 * @details
 * Programs the three CMT hardware registers for a single channel in the
 * following order:
 *
 * 1. **CMCR** - Sets the CKS (clock-select) field from divider and enables the
 *    compare-match interrupt (CMIE bit).
 * 2. **CMCOR** - Loads the compare-match value after subtracting the adjustment
 *    constant k_cmt_period_adj (-1) so the hardware fires at the correct period.
 * 3. **CMCNT** - Clears the counter to 0 so the first interrupt occurs exactly
 *    one full period after the timer is started.
 *
 * The timer must be stopped (CMSTR bit cleared) before calling this function
 * to avoid race conditions while writing to CMCOR and CMCNT.
 *
 *
 *
 * @pre cmt must not be nullptr (enforced by RX_ASSERT)
 * @pre The CMT timer must be stopped before calling this function
 *
 * @post cmt->cmcr  = (divider << CKS_SHIFT) | CMIE_BIT | RSVD7_BIT (bit7 must be written 1)
 * @post cmt->cmcor = cmcor - k_cmt_period_adj
 * @post cmt->cmcnt = 0
 *
 * @note Not thread-safe; caller must ensure the timer is stopped
 * @note RX_ASSERT fires in debug builds if cmt is nullptr
 *
 * @par Thread Safety:
 * Not thread-safe. Must be called with the timer stopped and no concurrent ISR
 * activity on this channel.
 *
 * @par Performance:
 * Execution time: ~3 register writes; negligible overhead
 *
 * @par Example:
 * @code{.c}
 * uint8_t  divider = 0;
 * uint16_t cmcor   = 0;
 * (void)internal_calculate_cmt_params(1000U, &divider, &cmcor);
 * volatile rx_cmt_channel_regs_t* cmt = internal_get_cmt_base(k_cmt_channel_1);
 * internal_configure_cmt_timer_registers(cmt, divider, cmcor);
 * @endcode
 *
 * @see internal_calculate_cmt_params() Produces the divider and cmcor arguments
 * @see rx_cmt_stop()                   Must be called before this function
 * @see cmt_cmcr_bits_t                 CKS shift and CMIE bit position constants
 *
 * @since Version 1.0.0
 */
static void internal_configure_cmt_timer_registers(volatile rx_cmt_channel_regs_t* cmt,
                                                   const uint8_t                   divider,
                                                   const uint16_t                  cmcor)
{
  RX_ASSERT(cmt != nullptr, "CMT registers are NULL");

  /* Configure timer control register
   * - Set clock divider (bits 1-0)
   * - Enable compare match interrupt (bit 6)
   * - Set reserved bit 7 to 1 (write value should be 1 per RX72N manual p.1583)
   */
  cmt->cmcr =
    (divider << k_cmt_cmcr_cks_shift) |           /* CKS: Clock Select */
    (k_cmt_bit_mask_lsb << k_cmt_cmcr_cmie_pos) | /* CMIE: Compare Match Interrupt Enable */
    (k_cmt_bit_mask_lsb << k_cmt_cmcr_rsvd7_pos); /* Reserved bit 7: write value should be 1 */

  /* Set compare match value (period - 1) */
  cmt->cmcor = cmcor - k_cmt_period_adj;

  /* Clear counter */
  cmt->cmcnt = k_cmt_value_zero;
}

/**
 * @brief Configure the ICU interrupt vector priority and enable for a CMT channel
 *
 * @details
 * Programs the Interrupt Controller Unit (ICU) to route and enable the
 * compare-match interrupt (CMIn) for the specified CMT channel. The three
 * ICU registers modified are:
 *
 * 1. **IPR[vector]** - Sets the interrupt priority level for the channel's
 *    CMIn vector to config.priority.
 * 2. **IER[index]** - Sets the enable bit for the channel's interrupt vector
 *    within the IER register array. The bit index and byte index are derived
 *    from the vector number.
 * 3. **IR[vector]** - Clears any pending interrupt flag before enabling, so
 *    a stale flag does not fire immediately after enable.
 *
 * The vector number is computed as k_vect_cmt0_cmi0 + config.channel, which
 * yields CMI0 for channel 0, CMI1 for channel 1, and so on.
 *
 *
 *
 * @pre config.channel must be in the range [0, k_cmt_max_channels)
 * @pre config.priority must be in [k_ipr_level_min, k_ipr_level_max]
 *
 * @post ICU IPR register for the channel's vector set to config.priority
 * @post ICU IER bit for the channel's vector enabled
 * @post ICU IR flag for the channel's vector cleared
 *
 * @note Not thread-safe; concurrent ICU modification from another context is unsafe
 * @note Clears IR before enabling to prevent a spurious first interrupt
 *
 * @par Thread Safety:
 * Not thread-safe. Must be called during initialization with interrupts disabled
 * or before the RTOS scheduler starts.
 *
 * @par Performance:
 * Execution time: ~4 register accesses; negligible overhead
 *
 * @par Example:
 * @code{.c}
 * rx_cmt_interrupt_config_t irq_cfg = {
 *   .channel  = k_cmt_channel_1,
 *   .priority = 5,
 * };
 * rx_err_t err = internal_configure_cmt_interrupt(irq_cfg);
 * if (err != k_rx_ok) {
 *   return err;
 * }
 * @endcode
 *
 * @see rx_cmt_interrupt_config_t  Configuration structure passed to this function
 * @see rx_cmt_deinit()            Clears the IER bit to disable the interrupt
 * @see rx_cmt_init()              Constructs config and calls this function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_cmt_interrupt(const rx_cmt_interrupt_config_t config)
{
  /* Note: Lower bound check omitted - config.channel is uint8_t, k_cmt_channel_0 == 0,
   * so config.channel >= k_cmt_channel_0 is always true (-Wtype-limits) */
  if ((uint8_t)config.channel >= (uint8_t)k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }
  if (config.priority < k_ipr_level_min || config.priority > k_ipr_level_max) {
    return k_rx_err_invalid_arg;
  }

  /* CMI0/CMI1 have fixed ICU vectors (28, 29). CMI2/CMI3 are SELECTB
   * sources on RX72N and need SLIBRn programming, which is not yet
   * implemented in this driver. Reject interrupt-mode init on ch2/ch3
   * rather than enabling the wrong vectors (30/31 are CMWI0/CMWI1 for
   * CMTW timers). Per RX72N HW manual R01UH0824EJ0111 Section 15.3
   * (Interrupt Vector Table) and Section 15.4 (SELECTB sources). */
  if (config.channel == k_cmt_channel_2 || config.channel == k_cmt_channel_3) {
    return k_rx_err_not_supported;
  }

  const uint8_t vector    = k_vect_cmt0_cmi0 + config.channel;
  const uint8_t ier_index = vector / k_icu_ier_bits_per_reg;
  const uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;

  icu()->ipr[vector]     = config.priority;
  const uint8_t ier_mask = (uint8_t)(k_cmt_bit_mask_lsb << ier_bit);
  icu()->ier[ier_index] |= ier_mask;
  icu()->ir[vector] = k_cmt_value_zero;

  return k_rx_ok;
}

/* =============================================================================
 * Interrupt Handlers
 * =============================================================================
 */

/**
 * @brief CMT1 (CMT channel 1) compare-match interrupt service routine
 *
 * @details
 * Invoked by the RX72N hardware whenever the CMT1 counter (CMCNT) reaches the
 * value stored in CMCOR. The ISR checks whether a callback has been registered
 * in s_cmt_callback[k_cmt_channel_1] and, if so, calls it with the associated
 * user data pointer from s_cmt_user_data[k_cmt_channel_1].
 *
 * The compare-match interrupt flag is automatically cleared by the RX72N
 * hardware upon entry; no explicit IR register write is needed.
 *
 * @note Executes in interrupt context; all code in this ISR and any registered
 *       callback must be ISR-safe (no blocking, no long operations)
 * @note Not thread-safe; the callback executes with the CMT1 interrupt masked
 *
 * @warning Do not call any ThreadX API that is not interrupt-safe from the
 *          registered callback (e.g., tx_thread_sleep() is forbidden; use
 *          ISR-safe variants such as tx_semaphore_put_from_isr())
 *
 * @see rx_cmt_init()       Registers the callback invoked by this ISR
 * @see rx_cmt_deinit()     Clears the callback and disables this interrupt
 * @see s_cmt_callback      Per-channel callback function pointer array
 *
 * @since Version 1.0.0
 */
void cmt1_isr(void)
{
  /* Call user callback if registered */
  if (s_cmt_callback[k_cmt_channel_1] != nullptr) {
    s_cmt_callback[k_cmt_channel_1](s_cmt_user_data[k_cmt_channel_1]);
  }

  /* Clear interrupt flag (read-modify-write not needed, auto-cleared) */
}

/**
 * @brief CMT2 (CMT channel 2) compare-match interrupt service routine
 *
 * @details
 * Invoked by the RX72N hardware whenever the CMT2 counter (CMCNT) reaches the
 * value stored in CMCOR. The ISR checks whether a callback has been registered
 * in s_cmt_callback[k_cmt_channel_2] and, if so, calls it with the associated
 * user data pointer from s_cmt_user_data[k_cmt_channel_2].
 *
 * The compare-match interrupt flag is automatically cleared by the RX72N
 * hardware upon entry; no explicit IR register write is needed.
 *
 * @note Executes in interrupt context; all code in this ISR and any registered
 *       callback must be ISR-safe (no blocking, no long operations)
 * @note Not thread-safe; the callback executes with the CMT2 interrupt masked
 *
 * @warning Do not call any ThreadX API that is not interrupt-safe from the
 *          registered callback (e.g., tx_thread_sleep() is forbidden; use
 *          ISR-safe variants such as tx_semaphore_put_from_isr())
 *
 * @see rx_cmt_init()       Registers the callback invoked by this ISR
 * @see rx_cmt_deinit()     Clears the callback and disables this interrupt
 * @see s_cmt_callback      Per-channel callback function pointer array
 *
 * @since Version 1.0.0
 */
void cmt2_isr(void)
{
  if (s_cmt_callback[k_cmt_channel_2] != nullptr) {
    s_cmt_callback[k_cmt_channel_2](s_cmt_user_data[k_cmt_channel_2]);
  }
}

/**
 * @brief CMT3 (CMT channel 3) compare-match interrupt service routine
 *
 * @details
 * Invoked by the RX72N hardware whenever the CMT3 counter (CMCNT) reaches the
 * value stored in CMCOR. The ISR checks whether a callback has been registered
 * in s_cmt_callback[k_cmt_channel_3] and, if so, calls it with the associated
 * user data pointer from s_cmt_user_data[k_cmt_channel_3].
 *
 * The compare-match interrupt flag is automatically cleared by the RX72N
 * hardware upon entry; no explicit IR register write is needed.
 *
 * @note Executes in interrupt context; all code in this ISR and any registered
 *       callback must be ISR-safe (no blocking, no long operations)
 * @note Not thread-safe; the callback executes with the CMT3 interrupt masked
 *
 * @warning Do not call any ThreadX API that is not interrupt-safe from the
 *          registered callback (e.g., tx_thread_sleep() is forbidden; use
 *          ISR-safe variants such as tx_semaphore_put_from_isr())
 *
 * @see rx_cmt_init()       Registers the callback invoked by this ISR
 * @see rx_cmt_deinit()     Clears the callback and disables this interrupt
 * @see s_cmt_callback      Per-channel callback function pointer array
 *
 * @since Version 1.0.0
 */
void cmt3_isr(void)
{
  if (s_cmt_callback[k_cmt_channel_3] != nullptr) {
    s_cmt_callback[k_cmt_channel_3](s_cmt_user_data[k_cmt_channel_3]);
  }
}

/* =============================================================================
 * Additional Internal Helpers for rx_cmt_init
 * =============================================================================
 */

/**
 * @brief Validate channel and configuration pointer before CMT initialisation
 *
 * @details
 * Performs all parameter checking required before rx_cmt_init() proceeds to
 * hardware configuration. Validation is performed in this strict order:
 *
 * 1. Verify config pointer is non-null (k_rx_err_null_ptr)
 * 2. Verify channel is within [0, k_cmt_max_channels) (k_rx_err_invalid_arg)
 * 3. Reject channel 0, which is reserved for the ThreadX system-tick timer
 *    (k_rx_err_conflict)
 * 4. Verify interrupt priority is within [k_ipr_level_min, k_ipr_level_max]
 *    (k_rx_err_invalid_arg)
 *
 * Centralising these checks in a dedicated helper keeps the main rx_cmt_init()
 * function concise and ensures that every check is logged before returning.
 *
 *
 *
 * @pre config must be a pointer to a caller-owned rx_cmt_config_t (may be nullptr)
 * @pre channel must be a member of rx_cmt_channel_t
 *
 * @post On k_rx_ok: channel and *config are safe to use for hardware programming
 * @post On error: an error message has been emitted via rx_log_error()
 *
 * @note Not thread-safe; pure validation with no shared state modification
 * @note The lower-bound check `channel >= k_cmt_channel_0` is intentionally
 *       omitted to avoid -Wtype-limits: channel is uint8_t and k_cmt_channel_0 == 0,
 *       making the comparison always true
 *
 * @par Thread Safety:
 * Pure validation; no shared mutable state is modified. Safe to call from any
 * context, but the result may be stale if state changes after the call.
 *
 * @par Performance:
 * Execution time: ~4 comparisons; negligible overhead
 *
 * @par Example:
 * @code{.c}
 * rx_err_t err = internal_validate_cmt_init_params(k_cmt_channel_1, &config);
 * if (err != k_rx_ok) {
 *   return err;
 * }
 * @endcode
 *
 * @see rx_cmt_init()     Sole caller; uses this to guard hardware programming
 * @see rx_cmt_config_t   Configuration structure whose fields are validated here
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_validate_cmt_init_params(const rx_cmt_channel_t channel,
                                                  const rx_cmt_config_t* config)
{
  if (config == nullptr) {
    rx_log_error(s_tag, "config pointer is nullptr");
    return k_rx_err_null_ptr;
  }

  /* Note: Lower bound check omitted - channel is uint8_t, k_cmt_channel_0 == 0,
   * so channel >= k_cmt_channel_0 is always true (-Wtype-limits) */
  if ((uint8_t)channel >= (uint8_t)k_cmt_max_channels) {
    rx_log_error(s_tag, "Invalid CMT channel");
    return k_rx_err_invalid_arg;
  }

  /* CMT0 is reserved for ThreadX system tick */
  if (channel == k_cmt_channel_0) {
    rx_log_error(s_tag, "CMT0 is reserved for ThreadX");
    return k_rx_err_conflict;
  }

  if (config->priority < k_ipr_level_min || config->priority > k_ipr_level_max) {
    rx_log_error(s_tag, "Invalid interrupt priority");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Persist the user callback, user-data pointer, and channel initialized flag
 *
 * @details
 * Copies the callback function pointer and user-data pointer from the caller's
 * configuration structure into the module-level static arrays, then marks the
 * channel as initialized. After this call the ISR for the channel will invoke
 * the registered callback on every compare-match event.
 *
 * This function is the final step in the rx_cmt_init() sequence. It must only
 * be called after all hardware registers have been programmed and the interrupt
 * has been configured in the ICU, so that the ISR never encounters a partially
 * initialised channel state.
 *
 *
 *
 * @pre channel must be valid and in range [1, k_cmt_max_channels) (validated by caller)
 * @pre config must be non-null (validated by internal_validate_cmt_init_params)
 *
 * @post s_cmt_callback[channel]    = config->callback
 * @post s_cmt_user_data[channel]   = config->user_data
 * @post s_cmt_initialized[channel] = true
 *
 * @note Not thread-safe; must be the last step of rx_cmt_init() with interrupts
 *       either disabled or not yet fired for this channel
 * @note After this call the channel is considered "live"; the ISR may fire at
 *       any point
 *
 * @par Thread Safety:
 * Not thread-safe. The three static array writes are not atomic. Call only
 * during initialization with the channel interrupt not yet active.
 *
 * @par Performance:
 * Execution time: ~3 stores; negligible overhead
 *
 * @par Example:
 * @code{.c}
 * // Final step inside rx_cmt_init() after all hardware setup is complete
 * internal_save_cmt_callback(channel, config);
 * // s_cmt_initialized[channel] is now true; ISR will call config->callback
 * @endcode
 *
 * @see rx_cmt_init()      Sole caller; calls this as the last initialisation step
 * @see rx_cmt_deinit()    Clears the state written by this function
 * @see s_cmt_callback     Destination array for config->callback
 * @see s_cmt_user_data    Destination array for config->user_data
 * @see s_cmt_initialized  Destination array for the initialized flag
 *
 * @since Version 1.0.0
 */
static void internal_save_cmt_callback(const rx_cmt_channel_t channel,
                                       const rx_cmt_config_t* config)
{
  s_cmt_callback[channel]    = config->callback;
  s_cmt_user_data[channel]   = config->user_data;
  s_cmt_initialized[channel] = true;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a CMT channel for periodic interrupt generation
 *
 * @details
 * Configures a CMT (Compare Match Timer) channel to fire a user callback at the
 * requested frequency. Validates parameters, selects the optimal clock divider,
 * enables the module clock, stops the timer for safe configuration, programs
 * timer registers, configures the ICU interrupt routing, saves the callback,
 * and starts the timer.
 *
 * **Algorithm steps:**
 * 1. Validate channel and config parameters via internal_validate_cmt_init_params()
 * 2. Obtain CMT channel register base via internal_get_cmt_base()
 * 3. Calculate optimal divider and CMCOR value via internal_calculate_cmt_params()
 * 4. Enable CMT module clock (clear MSTPCRB bit) via internal_enable_cmt_module_clock()
 * 5. Stop the timer to allow safe register writes via rx_cmt_stop()
 * 6. Program CMCR (divider + CMIE) and CMCOR via internal_configure_cmt_timer_registers()
 * 7. Configure ICU interrupt vector priority and enable via internal_configure_cmt_interrupt()
 * 8. Save callback pointer and mark channel initialized via internal_save_cmt_callback()
 * 9. Start the timer via rx_cmt_start()
 *
 * **Clock divider selection:**
 * The driver iterates dividers /8, /32, /128, /512 and picks the first that
 * produces a period fitting in 16 bits:
 * @code
 * period = (PCLKB / divider) / frequency_hz
 * if (0 < period <= 65535): use this divider
 * @endcode
 *
 *
 *
 *
 * @pre config must point to a valid rx_cmt_config_t structure
 * @pre config->frequency_hz must be > 0 and <= PCLKB / 8 (7.5 MHz at 60 MHz PCLKB)
 * @pre channel must not be k_cmt_channel_0
 * @pre System clocks must be initialized (PCLKB = 60 MHz)
 *
 * @post Timer running at requested frequency
 * @post config->callback registered and called from ISR at each compare match
 * @post s_cmt_initialized[channel] set to true
 * @post ICU interrupt for the channel enabled at specified priority
 *
 * @note Not thread-safe - call once per channel during system initialization
 * @note Callback executes in interrupt context; keep it short and non-blocking
 * @warning CMT0 is reserved for ThreadX; do not use k_cmt_channel_0
 *
 * @par Thread Safety:
 * Not thread-safe. Call once per channel during initialization before starting
 * the RTOS scheduler or with interrupts disabled.
 *
 * @par Performance:
 * - Execution time: ~5 us (mostly register writes)
 * - Stack usage: 48 bytes
 *
 * @par Example:
 * @code{.c}
 * static void my_timer_cb(void* user_data)
 * {
 *   // Called at 1 kHz from interrupt context
 *   (void)user_data;
 *   toggle_led();
 * }
 *
 * const rx_cmt_config_t cfg = {
 *   .frequency_hz = 1000,         // 1 kHz
 *   .priority     = 5,
 *   .callback     = my_timer_cb,
 *   .user_data    = nullptr,
 * };
 *
 * rx_err_t err = rx_cmt_init(k_cmt_channel_1, &cfg);
 * if (err != k_rx_ok) {
 *   rx_log_error("APP", "CMT init failed");
 * }
 * @endcode
 *
 * @see rx_cmt_deinit() Stop and release channel
 * @see rx_cmt_start() Start a previously stopped channel
 * @see rx_cmt_stop() Stop the timer without deinitializing
 * @see rx_cmt_config_t Configuration structure definition
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cmt_init(const rx_cmt_channel_t channel, const rx_cmt_config_t* config)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_cmt_init_params(channel, config);
  if (err != k_rx_ok) {
    return err;
  }

  volatile rx_cmt_channel_regs_t* cmt = internal_get_cmt_base(channel);
  if (cmt == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate divider and compare value */
  uint8_t  divider;
  uint16_t cmcor;
  err = internal_calculate_cmt_params(config->frequency_hz, &divider, &cmcor);
  if (err != k_rx_ok) {
    return err;
  }

  /* Enable CMT module (clear module stop bit) */
  internal_enable_cmt_module_clock();

  /* Stop timer before configuration */
  err = rx_cmt_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure timer registers */
  internal_configure_cmt_timer_registers(cmt, divider, cmcor);

  /* Configure interrupt */
  err = internal_configure_cmt_interrupt(
    (rx_cmt_interrupt_config_t){.channel = channel, .priority = config->priority});
  if (err != k_rx_ok) {
    return err;
  }

  /* Save callback and mark initialized */
  internal_save_cmt_callback(channel, config);

  /* Start timer */
  err = rx_cmt_start(channel);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "CMT initialized");

  return k_rx_ok;
}

/**
 * @brief Start CMT counter for a previously initialized channel
 *
 * @details
 * Sets the appropriate bit in the CMSTR0 or CMSTR1 register to start the
 * CMT counter running. After setting the bit the register is read back to
 * verify the hardware accepted the write. Channels 0/1 share CMSTR0;
 * channels 2/3 share CMSTR1. Channels 0 and 1 use STR0/STR1 bits respectively,
 * as do channels 2 and 3.
 *
 *
 *
 * @pre channel must be initialized via rx_cmt_init()
 * @pre channel must be in range [0, 3]
 *
 * @post CMSTR bit for channel is set
 * @post Counter begins incrementing from its current value
 *
 * @note Usually called automatically by rx_cmt_init(); call explicitly only after rx_cmt_stop()
 * @note Performs a readback check to verify the hardware write succeeded
 *
 * @par Thread Safety:
 * Not thread-safe; do not call concurrently with stop/deinit on the same channel.
 *
 * @par Example:
 * @code{.c}
 * // Pause then resume timer
 * rx_cmt_stop(k_cmt_channel_1);
 * // ... do critical work ...
 * rx_cmt_start(k_cmt_channel_1);
 * @endcode
 *
 * @see rx_cmt_stop() Stop the counter
 * @see rx_cmt_init() Initialize channel (calls start internally)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cmt_start(const rx_cmt_channel_t channel)
{
  if ((uint8_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_cmt_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Set corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr0 |= bit_mask;
      const uint16_t cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_1: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr0 |= bit_mask;
      const uint16_t cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_2: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl1()->cmstr0 |= bit_mask;
      const uint16_t cmstr_value = cmt_ctrl1()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_3: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl1()->cmstr0 |= bit_mask;
      const uint16_t cmstr_value = cmt_ctrl1()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Stop CMT counter without deinitializing the channel
 *
 * @details
 * Clears the appropriate bit in CMSTR0 or CMSTR1 to halt the CMT counter.
 * After clearing the bit the register is read back to verify the hardware
 * accepted the write. The channel remains initialized; call rx_cmt_start()
 * to resume counting.
 *
 * Note that rx_cmt_stop() does NOT require the channel to be initialized
 * (s_cmt_initialized check is omitted intentionally so that rx_cmt_init()
 * can call it safely before marking the channel as initialized).
 *
 *
 *
 * @pre channel must be in range [0, 3]
 * @pre Interrupts may still fire if one is already pending when stop is called
 *
 * @post CMSTR bit for channel is cleared
 * @post Counter stops incrementing (current CMCNT value preserved)
 *
 * @note Does not clear the initialized flag - channel can be restarted
 * @note Called by rx_cmt_init() before programming registers (safe on uninitialized channel)
 *
 * @par Thread Safety:
 * Not thread-safe; do not call concurrently with start/init/deinit on the same channel.
 *
 * @par Example:
 * @code{.c}
 * // Temporarily suspend timer
 * rx_cmt_stop(k_cmt_channel_2);
 * reconfigure_something();
 * rx_cmt_start(k_cmt_channel_2);
 * @endcode
 *
 * @see rx_cmt_start() Resume counting
 * @see rx_cmt_deinit() Stop and release channel resources
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cmt_stop(const rx_cmt_channel_t channel)
{
  if ((uint8_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Clear corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr0 &= (uint16_t)~bit_mask;
      const uint16_t cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_1: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr0 &= (uint16_t)~bit_mask;
      const uint16_t cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_2: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl1()->cmstr0 &= (uint16_t)~bit_mask;
      const uint16_t cmstr_value = cmt_ctrl1()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_3: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl1()->cmstr0 &= (uint16_t)~bit_mask;
      const uint16_t cmstr_value = cmt_ctrl1()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Read current CMT counter (CMCNT) value
 *
 * @details
 * Returns the instantaneous value of the 16-bit CMCNT register for the
 * specified channel. The counter increments at PCLKB divided by the
 * configured divider (/8, /32, /128, or /512) and resets to 0 on compare
 * match. Useful for measuring elapsed time within a period.
 *
 *
 *
 *
 * @pre Channel must be initialized via rx_cmt_init()
 * @pre count must point to valid uint16_t storage
 *
 * @post *count set to CMCNT register value at time of read
 * @post Counter continues running (read is non-destructive)
 *
 * @note Value may change between consecutive reads if counter is running
 * @note For precise time measurements disable interrupts around consecutive reads
 *
 * @par Thread Safety:
 * Read-only; safe to call from multiple contexts, but value may be stale.
 *
 * @par Example:
 * @code{.c}
 * uint16_t ticks = 0;
 * rx_err_t err   = rx_cmt_get_count(k_cmt_channel_1, &ticks);
 * if (err == k_rx_ok) {
 *   // ticks = current counter position within the period
 * }
 * @endcode
 *
 * @see rx_cmt_init() Initialize channel and set frequency
 * @see rx_cmt_start() Start the counter
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cmt_get_count(const rx_cmt_channel_t channel, uint16_t* count)
{
  const volatile rx_cmt_channel_regs_t* cmt = nullptr;

  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is nullptr");

  if ((uint8_t)channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  cmt = internal_get_cmt_base(channel);
  if (cmt == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *count = cmt->cmcnt;
  return k_rx_ok;
}

/**
 * @brief Deinitialize CMT channel and release all resources
 *
 * @details
 * Stops the CMT counter, disables the ICU interrupt for the channel,
 * clears the stored callback and user data, and marks the channel as
 * uninitialized. After this call the channel may be re-initialized with
 * rx_cmt_init().
 *
 * **Algorithm steps:**
 * 1. Validate channel range
 * 2. Stop timer via rx_cmt_stop() and propagate any error
 * 3. Compute ICU vector, IER index, and IER bit for the channel
 * 4. Clear the IER bit to disable the interrupt
 * 5. Clear callback, user_data, and initialized flag
 *
 *
 *
 * @pre channel must be in range [0, 3]
 * @pre Caller should ensure no other task is using the channel callback
 *
 * @post Counter halted (CMSTR bit cleared)
 * @post ICU interrupt disabled for channel
 * @post s_cmt_initialized[channel] set to false
 * @post s_cmt_callback[channel] and s_cmt_user_data[channel] set to nullptr
 *
 * @note Safe to call on an uninitialized channel (rx_cmt_stop tolerates it)
 * @note Module clock is NOT re-asserted - hardware remains clocked for re-use
 *
 * @par Thread Safety:
 * Not thread-safe. Ensure no ISR or other thread is using the channel.
 *
 * @par Example:
 * @code{.c}
 * rx_err_t err = rx_cmt_deinit(k_cmt_channel_1);
 * if (err != k_rx_ok) {
 *   rx_log_error("APP", "CMT deinit failed");
 * }
 * @endcode
 *
 * @see rx_cmt_init() Re-initialize after deinit
 * @see rx_cmt_stop() Stop without full deinit
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cmt_deinit(const rx_cmt_channel_t channel)
{
  if ((uint8_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer and propagate any errors */
  rx_err_t err = rx_cmt_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable interrupt only for channels with fixed ICU vectors (CMI0/CMI1).
   * CMI2/CMI3 are SELECTB sources; interrupt wiring for those is not
   * implemented (see internal_configure_cmt_interrupt() for details). */
  if (channel == k_cmt_channel_0 || channel == k_cmt_channel_1) {
    const uint8_t vector    = k_vect_cmt0_cmi0 + channel;
    const uint8_t ier_index = vector / k_icu_ier_bits_per_reg;
    const uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;
    const uint8_t ier_mask  = (uint8_t)(k_cmt_bit_mask_lsb << ier_bit);
    icu()->ier[ier_index] &= (uint8_t)~ier_mask;
  }

  /* Clear callback */
  s_cmt_callback[channel]    = nullptr;
  s_cmt_user_data[channel]   = nullptr;
  s_cmt_initialized[channel] = false;

  rx_log_info(s_tag, "CMT deinitialized");

  return k_rx_ok;
}

#endif /* __RX__ */
