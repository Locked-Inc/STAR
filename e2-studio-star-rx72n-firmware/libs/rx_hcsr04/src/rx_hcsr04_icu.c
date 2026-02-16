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
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.2.0 (Issue #296)
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
 * @brief ICU configuration constants
 */
typedef enum : uint8_t {
  k_irq_min          = 8,    /**< Minimum IRQ number (IRQ8 = P00) */
  k_irq_max          = 15,   /**< Maximum IRQ number (IRQ15 = P07) */
  k_priority_min     = 1,    /**< Minimum interrupt priority */
  k_priority_max     = 15,   /**< Maximum interrupt priority */
  k_irqcr_both_edges = 0x08, /**< Both edges trigger interrupt */
  k_vector_base      = 64,   /**< IRQ vector base (IRQ0 = vector 64) */
  k_bits_per_ier     = 8,    /**< IER register is 8 bits wide */
} icu_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* s_tag = "HCSR04_ICU"; /**< Logging tag */

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

rx_err_t rx_hcsr04_icu_configure(const uint8_t irq_num, const uint8_t priority)
{
  /* Validate IRQ number (8-15 for P00-P07) */
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
   * IRQ8-15 use vectors 72-79 (base 64 + irq_num) */
  const uint8_t vector = k_vector_base + irq_num;

  /* Step 4: Clear any pending interrupt flag */
  icu()->ir[vector] = 0;

  /* Step 5: Set interrupt priority */
  icu()->ipr[vector] = priority;

  /* Step 6: Enable interrupt in IER register
   * IER is array of 8-bit registers, need to calculate index and bit position */
  const uint8_t ier_index = vector / k_bits_per_ier;
  const uint8_t ier_bit   = vector % k_bits_per_ier;
  icu()->ier[ier_index] |= (1 << ier_bit);

  rx_log_info(s_tag, "Configured IRQ for HC-SR04");

  return k_rx_ok;
}

rx_err_t rx_hcsr04_icu_disable(const uint8_t irq_num)
{
  /* Validate IRQ number (8-15 for P00-P07) */
  RX_CHECK_RANGE(irq_num, k_irq_min, k_irq_max, k_rx_err_invalid_arg);

  /* Calculate vector number */
  const uint8_t vector = k_vector_base + irq_num;

  /* Disable interrupt in IER register */
  const uint8_t ier_index = vector / k_bits_per_ier;
  const uint8_t ier_bit   = vector % k_bits_per_ier;
  icu()->ier[ier_index] &= ~(1 << ier_bit);

  /* Clear any pending interrupt flag */
  icu()->ir[vector] = 0;

  /* Disable digital filter */
  const rx_err_t err = rx_irq_filter_disable(irq_num);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Failed to disable digital filter");
    /* Continue anyway - interrupt is disabled */
  }

  rx_log_info(s_tag, "Disabled IRQ");

  return k_rx_ok;
}
