/* tests/mocks/mock_rx_check.h */

/**
 * @file mock_rx_check.h
 * @brief Mock RX Check Header for Host-Side Testing
 *
 * Provides stub implementations of error checking macros without
 * RX-specific assembly instructions for host-side testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_MOCK_CHECK_H
#define STAR_RX72N_MOCK_CHECK_H

#include "rx_err.h"
#include "rx_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Fatal Error Handling (Mock - does not halt on host)
 * =============================================================================
 */

/**
 * @brief Mock halt execution with error message
 *
 * On host, this just logs the error but does not halt.
 */
static inline void internal_rx_fatal_error(const char* tag, const char* message, rx_err_t err)
{
  (void)tag;
  (void)message;
  (void)err;
  /* No-op for testing - do not halt on host */
}

/* =============================================================================
 * Error Checking Macros
 * =============================================================================
 */

/**
 * @brief Check error code and halt on failure (mock - no halt)
 */
#define RX_ERROR_CHECK(err)                                                                        \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      internal_rx_fatal_error("ERROR_CHECK", "Fatal error detected", err_rc_);                     \
    }                                                                                              \
  } while (0)

/**
 * @brief Check error code and log on failure (non-fatal)
 */
#define RX_ERROR_CHECK_WITHOUT_ABORT(err)                                                          \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error_val("ERROR_CHECK", "Error", err_rc_);                                           \
    }                                                                                              \
  } while (0)

/**
 * @brief Return early on error
 */
#define RX_RETURN_ON_ERROR(err, tag, message)                                                      \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return err_rc_;                                                                              \
    }                                                                                              \
  } while (0)

/**
 * @brief Jump to cleanup label on error
 */
#define RX_GOTO_ON_ERROR(err, label, tag, message)                                                 \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      err = err_rc_;                                                                               \
      goto label;                                                                                  \
    }                                                                                              \
  } while (0)

/**
 * @brief Return void on error
 */
#define RX_RETURN_VOID_ON_ERROR(err, tag, message)                                                 \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Return NULL on error
 */
#define RX_RETURN_NULL_ON_ERROR(err, tag, message)                                                 \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return NULL;                                                                                 \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Null Pointer Checking
 * =============================================================================
 */

/**
 * @brief Check for null pointer and return error
 */
#define RX_CHECK_NULL_PTR(ptr, tag, message)                                                       \
  do {                                                                                             \
    if ((ptr) == NULL) {                                                                           \
      rx_log_error(tag, message);                                                                  \
      return k_rx_err_null_pointer;                                                                \
    }                                                                                              \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MOCK_CHECK_H */
