/* lib/rx_hal/inc/rx72n_mtu_regs.h */

/**
 * @file rx72n_mtu_regs.h
 * @brief RX72N MTU Timer Register Definitions
 *
 * Register definitions for the Multi-Function Timer Unit (MTU3a) used for
 * PWM generation and timer functions.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_MTU_REGS_H
#define STAR_RX72N_MTU_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Multi-Function Timer Unit (MTU3a)
 * =============================================================================
 */

/* MTU Channel Register Structure (MTU0-MTU4, MTU6-MTU7) */
typedef struct {
  volatile uint8_t  TCR;   /* Timer Control Register */
  volatile uint8_t  TMDR;  /* Timer Mode Register */
  volatile uint8_t  TIORH; /* Timer I/O Control Register H */
  volatile uint8_t  TIORL; /* Timer I/O Control Register L */
  volatile uint8_t  TIER;  /* Timer Interrupt Enable Register */
  volatile uint8_t  TSR;   /* Timer Status Register */
  volatile uint16_t TCNT;  /* Timer Counter */
  volatile uint16_t TGRA;  /* Timer General Register A */
  volatile uint16_t TGRB;  /* Timer General Register B */
  volatile uint16_t TGRC;  /* Timer General Register C */
  volatile uint16_t TGRD;  /* Timer General Register D */
} rx_mtu_channel_regs_t;

/* MTU3 and MTU4 have additional registers */
typedef struct {
  volatile uint8_t  TCR;   /* 0x00: Timer Control Register */
  volatile uint8_t  TMDR;  /* 0x01: Timer Mode Register */
  volatile uint8_t  TIORH; /* 0x02: Timer I/O Control Register H */
  volatile uint8_t  TIORL; /* 0x03: Timer I/O Control Register L */
  volatile uint8_t  TIER;  /* 0x04: Timer Interrupt Enable Register */
  volatile uint8_t  TSR;   /* 0x05: Timer Status Register */
  volatile uint16_t TCNT;  /* 0x06: Timer Counter */
  volatile uint16_t TGRA;  /* 0x08: Timer General Register A */
  volatile uint16_t TGRB;  /* 0x0A: Timer General Register B */
  volatile uint16_t TGRC;  /* 0x0C: Timer General Register C */
  volatile uint16_t TGRD;  /* 0x0E: Timer General Register D */
  volatile uint16_t TGRE;  /* 0x10: Timer General Register E (MTU3/4 only) */
  volatile uint16_t TGRF;  /* 0x12: Timer General Register F (MTU3/4 only) */
  volatile uint8_t  TIER2; /* 0x14: Timer Interrupt Enable Register 2 */
  volatile uint8_t  TSR2;  /* 0x15: Timer Status Register 2 */
  volatile uint8_t  TBTM;  /* 0x16: Timer Buffer Transfer Mode Register */
} rx_mtu34_channel_regs_t;

/* MTU Start Register */
typedef struct {
  volatile uint8_t TSTR; /* Timer Start Register */
} rx_mtu_tstr_regs_t;

#define MTU0_BASE ((rx_mtu_channel_regs_t*)0x000D0600)
#define MTU1_BASE ((rx_mtu_channel_regs_t*)0x000D0680)
#define MTU2_BASE ((rx_mtu_channel_regs_t*)0x000D0700)
#define MTU3_BASE ((rx_mtu34_channel_regs_t*)0x000D0200)
#define MTU4_BASE ((rx_mtu34_channel_regs_t*)0x000D0201)
#define MTU6_BASE ((rx_mtu_channel_regs_t*)0x000D0A00)
#define MTU7_BASE ((rx_mtu_channel_regs_t*)0x000D0A80)

#define MTU0 (*MTU0_BASE)
#define MTU1 (*MTU1_BASE)
#define MTU2 (*MTU2_BASE)
#define MTU3 (*MTU3_BASE)
#define MTU4 (*MTU4_BASE)
#define MTU6 (*MTU6_BASE)
#define MTU7 (*MTU7_BASE)

#define MTU_TSTR_BASE ((rx_mtu_tstr_regs_t*)0x000D0880)
#define MTU_TSTR      (*MTU_TSTR_BASE)

/* Timer Control Register (TCR) bits */
typedef enum {
  k_mtu_tcr_tpsc_mask = 0x07,        /* Timer Prescaler mask (bits 0-2) */
  k_mtu_tcr_ckeg_mask = 0x18,        /* Clock Edge mask (bits 3-4) */
  k_mtu_tcr_cclr_mask = 0xE0,        /* Counter Clear Source mask (bits 5-7) */
  k_mtu_tcr_tpsc_1    = 0x00,        /* PCLKA/1 */
  k_mtu_tcr_tpsc_4    = 0x01,        /* PCLKA/4 */
  k_mtu_tcr_tpsc_16   = 0x02,        /* PCLKA/16 */
  k_mtu_tcr_tpsc_64   = 0x03,        /* PCLKA/64 */
  k_mtu_tcr_cclr_tgra = (0x01 << 5), /* Clear on TGRA compare match */
} mtu_tcr_bits_t;

/* Timer Mode Register (TMDR) bits */
typedef enum {
  k_mtu_tmdr_md_mask   = 0x0F,     /* Mode select mask (bits 0-3) */
  k_mtu_tmdr_md_normal = 0x00,     /* Normal mode */
  k_mtu_tmdr_md_pwm1   = 0x02,     /* PWM mode 1 */
  k_mtu_tmdr_md_pwm2   = 0x03,     /* PWM mode 2 */
  k_mtu_tmdr_bfa       = (1 << 4), /* Buffer mode A */
  k_mtu_tmdr_bfb       = (1 << 5), /* Buffer mode B */
} mtu_tmdr_bits_t;

/* Timer I/O Control Register (TIOR) bits */
typedef enum {
  k_mtu_tior_ioa_mask  = 0x0F, /* I/O Control A mask (bits 0-3) */
  k_mtu_tior_iob_mask  = 0xF0, /* I/O Control B mask (bits 4-7) */
  k_mtu_tior_init_low  = 0x02, /* Initial output low, compare match high */
  k_mtu_tior_init_high = 0x05, /* Initial output high, compare match low */
  k_mtu_tior_toggle    = 0x03, /* Toggle on compare match */
} mtu_tior_bits_t;

/* Timer Start Register (TSTR) bits */
typedef enum {
  k_mtu_tstr_cst0 = (1 << 0), /* Counter Start 0 */
  k_mtu_tstr_cst1 = (1 << 1), /* Counter Start 1 */
  k_mtu_tstr_cst2 = (1 << 2), /* Counter Start 2 */
  k_mtu_tstr_cst3 = (1 << 6), /* Counter Start 3 */
  k_mtu_tstr_cst4 = (1 << 7), /* Counter Start 4 */
} mtu_tstr_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MTU_REGS_H */
