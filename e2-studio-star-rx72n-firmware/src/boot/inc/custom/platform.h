/***********************************************************************************************************************
 * STAR Project - Boot Platform Header
 *
 * Provides all necessary BSP definitions for boot file compilation without Smart Configurator.
 *
 * The boot files (reset_program.S, resetprg.c, lowsrc.c, lowlvl.c, vecttbl.c, dbsct.c) are
 * SMC-generated code with extensive dependencies on Renesas BSP headers (hundreds of macros,
 * types, and functions). This header provides those dependencies via a modified r_bsp.h that
 * skips deleted Phase 3 files.
 *
 * Solution: Use local r_bsp.h (in src/boot/include/) which includes only what boot needs:
 * - Standard C types (stdint.h, stdbool.h)
 * - R_BSP_* macros (r_rx_compiler.h - 80,000+ LOC)
 * - BSP configuration (r_bsp_config.h)
 * - INTERNAL_NOT_USED macro (boot_common.h)
 *
 * This approach maintains boot code without Smart Configurator while avoiding deleted files.
 ***********************************************************************************************************************/

#ifndef PLATFORM_H
#define PLATFORM_H

/* Include our boot-compatible r_bsp.h (NOT the SMC version which tries to include deleted files) */
#include "r_bsp.h"

#endif /* PLATFORM_H */
