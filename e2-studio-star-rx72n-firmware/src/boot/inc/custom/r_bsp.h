/***********************************************************************************************************************
 * STAR Project - Boot-Compatible r_bsp.h
 *
 * Modified version of SMC r_bsp.h that works with boot files after Phase 3 deletions.
 * This file includes only the headers needed for boot sequence, skipping deleted BSP infrastructure.
 *
 * Original: src/smc_gen/r_bsp/board/generic_rx72n/r_bsp.h
 * Changes:
 * - Replaced r_bsp_common.h with boot_common.h (provides INTERNAL_NOT_USED)
 * - Added stdint.h/stdbool.h directly (needed for basic types)
 * - Removed r_bsp_cpu.h (function prototypes not needed by boot)
 * - Removed r_bsp_locking.h (locking not needed by boot)
 * - Kept only headers that boot files actually use
 ***********************************************************************************************************************/

/* Make sure that no other platforms have already been defined. Do not touch this! */
#ifdef  PLATFORM_DEFINED
#error  "Error - Multiple platforms defined in platform.h!"
#else
#define PLATFORM_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Standard C Headers (replaces r_bsp_common.h which was deleted in Phase 3)
 ***********************************************************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#if defined(__CCRX__) || defined(__ICCRX__)
/* Intrinsic functions provided by compiler */
#include <machine.h>
#endif

/***********************************************************************************************************************
 * Boot-Required Headers (from src/boot/include/)
 ***********************************************************************************************************************/
#include "boot_common.h"       /* Replaces r_bsp_common.h - provides INTERNAL_NOT_USED */
#include "r_bsp_config.h"      /* BSP configuration macros */
#include "mcu_info.h"          /* RX72N MCU definitions (must be before r_rx_intrinsic_functions.h) */
#include "r_rx_compiler.h"     /* R_BSP_* macros (80,000+ LOC) */
#include "mcu/all/r_rx_intrinsic_functions.h"  /* R_BSP_NOP, R_BSP_SET_INTB, etc. (from SMC) */

/***********************************************************************************************************************
 * MCU-Specific Headers (minimal subset needed for boot)
 ***********************************************************************************************************************/
#include "lowlvl.h"            /* Low-level hardware init prototypes */
#include "lowsrc.h"            /* Data initialization prototypes */
#include "r_rtos.h"            /* RTOS configuration */

/***********************************************************************************************************************
 * Boot-Specific Type Definitions (needed by vecttbl.c)
 ***********************************************************************************************************************/
#if defined(__GNUC__)
/**
 * @brief Option-setting memory security (OFS1) register structure
 * @details Used by vecttbl.c to define flash option bytes for RX72N
 */
typedef struct st_ofsm_sec_ofs1
{
    uint32_t __MDEreg;   /**< MDE register (endian select) */
    uint32_t __OFS0reg;  /**< OFS0 register (option function select) */
    uint32_t __OFS1reg;  /**< OFS1 register (option function select) */
} st_ofsm_sec_ofs1_t;

/**
 * @brief Option-setting memory security (OFS6) register structure
 * @details Used by vecttbl.c to define ID code registers for RX72N
 */
typedef struct st_ofsm_sec_ofs6
{
    uint32_t __OSIS1reg; /**< ID code register 1 */
    uint32_t __OSIS2reg; /**< ID code register 2 */
    uint32_t __OSIS3reg; /**< ID code register 3 */
    uint32_t __OSIS4reg; /**< ID code register 4 */
} st_ofsm_sec_ofs6_t;
#endif /* defined(__GNUC__) */

/***********************************************************************************************************************
 * Boot Function Declarations
 ***********************************************************************************************************************/
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

/***********************************************************************************************************************
 * Exception Handler Declarations (weak symbols, can be overridden)
 ***********************************************************************************************************************/
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

/***********************************************************************************************************************
 * Stub Definitions for Deleted BSP Functions
 ***********************************************************************************************************************/
/* Interrupt control types (from deleted mcu_interrupts.h) */
typedef enum {
    BSP_INT_SRC_BUS_ERROR = 0
} bsp_int_src_t;

typedef enum {
    BSP_INT_CMD_INTERRUPT_ENABLE = 0
} bsp_int_cmd_t;

/* FIT_NO_PTR is defined in mcu_info.h - don't redefine it */

/**
 * @brief Stub for deleted BSP interrupt control function
 * @details Does nothing - interrupt management was deleted in Phase 3
 * @return true Always succeeds
 */
static inline bool R_BSP_InterruptControl(bsp_int_src_t src, bsp_int_cmd_t cmd, void* ptr)
{
    (void)src;
    (void)cmd;
    (void)ptr;
    return true;  /* Stub - no actual interrupt management */
}

/***********************************************************************************************************************
 * REMOVED: Phase 3 deleted files (not needed for boot sequence)
 *
 * The following files were removed and are NOT included:
 * - mcu/all/r_bsp_common.h          - Replaced with boot_common.h + stdint.h
 * - mcu/rx72n/r_bsp_cpu.h           - Function prototypes (not needed)
 * - mcu/rx72n/r_bsp_locking.h       - Locking functions (not needed)
 * - mcu/all/r_bsp_mcu_startup.h     - Startup functions (not needed)
 * - mcu/rx72n/register_access/(compiler)/iodefine.h - Register definitions (not needed for boot)
 * - mcu/rx72n/mcu_clocks.h          - Clock functions (not needed)
 * - mcu/rx72n/mcu_init.h            - MCU init functions (not needed)
 * - mcu/rx72n/mcu_interrupts.h      - Interrupt functions (not needed)
 * - mcu/rx72n/mcu_locks.h           - Lock definitions (not needed)
 * - mcu/rx72n/mcu_mapped_interrupts*.h - Interrupt mapping (not needed)
 * - mcu/rx72n/vecttbl.h             - Vector table header (not needed)
 * - board/generic_rx72n/hwsetup.h   - Hardware setup (not needed)
 * - mcu/all/r_bsp_interrupts.h      - Interrupt management (not needed)
 * - mcu/all/r_bsp_software_interrupt.h - Software interrupts (not needed)
 * - mcu/all/fsp_common_api.h        - FSP API (not needed)
 * - mcu/all/r_fsp_error.h           - FSP errors (not needed)
 *
 * Boot files ONLY need:
 * 1. Standard C types (stdint.h, stdbool.h)
 * 2. R_BSP_* macros (r_rx_compiler.h)
 * 3. BSP configuration (r_bsp_config.h)
 * 4. INTERNAL_NOT_USED macro (boot_common.h)
 * 5. Function prototypes for lowlvl/lowsrc (lowlvl.h, lowsrc.h)
 ***********************************************************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef BSP_BOARD_GENERIC_RX72N
#define BSP_BOARD_GENERIC_RX72N
#endif /* BSP_BOARD_GENERIC_RX72N */
