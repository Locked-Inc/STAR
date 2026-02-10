/* lib/rx_hal/src/rx_bms_alert.c */

/**
 * @file rx_bms_alert.c
 * @brief BMS Alert Interrupt Driver Implementation (IRQ13, Vector 77)
 *
 * @details
 * Configures the ICU for BQ4050 ALERT pin interrupt on IRQ13 (P05).
 * The BQ4050 asserts ALERT low on battery fault conditions including
 * over-temperature, over-charge, and terminate-discharge alarms.
 *
 * The ISR triggers an immediate emergency stop via shared_data and sets
 * a software flag that the BMS polling task can check for further
 * fault diagnosis.
 *
 * ## ISR Flow
 *
 * ```
 * BQ4050 ALERT pin goes LOW
 *         |
 * ICU digital filter (3 samples @ PCLK/32 = 1.6 us)
 *         |
 * IRQ13 ISR (vector 77)
 *   1. Clear IR[77] flag
 *   2. Set s_alert_triggered = true
 *   3. Trigger e-stop (k_estop_reason_battery_fault)
 *   4. Return (~4 us total)
 * ```
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 3: No dynamic memory (static variables only)
 * - Rule 4: All functions < 60 lines
 * - Rule 5: 2+ validation checks per function
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, no magic numbers
 * - Rule 10: -Wall -Wextra -Werror clean
 *
 * @see rx_bms_alert.h Public API
 * @see rx_poeg.c POEG ISR (same interrupt pattern)
 * @see bms_monitor_task.c BMS polling task
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "rx_bms_alert.h"

#include "rx72n_icu_regs.h"
#include "rx_irq_filter.h"
#include "rx_log.h"
#include "shared_data.h"

static const char* s_tag = "BMS_ALERT";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief ICU configuration constants for BMS alert */
typedef enum : uint8_t {
  k_bms_alert_irq_num    = 13,   /**< External IRQ number (IRQ13) */
  k_bms_alert_vector     = 77,   /**< ICU vector number (64 + 13) */
  k_bms_alert_priority   = 13,   /**< Interrupt priority (high, below POEG=14) */
  k_icu_ir_clear         = 0,    /**< Write 0 to clear IR flag */
  k_ier_bit_enable_base  = 1,    /**< Base value for IER bit shift */
  k_ier_bits_per_reg     = 8,    /**< 8 interrupt enables per IER register */
  k_irqcr_falling_edge   = 0x04, /**< IRQCR[n] bits[3:2]=01 for falling edge */
} bms_alert_icu_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Initialization flag */
static bool s_initialized = false;

/**
 * @brief Alert triggered flag (set by ISR, cleared by rx_bms_alert_clear)
 * @note Declared volatile for ISR-to-task communication
 */
static volatile bool s_alert_triggered = false;

/* =============================================================================
 * ISR Handler
 * =============================================================================
 */

/**
 * @brief IRQ13 ISR for BQ4050 ALERT pin (vector 77)
 *
 * @details
 * Called when the BQ4050 asserts ALERT low (battery fault). Clears
 * the interrupt, sets the software alert flag, and triggers emergency stop.
 *
 * @pre IRQ13 enabled via rx_bms_alert_init()
 * @post s_alert_triggered set to true
 * @post Emergency stop triggered
 *
 * @note ISR context, runs at priority 13
 * @since Version 1.0.0
 */
void __attribute__((interrupt)) irq13_bms_alert_isr(void);
void __attribute__((interrupt)) irq13_bms_alert_isr(void)
{
  /* Clear ICU interrupt request flag immediately */
  icu()->ir[k_bms_alert_vector] = k_icu_ir_clear;

  /* Set software alert flag for polling task */
  s_alert_triggered = true;

  /* Log fault (brief - ISR context) */
  rx_log_error(s_tag, "BQ4050 ALERT - battery fault detected");

  /* Trigger emergency stop */
  (void)shared_data_trigger_estop(k_estop_reason_battery_fault);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bms_alert_init(void)
{
  if (s_initialized) {
    rx_log_error(s_tag, "Already initialized");
    return k_rx_err_invalid_state;
  }

  volatile rx_icu_regs_t* icu_regs = icu();

  /* Step 1: Clear any pending interrupt */
  icu_regs->ir[k_bms_alert_vector] = k_icu_ir_clear;

  /* Step 2: Configure IRQ13 for falling edge detection */
  icu_regs->irqcr[k_bms_alert_irq_num] = k_irqcr_falling_edge;

  /* Step 3: Enable digital filter at PCLK/32 for noise rejection */
  rx_err_t err = rx_irq_filter_enable(k_bms_alert_irq_num, k_irq_filter_pclk_32);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "IRQ filter enable failed");
    return err;
  }

  /* Step 4: Set interrupt priority */
  icu_regs->ipr[k_bms_alert_vector] = k_bms_alert_priority;

  /* Step 5: Enable in IER register */
  const uint8_t ier_idx  = (uint8_t)(k_bms_alert_vector / k_ier_bits_per_reg);
  const uint8_t ier_bit  = (uint8_t)(k_bms_alert_vector % k_ier_bits_per_reg);
  const uint8_t ier_mask = (uint8_t)(k_ier_bit_enable_base << ier_bit);
  icu_regs->ier[ier_idx] |= ier_mask;

  s_alert_triggered = false;
  s_initialized     = true;

  rx_log_info(s_tag, "BMS alert IRQ13 initialized (falling edge, priority 13)");
  return k_rx_ok;
}

rx_err_t rx_bms_alert_deinit(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  volatile rx_icu_regs_t* icu_regs = icu();

  /* Disable in IER register */
  const uint8_t ier_idx  = (uint8_t)(k_bms_alert_vector / k_ier_bits_per_reg);
  const uint8_t ier_bit  = (uint8_t)(k_bms_alert_vector % k_ier_bits_per_reg);
  const uint8_t ier_mask = (uint8_t)(k_ier_bit_enable_base << ier_bit);
  icu_regs->ier[ier_idx] &= (uint8_t)~ier_mask;

  /* Clear pending interrupt */
  icu_regs->ir[k_bms_alert_vector] = k_icu_ir_clear;

  /* Disable digital filter */
  (void)rx_irq_filter_disable(k_bms_alert_irq_num);

  s_alert_triggered = false;
  s_initialized     = false;

  rx_log_info(s_tag, "BMS alert IRQ13 deinitialized");
  return k_rx_ok;
}

rx_err_t rx_bms_alert_is_triggered(bool* triggered)
{
  if (triggered == nullptr) {
    return k_rx_err_null_ptr;
  }

  *triggered = s_alert_triggered;
  return k_rx_ok;
}

rx_err_t rx_bms_alert_clear(void)
{
  s_alert_triggered = false;
  return k_rx_ok;
}
