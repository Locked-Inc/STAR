/**
 * @file rx_exception.h
 * @brief RX72N CPU Exception Handling
 *
 * @details
 * Provides exception handling for RX72N CPU exceptions as defined in
 * the hardware manual Chapter 14. This module captures debug information
 * when exceptions occur and provides handlers for:
 * - Undefined instruction exception (EXTB + 0x5C)
 * - Privileged instruction exception (EXTB + 0x50)
 * - Access exception (EXTB + 0x54)
 * - Address exception (EXTB + 0x60)
 * - Single-precision floating-point exception (EXTB + 0x64)
 * - Non-maskable interrupt (EXTB + 0x78)
 *
 * The RXv3 CPU saves PC and PSW to the stack (ISP) on exception entry,
 * shifts to supervisor mode, and disables interrupts (I=0, U=0, PM=0).
 *
 * @par Thread Safety:
 * Exception handlers execute in supervisor mode with interrupts disabled.
 * Not thread-safe by nature - exceptions preempt all other execution.
 *
 * @par Manual Reference:
 * RX72N Group User's Manual: Hardware, Rev.1.20, Chapter 14
 *
 * @see rx72n_icu_regs.h For NMI-related ICU registers
 *
 * @since Version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Exception Type Enumeration
 * ============================================================================ */

/**
 * @enum rx_exception_type_t
 * @brief CPU exception types supported by RXv3 core
 *
 * @details
 * Defines all exception types from Chapter 14 of the RX72N hardware manual.
 * Exception priority from highest to lowest:
 * 1. Reset (highest)
 * 2. Non-maskable interrupt
 * 3. Interrupt
 * 4. Instruction access exception
 * 5. Undefined/Privileged instruction exception
 * 6. Unconditional trap
 * 7. Address exception
 * 8. Operand access exception
 * 9. Single-precision floating-point exception (lowest)
 *
 * @par Manual Reference:
 * Table 14.4 - Priority of Exception Events
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
     * @brief Undefined instruction exception
     * @details
     * Occurs when CPU attempts to execute an instruction not implemented
     * in the RXv3 instruction set.
     * @par Vector: EXTB + 0x5C
     * @par Return: RTE instruction (if recoverable)
     */
  k_rx_exception_undefined_instruction = 0,

  /**
     * @brief Privileged instruction exception
     * @details
     * Occurs when a privileged instruction is executed in user mode.
     * Privileged instructions can only execute in supervisor mode.
     * @par Vector: EXTB + 0x50
     * @par Return: RTE instruction (if recoverable)
     */
  k_rx_exception_privileged_instruction = 1,

  /**
     * @brief Access exception (instruction or operand)
     * @details
     * Occurs when memory protection unit (MPU) detects a memory access
     * violation. Can be instruction-access (fetch) or operand-access (load/store).
     * @par Vector: EXTB + 0x54
     * @par Return: RTE instruction (if recoverable)
     */
  k_rx_exception_access = 2,

  /**
     * @brief Address exception
     * @details
     * Occurs on 64-bit operand access to an address not on a 32-bit boundary.
     * @par Vector: EXTB + 0x60
     * @par Return: RTE instruction (if recoverable)
     */
  k_rx_exception_address = 3,

  /**
     * @brief Single-precision floating-point exception
     * @details
     * Occurs when IEEE 754 floating-point exception is detected:
     * overflow, underflow, inexact, division-by-zero, or invalid operation.
     * Only triggers if corresponding enable bit in FPSW is set.
     * @par Vector: EXTB + 0x64
     * @par Return: RTE instruction (if recoverable)
     */
  k_rx_exception_floating_point = 4,

  /**
     * @brief Non-maskable interrupt (NMI)
     * @details
     * Fatal system fault indication. NEVER return from NMI handler -
     * must reset system or halt permanently.
     * @par Vector: EXTB + 0x78
     * @par Return: PROHIBITED - must reset or halt
     * @warning Do not attempt to return from NMI handler
     */
  k_rx_exception_nmi = 5,

  /**
     * @brief Total count of exception types
     * @details Used for array sizing and validation
     */
  k_rx_exception_type_count = 6,

} rx_exception_type_t;

/* ============================================================================
 * Exception Vector Offsets
 * ============================================================================ */

/**
 * @enum rx_exception_vector_offset_t
 * @brief Exception vector table offsets from EXTB base
 *
 * @details
 * The RX72N uses EXTB register to specify the exception vector table base.
 * Default EXTB value after reset is 0xFFFF_FF80.
 * Each offset specifies where the exception handler address is stored.
 *
 * @par Manual Reference:
 * Section 14.5 - Hardware Pre-Processing
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
     * @brief Privileged instruction exception vector offset
     * @details Handler address at EXTB + 0x50 = default 0xFFFF_FFD0
     */
  k_rx_exc_offset_privileged = 0x50,

  /**
     * @brief Access exception vector offset
     * @details Handler address at EXTB + 0x54 = default 0xFFFF_FFD4
     */
  k_rx_exc_offset_access = 0x54,

  /**
     * @brief Undefined instruction exception vector offset
     * @details Handler address at EXTB + 0x5C = default 0xFFFF_FFDC
     */
  k_rx_exc_offset_undefined = 0x5C,

  /**
     * @brief Address exception vector offset
     * @details Handler address at EXTB + 0x60 = default 0xFFFF_FFE0
     */
  k_rx_exc_offset_address = 0x60,

  /**
     * @brief Floating-point exception vector offset
     * @details Handler address at EXTB + 0x64 = default 0xFFFF_FFE4
     */
  k_rx_exc_offset_fpu = 0x64,

  /**
     * @brief Non-maskable interrupt vector offset
     * @details Handler address at EXTB + 0x78 = default 0xFFFF_FFF8
     */
  k_rx_exc_offset_nmi = 0x78,

  /**
     * @brief Reset vector offset
     * @details Handler address at EXTB + 0x7C = fixed 0xFFFF_FFFC
     */
  k_rx_exc_offset_reset = 0x7C,

} rx_exception_vector_offset_t;

/* ============================================================================
 * Exception Frame Structure
 * ============================================================================ */

/**
 * @struct rx_exception_frame_t
 * @brief Captured CPU state at time of exception
 *
 * @details
 * When an exception occurs, the RXv3 CPU saves PC and PSW to the interrupt
 * stack (ISP). This structure captures additional context for debugging.
 * The exception handler captures this information before calling the
 * C-level handler function.
 *
 * @par Stack Layout on Exception Entry:
 * @code
 * ISP before exception:
 * [lower address]
 *     PC (return address, 4 bytes)
 *     PSW (processor status word, 4 bytes)
 * [higher address] <- ISP after hardware push
 * @endcode
 *
 * @par Manual Reference:
 * Section 14.4 - Hardware Processing for Accepting and Returning from Exceptions
 *
 * @invariant pc must be aligned to 2-byte boundary (RX instructions are 2-byte aligned)
 * @invariant psw I bit and U bit are 0 in exception context
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Program counter at time of exception
     * @details
     * For instruction-canceling exceptions (UND, PIE, ACE, ADE, FPE):
     * Points to the instruction that caused the exception.
     * For instruction-completion exceptions (EI, TRAP):
     * Points to the next instruction.
     * @par Range: 0x00000000 to 0xFFFFFFFF
     */
  uint32_t pc;

  /**
     * @brief Processor status word at time of exception
     * @details
     * Contains flags, interrupt priority level, and mode bits.
     * @par Bit Layout:
     * - [31:28] IPL - Interrupt priority level (0-15)
     * - [27:24] Reserved
     * - [23:20] PM, U, C, Z - Processor mode, stack, carry, zero
     * - [19:16] S, O, I, Reserved - Sign, overflow, interrupt enable
     * - [15:0] Reserved and other flags
     */
  uint32_t psw;

  /**
     * @brief Exception type that occurred
     * @details Identifies which exception handler was invoked
     */
  rx_exception_type_t type;

  /**
     * @brief Padding for alignment
     * @details Ensures structure is 4-byte aligned
     */
  uint8_t reserved[3];

} rx_exception_frame_t;

/* Static assertion for structure size */
static_assert(sizeof(rx_exception_frame_t) == 12, "rx_exception_frame_t must be 12 bytes");

/* ============================================================================
 * Exception Statistics
 * ============================================================================ */

/**
 * @struct rx_exception_stats_t
 * @brief Exception occurrence statistics for debugging
 *
 * @details
 * Tracks how many times each exception type has occurred.
 * Useful for identifying systemic issues in the firmware.
 *
 * @par Thread Safety:
 * Read/write operations on counters are not atomic. For precise counts
 * in multi-threaded environments, use appropriate synchronization.
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
     * @brief Count per exception type
     * @details Index by rx_exception_type_t enum values
     */
  uint32_t count[k_rx_exception_type_count];

  /**
     * @brief Last exception frame captured
     * @details Preserved for post-mortem debugging
     */
  rx_exception_frame_t last_frame;

} rx_exception_stats_t;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/**
 * @brief Initialize exception handling module
 *
 * @details
 * Initializes exception statistics and optionally installs custom handlers.
 * Should be called early in system startup, after basic hardware init.
 *
 * @pre None
 * @post Exception statistics zeroed
 * @post Default handlers installed (if not using assembly stubs)
 *
 * @note Thread-safe: No, call only once during startup
 *
 * @par Example:
 * @code
 * void main(void) {
 *     rx_exception_init();
 *     // ... rest of initialization
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
void rx_exception_init(void);

/**
 * @brief Get exception statistics
 *
 * @details
 * Returns pointer to read-only exception statistics structure.
 * Statistics are updated by exception handlers. Returns NULL if
 * rx_exception_init() has not yet been called; callers must check
 * the return value before dereferencing.
 *
 * @return Pointer to exception statistics, or NULL if not initialized
 * @retval non-NULL Valid pointer to statistics after rx_exception_init()
 * @retval NULL     rx_exception_init() has not been called yet
 *
 * @pre rx_exception_init() must have been called for a non-NULL return
 * @post No side effects
 *
 * @note Thread-safe: Read access only
 *
 * @par Example:
 * @code
 * const rx_exception_stats_t* stats = rx_exception_get_stats();
 * if (stats != NULL && stats->count[k_rx_exception_nmi] > 0) {
 *     // NMI occurred - investigate
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
const rx_exception_stats_t* rx_exception_get_stats(void);

/**
 * @brief Get human-readable name for exception type
 *
 * @details
 * Returns a string describing the exception type for logging.
 *
 * @param[in] type Exception type enumeration value
 *
 * @return Pointer to static string (never NULL)
 * @retval "Unknown" If type is out of range
 *
 * @pre None
 * @post No side effects
 *
 * @note Thread-safe: Yes (returns pointer to const data)
 *
 * @since Version 1.0.0
 */
const char* rx_exception_get_name(rx_exception_type_t type);

/* ============================================================================
 * Assembly-Level Exception Handlers (called from vector table)
 * ============================================================================ */

/**
 * @brief Undefined instruction exception handler (ASM entry point)
 * @details Called from vector at EXTB + 0x5C. Saves context and calls C handler.
 * @warning Do not call directly from C code
 */
void rx_exc_undefined_instruction_handler(void);

/**
 * @brief Privileged instruction exception handler (ASM entry point)
 * @details Called from vector at EXTB + 0x50. Saves context and calls C handler.
 * @warning Do not call directly from C code
 */
void rx_exc_privileged_instruction_handler(void);

/**
 * @brief Access exception handler (ASM entry point)
 * @details Called from vector at EXTB + 0x54. Saves context and calls C handler.
 * @warning Do not call directly from C code
 */
void rx_exc_access_handler(void);

/**
 * @brief Address exception handler (ASM entry point)
 * @details Called from vector at EXTB + 0x60. Saves context and calls C handler.
 * @warning Do not call directly from C code
 */
void rx_exc_address_handler(void);

/**
 * @brief Floating-point exception handler (ASM entry point)
 * @details Called from vector at EXTB + 0x64. Saves context and calls C handler.
 * @warning Do not call directly from C code
 */
void rx_exc_floating_point_handler(void);

/**
 * @brief Non-maskable interrupt handler (ASM entry point)
 * @details Called from vector at EXTB + 0x78. NEVER returns.
 * @warning Do not call directly from C code
 * @warning MUST NOT return - system must reset or halt
 */
void rx_exc_nmi_handler(void);

#ifdef __cplusplus
}
#endif
