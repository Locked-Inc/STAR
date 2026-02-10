/* tests/mocks/rx72n_tpu_regs.h */

/**
 * @file rx72n_tpu_regs.h
 * @brief Mock TPU Register Definitions for Host-Side Unit Testing
 *
 * @details
 * Shadow header that replaces the real lib/rx_hal/inc/rx72n_tpu_regs.h in
 * test builds. Provides RAM-based register structures and inline accessor
 * functions pointing to mock storage, enabling unit testing of the TPU HAL
 * driver without RX72N hardware.
 *
 * ## Include Path Shadowing
 *
 * CMake prepends tests/mocks/ to the include path (BEFORE real headers).
 * When rx72n_regs.h does `#include "rx72n_tpu_regs.h"`, it resolves to THIS
 * file instead of the real hardware header.
 *
 * @see rx_tpu.h TPU HAL driver API
 * @see rx_tpu.c TPU HAL driver implementation
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#ifdef UNIT_TEST

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Register Structures (match real hardware layout)
 * =============================================================================
 */

/**
 * @brief Mock TPU control block registers
 * @invariant sizeof(rx_tpu_control_regs_t) == 14 bytes
 */
typedef struct {
  volatile uint8_t tstr;        /**< Timer Start Register @ +0x00 */
  volatile uint8_t tsyr;        /**< Timer Synchronous Register @ +0x01 */
  volatile uint8_t reserved[6]; /**< Reserved @ +0x02-0x07 */
  volatile uint8_t nfcr[6];    /**< Noise Filter Control Registers @ +0x08-0x0D */
} rx_tpu_control_regs_t;

/**
 * @brief Mock TPU extended channel registers (TPU0/TPU3)
 * @invariant sizeof(rx_tpu_ext_regs_t) == 16 bytes
 */
typedef struct {
  volatile uint8_t  tcr;   /**< Timer Control Register (TCR) @ +0x00 */
  volatile uint8_t  tmdr;  /**< Timer Mode Register (TMDR) @ +0x01 */
  volatile uint8_t  tiorh; /**< Timer I/O Control Register H @ +0x02 */
  volatile uint8_t  tiorl; /**< Timer I/O Control Register L @ +0x03 */
  volatile uint8_t  tier;  /**< Timer Interrupt Enable Register @ +0x04 */
  volatile uint8_t  tsr;   /**< Timer Status Register @ +0x05 */
  volatile uint16_t tcnt;  /**< Timer Counter @ +0x06 */
  volatile uint16_t tgra;  /**< Timer General Register A @ +0x08 */
  volatile uint16_t tgrb;  /**< Timer General Register B @ +0x0A */
  volatile uint16_t tgrc;  /**< Timer General Register C @ +0x0C */
  volatile uint16_t tgrd;  /**< Timer General Register D @ +0x0E */
} rx_tpu_ext_regs_t;

/**
 * @brief Mock TPU basic channel registers (TPU1/2/4/5 - phase counting)
 * @invariant sizeof(rx_tpu_regs_t) == 16 bytes
 */
typedef struct {
  volatile uint8_t  tcr;          /**< Timer Control Register (TCR) @ +0x00 */
  volatile uint8_t  tmdr;         /**< Timer Mode Register (TMDR) @ +0x01 */
  volatile uint8_t  tior;         /**< Timer I/O Control Register @ +0x02 */
  volatile uint8_t  reserved;     /**< Reserved @ +0x03 */
  volatile uint8_t  tier;         /**< Timer Interrupt Enable Register @ +0x04 */
  volatile uint8_t  tsr;          /**< Timer Status Register @ +0x05 */
  volatile uint16_t tcnt;         /**< Timer Counter @ +0x06 */
  volatile uint16_t tgra;         /**< Timer General Register A @ +0x08 */
  volatile uint16_t tgrb;         /**< Timer General Register B @ +0x0A */
  volatile uint8_t  reserved2[4]; /**< Reserved @ +0x0C-0x0F */
} rx_tpu_regs_t;

/* =============================================================================
 * Bit/Mode Constant Enums (match real header values)
 * =============================================================================
 */

/** @brief Total TPU channels */
typedef enum : uint8_t {
  k_tpu_channel_count = 6,
} rx_tpu_channel_count_t;

/** @brief TSTR bit definitions */
typedef enum : uint8_t {
  k_tpu_tstr_cst0 = (1U << 0),
  k_tpu_tstr_cst1 = (1U << 1),
  k_tpu_tstr_cst2 = (1U << 2),
  k_tpu_tstr_cst3 = (1U << 3),
  k_tpu_tstr_cst4 = (1U << 4),
  k_tpu_tstr_cst5 = (1U << 5),
} rx_tpu_tstr_bits_t;

/** @brief TSYR bit definitions */
typedef enum : uint8_t {
  k_tpu_tsyr_sync0 = (1U << 0),
  k_tpu_tsyr_sync1 = (1U << 1),
  k_tpu_tsyr_sync2 = (1U << 2),
  k_tpu_tsyr_sync3 = (1U << 3),
  k_tpu_tsyr_sync4 = (1U << 4),
  k_tpu_tsyr_sync5 = (1U << 5),
} rx_tpu_tsyr_bits_t;

/** @brief TMDR mode values */
typedef enum : uint8_t {
  k_tpu_tmdr_md_normal        = 0x00,
  k_tpu_tmdr_md_pwm1          = 0x02,
  k_tpu_tmdr_md_pwm2          = 0x03,
  k_tpu_tmdr_md_phase_count_1 = 0x04,
  k_tpu_tmdr_md_phase_count_2 = 0x05,
  k_tpu_tmdr_md_phase_count_3 = 0x06,
  k_tpu_tmdr_md_phase_count_4 = 0x07,
} rx_tpu_tmdr_md_t;

/** @brief TMDR bit positions */
typedef enum : uint8_t {
  k_tpu_tmdr_md_shift     = 0,
  k_tpu_tmdr_bfa_shift    = 4,
  k_tpu_tmdr_bfb_shift    = 5,
  k_tpu_tmdr_icselb_shift = 6,
  k_tpu_tmdr_icseld_shift = 7,
} rx_tpu_tmdr_shift_t;

/** @brief TMDR field masks */
typedef enum : uint8_t {
  k_tpu_tmdr_md_mask = 0x0F,
} rx_tpu_tmdr_masks_t;

/** @brief TCR bit positions */
typedef enum : uint8_t {
  k_tpu_tcr_tpsc_shift = 0,
  k_tpu_tcr_ckeg_shift = 3,
  k_tpu_tcr_cclr_shift = 5,
} rx_tpu_tcr_shift_t;

/** @brief TCR CCLR values */
typedef enum : uint8_t {
  k_tpu_tcr_cclr_disabled   = (0U << 5),
  k_tpu_tcr_cclr_tgra_match = (1U << 5),
  k_tpu_tcr_cclr_tgrb_match = (2U << 5),
  k_tpu_tcr_cclr_sync_clear = (3U << 5),
} rx_tpu_tcr_cclr_t;

/** @brief TCR field masks */
typedef enum : uint8_t {
  k_tpu_tcr_tpsc_mask = 0x07,
  k_tpu_tcr_ckeg_mask = 0x18,
  k_tpu_tcr_cclr_mask = 0xE0,
} rx_tpu_tcr_masks_t;

/** @brief TIER bit definitions */
typedef enum : uint8_t {
  k_tpu_tier_tgiea = (1U << 0),
  k_tpu_tier_tgieb = (1U << 1),
  k_tpu_tier_tgiec = (1U << 2),
  k_tpu_tier_tgied = (1U << 3),
  k_tpu_tier_tciev = (1U << 4),
  k_tpu_tier_tcieu = (1U << 5),
  k_tpu_tier_ttge  = (1U << 7),
} rx_tpu_tier_bits_t;

/** @brief TSR bit definitions */
typedef enum : uint8_t {
  k_tpu_tsr_tgfa = (1U << 0),
  k_tpu_tsr_tgfb = (1U << 1),
  k_tpu_tsr_tgfc = (1U << 2),
  k_tpu_tsr_tgfd = (1U << 3),
  k_tpu_tsr_tcfv = (1U << 4),
  k_tpu_tsr_tcfu = (1U << 5),
  k_tpu_tsr_tcfd = (1U << 7),
} rx_tpu_tsr_bits_t;

/** @brief TSR reserved bit */
typedef enum : uint8_t {
  k_tpu_tsr_reserved_bit6 = (1U << 6),
} rx_tpu_tsr_reserved_t;

/** @brief NFCR bit definitions */
typedef enum : uint8_t {
  k_tpu_nfcr_nfaen = (1U << 0),
  k_tpu_nfcr_nfben = (1U << 1),
  k_tpu_nfcr_nfcen = (1U << 2),
  k_tpu_nfcr_nfden = (1U << 3),
} rx_tpu_nfcr_bits_t;

/** @brief NFCR clock select values */
typedef enum : uint8_t {
  k_tpu_nfcr_nfcs_pclk_div1   = (0U << 4),
  k_tpu_nfcr_nfcs_pclk_div8   = (1U << 4),
  k_tpu_nfcr_nfcs_pclk_div32  = (2U << 4),
  k_tpu_nfcr_nfcs_count_clock = (3U << 4),
} rx_tpu_nfcr_clock_t;

/** @brief NFCR field masks */
typedef enum : uint8_t {
  k_tpu_nfcr_nfcs_mask   = 0x30,
  k_tpu_nfcr_enable_mask = 0x0F,
} rx_tpu_nfcr_masks_t;

/** @brief MSTPCRA bit for TPU module stop control */
typedef enum : uint32_t {
  k_tpu_mstpcra_mstpa13 = (1U << 13),
} rx_tpu_mstpcra_t;

/** @brief TPU interrupt source numbers */
typedef enum : uint8_t {
  k_tpu_intb_tgi0a = 15,
  k_tpu_intb_tgi0b = 16,
  k_tpu_intb_tgi0c = 17,
  k_tpu_intb_tgi0d = 18,
  k_tpu_intb_tci0v = 19,
  k_tpu_intb_tgi1a = 20,
  k_tpu_intb_tgi1b = 21,
  k_tpu_intb_tci1v = 22,
  k_tpu_intb_tci1u = 23,
  k_tpu_intb_tgi2a = 24,
  k_tpu_intb_tgi2b = 25,
  k_tpu_intb_tci2v = 26,
  k_tpu_intb_tci2u = 27,
  k_tpu_intb_tgi3a = 28,
  k_tpu_intb_tgi3b = 29,
  k_tpu_intb_tgi3c = 30,
  k_tpu_intb_tgi3d = 31,
  k_tpu_intb_tci3v = 32,
  k_tpu_intb_tgi4a = 33,
  k_tpu_intb_tgi4b = 34,
  k_tpu_intb_tci4v = 35,
  k_tpu_intb_tci4u = 36,
  k_tpu_intb_tgi5a = 37,
  k_tpu_intb_tgi5b = 38,
  k_tpu_intb_tci5v = 39,
  k_tpu_intb_tci5u = 40,
} rx_tpu_intb_source_t;

/* =============================================================================
 * Mock Register Storage (defined in test file)
 * =============================================================================
 */

/** @brief Number of phase-counting-capable channels for mock array sizing */
typedef enum : uint8_t {
  k_mock_tpu_channel_count = 4,
} mock_tpu_channel_count_t;

/** @brief Mock TPU control registers (shared across all channels) */
extern rx_tpu_control_regs_t g_mock_tpu_control;

/** @brief Mock TPU channel registers (0=TPU1, 1=TPU2, 2=TPU4, 3=TPU5) */
extern rx_tpu_regs_t g_mock_tpu_regs[k_mock_tpu_channel_count];

/* =============================================================================
 * Inline Accessor Functions (point to mock storage)
 * =============================================================================
 */

/** @brief Get pointer to mock TPU control block registers */
static inline volatile rx_tpu_control_regs_t* tpu_control(void)
{
  return &g_mock_tpu_control;
}

/** @brief Get pointer to mock TPU1 channel registers (rear-left encoder) */
static inline volatile rx_tpu_regs_t* tpu1(void)
{
  return &g_mock_tpu_regs[0];
}

/** @brief Get pointer to mock TPU2 channel registers (rear-right encoder) */
static inline volatile rx_tpu_regs_t* tpu2(void)
{
  return &g_mock_tpu_regs[1];
}

/** @brief Get pointer to mock TPU4 channel registers */
static inline volatile rx_tpu_regs_t* tpu4(void)
{
  return &g_mock_tpu_regs[2];
}

/** @brief Get pointer to mock TPU5 channel registers */
static inline volatile rx_tpu_regs_t* tpu5(void)
{
  return &g_mock_tpu_regs[3];
}

#ifdef __cplusplus
}
#endif

#endif /* UNIT_TEST */
