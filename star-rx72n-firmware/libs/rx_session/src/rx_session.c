/**
 * @file rx_session.c
 *
 * @brief Shared session state implementation for cross-transport sequence continuity
 *
 * @details
 * Implements the cross-transport shared session state module. Provides
 * thread-safe TX/RX sequence tracking with gap tolerance, mirroring the
 * Go gateway's `manager/session.go` implementation.
 *
 * Both USB CDC and SPI transports share a single session state instance
 * (owned by rx_comm_manager_t), ensuring sequence continuity when the active
 * transport switches.
 *
 * ## Thread Safety
 *
 * All public functions acquire the internal ThreadX mutex before accessing
 * state. The mutex uses TX_NO_INHERIT (no priority inheritance) since
 * contention is expected to be low (comm_task is the primary consumer).
 *
 * In simulator builds (RX_SIMULATOR_MODE), mutex operations are skipped
 * since the ThreadX kernel is not running.
 *
 * @pre Caller must call rx_session_init() before any other API function.
 * @post After rx_session_deinit(), no API calls may be made until re-init.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto/setjmp/longjmp/recursion
 * - Rule 5: RX_CHECK_NULL_PTR / RX_VALIDATE_INIT preconditions on all functions
 * - Rule 7: All return values checked (RX_RETURN_ON_ERROR)
 * - Rule 8: Macros only for conditional compilation and validation
 *
 * @par SOLID Compliance:
 * - S: Single responsibility (sequence tracking only)
 * - D: Depends on rx_err_t abstraction, not concrete transport details
 *
 * @author Locked, Inc. Team
 * @date February 2026
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see rx_session.h  Public API
 * @see star-gateway/internal/manager/session.go  Go reference implementation
 *
 * @since Version 1.0.0
 */

/* ============================================================================
 * Includes
 * ============================================================================ */

#include "rx_session.h"

#include "rx_check.h"
#include "rx_log.h"
#include "rx_simulator_config.h"

#if !RX_IS_SIMULATOR
#include "tx_api.h"
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @var s_tag
 * @brief Logging tag for session state messages
 * @note Used with rx_log_*() macros for consistent log filtering
 * @since Version 1.0.0
 */
static const char s_tag[] = "SESSION";

/* ============================================================================
 * Private Data
 * ============================================================================ */

#if !RX_IS_SIMULATOR
/**
 * @var s_session_mutex
 * @brief ThreadX mutex for protecting session state access
 *
 * @details
 * Single mutex shared across all session operations. Created during
 * rx_session_init() and deleted during rx_session_deinit().
 *
 * @warning Do not access directly. Use internal_lock() / internal_unlock().
 * @since Version 1.0.0
 */
static TX_MUTEX s_session_mutex;
#endif

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Acquire the session mutex
 *
 * @details
 * Locks the internal mutex with TX_WAIT_FOREVER. In simulator mode, this
 * is a no-op since ThreadX is not running.
 *
 *
 * @pre Mutex must have been created via rx_session_init()
 * @post Mutex is held by calling thread
 *
 * @note Thread-safe: This IS the synchronization primitive
 * @since Version 1.0.0
 */
static rx_err_t internal_lock(void)
{
#if !RX_IS_SIMULATOR
  UINT status = tx_mutex_get(&s_session_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex lock failed");
    return k_rx_err_rtos_mutex;
  }
#endif
  return k_rx_ok;
}

/**
 * @brief Release the session mutex
 *
 * @details
 * Unlocks the internal mutex. In simulator mode, this is a no-op.
 *
 * @pre Mutex must be held by calling thread
 * @post Mutex is released
 *
 * @note Thread-safe: This IS the synchronization primitive
 * @since Version 1.0.0
 */
static void internal_unlock(void)
{
#if !RX_IS_SIMULATOR
  (void)tx_mutex_put(&s_session_mutex);
#endif
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * @brief Initialize session state with zeroed sequences
 *
 * @details
 * Sets both TX and RX sequences to k_session_initial_sequence and creates
 * the internal ThreadX mutex for thread-safe access. Must be called before
 * any other rx_session_*() function.
 *
 * In simulator builds (RX_SIMULATOR_MODE), mutex creation is skipped since
 * the ThreadX kernel is not running.
 *
 *
 *
 * @pre state must point to valid, allocated memory
 * @pre state must not already be initialized (call rx_session_deinit() first)
 * @post state->tx_sequence == k_session_initial_sequence
 * @post state->rx_sequence == k_session_initial_sequence
 * @post state->initialized == true
 *
 * @note Thread-safe: No mutex needed during init (single-threaded startup)
 *
 * @see rx_session_deinit() Cleanup counterpart
 * @since Version 1.0.0
 */
rx_err_t rx_session_init(rx_session_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "Init: state is NULL");

  state->tx_sequence = k_session_initial_sequence;
  state->rx_sequence = k_session_initial_sequence;
  state->initialized = true;

#if !RX_IS_SIMULATOR
  UINT status = tx_mutex_create(&s_session_mutex, "SessionMutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex creation failed");
    state->initialized = false;
    return k_rx_err_rtos_mutex;
  }
#endif

  /* Post-condition: verify initialization succeeded */

  rx_log_info(s_tag, "Session initialized (tx=0, rx=0)");
  return k_rx_ok;
}

/**
 * @brief Deinitialize session state and release resources
 *
 * @details
 * Deletes the internal ThreadX mutex and marks the session as uninitialized.
 * After this call, no other rx_session_*() function may be called until
 * rx_session_init() is called again.
 *
 * In simulator builds, mutex deletion is skipped since it was never created.
 *
 *
 *
 * @pre state !=nullptr
 * @pre state->initialized == true
 * @post state->initialized == false
 * @post Internal mutex deleted (on non-simulator builds)
 *
 * @note Thread-safe: Caller must ensure no other threads are using the session
 *
 * @see rx_session_init() Initialization counterpart
 * @since Version 1.0.0
 */
rx_err_t rx_session_deinit(rx_session_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "Deinit: state is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "Deinit: session not initialized");
    return k_rx_err_not_initialized;
  }

#if !RX_IS_SIMULATOR
  /* tx_mutex_delete return value deliberately discarded: mutex deletion errors
   * are non-fatal during deinit because the mutex may already be uninitialized
   * or no recovery is possible. The module is being torn down regardless. */
  (void)tx_mutex_delete(&s_session_mutex);
#endif

  state->initialized = false;

  rx_log_info(s_tag, "Session deinitialized");
  return k_rx_ok;
}

/**
 * @brief Get next TX sequence number and increment the counter
 *
 * @details
 * Atomically reads the current TX sequence, increments it (with wraparound
 * at k_session_seq_wrap_mask), and returns the pre-increment value. This is
 * used by both USB and SPI transport layers when sending frames.
 *
 * Mirrors Go gateway's `SessionState.NextTxSequence()`.
 *
 *
 *
 * @pre state must be initialized via rx_session_init()
 * @pre sequence must point to valid memory
 * @post state->tx_sequence incremented by 1 (with wraparound)
 * @post *sequence contains the pre-increment value
 * @post *sequence <= k_session_seq_wrap_mask
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_session_get_tx() Read without incrementing
 * @since Version 1.0.0
 */
rx_err_t rx_session_next_tx(rx_session_state_t* state, uint16_t* sequence)
{
  RX_CHECK_NULL_PTR(state, s_tag, "NextTx: state is NULL");
  RX_CHECK_NULL_PTR(sequence, s_tag, "NextTx: sequence is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "NextTx: session not initialized");
    return k_rx_err_not_initialized;
  }

  rx_err_t lock_err = internal_lock();
  (void)lock_err;

  *sequence          = state->tx_sequence;
  state->tx_sequence = (state->tx_sequence + 1) & k_session_seq_wrap_mask;

  internal_unlock();

  /* Post-condition: *sequence <= k_session_seq_wrap_mask.
   * Guaranteed by the bitwise AND above; no runtime check needed. */

  return k_rx_ok;
}

/**
 * @brief Validate a received sequence number against expected RX sequence
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
 * | diff == k_session_diff_exact_match | k_session_validate_ok | rx_sequence = received + 1 |
 * | 0 < diff < k_session_max_gap_tolerance | k_session_validate_gap | rx_sequence = received + 1 |
 * | diff >= k_session_max_gap_tolerance | k_session_validate_fail | rx_sequence unchanged |
 *
 *
 *
 * @pre state must be initialized via rx_session_init()
 * @post On accept: state->rx_sequence updated to received_seq + 1
 * @post On reject: state->rx_sequence unchanged
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see k_session_max_gap_tolerance Maximum acceptable gap
 * @since Version 1.0.0
 */
rx_err_t rx_session_validate_rx(rx_session_state_t*           state,
                                uint16_t                      received_seq,
                                rx_session_validate_result_t* result)
{
  RX_CHECK_NULL_PTR(state, s_tag, "ValidateRx: state is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "ValidateRx: session not initialized");
    return k_rx_err_not_initialized;
  }

  rx_err_t lock_err = internal_lock();
  (void)lock_err;

  /* Calculate difference using unsigned 16-bit arithmetic (handles wraparound) */
  uint16_t diff = received_seq - state->rx_sequence;

  /* Exact match - most common case */
  if (diff == k_session_diff_exact_match) {
    state->rx_sequence = (state->rx_sequence + 1) & k_session_seq_wrap_mask;
    internal_unlock();

    if (result != NULL) {
      *result = k_session_validate_ok;
    }
    return k_rx_ok;
  }

  /* Small gap (packet loss) - accept and catch up.
   * diff > k_session_diff_exact_match is always true here: the exact-match case
   * already returned above, so the condition reduces to a single comparison. */
  if (diff < k_session_max_gap_tolerance) {
    rx_log_warn_val(s_tag, "Sequence gap detected, frames lost", diff);
    state->rx_sequence = (received_seq + 1) & k_session_seq_wrap_mask;
    internal_unlock();

    if (result != NULL) {
      *result = k_session_validate_gap;
    }
    return k_rx_ok;
  }

  /* Large gap or duplicate (diff >= gap tolerance or near-65535 wraparound) */
  rx_log_error_val(s_tag, "Sequence rejected", received_seq);

  internal_unlock();

  if (result != NULL) {
    *result = k_session_validate_fail;
  }
  return k_rx_err_protocol_error;
}

/**
 * @brief Reset both TX and RX sequences to initial values
 *
 * @details
 * Called after a RESET/RESET_ACK handshake completes to synchronize sequences
 * between firmware and gateway. Both counters are atomically set to
 * k_session_initial_sequence.
 *
 * Mirrors Go gateway's `SessionState.Reset()`.
 *
 *
 *
 * @pre state must be initialized via rx_session_init()
 * @post state->tx_sequence == k_session_initial_sequence
 * @post state->rx_sequence == k_session_initial_sequence
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_frame_type_t::k_frame_type_reset RESET frame triggers this
 * @see rx_frame_type_t::k_frame_type_reset_ack RESET_ACK confirms reset
 * @since Version 1.0.0
 */
rx_err_t rx_session_reset(rx_session_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "Reset: state is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "Reset: session not initialized");
    return k_rx_err_not_initialized;
  }

  rx_err_t lock_err = internal_lock();
  (void)lock_err;

  state->tx_sequence = k_session_initial_sequence;
  state->rx_sequence = k_session_initial_sequence;

  internal_unlock();

  rx_log_info(s_tag, "Session reset (tx=0, rx=0)");
  return k_rx_ok;
}

/**
 * @brief Get current TX sequence without incrementing
 *
 * @details
 * Returns the current TX sequence counter value for diagnostic purposes.
 * Does not modify the counter.
 *
 *
 *
 * @pre state must be initialized via rx_session_init()
 * @pre sequence must point to valid memory
 * @post state unchanged
 * @post *sequence <= k_session_seq_wrap_mask
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_session_next_tx() Read and increment
 * @since Version 1.0.0
 */
rx_err_t rx_session_get_tx(const rx_session_state_t* state, uint16_t* sequence)
{
  RX_CHECK_NULL_PTR(state, s_tag, "GetTx: state is NULL");
  RX_CHECK_NULL_PTR(sequence, s_tag, "GetTx: sequence is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "GetTx: session not initialized");
    return k_rx_err_not_initialized;
  }

  rx_err_t lock_err = internal_lock();
  (void)lock_err;

  *sequence = state->tx_sequence;

  internal_unlock();

  /* Post-condition: *sequence <= k_session_seq_wrap_mask.
   * Guaranteed by the invariant on tx_sequence; no runtime check needed. */

  return k_rx_ok;
}

/**
 * @brief Get current expected RX sequence without modifying
 *
 * @details
 * Returns the next expected RX sequence counter value for diagnostic purposes.
 * Does not modify the counter.
 *
 *
 *
 * @pre state must be initialized via rx_session_init()
 * @pre sequence must point to valid memory
 * @post state unchanged
 * @post *sequence <= k_session_seq_wrap_mask
 *
 * @note Thread-safe: Protected by internal mutex
 *
 * @see rx_session_validate_rx() Validate and update
 * @since Version 1.0.0
 */
rx_err_t rx_session_get_rx(const rx_session_state_t* state, uint16_t* sequence)
{
  RX_CHECK_NULL_PTR(state, s_tag, "GetRx: state is NULL");
  RX_CHECK_NULL_PTR(sequence, s_tag, "GetRx: sequence is NULL");
  if (!state->initialized) {
    rx_log_error(s_tag, "GetRx: session not initialized");
    return k_rx_err_not_initialized;
  }

  rx_err_t lock_err = internal_lock();
  (void)lock_err;

  *sequence = state->rx_sequence;

  internal_unlock();

  /* Post-condition: *sequence <= k_session_seq_wrap_mask.
   * Guaranteed by the invariant on rx_sequence; no runtime check needed. */

  return k_rx_ok;
}
