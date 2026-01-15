/* lib/rx_core/inc/rx_check.h */

/**
 * @file rx_check.h
 * @brief Error Checking Macros for RX72N Firmware
 * @details
 * Provides ESP-IDF-style error checking macros adapted for RX72N.
 * Follows ESP_ERROR_CHECK pattern for architectural consistency.
 *
 * Error checking strategies:
 * - RX_ERROR_CHECK: Fatal error checking (log and halt)
 * - RX_ERROR_CHECK_WITHOUT_ABORT: Non-fatal error checking (log only)
 * - RX_RETURN_ON_ERROR: Early return on error
 *
 * Usage:
 * @code
 *   rx_err_t err = gpio_init();
 *   RX_ERROR_CHECK(err);  // Halts on error
 *
 *   err = optional_feature_init();
 *   RX_ERROR_CHECK_WITHOUT_ABORT(err);  // Logs but continues
 *
 *   err = critical_operation();
 *   RX_RETURN_ON_ERROR(err, "TAG", "Critical operation failed");
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CHECK_H
#define STAR_RX72N_CHECK_H

#include "rx_err.h"
#include "rx_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Fatal Error Handling
 * =============================================================================
 */

/**
 * @brief Halt execution with error message
 *
 * @param[in] tag Component tag
 * @param[in] message Error message
 * @param[in] err Error code
 *
 * This function logs the error and enters an infinite loop.
 * Used by RX_ERROR_CHECK on fatal errors.
 */
static inline void internal_rx_fatal_error(const char* tag, const char* message, rx_err_t err)
{
  uart_puts("\r\n");
  uart_puts("========================================\r\n");
  uart_puts("FATAL ERROR\r\n");
  uart_puts("========================================\r\n");
  uart_putc('[');
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(message);
  uart_puts("\r\n");
  uart_puts("Error code: ");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
  uart_puts("========================================\r\n");
  uart_puts("System halted. Reset required.\r\n");
  uart_puts("========================================\r\n");

  /* Disable interrupts and halt */
#ifndef UNIT_TEST
  __asm__ volatile("clrpsw i");
  while (1) {
    __asm__ volatile("wait");
  }
#else
  return;
#endif
}

/* =============================================================================
 * Assertion Macros
 * =============================================================================
 */

/**
 * @brief Assert a condition and halt on failure
 *
 * @param[in] condition Boolean condition to check
 * @param[in] message Error message to display on failure
 *
 * If condition is false, logs the error message and halts execution.
 * Use this for checking invariants and preconditions.
 *
 * Example:
 *   RX_ASSERT(ptr != NULL, "Pointer must not be NULL");
 *   RX_ASSERT(count > 0, "Count must be positive");
 */
#define RX_ASSERT(condition, message)                                                              \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      internal_rx_fatal_error("ASSERT", message, k_rx_fail);                                       \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Error Checking Macros
 * =============================================================================
 */

/**
 * @brief Check error code and halt on failure
 *
 * @param[in] err Error code to check
 *
 * If err is not k_rx_ok, logs the error and halts execution.
 * This is a fatal error check - system will not continue.
 *
 * Example:
 *   rx_err_t err = critical_init();
 *   RX_ERROR_CHECK(err);  // Halts if err != k_rx_ok
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
 *
 * @param[in] err Error code to check
 *
 * If err is not k_rx_ok, logs the error but continues execution.
 * Use this for non-critical errors where system can continue.
 *
 * Example:
 *   rx_err_t err = optional_feature_init();
 *   RX_ERROR_CHECK_WITHOUT_ABORT(err);  // Logs but continues
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
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns err from the function.
 * Use this for functions that return rx_err_t.
 *
 * Example:
 *   rx_err_t init_subsystem(void) {
 *       rx_err_t err = gpio_init();
 *       RX_RETURN_ON_ERROR(err, "SUBSYS", "GPIO init failed");
 *       // Only reached if gpio_init succeeded
 *       return k_rx_ok;
 *   }
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
 * @brief Return void on error (for void functions)
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns from the function.
 * Use this for void functions that cannot return an error code.
 *
 * Example:
 *   void configure_system(void) {
 *       rx_err_t err = gpio_init();
 *       RX_RETURN_VOID_ON_ERROR(err, "SYSTEM", "GPIO init failed");
 *       // Only reached if gpio_init succeeded
 *   }
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
 * @brief Return NULL on error (for pointer-returning functions)
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns NULL from the function.
 * Use this for functions that return pointers.
 *
 * Example:
 *   void* create_object(void) {
 *       rx_err_t err = validate_preconditions();
 *       RX_RETURN_NULL_ON_ERROR(err, "FACTORY", "Precondition check failed");
 *       // Only reached if validation succeeded
 *       return allocate_object();
 *   }
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
 *
 * @param[in] ptr Pointer to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If ptr is NULL, logs the error and returns k_rx_err_null_pointer.
 *
 * Example:
 *   rx_err_t process_data(const uint8_t* data) {
 *       RX_CHECK_NULL_PTR(data, "PROCESS", "Data pointer is NULL");
 *       // Only reached if data != NULL
 *       return k_rx_ok;
 *   }
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

#endif /* STAR_RX72N_CHECK_H */
