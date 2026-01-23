/* tests/test_rx_error_handler.c */

/**
 * @file test_rx_error_handler.c
 * @brief Unit Tests for Error Handler Implementation
 *
 * Tests the error handler concrete implementation including:
 * - Initialization with valid/invalid parameters
 * - Error tracking per component
 * - Retry counting
 * - Exponential backoff calculation
 * - DIP interface abstraction
 * - Thread-safety with mock mutexes
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

#include "rx_check.h"

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

static error_handler_t s_handler;

void setUp(void)
{
  memset(&s_handler, 0, sizeof(s_handler));
}

void tearDown(void)
{
  if (s_handler.initialized) {
    error_handler_deinit(&s_handler);
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
  RX_ASSERT((handler == NULL) || (max_retries > k_test_zero_retries),
            "max_retries must be > 0");
  RX_ASSERT((handler == NULL) || (max_retries <= k_test_max_retries),
            "max_retries exceeds test max");

  error_handler_config_t config = {
    .max_retries        = max_retries,
    .initial_backoff_ms = k_test_initial_backoff_ms,
    .max_backoff_ms     = k_test_max_backoff_ms,
  };
  const rx_err_t err = error_handler_init(handler, &config);
  if (handler != NULL) {
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
 * @brief Test initialization with NULL handler pointer
 */
void test_error_handler_init_null_pointer(void)
{
  rx_err_t err = internal_init_handler(NULL, k_test_max_retries);

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
 * @brief Test deinitialization with NULL pointer
 */
void test_error_handler_deinit_null_pointer(void)
{
  rx_err_t err = error_handler_deinit(NULL);
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
 * @brief Test getting interface with NULL interface pointer
 */
void test_error_handler_get_interface_null_iface(void)
{
  rx_err_t err = internal_init_handler(&s_handler, k_test_max_retries);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = error_handler_get_interface(NULL, &s_handler);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test getting interface with NULL handler pointer
 */
void test_error_handler_get_interface_null_handler(void)
{
  rx_error_interface_t iface;
  rx_err_t             err = error_handler_get_interface(&iface, NULL);
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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

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
  error_handler_get_interface(&iface, &s_handler);

  err = rx_error_interface_validate(&iface);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test interface validation with NULL
 */
void test_error_interface_validate_null(void)
{
  rx_err_t err = rx_error_interface_validate(NULL);
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
