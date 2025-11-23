/* Integration tests for STAR Firmware components */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "star_core.h"
#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

void setUp(void) {}
void tearDown(void) {}

/* Test that all core components initialize together */
void test_full_stack_initialization(void)
{
  /* Initialize error handler */
  error_handler_t error_handler;
  esp_err_t result = error_handler_init(&error_handler, 3, 100, 1000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Get interfaces */
  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &error_handler);

  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Initialize bus manager with real interfaces */
  star_bus_manager_t manager;
  result = star_bus_manager_init(&manager, "Integration", &error_iface, &pin_iface);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
  error_handler_deinit(&error_handler);
}

/* Test bus configuration creation */
void test_bus_config_lifecycle(void)
{
  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "ConfigTest", NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Create an I2C bus config */
  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "test_i2c", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);
  TEST_ASSERT_NOT_NULL(i2c);

  /* Add to manager */
  result = star_bus_manager_add_bus(&manager, i2c);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Cleanup */
  star_bus_manager_deinit(&manager);
}

/* Test error recording through the stack */
void test_error_propagation(void)
{
  error_handler_t error_handler;
  error_handler_init(&error_handler, 3, 100, 1000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &error_handler);

  /* Record an error */
  esp_err_t result = iface.record_error(
    iface.ctx, ESP_ERR_TIMEOUT, "Integration test error",
    __FILE__, __LINE__, __func__);

  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, result);

  /* Check error was recorded */
  TEST_ASSERT_EQUAL(1, error_handler.error_count);

  error_handler_deinit(&error_handler);
}

/* Test pin validation through the stack */
void test_pin_validation(void)
{
  star_pin_interface_t iface;
  pin_validator_get_interface(&iface);

  /* Register a pin */
  esp_err_t result = iface.register_pin(iface.ctx, GPIO_NUM_4, "SDA", true);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Register another pin */
  result = iface.register_pin(iface.ctx, GPIO_NUM_5, "SCL", true);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Validate */
  result = iface.validate(iface.ctx);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Unregister */
  result = iface.unregister_pin(iface.ctx, GPIO_NUM_4, "SDA");
  TEST_ASSERT_EQUAL(ESP_OK, result);

  star_free_pin_validator();
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_full_stack_initialization);
  RUN_TEST(test_bus_config_lifecycle);
  RUN_TEST(test_error_propagation);
  RUN_TEST(test_pin_validation);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
