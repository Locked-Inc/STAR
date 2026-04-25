/**
 * @file rx_exception.c
 * @brief RX72N CPU Exception Handling Implementation
 *
 * @details
 * Implements exception handlers for RX72N CPU exceptions. Each handler:
 * 1. Captures the exception context (PC, PSW from stack)
 * 2. Updates exception statistics
 * 3. Logs the exception via UART/USB (if available)
 * 4. Either halts (NMI) or allows return (other exceptions)
 *
 * @par Assembly Entry Points:
 * The assembly-level handlers in startup_rx72n.S call these C functions
 * after saving/restoring context appropriately.
 *
 * @par Manual Reference:
 * RX72N Group User's Manual: Hardware, Rev.1.20, Chapter 14
 *
 * @since Version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_exception.h"

#include <stddef.h>

#include "rx_log.h"

/* Exception handler forward declarations (required for -Wmissing-declarations) */
void rx_exc_undefined_instruction_c_handler(uint32_t pc, uint32_t psw);
void rx_exc_privileged_instruction_c_handler(uint32_t pc, uint32_t psw);
void rx_exc_access_c_handler(uint32_t pc, uint32_t psw);
void rx_exc_address_c_handler(uint32_t pc, uint32_t psw);
void rx_exc_floating_point_c_handler(uint32_t pc, uint32_t psw);
void rx_exc_nmi_c_handler(uint32_t pc, uint32_t psw);

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/**
 * @brief Log tag for exception module
 * @details Used for identifying log messages from this module
 */
static const char* const s_tag = "EXCEPTION";

/**
 * @brief Indices for the reserved padding bytes in rx_exception_frame_t
 * @details The reserved array has k_exception_frame_reserved_size (3) elements.
 */
typedef enum : uint8_t {
  k_exc_reserved_idx_0 = 0, /**< First reserved padding byte */
  k_exc_reserved_idx_1 = 1, /**< Second reserved padding byte */
  k_exc_reserved_idx_2 = 2, /**< Third reserved padding byte */
} exc_reserved_idx_t;

/**
 * @brief Exception type names for logging
 * @details Indexed by rx_exception_type_t values
 */
static const char* const s_exception_names[k_rx_exception_type_count] = {"Undefined Instruction",
                                                                         "Privileged Instruction",
                                                                         "Access Exception",
                                                                         "Address Exception",
                                                                         "Floating-Point Exception",
                                                                         "Non-Maskable Interrupt"};

/* ============================================================================
 * Module State
 * ============================================================================ */

/**
 * @brief Exception statistics storage
 * @details
 * Static storage for exception occurrence counts and last frame.
 * @warning Direct modification discouraged - use handler functions
 */
static rx_exception_stats_t s_exception_stats = {};

/**
 * @enum rx_exc_init_state_t
 * @brief Initialization state for the exception module
 *
 * @details
 * Tracks whether rx_exception_init() has been called. Used to guard
 * rx_exception_get_stats() so callers cannot observe uninitialized data.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_exc_not_initialized = 0U, /**< rx_exception_init() has not been called */
  k_rx_exc_initialized     = 1U, /**< rx_exception_init() completed successfully */
} rx_exc_init_state_t;

/**
 * @var s_initialized
 * @brief Initialization flag for the exception module
 * @details Set to k_rx_exc_initialized after rx_exception_init() completes.
 *          Guards rx_exception_get_stats() against returning stale data.
 * @note Not thread-safe; rx_exception_init() must be called before any
 *       concurrent access to exception module state.
 * @since Version 1.0.0
 */
static rx_exc_init_state_t s_initialized = k_rx_exc_not_initialized;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Common exception processing logic
 *
 * @details
 * Called by all exception handlers to:
 * 1. Update statistics
 * 2. Save frame for post-mortem analysis
 * 3. Log the exception
 *
 *
 * @pre frame is not nullptr
 * @pre frame->type is valid enum value
 * @post Statistics updated
 * @post Last frame saved
 *
 * @note Not thread-safe - called from exception context with interrupts disabled
 */
static void internal_process_exception(const rx_exception_frame_t* frame)
{
  /* Validate type is in range */
  if (frame->type >= k_rx_exception_type_count) {
    return;
  }

  /* Update statistics */
  s_exception_stats.count[frame->type]++;
  s_exception_stats.last_frame = *frame;

  /* Log the exception */
  (void)rx_log_error(s_tag, "*** CPU EXCEPTION ***");
  (void)rx_log_error(s_tag, s_exception_names[frame->type]);
  /* Note: PC and PSW values available in s_exception_stats.last_frame for debugging */
}

/**
 * @brief Halt CPU in infinite loop
 *
 * @details
 * Used after fatal exceptions (NMI) where recovery is not possible.
 * Enters low-power wait state to reduce power consumption while halted.
 *
 * @note This function never returns
 */
static void internal_halt_cpu(void)
{
  /* Disable interrupts and halt */
  for (;;) {
    __asm__ volatile("wait");
  }
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize exception handling module
 */
void rx_exception_init(void)
{
  /* Zero all statistics */
  for (uint8_t i = 0; i < k_rx_exception_type_count; i++) {
    s_exception_stats.count[i] = 0;
  }

  /* Clear last frame */
  s_exception_stats.last_frame.pc   = 0;
  s_exception_stats.last_frame.psw  = 0;
  s_exception_stats.last_frame.type = k_rx_exception_undefined_instruction;
  s_exception_stats.last_frame.reserved[k_exc_reserved_idx_0] = 0;
  s_exception_stats.last_frame.reserved[k_exc_reserved_idx_1] = 0;
  s_exception_stats.last_frame.reserved[k_exc_reserved_idx_2] = 0;

  /* Mark as initialized */
  s_initialized = k_rx_exc_initialized;

  (void)rx_log_info(s_tag, "Exception handling initialized");
}

/**
 * @brief Get exception statistics
 */
const rx_exception_stats_t* rx_exception_get_stats(void)
{
  if (s_initialized != k_rx_exc_initialized) {
    return nullptr;
  }
  return &s_exception_stats;
}

/**
 * @brief Get human-readable name for exception type
 */
const char* rx_exception_get_name(rx_exception_type_t type)
{
  if (type >= k_rx_exception_type_count) {
    return "Unknown";
  }
  return s_exception_names[type];
}

/* ============================================================================
 * C-Level Exception Handlers (called from assembly stubs)
 * ============================================================================ */

/**
 * @brief C-level undefined instruction exception handler
 *
 * @details
 * Called from assembly stub after context is saved.
 * The faulting instruction PC is on the stack.
 *
 *
 * @note Called with interrupts disabled (I=0)
 * @note Can return if instruction can be emulated/skipped
 */
void rx_exc_undefined_instruction_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_undefined_instruction,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  /* For safety, halt on undefined instruction in embedded system */
  internal_halt_cpu();
}

/**
 * @brief C-level privileged instruction exception handler
 *
 */
void rx_exc_privileged_instruction_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_privileged_instruction,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  /* Halt - user mode attempted privileged operation */
  internal_halt_cpu();
}

/**
 * @brief C-level access exception handler
 *
 */
void rx_exc_access_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_access,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  /* Halt - memory protection violation */
  internal_halt_cpu();
}

/**
 * @brief C-level address exception handler
 *
 */
void rx_exc_address_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_address,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  /* Halt - unaligned access is a programming error */
  internal_halt_cpu();
}

/**
 * @brief C-level floating-point exception handler
 *
 */
void rx_exc_floating_point_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_floating_point,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  /* Halt - FPU exception in motor control is dangerous */
  internal_halt_cpu();
}

/**
 * @brief C-level non-maskable interrupt handler
 *
 * @details
 * NMI indicates a fatal system fault. Per the manual (Section 14.1.7):
 * "Never use the non-maskable interrupt with an attempt to return to
 * the program that was being executed at the time of interrupt generation."
 *
 *
 * @warning This function NEVER returns
 */
void rx_exc_nmi_c_handler(uint32_t pc, uint32_t psw)
{
  rx_exception_frame_t frame = {.pc       = pc,
                                .psw      = psw,
                                .type     = k_rx_exception_nmi,
                                .reserved = {0, 0, 0}};

  internal_process_exception(&frame);

  (void)rx_log_error(s_tag, "FATAL: NMI - System halted");

  /* NMI is fatal - MUST NOT return */
  internal_halt_cpu();
}
