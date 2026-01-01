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

typedef struct {
  volatile uint16_t ADCSR; /* A/D Control/Status Register */
  uint8_t           RESERVED0[2];
  volatile uint16_t ADANSA0; /* A/D Channel Select Register A0 */
  volatile uint16_t ADANSA1; /* A/D Channel Select Register A1 */
  volatile uint16_t ADADS0;  /* A/D-Converted Value Addition/Average Select Register 0 */
  volatile uint16_t ADADS1;  /* A/D-Converted Value Addition/Average Select Register 1 */
  volatile uint8_t  ADADC;   /* A/D-Converted Value Addition/Average Count Select Register */
  uint8_t           RESERVED1[1];
  volatile uint16_t ADCER;   /* A/D Control Extended Register */
  volatile uint16_t ADSTRGR; /* A/D Start Trigger Select Register */
  uint8_t           RESERVED2[4];
  volatile uint16_t ADDR0; /* A/D Data Register 0 */
  volatile uint16_t ADDR1; /* A/D Data Register 1 */
  volatile uint16_t ADDR2; /* A/D Data Register 2 */
  volatile uint16_t ADDR3; /* A/D Data Register 3 */
  volatile uint16_t ADDR4; /* A/D Data Register 4 */
  volatile uint16_t ADDR5; /* A/D Data Register 5 */
  volatile uint16_t ADDR6; /* A/D Data Register 6 */
  volatile uint16_t ADDR7; /* A/D Data Register 7 */
} rx_s12ad_regs_t;

#define S12AD0_BASE ((rx_s12ad_regs_t*)0x00089000)
#define S12AD1_BASE ((rx_s12ad_regs_t*)0x00089100)

#define S12AD0 (*S12AD0_BASE)
#define S12AD1 (*S12AD1_BASE)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ADC_REGS_H */
