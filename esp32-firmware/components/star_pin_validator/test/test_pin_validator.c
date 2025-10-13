/* esp32-firmware/components/star_pin_validator/test/test_pin_validator.c */

/**
 * @file test_pin_validator.c
 * @brief STAR_TEST comprehensive tests for pin validator component
 */

#include "driver/gpio.h"

#include <inttypes.h>

#include "soc/gpio_num.h"
#include "star_pin_validator.h"
#include "star_test.h"

/* Helper to clean up after each test */
static void cleanup_pin_validator(void)
{
  star_free_pin_validator();
}

/* ========================================================================
 * Basic Pin Registration Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(pin_validator, register_valid_pin)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Test User", false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_shared_pin)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin(GPIO_NUM_5, "Shared User", true);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_pin_zero)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin(GPIO_NUM_0, "GPIO 0 User", false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_max_valid_pin)
{
  cleanup_pin_validator();
  /* GPIO_NUM_MAX - 1 is the last valid pin */
  esp_err_t result = star_register_pin((gpio_num_t)(GPIO_NUM_MAX - 1), "Max Pin User", false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_invalid_pin_too_high)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin((gpio_num_t)GPIO_NUM_MAX, "Invalid User", false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_invalid_pin_negative)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin((gpio_num_t)-1, "Invalid User", false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_pin_null_description)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin(GPIO_NUM_10, NULL, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_pin_empty_description)
{
  cleanup_pin_validator();
  esp_err_t result = star_register_pin(GPIO_NUM_11, "", false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_pin_max_length_description)
{
  cleanup_pin_validator();
  /* Create description exactly at max length - 1 (for null terminator) */
  char desc[PIN_VALIDATOR_DESC_MAX_LEN];
  for (uint32_t i = 0; i < PIN_VALIDATOR_DESC_MAX_LEN - 1; i++) {
    desc[i] = 'A';
  }
  desc[PIN_VALIDATOR_DESC_MAX_LEN - 1] = '\0';

  esp_err_t result = star_register_pin(GPIO_NUM_12, desc, false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, register_pin_too_long_description)
{
  cleanup_pin_validator();
  /* Create description that exceeds max length */
  char desc[PIN_VALIDATOR_DESC_MAX_LEN + 10];
  for (uint32_t i = 0; i < PIN_VALIDATOR_DESC_MAX_LEN + 5; i++) {
    desc[i] = 'B';
  }
  desc[PIN_VALIDATOR_DESC_MAX_LEN + 5] = '\0';

  esp_err_t result = star_register_pin(GPIO_NUM_13, desc, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  cleanup_pin_validator();
}

/* ========================================================================
 * Pin Sharing Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(pin_validator, share_pin_both_shareable)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_14, "User 1", true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_14, "User 2", true);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, share_pin_three_users)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_15, "User 1", true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_15, "User 2", true);
  esp_err_t result3 = star_register_pin(GPIO_NUM_15, "User 3", true);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  STAR_ASSERT_EQUAL(ESP_OK, result3);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, conflict_first_not_shareable)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_16, "User 1", false);
  esp_err_t result2 = star_register_pin(GPIO_NUM_16, "User 2", true);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, conflict_second_not_shareable)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_17, "User 1", true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_17, "User 2", false);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, conflict_both_not_shareable)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_18, "User 1", false);
  esp_err_t result2 = star_register_pin(GPIO_NUM_18, "User 2", false);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, multiple_different_pins)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_19, "Pin 19 User", false);
  esp_err_t result2 = star_register_pin(GPIO_NUM_20, "Pin 20 User", false);
  esp_err_t result3 = star_register_pin(GPIO_NUM_21, "Pin 21 User", false);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  STAR_ASSERT_EQUAL(ESP_OK, result3);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, mixed_shared_and_exclusive_pins)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_22, "Pin 22 Shared", true);
  star_register_pin(GPIO_NUM_22, "Pin 22 Shared 2", true);
  star_register_pin(GPIO_NUM_23, "Pin 23 Exclusive", false);
  star_register_pin(GPIO_NUM_21, "Pin 21 Shared", true);

  esp_err_t conflict = star_register_pin(GPIO_NUM_23, "Pin 23 Conflict", false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, conflict);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, shared_pin_long_descriptions)
{
  cleanup_pin_validator();
  char desc1[PIN_VALIDATOR_DESC_MAX_LEN];
  char desc2[PIN_VALIDATOR_DESC_MAX_LEN];

  for (uint32_t i = 0; i < PIN_VALIDATOR_DESC_MAX_LEN - 2; i++) {
    desc1[i] = 'X';
    desc2[i] = 'Y';
  }
  desc1[PIN_VALIDATOR_DESC_MAX_LEN - 2] = '\0';
  desc2[PIN_VALIDATOR_DESC_MAX_LEN - 2] = '\0';

  esp_err_t result1 = star_register_pin(GPIO_NUM_25, desc1, true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_25, desc2, true);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, share_same_description)
{
  cleanup_pin_validator();
  esp_err_t result1 = star_register_pin(GPIO_NUM_26, "Same Description", true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_26, "Same Description", true);
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, many_shared_users)
{
  cleanup_pin_validator();
  /* Register 10 users on same shared pin */
  for (uint32_t i = 0; i < 10; i++) {
    char desc[32];
    snprintf(desc, sizeof(desc), "User %" PRIu32 "", i);
    esp_err_t result = star_register_pin(GPIO_NUM_27, desc, true);
    STAR_ASSERT_EQUAL(ESP_OK, result);
  }
  cleanup_pin_validator();
}

/* ========================================================================
 * Validation Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(pin_validator, validate_no_pins_registered)
{
  cleanup_pin_validator();
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_single_exclusive_pin)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_28, "Exclusive", false);
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_multiple_exclusive_pins)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_29, "Exclusive 1", false);
  star_register_pin(GPIO_NUM_30, "Exclusive 2", false);
  star_register_pin(GPIO_NUM_31, "Exclusive 3", false);
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_shared_pins_no_conflict)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_32, "Shared 1", true);
  star_register_pin(GPIO_NUM_32, "Shared 2", true);
  star_register_pin(GPIO_NUM_32, "Shared 3", true);
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_mixed_pins_no_conflict)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_33, "Exclusive", false);
  star_register_pin(GPIO_NUM_34, "Shared 1", true);
  star_register_pin(GPIO_NUM_34, "Shared 2", true);
  star_register_pin(GPIO_NUM_35, "Exclusive 2", false);
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_multiple_calls)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_36, "User 1", true);
  esp_err_t result1 = star_validate_pins();
  star_register_pin(GPIO_NUM_36, "User 2", true);
  esp_err_t result2 = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_after_registration_failure)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_37, "User 1", false);
  /* This should fail */
  star_register_pin(GPIO_NUM_37, "User 2", false);
  /* Validation should still pass - failed registration didn't add user */
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_comprehensive_setup)
{
  cleanup_pin_validator();
  /* Register various pins */
  star_register_pin(GPIO_NUM_0, "Pin 0", false);
  star_register_pin(GPIO_NUM_2, "SDA", true);
  star_register_pin(GPIO_NUM_2, "I2C", true);
  star_register_pin(GPIO_NUM_4, "LED", false);
  star_register_pin(GPIO_NUM_15, "UART TX", false);
  star_register_pin(GPIO_NUM_16, "UART RX", false);

  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

/* ========================================================================
 * Cleanup/Free Tests (7 tests)
 * ======================================================================== */

STAR_TEST_CASE(pin_validator, free_uninitialized)
{
  cleanup_pin_validator();
  esp_err_t result = star_free_pin_validator();
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(pin_validator, free_after_single_registration)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_38, "Test", false);
  esp_err_t result = star_free_pin_validator();
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(pin_validator, free_after_multiple_registrations)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_39, "User 1", false);
  star_register_pin(GPIO_NUM_4, "User 2", false);
  star_register_pin(GPIO_NUM_5, "User 3", true);
  star_register_pin(GPIO_NUM_5, "User 4", true);
  esp_err_t result = star_free_pin_validator();
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(pin_validator, free_twice)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_12, "Test", false);
  esp_err_t result1 = star_free_pin_validator();
  esp_err_t result2 = star_free_pin_validator();
  STAR_ASSERT_EQUAL(ESP_OK, result1);
  STAR_ASSERT_EQUAL(ESP_OK, result2);
}

STAR_TEST_CASE(pin_validator, register_after_free)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_13, "First", false);
  star_free_pin_validator();
  /* Should be able to register again after free */
  esp_err_t result = star_register_pin(GPIO_NUM_13, "After Free", false);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  cleanup_pin_validator();
}

STAR_TEST_CASE(pin_validator, validate_after_free)
{
  cleanup_pin_validator();
  star_register_pin(GPIO_NUM_14, "Test", false);
  star_free_pin_validator();
  /* Validation after free should succeed (no pins registered) */
  esp_err_t result = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(pin_validator, free_with_many_shared_users)
{
  cleanup_pin_validator();
  /* Register many users on shared pin */
  for (uint32_t i = 0; i < 15; i++) {
    char desc[32];
    snprintf(desc, sizeof(desc), "User %" PRIu32 "", i);
    star_register_pin(GPIO_NUM_27, desc, true);
  }
  esp_err_t result = star_free_pin_validator();
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()
/* Basic registration tests */
STAR_TEST_REF(pin_validator, register_valid_pin)
STAR_TEST_REF(pin_validator, register_shared_pin)
STAR_TEST_REF(pin_validator, register_pin_zero)
STAR_TEST_REF(pin_validator, register_max_valid_pin)
STAR_TEST_REF(pin_validator, register_invalid_pin_too_high)
STAR_TEST_REF(pin_validator, register_invalid_pin_negative)
STAR_TEST_REF(pin_validator, register_pin_null_description)
STAR_TEST_REF(pin_validator, register_pin_empty_description)
STAR_TEST_REF(pin_validator, register_pin_max_length_description)
STAR_TEST_REF(pin_validator, register_pin_too_long_description)

/* Pin sharing tests */
STAR_TEST_REF(pin_validator, share_pin_both_shareable)
STAR_TEST_REF(pin_validator, share_pin_three_users)
STAR_TEST_REF(pin_validator, conflict_first_not_shareable)
STAR_TEST_REF(pin_validator, conflict_second_not_shareable)
STAR_TEST_REF(pin_validator, conflict_both_not_shareable)
STAR_TEST_REF(pin_validator, multiple_different_pins)
STAR_TEST_REF(pin_validator, mixed_shared_and_exclusive_pins)
STAR_TEST_REF(pin_validator, shared_pin_long_descriptions)
STAR_TEST_REF(pin_validator, share_same_description)
STAR_TEST_REF(pin_validator, many_shared_users)

/* Validation tests */
STAR_TEST_REF(pin_validator, validate_no_pins_registered)
STAR_TEST_REF(pin_validator, validate_single_exclusive_pin)
STAR_TEST_REF(pin_validator, validate_multiple_exclusive_pins)
STAR_TEST_REF(pin_validator, validate_shared_pins_no_conflict)
STAR_TEST_REF(pin_validator, validate_mixed_pins_no_conflict)
STAR_TEST_REF(pin_validator, validate_multiple_calls)
STAR_TEST_REF(pin_validator, validate_after_registration_failure)
STAR_TEST_REF(pin_validator, validate_comprehensive_setup)

/* Cleanup tests */
STAR_TEST_REF(pin_validator, free_uninitialized)
STAR_TEST_REF(pin_validator, free_after_single_registration)
STAR_TEST_REF(pin_validator, free_after_multiple_registrations)
STAR_TEST_REF(pin_validator, free_twice)
STAR_TEST_REF(pin_validator, register_after_free)
STAR_TEST_REF(pin_validator, validate_after_free)
STAR_TEST_REF(pin_validator, free_with_many_shared_users)
STAR_TEST_LIST_END()
