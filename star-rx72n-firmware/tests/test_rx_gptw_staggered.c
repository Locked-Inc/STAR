/* tests/test_rx_gptw_staggered.c */

/**
 * @file test_rx_gptw_staggered.c
 * @brief Unit Tests for GPTW Staggered Initialization API
 * @details
 * Verifies the API contract of rx_gptw_init_all_staggered() against the mock.
 * Ensures the function correctly requests initialization and start for all
 * 4 channels.
 *
 * Note: The actual phase calculation logic is in rx_gptw.c (HAL) and is
 * NOT executed here (replaced by mock). This test validates the Interface
 * and global state changes.
 *
 * @date 2026-01-24
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"
#include "rx_gptw.h"
#include "mock_rx_gptw.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Test configuration constants */
typedef enum : uint32_t {
  k_staggered_test_freq_hz   = 20000, /**< Test PWM frequency in Hz */
  k_staggered_channel_count  = 4,     /**< Number of GPTW channels to test */
  k_staggered_start_channel  = 0,     /**< Starting channel index */
} staggered_test_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @brief Test setup function called before each test
 *
 * Resets mock GPTW state to ensure clean test isolation.
 */
void setUp(void)
{
  mock_gptw_reset();
}

/**
 * @brief Test teardown function called after each test
 *
 * Currently empty as mock_gptw_reset() in setUp handles cleanup.
 */
void tearDown(void)
{
}

/* =============================================================================
 * Tests
 * =============================================================================
 */

/**
 * @brief Test successful initialization of all 4 GPTW channels with staggering
 *
 * Verifies that rx_gptw_init_all_staggered() correctly initializes and starts
 * all 4 GPTW channels when given a valid configuration.
 */
void test_staggered_init_success(void)
{
  rx_gptw_config_t  config;
  rx_err_t          err;
  uint8_t           i;
  rx_gptw_channel_t ch;

  /* Initialize config structure */
  config.frequency_hz     = k_staggered_test_freq_hz;
  config.wave_mode        = k_gptw_wave_tri_pwm3;
  config.invert_polarity  = false;
  config.deadtime_ns      = 0;
  config.enable_complementary = false;

  /* Call the API under test */
  err = rx_gptw_init_all_staggered(&config);

  /* Verify it returns OK */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify all 4 channels are initialized and running */
  for (i = k_staggered_start_channel; i < k_staggered_channel_count; i++) {
    ch = (rx_gptw_channel_t)i;
    TEST_ASSERT_TRUE_MESSAGE(mock_gptw_is_initialized(ch), "Channel not initialized");
    TEST_ASSERT_TRUE_MESSAGE(mock_gptw_is_running(ch), "Channel not running");
  }
}

/**
 * @brief Test that NULL config pointer returns k_rx_err_null_ptr
 *
 * Verifies that rx_gptw_init_all_staggered() correctly rejects a NULL
 * configuration pointer and returns the appropriate error code.
 */
void test_staggered_init_null_config_fails(void)
{
  rx_err_t err;

  err = rx_gptw_init_all_staggered(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

/**
 * @brief Main entry point for GPTW staggered initialization tests
 *
 * Runs all test cases using the Unity test framework.
 *
 * @return Unity test result (0 = all passed, non-zero = failures)
 */
int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_staggered_init_success);
  RUN_TEST(test_staggered_init_null_config_fails);

  return UNITY_END();
}
