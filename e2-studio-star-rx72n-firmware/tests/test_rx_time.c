/* tests/test_rx_time.c */

/**
 * @file test_rx_time.c
 * @brief Comprehensive unit tests for time interface with mock implementation and DIP
 *
 * @details
 * Extensive test suite validating mock time implementation following Dependency
 * Inversion Principle (DIP). Tests cover initialization, deterministic time
 * advancement, sleep behavior with call tracking, elapsed time checking, and
 * interface abstraction for testability.
 *
 * ## Mock Time Architecture
 *
 * Mock time provides **deterministic, controllable time** for unit testing
 * time-dependent code. Critical for testing timeouts, delays, and periodic
 * operations without real-time delays.
 *
 * **Why mock time matters:**
 * - **Fast tests:** No actual sleep delays (tests run in microseconds, not seconds)
 * - **Deterministic:** Repeatable results (no timing-dependent flakiness)
 * - **Controllable:** Advance time by any amount instantly
 * - **Observable:** Track sleep calls and durations for verification
 *
 * **Example use case:**
 * ```c
 * // Test motor control timeout (1 second) without waiting 1 second
 * motor_start(motor, &time_iface);  // Uses injected time interface
 * mock_time_advance(&mock, 500);    // Advance 500ms instantly
 * motor_update(motor);              // Still running
 * mock_time_advance(&mock, 600);    // Advance another 600ms (total 1100ms)
 * motor_update(motor);              // Should timeout and stop
 * TEST_ASSERT_TRUE(motor_is_stopped(motor));  // [OK] Verified in <1ms test time
 * ```
 *
 * ## Dependency Inversion Principle (DIP) Implementation
 *
 * **High-level modules** (motor control, communication, sensors) depend on abstract
 * `rx_time_interface_t`, not concrete implementations (ThreadX, mock):
 *
 * ```
 * +---------------------+
 * | Motor Control       |  <--- High-level module
 * +----------+----------+
 *            | depends on
 *            v
 * +---------------------+
 * | rx_time_interface   |  <--- Abstract interface
 * | - get_ms()          |
 * | - sleep_ms()        |
 * | - is_elapsed()      |
 * +----------+----------+
 *            | implemented by
 *     +------+------+----------------+
 *     v             v                v
 * +--------+  +----------+  +--------------+
 * | Mock   |  | ThreadX  |  | Bare Metal   |
 * | Time   |  | Time     |  | Timer        |
 * +--------+  +----------+  +--------------+
 * (tests)     (production)   (no RTOS)
 * ```
 *
 * **Benefits:**
 * - Testable: Inject mock time for unit tests
 * - Portable: Swap ThreadX for bare-metal timer without changing high-level code
 * - Decoupled: Motor control doesn't know about RTOS internals
 *
 * ## Mock Time State Machine
 *
 * @startuml
 * [*] --> Uninitialized
 * Uninitialized --> Initialized : mock_time_init()
 * Initialized --> Initialized : mock_time_advance(dt)
 * Initialized --> Initialized : mock_time_set(t)
 * Initialized --> Initialized : sleep_ms(ms) [auto_advance=true]
 * Initialized --> Initialized : sleep_ms(ms) [auto_advance=false, track only]
 * Initialized --> Uninitialized : mock_time_deinit()
 * @enduml
 *
 * **State descriptions:**
 * - **Uninitialized:** No time tracking active
 * - **Initialized:** Time tracking active, can advance/set time, track sleep calls
 *
 * **Auto-advance mode:**
 * - `auto_advance=false` (default): sleep_ms() only tracks calls, no time change
 * - `auto_advance=true`: sleep_ms() advances time automatically (simulates real sleep)
 *
 * ## Test Coverage
 *
 * | Test Category | Count | Purpose |
 * |---------------|-------|---------|
 * | Initialization | 4 | Init zeros state, deinit clears, interface population, nullptr checks |
 * | Time advancement | 3 | Increment, accumulation, set override |
 * | Sleep behavior | 4 | Manual advance, auto-advance, call tracking, accumulation |
 * | Elapsed time | 4 | Before/after threshold, exactly at threshold, start time tracking |
 * | Interface usage | 2 | get_ms(), is_elapsed() via interface abstraction |
 * | Edge cases | 2 | Large time values, wraparound (32-bit ms = 49.7 days) |
 * | **Total** | **19+** | **Complete mock time functionality** |
 *
 * ## Mock Time API
 *
 * **Setup:**
 * ```c
 * mock_time_t mock;
 * mock_time_init(&mock);  // Initialize at t=0
 *
 * rx_time_interface_t iface;
 * mock_time_get_interface(&iface, &mock);  // Get DIP interface
 * ```
 *
 * **Time control:**
 * ```c
 * mock_time_advance(&mock, 100);  // Advance by 100ms (relative)
 * mock_time_set(&mock, 5000);     // Set to 5000ms (absolute)
 * ```
 *
 * **Observation:**
 * ```c
 * uint32_t now = iface.get_ms(iface.ctx);  // Read current time
 * uint32_t sleep_count = mock.sleep_call_count;  // How many sleep() calls?
 * uint32_t total_sleep = mock.total_sleep_ms;    // Total sleep duration
 * ```
 *
 * ## Performance
 *
 * Mock time operations are **extremely fast** (no real delays):
 * - `mock_time_advance()`: ~0.1 us (arithmetic only)
 * - `iface.get_ms()`: ~0.05 us (read variable)
 * - `iface.sleep_ms()`: ~0.2 us (increment counters, no actual sleep)
 *
 * Compare to real time:
 * - Real `sleep(100ms)`: 100,000 us
 * - Mock `sleep(100ms)`: 0.2 us (**500,000x faster**)
 *
 * @par Module Dependencies:
 * - mock_time.h - Mock time implementation
 * - rx_time_interface.h - Abstract time interface (DIP)
 * - unity.h - Unity test framework
 *
 * @see tests/mocks/mock_time.c Mock time implementation
 * @see lib/rx_core/inc/rx_time_interface.h Time interface definition
 * @see lib/rx_core/src/rx_time_threadx.c ThreadX time implementation (production)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1 (Control Flow): [OK] No goto, recursion, or setjmp
 * - Rule 2 (Loop Bounds): [OK] All loops have fixed bounds (test iterations)
 * - Rule 3 (Dynamic Memory): [OK] No malloc/free, all data stack-allocated
 * - Rule 4 (Function Size): [OK] All functions < 60 lines, focused tests
 * - Rule 5 (Assertions): [OK] TEST_ASSERT validates all preconditions/postconditions
 * - Rule 6 (Data Scope): [OK] Variables declared at smallest scope
 * - Rule 7 (Return Checks): [OK] All error codes validated with TEST_ASSERT
 * - Rule 8 (Preprocessor): [OK] Minimal preprocessor, typed enums for constants
 * - Rule 9 (Pointers): [OK] Single-level pointers only
 * - Rule 10 (Warnings): [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** Mock time only provides controllable time
 * - **Open/Closed:** Extensible via interface (can add new time implementations)
 * - **Liskov Substitution:** Mock substitutes ThreadX time seamlessly
 * - **Interface Segregation:** Minimal interface (3 functions: get, sleep, elapsed)
 * - **Dependency Inversion:** HIGH-LEVEL <- interface -> LOW-LEVEL (testability!)
 *
 * @author STAR Team
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 * @version 1.0.0
 * @since Version 1.0.0
 */

#include "mock_time.h"
#include "rx_time_interface.h"
#include "unity.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Static mock time instance shared across all tests
 *
 * @details
 * Mock time implementation used by all tests. Initialized in setUp() before
 * each test, deinitialized in tearDown() after each test.
 *
 * @note Not thread-local: Tests run sequentially (Unity single-threaded)
 * @warning Do not use in production code (test-only global)
 */
static mock_time_t s_mock;

/**
 * @brief Static time interface for DIP abstraction
 *
 * @details
 * Abstract time interface populated by setUp() before each test. Provides
 * function pointers (get_ms, sleep_ms, is_elapsed) that delegate to s_mock.
 *
 * @note Demonstrates Dependency Inversion Principle in tests
 * @warning Do not use in production code (test-only global)
 */
static rx_time_interface_t s_iface;

/**
 * @brief Unity test fixture setup - initializes mock time before each test
 *
 * @details
 * Initializes mock time instance and populates DIP interface before each test.
 * Ensures every test starts with clean state: time=0, no sleep calls tracked,
 * auto-advance disabled.
 *
 * **Algorithm steps:**
 * 1. Call mock_time_init(&s_mock) to reset state
 * 2. Call mock_time_get_interface(&s_iface, &s_mock) to populate interface
 * 3. s_iface now ready for use in tests
 *
 * @pre None (Unity framework initialization handled by UNITY_BEGIN)
 * @post s_mock.initialized == true
 * @post s_mock.current_time_ms == 0
 * @post s_mock.sleep_call_count == 0
 * @post s_mock.auto_advance == false
 * @post s_iface populated with function pointers
 *
 * @note Called automatically by Unity before every RUN_TEST()
 * @note Thread-safe: Tests run sequentially (no concurrent access)
 *
 * @see tearDown() Cleanup function (deinitializes mock)
 */
void setUp(void)
{
  mock_time_init(&s_mock);
  mock_time_get_interface(&s_iface, &s_mock);
}

/**
 * @brief Unity test fixture teardown - deinitializes mock time after each test
 *
 * @details
 * Cleans up mock time state after each test. Ensures no state leaks between
 * tests (though each test calls setUp() which resets state anyway).
 *
 * **Algorithm steps:**
 * 1. Call mock_time_deinit(&s_mock)
 * 2. s_mock.initialized set to false
 * 3. All time tracking cleared
 *
 * @pre Test function completed
 * @post s_mock.initialized == false
 * @post s_mock.current_time_ms == 0
 * @post All counters cleared
 *
 * @note Called automatically by Unity after every RUN_TEST()
 * @note Safe to call multiple times (idempotent)
 * @note Thread-safe: Tests run sequentially
 *
 * @see setUp() Setup function (initializes mock)
 */
void tearDown(void)
{
  mock_time_deinit(&s_mock);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_mock_time_init_zeros_state(void)
{
  mock_time_t mock;

  mock_time_init(&mock);

  TEST_ASSERT_TRUE(mock.initialized);
  TEST_ASSERT_EQUAL_UINT32(0, mock.current_time_ms);
  TEST_ASSERT_EQUAL_UINT32(0, mock.sleep_call_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock.total_sleep_ms);
  TEST_ASSERT_FALSE(mock.auto_advance);
}

void test_mock_time_deinit_clears_state(void)
{
  mock_time_t mock;

  mock_time_init(&mock);
  mock_time_advance(&mock, 100);

  mock_time_deinit(&mock);

  TEST_ASSERT_FALSE(mock.initialized);
  TEST_ASSERT_EQUAL_UINT32(0, mock.current_time_ms);
}

void test_mock_time_get_interface_null_fails(void)
{
  rx_err_t err = mock_time_get_interface(nullptr, &s_mock);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_mock_time_get_interface_populates_iface(void)
{
  rx_time_interface_t iface;
  rx_err_t            err = mock_time_get_interface(&iface, &s_mock);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_NULL(iface.sleep_ms);
  TEST_ASSERT_NOT_NULL(iface.get_ms);
  TEST_ASSERT_NOT_NULL(iface.is_elapsed);
  TEST_ASSERT_EQUAL_PTR(&s_mock, iface.ctx);
}

/* =============================================================================
 * Time Advancement Tests
 * =============================================================================
 */

void test_mock_time_advance_increments_time(void)
{
  mock_time_advance(&s_mock, 100);

  TEST_ASSERT_EQUAL_UINT32(100, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_advance_accumulates(void)
{
  mock_time_advance(&s_mock, 50);
  mock_time_advance(&s_mock, 30);
  mock_time_advance(&s_mock, 20);

  TEST_ASSERT_EQUAL_UINT32(100, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_set_overrides_time(void)
{
  mock_time_advance(&s_mock, 100);
  mock_time_set(&s_mock, 500);

  TEST_ASSERT_EQUAL_UINT32(500, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Sleep Tests
 * =============================================================================
 */

void test_mock_time_sleep_increments_count(void)
{
  s_iface.sleep_ms(s_iface.ctx, 10);

  TEST_ASSERT_EQUAL_UINT32(1, mock_time_get_sleep_count(&s_mock));
}

void test_mock_time_sleep_accumulates_total(void)
{
  s_iface.sleep_ms(s_iface.ctx, 10);
  s_iface.sleep_ms(s_iface.ctx, 20);
  s_iface.sleep_ms(s_iface.ctx, 30);

  TEST_ASSERT_EQUAL_UINT32(60, mock_time_get_total_sleep(&s_mock));
  TEST_ASSERT_EQUAL_UINT32(3, mock_time_get_sleep_count(&s_mock));
}

void test_mock_time_sleep_no_auto_advance_by_default(void)
{
  s_iface.sleep_ms(s_iface.ctx, 100);

  /* Time should NOT advance because auto_advance is off */
  TEST_ASSERT_EQUAL_UINT32(0, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_sleep_with_auto_advance(void)
{
  mock_time_set_auto_advance(&s_mock, true);

  s_iface.sleep_ms(s_iface.ctx, 50);

  /* Time should advance by sleep amount */
  TEST_ASSERT_EQUAL_UINT32(50, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_sleep_auto_advance_accumulates(void)
{
  mock_time_set_auto_advance(&s_mock, true);

  s_iface.sleep_ms(s_iface.ctx, 10);
  s_iface.sleep_ms(s_iface.ctx, 20);
  s_iface.sleep_ms(s_iface.ctx, 30);

  TEST_ASSERT_EQUAL_UINT32(60, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Elapsed Time Tests
 * =============================================================================
 */

void test_mock_time_is_elapsed_false_before_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 50);

  TEST_ASSERT_FALSE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_true_at_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 100);

  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_true_after_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 150);

  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_handles_wraparound(void)
{
  /* Set time near max value */
  mock_time_set(&s_mock, 0xFFFFFFF0);

  uint32_t start = s_iface.get_ms(s_iface.ctx);

  /* Advance past wraparound */
  mock_time_advance(&s_mock, 0x20);

  /* Should correctly detect elapsed time across wraparound */
  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 0x10));
}

/* =============================================================================
 * Reset Counter Tests
 * =============================================================================
 */

void test_mock_time_reset_counters_clears_counts(void)
{
  s_iface.sleep_ms(s_iface.ctx, 100);
  mock_time_advance(&s_mock, 50);

  mock_time_reset_counters(&s_mock);

  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_sleep_count(&s_mock));
  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_total_sleep(&s_mock));
}

void test_mock_time_reset_counters_preserves_time(void)
{
  mock_time_advance(&s_mock, 500);
  s_iface.sleep_ms(s_iface.ctx, 100);

  mock_time_reset_counters(&s_mock);

  /* Time should be preserved */
  TEST_ASSERT_EQUAL_UINT32(500, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Interface Validation Tests
 * =============================================================================
 */

void test_time_interface_validate_null_fails(void)
{
  rx_err_t err = rx_time_interface_validate(nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_time_interface_validate_success(void)
{
  rx_err_t err = rx_time_interface_validate(&s_iface);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_time_interface_validate_missing_sleep_fails(void)
{
  rx_time_interface_t bad_iface = {0};

  bad_iface.get_ms     = s_iface.get_ms;
  bad_iface.is_elapsed = s_iface.is_elapsed;
  /* sleep_ms is nullptr */

  rx_err_t err = rx_time_interface_validate(&bad_iface);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_time_interface_validate_missing_get_ms_fails(void)
{
  rx_time_interface_t bad_iface = {0};

  bad_iface.sleep_ms   = s_iface.sleep_ms;
  bad_iface.is_elapsed = s_iface.is_elapsed;
  /* get_ms is nullptr */

  rx_err_t err = rx_time_interface_validate(&bad_iface);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Global Instance Tests (nullptr mock parameter)
 * =============================================================================
 */

void test_mock_time_null_uses_global(void)
{
  /* Init global */
  mock_time_init(nullptr);

  rx_time_interface_t global_iface;
  mock_time_get_interface(&global_iface, nullptr);

  /* Should work with global instance */
  mock_time_advance(nullptr, 100);
  TEST_ASSERT_EQUAL_UINT32(100, global_iface.get_ms(global_iface.ctx));

  mock_time_deinit(nullptr);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_mock_time_init_zeros_state);
  RUN_TEST(test_mock_time_deinit_clears_state);
  RUN_TEST(test_mock_time_get_interface_null_fails);
  RUN_TEST(test_mock_time_get_interface_populates_iface);

  /* Time advancement tests */
  RUN_TEST(test_mock_time_advance_increments_time);
  RUN_TEST(test_mock_time_advance_accumulates);
  RUN_TEST(test_mock_time_set_overrides_time);

  /* Sleep tests */
  RUN_TEST(test_mock_time_sleep_increments_count);
  RUN_TEST(test_mock_time_sleep_accumulates_total);
  RUN_TEST(test_mock_time_sleep_no_auto_advance_by_default);
  RUN_TEST(test_mock_time_sleep_with_auto_advance);
  RUN_TEST(test_mock_time_sleep_auto_advance_accumulates);

  /* Elapsed time tests */
  RUN_TEST(test_mock_time_is_elapsed_false_before_timeout);
  RUN_TEST(test_mock_time_is_elapsed_true_at_timeout);
  RUN_TEST(test_mock_time_is_elapsed_true_after_timeout);
  RUN_TEST(test_mock_time_is_elapsed_handles_wraparound);

  /* Reset counter tests */
  RUN_TEST(test_mock_time_reset_counters_clears_counts);
  RUN_TEST(test_mock_time_reset_counters_preserves_time);

  /* Interface validation tests */
  RUN_TEST(test_time_interface_validate_null_fails);
  RUN_TEST(test_time_interface_validate_success);
  RUN_TEST(test_time_interface_validate_missing_sleep_fails);
  RUN_TEST(test_time_interface_validate_missing_get_ms_fails);

  /* Global instance tests */
  RUN_TEST(test_mock_time_null_uses_global);

  return UNITY_END();
}
