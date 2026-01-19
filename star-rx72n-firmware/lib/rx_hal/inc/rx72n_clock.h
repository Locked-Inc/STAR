/* lib/rx_hal/inc/rx72n_clock.h */

/**
 * @file rx72n_clock.h
 * @brief RX72N Clock Frequency Definitions
 *
 * Clock frequency constants for the RX72N configured with PLL for 240 MHz
 * operation.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CLOCK_H
#define STAR_RX72N_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Clock Frequencies (assume PLL configured for 240 MHz)
 * =============================================================================
 */

typedef enum : uint32_t {
  k_iclk_hz  = 240000000UL, /**< CPU clock */
  k_pclka_hz = 120000000UL, /**< Peripheral clock A */
  k_pclkb_hz = 60000000UL,  /**< Peripheral clock B */
  k_pclkc_hz = 60000000UL,  /**< Peripheral clock C */
  k_pclkd_hz = 60000000UL,  /**< Peripheral clock D */
  k_bclk_hz  = 120000000UL, /**< External bus clock */
  k_fclk_hz  = 60000000UL,  /**< Flash clock */
} rx_clock_frequencies_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CLOCK_H */
