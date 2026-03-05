/* star-rx72n-firmware/tests/mocks/mock_rx_tpu_regs.h */

/**
 * @file mock_rx_tpu_regs.h
 * @brief TPU Mock Header for Unit Tests
 *
 * @details
 * Convenience header for TPU unit tests. Includes the shadow rx72n_tpu_regs.h
 * which provides all TPU register types, enums, and mock accessor functions.
 *
 * TPU types and accessors come from mocks/rx72n_tpu_regs.h (shadow header).
 * System register access (system_regs, prcr_reg) comes from mock_rx_onewire_hw.h
 * and mocks/rx72n_system_regs.h, included via the shadow mocks/rx72n_regs.h.
 *
 * @see rx72n_tpu_regs.h Shadow TPU register header (mocks/)
 * @see rx_tpu.h TPU HAL driver API
 *
 * @author STAR Team
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once
#define MOCK_RX_TPU_REGS_H

#ifdef UNIT_TEST

/* Shadow rx72n_tpu_regs.h provides TPU types, enums, and mock accessors */
#include "rx72n_tpu_regs.h"

#endif /* UNIT_TEST */
