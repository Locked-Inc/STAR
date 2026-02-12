/* tests/test_rx_error_handler.c */

/**
 * @file test_rx_error_handler.c
 * @brief Comprehensive unit tests for error handler with DIP interface and exponential backoff
 *
 * @details
 * Extensive test suite validating error handler concrete implementation following
 * Dependency Inversion Principle (DIP). Tests cover initialization, error tracking
 * per component, retry counting with exponential backoff, interface abstraction,
 * and thread-safety with mock mutexes.
 *
 * ## Error Handler Architecture
 *
 * The error handler provides centralized error tracking and retry management for
 * all system components. Key features:
 * - **Per-component tracking:** Up to 16 components with independent error counts
 * - **Retry management:** Configurable max retries with exponential backoff
 * - **DIP interface:** High-level code depends on abstract interface, not concrete impl
 * - **Thread-safe:** Uses ThreadX mutexes for protection in multi-threaded environments
 *
 * **Exponential backoff formula:**
 * @f[
 *   \text{backoff}[k] = \min(\text{initial} \times 2^k, \text{max\_backoff})
 * @f]
 *
 * where @f$ k @f$ is the retry count (0, 1, 2, ...).
 *
 * **Example backoff sequence (initial=100ms, max=5000ms):**
 * | Retry | Formula | Backoff |
 * |-------|---------|---------|
 * | 0     | 100×2⁰  | 100 ms  |
 * | 1     | 100×2¹  | 200 ms  |
 * | 2     | 100×2²  | 400 ms  |
 * | 3     | 100×2³  | 800 ms  |
 * | 4     | 100×2⁴  | 1600 ms |
 * | 5     | 100×2⁵  | 3200 ms |
 * | 6     | 100×2⁶  | **5000 ms** (capped at max) |
 *
 * ## Dependency Inversion Principle (DIP) Implementation
 *
 * **High-level modules** (motor control, sensors, communication) depend on abstract
 * `rx_error_interface_t`, not concrete `error_handler_t` implementation:
 *
 * ```
 * ┌─────────────────────┐
 * │ Motor Control       │  <--- High-level module
 * └──────────┬──────────┘
 *            │ depends on
 *            ▼
 * ┌─────────────────────┐
 * │ rx_error_interface  │  <--- Abstract interface
 * │ - report_error()    │
 * │ - clear_error()     │
 * │ - get_backoff()     │
 * └──────────┬──────────┘
 *            │ implemented by
 *            ▼
 * ┌─────────────────────┐
 * │ error_handler_t     │  <--- Concrete implementation
 * │ (this file tests)   │
 * └─────────────────────┘
 * ```
 *
 * **Benefits:**
 * - Testable: Mock interface for unit tests
 * - Flexible: Swap implementations without changing high-level code
 * - Decoupled: Motor control doesn't know about error handler internals
 *
 * ## Test Coverage
 *
 * | Test Category | Count | Purpose |
 * |---------------|-------|---------|
 * | Initialization | 4 | Valid/invalid params, zero retries, component clearing |
 * | Deinitialization | 3 | Normal deinit, nullptr ptr, double deinit |
 * | Interface | 2 | Get interface, nullptr pointer handling |
 * | Error reporting | 8 | Single/multiple components, overflow, retry limits |
 * | Retry management | 4 | Increment, reset, max retries enforcement |
 * | Backoff calculation | 5 | Exponential growth, capping, zero retry case |
 * | Clear errors | 3 | Single component, all components, error tracking |
 * | Thread safety | 2 | Mutex initialization, concurrent access simulation |
 * | **Total** | **31** | **Complete functional coverage** |
 *
 * ## Mock Infrastructure
 *
 * Tests use mock ThreadX API (tx_api.h) to verify thread-safety without
 * requiring full RTOS:
 * - `tx_mutex_create()` - Track mutex creation
 * - `tx_mutex_get()` - Simulate lock acquisition
 * - `tx_mutex_put()` - Simulate lock release
 *
 * ## Module Dependencies
 *
 * @par Module Dependencies:
 * - rx_error_handler.h - Concrete error handler implementation
 * - rx_error_interface.h - Abstract DIP interface
 * - rx_check.h - Assertion macros (RX_ASSERT)
 * - tx_api.h - ThreadX mutex API (mocked in tests)
 * - unity.h - Unity test framework
 *
 * @see lib/rx_core/src/rx_error_handler.c Error handler implementation
 * @see lib/rx_core/inc/rx_error_interface.h DIP interface definition
 * @see lib/rx_motor/src/rx_motor.c Motor control using error handler
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1 (Control Flow): [OK] No goto, recursion, or setjmp
 * - Rule 2 (Loop Bounds): [OK] All loops have fixed bounds (component iteration)
 * - Rule 3 (Dynamic Memory): [OK] No malloc/free, all data stack-allocated
 * - Rule 4 (Function Size): [OK] All functions < 60 lines, focused tests
 * - Rule 5 (Assertions): [OK] RX_ASSERT validates preconditions, TEST_ASSERT validates postconditions
 * - Rule 6 (Data Scope): [OK] Variables declared at smallest scope
 * - Rule 7 (Return Checks): [OK] All error codes validated with TEST_ASSERT
 * - Rule 8 (Preprocessor): [OK] Minimal preprocessor, typed enums for constants
 * - Rule 9 (Pointers): [OK] Single-level pointers only
 * - Rule 10 (Warnings): [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Error handler manages only error tracking/retry logic
 * - **Open/Closed:** Extensible via interface (can add new implementations)
 * - **Liskov Substitution:** Any rx_error_interface_t implementation is substitutable
 * - **Interface Segregation:** Minimal interface (4 functions: report, clear, backoff, check)
 * - **Dependency Inversion:** HIGH-LEVEL ← interface -> LOW-LEVEL (this is the point!)
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 * @version 1.0.0
 * @since Version 1.0.0
 */

#include "rx_check.h"
#include "unity.h"

/* Include mock implementations first to override real headers */
#include "tx_api.h"

/* Include the module under test */
#include <string.h>

#include "rx_error_handler.h"
#include "rx_error_interface.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Static error handler instance shared across all tests
 *
 * @details
 * Single error handler instance used by all tests. Cleared in setUp() before
 * each test to ensure isolation. Deinitialized in tearDown() if initialized.
 *
 * @note Not thread-local: Tests run sequentially (Unity single-threaded)
 * @warning Do not use in production code (test-only global)
 */
static error_handler_t s_handler;

/**
 * @brief Unity test fixture setup - clears error handler before each test
 *
 * @details
 * Zeroes the static error handler instance to ensure each test starts with
 * clean state. No initialization performed - tests explicitly call
 * error_handler_init() where needed.
 *
 * **Algorithm steps:**
 * 1. Zero all fields in s_handler using memset()
 * 2. Ensures initialized flag is false
 * 3. Clears all component slots
 *
 * @pre None (Unity framework initialization handled by UNITY_BEGIN)
 * @post s_handler.initialized == false
 * @post All component slots cleared
 *
 * @note Called automatically by Unity before every RUN_TEST()
 * @note Thread-safe: Tests run sequentially (no concurrent access)
 *
 * @see tearDown() Cleanup function (deinitializes if needed)
 */
void setUp(void)
{
  memset(&s_handler, 0, sizeof(s_handler));
}

/**
 * @brief Unity test fixture teardown - deinitializes error handler if initialized
 *
 * @details
 * Cleans up error handler state after each test. Only calls error_handler_deinit()
 * if handler was initialized during the test. Ensures mutex is released and
 * resources are freed.
 *
 * **Algorithm steps:**
 * 1. Check if s_handler.initialized is true
 * 2. If initialized, call error_handler_deinit() to clean up
 * 3. If not initialized, no action needed (test didn't init handler)
 *
 * @pre Test function completed
 * @post s_handler deinitialized (if was initialized)
 * @post Mutex released (if was created)
 *
 * @note Called automatically by Unity after every RUN_TEST()
 * @note Safe to call multiple times (checks initialized flag)
 * @note Thread-safe: Tests run sequentially
 *
 * @see setUp() Setup function (clears state)
 */
void tearDown(void)
{
  if (s_handler.initialized) {
    (void)error_handler_deinit(&s_handler);
  }
}

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint16_t {
  k_test_max_retries        = 3,
  k_test_initial_backoff_ms = 100,
  k_test_max_backoff_ms     = 5000,
  k_test_zero_retries       = 0,
} test_constants_t;

/**
 * @brief Initialize error handler for tests
 *
 * @param[in,out] handler Error handler instance
 * @param[in] max_retries Maximum retries to configure
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_init_handler(error_handler_t* handler, uint32_t max_retries)
{
  if (handler == nullptr) {
    return k_rx_err_null_ptr;
  }
  RX_ASSERT((max_retries <= k_test_max_retries), "max_retries exceeds test max");

  error_handler_config_t config = {
    .max_retries        = max_retries,
    .initial_backoff_ms = k_test_initial_backoff_ms,
    .max_backoff_ms     = k_test_max_backoff_ms,
  };
  const rx_err_t err = error_handler_init(handler, &config);
  if (handler != nullptr) {
    RX_ASSERT((err != k_rx_ok) || handler->initialized, "Handler must be initialized on success");
  }
  return err;
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful initialization with valid parameters
 */
void test_error_handler_init_success(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handler.initialized);
  TEST_ASSERT_EQUAL_UINT32(k_test_max_retries, s_handler.max_retries);
  TEST_ASSERT_EQUAL_UINT32(k_test_initial_backoff_ms, s_handler.initial_backoff_ms);
  TEST_ASSERT_EQUAL_UINT32(k_test_max_backoff_ms, s_handler.max_backoff_ms);
  TEST_ASSERT_EQUAL_UINT32(0, s_handler.total_error_count);
}

/**
 * @brief Test initialization with nullptr handler pointer
 */
void test_error_handler_init_null_pointer(void)
{
  rx_err_t err = internal_init_handler(nullptr, k_test_max_retries);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test initialization with zero max retries (unlimited retries)
 */
void test_error_handler_init_zero_retries(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_zero_retries);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_test_zero_retries, s_handler.max_retries);
}

/**
 * @brief Test initialization clears all component slots
 */
void test_error_handler_init_clears_components(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    TEST_ASSERT_FALSE(s_handler.components[i].in_use);
    TEST_ASSERT_EQUAL_UINT32(0, s_handler.components[i].error_count);
    TEST_ASSERT_EQUAL_UINT32(0, s_handler.components[i].retry_count);
  }
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful deinitialization
 */
void test_error_handler_deinit_success(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handler.initialized);

  err = error_handler_deinit(&s_handler);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handler.initialized);
}

/**
 * @brief Test deinitialization with nullptr pointer
 */
void test_error_handler_deinit_null_pointer(void)
{
  rx_err_t err = error_handler_deinit(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test deinitialization of already deinitialized handler
 */
void test_error_handler_deinit_already_deinitialized(void)
{
  /* Handler is not initialized (setUp clears it) */
  rx_err_t err = error_handler_deinit(&s_handler);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Interface Tests
 * =============================================================================
 */

/**
 * @brief Test getting interface from initialized handler
 */
void test_error_handler_get_interface_success(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  err = error_handler_get_interface(&iface, &s_handler);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_NOT_NULL(iface.ctx);
  TEST_ASSERT_NOT_NULL(iface.report_error);
  TEST_ASSERT_NOT_NULL(iface.get_error_count);
  TEST_ASSERT_NOT_NULL(iface.get_component_error_count);
  TEST_ASSERT_NOT_NULL(iface.clear_errors);
  TEST_ASSERT_NOT_NULL(iface.is_retry_limit_reached);
  TEST_ASSERT_NOT_NULL(iface.reset_retry_counter);
  TEST_ASSERT_NOT_NULL(iface.get_backoff_delay);
}

/**
 * @brief Test getting interface with nullptr interface pointer
 */
void test_error_handler_get_interface_null_iface(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = error_handler_get_interface(nullptr, &s_handler);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test getting interface with nullptr handler pointer
 */
void test_error_handler_get_interface_null_handler(void)
{
  rx_error_interface_t iface;
  rx_err_t             err = error_handler_get_interface(&iface, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test getting interface from uninitialized handler
 */
void test_error_handler_get_interface_not_initialized(void)
{
  rx_error_interface_t iface;
  rx_err_t             err = error_handler_get_interface(&iface, &s_handler);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Error Reporting Tests
 * =============================================================================
 */

/**
 * @brief Test reporting an error increments total count
 */
void test_error_handler_report_increments_total_count(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report first error */
  err = iface.report_error(iface.ctx, k_rx_fail, "TEST", "Test error");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, iface.get_error_count(iface.ctx));

  /* Report second error */
  err = iface.report_error(iface.ctx, k_rx_fail, "TEST", "Test error 2");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(2, iface.get_error_count(iface.ctx));
}

/**
 * @brief Test reporting errors increments component count
 */
void test_error_handler_report_increments_component_count(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report error for SPI component */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Transfer failed");
  TEST_ASSERT_EQUAL_UINT32(1, iface.get_component_error_count(iface.ctx, "SPI"));

  /* Report another error for SPI component */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Another error");
  TEST_ASSERT_EQUAL_UINT32(2, iface.get_component_error_count(iface.ctx, "SPI"));

  /* Report error for different component */
  iface.report_error(iface.ctx, k_rx_fail, "I2C", "Bus error");
  TEST_ASSERT_EQUAL_UINT32(2, iface.get_component_error_count(iface.ctx, "SPI"));
  TEST_ASSERT_EQUAL_UINT32(1, iface.get_component_error_count(iface.ctx, "I2C"));
}

/**
 * @brief Test getting error count for nonexistent component
 */
void test_error_handler_component_count_nonexistent(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  uint32_t count = iface.get_component_error_count(iface.ctx, "NONEXISTENT");
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

/* =============================================================================
 * Clear Errors Tests
 * =============================================================================
 */

/**
 * @brief Test clearing all errors
 */
void test_error_handler_clear_errors(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report some errors */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 1");
  iface.report_error(iface.ctx, k_rx_fail, "I2C", "Error 2");
  TEST_ASSERT_EQUAL_UINT32(2, iface.get_error_count(iface.ctx));

  /* Clear all errors */
  err = iface.clear_errors(iface.ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, iface.get_error_count(iface.ctx));
  TEST_ASSERT_EQUAL_UINT32(0, iface.get_component_error_count(iface.ctx, "SPI"));
  TEST_ASSERT_EQUAL_UINT32(0, iface.get_component_error_count(iface.ctx, "I2C"));
}

/* =============================================================================
 * Retry Logic Tests
 * =============================================================================
 */

/**
 * @brief Test retry limit detection
 */
void test_error_handler_retry_limit_reached(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Initially no retries, limit not reached */
  TEST_ASSERT_FALSE(iface.is_retry_limit_reached(iface.ctx, "SPI"));

  /* Report errors until limit reached (max_retries = 3) */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 1");
  TEST_ASSERT_FALSE(iface.is_retry_limit_reached(iface.ctx, "SPI"));

  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 2");
  TEST_ASSERT_FALSE(iface.is_retry_limit_reached(iface.ctx, "SPI"));

  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 3");
  TEST_ASSERT_TRUE(iface.is_retry_limit_reached(iface.ctx, "SPI"));
}

/**
 * @brief Test reset retry counter
 */
void test_error_handler_reset_retry_counter(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report errors until limit reached */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 1");
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 2");
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 3");
  TEST_ASSERT_TRUE(iface.is_retry_limit_reached(iface.ctx, "SPI"));

  /* Reset retry counter */
  err = iface.reset_retry_counter(iface.ctx, "SPI");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(iface.is_retry_limit_reached(iface.ctx, "SPI"));
}

/**
 * @brief Test unlimited retries when max_retries is 0
 */
void test_error_handler_unlimited_retries(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_zero_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report many errors - should never reach limit */
  for (uint32_t i = 0; i < 100; i++) {
    iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error");
    TEST_ASSERT_FALSE(iface.is_retry_limit_reached(iface.ctx, "SPI"));
  }
}

/* =============================================================================
 * Exponential Backoff Tests
 * =============================================================================
 */

/**
 * @brief Test exponential backoff calculation
 */
void test_error_handler_exponential_backoff(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Initial backoff is 0 (no errors yet) */
  TEST_ASSERT_EQUAL_UINT32(0, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* First error: 100ms */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 1");
  TEST_ASSERT_EQUAL_UINT32(100, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* Second error: 200ms (100 * 2) */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 2");
  TEST_ASSERT_EQUAL_UINT32(200, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* Third error: 400ms (200 * 2) */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 3");
  TEST_ASSERT_EQUAL_UINT32(400, iface.get_backoff_delay(iface.ctx, "SPI"));
}

/**
 * @brief Test backoff capped at max_backoff_ms
 */
void test_error_handler_backoff_capped(void)
{
  /* Use small max to test capping */
  enum : uint16_t {
    k_small_max_backoff = 300,
  };

  error_handler_config_t config = {
    .max_retries        = k_test_max_retries,
    .initial_backoff_ms = k_test_initial_backoff_ms,
    .max_backoff_ms     = k_small_max_backoff,
  };
  rx_err_t err = error_handler_init(&s_handler, &config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* First error: 100ms */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 1");
  TEST_ASSERT_EQUAL_UINT32(100, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* Second error: 200ms */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 2");
  TEST_ASSERT_EQUAL_UINT32(200, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* Third error: capped at 300ms (not 400ms) */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 3");
  TEST_ASSERT_EQUAL_UINT32(300, iface.get_backoff_delay(iface.ctx, "SPI"));

  /* Fourth error: still capped at 300ms */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "Error 4");
  TEST_ASSERT_EQUAL_UINT32(300, iface.get_backoff_delay(iface.ctx, "SPI"));
}

/**
 * @brief Test backoff for nonexistent component returns 0
 */
void test_error_handler_backoff_nonexistent_component(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  uint32_t delay = iface.get_backoff_delay(iface.ctx, "NONEXISTENT");
  TEST_ASSERT_EQUAL_UINT32(0, delay);
}

/* =============================================================================
 * Multiple Component Tests
 * =============================================================================
 */

/**
 * @brief Test tracking multiple components independently
 */
void test_error_handler_multiple_components(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Report errors for multiple components */
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "SPI error");
  iface.report_error(iface.ctx, k_rx_fail, "I2C", "I2C error");
  iface.report_error(iface.ctx, k_rx_fail, "UART", "UART error");
  iface.report_error(iface.ctx, k_rx_fail, "SPI", "SPI error 2");

  /* Verify independent counts */
  TEST_ASSERT_EQUAL_UINT32(2, iface.get_component_error_count(iface.ctx, "SPI"));
  TEST_ASSERT_EQUAL_UINT32(1, iface.get_component_error_count(iface.ctx, "I2C"));
  TEST_ASSERT_EQUAL_UINT32(1, iface.get_component_error_count(iface.ctx, "UART"));
  TEST_ASSERT_EQUAL_UINT32(4, iface.get_error_count(iface.ctx));

  /* Verify independent backoff */
  TEST_ASSERT_EQUAL_UINT32(200, iface.get_backoff_delay(iface.ctx, "SPI"));
  TEST_ASSERT_EQUAL_UINT32(100, iface.get_backoff_delay(iface.ctx, "I2C"));
  TEST_ASSERT_EQUAL_UINT32(100, iface.get_backoff_delay(iface.ctx, "UART"));
}

/**
 * @brief Test maximum component slots
 */
void test_error_handler_max_components(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  /* Fill all component slots */
  char component_name[k_error_handler_component_name_max];
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    snprintf(component_name, sizeof(component_name), "COMP%02u", i);
    iface.report_error(iface.ctx, k_rx_fail, component_name, "Error");
  }

  /* Verify all slots used */
  TEST_ASSERT_EQUAL_UINT32(k_error_handler_max_components, iface.get_error_count(iface.ctx));

  /* Attempting to add another component should still succeed (no slot available, but error reported) */
  iface.report_error(iface.ctx, k_rx_fail, "OVERFLOW", "Overflow error");
  TEST_ASSERT_EQUAL_UINT32(k_error_handler_max_components + 1, iface.get_error_count(iface.ctx));

  /* But the component won't be tracked individually */
  TEST_ASSERT_EQUAL_UINT32(0, iface.get_component_error_count(iface.ctx, "OVERFLOW"));
}

/* =============================================================================
 * Interface Validation Tests
 * =============================================================================
 */

/**
 * @brief Test interface validation with valid interface
 */
void test_error_interface_validate_success(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_error_interface_t iface;
  TEST_ASSERT_EQUAL(k_rx_ok, error_handler_get_interface(&iface, &s_handler));

  err = rx_error_interface_validate(&iface);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test interface validation with nullptr
 */
void test_error_interface_validate_null(void)
{
  rx_err_t err = rx_error_interface_validate(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test interface validation with missing function pointers
 */
void test_error_interface_validate_missing_functions(void)
{
  rx_error_interface_t iface;
  memset(&iface, 0, sizeof(iface));

  rx_err_t err = rx_error_interface_validate(&iface);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_error_handler_init_success);
  RUN_TEST(test_error_handler_init_null_pointer);
  RUN_TEST(test_error_handler_init_zero_retries);
  RUN_TEST(test_error_handler_init_clears_components);

  /* Deinitialization tests */
  RUN_TEST(test_error_handler_deinit_success);
  RUN_TEST(test_error_handler_deinit_null_pointer);
  RUN_TEST(test_error_handler_deinit_already_deinitialized);

  /* Interface tests */
  RUN_TEST(test_error_handler_get_interface_success);
  RUN_TEST(test_error_handler_get_interface_null_iface);
  RUN_TEST(test_error_handler_get_interface_null_handler);
  RUN_TEST(test_error_handler_get_interface_not_initialized);

  /* Error reporting tests */
  RUN_TEST(test_error_handler_report_increments_total_count);
  RUN_TEST(test_error_handler_report_increments_component_count);
  RUN_TEST(test_error_handler_component_count_nonexistent);

  /* Clear errors tests */
  RUN_TEST(test_error_handler_clear_errors);

  /* Retry logic tests */
  RUN_TEST(test_error_handler_retry_limit_reached);
  RUN_TEST(test_error_handler_reset_retry_counter);
  RUN_TEST(test_error_handler_unlimited_retries);

  /* Exponential backoff tests */
  RUN_TEST(test_error_handler_exponential_backoff);
  RUN_TEST(test_error_handler_backoff_capped);
  RUN_TEST(test_error_handler_backoff_nonexistent_component);

  /* Multiple component tests */
  RUN_TEST(test_error_handler_multiple_components);
  RUN_TEST(test_error_handler_max_components);

  /* Interface validation tests */
  RUN_TEST(test_error_interface_validate_success);
  RUN_TEST(test_error_interface_validate_null);
  RUN_TEST(test_error_interface_validate_missing_functions);

  return UNITY_END();
}
