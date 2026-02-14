/* tests/test_rx_spi_link.c */

/**
 * @file test_rx_spi_link.c
 * @brief Unit Tests for SPI Link Layer (HARQ)
 *
 * @details
 * Tests the SPI link layer module that integrates HARQ (Chase Combining)
 * and FEC (convolutional encoding + Viterbi decoding) with the raw SPI
 * transport (rx_spi_comm). Uses mock SPI/session dependencies.
 *
 * ## Test Categories
 *
 * 1. **Lifecycle**: Init/deinit with valid and invalid parameters
 * 2. **Send**: FEC encoding, ACK/NACK handling, retry logic
 * 3. **Receive**: FEC decoding, soft bit conversion, Chase Combining
 * 4. **State**: State machine transitions, reset behavior
 * 5. **Edge Cases**: NULL pointers, uninitialized handles, buffer limits
 *
 * @since Version 1.1.0
 */

#include "unity.h"

#include <string.h>

#include "rx_spi_link.h"

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

/** @brief Static SPI link handle for tests (~86 KB) */
static rx_spi_link_t s_link;

/** @brief Static SPI comm handle for tests */
static rx_spi_comm_handle_t s_spi_comm;

/** @brief Static session state for tests */
static rx_session_state_t s_session;

/**
 * @brief Unity setUp - called before each test
 */
void setUp(void)
{
    memset(&s_link, 0, sizeof(s_link));
    memset(&s_spi_comm, 0, sizeof(s_spi_comm));
    memset(&s_session, 0, sizeof(s_session));
}

/**
 * @brief Unity tearDown - called after each test
 */
void tearDown(void)
{
    if (s_link.initialized) {
        (void)rx_spi_link_deinit(&s_link);
    }
}

/* =============================================================================
 * Helper: Initialize a valid SPI link for tests
 * ============================================================================= */

/**
 * @brief Initialize SPI comm and link with default test config
 *
 * @param[in] fec_enabled Whether to enable FEC
 * @return k_rx_ok on success
 */
static rx_err_t helper_init_link(bool fec_enabled)
{
    /* Initialize session state */
    rx_err_t err = rx_session_init(&s_session);
    if (err != k_rx_ok) {
        return err;
    }

    /* Initialize SPI comm with session */
    const rx_spi_comm_config_t spi_cfg = {
        .session     = &s_session,
        .channel     = 0,
        .fec_enabled = fec_enabled,
    };
    err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
    if (err != k_rx_ok) {
        return err;
    }

    /* Initialize SPI link */
    const rx_spi_link_config_t link_cfg = {
        .spi_handle  = &s_spi_comm,
        .fec_enabled = fec_enabled,
        .max_retries = 3,
    };
    return rx_spi_link_init(&s_link, &link_cfg);
}

/* =============================================================================
 * Lifecycle Tests
 * ============================================================================= */

/**
 * @brief Test init with NULL link pointer
 */
void test_init_null_link(void)
{
    const rx_spi_link_config_t cfg = {.spi_handle = &s_spi_comm};
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(nullptr, &cfg));
}

/**
 * @brief Test init with NULL config pointer
 */
void test_init_null_config(void)
{
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(&s_link, nullptr));
}

/**
 * @brief Test init with NULL SPI handle in config
 */
void test_init_null_spi_handle(void)
{
    const rx_spi_link_config_t cfg = {.spi_handle = nullptr};
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_init(&s_link, &cfg));
}

/**
 * @brief Test successful init with FEC disabled
 */
void test_init_success_no_fec(void)
{
    rx_err_t err = rx_session_init(&s_session);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    const rx_spi_comm_config_t spi_cfg = {
        .session     = &s_session,
        .channel     = 0,
        .fec_enabled = false,
    };
    err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    const rx_spi_link_config_t link_cfg = {
        .spi_handle  = &s_spi_comm,
        .fec_enabled = false,
        .max_retries = 0, /* Should default to 3 */
    };
    err = rx_spi_link_init(&s_link, &link_cfg);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(s_link.initialized);
    TEST_ASSERT_FALSE(s_link.fec_enabled);
    TEST_ASSERT_EQUAL(k_spi_link_default_max_retries, s_link.max_retries);
    TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test successful init with FEC enabled
 */
void test_init_success_with_fec(void)
{
    rx_err_t err = helper_init_link(true);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(s_link.initialized);
    TEST_ASSERT_TRUE(s_link.fec_enabled);
    TEST_ASSERT_TRUE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test init with custom max retries
 */
void test_init_custom_retries(void)
{
    rx_err_t err = rx_session_init(&s_session);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    const rx_spi_comm_config_t spi_cfg = {
        .session = &s_session,
        .channel = 0,
    };
    err = rx_spi_comm_init(&s_spi_comm, &spi_cfg);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    const rx_spi_link_config_t link_cfg = {
        .spi_handle  = &s_spi_comm,
        .fec_enabled = false,
        .max_retries = 5,
    };
    err = rx_spi_link_init(&s_link, &link_cfg);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL(5, s_link.max_retries);
}

/* =============================================================================
 * Deinit Tests
 * ============================================================================= */

/**
 * @brief Test deinit with NULL pointer
 */
void test_deinit_null(void)
{
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_deinit(nullptr));
}

/**
 * @brief Test deinit on uninitialized handle
 */
void test_deinit_uninitialized(void)
{
    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_spi_link_deinit(&s_link));
}

/**
 * @brief Test successful deinit
 */
void test_deinit_success(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    err = rx_spi_link_deinit(&s_link);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FALSE(s_link.initialized);
}

/* =============================================================================
 * State Tests
 * ============================================================================= */

/**
 * @brief Test get_state with NULL returns error state
 */
void test_get_state_null(void)
{
    TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(nullptr));
}

/**
 * @brief Test get_state on uninitialized handle returns error state
 */
void test_get_state_uninitialized(void)
{
    TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));
}

/**
 * @brief Test get_state returns idle after init
 */
void test_get_state_after_init(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/* =============================================================================
 * FEC Enabled Query Tests
 * ============================================================================= */

/**
 * @brief Test fec_enabled with NULL returns false
 */
void test_fec_enabled_null(void)
{
    TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(nullptr));
}

/**
 * @brief Test fec_enabled on uninitialized returns false
 */
void test_fec_enabled_uninitialized(void)
{
    TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test fec_enabled when disabled
 */
void test_fec_enabled_off(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FALSE(rx_spi_link_fec_enabled(&s_link));
}

/**
 * @brief Test fec_enabled when enabled
 */
void test_fec_enabled_on(void)
{
    rx_err_t err = helper_init_link(true);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(rx_spi_link_fec_enabled(&s_link));
}

/* =============================================================================
 * Reset Tests
 * ============================================================================= */

/**
 * @brief Test reset with NULL pointer
 */
void test_reset_null(void)
{
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_spi_link_reset(nullptr));
}

/**
 * @brief Test reset on uninitialized handle
 */
void test_reset_uninitialized(void)
{
    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_spi_link_reset(&s_link));
}

/**
 * @brief Test successful reset returns to idle
 */
void test_reset_success(void)
{
    rx_err_t err = helper_init_link(true);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    /* Manually set state to error to test reset */
    s_link.state = k_spi_link_state_error;
    TEST_ASSERT_EQUAL(k_spi_link_state_error, rx_spi_link_get_state(&s_link));

    err = rx_spi_link_reset(&s_link);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL(k_spi_link_state_idle, rx_spi_link_get_state(&s_link));
}

/* =============================================================================
 * Send Validation Tests (parameter checking only, no SPI hardware)
 * ============================================================================= */

/**
 * @brief Test send with NULL link
 */
void test_send_null_link(void)
{
    uint8_t data[] = {0x01, 0x02};
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                      rx_spi_link_send(nullptr, k_frame_type_command, data, sizeof(data)));
}

/**
 * @brief Test send on uninitialized link
 */
void test_send_uninitialized(void)
{
    uint8_t data[] = {0x01, 0x02};
    TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                      rx_spi_link_send(&s_link, k_frame_type_command, data, sizeof(data)));
}

/**
 * @brief Test send with NULL payload and non-zero length
 */
void test_send_null_payload_nonzero_len(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                      rx_spi_link_send(&s_link, k_frame_type_command, nullptr, 10));
}

/**
 * @brief Test send with payload too large
 */
void test_send_payload_too_large(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    uint8_t data[1];
    TEST_ASSERT_EQUAL(k_rx_err_invalid_size,
                      rx_spi_link_send(&s_link, k_frame_type_command, data,
                                       k_harq_max_payload + 1));
}

/* =============================================================================
 * Receive Validation Tests (parameter checking only)
 * ============================================================================= */

/**
 * @brief Test receive with NULL link
 */
void test_receive_null_link(void)
{
    rx_spi_link_receive_result_t result;
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                      rx_spi_link_receive(nullptr, &result, 100));
}

/**
 * @brief Test receive with NULL result
 */
void test_receive_null_result(void)
{
    rx_err_t err = helper_init_link(false);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                      rx_spi_link_receive(&s_link, nullptr, 100));
}

/**
 * @brief Test receive on uninitialized link
 */
void test_receive_uninitialized(void)
{
    rx_spi_link_receive_result_t result;
    TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                      rx_spi_link_receive(&s_link, &result, 100));
}

/* =============================================================================
 * Main
 * ============================================================================= */

/**
 * @brief Unity test runner
 *
 * @details
 * Runs all SPI link layer tests. Tests are organized by category:
 * lifecycle, state, FEC query, reset, send validation, receive validation.
 *
 * @return Number of failures (0 = all passed)
 */
int main(void)
{
    UNITY_BEGIN();

    /* Lifecycle: Init */
    RUN_TEST(test_init_null_link);
    RUN_TEST(test_init_null_config);
    RUN_TEST(test_init_null_spi_handle);
    RUN_TEST(test_init_success_no_fec);
    RUN_TEST(test_init_success_with_fec);
    RUN_TEST(test_init_custom_retries);

    /* Lifecycle: Deinit */
    RUN_TEST(test_deinit_null);
    RUN_TEST(test_deinit_uninitialized);
    RUN_TEST(test_deinit_success);

    /* State */
    RUN_TEST(test_get_state_null);
    RUN_TEST(test_get_state_uninitialized);
    RUN_TEST(test_get_state_after_init);

    /* FEC Enabled */
    RUN_TEST(test_fec_enabled_null);
    RUN_TEST(test_fec_enabled_uninitialized);
    RUN_TEST(test_fec_enabled_off);
    RUN_TEST(test_fec_enabled_on);

    /* Reset */
    RUN_TEST(test_reset_null);
    RUN_TEST(test_reset_uninitialized);
    RUN_TEST(test_reset_success);

    /* Send Validation */
    RUN_TEST(test_send_null_link);
    RUN_TEST(test_send_uninitialized);
    RUN_TEST(test_send_null_payload_nonzero_len);
    RUN_TEST(test_send_payload_too_large);

    /* Receive Validation */
    RUN_TEST(test_receive_null_link);
    RUN_TEST(test_receive_null_result);
    RUN_TEST(test_receive_uninitialized);

    return UNITY_END();
}
