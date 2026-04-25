/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2014 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/**
 * @file resetprg.c
 * @brief RX72N Reset Program - C Runtime Initialization and Boot Sequence
 *
 * @details
 * This file implements the **first C code** executed after hardware reset.
 * It is called from reset_program.S assembly code and is responsible for:
 *
 * 1. Interrupt vector table setup (INTB register)
 * 2. Exception vector table setup (EXTB register)
 * 3. Floating-point unit initialization (FPSW/DPSW registers)
 * 4. Trigonometric function unit initialization (if available)
 * 5. C runtime initialization (_INITSCT - copy .data, zero .bss)
 * 6. C++ constructor calls (_CALL_INIT if C++ enabled)
 * 7. Jump to main() application entry point
 *
 * **Boot Sequence:**
 * ```
 * Hardware Reset -> reset_program.S -> PowerON_Reset_PC() -> _INITSCT() -> main()
 * ```
 *
 * **Critical Note:** This code runs BEFORE main() and BEFORE ThreadX.
 * Only minimal initialization should happen here. All hardware peripheral
 * initialization (clocks, GPIO, timers, UART) is deferred to main().
 *
 * **Initialization Responsibilities:**
 * - **Boot code (this file)**: CPU core setup, C runtime, vector tables
 * - **Application (main.c)**: Clock config, peripheral init, RTOS startup
 *
 * @see reset_program.S Assembly reset vector handler
 * @see main.c Application entry point (calls rx_clock_power_init, hardware_init)
 * @see _INITSCT() C runtime initialization (copies ROM->RAM, zeros BSS)
 *
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 *
 * @par History
 * - 28.02.2019 v3.00 - Merged processing of all devices (Renesas)
 * - 26.07.2019 v3.01 - Added VBATT stability wait (Renesas)
 * - 08.10.2019 v3.10 - Added RTOS support (Renesas)
 * - 20.11.2020 v3.11 - Updated VBATT wait function (Renesas)
 * - 26.02.2021 v3.12 - Azure RTOS support (Renesas)
 * - 18.05.2021 v3.13 - VBATT wait improvements (Renesas)
 * - 30.11.2021 v3.14 - Updated _CALL_INIT switch (Renesas)
 * - 28.04.2022 v3.15 - CCRX ResetPRG section (Renesas)
 * - 28.02.2023 v3.16 - TFU initialization switch (Renesas)
 * - 2026       vSTAR - STAR coding standards (C23 enums, removed duplicate init)
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version   Description
*         : 28.02.2019 3.00      Merged processing of all devices.
*                                Added support for GNUC and ICCRX.
*                                Fixed coding style.
*                                Renamed following macro definitions.
*                                - BSP_PRV_PSW_INIT
*                                - BSP_PRV_FPSW_INIT
*                                - BSP_PRV_FPU_ROUND
*                                - BSP_PRV_FPU_DENOM
*                                Added following macro definitions.
*                                - BSP_PRV_DPSW_INIT
*         : 26.07.2019 3.01      Added vbatt_voltage_stability_wait function.
*         : 08.10.2019 3.10      Changed for added support of Renesas RTOS (RI600V4 or RI600PX).
*         : 20.11.2020 3.11      Changed vbatt_voltage_stability_wait function.
*         : 26.02.2021 3.12      Changed BSP_CFG_RTOS_USED for Azure RTOS.
*         : 18.05.2021 3.13      Changed vbatt_voltage_stability_wait function.
*         : 30.11.2021 3.14      Changed the compile switch of _CALL_INIT.
*         : 28.04,2022 3.15      Added the section of ResetPRG only for CCRX.
*         : 28.02.2023 3.16      Added the compile switch of R_BSP_INIT_TFU.
*                                Added the bsp_tfu_init function.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#if defined(__CCRX__)
/* Defines MCU configuration functions used in this file */
#include <_h_c_lib.h>
#endif /* defined(__CCRX__) */

/* Define the target platform */
#include "platform.h"

/* Custom clock initialization (replaces SMC mcu_clock_setup) */
#include "rx_clock_power_init.h"

/* When using the user startup program, disable the following code. */
#if BSP_CFG_STARTUP_DISABLE == 0

#if BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */

#if BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600PX
#pragma section P PS
#pragma section B BS
#pragma section C CS
#pragma section D DS
#else
#include "ri_cmt.h" /*  Generated by cfg600 */
#endif              /* BSP_CFG_RENESAS_RTOS_USED */

#else /* BSP_CFG_RTOS_USED!=4 */

/* Declaration of stack size. */
#if BSP_CFG_USER_STACK_ENABLE == 1
R_BSP_PRAGMA_STACKSIZE_SU(BSP_CFG_USTACK_BYTES)
#endif
R_BSP_PRAGMA_STACKSIZE_SI(BSP_CFG_ISTACK_BYTES)

#endif /* BSP_CFG_RTOS_USED */

/**
 * @brief CPU control register initial values
 *
 * @details
 * Defines initial values for RX72N CPU control registers configured during
 * boot before jumping to main(). These values control processor mode, stack
 * configuration, and interrupt state at startup.
 */

/**
 * @enum psw_init_values_t
 * @brief Program Status Word (PSW) initial values
 *
 * @details
 * PSW register controls:
 * - Bit 17 (U): User/Supervisor stack select (0=ISP, 1=USP)
 * - Bit 16 (I): Interrupt enable (0=disabled, 1=enabled)
 * - Bits 27-24 (IPL): Interrupt Priority Level
 *
 * **RTOS Mode (RI600V4/PX):**
 * - Starts in supervisor mode with interrupts disabled (PSW=0)
 * - RTOS kernel enables interrupts after initialization
 *
 * **Non-RTOS Mode:**
 * - Single stack: Interrupt stack only (PSW.U=0, bit 16 set)
 * - Dual stack: User + Interrupt stacks (PSW.U=1, bits 16-17 set)
 *
 * @see RX72N Manual Chapter 2 - CPU (PSW register description)
 */
#if BSP_CFG_RTOS_USED == 4
typedef enum : uint32_t {
  k_psw_init = 0x0000_0000, /**< Supervisor mode, interrupts disabled (RTOS) */
} psw_init_values_t;
#elif BSP_CFG_USER_STACK_ENABLE == 1
typedef enum : uint32_t {
  k_psw_init = 0x00030000, /**< User stack enabled (U=1), I=1, IPL=0 */
} psw_init_values_t;
#else
typedef enum : uint32_t {
  k_psw_init = 0x00010000, /**< Interrupt stack only (U=0), I=1, IPL=0 */
} psw_init_values_t;
#endif

#if defined(__CCRX__) || defined(__GNUC__)

#ifdef BSP_MCU_FLOATING_POINT

/**
 * @enum fpu_init_values_t
 * @brief Floating-Point Unit (FPU) initial values
 *
 * @details
 * Configures the Floating-Point Status Word (FPSW) register for single-precision
 * floating-point operations. Controls rounding mode and denormal handling.
 *
 * **Rounding Modes:**
 * - RM=00 (k_fpu_round_nearest): Round to nearest (IEEE 754 default)
 * - RM=01 (k_fpu_round_zero): Round toward zero (truncate)
 *
 * **Denormal Handling:**
 * - DN=0: Preserve denormal numbers (IEEE 754 compliant)
 * - DN=1: Flush denormals to zero (performance optimization)
 *
 * @see RX72N Manual Chapter 2 - CPU (FPSW register)
 */
typedef enum : uint32_t {
  k_fpsw_init = 0x00000000, /**< FPSW initial value (no flags set) */

#ifdef __ROZ
  k_fpu_round = 0x00000001, /**< RM=01: Round to zero (truncate) */
#else
  k_fpu_round = 0x00000000, /**< RM=00: Round to nearest (IEEE default) */
#endif

#ifdef __DOFF
  k_fpu_denom = 0x00000100, /**< DN=1: Flush denormals to zero */
#else
  k_fpu_denom = 0x00000000, /**< DN=0: Preserve denormals (IEEE 754) */
#endif
} fpu_init_values_t;

#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
/**
 * @enum dpfpu_init_values_t
 * @brief Double-Precision FPU (DPFPU) initial values
 *
 * @details
 * Configures the Double-Precision Floating-Point Status Word (DPSW) register.
 * Controls same parameters as FPSW but for double-precision (64-bit) operations.
 *
 * @see RX72N Manual Chapter 2 - CPU (DPSW register)
 */
typedef enum : uint32_t {
  k_dpsw_init = 0x00000000, /**< DPSW initial value (no flags set) */
} dpfpu_init_values_t;
#endif

#endif /* BSP_MCU_FLOATING_POINT */

#endif /* defined(__CCRX__), defined(__GNUC__) */

/***********************************************************************************************************************
Pre-processor Directives
***********************************************************************************************************************/
/* Set this as the entry point from a power-on reset */
#if defined(__CCRX__)
#if BSP_CFG_CONFIGURATOR_VERSION < 2140
/* The PResetPRG section of compiler setting are not added by Smart configurator if you are using Smart Configurator
       for RX V2.13.0 (equivalent to e2 studio 2022-04) or earlier version.
       Please update Smart configurator to Smart Configurator for RX V2.14.0 (equivalent to e2 studio 2022-07) or 
       later version.
     */
#error                                                                                             \
  "To use this version of BSP, you need to upgrade Smart configurator. Please upgrade Smart configurator. If you don't use Smart Configurator, please change value of BSP_CFG_CONFIGURATOR_VERSION in r_bsp_config.h."
#endif
#pragma section ResetPRG /* Put PResetPRG section at the start address of the code flash. */
#pragma entry PowerON_Reset_PC
#endif /* defined(__CCRX__) */

/***********************************************************************************************************************
External function Prototypes
***********************************************************************************************************************/
/* Initialize C runtime environment */
extern void _INITSCT(void);

#if BSP_CFG_CPLUSPLUS == 1
/* Initialize C++ global class object */
extern void _CALL_INIT(void);
#endif

#if BSP_CFG_USER_WARM_START_CALLBACK_PRE_INITC_ENABLED != 0
/* If user is requesting warm start callback functions then these are the prototypes. */
void BSP_CFG_USER_WARM_START_PRE_C_FUNCTION(void);
#endif

#if BSP_CFG_USER_WARM_START_CALLBACK_POST_INITC_ENABLED != 0
/* If user is requesting warm start callback functions then these are the prototypes. */
void BSP_CFG_USER_WARM_START_POST_C_FUNCTION(void);
#endif

#if BSP_CFG_RTOS_USED == 1 /* FreeRTOS */
/* A function is used to create a main task, rtos's objects required to be available in advance. */
extern void Processing_Before_Start_Kernel(void);
#elif BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
/* kernel initialization routine */
extern void vsta_knl(void);
#endif                       /* BSP_CFG_RTOS_USED */

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
/* Power-on reset function declaration */
R_BSP_POR_FUNCTION(R_BSP_STARTUP_FUNCTION);

/* Main program function declaration */
#if (BSP_CFG_RTOS_USED == 0) || (BSP_CFG_RTOS_USED == 5) /* Non-OS or Azure RTOS */
extern void R_BSP_MAIN_FUNCTION(void);
#endif

/**
 * @brief Reset program entry point - First C code executed after power-on reset
 *
 * @details
 * Called from reset_program.S after hardware reset. Initializes the CPU core
 * and C runtime environment, then transfers control to main(). This function
 * implements the boot sequence common to all RX72N applications.
 *
 * **Initialization Sequence:**
 *
 * **1. Stack Setup (done in reset_program.S before calling this function):**
 * - Interrupt Stack Pointer (ISP) configured
 * - User Stack Pointer (USP) configured (if dual-stack mode enabled)
 * - Stack sizes defined in r_bsp_config.h
 *
 * **2. CPU Core Configuration:**
 * - Set INTB register -> relocatable interrupt vector table base address
 * - Set EXTB register -> exception vector table base address (if supported)
 * - Set FPSW register -> floating-point status (rounding, denormal handling)
 * - Set DPSW register -> double-precision FP status (if DPFPU supported)
 * - Initialize TFU -> trigonometric function unit (if supported)
 *
 * **3. VBATT Stabilization (if RTC backup function enabled):**
 * - Wait for VBATT voltage stability before accessing RTC/backup RAM
 *
 * **4. C Runtime Initialization:**
 * - Call _INITSCT() -> copy .data section from ROM to RAM, zero .bss section
 * - Call _CALL_INIT() -> execute C++ global constructors (if C++ enabled)
 * - Call init_iolib() -> initialize standard I/O library (if enabled)
 *
 * **5. Interrupt and Stack Configuration:**
 * - Set PSW register -> enable interrupts, select ISP or USP
 * - Switch to user mode (if BSP_CFG_RUN_IN_USER_MODE == 1)
 * - Enable bus error interrupt -> catch invalid memory accesses
 *
 * **6. Transfer Control:**
 * - Call main() (Non-OS or Azure RTOS)
 * - OR call vTaskStartScheduler() (FreeRTOS)
 * - OR call vsta_knl() (Renesas RI600V4/PX)
 *
 * **Critical Notes:**
 * - This function runs BEFORE ThreadX and BEFORE any peripheral initialization
 * - Clock configuration (rx_clock_power_init) happens in main(), NOT here
 * - Hardware initialization (hardware_init) happens in main(), NOT here
 * - This function should NEVER return - infinite loop at end catches main() exit
 *
 * @pre CPU just reset, no C runtime available, data/bss sections uninitialized
 * @pre Stack pointers (ISP, USP) configured by reset_program.S
 *
 * @post CPU core configured (INTB, EXTB, FPSW, DPSW set)
 * @post C runtime initialized (.data copied from ROM, .bss zeroed)
 * @post Interrupts enabled, PSW configured
 * @post Control transferred to main() or RTOS kernel
 *
 * @note **Never returns** - transfers control to main() or RTOS, then infinite loop
 * @note Runs in supervisor mode before optional switch to user mode
 * @note VBATT, STDIO, and C++ support are conditional (compile-time config)
 *
 * @warning Do NOT add hardware peripheral initialization here - belongs in main()
 * @warning Do NOT add clock configuration here - belongs in main()
 *
 * @par Example Boot Flow:
 * @code
 * Reset -> reset_program.S (setup stacks)
 *      -> PowerON_Reset_PC() (this function - CPU core + C runtime)
 *      -> main() (clock init + hardware init + ThreadX startup)
 * @endcode
 *
 * @see reset_program.S Assembly code that calls this function
 * @see main() Application entry point (clock/hardware init, RTOS startup)
 * @see _INITSCT() C runtime initialization (copies .data, zeros .bss)
 * @see rx_clock_power_init() Clock configuration (called from main, not here)
 * @see hardware_init() Peripheral initialization (called from main, not here)
 *
 * @since Renesas BSP v3.00 (2019), adapted for STAR 2026
 *
 * @par STAR Modification History:
 * - 2026: Removed duplicate clock/hardware init calls
 * - 2026: Converted \#defines to C23 typed enums
 * - 2026: Added comprehensive Doxygen documentation
 */
R_BSP_POR_FUNCTION(R_BSP_STARTUP_FUNCTION)
{
  /* Stack pointers are setup prior to calling this function - see comments above */

  /* You can use auto variables in this function but such variables other than register variables 
     * will be unavailable after you change the stack from the I stack to the U stack (if change). */

  /* The bss sections have not been cleared and the data sections have not been initialized 
     * and constructors of C++ objects have not been executed until the _INITSCT() is executed. */
#if defined(__GNUC__)
#if BSP_CFG_USER_STACK_ENABLE == 1
  INTERNAL_NOT_USED(ustack_area);
#endif
  INTERNAL_NOT_USED(istack_area);
#endif

#if defined(__CCRX__) || defined(__GNUC__)

  /* Initialize the Interrupt Table Register */
  R_BSP_SET_INTB(R_BSP_SECTOP_INTVECTTBL);

#ifdef BSP_MCU_EXCEPTION_TABLE
  /* Initialize the Exception Table Register */
  R_BSP_SET_EXTB(R_BSP_SECTOP_EXCEPTVECTTBL);
#endif

#ifdef BSP_MCU_FLOATING_POINT
#ifdef __FPU
  /* Initialize the Floating-Point Status Word Register. */
  R_BSP_SET_FPSW(k_fpsw_init | k_fpu_round | k_fpu_denom);
#endif
#endif

#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
#ifdef __DPFPU
  /* Initialize the Double-Precision Floating-Point Status Word Register. */
  R_BSP_SET_DPSW(k_dpsw_init | k_fpu_round | k_fpu_denom);
#endif
#endif

  /* TFU initialization moved to hardware_init() in main() */

#endif /* defined(__CCRX__), defined(__GNUC__) */

  /* VBATT initialization moved to hardware_init() in main() */

  /* If the warm start Pre C runtime callback is enabled, then call it. */
#if BSP_CFG_USER_WARM_START_CALLBACK_PRE_INITC_ENABLED == 1
  BSP_CFG_USER_WARM_START_PRE_C_FUNCTION();
#endif

  /* Initialize C runtime environment */
  _INITSCT();

#if BSP_CFG_CPLUSPLUS == 1
  /* Initialize C++ global class object */
  extern void __CALL_INIT(void); /* Defined in reset_program.S */
  __CALL_INIT();
#endif

  /* If the warm start Post C runtime callback is enabled, then call it. */
#if BSP_CFG_USER_WARM_START_CALLBACK_POST_INITC_ENABLED == 1
  BSP_CFG_USER_WARM_START_POST_C_FUNCTION();
#endif

#if BSP_CFG_IO_LIB_ENABLE == 1
  /* Comment this out if not using I/O lib */
#if defined(__CCRX__)
  init_iolib();
#endif /* defined(__CCRX__) */
#endif

  /* Enable interrupt and select the I stack or the U stack */
  R_BSP_SET_PSW(k_psw_init);

#if BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
                           /* Does not change the MCU's user mode to user in Renesas RTOS. */
#else                      /* BSP_CFG_RTOS_USED != 4 */
#if BSP_CFG_RUN_IN_USER_MODE == 1
/* Change the MCU's user mode from supervisor to user */
#if BSP_CFG_USER_STACK_ENABLE == 1
  R_BSP_CHG_PMUSR();
#else
#error                                                                                             \
  "Settings of BSP_CFG_RUN_IN_USER_MODE and BSP_CFG_USER_STACK_ENABLE are inconsistent with each other."
#endif
#endif /* BSP_CFG_RUN_IN_USER_MODE */
#endif /* BSP_CFG_RTOS_USED */

#if (BSP_CFG_RTOS_USED == 0) || (BSP_CFG_RTOS_USED == 5) /* Non-OS or Azure RTOS */
  /* Call the main program function (should not return) */
  R_BSP_MAIN_FUNCTION();
#elif BSP_CFG_RTOS_USED == 1 /* FreeRTOS */
/* Lock the channel that system timer of RTOS is using. */
#if (((BSP_CFG_RTOS_SYSTEM_TIMER) >= 0) && ((BSP_CFG_RTOS_SYSTEM_TIMER) <= 3))
  if (R_BSP_HardwareLock((mcu_lock_t)(BSP_LOCK_CMT0 + BSP_CFG_RTOS_SYSTEM_TIMER)) == false) {
    /* WAIT_LOOP */
    while (1)
      ;
  }
#else
#error "Setting BSP_CFG_RTOS_SYSTEM_TIMER is invalid."
#endif

  /* Prepare the necessary tasks, FreeRTOS's resources... required to be executed at the beginning
     * after vTaskStarScheduler() is called. Other tasks can also be created after starting scheduler at any time */
  Processing_Before_Start_Kernel();

  /* Call the kernel startup (should not return) */
  vTaskStartScheduler();
#elif BSP_CFG_RTOS_USED == 2 /* SEGGER embOS */
#elif BSP_CFG_RTOS_USED == 3 /* Micrium MicroC/OS */
#elif BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
#if BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600V4
  /* Lock a timer resource by r_bsp, if using time function on RTOS. */
  if (R_BSP_HardwareLock((mcu_lock_t)(BSP_LOCK_CMT0 + _RI_CLOCK_TIMER)) == false) {
    /* WAIT_LOOP */
    while (1)
      ;
  }
  /* Initialize CMT for RI600V4 */
  _RI_init_cmt();
#else
  /* When RI600PX, the above are in _RI_init_cmt_knl called from the kernel. */
#endif
  /* Make sure to disable interrupt. */
  R_BSP_CLRPSW_I(); /* clrpsw_i() */
  vsta_knl();
#endif /* BSP_CFG_RTOS_USED */

#if BSP_CFG_IO_LIB_ENABLE == 1
  /* Comment this out if not using I/O lib - cleans up open files */
#if defined(__CCRX__)
  close_all();
#endif /* defined(__CCRX__) */
#endif

  /* Infinite loop is intended here. */
  /* WAIT_LOOP */
  while (1) {
    /* Infinite loop. Put a breakpoint here if you want to catch an exit of main(). */
    R_BSP_NOP();
  }
} /* End of function PowerON_Reset_PC() */

#if BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
/* Definition of Kernel data section */
#include "kernel_ram.h" /* generated by cfg600 */
#include "kernel_rom.h" /* generated by cfg600 */
#endif                  /* BSP_CFG_RTOS_USED */

#endif /* BSP_CFG_STARTUP_DISABLE == 0 */
