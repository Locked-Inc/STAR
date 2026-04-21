/**
 * @file rx_eccram.h
 * @brief ECC-Protected RAM (ECCRAM) Driver API
 *
 * @details
 * Public driver interface for the 32 KB ECC-protected RAM region on the
 * RX72N. Provides mode configuration, error-status readout, error
 * clearing, ISR registration for 1-bit/2-bit errors, and region
 * introspection (start/end addresses for use by linker scripts).
 *
 * @par STAR Use Case
 * Bit-flip-resistant storage for safety-critical mutable state:
 * - Motor PID integrator accumulators
 * - Last-good encoder counts
 * - nanopb frame sequence numbers
 * - IWDT kick counter
 * - shared_data_t handoff between motor_control_task and comm_task
 *
 * Single-bit errors are silently corrected by hardware; double-bit errors
 * trigger the registered on_2bit handler, which captures the failing
 * address and typically e-stops the system instead of acting on garbage.
 *
 * @par Initialization Order
 * 1. Call rx_eccram_init(k_eccram_mode_correct_and_detect) during
 *    hardware_init before any code places data in ECCRAM.
 * 2. The driver clears MSTPCRC.MSTPC6, enables ECC generation without
 *    error checking, zeroes the entire 32 KB region with 32-bit stores
 *    so every ECC syndrome matches its data, then switches to the
 *    requested check mode.
 * 3. Call rx_eccram_register_error_isr() to install application
 *    handlers (optional -- the driver installs safe defaults).
 *
 * @par Linker Integration
 * The driver provides `rx_eccram_region_start()` and
 * `rx_eccram_region_end()` so linker scripts can place a dedicated
 * `.eccram` output section between these addresses. See
 * `star-rx72n-firmware/linker.ld`.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Types
 * =============================================================================
 */

/**
 * @enum rx_eccram_mode_t
 * @brief ECCRAM operating modes exposed to application code
 *
 * @details
 * Maps the raw hardware ECCRAMMODE field (RX72N HW Manual Ch60,
 * section 60.2.9, page 2981) into the four behaviours the application
 * actually cares about. The "detect only" configuration is emulated by
 * installing a 1-bit handler that treats any 1-bit event as a fault
 * rather than a silent correction -- the RX72N hardware always corrects
 * 1-bit errors on the data delivered to the CPU when ECC is enabled.
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.9, page 2981
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECC disabled -- contents are unchecked (debug/test only) */
  k_eccram_mode_disabled = 0,
  /** @brief 1-bit correction only -- 2-bit errors go undetected */
  k_eccram_mode_correct_only = 1,
  /** @brief 1-bit correction + 2-bit detection (recommended default) */
  k_eccram_mode_correct_and_detect = 2,
  /** @brief Detect-only: surface both 1-bit and 2-bit errors via ISRs */
  k_eccram_mode_detect_only = 3,
} rx_eccram_mode_t;

/**
 * @struct rx_eccram_status_t
 * @brief Snapshot of ECCRAM error flags and captured addresses
 *
 * @details
 * Returned by rx_eccram_get_error_status() so callers can introspect
 * ECC health without touching registers directly.
 *
 * @invariant one_bit_addr is meaningful only when one_bit_error is true
 * @invariant two_bit_addr is meaningful only when two_bit_error is true
 *
 * @see rx_eccram_get_error_status()
 * @see rx_eccram_clear_errors()
 * @since Version 1.0.0
 */
typedef struct {
  /** @brief true if ECCRAM1STS.ECC1ERR is set */
  bool one_bit_error;
  /** @brief true if ECCRAM2STS.ECC2ERR is set */
  bool two_bit_error;
  /** @brief Captured address of last 1-bit error (from ECCRAM1ECAD) */
  uintptr_t one_bit_addr;
  /** @brief Captured address of last 2-bit error (from ECCRAM2ECAD) */
  uintptr_t two_bit_addr;
} rx_eccram_status_t;

/**
 * @brief 1-bit error ISR callback signature
 *
 * @param[in] addr Captured failing address (from ECCRAM1ECAD)
 * @param[in] ctx  Opaque context pointer supplied at registration
 *
 * @note Invoked from ISR context. Keep it short; log and return.
 * @since Version 1.0.0
 */
typedef void (*rx_eccram_on_1bit_fn_t)(uintptr_t addr, void* ctx);

/**
 * @brief 2-bit error ISR callback signature
 *
 * @param[in] addr Captured failing address (from ECCRAM2ECAD)
 * @param[in] ctx  Opaque context pointer supplied at registration
 *
 * @note Invoked from ISR context. Typical implementation triggers
 *       e-stop via shared_data rather than continuing.
 * @since Version 1.0.0
 */
typedef void (*rx_eccram_on_2bit_fn_t)(uintptr_t addr, void* ctx);

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialise ECCRAM in the requested operating mode
 *
 * @details
 * Performs the full initialisation sequence required by the RX72N HW
 * manual:
 * 1. Clear MSTPCRC.MSTPC6 under system PRCR unlock so that the ECCRAM
 *    control block is clocked (Manual section 60.4.1, page 2994).
 * 2. Unlock ECCRAMPRCR and set ECCRAMMODE = k_rx_eccrammode_ecc_no_check
 *    so ECC bits are generated but nothing is checked.
 * 3. Zero all 32 KB of ECCRAM using aligned 32-bit stores (Manual
 *    section 60.3.3: uninitialised ECC syndromes would falsely flag).
 * 4. Switch ECCRAMMODE to the mode that backs the requested
 *    @p mode argument and re-lock ECCRAMPRCR.
 * 5. For any mode other than k_eccram_mode_disabled, enable the 1-bit
 *    status-update latch (ECCRAM1STSEN).
 *
 * @param[in] mode Desired operating mode (see rx_eccram_mode_t)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Initialisation successful.
 * @retval k_rx_err_invalid_arg    @p mode is outside rx_eccram_mode_t range.
 * @retval k_rx_err_hw_init_failed Hardware mode read-back did not match.
 *
 * @pre  System clock is configured before this call.
 * @pre  Interrupts are globally disabled OR this function is called
 *       before any task can generate ECCRAM traffic.
 * @post MSTPCRC.MSTPC6 == 0 (ECCRAM control block clocked).
 * @post All 32 KB of ECCRAM are zeroed with matching ECC syndromes.
 * @post ECCRAMMODE reflects @p mode.
 *
 * @note Thread Safety: Call once from hardware_init(). Not reentrant.
 * @warning This function must run before any data is placed in the
 *          `.eccram` linker section -- it wipes the region.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 2: Zero-fill loop bounded by k_rx_eccram_region_size_bytes / 4.
 * - Rule 5: 2 preconditions, 3 postconditions.
 * - Rule 7: Every internal helper's return value is checked.
 *
 * @see rx_eccram_register_error_isr()
 * @see rx_eccram_get_error_status()
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_eccram_init(rx_eccram_mode_t mode);

/**
 * @brief Read a snapshot of ECCRAM error status and captured addresses
 *
 * @details
 * Reads ECCRAM1STS, ECCRAM2STS, ECCRAM1ECAD, and ECCRAM2ECAD into a
 * plain struct. No registers are modified -- call rx_eccram_clear_errors()
 * to acknowledge faults.
 *
 * @param[out] out Pointer to status struct populated on success.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok            Status read successfully.
 * @retval k_rx_err_null_ptr  @p out is nullptr.
 *
 * @pre  rx_eccram_init() has been called.
 * @pre  @p out is a valid writable pointer.
 * @post @p out fields reflect the register state at call time.
 * @post No hardware state is modified by this call.
 *
 * @note Thread Safety: Safe for concurrent readers (pure register reads).
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_eccram_get_error_status(rx_eccram_status_t* out);

/**
 * @brief Clear 1-bit and 2-bit error status flags
 *
 * @details
 * Writes k_rx_eccram1sts_clear and k_rx_eccram2sts_clear to acknowledge
 * any outstanding error flags. Does not touch the captured addresses:
 * those remain latched until the next error.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok  Flags cleared.
 *
 * @pre  rx_eccram_init() has been called.
 * @pre  ECCRAMPRCR protection does not guard ECCRAM1STS/ECCRAM2STS
 *       (verified in Manual section 60.2.13 -- both registers are
 *       directly writable without PRCR unlock).
 * @post ECCRAM1STS.ECC1ERR == 0 and ECCRAM2STS.ECC2ERR == 0.
 *
 * @note Thread Safety: Call from ISR or task context; writes are atomic.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_eccram_clear_errors(void);

/**
 * @brief Register user callbacks for ECC error ISRs
 *
 * @details
 * Installs 1-bit and/or 2-bit error callbacks that the driver's ISR
 * trampoline will invoke on the respective RAM-error vectors. Passing
 * nullptr for either callback leaves the previously installed handler
 * in place, allowing callers to register them independently.
 *
 * @par Dependency-Injection Rationale (NASA Rule 9 deviation)
 * Function pointers are permitted here because they are the exact
 * pattern STAR uses to allow test mocks and application-level policy
 * to substitute in without modifying the driver. See CLAUDE.md "SOLID
 * Principles for C / Dependency Inversion".
 *
 * @param[in] on_1bit Handler invoked from ISR on 1-bit error (may be nullptr).
 * @param[in] on_2bit Handler invoked from ISR on 2-bit error (may be nullptr).
 * @param[in] ctx     Opaque pointer passed through to the handlers.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok             Handlers installed.
 * @retval k_rx_err_invalid_state rx_eccram_init() has not yet been called.
 *
 * @pre  rx_eccram_init() has been called.
 * @pre  Handlers, if non-null, are safe to call from interrupt context.
 * @post The driver's ISR trampolines dispatch to the supplied handlers.
 * @post @p ctx is retained until this function is called again.
 *
 * @note Thread Safety: Install once during init before interrupts are
 *       enabled, or guard calls at the task level.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_eccram_register_error_isr(rx_eccram_on_1bit_fn_t on_1bit,
                                                    rx_eccram_on_2bit_fn_t on_2bit,
                                                    void*                  ctx);

/**
 * @brief Return the inclusive start address of the ECCRAM data region
 *
 * @details
 * Exposes the hardware-defined base of the ECC-protected region so that
 * linker scripts and runtime allocators can place objects inside it
 * without hardcoding the literal address.
 *
 * @return Physical address 0x00FF8000 as a uintptr_t.
 *
 * @pre  None.
 * @post Return value equals k_rx_eccram_region_base_addr.
 * @post Return value is word-aligned.
 *
 * @note Thread Safety: Returns a compile-time constant; always safe.
 *
 * @see rx_eccram_region_end()
 * @see RX72N HW Manual Chapter 60, Table 60.1, page 2977
 * @since Version 1.0.0
 */
uintptr_t rx_eccram_region_start(void);

/**
 * @brief Return the inclusive end address of the ECCRAM data region
 *
 * @return Physical address 0x00FFFFFF as a uintptr_t.
 *
 * @pre  None.
 * @post Return value equals k_rx_eccram_region_end_addr.
 * @post Return value - rx_eccram_region_start() + 1 equals 32 KB.
 *
 * @note Thread Safety: Returns a compile-time constant; always safe.
 *
 * @see rx_eccram_region_start()
 * @see RX72N HW Manual Chapter 60, Table 60.1, page 2977
 * @since Version 1.0.0
 */
uintptr_t rx_eccram_region_end(void);

#ifdef __cplusplus
}
#endif
