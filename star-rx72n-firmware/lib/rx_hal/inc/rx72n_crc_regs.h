/* lib/rx_hal/inc/rx72n_crc_regs.h */

/**
 * @file rx72n_crc_regs.h
 * @brief RX72N CRC Calculator Register Definitions
 *
 * Register definitions for the hardware CRC Calculator used for CRC-32
 * acceleration.
 *
 * This project uses only CRC-32 IEEE 802.3 (polynomial 0x04C11DB7),
 * compatible with Go's crc32.ChecksumIEEE().
 *
 * References:
 * - RX72N Group User's Manual: Hardware (CRC Calculator section)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 *
 * NOTE: The hardware supports other polynomials (CRC-8, CRC-16, CRC-CCITT,
 * CRC-32C) but they are not used in this project.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CRC_REGS_H
#define STAR_RX72N_CRC_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CRC Calculator - Hardware CRC-32 Acceleration
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  CRCCR; /* 0x00 - CRC Control Register */
  uint8_t           RESERVED0[3];
  volatile uint32_t CRCDIR; /* 0x04 - CRC Data Input Register (32-bit) */
  volatile uint32_t CRCDOR; /* 0x08 - CRC Data Output Register (32-bit) */
} rx_crc_regs_t;

#define CRC_BASE ((rx_crc_regs_t*)0x00088280)
#define CRC      (*CRC_BASE)

/* CRC Control Register (CRCCR) bit definitions - CRC-32 only */
typedef enum {
  k_crc_crccr_gps_crc32 = 0x03,     /* CRC-32 IEEE 802.3 polynomial */
  k_crc_crccr_lms       = (1 << 6), /* LSB/MSB First (1=LSB first, reflected) */
  k_crc_crccr_dorclr    = (1 << 7), /* Data Output Register Clear */
} crc_crccr_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CRC_REGS_H */
