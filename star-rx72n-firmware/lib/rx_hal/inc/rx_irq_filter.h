/**
 * @file rx_irq_filter.h
 * @brief IRQ Digital Filter Driver for RX72N
 *
 * Provides digital noise filtering for external interrupt (IRQ) pins.
 * This helps filter out electrical noise, switch bounce, and transient
 * glitches that could cause spurious interrupts.
 *
 * The RX72N provides hardware digital filters on all 16 IRQ pins (IRQ0-15).
 * Each filter samples the input at a configurable rate and requires
 * a stable level for 3 consecutive samples to register as a valid transition.
 *
 * Common use cases:
 * - E-STOP button debouncing
 * - VBUS detection noise rejection
 * - Battery charger interrupt filtering
 * - Hall encoder noise filtering
 *
 * Usage:
 * @code
 * // Enable filtering on E-STOP button (IRQ0) with maximum filtering
 * rx_irq_filter_enable(0, k_irq_filter_pclk_64);
 *
 * // Disable filter when no longer needed
 * rx_irq_filter_disable(0);
 * @endcode
 *
 * @see RX72N Hardware Manual, Section 13 - Interrupt Controller Unit (ICU)
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#ifndef STAR_RX_IRQ_FILTER_H
#define STAR_RX_IRQ_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief IRQ filter clock divisor options
 *
 * Higher divisors provide more filtering (slower sampling = more noise rejection)
 * but also slower response to valid transitions.
 *
 * Filter response time = 3 samples * (PCLKB / divisor)
 * With PCLKB = 60 MHz:
 * - PCLK/1:   50ns per sample, 150ns response
 * - PCLK/8:   133ns per sample, 400ns response
 * - PCLK/32:  533ns per sample, 1.6us response
 * - PCLK/64:  1.07us per sample, 3.2us response
 */
typedef enum {
  k_irq_filter_pclk_1  = 0, /**< PCLKB/1 (fastest response, minimal filtering) */
  k_irq_filter_pclk_8  = 1, /**< PCLKB/8 (good for fast signals) */
  k_irq_filter_pclk_32 = 2, /**< PCLKB/32 (moderate filtering) */
  k_irq_filter_pclk_64 = 3, /**< PCLKB/64 (maximum filtering, best for buttons) */
} rx_irq_filter_clk_t;

/**
 * @brief Maximum IRQ number (0-15)
 */
#define RX_IRQ_MAX 15

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Enable digital filter on an IRQ pin
 *
 * Enables the hardware digital filter with the specified clock divisor.
 * The filter requires 3 consecutive samples at the same level to
 * recognize a valid transition.
 *
 * @param[in] irq_num IRQ number (0-15)
 * @param[in] filter_clk Clock divisor for filter sampling
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if irq_num > 15
 *
 * @note Filter applies to all edge types (rising, falling, both)
 * @note Filter continues operating during standby modes
 */
rx_err_t rx_irq_filter_enable(uint8_t irq_num, rx_irq_filter_clk_t filter_clk);

/**
 * @brief Disable digital filter on an IRQ pin
 *
 * Disables the hardware digital filter for the specified IRQ.
 * Raw unfiltered input is passed through.
 *
 * @param[in] irq_num IRQ number (0-15)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if irq_num > 15
 */
rx_err_t rx_irq_filter_disable(uint8_t irq_num);

/**
 * @brief Check if filter is enabled on an IRQ pin
 *
 * @param[in]  irq_num IRQ number (0-15)
 * @param[out] enabled Pointer to receive enable status
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if irq_num > 15 or enabled is NULL
 */
rx_err_t rx_irq_filter_is_enabled(uint8_t irq_num, bool* enabled);

/**
 * @brief Get current filter clock divisor for an IRQ
 *
 * @param[in]  irq_num IRQ number (0-15)
 * @param[out] filter_clk Pointer to receive clock divisor setting
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if irq_num > 15 or filter_clk is NULL
 */
rx_err_t rx_irq_filter_get_clock(uint8_t irq_num, rx_irq_filter_clk_t* filter_clk);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_IRQ_FILTER_H */
