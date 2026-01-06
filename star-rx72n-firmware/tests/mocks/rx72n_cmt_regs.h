/* tests/mocks/rx72n_cmt_regs.h */

/**
 * @file rx72n_cmt_regs.h
 * @brief Mock CMT Register Definitions for Host Testing
 *
 * Provides mock CMT register access for host-side unit testing.
 * When included before the real rx72n_cmt_regs.h, this file overrides
 * the hardware register access functions with mock implementations.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CMT_REGS_H
#define STAR_RX72N_CMT_REGS_H

#include "mock_rx_onewire_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Aliases for Compatibility
 * =============================================================================
 */

typedef mock_cmt_channel_t rx_cmt_channel_regs_t;
typedef mock_cmt_ctrl_t    rx_cmt_control_regs_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CMT_REGS_H */
