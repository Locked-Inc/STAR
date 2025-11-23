/* Mock interface tests for dependency injection verification */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>
#include <stdbool.h>

#include "star_core.h"
#include "star_bus_manager.h"
#include "star_bus_config.h"

/* Mock error interface implementation */
typedef struct {
  int record_error_calls;
  int can_retry_calls;
  int reset_state_calls;
  esp_err_t last_error_code;
  const char *last_error_desc;
  bool should_retry;
} mock_error_ctx_t;

static esp_err_t priv_mock_record_error(void *ctx, esp_err_t err, const char *desc,
                                    const char *file, int line, const char *func)
{
  mock_error_ctx_t *mock = (mock_error_ctx_t *)ctx;
  mock->record_error_calls++;
  mock->last_error_code = err;
  mock->last_error_desc = desc;
  return err;
}

static bool priv_mock_can_retry(void *ctx)
{
  mock_error_ctx_t *mock = (mock_error_ctx_t *)ctx;
  mock->can_retry_calls++;
  return mock->should_retry;
}

static esp_err_t priv_mock_reset_state(void *ctx)
{
  mock_error_ctx_t *mock = (mock_error_ctx_t *)ctx;
  mock->reset_state_calls++;
  mock->should_retry = false;
  return ESP_OK;
}

/* Mock pin interface implementation */
typedef struct {
  int register_calls;
  int unregister_calls;
  int validate_calls;
  gpio_num_t last_registered_pin;
  const char *last_registered_desc;
  bool last_registered_shared;
  bool should_conflict;
} mock_pin_ctx_t;

static esp_err_t priv_mock_register_pin(void *ctx, gpio_num_t pin, const char *desc, bool shared)
{
  mock_pin_ctx_t *mock = (mock_pin_ctx_t *)ctx;
  mock->register_calls++;
  mock->last_registered_pin = pin;
  mock->last_registered_desc = desc;
  mock->last_registered_shared = shared;
  return mock->should_conflict ? ESP_ERR_INVALID_STATE : ESP_OK;
}

static esp_err_t priv_mock_unregister_pin(void *ctx, gpio_num_t pin, const char *desc)
{
  mock_pin_ctx_t *mock = (mock_pin_ctx_t *)ctx;
  mock->unregister_calls++;
  return ESP_OK;
}

static esp_err_t priv_mock_validate_pins(void *ctx)
{
  mock_pin_ctx_t *mock = (mock_pin_ctx_t *)ctx;
  mock->validate_calls++;
  return mock->should_conflict ? ESP_ERR_INVALID_STATE : ESP_OK;
}

void setUp(void) {}
void tearDown(void) {}

/* --- Mock Error Interface Tests --- */

void test_mock_error_interface_creation(void)
{
  mock_error_ctx_t mock_ctx = {0};
  star_error_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  TEST_ASSERT_TRUE(star_error_interface_is_valid(&mock_iface));
}

void test_mock_error_tracks_record_calls(void)
{
  mock_error_ctx_t mock_ctx = {0};
  star_error_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  mock_iface.record_error(mock_iface.ctx, ESP_ERR_TIMEOUT, "Test error 1", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(1, mock_ctx.record_error_calls);
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, mock_ctx.last_error_code);

  mock_iface.record_error(mock_iface.ctx, ESP_FAIL, "Test error 2", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(2, mock_ctx.record_error_calls);
  TEST_ASSERT_EQUAL(ESP_FAIL, mock_ctx.last_error_code);
}

void test_mock_error_tracks_retry_calls(void)
{
  mock_error_ctx_t mock_ctx = {.should_retry = true};
  star_error_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  bool result = mock_iface.can_retry(mock_iface.ctx);
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_EQUAL(1, mock_ctx.can_retry_calls);

  mock_ctx.should_retry = false;
  result = mock_iface.can_retry(mock_iface.ctx);
  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_EQUAL(2, mock_ctx.can_retry_calls);
}

void test_mock_error_tracks_reset_calls(void)
{
  mock_error_ctx_t mock_ctx = {.should_retry = true};
  star_error_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  TEST_ASSERT_TRUE(mock_ctx.should_retry);
  mock_iface.reset_state(mock_iface.ctx);
  TEST_ASSERT_EQUAL(1, mock_ctx.reset_state_calls);
  TEST_ASSERT_FALSE(mock_ctx.should_retry);
}

/* --- Mock Pin Interface Tests --- */

void test_mock_pin_interface_creation(void)
{
  mock_pin_ctx_t mock_ctx = {0};
  star_pin_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  TEST_ASSERT_TRUE(star_pin_interface_is_valid(&mock_iface));
}

void test_mock_pin_tracks_register_calls(void)
{
  mock_pin_ctx_t mock_ctx = {0};
  star_pin_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  mock_iface.register_pin(mock_iface.ctx, GPIO_NUM_4, "SDA", true);
  TEST_ASSERT_EQUAL(1, mock_ctx.register_calls);
  TEST_ASSERT_EQUAL(GPIO_NUM_4, mock_ctx.last_registered_pin);
  TEST_ASSERT_EQUAL_STRING("SDA", mock_ctx.last_registered_desc);
  TEST_ASSERT_TRUE(mock_ctx.last_registered_shared);

  mock_iface.register_pin(mock_iface.ctx, GPIO_NUM_5, "SCL", false);
  TEST_ASSERT_EQUAL(2, mock_ctx.register_calls);
  TEST_ASSERT_EQUAL(GPIO_NUM_5, mock_ctx.last_registered_pin);
  TEST_ASSERT_FALSE(mock_ctx.last_registered_shared);
}

void test_mock_pin_simulates_conflicts(void)
{
  mock_pin_ctx_t mock_ctx = {.should_conflict = true};
  star_pin_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  esp_err_t result = mock_iface.register_pin(mock_iface.ctx, GPIO_NUM_10, "Test", false);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);

  result = mock_iface.validate(mock_iface.ctx);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

void test_mock_pin_tracks_unregister_calls(void)
{
  mock_pin_ctx_t mock_ctx = {0};
  star_pin_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  mock_iface.unregister_pin(mock_iface.ctx, GPIO_NUM_15, "Test pin");
  TEST_ASSERT_EQUAL(1, mock_ctx.unregister_calls);

  mock_iface.unregister_pin(mock_iface.ctx, GPIO_NUM_16, "Another pin");
  TEST_ASSERT_EQUAL(2, mock_ctx.unregister_calls);
}

/* --- Bus Manager with Mock Interfaces --- */

void test_bus_manager_with_mock_error_interface(void)
{
  mock_error_ctx_t mock_ctx = {0};
  star_error_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "MockTest", &mock_iface, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Verify mock was injected correctly */
  TEST_ASSERT_EQUAL_PTR(&mock_iface, manager.error_iface);

  star_bus_manager_deinit(&manager);
}

void test_bus_manager_with_mock_pin_interface(void)
{
  mock_pin_ctx_t mock_ctx = {0};
  star_pin_interface_t mock_iface = {
    .ctx = &mock_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "MockTest", NULL, &mock_iface);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Verify mock was injected correctly */
  TEST_ASSERT_EQUAL_PTR(&mock_iface, manager.pin_iface);

  star_bus_manager_deinit(&manager);
}

void test_bus_manager_with_both_mock_interfaces(void)
{
  mock_error_ctx_t error_ctx = {.should_retry = true};
  star_error_interface_t error_iface = {
    .ctx = &error_ctx,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  mock_pin_ctx_t pin_ctx = {0};
  star_pin_interface_t pin_iface = {
    .ctx = &pin_ctx,
    .register_pin = priv_mock_register_pin,
    .unregister_pin = priv_mock_unregister_pin,
    .validate = priv_mock_validate_pins
  };

  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "FullMock", &error_iface, &pin_iface);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  TEST_ASSERT_EQUAL_PTR(&error_iface, manager.error_iface);
  TEST_ASSERT_EQUAL_PTR(&pin_iface, manager.pin_iface);

  star_bus_manager_deinit(&manager);
}

void test_mock_interfaces_independence(void)
{
  /* Create two independent mocks */
  mock_error_ctx_t ctx1 = {0};
  mock_error_ctx_t ctx2 = {0};

  star_error_interface_t iface1 = {
    .ctx = &ctx1,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  star_error_interface_t iface2 = {
    .ctx = &ctx2,
    .record_error = priv_mock_record_error,
    .can_retry = priv_mock_can_retry,
    .reset_state = priv_mock_reset_state
  };

  /* Operations on one don't affect the other */
  iface1.record_error(iface1.ctx, ESP_FAIL, "Error 1", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(1, ctx1.record_error_calls);
  TEST_ASSERT_EQUAL(0, ctx2.record_error_calls);

  iface2.record_error(iface2.ctx, ESP_ERR_TIMEOUT, "Error 2", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(1, ctx1.record_error_calls);
  TEST_ASSERT_EQUAL(1, ctx2.record_error_calls);
  TEST_ASSERT_EQUAL(ESP_FAIL, ctx1.last_error_code);
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, ctx2.last_error_code);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Mock Error Interface Tests */
  RUN_TEST(test_mock_error_interface_creation);
  RUN_TEST(test_mock_error_tracks_record_calls);
  RUN_TEST(test_mock_error_tracks_retry_calls);
  RUN_TEST(test_mock_error_tracks_reset_calls);

  /* Mock Pin Interface Tests */
  RUN_TEST(test_mock_pin_interface_creation);
  RUN_TEST(test_mock_pin_tracks_register_calls);
  RUN_TEST(test_mock_pin_simulates_conflicts);
  RUN_TEST(test_mock_pin_tracks_unregister_calls);

  /* Bus Manager with Mocks */
  RUN_TEST(test_bus_manager_with_mock_error_interface);
  RUN_TEST(test_bus_manager_with_mock_pin_interface);
  RUN_TEST(test_bus_manager_with_both_mock_interfaces);
  RUN_TEST(test_mock_interfaces_independence);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
