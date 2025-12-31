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
 * @see RX72N Hardware Manual, Section 13.3.15 - IRQ Digital Filter
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "rx_irq_filter.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/** @brief IRQs per register (IRQ0-7 in register 0, IRQ8-15 in register 1) */
#define IRQS_PER_REGISTER 8

/** @brief Bits per filter clock setting */
#define FILTER_CLOCK_BITS 2

/** @brief Mask for filter clock setting */
#define FILTER_CLOCK_MASK 0x03

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_irq_filter_enable(uint8_t irq_num, rx_irq_filter_clk_t filter_clk)
{
  if (irq_num > RX_IRQ_MAX) {
    return RX_ERR_INVALID_ARG;
  }

  if (filter_clk > k_irq_filter_pclk_64) {
    return RX_ERR_INVALID_ARG;
  }

#ifdef __RX__
  /*
   * Determine which register to use:
   * - IRQ0-7:  IRQFLTE[0], IRQFLTC[0]
   * - IRQ8-15: IRQFLTE[1], IRQFLTC[1]
   */
  uint8_t reg_idx   = irq_num / IRQS_PER_REGISTER;
  uint8_t bit_pos   = irq_num % IRQS_PER_REGISTER;
  uint8_t clock_pos = bit_pos * FILTER_CLOCK_BITS;

  /*
   * Set filter clock divisor first (before enabling)
   *
   * IRQFLTC[n] has 2 bits per IRQ:
   * - Bits [1:0]:   IRQ0/8 clock select
   * - Bits [3:2]:   IRQ1/9 clock select
   * - Bits [5:4]:   IRQ2/10 clock select
   * - etc.
   */
  uint16_t clock_mask  = (uint16_t)(FILTER_CLOCK_MASK << clock_pos);
  uint16_t clock_value = (uint16_t)(filter_clk << clock_pos);

  ICU.IRQFLTC[reg_idx] = (ICU.IRQFLTC[reg_idx] & ~clock_mask) | clock_value;

  /*
   * Enable filter for this IRQ
   *
   * IRQFLTE[n] has 1 bit per IRQ:
   * - Bit 0: IRQ0/8 filter enable
   * - Bit 1: IRQ1/9 filter enable
   * - etc.
   */
  ICU.IRQFLTE[reg_idx] |= (uint8_t)(1 << bit_pos);

#else
  /* Host-side stub for unit testing */
  (void)filter_clk;
#endif

  return RX_OK;
}

rx_err_t rx_irq_filter_disable(uint8_t irq_num)
{
  if (irq_num > RX_IRQ_MAX) {
    return RX_ERR_INVALID_ARG;
  }

#ifdef __RX__
  uint8_t reg_idx = irq_num / IRQS_PER_REGISTER;
  uint8_t bit_pos = irq_num % IRQS_PER_REGISTER;

  /* Clear filter enable bit */
  ICU.IRQFLTE[reg_idx] &= (uint8_t)~(1 << bit_pos);

#endif

  return RX_OK;
}

rx_err_t rx_irq_filter_is_enabled(uint8_t irq_num, bool* enabled)
{
  if (irq_num > RX_IRQ_MAX) {
    return RX_ERR_INVALID_ARG;
  }

  if (enabled == NULL) {
    return RX_ERR_INVALID_ARG;
  }

#ifdef __RX__
  uint8_t reg_idx = irq_num / IRQS_PER_REGISTER;
  uint8_t bit_pos = irq_num % IRQS_PER_REGISTER;

  *enabled = (ICU.IRQFLTE[reg_idx] & (1 << bit_pos)) != 0;

#else
  *enabled = false;
#endif

  return RX_OK;
}

rx_err_t rx_irq_filter_get_clock(uint8_t irq_num, rx_irq_filter_clk_t* filter_clk)
{
  if (irq_num > RX_IRQ_MAX) {
    return RX_ERR_INVALID_ARG;
  }

  if (filter_clk == NULL) {
    return RX_ERR_INVALID_ARG;
  }

#ifdef __RX__
  uint8_t reg_idx   = irq_num / IRQS_PER_REGISTER;
  uint8_t bit_pos   = irq_num % IRQS_PER_REGISTER;
  uint8_t clock_pos = bit_pos * FILTER_CLOCK_BITS;

  uint16_t clock_value = (ICU.IRQFLTC[reg_idx] >> clock_pos) & FILTER_CLOCK_MASK;
  *filter_clk          = (rx_irq_filter_clk_t)clock_value;

#else
  *filter_clk = k_irq_filter_pclk_1;
#endif

  return RX_OK;
}
