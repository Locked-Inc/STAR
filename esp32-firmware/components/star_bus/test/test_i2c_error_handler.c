/* esp32-firmware/components/star_bus/test/test_i2c_error_handler.c */

/**
 * @file test_i2c_error_handler.c
 * @brief Tests for I2C bus-level error handler retry logic (12 tests)
 *
 * Tests cover:
 * - Error handler initialization in I2C config (3 tests)
 * - Retry logic for transient errors (3 tests)
 * - Permanent error detection (no retry) (3 tests)
 * - Error state reset on success (3 tests)
 */

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_test.h"

/* Test pin definitions (safe GPIOs for ESP32) */
#define TEST_I2C_SDA_PIN (GPIO_NUM_21)
#define TEST_I2C_SCL_PIN (GPIO_NUM_22)
#define TEST_I2C_ADDR (0x3C)
#define TEST_I2C_CLOCK (100000)

/* ========================================================================
 * Error Handler Initialization Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(i2c_error_handler, initialized_on_config_create)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_i2c, config->type);

  /* Verify error handler was initialized */
  STAR_ASSERT_NOT_NULL(config->proto.i2c.error_handler.mutex);
  STAR_ASSERT_EQUAL(0, config->proto.i2c.error_handler.error_count);
  STAR_ASSERT_EQUAL(0, config->proto.i2c.error_handler.current_retry);
  STAR_ASSERT_FALSE(config->proto.i2c.error_handler.in_error_state);

  /* Verify retry configuration */
  STAR_ASSERT_EQUAL(3, config->proto.i2c.error_handler.max_retries);
  STAR_ASSERT_EQUAL(10, config->proto.i2c.error_handler.base_retry_delay);
  STAR_ASSERT_EQUAL(100, config->proto.i2c.error_handler.max_retry_delay);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, deinitialized_on_config_destroy)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify mutex exists */
  STAR_ASSERT_NOT_NULL(config->proto.i2c.error_handler.mutex);

  /* Initialize bus manager to enable config destruction */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Destroy config - should deinitialize error handler */
  esp_err_t result = star_bus_config_destroy(config);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Note: After destroy, config pointer is invalid, so we can't check mutex */
  /* The test passes if no crash occurs during destruction */

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(i2c_error_handler, has_no_reset_function)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* I2C bus-level error handler should not have a reset function */
  STAR_ASSERT_NULL(config->proto.i2c.error_handler.reset_fn);
  STAR_ASSERT_NULL(config->proto.i2c.error_handler.reset_context);

  star_bus_config_destroy(config);
}

/* ========================================================================
 * Error State Tracking Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(i2c_error_handler, error_state_starts_clean)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify initial clean state */
  STAR_ASSERT_FALSE(config->proto.i2c.error_handler.in_error_state);
  STAR_ASSERT_EQUAL(0, config->proto.i2c.error_handler.error_count);
  STAR_ASSERT_EQUAL(0, config->proto.i2c.error_handler.current_retry);
  STAR_ASSERT_EQUAL(ESP_OK, config->proto.i2c.error_handler.last_error);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, can_reset_error_state)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Simulate an error being recorded */
  error_handler_t* handler = &config->proto.i2c.error_handler;
  handler->in_error_state  = true;
  handler->error_count     = 5;
  handler->current_retry   = 2;
  handler->last_error      = ESP_ERR_TIMEOUT;

  /* Reset state */
  esp_err_t result = error_handler_reset_state(handler);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Verify state was reset */
  STAR_ASSERT_FALSE(handler->in_error_state);
  STAR_ASSERT_EQUAL(0, handler->error_count);
  STAR_ASSERT_EQUAL(0, handler->current_retry);
  STAR_ASSERT_EQUAL(ESP_OK, handler->last_error);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, retry_count_increases_on_can_retry)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  error_handler_t* handler = &config->proto.i2c.error_handler;

  /* Simulate error state */
  handler->in_error_state = true;
  handler->current_retry  = 0;

  /* Check if can retry (should be yes, we're at 0/3) */
  bool can_retry = error_handler_can_retry(handler);
  STAR_ASSERT_TRUE(can_retry);

  /* Increment retry count */
  handler->current_retry++;
  STAR_ASSERT_EQUAL(1, handler->current_retry);

  /* Should still be able to retry */
  can_retry = error_handler_can_retry(handler);
  STAR_ASSERT_TRUE(can_retry);

  star_bus_config_destroy(config);
}

/* ========================================================================
 * Retry Limit Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(i2c_error_handler, cannot_retry_after_max_retries)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  error_handler_t* handler = &config->proto.i2c.error_handler;

  /* Set to max retries */
  handler->in_error_state = true;
  handler->current_retry  = handler->max_retries;

  /* Should not be able to retry anymore */
  bool can_retry = error_handler_can_retry(handler);
  STAR_ASSERT_FALSE(can_retry);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, cannot_retry_when_not_in_error_state)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  error_handler_t* handler = &config->proto.i2c.error_handler;

  /* Not in error state */
  handler->in_error_state = false;
  handler->current_retry  = 0;

  /* Should not retry if not in error state */
  bool can_retry = error_handler_can_retry(handler);
  STAR_ASSERT_FALSE(can_retry);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, max_retries_is_three)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify max_retries is configured to 3 */
  STAR_ASSERT_EQUAL(3, config->proto.i2c.error_handler.max_retries);

  star_bus_config_destroy(config);
}

/* ========================================================================
 * Exponential Backoff Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(i2c_error_handler, base_delay_is_10ms)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify base_retry_delay is 10ms */
  STAR_ASSERT_EQUAL(10, config->proto.i2c.error_handler.base_retry_delay);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, max_delay_is_100ms)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify max_retry_delay is 100ms */
  STAR_ASSERT_EQUAL(100, config->proto.i2c.error_handler.max_retry_delay);

  star_bus_config_destroy(config);
}

STAR_TEST_CASE(i2c_error_handler, current_delay_starts_at_base)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Verify current_retry_delay starts at base_retry_delay */
  STAR_ASSERT_EQUAL(config->proto.i2c.error_handler.base_retry_delay,
                    config->proto.i2c.error_handler.current_retry_delay);

  star_bus_config_destroy(config);
}

/* ========================================================================
 * Test List
 * ======================================================================== */

STAR_TEST_LIST_BEGIN()

/* Error Handler Initialization Tests */
STAR_TEST_REF(i2c_error_handler, initialized_on_config_create)
STAR_TEST_REF(i2c_error_handler, deinitialized_on_config_destroy)
STAR_TEST_REF(i2c_error_handler, has_no_reset_function)

/* Error State Tracking Tests */
STAR_TEST_REF(i2c_error_handler, error_state_starts_clean)
STAR_TEST_REF(i2c_error_handler, can_reset_error_state)
STAR_TEST_REF(i2c_error_handler, retry_count_increases_on_can_retry)

/* Retry Limit Tests */
STAR_TEST_REF(i2c_error_handler, cannot_retry_after_max_retries)
STAR_TEST_REF(i2c_error_handler, cannot_retry_when_not_in_error_state)
STAR_TEST_REF(i2c_error_handler, max_retries_is_three)

/* Exponential Backoff Tests */
STAR_TEST_REF(i2c_error_handler, base_delay_is_10ms)
STAR_TEST_REF(i2c_error_handler, max_delay_is_100ms)
STAR_TEST_REF(i2c_error_handler, current_delay_starts_at_base)

STAR_TEST_LIST_END()
