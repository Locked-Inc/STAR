/**
 * @file rx_session.c
 *
 * @brief Shared session state implementation
 *
 * @details
 * Implements the cross-transport shared session state module. Provides
 * thread-safe TX/RX sequence tracking with gap tolerance, mirroring the
 * Go gateway's `manager/session.go` implementation.
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
 * @see rx_session.h  Public API
 * @see star-gateway/internal/manager/session.go  Go reference implementation
 *
 * @since Version 1.0.0
 *
 * @par STAR Project - Texas A&M University
 * @par February 2026
 */

/* ============================================================================
 * Includes
 * ============================================================================ */

#include "rx_session.h"

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
 * @return rx_err_t
 * @retval k_rx_ok Mutex acquired
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre Mutex must have been created via rx_session_init()
 * @post Mutex is held by calling thread
 *
 * @note Thread-safe: This IS the synchronization primitive
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

rx_err_t rx_session_init(rx_session_state_t* state)
{
  if (state == NULL) {
    return k_rx_err_null_ptr;
  }

  state->tx_sequence = 0;
  state->rx_sequence = 0;
  state->initialized = true;

#if !RX_IS_SIMULATOR
  UINT status = tx_mutex_create(&s_session_mutex, "SessionMutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Mutex creation failed");
    state->initialized = false;
    return k_rx_err_rtos_mutex;
  }
#endif

  rx_log_info(s_tag, "Session initialized (tx=0, rx=0)");
  return k_rx_ok;
}

rx_err_t rx_session_deinit(rx_session_state_t* state)
{
  if (state == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

#if !RX_IS_SIMULATOR
  (void)tx_mutex_delete(&s_session_mutex);
#endif

  state->initialized = false;

  rx_log_info(s_tag, "Session deinitialized");
  return k_rx_ok;
}

rx_err_t rx_session_next_tx(rx_session_state_t* state, uint16_t* sequence)
{
  if (state == NULL || sequence == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_lock();
  if (err != k_rx_ok) {
    return err;
  }

  *sequence = state->tx_sequence;
  state->tx_sequence = (state->tx_sequence + 1) & k_session_seq_wrap_mask;

  internal_unlock();
  return k_rx_ok;
}

rx_err_t rx_session_validate_rx(rx_session_state_t*           state,
                                uint16_t                      received_seq,
                                rx_session_validate_result_t* result)
{
  if (state == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_lock();
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate difference using unsigned 16-bit arithmetic (handles wraparound) */
  uint16_t diff = received_seq - state->rx_sequence;

  /* Exact match - most common case */
  if (diff == 0) {
    state->rx_sequence = (state->rx_sequence + 1) & k_session_seq_wrap_mask;
    internal_unlock();

    if (result != NULL) {
      *result = k_session_validate_ok;
    }
    return k_rx_ok;
  }

  /* Small gap (packet loss) - accept and catch up */
  if (diff > 0 && diff < k_session_max_gap_tolerance) {
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

rx_err_t rx_session_reset(rx_session_state_t* state)
{
  if (state == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_lock();
  if (err != k_rx_ok) {
    return err;
  }

  state->tx_sequence = 0;
  state->rx_sequence = 0;

  internal_unlock();

  rx_log_info(s_tag, "Session reset (tx=0, rx=0)");
  return k_rx_ok;
}

rx_err_t rx_session_get_tx(const rx_session_state_t* state, uint16_t* sequence)
{
  if (state == NULL || sequence == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_lock();
  if (err != k_rx_ok) {
    return err;
  }

  *sequence = state->tx_sequence;

  internal_unlock();
  return k_rx_ok;
}

rx_err_t rx_session_get_rx(const rx_session_state_t* state, uint16_t* sequence)
{
  if (state == NULL || sequence == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!state->initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_lock();
  if (err != k_rx_ok) {
    return err;
  }

  *sequence = state->rx_sequence;

  internal_unlock();
  return k_rx_ok;
}
