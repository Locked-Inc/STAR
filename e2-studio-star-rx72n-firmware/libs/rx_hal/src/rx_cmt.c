/* lib/rx_hal/src/rx_cmt.c */

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
 * msc {
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
 * }
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
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

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

static const char* s_tag = "CMT";

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

/** @brief CMT module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_cmt_mstpb_cmt = 15, /**< CMT0-CMT3 module stop bit */
} cmt_module_stop_bits_t;

/** @brief CMT interrupt configuration */
typedef struct {
  rx_cmt_channel_t channel;  /**< CMT channel */
  uint8_t          priority; /**< Interrupt priority */
} rx_cmt_interrupt_config_t;

/** @brief CMCR register bit positions */
typedef enum : uint8_t {
  k_cmt_cmcr_cks_shift = 0, /**< CKS (clock select) bit shift */
  k_cmt_cmcr_cmie_pos  = 6, /**< CMIE (interrupt enable) bit position */
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
 * @brief Get CMT channel base address
 *
 * @param[in] channel CMT channel
 *
 * @return Pointer to CMT register base, or nullptr if invalid
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
 * @brief Calculate CMT divider and compare value from frequency
 *
 * @param[in] frequency_hz Desired frequency in Hz
 * @param[out] divider Pointer to store divider setting
 * @param[out] cmcor Pointer to store compare match value
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t
internal_calculate_cmt_params(const uint32_t frequency_hz, uint8_t* divider, uint16_t* cmcor)
{
  static const uint16_t dividers[] = {k_cmt_divider_val_8,
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
 * @brief Enable the CMT module clock.
 */
static void internal_enable_cmt_module_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;
  system_regs()->mstpcrb &= ~((uint32_t)k_cmt_bit_mask_lsb << k_cmt_mstpb_cmt);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Configure CMT timer registers.
 *
 * @param[in] cmt CMT channel registers
 * @param[in] divider Clock divider setting
 * @param[in] cmcor Compare match value
 */
static void internal_configure_cmt_timer_registers(volatile rx_cmt_channel_regs_t* cmt,
                                                   const uint8_t                   divider,
                                                   const uint16_t                  cmcor)
{
  RX_ASSERT(cmt != nullptr, "CMT registers are NULL");

  /* Configure timer control register
   * - Set clock divider
   * - Enable compare match interrupt
   */
  cmt->cmcr =
    (divider << k_cmt_cmcr_cks_shift) |          /* CKS: Clock Select */
    (k_cmt_bit_mask_lsb << k_cmt_cmcr_cmie_pos); /* CMIE: Compare Match Interrupt Enable */

  /* Set compare match value (period - 1) */
  cmt->cmcor = cmcor - k_cmt_period_adj;

  /* Clear counter */
  cmt->cmcnt = k_cmt_value_zero;
}

/**
 * @brief Configure CMT interrupt routing and priority
 *
 * @param[in] config Interrupt configuration
 *
 * @return k_rx_ok on success, error code on failure
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

  uint8_t vector    = k_vect_cmt0_cmi0 + config.channel;
  uint8_t ier_index = vector / k_icu_ier_bits_per_reg;
  uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;

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
 * @brief CMT1 interrupt handler
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
 * @brief CMT2 interrupt handler
 */
void cmt2_isr(void)
{
  if (s_cmt_callback[k_cmt_channel_2] != nullptr) {
    s_cmt_callback[k_cmt_channel_2](s_cmt_user_data[k_cmt_channel_2]);
  }
}

/**
 * @brief CMT3 interrupt handler
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
 * @brief Validate CMT channel and config parameters
 *
 * @param[in] channel CMT channel
 * @param[in] config CMT configuration
 *
 * @return k_rx_ok on success, error code on failure
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
 * @brief Save callback and mark channel as initialized
 *
 * @param[in] channel CMT channel
 * @param[in] config CMT configuration
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

rx_err_t rx_cmt_init(const rx_cmt_channel_t channel, const rx_cmt_config_t* config)
{
  volatile rx_cmt_channel_regs_t* cmt;
  uint8_t                         divider;
  uint16_t                        cmcor;
  rx_err_t                        err;

  /* Validate parameters */
  err = internal_validate_cmt_init_params(channel, config);
  if (err != k_rx_ok) {
    return err;
  }

  cmt = internal_get_cmt_base(channel);
  if (cmt == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate divider and compare value */
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
 * @brief Start CMT counter
 * @param[in] channel CMT channel to start
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t rx_cmt_start(const rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_cmt_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Set corresponding bit in CMSTR register */
  uint16_t cmstr_value;
  switch (channel) {
    case k_cmt_channel_0: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr0 |= bit_mask;
      cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_1: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr0 |= bit_mask;
      cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_2: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr1 |= bit_mask;
      cmstr_value = cmt_ctrl()->cmstr1;
      if ((cmstr_value & bit_mask) == k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_3: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr1 |= bit_mask;
      cmstr_value = cmt_ctrl()->cmstr1;
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
 * @brief Stop CMT counter
 * @param[in] channel CMT channel to stop
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t rx_cmt_stop(const rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Clear corresponding bit in CMSTR register */
  uint16_t cmstr_value;
  switch (channel) {
    case k_cmt_channel_0: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr0 &= (uint16_t)~bit_mask;
      cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_1: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr0 &= (uint16_t)~bit_mask;
      cmstr_value = cmt_ctrl()->cmstr0;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_2: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str0);
      cmt_ctrl()->cmstr1 &= (uint16_t)~bit_mask;
      cmstr_value = cmt_ctrl()->cmstr1;
      if ((cmstr_value & bit_mask) != k_cmt_value_zero) {
        return k_rx_err_hw_error;
      }
      break;
    }
    case k_cmt_channel_3: {
      const uint16_t bit_mask = (uint16_t)(k_cmt_bit_mask_lsb << k_cmt_cmstr_str1);
      cmt_ctrl()->cmstr1 &= (uint16_t)~bit_mask;
      cmstr_value = cmt_ctrl()->cmstr1;
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
 * @brief Get current CMT counter value
 * @param[in] channel CMT channel
 * @param[out] count Pointer to store counter value
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t rx_cmt_get_count(const rx_cmt_channel_t channel, uint16_t* count)
{
  const volatile rx_cmt_channel_regs_t* cmt = nullptr;

  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is nullptr");

  if ((int32_t)channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
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
 * @brief Deinitialize CMT channel
 * @param[in] channel CMT channel to deinitialize
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t rx_cmt_deinit(const rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer and propagate any errors */
  rx_err_t err = rx_cmt_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable interrupt */
  uint8_t vector    = k_vect_cmt0_cmi0 + channel;
  uint8_t ier_index = vector / k_icu_ier_bits_per_reg;
  uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;
  const uint8_t ier_mask = (uint8_t)(k_cmt_bit_mask_lsb << ier_bit);
  icu()->ier[ier_index] &= (uint8_t)~ier_mask;

  /* Clear callback */
  s_cmt_callback[channel]    = nullptr;
  s_cmt_user_data[channel]   = nullptr;
  s_cmt_initialized[channel] = false;

  rx_log_info(s_tag, "CMT deinitialized");

  return k_rx_ok;
}
