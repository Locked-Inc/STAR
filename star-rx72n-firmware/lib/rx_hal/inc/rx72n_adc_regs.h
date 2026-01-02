/* lib/rx_hal/inc/rx72n_adc_regs.h */

/**
 * @file rx72n_adc_regs.h
 * @brief RX72N S12AD ADC Register Definitions
 *
 * Register definitions for the 12-bit A/D Converter (S12ADFa) used for
 * current sensing in motor control applications.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_ADC_REGS_H
#define STAR_RX72N_ADC_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 12-bit A/D Converter (S12ADFa) - For Current Sensing
 * =============================================================================
 */

/** @brief S12ADFa register reserved field sizes */
typedef enum {
  k_s12ad_reserved_after_adcsr_bytes   = 2, /**< Reserved bytes after ADCSR */
  k_s12ad_reserved_after_adadc_bytes   = 1, /**< Reserved byte after ADADC */
  k_s12ad_reserved_after_adstrgr_bytes = 4, /**< Reserved bytes after ADSTRGR */
} s12ad_reserved_sizes_t;

/**
 * @brief S12ADFa Register Map
 * @details
 * 12-bit A/D Converter (S12ADFa) peripheral registers for current sensing.
 * Base addresses:
 * - S12AD0: 0x00089000
 * - S12AD1: 0x00089100
 */
typedef struct {
  volatile uint16_t adcsr; /**< A/D Control/Status Register (ADST, ADCS, ADIE, etc.) */
  uint8_t           reserved0[k_s12ad_reserved_after_adcsr_bytes]; /**< Reserved */
  volatile uint16_t adansa0; /**< A/D Channel Select Register A0 (channels 0-15) */
  volatile uint16_t adansa1; /**< A/D Channel Select Register A1 (channels 16-31) */
  volatile uint16_t adads0;  /**< A/D-Converted Value Addition/Average Select Register 0 */
  volatile uint16_t adads1;  /**< A/D-Converted Value Addition/Average Select Register 1 */
  volatile uint8_t  adadc;   /**< A/D-Converted Value Addition/Average Count Select Register */
  uint8_t           reserved1[k_s12ad_reserved_after_adadc_bytes]; /**< Reserved */
  volatile uint16_t adcer;   /**< A/D Control Extended Register (resolution, alignment) */
  volatile uint16_t adstrgr; /**< A/D Start Trigger Select Register */
  uint8_t           reserved2[k_s12ad_reserved_after_adstrgr_bytes]; /**< Reserved */
  volatile uint16_t addr0; /**< A/D Data Register 0 (conversion result for AN0) */
  volatile uint16_t addr1; /**< A/D Data Register 1 (conversion result for AN1) */
  volatile uint16_t addr2; /**< A/D Data Register 2 (conversion result for AN2) */
  volatile uint16_t addr3; /**< A/D Data Register 3 (conversion result for AN3) */
  volatile uint16_t addr4; /**< A/D Data Register 4 (conversion result for AN4) */
  volatile uint16_t addr5; /**< A/D Data Register 5 (conversion result for AN5) */
  volatile uint16_t addr6; /**< A/D Data Register 6 (conversion result for AN6) */
  volatile uint16_t addr7; /**< A/D Data Register 7 (conversion result for AN7) */
} rx_s12ad_regs_t;

#define S12AD0_BASE ((rx_s12ad_regs_t*)0x00089000)
#define S12AD1_BASE ((rx_s12ad_regs_t*)0x00089100)

#define S12AD0 (*S12AD0_BASE)
#define S12AD1 (*S12AD1_BASE)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ADC_REGS_H */
