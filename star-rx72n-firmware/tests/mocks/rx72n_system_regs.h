/* tests/mocks/rx72n_system_regs.h */

/**
 * @file rx72n_system_regs.h
 * @brief Mock System Register Definitions for Host Testing
 *
 * Provides mock system register access for host-side unit testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_SYSTEM_REGS_H
#define STAR_RX72N_SYSTEM_REGS_H

#include "mock_rx_onewire_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Aliases for Compatibility
 * =============================================================================
 */

#ifndef RX_SYSTEM_REGS_T_DEFINED
#define RX_SYSTEM_REGS_T_DEFINED
typedef mock_system_regs_t rx_system_regs_t;
#endif

/* Module stop bits for MSTPCRB register */
typedef enum {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
  k_mstpb_crc  = 23, /**< CRC module stop bit in MSTPCRB */
} rx_module_stop_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SYSTEM_REGS_H */
