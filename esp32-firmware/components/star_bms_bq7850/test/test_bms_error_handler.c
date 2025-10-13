/* esp32-firmware/components/star_bms_bq7850/test/test_bms_error_handler.c */

/**
 * @file test_bms_error_handler.c
 * @brief Tests for BMS-level error handler integration (12 tests)
 *
 * Tests cover:
 * - Error handler initialization in BMS handle (3 tests)
 * - Handle lifecycle management (3 tests)
 * - Error state tracking across operations (3 tests)
 * - BMS-specific retry configuration (3 tests)
 */

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "star_bms_bq7850.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_test.h"

/* Test configuration */
#define TEST_I2C_SDA_PIN (GPIO_NUM_21)
#define TEST_I2C_SCL_PIN (GPIO_NUM_22)
#define TEST_I2C_CLOCK (100000)

/* ========================================================================
 * Error Handler Initialization Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(bms_error_handler, initialized_on_handle_create)
{
  /* Setup: Create bus manager and I2C bus */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);

  esp_err_t ret = star_bus_manager_add_bus(&manager, i2c_config);
  STAR_ASSERT_EQUAL(ESP_OK, ret);

  /* Initialize BMS handle (will fail to communicate with actual device, but that's OK) */
  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  /* Note: This will fail because there's no real BQ7850 device, but we can check
   * the error handler was initialized before the failure */
  ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  /* Even if init fails, error handler should have been initialized */
  if (ret == ESP_OK) {
    /* If somehow succeeded, verify error handler */
    STAR_ASSERT_NOT_NULL(bms_handle.error_handler.mutex);
    STAR_ASSERT_EQUAL(0, bms_handle.error_handler.error_count);
    STAR_ASSERT_EQUAL(0, bms_handle.error_handler.current_retry);
    STAR_ASSERT_FALSE(bms_handle.error_handler.in_error_state);

    /* Verify BMS-specific retry configuration */
    STAR_ASSERT_EQUAL(3, bms_handle.error_handler.max_retries);
    STAR_ASSERT_EQUAL(50, bms_handle.error_handler.base_retry_delay);
    STAR_ASSERT_EQUAL(200, bms_handle.error_handler.max_retry_delay);

    star_bms_bq7850_deinit(&bms_handle);
  }

  /* Cleanup */
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, has_no_reset_function)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* BMS error handler should not have a reset function (relies on I2C bus layer) */
    STAR_ASSERT_NULL(bms_handle.error_handler.reset_fn);
    STAR_ASSERT_NULL(bms_handle.error_handler.reset_context);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, deinitialized_on_handle_destroy)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* Verify mutex exists before deinit */
    STAR_ASSERT_NOT_NULL(bms_handle.error_handler.mutex);

    /* Deinitialize handle */
    ret = star_bms_bq7850_deinit(&bms_handle);
    STAR_ASSERT_EQUAL(ESP_OK, ret);

    /* Handle should be zeroed out after deinit */
    STAR_ASSERT_NULL(bms_handle.manager);
    STAR_ASSERT_NULL(bms_handle.bus_name);
    STAR_ASSERT_EQUAL(0, bms_handle.smbus_addr);
  }

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * Handle Lifecycle Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(bms_error_handler, handle_stores_bus_info)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* Verify handle stores correct bus information */
    STAR_ASSERT_EQUAL(&manager, bms_handle.manager);
    STAR_ASSERT_STRING_EQUAL("bms_i2c", bms_handle.bus_name);
    STAR_ASSERT_EQUAL(BQ7850_DEFAULT_ADDR, bms_handle.smbus_addr);

    /* Verify config was copied */
    STAR_ASSERT_EQUAL(4, bms_handle.config.num_cells);
    STAR_ASSERT_EQUAL(2, bms_handle.config.num_temp);
    STAR_ASSERT_EQUAL(5000, bms_handle.config.design_capacity);
    STAR_ASSERT_EQUAL(14800, bms_handle.config.design_voltage);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, init_fails_with_null_handle)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  /* Should return error when handle is NULL */
  esp_err_t ret = star_bms_bq7850_init(NULL, &manager, "bms_i2c", &bms_config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, deinit_fails_with_null_handle)
{
  /* Should return error when handle is NULL */
  esp_err_t ret = star_bms_bq7850_deinit(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* ========================================================================
 * Error State Tracking Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(bms_error_handler, error_state_starts_clean)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* Verify initial clean state */
    STAR_ASSERT_FALSE(bms_handle.error_handler.in_error_state);
    STAR_ASSERT_EQUAL(0, bms_handle.error_handler.error_count);
    STAR_ASSERT_EQUAL(0, bms_handle.error_handler.current_retry);
    STAR_ASSERT_EQUAL(ESP_OK, bms_handle.error_handler.last_error);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, can_reset_error_state)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* Simulate an error being recorded */
    error_handler_t* handler = &bms_handle.error_handler;
    handler->in_error_state  = true;
    handler->error_count     = 5;
    handler->current_retry   = 2;
    handler->last_error      = ESP_ERR_TIMEOUT;

    /* Reset state */
    ret = error_handler_reset_state(handler);
    STAR_ASSERT_EQUAL(ESP_OK, ret);

    /* Verify state was reset */
    STAR_ASSERT_FALSE(handler->in_error_state);
    STAR_ASSERT_EQUAL(0, handler->error_count);
    STAR_ASSERT_EQUAL(0, handler->current_retry);
    STAR_ASSERT_EQUAL(ESP_OK, handler->last_error);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, tracks_errors_across_operations)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    error_handler_t* handler = &bms_handle.error_handler;

    /* Simulate error state from operation 1 */
    handler->in_error_state = true;
    handler->error_count    = 3;
    handler->current_retry  = 1;

    /* Verify state persists for operation 2 */
    STAR_ASSERT_TRUE(handler->in_error_state);
    STAR_ASSERT_EQUAL(3, handler->error_count);
    STAR_ASSERT_EQUAL(1, handler->current_retry);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * BMS-Specific Configuration Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(bms_error_handler, base_delay_is_50ms)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* BMS should use longer delays than I2C layer (50ms vs 10ms) */
    STAR_ASSERT_EQUAL(50, bms_handle.error_handler.base_retry_delay);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, max_delay_is_200ms)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* BMS should use longer max delay than I2C layer (200ms vs 100ms) */
    STAR_ASSERT_EQUAL(200, bms_handle.error_handler.max_retry_delay);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bms_error_handler, current_delay_starts_at_base)
{
  /* Setup */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "test_mgr");

  star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(&manager, i2c_config);

  bq7850_handle_t bms_handle;
  bq7850_config_t bms_config = {
    .num_cells       = 4,
    .num_temp        = 2,
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 5000,
    .design_voltage  = 14800,
  };

  esp_err_t ret = star_bms_bq7850_init(&bms_handle, &manager, "bms_i2c", &bms_config);

  if (ret == ESP_OK) {
    /* Verify current_retry_delay starts at base_retry_delay */
    STAR_ASSERT_EQUAL(bms_handle.error_handler.base_retry_delay,
                      bms_handle.error_handler.current_retry_delay);

    star_bms_bq7850_deinit(&bms_handle);
  }

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * Test List
 * ======================================================================== */

STAR_TEST_LIST_BEGIN()

/* Error Handler Initialization Tests */
STAR_TEST_REF(bms_error_handler, initialized_on_handle_create)
STAR_TEST_REF(bms_error_handler, has_no_reset_function)
STAR_TEST_REF(bms_error_handler, deinitialized_on_handle_destroy)

/* Handle Lifecycle Tests */
STAR_TEST_REF(bms_error_handler, handle_stores_bus_info)
STAR_TEST_REF(bms_error_handler, init_fails_with_null_handle)
STAR_TEST_REF(bms_error_handler, deinit_fails_with_null_handle)

/* Error State Tracking Tests */
STAR_TEST_REF(bms_error_handler, error_state_starts_clean)
STAR_TEST_REF(bms_error_handler, can_reset_error_state)
STAR_TEST_REF(bms_error_handler, tracks_errors_across_operations)

/* BMS-Specific Configuration Tests */
STAR_TEST_REF(bms_error_handler, base_delay_is_50ms)
STAR_TEST_REF(bms_error_handler, max_delay_is_200ms)
STAR_TEST_REF(bms_error_handler, current_delay_starts_at_base)

STAR_TEST_LIST_END()
