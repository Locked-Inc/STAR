/* lib/rx_hal/src/rx_irq_filter.c */

/**
 * @file rx_irq_filter.c
 * @brief IRQ Digital Filter Driver Implementation for RX72N
 *
 * Implements hardware digital filtering on IRQ pins using the ICU peripheral.
 *
 * Hardware Details:
 * - IRQFLTE[0]: Filter enable for IRQ0-7 (bits 0-7)
 * - IRQFLTE[1]: Filter enable for IRQ8-15 (bits 0-7)
 * - IRQFLTC[0]: Filter clock for IRQ0-7 (2 bits per IRQ)
 * - IRQFLTC[1]: Filter clock for IRQ8-15 (2 bits per IRQ)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_irq_filter.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/** @brief IRQ filter register organization constants */
typedef enum {
  k_irq_filter_irqs_per_reg    = 8,    /**< IRQs per register (IRQ0-7 in reg 0, IRQ8-15 in reg 1) */
  k_irq_filter_clock_bits      = 2,    /**< Bits per filter clock setting */
  k_irq_filter_clock_mask      = 0x03, /**< Mask for filter clock setting (2 bits) */
} irq_filter_constants_t;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_irq_filter_enable(uint8_t irq_num, rx_irq_filter_clk_t filter_clk)
{
  if (irq_num > RX_IRQ_MAX) {
    return k_rx_err_invalid_arg;
  }

  if (filter_clk > k_irq_filter_pclk_64) {
    return k_rx_err_invalid_arg;
  }

#ifdef __RX__
  /*
   * Determine which register to use:
   * - IRQ0-7:  IRQFLTE[0], IRQFLTC[0]
   * - IRQ8-15: IRQFLTE[1], IRQFLTC[1]
   */
  uint8_t reg_idx   = irq_num / k_irq_filter_irqs_per_reg;
  uint8_t bit_pos   = irq_num % k_irq_filter_irqs_per_reg;
  uint8_t clock_pos = bit_pos * k_irq_filter_clock_bits;

  /*
   * Set filter clock divisor first (before enabling)
   *
   * IRQFLTC[n] has 2 bits per IRQ:
   * - Bits [1:0]:   IRQ0/8 clock select
   * - Bits [3:2]:   IRQ1/9 clock select
   * - Bits [5:4]:   IRQ2/10 clock select
   * - etc.
   */
  uint16_t clock_mask  = (uint16_t)(k_irq_filter_clock_mask << clock_pos);
  uint16_t clock_value = (uint16_t)(filter_clk << clock_pos);

  ICU.irqfltc[reg_idx] = (ICU.irqfltc[reg_idx] & ~clock_mask) | clock_value;

  /*
   * Enable filter for this IRQ
   *
   * IRQFLTE[n] has 1 bit per IRQ:
   * - Bit 0: IRQ0/8 filter enable
   * - Bit 1: IRQ1/9 filter enable
   * - etc.
   */
  ICU.irqflte[reg_idx] |= (uint8_t)(1 << bit_pos);

#else
  /* Host-side stub for unit testing */
  (void)filter_clk;
#endif

  return k_rx_ok;
}

rx_err_t rx_irq_filter_disable(uint8_t irq_num)
{
  if (irq_num > RX_IRQ_MAX) {
    return k_rx_err_invalid_arg;
  }

#ifdef __RX__
  uint8_t reg_idx = irq_num / k_irq_filter_irqs_per_reg;
  uint8_t bit_pos = irq_num % k_irq_filter_irqs_per_reg;

  /* Clear filter enable bit */
  ICU.irqflte[reg_idx] &= (uint8_t)~(1 << bit_pos);

#endif

  return k_rx_ok;
}

rx_err_t rx_irq_filter_is_enabled(uint8_t irq_num, bool* enabled)
{
  if (irq_num > RX_IRQ_MAX) {
    return k_rx_err_invalid_arg;
  }

  if (enabled == NULL) {
    return k_rx_err_invalid_arg;
  }

#ifdef __RX__
  uint8_t reg_idx = irq_num / k_irq_filter_irqs_per_reg;
  uint8_t bit_pos = irq_num % k_irq_filter_irqs_per_reg;

  *enabled = (ICU.irqflte[reg_idx] & (1 << bit_pos)) != 0;

#else
  *enabled = false;
#endif

  return k_rx_ok;
}

rx_err_t rx_irq_filter_get_clock(uint8_t irq_num, rx_irq_filter_clk_t* filter_clk)
{
  if (irq_num > RX_IRQ_MAX) {
    return k_rx_err_invalid_arg;
  }

  if (filter_clk == NULL) {
    return k_rx_err_invalid_arg;
  }

#ifdef __RX__
  uint8_t reg_idx   = irq_num / k_irq_filter_irqs_per_reg;
  uint8_t bit_pos   = irq_num % k_irq_filter_irqs_per_reg;
  uint8_t clock_pos = bit_pos * k_irq_filter_clock_bits;

  uint16_t clock_value = (ICU.irqfltc[reg_idx] >> clock_pos) & k_irq_filter_clock_mask;
  *filter_clk          = (rx_irq_filter_clk_t)clock_value;

#else
  *filter_clk = k_irq_filter_pclk_1;
#endif

  return k_rx_ok;
}
