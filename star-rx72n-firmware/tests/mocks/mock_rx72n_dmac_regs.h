/**
 * @file mock_rx72n_dmac_regs.h
 * @brief Mock DMAC Register Definitions for Host-Side Unit Testing
 *
 * @details
 * Shadow header that replaces libs/rx_hal/inc/rx72n_dmac_regs.h in test builds.
 * Provides RAM-backed register structures and inline accessor functions pointing
 * to mock storage, enabling unit testing of the DMAC driver without RX72N hardware.
 *
 * Source files that use DMAC registers include this header explicitly via
 * `#ifdef UNIT_TEST` guards: `#include "mock_rx72n_dmac_regs.h"` in test builds
 * and `#include "rx72n_dmac_regs.h"` in production builds.
 *
 * Global mock storage (g_mock_dmac_ch[], g_mock_dmast) must be defined by the
 * test file (see test_rx_dmaca.c for the definition pattern).
 *
 * @see rx72n_dmac_regs.h Real hardware register definitions (target build)
 * @see rx_dmaca.c DMAC driver implementation
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#ifdef UNIT_TEST

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Register Structure (matches real hardware layout)
 * =============================================================================
 */

/**
 * @brief Mock DMAC per-channel register structure
 * @details RAM-backed replacement for memory-mapped hardware channels.
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  volatile uint32_t dmsar;           /**< Source Address Register */
  volatile uint32_t dmdar;           /**< Destination Address Register */
  volatile uint32_t dmcra;           /**< Transfer Count Register A */
  volatile uint16_t dmcrb;           /**< Transfer Count Register B */
  volatile uint8_t  reserved_0e[2];  /**< Reserved */
  volatile uint16_t dmtmd;           /**< Transfer Mode Register */
  volatile uint8_t  reserved_12;     /**< Reserved */
  volatile uint8_t  dmint;           /**< Interrupt Setting Register */
  volatile uint16_t dmamd;           /**< Address Mode Register */
  volatile uint8_t  reserved_16[2];  /**< Reserved */
  volatile uint32_t dmofr;           /**< Offset Register (ch0 only) */
  volatile uint8_t  dmcnt;           /**< DMA Transfer Enable Register */
  volatile uint8_t  dmreq;           /**< DMA Software Start Register */
  volatile uint8_t  dmsts;           /**< DMA Status Register */
  volatile uint8_t  dmcsl;           /**< DMA Channel Select Register */
  volatile uint8_t  reserved_20[32]; /**< Reserved padding to 64 bytes */
} rx_dmac_channel_regs_t;

/* =============================================================================
 * Bit Constant Enums (match real header values exactly)
 * =============================================================================
 */

/** @brief DMAC base address (unused in mock but needed for enum) */
typedef enum : uintptr_t {
  k_dmac_base_addr    = 0x00082000U, /**< DMAC channel 0 base (mock: unused) */
  k_dmac_channel_size = 0x40U,       /**< Channel register block size */
  k_dmac_dmast_addr   = 0x00082200U, /**< DMAST register address (mock: unused) */
  k_dmac_dmist_addr   = 0x00082204U, /**< DMIST register address (mock: unused) */
} dmac_addresses_t;

/** @brief DMAC channel count */
typedef enum : uint8_t {
  k_dmac_channel_count = 8U, /**< Total number of DMAC channels */
} dmac_channel_t;

/** @brief DMTMD register bit definitions */
typedef enum : uint16_t {
  k_dmtmd_dctg_software = 0x0000U, /**< Software trigger */
  k_dmtmd_sz_8bit       = 0x0000U, /**< 8-bit transfer */
  k_dmtmd_sz_16bit      = 0x0100U, /**< 16-bit transfer */
  k_dmtmd_sz_32bit      = 0x0200U, /**< 32-bit transfer */
  k_dmtmd_md_normal     = 0x0000U, /**< Normal transfer mode */
} dmtmd_bits_t;

/** @brief DMAMD register bit definitions */
typedef enum : uint16_t {
  k_dmamd_dm_fixed     = 0x0000U, /**< Destination address fixed */
  k_dmamd_sm_increment = 0x8000U, /**< Source address increment */
} dmamd_bits_t;

/** @brief DMCNT register bit definitions */
typedef enum : uint8_t {
  k_dmcnt_dte = 0x01U, /**< DMA Transfer Enable */
} dmcnt_bits_t;

/** @brief DMREQ register bit definitions */
typedef enum : uint8_t {
  k_dmreq_swreq = 0x01U, /**< Software start trigger */
  k_dmreq_clrs  = 0x10U, /**< Auto-clear SWREQ after start */
} dmreq_bits_t;

/** @brief DMINT register bit definitions */
typedef enum : uint8_t {
  k_dmint_disabled = 0x00U, /**< All interrupts disabled */
  k_dmint_dtie     = 0x10U, /**< Transfer end interrupt enable */
} dmint_bits_t;

/** @brief DMSTS register bit definitions */
typedef enum : uint8_t {
  k_dmsts_act = 0x80U, /**< DMA active flag (1 = transfer in progress) */
} dmsts_bits_t;

/** @brief DMAST register bit definitions */
typedef enum : uint8_t {
  k_dmast_dmst = 0x01U, /**< DMAC module start */
} dmast_bits_t;

/** @brief DMAC module stop control bit */
typedef enum : uint32_t {
  k_dmac_mstpcra_bit = (1U << 28U), /**< MSTPCRA bit 28 = DMAC module stop */
} dmac_mstpcr_t;

/* =============================================================================
 * Mock Register Storage (defined in test file)
 * =============================================================================
 */

/** @brief Mock DMAC channel register arrays (one per channel) */
extern rx_dmac_channel_regs_t g_mock_dmac_ch[k_dmac_channel_count];

/** @brief Mock DMAC Module Start Register (DMAST) */
extern uint8_t g_mock_dmast;

/* =============================================================================
 * Inline Accessor Functions (point to mock storage)
 * =============================================================================
 */

/**
 * @brief Get pointer to mock DMAC channel registers
 * @param channel Channel number (0-7)
 * @return Pointer to mock channel register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac_channel(uint8_t channel)
{
  return &g_mock_dmac_ch[channel];
}

/**
 * @brief Get pointer to mock DMAC Module Start Register (DMAST)
 * @return Pointer to mock DMAST byte
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dmac_dmast_reg(void)
{
  return (volatile uint8_t*)&g_mock_dmast;
}

#ifdef __cplusplus
}
#endif

#endif /* UNIT_TEST */
