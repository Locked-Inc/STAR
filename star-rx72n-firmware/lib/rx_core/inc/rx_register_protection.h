/* lib/rx_core/inc/rx_register_protection.h */

/**
 * @file rx_register_protection.h
 * @brief RX72N Register Protection (PRCR) Constants
 *
 * Centralized definitions for the Protection Register (PRCR) unlock/lock
 * sequences used across the RX72N firmware.
 *
 * The PRCR register controls write protection for critical system registers
 * including module stop control (MSTPCR) and clock generation (CGC).
 *
 * @date 2026-01-07
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_REGISTER_PROTECTION_H
#define STAR_RX_REGISTER_PROTECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Protection Register (PRCR) Values
 * =============================================================================
 */

/**
 * @brief PRCR register unlock/lock values
 *
 * The PRCR register uses a key code (0xA5xx) to prevent accidental writes
 * to protected registers. Different bit patterns unlock different register
 * groups:
 *
 * - PRC0: Protects clock generation registers
 * - PRC1: Protects module stop control registers (MSTPCR)
 * - PRC3: Protects LVD registers
 *
 * Common usage:
 * @code
 * system_regs()->prcr = k_rx_prcr_unlock_prc1;  // Unlock MSTPCR
 * // ... modify module stop registers ...
 * system_regs()->prcr = k_rx_prcr_lock;         // Lock protection
 * @endcode
 */
typedef enum {
  k_rx_prcr_unlock_prc1      = 0xA502, /**< Unlock PRC1 (MSTPCR only) */
  k_rx_prcr_unlock_prc0_prc1 = 0xA503, /**< Unlock PRC0+PRC1 (CGC + MSTPCR) */
  k_rx_prcr_unlock_prc1_prc3 = 0xA50B, /**< Unlock PRC1+PRC3 (MSTPCR + LVD) */
  k_rx_prcr_unlock_all       = 0xA50F, /**< Unlock PRC0+PRC1+PRC3 (all) */
  k_rx_prcr_lock             = 0xA500, /**< Lock all protection bits */
} rx_prcr_values_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_REGISTER_PROTECTION_H */
