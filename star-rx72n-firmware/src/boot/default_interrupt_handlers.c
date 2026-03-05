/* star-rx72n-firmware/src/boot/default_interrupt_handlers.c */

/**
 * @file default_interrupt_handlers.c
 * @brief Default exception and interrupt handlers for RX72N boot sequence
 *
 * @details
 * Provides weak default implementations of exception handlers referenced by
 * vecttbl.c. These can be overridden by user implementations if needed.
 *
 * **Default behavior:** Infinite loop (halt on exception). This is the safest
 * default for unhandled exceptions in safety-critical embedded systems.
 *
 * @see vecttbl.c Vector table referencing these handlers
 * @see r_bsp.h Declarations of these handlers (weak symbols)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "platform.h"

/**
 * @name Default Exception Handlers (Weak Symbols)
 * @brief All handlers halt the CPU in an infinite loop.
 * @details Override these in your application to provide custom exception handling.
 * @{
 */

/**
 * @brief Default supervisor instruction exception handler
 * @details Halts CPU when a privileged instruction is executed in user mode
 */
__attribute__((weak)) void excep_supervisor_inst_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on supervisor instruction exception */
  }
}

/**
 * @brief Default access exception handler
 * @details Halts CPU on memory access violations
 */
__attribute__((weak)) void excep_access_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on access exception */
  }
}

/**
 * @brief Default undefined instruction exception handler
 * @details Halts CPU when an invalid opcode is encountered
 */
__attribute__((weak)) void excep_undefined_inst_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on undefined instruction */
  }
}

/**
 * @brief Default address exception handler
 * @details Halts CPU on unaligned access or invalid address
 */
__attribute__((weak)) void excep_address_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on address exception */
  }
}

/**
 * @brief Default floating-point exception handler
 * @details Halts CPU on FPU exceptions (overflow, underflow, etc.)
 */
__attribute__((weak)) void excep_floating_point_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on floating-point exception */
  }
}

/**
 * @brief Default non-maskable interrupt handler
 * @details Halts CPU on NMI (critical hardware faults)
 */
__attribute__((weak)) void non_maskable_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on NMI */
  }
}

/**
 * @brief Default undefined interrupt source handler
 * @details Halts CPU for reserved/unused interrupt vectors
 */
__attribute__((weak)) void undefined_interrupt_source_isr(void)
{
  /* WAIT_LOOP */
  while (1) {
    R_BSP_NOP(); /* Halt on undefined interrupt */
  }
}

/** @} */
