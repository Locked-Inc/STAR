/**
 * @file r_bsp.h
 * @brief Boot-compatible minimal replacement for full Renesas SMC r_bsp.h
 *
 * @details
 * Provides only the headers and macros required by boot sequence files
 * (resetprg.c, vecttbl.c, lowsrc.c, etc.) without requiring the full Renesas
 * Smart Configurator BSP package.
 *
 * **Includes:**
 * - Standard C types (stdint.h, stdbool.h)
 * - R_BSP_* compiler macros (r_rx_compiler.h)
 * - BSP configuration (r_bsp_config.h)
 * - INTERNAL_NOT_USED macro (boot_common.h)
 * - Function prototypes for lowlvl/lowsrc
 *
 * @see platform.h Routes to this header
 * @see boot_common.h Common boot definitions
 * @see r_bsp_config.h BSP configuration macros
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

/* Make sure that no other platforms have already been defined. Do not touch this! */
#ifdef PLATFORM_DEFINED
#error "Error - Multiple platforms defined in platform.h!"
#else
#define PLATFORM_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @name Standard C Headers */
/** @{ */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__CCRX__) || defined(__ICCRX__)
/* Intrinsic functions provided by compiler */
#include <machine.h>
#endif

/** @} */

/** @name Boot-Required Headers (from src/boot/include/) */
/** @{ */
#include "boot_common.h"   /* Replaces r_bsp_common.h - provides INTERNAL_NOT_USED */
#include "mcu_info.h"      /* RX72N MCU definitions */
#include "r_bsp_config.h"  /* BSP configuration macros */
#include "r_rx_compiler.h" /* R_BSP_* macros (must be before r_rx_intrinsic_functions.h) */
#include "r_rx_intrinsic_functions.h" /* R_BSP_NOP, R_BSP_SET_INTB, etc. (ported to boot/) */

/** @} */

/** @name MCU-Specific Headers (minimal subset needed for boot) */
/** @{ */
#include "lowlvl.h" /* Low-level hardware init prototypes */
#include "lowsrc.h" /* Data initialization prototypes */
#include "r_rtos.h" /* RTOS configuration */

/** @} */

/** @name Boot-Specific Type Definitions (needed by vecttbl.c) */
/** @{ */
#if defined(__GNUC__)
/**
 * @brief Option-setting memory security (OFS1) register structure
 * @details Used by vecttbl.c to define flash option bytes for RX72N
 */
typedef struct st_ofsm_sec_ofs1 {
  uint32_t __MDEreg;  /**< MDE register (endian select) */
  uint32_t __OFS0reg; /**< OFS0 register (option function select) */
  uint32_t __OFS1reg; /**< OFS1 register (option function select) */
} st_ofsm_sec_ofs1_t;

/**
 * @brief Option-setting memory security (OFS6) register structure
 * @details Used by vecttbl.c to define ID code registers for RX72N
 */
typedef struct st_ofsm_sec_ofs6 {
  uint32_t __OSIS1reg; /**< ID code register 1 */
  uint32_t __OSIS2reg; /**< ID code register 2 */
  uint32_t __OSIS3reg; /**< ID code register 3 */
  uint32_t __OSIS4reg; /**< ID code register 4 */
} st_ofsm_sec_ofs6_t;
#endif /* defined(__GNUC__) */

/** @} */

/** @name Boot Function Declarations */
/** @{ */
/**
 * @brief Power-on reset entry point
 * @details Called by reset vector, initializes hardware and jumps to main()
 */
void PowerON_Reset_PC(void);

/**
 * @brief Low-level hardware setup
 * @details Initializes clocks, peripherals, and hardware before main()
 */
void hardware_setup(void);

/** @} */

/** @name Exception Handler Declarations (weak symbols, can be overridden) */
/** @{ */
/**
 * @brief Supervisor instruction exception handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void excep_supervisor_inst_isr(void) __attribute__((weak));

/**
 * @brief Access exception handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void excep_access_isr(void) __attribute__((weak));

/**
 * @brief Undefined instruction exception handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void excep_undefined_inst_isr(void) __attribute__((weak));

/**
 * @brief Address exception handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void excep_address_isr(void) __attribute__((weak));

/**
 * @brief Floating-point exception handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void excep_floating_point_isr(void) __attribute__((weak));

/**
 * @brief Non-maskable interrupt handler (weak)
 * @details Default: infinite loop, override for custom handling
 */
void non_maskable_isr(void) __attribute__((weak));

/**
 * @brief Undefined interrupt source handler (weak)
 * @details Default: infinite loop for reserved vectors, override for custom handling
 */
void undefined_interrupt_source_isr(void) __attribute__((weak));

/** @} */

#ifdef __cplusplus
}
#endif

#pragma once
