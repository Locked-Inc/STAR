/**
 * @file test_rx_stack_monitor.c
 * @brief Unit Tests for ThreadX Stack Overflow Detection and High-Water Mark Monitoring
 *
 * @details
 * Validates the public API of rx_stack_monitor.c using the Unity test framework
 * and the existing mock ThreadX layer (tests/mocks/tx_api.h).
 *
 * ## Scope
 *
 * These tests exercise the host-compiled, UNIT_TEST-guarded code paths.  The
 * [[noreturn]] attribute on the overflow handler is suppressed in UNIT_TEST
 * builds, so the test runner can call internal_stack_overflow_handler()
 * indirectly through rx_stack_monitor_init() without crashing.
 *
 * ## Test Coverage
 *
 * | Category | Tests | Notes |
 * |----------|-------|-------|
 * | Initialization | 2 | success, idempotent re-registration |
 * | High-water mark | 6 | NULL args, null stack_start, pattern absent, fully filled, partially filled, null output buf check |
 * | Total | 8 | Complete public API coverage |
 *
 * ## How Stack Checking Works in Tests
 *
 * The mock tx_api.h implements tx_thread_stack_error_notify() as a no-op
 * returning TX_SUCCESS, so rx_stack_monitor_init() always returns k_rx_ok in
 * host builds.  This exercises the registration path without needing the
 * ThreadX kernel.
 *
 * The high-water mark tests populate a byte array with known patterns and
 * attach it to a mock TX_THREAD, then verify that
 * rx_stack_monitor_get_free_bytes() counts correctly.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 2: All loop bounds static (stack size known at test start)
 * - Rule 3: No dynamic memory (stack arrays are compile-time constants)
 * - Rule 4: All functions < 30 lines
 * - Rule 5: Every test has >= 2 TEST_ASSERT checks
 * - Rule 7: All return values checked via TEST_ASSERT_EQUAL
 * - Rule 8: C23 typed enums for all integer constants
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @see rx_stack_monitor.h Public API under test
 * @see rx_stack_monitor.c Implementation
 * @see tests/mocks/tx_api.h Mock ThreadX API
 *
 * @par SOLID Adherence:
 * - Single Responsibility: Tests one module (rx_stack_monitor) only
 * - Dependency Inversion: Firmware coupled to mock ThreadX via include path
 *
 * @date 2026-01-11
 * @version 1.0.0
 * @par Hardware:
 * - Runs on host (x86-64/aarch64) under CTest; no RX72N hardware required
 *
 * @since Version 1.0.0
 * @author Locked, Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rx_err.h"
#include "rx_stack_monitor.h"
#include "tx_api.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_stack_constants_t
 * @brief Stack buffer sizes and fill-pattern constants for high-water mark tests
 *
 * @details
 * These sizes are chosen to be large enough to verify scanning correctness
 * while remaining small enough for stack allocation in test functions.
 *
 * @invariant k_test_stack_size > k_test_used_bytes (at least one free byte)
 *
 * @code
 * uint8_t stack_buf[k_test_stack_size];
 * memset(stack_buf, (uint8_t)k_stack_fill_byte, k_test_stack_size);
 * memset(&stack_buf[k_test_stack_size - k_test_used_bytes],
 *        (uint8_t)k_stack_fill_absent_byte, k_test_used_bytes);
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_stack_size        = 64U,   /**< Total mock stack buffer size in bytes */
  k_test_used_bytes        = 20U,   /**< Bytes "used" (not 0xEF) at high end of stack */
  k_stack_fill_byte        = 0xEFU, /**< ThreadX stack fill sentinel byte */
  k_stack_fill_absent_byte = 0x00U, /**< Fill byte used to simulate absent fill pattern */
  k_zero_u32               = 0U,    /**< Named zero constant (avoids raw 0 literals; NASA Rule 8) */
} test_stack_constants_t;

/* =============================================================================
 * Unity Lifecycle Callbacks
 * =============================================================================
 */

/**
 * @brief Unity setUp - called before each test
 *
 * @details No global state to reset in this module.
 *
 * @pre Unity test framework has been initialised via UNITY_BEGIN()
 * @pre No test has left residual side-effects (module has no shared state)
 * @post Test environment is clean; no state carried in from prior tests
 * @post No dynamic resources have been allocated
 *
 * @note Not thread-safe; Unity runs tests single-threaded
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* No shared state to reset between tests */
}

/**
 * @brief Unity tearDown - called after each test
 *
 * @details No resources to release in this module.
 *
 * @pre The preceding test has completed (pass or fail)
 * @pre No dynamic resources were allocated during the test
 * @post All stack buffers declared in the test are out of scope
 * @post No allocated resources remain; heap is unchanged
 *
 * @note Not thread-safe; Unity runs tests single-threaded
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* No cleanup required */
}

/* =============================================================================
 * rx_stack_monitor_init() Tests
 * =============================================================================
 */

/**
 * @brief Verify that rx_stack_monitor_init() returns k_rx_ok
 *
 * @details
 * The mock tx_thread_stack_error_notify() always returns TX_SUCCESS, so
 * rx_stack_monitor_init() must map that to k_rx_ok.
 *
 * @pre Mock tx_api.h in include path (TX_SUCCESS stub)
 * @pre UNIT_TEST build context active (mock tx_thread_stack_error_notify present)
 * @post rx_stack_monitor_init() returned k_rx_ok
 * @post No k_rx_err_not_supported error was logged (stack checking feature present in mock)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Call rx_stack_monitor_init()
 * 2. Verify return value is k_rx_ok
 *
 * @see rx_stack_monitor_init()
 * @since Version 1.0.0
 */
static void test_init_succeeds(void)
{
  rx_err_t err = rx_stack_monitor_init();

  /* Postcondition 1: mock TX_SUCCESS maps to k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Postcondition 2: stack checking feature was not reported as absent */
  TEST_ASSERT_NOT_EQUAL(k_rx_err_not_supported, err);
}

/**
 * @brief Verify that calling rx_stack_monitor_init() twice is idempotent
 *
 * @details
 * ThreadX allows re-registering the stack error handler; calling init twice
 * must not fail.  This guards against accidental double-registration in
 * tx_application_define().
 *
 * @pre rx_stack_monitor_init() succeeds on the first call
 * @pre Mock tx_thread_stack_error_notify() always returns TX_SUCCESS
 * @post Second call also returns k_rx_ok (both err_first and err_second are k_rx_ok)
 * @post No error or not-supported code is returned by either call
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Call rx_stack_monitor_init() twice
 * 2. Verify both calls return k_rx_ok
 *
 * @see rx_stack_monitor_init()
 * @since Version 1.0.0
 */
static void test_init_idempotent(void)
{
  rx_err_t err_first  = rx_stack_monitor_init();
  rx_err_t err_second = rx_stack_monitor_init();

  TEST_ASSERT_EQUAL(k_rx_ok, err_first);
  TEST_ASSERT_EQUAL(k_rx_ok, err_second);
}

/* =============================================================================
 * rx_stack_monitor_get_free_bytes() Tests
 * =============================================================================
 */

/**
 * @brief Verify that NULL thread_ptr returns k_rx_err_null_ptr
 *
 * @details
 * Passes NULL as thread_ptr; expects the function to reject it with
 * k_rx_err_null_ptr before touching free_bytes.
 *
 * @pre free_bytes is initialised to k_zero_u32 before the call
 * @pre No valid TX_THREAD is required; thread_ptr is intentionally NULL
 * @post Return value is k_rx_err_null_ptr
 * @post free_bytes remains unchanged at k_zero_u32 (not written on error)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Call rx_stack_monitor_get_free_bytes(NULL, &free_bytes)
 * 2. Verify return value is k_rx_err_null_ptr
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_null_thread(void)
{
  uint32_t free_bytes = k_zero_u32;

  rx_err_t err = rx_stack_monitor_get_free_bytes(NULL, &free_bytes);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  /* free_bytes must not have been written */
  TEST_ASSERT_EQUAL(k_zero_u32, free_bytes);
}

/**
 * @brief Verify that NULL free_bytes output pointer returns k_rx_err_null_ptr
 *
 * @details
 * Passes a valid thread_ptr but NULL for the output pointer; function must
 * not dereference the NULL and must return k_rx_err_null_ptr.
 *
 * @pre Valid TX_THREAD with stack_buf filled with k_stack_fill_byte (0xEF)
 * @pre tx_thread_stack_size is set to k_test_stack_size (non-zero)
 * @post Return value is k_rx_err_null_ptr (not k_rx_ok)
 * @post stack_buf contents are unchanged (no bytes were written by the function)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Create a mock TX_THREAD with valid stack fields
 * 2. Call rx_stack_monitor_get_free_bytes(thread_ptr, NULL)
 * 3. Verify return value is k_rx_err_null_ptr
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_null_output(void)
{
  uint8_t   stack_buf[k_test_stack_size];
  TX_THREAD thread = {
    .tx_thread_name        = "MockThread",
    .tx_thread_id          = k_tx_invalid_id,
    .tx_thread_stack_start = stack_buf,
    .tx_thread_stack_size  = k_test_stack_size,
  };

  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(stack_buf, (uint8_t)k_stack_fill_byte, k_test_stack_size);

  rx_err_t err = rx_stack_monitor_get_free_bytes(&thread, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);

  /* Postcondition (NASA Rule 5): stack buffer must not have been modified */
  for (uint32_t i = k_zero_u32; i < k_test_stack_size; i++) {
    TEST_ASSERT_EQUAL((uint8_t)k_stack_fill_byte, stack_buf[i]);
  }
}

/**
 * @brief Verify return value when fill pattern is absent
 *
 * @details
 * Simulates a stack where the fill pattern was never written (e.g.,
 * TX_DISABLE_STACK_FILLING was defined).  The first byte is 0x00 rather
 * than 0xEF, so the function should detect the absent pattern and return
 * k_rx_err_invalid_state.
 *
 * @pre stack_buf[0] != 0xEF (filled with k_stack_fill_absent_byte = 0x00)
 * @pre free_bytes is initialised to k_zero_u32 before the call
 * @post Return value is k_rx_err_invalid_state
 * @post free_bytes remains k_zero_u32 (not written on error)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Create stack_buf filled with 0x00
 * 2. Attach to mock TX_THREAD
 * 3. Call rx_stack_monitor_get_free_bytes()
 * 4. Verify return value is k_rx_err_invalid_state
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_pattern_absent(void)
{
  uint8_t   stack_buf[k_test_stack_size];
  TX_THREAD thread = {
    .tx_thread_name        = "MockThread",
    .tx_thread_id          = k_tx_invalid_id,
    .tx_thread_stack_start = stack_buf,
    .tx_thread_stack_size  = k_test_stack_size,
  };
  uint32_t free_bytes = k_zero_u32;

  /* Fill with absent-pattern byte - no 0xEF sentinel present */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(stack_buf, (uint8_t)k_stack_fill_absent_byte, k_test_stack_size);

  rx_err_t err = rx_stack_monitor_get_free_bytes(&thread, &free_bytes);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  /* Postcondition: output parameter must not be modified on error */
  TEST_ASSERT_EQUAL(k_zero_u32, free_bytes);
}

/**
 * @brief Verify high-water mark when the entire stack is unused
 *
 * @details
 * Fills the entire stack buffer with 0xEF (thread has never executed).
 * Expects free_bytes == k_test_stack_size (all bytes unused).
 *
 * @pre stack_buf completely filled with k_stack_fill_byte (0xEF) for all k_test_stack_size bytes
 * @pre free_bytes is initialised to k_zero_u32 before the call
 * @post free_bytes == k_test_stack_size (all bytes reported as unused)
 * @post err == k_rx_ok (scan succeeded without error)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Fill k_test_stack_size bytes with 0xEF
 * 2. Attach to mock TX_THREAD
 * 3. Call rx_stack_monitor_get_free_bytes()
 * 4. Verify return value is k_rx_ok
 * 5. Verify free_bytes == k_test_stack_size
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_all_unused(void)
{
  uint8_t   stack_buf[k_test_stack_size];
  TX_THREAD thread = {
    .tx_thread_name        = "MockThread",
    .tx_thread_id          = k_tx_invalid_id,
    .tx_thread_stack_start = stack_buf,
    .tx_thread_stack_size  = k_test_stack_size,
  };
  uint32_t free_bytes = k_zero_u32;

  /* Entire stack is 0xEF - thread never used any stack */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(stack_buf, (uint8_t)k_stack_fill_byte, k_test_stack_size);

  rx_err_t err = rx_stack_monitor_get_free_bytes(&thread, &free_bytes);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_test_stack_size, free_bytes);
}

/**
 * @brief Verify high-water mark when part of the stack has been used
 *
 * @details
 * Simulates a thread that has used k_test_used_bytes of stack.  The low
 * (k_test_stack_size - k_test_used_bytes) bytes remain 0xEF; the top
 * k_test_used_bytes bytes are 0x00 (simulating written data).
 *
 * Expected free_bytes == k_test_stack_size - k_test_used_bytes.
 *
 * @pre Lower (k_test_stack_size - k_test_used_bytes) bytes are k_stack_fill_byte (0xEF)
 * @pre Upper k_test_used_bytes are k_stack_fill_absent_byte (0x00, simulating written data)
 * @post free_bytes == k_test_stack_size - k_test_used_bytes
 * @post err == k_rx_ok (scan succeeded; partial fill pattern detected correctly)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Fill entire buffer with 0xEF
 * 2. Overwrite last k_test_used_bytes with 0x00
 * 3. Attach to mock TX_THREAD
 * 4. Call rx_stack_monitor_get_free_bytes()
 * 5. Verify return value is k_rx_ok
 * 6. Verify free_bytes == k_test_stack_size - k_test_used_bytes
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_partial_usage(void)
{
  uint8_t   stack_buf[k_test_stack_size];
  TX_THREAD thread = {
    .tx_thread_name        = "MockThread",
    .tx_thread_id          = k_tx_invalid_id,
    .tx_thread_stack_start = stack_buf,
    .tx_thread_stack_size  = k_test_stack_size,
  };
  uint32_t free_bytes = k_zero_u32;

  /* Fill entire stack with sentinel, then "use" top k_test_used_bytes */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(stack_buf, (uint8_t)k_stack_fill_byte, k_test_stack_size);
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&stack_buf[k_test_stack_size - k_test_used_bytes],
         (uint8_t)k_stack_fill_absent_byte,
         k_test_used_bytes);

  rx_err_t err = rx_stack_monitor_get_free_bytes(&thread, &free_bytes);

  const uint32_t expected_free = k_test_stack_size - k_test_used_bytes;

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(expected_free, free_bytes);
}

/**
 * @brief Verify that NULL tx_thread_stack_start returns k_rx_err_null_ptr
 *
 * @details
 * Passes a TX_THREAD whose tx_thread_stack_start is TX_NULL but has a valid
 * non-zero tx_thread_stack_size.  The function must detect the NULL stack
 * base pointer before attempting to read any stack bytes, and must return
 * k_rx_err_null_ptr without modifying free_bytes.
 *
 * @pre TX_THREAD has tx_thread_stack_start == TX_NULL and tx_thread_stack_size == k_test_stack_size
 * @pre free_bytes is initialised to k_zero_u32 before the call
 * @post Return value is k_rx_err_null_ptr
 * @post free_bytes remains k_zero_u32 (not written on error)
 *
 * @note Not thread-safe; Unity executes tests serially in a single-threaded context.
 *
 * @test
 * 1. Construct a mock TX_THREAD with tx_thread_stack_start = TX_NULL
 * 2. Set tx_thread_stack_size to k_test_stack_size (non-zero)
 * 3. Call rx_stack_monitor_get_free_bytes(&thread, &free_bytes)
 * 4. Verify return value is k_rx_err_null_ptr
 * 5. Verify free_bytes is still k_zero_u32
 *
 * @see rx_stack_monitor_get_free_bytes()
 * @since Version 1.0.0
 */
static void test_get_free_bytes_null_stack_start(void)
{
  TX_THREAD thread = {
    .tx_thread_name        = "NullStackThread",
    .tx_thread_id          = k_tx_invalid_id,
    .tx_thread_stack_start = TX_NULL,
    .tx_thread_stack_size  = k_test_stack_size,
  };
  uint32_t free_bytes = k_zero_u32;

  rx_err_t err = rx_stack_monitor_get_free_bytes(&thread, &free_bytes);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  /* Postcondition: free_bytes must not have been written on error */
  TEST_ASSERT_EQUAL(k_zero_u32, free_bytes);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Main test entry point - runs all stack monitor tests
 *
 * @details
 * Initialises the Unity test framework, runs all test cases, and returns
 * the Unity exit code (0 = all pass, non-zero = failures detected).
 *
 * @return int Unity result code (0 on success)
 *
 * @pre Unity test framework available (FetchContent from CMakeLists.txt)
 * @pre Test environment and hardware mocks initialised (mock tx_api.h in include path)
 * @post Test results printed to stdout
 * @post Non-zero exit code if any test fails
 *
 * @note Not thread-safe; the test harness and these tests must be executed serially
 *       in a single-threaded context.
 *
 * @see UNITY_BEGIN() Start Unity harness
 * @see UNITY_END() Finalize and return exit code
 * @see RUN_TEST() Execute individual test
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* rx_stack_monitor_init() tests */
  RUN_TEST(test_init_succeeds);
  RUN_TEST(test_init_idempotent);

  /* rx_stack_monitor_get_free_bytes() tests */
  RUN_TEST(test_get_free_bytes_null_thread);
  RUN_TEST(test_get_free_bytes_null_stack_start);
  RUN_TEST(test_get_free_bytes_null_output);
  RUN_TEST(test_get_free_bytes_pattern_absent);
  RUN_TEST(test_get_free_bytes_all_unused);
  RUN_TEST(test_get_free_bytes_partial_usage);

  return UNITY_END();
}
