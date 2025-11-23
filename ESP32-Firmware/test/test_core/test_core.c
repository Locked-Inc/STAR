/* Test star_core interfaces with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_core.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Error Interface Tests --- */

void test_error_interface_struct_size(void)
{
  /* Verify the interface struct has expected size */
  TEST_ASSERT_TRUE(sizeof(star_error_interface_t) > 0);
}

void test_error_interface_null_ctx(void)
{
  star_error_interface_t iface = {0};
  TEST_ASSERT_NULL(iface.ctx);
  TEST_ASSERT_NULL(iface.record_error);
  TEST_ASSERT_NULL(iface.can_retry);
  TEST_ASSERT_NULL(iface.reset_state);
}

void test_error_interface_from_handler(void)
{
  error_handler_t handler;
  esp_err_t result = error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  TEST_ASSERT_NOT_NULL(iface.record_error);
  TEST_ASSERT_NOT_NULL(iface.can_retry);
  TEST_ASSERT_NOT_NULL(iface.reset_state);
  TEST_ASSERT_EQUAL_PTR(&handler, iface.ctx);

  error_handler_deinit(&handler);
}

void test_error_interface_record_error(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  /* Record an error through the interface */
  esp_err_t result = iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test error", __FILE__, __LINE__, __func__);
  TEST_ASSERT_NOT_EQUAL(ESP_OK, result); /* Returns original error or retry status */

  error_handler_deinit(&handler);
}

void test_error_interface_can_retry(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  /* Initially no retry needed */
  bool can_retry = iface.can_retry(iface.ctx);
  TEST_ASSERT_FALSE(can_retry);

  /* After recording an error, retry should be possible */
  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);
  can_retry = iface.can_retry(iface.ctx);
  TEST_ASSERT_TRUE(can_retry);

  error_handler_deinit(&handler);
}

void test_error_interface_reset_state(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  /* Record an error */
  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);
  TEST_ASSERT_TRUE(iface.can_retry(iface.ctx));

  /* Reset state */
  esp_err_t result = iface.reset_state(iface.ctx);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* After reset, should not be in error state */
  TEST_ASSERT_FALSE(iface.can_retry(iface.ctx));

  error_handler_deinit(&handler);
}

void test_error_interface_get_null(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  /* Should handle NULL interface pointer gracefully */
  error_handler_get_interface(NULL, &handler);
  /* No crash = pass */

  error_handler_deinit(&handler);
}

/* --- Pin Interface Tests --- */

void test_pin_interface_struct_size(void)
{
  TEST_ASSERT_TRUE(sizeof(star_pin_interface_t) > 0);
}

void test_pin_interface_null_ctx(void)
{
  star_pin_interface_t iface = {0};
  TEST_ASSERT_NULL(iface.ctx);
  TEST_ASSERT_NULL(iface.register_pin);
  TEST_ASSERT_NULL(iface.unregister_pin);
  TEST_ASSERT_NULL(iface.validate);
}

void test_pin_interface_from_validator(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  TEST_ASSERT_NOT_NULL(iface.register_pin);
  TEST_ASSERT_NOT_NULL(iface.unregister_pin);
  TEST_ASSERT_NOT_NULL(iface.validate);
  /* Global validator uses NULL ctx */
  TEST_ASSERT_NULL(iface.ctx);
}

void test_pin_interface_register_pin(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register a pin through the interface */
  esp_err_t result = iface.register_pin(iface.ctx, 4, "Test pin", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Clean up */
  iface.unregister_pin(iface.ctx, 4, "Test pin");
  star_free_pin_validator();
}

void test_pin_interface_unregister_pin(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register then unregister */
  iface.register_pin(iface.ctx, 5, "Test pin 5", false);
  esp_err_t result = iface.unregister_pin(iface.ctx, 5, "Test pin 5");
  TEST_ASSERT_EQUAL(ESP_OK, result);

  star_free_pin_validator();
}

void test_pin_interface_validate(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register some pins */
  iface.register_pin(iface.ctx, 6, "Pin A", true);
  iface.register_pin(iface.ctx, 7, "Pin B", true);

  /* Validate should pass */
  esp_err_t result = iface.validate(iface.ctx);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Clean up */
  iface.unregister_pin(iface.ctx, 6, "Pin A");
  iface.unregister_pin(iface.ctx, 7, "Pin B");
  star_free_pin_validator();
}

void test_pin_interface_get_null(void)
{
  /* Should handle NULL interface pointer gracefully */
  pin_validator_get_interface(NULL);
  /* No crash = pass */
}

void test_pin_interface_conflict_detection(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register same pin twice with non-shareable */
  esp_err_t result = iface.register_pin(iface.ctx, 8, "First user", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  result = iface.register_pin(iface.ctx, 8, "Second user", false);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);

  /* Clean up */
  iface.unregister_pin(iface.ctx, 8, "First user");
  star_free_pin_validator();
}

void test_pin_interface_shared_pins(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register same pin twice with shareable */
  esp_err_t result = iface.register_pin(iface.ctx, 9, "First user", true);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  result = iface.register_pin(iface.ctx, 9, "Second user", true);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Validate should pass */
  result = iface.validate(iface.ctx);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Clean up */
  iface.unregister_pin(iface.ctx, 9, "First user");
  iface.unregister_pin(iface.ctx, 9, "Second user");
  star_free_pin_validator();
}

/* --- Unified Header Tests --- */

void test_star_core_version(void)
{
  const char * version = star_core_version();
  TEST_ASSERT_NOT_NULL(version);
  TEST_ASSERT_TRUE(strlen(version) > 0);
}

void test_error_interface_is_valid_with_valid(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  TEST_ASSERT_TRUE(star_error_interface_is_valid(&iface));

  error_handler_deinit(&handler);
}

void test_error_interface_is_valid_with_null(void)
{
  TEST_ASSERT_FALSE(star_error_interface_is_valid(NULL));
}

void test_error_interface_is_valid_with_incomplete(void)
{
  star_error_interface_t iface = {0};
  iface.record_error = NULL; /* Missing function */
  TEST_ASSERT_FALSE(star_error_interface_is_valid(&iface));
}

void test_pin_interface_is_valid_with_valid(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  TEST_ASSERT_TRUE(star_pin_interface_is_valid(&iface));
}

void test_pin_interface_is_valid_with_null(void)
{
  TEST_ASSERT_FALSE(star_pin_interface_is_valid(NULL));
}

void test_pin_interface_is_valid_with_incomplete(void)
{
  star_pin_interface_t iface = {0};
  iface.register_pin = NULL; /* Missing function */
  TEST_ASSERT_FALSE(star_pin_interface_is_valid(&iface));
}

/* --- Convenience Macro Tests --- */

void test_iface_record_error_macro_with_null(void)
{
  star_error_interface_t * null_iface = NULL;
  esp_err_t result = STAR_IFACE_RECORD_ERROR(null_iface, ESP_FAIL, "Test");
  TEST_ASSERT_EQUAL(ESP_FAIL, result);
}

void test_iface_record_error_macro_with_valid(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);
  star_error_interface_t * iface_ptr = &iface;

  esp_err_t result = STAR_IFACE_RECORD_ERROR(iface_ptr, ESP_ERR_TIMEOUT, "Test error");
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, result);

  error_handler_deinit(&handler);
}

void test_iface_can_retry_macro_with_null(void)
{
  star_error_interface_t * null_iface = NULL;
  bool can_retry = STAR_IFACE_CAN_RETRY(null_iface);
  TEST_ASSERT_FALSE(can_retry);
}

void test_iface_reset_state_macro_with_null(void)
{
  star_error_interface_t * null_iface = NULL;
  esp_err_t result = STAR_IFACE_RESET_STATE(null_iface);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_iface_register_pin_macro_with_null(void)
{
  star_pin_interface_t * null_iface = NULL;
  esp_err_t result = STAR_IFACE_REGISTER_PIN(null_iface, 10, "Test", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_iface_unregister_pin_macro_with_null(void)
{
  star_pin_interface_t * null_iface = NULL;
  esp_err_t result = STAR_IFACE_UNREGISTER_PIN(null_iface, 10, "Test");
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_iface_validate_pins_macro_with_null(void)
{
  star_pin_interface_t * null_iface = NULL;
  esp_err_t result = STAR_IFACE_VALIDATE_PINS(null_iface);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_iface_register_pin_macro_with_valid(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);
  star_pin_interface_t * iface_ptr = &iface;

  esp_err_t result = STAR_IFACE_REGISTER_PIN(iface_ptr, 11, "Macro test", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Clean up */
  STAR_IFACE_UNREGISTER_PIN(iface_ptr, 11, "Macro test");
  star_free_pin_validator();
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Error Interface Tests */
  RUN_TEST(test_error_interface_struct_size);
  RUN_TEST(test_error_interface_null_ctx);
  RUN_TEST(test_error_interface_from_handler);
  RUN_TEST(test_error_interface_record_error);
  RUN_TEST(test_error_interface_can_retry);
  RUN_TEST(test_error_interface_reset_state);
  RUN_TEST(test_error_interface_get_null);

  /* Pin Interface Tests */
  RUN_TEST(test_pin_interface_struct_size);
  RUN_TEST(test_pin_interface_null_ctx);
  RUN_TEST(test_pin_interface_from_validator);
  RUN_TEST(test_pin_interface_register_pin);
  RUN_TEST(test_pin_interface_unregister_pin);
  RUN_TEST(test_pin_interface_validate);
  RUN_TEST(test_pin_interface_get_null);
  RUN_TEST(test_pin_interface_conflict_detection);
  RUN_TEST(test_pin_interface_shared_pins);

  /* Unified Header Tests */
  RUN_TEST(test_star_core_version);
  RUN_TEST(test_error_interface_is_valid_with_valid);
  RUN_TEST(test_error_interface_is_valid_with_null);
  RUN_TEST(test_error_interface_is_valid_with_incomplete);
  RUN_TEST(test_pin_interface_is_valid_with_valid);
  RUN_TEST(test_pin_interface_is_valid_with_null);
  RUN_TEST(test_pin_interface_is_valid_with_incomplete);

  /* Convenience Macro Tests */
  RUN_TEST(test_iface_record_error_macro_with_null);
  RUN_TEST(test_iface_record_error_macro_with_valid);
  RUN_TEST(test_iface_can_retry_macro_with_null);
  RUN_TEST(test_iface_reset_state_macro_with_null);
  RUN_TEST(test_iface_register_pin_macro_with_null);
  RUN_TEST(test_iface_unregister_pin_macro_with_null);
  RUN_TEST(test_iface_validate_pins_macro_with_null);
  RUN_TEST(test_iface_register_pin_macro_with_valid);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
