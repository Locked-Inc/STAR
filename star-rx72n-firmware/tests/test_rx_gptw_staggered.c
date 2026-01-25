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
 * Test Fixtures
 * =============================================================================
 */

void setUp(void)
{
    mock_gptw_reset();
}

void tearDown(void)
{
}

/* =============================================================================
 * Tests
 * =============================================================================
 */

void test_staggered_init_success(void)
{
    rx_gptw_config_t config = {
        .frequency_hz = 20000,
        .wave_mode = k_gptw_wave_tri_pwm3,
        .invert_polarity = false
    };

    /* Call the new API */
    rx_err_t err = rx_gptw_init_all_staggered(&config);

    /* Verify it returns OK */
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    /* Verify all 4 channels are initialized and running */
    /* This confirms the API interacts with critical system state correctly */
    for (int i = 0; i < 4; i++) {
        rx_gptw_channel_t ch = (rx_gptw_channel_t)i;
        TEST_ASSERT_TRUE_MESSAGE(mock_gptw_is_initialized(ch), "Channel not initialized");
        TEST_ASSERT_TRUE_MESSAGE(mock_gptw_is_running(ch), "Channel not running");
    }
}

void test_staggered_init_null_config_fails(void)
{
    rx_err_t err = rx_gptw_init_all_staggered(NULL);
    TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_staggered_init_success);
  RUN_TEST(test_staggered_init_null_config_fails);

  return UNITY_END();
}
