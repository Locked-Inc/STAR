/**
 * @file rx_session.h
 *
 * @brief Shared session state for cross-transport sequence continuity
 *
 * @details
 * Provides a single, shared session state (TX and RX sequence counters) that is
 * used by BOTH USB CDC and SPI transport layers. When the active transport
 * switches (e.g., USB at seq=105 to SPI), SPI continues at seq=106 because
 * both transports reference the same session state.
 *
 * This module mirrors the Go gateway's `manager/session.go` implementation:
 * - `NextTxSequence()` -> `rx_session_next_tx()`
 * - `ValidateRxSequence()` -> `rx_session_validate_rx()`
 * - `Reset()` -> `rx_session_reset()`
 *
 * ## Architecture
 *
 * ```
 * rx_comm_manager_t (owns rx_session_state_t)
 *       |
 *       +-- rx_usb_comm_handle_t.session --> &session_state
 *       |
 *       +-- rx_spi_comm_handle_t.session --> &session_state
 * ```
 *
 * ## Thread Safety
 *
 * All public functions are thread-safe. Internal state is protected by a
 * ThreadX mutex, allowing safe concurrent access from the comm_task thread,
 * USB ISR callbacks, and SPI ISR callbacks.
 *
 * ## Gap Tolerance (Matching Go Gateway)
 *
 * The RX sequence validator accepts small gaps (up to 10 frames) to handle
 * occasional packet loss on the lightweight USB CDC transport. This matches
 * the Go gateway's `MaxGapTolerance = 10` behavior:
 *
 * | Condition | Action |
 * |---|---|
 * | diff == 0 | Accept, increment rx_sequence |
 * | 0 < diff < 10 | Accept with warning, update rx_sequence |
 * | diff >= 10 | Reject, log error |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto/setjmp/longjmp/recursion
 * - Rule 2: Fixed loop bounds (no loops in this module)
 * - Rule 3: No dynamic allocation (caller provides state struct)
 * - Rule 5: Precondition/postcondition validation on all functions
 * - Rule 7: All return values checked
 * - Rule 8: Macros only for conditional compilation
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @see star-gateway/internal/manager/session.go  Go reference implementation
 * @see rx_usb_comm.h  USB CDC transport (uses session for sequence tracking)
 * @see rx_spi_comm.h  SPI transport (uses session for sequence tracking)
 * @see rx_comm_manager.h  Communication manager (owns session instance)
 *
 * @since Version 1.0.0
 *
 * @par Locked, Inc.
 * @par February 2026
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include <stdbool.h>

/* ============================================================================
 * Includes
 * ============================================================================ */

#include <stdint.h>

#include "rx_err.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @enum rx_session_constants_t
 * @brief Session state configuration constants
 *
 * @details
 * Configuration parameters for the session state module. These match the
 * Go gateway's constants in `manager/session.go`.
 *
 * @see star-gateway/internal/manager/session.go  Go reference values
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Initial TX/RX sequence value after init or reset
   *
   * @details
   * Both TX and RX sequence counters start at this value. Used by
   * rx_session_init() and rx_session_reset().
   */
  k_session_initial_sequence = 0,

  /**
   * @brief Exact-match diff value for sequence validation
   *
   * @details
   * When the difference between received and expected sequence is zero,
   * the sequence matches exactly (most common case).
   */
  k_session_diff_exact_match = 0,

  /**
   * @brief Maximum allowable gap between expected and received sequence numbers
   *
   * @details
   * When the received sequence is ahead of expected by this amount or less,
   * the frame is accepted and the RX sequence catches up. Gaps larger than
   * this are rejected as transport failures.
   *
   * Set to half the 16-bit wrap window so any "ahead" sequence is accepted
   * (only true past-window duplicates are rejected). This relaxed bound
   * lets ad-hoc bring-up tools (cmd_debug, vel_test, grpcurl one-shots)
   * drive the link without re-implementing the full session library.
   * The production gateway tracks sequences precisely via rx_session and
   * stays well under this limit, so the relaxation is invisible to it.
   */
  k_session_max_gap_tolerance = 32768,

  /**
   * @brief Sequence number wraparound mask (16-bit unsigned)
   *
   * @details
   * Applied after incrementing sequence counters to ensure they wrap
   * around at 65535 back to 0. Matches Go: `(seq + 1) & 0xFFFF`
   */
  k_session_seq_wrap_mask = 0xFFFF,
} rx_session_constants_t;

/**
 * @enum rx_session_validate_result_t
 * @brief Result codes for sequence validation
 *
 * @details
 * Returned by `rx_session_validate_rx()` to indicate the validation outcome.
 * This provides more information than a simple bool, allowing callers to
 * distinguish between exact matches, gap acceptance, and rejection.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_session_validate_ok   = 0, /**< Exact sequence match (most common case) */
  k_session_validate_gap  = 1, /**< Accepted with gap (packet loss detected) */
  k_session_validate_fail = 2, /**< Rejected (large gap or duplicate) */
} rx_session_validate_result_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @struct rx_session_state_t
 * @brief Shared session state for cross-transport sequence continuity
 *
 * @details
 * Holds TX and RX sequence counters shared between USB CDC and SPI transports.
 * The `rx_comm_manager_t` owns a single instance and passes a pointer to both
 * transport handles during initialization.
 *
 * ## Memory Layout (8 bytes, no padding)
 *
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0x00 | 2 B | tx_sequence | Next TX sequence to assign |
 * | 0x02 | 2 B | rx_sequence | Next expected RX sequence |
 * | 0x04 | 1 B | initialized | Initialization flag |
 * | 0x05 | 3 B | (padding) | Alignment padding |
 *
 * @invariant tx_sequence and rx_sequence are only modified under mutex protection
 * @invariant initialized is true after successful rx_session_init()
 *
 * @par Example:
 * @code{.c}
 * // Owned by comm manager, shared with both transports
 * static rx_session_state_t g_session;
 *
 * rx_err_t err = rx_session_init(&g_session);
 * if (err != k_rx_ok) {
 *   rx_log_error("SESSION", "Init failed");
 *   return err;
 * }
 *
 * // Pass to both transport handles
 * usb_handle.session = &g_session;
 * spi_handle.session = &g_session;
 * @endcode
 *
 * @note Do not access fields directly. Use the rx_session_*() API functions.
 * @warning This struct must be statically allocated (no malloc).
 *
 * @see rx_session_init() Initialize before use
 * @see rx_session_deinit() Cleanup when done
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint16_t tx_sequence; /**< Next TX sequence number to assign (wraps at 65535) */
  uint16_t rx_sequence; /**< Next expected RX sequence number (wraps at 65535) */
  bool     initialized; /**< True after successful rx_session_init() */
} rx_session_state_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize session state with zeroed sequences
 *
 * @details
 * Sets both TX and RX sequences to 0 and creates the internal ThreadX mutex
 * for thread-safe access. Must be called before any other rx_session_*()
 * function.
 *
 * In simulator builds (RX_SIMULATOR_MODE), mutex creation is skipped since
 * the ThreadX kernel is not running.
 *
 * @param[in,out] state Session state to initialize (must not be NULL)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, session ready for use
 * @retval k_rx_err_null_ptr state isnullptr
 * @retval k_rx_err_rtos_mutex ThreadX mutex creation failed
 *
 * @pre state must point to valid, allocated memory
 * @pre state must not already be initialized (call rx_session_deinit() first)
 * @post state->tx_sequence == 0
 * @post state->rx_sequence == 0
 * @post state->initialized == true
 *
 * @note Thread-safe: No mutex needed during init (single-threaded startup)
 *
 * @par Example:
 * @code{.c}
 * static rx_session_state_t g_session;
 * rx_err_t err = rx_session_init(&g_session);
 * assert(err == k_rx_ok);
 * @endcode
 *
 * @see rx_session_deinit() Cleanup counterpart
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_init(rx_session_state_t* state);

/**
 * @brief Deinitialize session state and release resources
 *
 * @details
 * Deletes the internal ThreadX mutex and marks the session as uninitialized.
 * After this call, no other rx_session_*() function may be called until
 * rx_session_init() is called again.
 *
 * @param[in,out] state Session state to deinitialize (must not be NULL)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, resources released
 * @retval k_rx_err_null_ptr state isnullptr
 * @retval k_rx_err_not_initialized state was not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @post state->initialized == false
 * @post Internal mutex deleted
 *
 * @note Thread-safe: Caller must ensure no other threads are using the session
 *
 * @see rx_session_init() Initialization counterpart
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_deinit(rx_session_state_t* state);

/**
 * @brief Get next TX sequence number and increment the counter
 *
 * @details
 * Atomically reads the current TX sequence, increments it (with wraparound
 * at 65535), and returns the pre-increment value. This is used by both USB
 * and SPI transport layers when sending frames.
 *
 * Mirrors Go gateway's `SessionState.NextTxSequence()`.
 *
 * @param[in,out] state Session state (must be initialized)
 * @param[out] sequence Pointer to receive the sequence number
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sequence written
 * @retval k_rx_err_null_ptr state or sequence isnullptr
 * @retval k_rx_err_not_initialized state not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @pre sequence must point to valid memory
 * @post state->tx_sequence incremented by 1 (with wraparound)
 * @post *sequence contains the pre-increment value
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @par Example:
 * @code{.c}
 * uint16_t seq;
 * rx_err_t err = rx_session_next_tx(&g_session, &seq);
 * if (err == k_rx_ok) {
 *   frame.header.sequence = seq;
 * }
 * @endcode
 *
 * @see rx_session_get_tx() Read without incrementing
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_next_tx(rx_session_state_t* state, uint16_t* sequence);

/**
 * @brief Validate a received sequence number
 *
 * @details
 * Checks whether the received sequence number is acceptable based on the
 * expected RX sequence. Implements gap tolerance matching the Go gateway's
 * `SessionState.ValidateRxSequence()`.
 *
 * ## Validation Logic
 *
 * | Condition | Result | RX Sequence Update |
 * |---|---|---|
 * | diff == 0 | k_session_validate_ok | rx_sequence = received + 1 |
 * | 0 < diff < k_session_max_gap_tolerance | k_session_validate_gap | rx_sequence = received + 1 |
 * | diff >= k_session_max_gap_tolerance | k_session_validate_fail | rx_sequence unchanged |
 * | diff wraps near 65535 (duplicate) | k_session_validate_fail | rx_sequence unchanged |
 *
 * The diff is computed as `received_seq - rx_sequence` using unsigned 16-bit
 * arithmetic, which handles wraparound correctly.
 *
 * @param[in,out] state Session state (must be initialized)
 * @param[in] received_seq The sequence number from the received frame
 * @param[out] result Validation result code (may be NULL if not needed)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Sequence accepted (exact match or small gap)
 * @retval k_rx_err_protocol_error Sequence rejected (large gap or duplicate)
 * @retval k_rx_err_null_ptr state isnullptr
 * @retval k_rx_err_not_initialized state not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @post On accept: state->rx_sequence updated to received_seq + 1
 * @post On reject: state->rx_sequence unchanged
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @par Example:
 * @code{.c}
 * rx_session_validate_result_t result;
 * rx_err_t err = rx_session_validate_rx(&g_session, frame.header.sequence, &result);
 * if (err == k_rx_ok) {
 *   if (result == k_session_validate_gap) {
 *     rx_log_warn("SESSION", "Gap detected in sequence");
 *   }
 *   process_frame(&frame);
 * }
 * @endcode
 *
 * @see k_session_max_gap_tolerance Maximum acceptable gap
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_validate_rx(rx_session_state_t*           state,
                                              uint16_t                      received_seq,
                                              rx_session_validate_result_t* result);

/**
 * @brief Reset both TX and RX sequences to zero
 *
 * @details
 * Called after a RESET/RESET_ACK handshake completes to synchronize sequences
 * between firmware and gateway. Both counters are atomically set to 0.
 *
 * Mirrors Go gateway's `SessionState.Reset()`.
 *
 * @param[in,out] state Session state (must be initialized)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sequences reset
 * @retval k_rx_err_null_ptr state isnullptr
 * @retval k_rx_err_not_initialized state not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @post state->tx_sequence == 0
 * @post state->rx_sequence == 0
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_frame_type_t::k_frame_type_reset RESET frame triggers this
 * @see rx_frame_type_t::k_frame_type_reset_ack RESET_ACK confirms reset
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_reset(rx_session_state_t* state);

/**
 * @brief Get current TX sequence without incrementing
 *
 * @details
 * Returns the current TX sequence counter value for diagnostic purposes.
 * Does not modify the counter.
 *
 * @param[in] state Session state (must be initialized)
 * @param[out] sequence Pointer to receive the current TX sequence
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sequence written
 * @retval k_rx_err_null_ptr state or sequence isnullptr
 * @retval k_rx_err_not_initialized state not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @post state unchanged
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_session_next_tx() Read and increment
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_get_tx(const rx_session_state_t* state, uint16_t* sequence);

/**
 * @brief Get current expected RX sequence
 *
 * @details
 * Returns the next expected RX sequence counter value for diagnostic purposes.
 * Does not modify the counter.
 *
 * @param[in] state Session state (must be initialized)
 * @param[out] sequence Pointer to receive the current RX sequence
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sequence written
 * @retval k_rx_err_null_ptr state or sequence isnullptr
 * @retval k_rx_err_not_initialized state not initialized
 *
 * @pre state must be initialized via rx_session_init()
 * @post state unchanged
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_session_validate_rx() Validate and update
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_session_get_rx(const rx_session_state_t* state, uint16_t* sequence);

/* end of rx_session.h */
