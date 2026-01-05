/* tests/mocks/rx72n_regs.h */

/**
 * @file rx72n_regs.h
 * @brief Mock RX72N Register Definitions for Host Testing
 *
 * Provides mock register access for host-side unit testing.
 * This file shadows the real rx72n_regs.h when mocks/ is in include path.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_REGS_H
#define STAR_RX72N_REGS_H

#include <stdint.h>

/* Include mock hardware stubs */
#include "mock_rx_onewire_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Aliases for Compatibility with Real Code
 * =============================================================================
 */

typedef mock_cmt_channel_t rx_cmt_channel_regs_t;
typedef mock_cmt_ctrl_t    rx_cmt_control_regs_t;

#ifndef RX_SYSTEM_REGS_T_DEFINED
#define RX_SYSTEM_REGS_T_DEFINED
typedef mock_system_regs_t rx_system_regs_t;
#endif

/* =============================================================================
 * Module Stop Bits (from rx72n_system_regs.h)
 * =============================================================================
 */

typedef enum {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
  k_mstpb_crc  = 23, /**< CRC module stop bit in MSTPCRB */
} rx_module_stop_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_REGS_H */
