/* star-rx72n-firmware/src/boot/inc/custom/platform.h */
/**
 * @file platform.h
 * @brief Boot platform header providing BSP definitions for boot file compilation
 *
 * @details
 * Provides all necessary BSP definitions for boot file compilation without Smart
 * Configurator. The boot files (reset_program.S, resetprg.c, lowsrc.c, lowlvl.c,
 * vecttbl.c, dbsct.c) are SMC-generated code with extensive dependencies on Renesas
 * BSP headers. This header routes to our minimal r_bsp.h which includes only what
 * boot actually needs:
 * - Standard C types (stdint.h, stdbool.h)
 * - R_BSP_* macros (r_rx_compiler.h)
 * - BSP configuration (r_bsp_config.h)
 * - INTERNAL_NOT_USED macro (boot_common.h)
 *
 * @see r_bsp.h Boot-compatible BSP header
 * @see boot_common.h Common boot definitions
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

/** @brief Include our boot-compatible r_bsp.h (minimal subset of full Renesas BSP) */
#include "r_bsp.h"
