/**
 * @file rx_hcsr04_icu.c
 * @brief HC-SR04 ICU (Interrupt Controller Unit) Configuration Implementation
 *
 * @details
 * Implements ICU configuration for HC-SR04 ultrasonic sensors operating
 * in IRQ mode with hardware edge detection.
 *
 * **Configuration Steps:**
 * 1. Validate IRQ number and priority
 * 2. Configure IRQCR for both-edge detection
 * 3. Enable digital filter (PCLK/8 = 133ns @ 60MHz)
 * 4. Clear pending interrupt flag
 * 5. Set interrupt priority
 * 6. Enable interrupt in IER register
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, no recursion, all control flow via if/for/while
 * - Rule 2: No unbounded loops; all operations are O(1) register writes
 * - Rule 3: No dynamic allocation; all storage in automatic variables
 * - Rule 4: Functions are short (<25 lines each)
 * - Rule 5: 2+ preconditions and postconditions per public function
 * - Rule 7: All register writes are unconditional; rx_irq_filter_enable return checked
 * - Rule 8: All constants are named C23 typed enums (icu_constants_t)
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: Only ICU enable/disable; pin config in rx_mpc, ISR in rx_hcsr04_isr
 * - Dependency Inversion: Uses rx_irq_filter.h abstraction for digital filter configuration
 *
 * @author Locked, Inc.
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 * @version 1.0.0
 */

#include "rx_hcsr04_icu.h"

#include "rx72n_icu_regs.h"
#include "rx_check.h"
#include "rx_irq_filter.h"
#include "rx_log.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum icu_constants_t
 * @brief ICU configuration constants for HC-SR04 edge detection
 *
 * @details
 * Named constants for ICU (Interrupt Controller Unit) register configuration.
 * All magic numbers in register operations must reference this enum per the
 * project "No Magic Numbers" policy.
 *
 * **Register Background (RX72N Manual Chapter 15):**
 * - IRQCR[n] controls edge sensitivity for IRQn
 * - IER[byte][bit] enables interrupt; byte = vector/8, bit = vector%8
 * - IR[vector] is the pending-flag register; write 0 to clear
 * - IPR[vector] sets interrupt priority 1-15 (0 = disabled)
 *
 * @invariant k_irq_min <= all valid IRQ numbers <= k_irq_max
 * @invariant k_priority_min <= all valid priorities <= k_priority_max
 * @invariant k_vector_base + k_irq_max == 75 (last HC-SR04 vector; IRQ11 = vector 75)
 *
 * @code
 * // Access all constants in this enum via named identifiers:
 * const uint8_t vector    = k_vector_base + irq_num;  // e.g. 72-75 (IRQ8-11)
 * const uint8_t ier_index = vector / k_bits_per_ier;
 * const uint8_t ier_bit   = vector % k_bits_per_ier;
 * icu()->ier[ier_index] |= (k_bit_base << ier_bit);
 * icu()->ir[vector]      = k_ir_flag_clear;
 * @endcode
 *
 * @see rx_hcsr04_icu_configure() Uses all these constants
 * @see rx_hcsr04_icu_disable() Uses k_vector_base, k_bits_per_ier, k_ir_flag_clear, k_ier_bit_clear
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_irq_min          = 8,  /**< Minimum IRQ number (IRQ8 = P00) */
  k_irq_max          = 11, /**< Maximum IRQ number (IRQ11 = P03; only ISR handlers IRQ8-11 exist) */
  k_priority_min     = 1,  /**< Minimum interrupt priority */
  k_priority_max     = 15, /**< Maximum interrupt priority */
  k_irqcr_both_edges = 0x08, /**< Both edges trigger interrupt (IRQCR[n] value) */
  k_vector_base      = 64,   /**< IRQ vector base (IRQ0 = vector 64) */
  k_bits_per_ier     = 8,    /**< IER register is 8 bits wide */
  k_bit_base         = 1,    /**< Bit value 1 for IER enable mask construction */
  k_ir_flag_clear    = 0,    /**< Write 0 to IR register to clear pending flag */
  k_ier_bit_clear    = 0,    /**< IER bit value when interrupt is disabled (not enabled) */
} icu_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* const s_tag = "HCSR04_ICU"; /**< Logging tag (pointer and data both const) */

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Configure ICU for HC-SR04 echo IRQ measurement
 *
 * @details
 * Configures IRQ edge detection, digital filter, interrupt priority, and
 * enables the interrupt in the IER register. Must be called after
 * rx_mpc_set_irq() has put the pin in IRQ input mode.
 *
 * **Algorithm Steps:**
 * 1. Validate IRQ number in range [k_irq_min, k_irq_max]
 * 2. Validate priority in range [k_priority_min, k_priority_max]
 * 3. Write k_irqcr_both_edges to IRQCR[irq_num]
 * 4. Enable digital filter via rx_irq_filter_enable()
 * 5. Calculate vector = k_vector_base + irq_num
 * 6. Clear IR[vector] flag (discard any pending interrupt)
 * 7. Write priority to IPR[vector]
 * 8. Set IER[vector/8] bit (vector%8) to enable interrupt
 *
 *
 *
 * @pre Pin already configured for IRQ function via rx_mpc_set_irq()
 * @pre ICU module not in module stop (MSTPCRA bit cleared)
 *
 * @post IRQCR[irq_num] set for both-edge detection
 * @post Digital filter enabled at PCLK/8
 * @post IR[vector] cleared (no stale pending interrupt)
 * @post IPR[vector] contains priority
 * @post IER[vector/8] bit set (interrupt enabled)
 *
 * @note Not thread-safe. Call during initialization only.
 * @warning Do not call while a measurement is in progress.
 *
 * @par Example
 * @code
 * // Configure P03 (IRQ11) for HC-SR04 echo detection
 * rx_err_t err = rx_mpc_set_irq(k_rx_p0_3);    // Pin -> IRQ function first
 * if (err != k_rx_ok) { return err; }
 * err = rx_hcsr04_icu_configure((uint8_t)k_hcsr04_irq_11, k_hcsr04_irq_priority_default);
 * if (err != k_rx_ok) { return err; }
 * @endcode
 *
 * @see rx_mpc_set_irq() Configure pin before calling this
 * @see rx_hcsr04_icu_disable() Reverse this configuration
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_icu_configure(const uint8_t irq_num, const uint8_t priority)
{
  /* Validate IRQ number (8-11 for P00-P03) */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Validate priority (1-15) */
  RX_CHECK_RANGE(priority, k_priority_min, k_priority_max, k_rx_err_invalid_arg);

  /* Step 1: Configure IRQCR for both-edge detection */
  icu()->irqcr[irq_num] = k_irqcr_both_edges;

  /* Step 2: Enable digital filter (PCLK/8 = 133ns @ 60MHz PCLKB)
   * Provides noise immunity while maintaining microsecond-level timing */
  rx_err_t err = rx_irq_filter_enable(irq_num, k_irq_filter_pclk_8);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to enable digital filter for IRQ");
    return err;
  }

  /* Step 3: Calculate vector number for this IRQ
   * IRQ8-11 use vectors 72-75 (base 64 + irq_num) */
  const uint8_t vector = k_vector_base + irq_num;

  /* Step 4: Clear any pending interrupt flag */
  icu()->ir[vector] = k_ir_flag_clear;

  /* Step 5: Set interrupt priority */
  icu()->ipr[vector] = priority;

  /* Step 6: Enable interrupt in IER register
   * IER is array of 8-bit registers, need to calculate index and bit position */
  const uint8_t ier_index = vector / k_bits_per_ier;
  const uint8_t ier_bit   = vector % k_bits_per_ier;
  icu()->ier[ier_index] |= (uint8_t)(k_bit_base << ier_bit);

  rx_log_info(s_tag, "Configured IRQ for HC-SR04");

  return k_rx_ok;
}

/**
 * @brief Disable HC-SR04 echo IRQ in ICU
 *
 * @details
 * Disables the specified IRQ interrupt and digital filter. Performs all
 * cleanup steps then returns the first error encountered (if any).
 *
 * **Algorithm Steps:**
 * 1. Validate IRQ number in range [k_irq_min, k_irq_max]
 * 2. Calculate vector = k_vector_base + irq_num
 * 3. Clear IER[vector/8] bit (vector%8) to disable interrupt
 * 4. Write k_ir_flag_clear to IR[vector] to clear any pending flag
 * 5. Disable digital filter via rx_irq_filter_disable() and return its error
 *
 *
 *
 * @pre IRQ was previously enabled via rx_hcsr04_icu_configure() (IER bit must be set)
 * @pre No measurement in progress on this IRQ
 *
 * @post IER bit cleared (interrupt will not fire)
 * @post IR flag cleared (no stale pending interrupt)
 * @post Digital filter disabled
 * @post ISR will not trigger on pin edges
 *
 * @note Only safe to call after rx_hcsr04_icu_configure() has enabled the IRQ
 * @note Does NOT reconfigure pin back to GPIO (use rx_mpc_set_gpio separately)
 * @warning Do not call while measurement in progress
 * @warning Returns k_rx_err_invalid_state if IRQ was not enabled; call only after configure()
 *
 * @par Example
 * @code
 * rx_err_t err = rx_hcsr04_icu_disable((uint8_t)k_hcsr04_irq_11);
 * if (err != k_rx_ok) { rx_log_warn("SONAR", "Failed to disable IRQ11"); }
 * @endcode
 *
 * @see rx_hcsr04_icu_configure() Enable IRQ
 * @see rx_mpc_set_gpio() Reconfigure pin back to GPIO mode
 *
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_icu_disable(const uint8_t irq_num)
{
  /* Validate IRQ number (8-11 for P00-P03) */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Calculate vector number and IER location */
  const uint8_t vector    = k_vector_base + irq_num;
  const uint8_t ier_index = vector / k_bits_per_ier;
  const uint8_t ier_bit   = vector % k_bits_per_ier;

  /* Second precondition: verify the IRQ is actually enabled before clearing it */
  if ((icu()->ier[ier_index] & (uint8_t)(k_bit_base << ier_bit)) == k_ier_bit_clear) {
    return k_rx_err_invalid_state; /* IRQ not enabled; rx_hcsr04_icu_configure() not called */
  }

  /* Disable interrupt in IER register */
  icu()->ier[ier_index] &= (uint8_t) ~(k_bit_base << ier_bit);

  /* Clear any pending interrupt flag */
  icu()->ir[vector] = k_ir_flag_clear;

  /* Disable digital filter - propagate error to caller */
  const rx_err_t err = rx_irq_filter_disable(irq_num);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Failed to disable digital filter for IRQ");
    return err;
  }

  rx_log_info(s_tag, "Disabled IRQ");

  return k_rx_ok;
}
