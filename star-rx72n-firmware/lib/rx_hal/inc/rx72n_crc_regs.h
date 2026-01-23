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

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CRC Calculator - Hardware CRC-32 Acceleration
 * =============================================================================
 */

/** @brief CRC base address */
typedef enum : uint32_t {
  k_crc_base_addr = 0x00088280, /**< CRC register base address */
} rx_crc_addresses_t;

/** @brief CRC register reserved field sizes */
typedef enum : uint8_t {
  k_crc_reserved_after_crccr_bytes = 3, /**< Reserved bytes after CRCCR */
} rx_crc_reserved_sizes_t;

/**
 * @brief CRC Register Map
 * @details
 * Hardware CRC Calculator registers for accelerated CRC computation.
 * Supports CRC-32 IEEE 802.3 polynomial (0x04C11DB7).
 * Base address: 0x00088280
 */
typedef struct {
  volatile uint8_t  crccr; /**< CRC Control Register (polynomial, bit order) */
  uint8_t           reserved0[k_crc_reserved_after_crccr_bytes]; /**< Reserved */
  volatile uint32_t crcdir; /**< CRC Data Input Register (32-bit data input) */
  volatile uint32_t crcdor; /**< CRC Data Output Register (32-bit CRC result) */
} rx_crc_regs_t;

/**
 * @brief Get pointer to CRC registers
 * @return Volatile pointer to CRC register structure
 */
static inline volatile rx_crc_regs_t* crc_regs(void)
{
  return (volatile rx_crc_regs_t*)k_crc_base_addr;
}

/** @brief CRC Control Register (CRCCR) bit shift positions and constants */
typedef enum : uint8_t {
  k_crc_bit_mask           = 1, /**< Single-bit mask */
  k_crc_crccr_lms_shift    = 6, /**< LMS field shift position */
  k_crc_crccr_dorclr_shift = 7, /**< DORCLR field shift position */
} rx_crc_crccr_shifts_t;

/** @brief CRC Control Register (CRCCR) bit masks */
typedef enum : uint8_t {
  k_crc_crccr_lms_mask    = (k_crc_bit_mask << k_crc_crccr_lms_shift),    /**< LSB/MSB First mask */
  k_crc_crccr_dorclr_mask = (k_crc_bit_mask << k_crc_crccr_dorclr_shift), /**< DORCLR mask */
} rx_crc_crccr_masks_t;

/* CRC Control Register (CRCCR) bit definitions - CRC-32 only */
typedef enum : uint8_t {
  k_crc_crccr_gps_crc32 = 0x03,                    /**< CRC-32 IEEE 802.3 polynomial */
  k_crc_crccr_lms       = k_crc_crccr_lms_mask,    /**< LSB/MSB First (1=LSB first, reflected) */
  k_crc_crccr_dorclr    = k_crc_crccr_dorclr_mask, /**< Data Output Register Clear */
} rx_crc_crccr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base address matches Hardware Manual */
_Static_assert(k_crc_base_addr == 0x00088280, "CRC base address incorrect");

/* Verify register structure layout */
_Static_assert(sizeof(rx_crc_regs_t) == 12, "CRC register structure size incorrect");
_Static_assert(offsetof(rx_crc_regs_t, crccr) == 0x00, "CRCCR offset incorrect");
_Static_assert(offsetof(rx_crc_regs_t, crcdir) == 0x04, "CRCDIR offset incorrect");
_Static_assert(offsetof(rx_crc_regs_t, crcdor) == 0x08, "CRCDOR offset incorrect");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CRC_REGS_H */
