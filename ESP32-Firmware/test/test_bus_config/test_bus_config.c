/* Test bus config component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_types.h"
#include "star_pin_validator.h"
#include "driver/gpio.h"

/* Test helper globals */
static star_bus_manager_t s_manager;
static bool s_manager_initialized = false;

void setUp(void)
{
  /* Initialize manager for tests that need it */
  if (!s_manager_initialized) {
    star_bus_manager_init(&s_manager, "TestMgr", NULL, NULL);
    s_manager_initialized = true;
  }
}

void tearDown(void)
{
  /* Clean up pin validator between tests */
  star_free_pin_validator();

  /* Deinit manager if it was initialized */
  if (s_manager_initialized) {
    star_bus_manager_deinit(&s_manager);
    s_manager_initialized = false;
  }
}

/* ============================================================================
 * I2C CONFIG CREATION TESTS
 * ============================================================================ */

void test_config_create_i2c_valid(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL_STRING("TestI2C", config->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_i2c, config->type);
  TEST_ASSERT_EQUAL(I2C_NUM_0, config->proto.i2c.port);
  TEST_ASSERT_EQUAL(0x50, config->proto.i2c.address);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_config_create_i2c_null_name(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    NULL,
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_i2c_invalid_port(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    (i2c_port_t)99,  /* Invalid port number */
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_i2c_invalid_address(void)
{
  /* Address 0x00 is reserved (general call) */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x00,  /* Invalid address */
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_i2c_invalid_pins(void)
{
  /* Test with invalid GPIO pins */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_NC,  /* Invalid SDA pin */
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NULL(config);

  /* Test with invalid SCL pin */
  config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_NC,  /* Invalid SCL pin */
    100000
  );

  TEST_ASSERT_NULL(config);
}

/* ============================================================================
 * SPI CONFIG CREATION TESTS
 * ============================================================================ */

void test_config_create_spi_device_valid(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "TestSPI",
    SPI2_HOST,
    GPIO_NUM_23,  /* COPI */
    GPIO_NUM_19,  /* CIPO */
    GPIO_NUM_18,  /* SCLK */
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL_STRING("TestSPI", config->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  TEST_ASSERT_EQUAL(SPI2_HOST, config->proto.spi.host);
  TEST_ASSERT_FALSE(config->proto.spi.is_peripheral);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_config_create_spi_device_null_name(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    NULL,  /* Null name */
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_spi_device_null_dev_cfg(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_device(
    "TestSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    NULL  /* Null device config */
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_spi_peripheral_valid(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_peripheral(
    "TestSPIPeripheral",
    SPI2_HOST,
    GPIO_NUM_23,  /* COPI */
    GPIO_NUM_19,  /* CIPO */
    GPIO_NUM_18,  /* SCLK */
    GPIO_NUM_5,   /* CS */
    3,            /* queue_size */
    0             /* mode */
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL_STRING("TestSPIPeripheral", config->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  TEST_ASSERT_EQUAL(SPI2_HOST, config->proto.spi.host);
  TEST_ASSERT_TRUE(config->proto.spi.is_peripheral);
  TEST_ASSERT_EQUAL(3, config->proto.spi.queue_size);
  TEST_ASSERT_EQUAL(0, config->proto.spi.mode);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_config_create_spi_peripheral_invalid_mode(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_peripheral(
    "TestSPIPeripheral",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    GPIO_NUM_5,
    3,
    4  /* Invalid mode (must be 0-3) */
  );

  TEST_ASSERT_NULL(config);
}

void test_config_create_spi_peripheral_invalid_queue_size(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_peripheral(
    "TestSPIPeripheral",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    GPIO_NUM_5,
    0,  /* Invalid queue size */
    0
  );

  TEST_ASSERT_NULL(config);
}

/* ============================================================================
 * CONFIG LIFECYCLE TESTS
 * ============================================================================ */

void test_config_init_success(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_FALSE(config->initialized);

  esp_err_t result = star_bus_config_init(config, &s_manager);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_TRUE(config->initialized);

  star_bus_config_deinit(config);
  star_bus_config_destroy(config);
}

void test_config_init_already_initialized(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  /* First init should succeed */
  esp_err_t result = star_bus_config_init(config, &s_manager);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Second init should fail with INVALID_STATE */
  result = star_bus_config_init(config, &s_manager);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);

  star_bus_config_deinit(config);
  star_bus_config_destroy(config);
}

void test_config_deinit_success(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  star_bus_config_init(config, &s_manager);
  TEST_ASSERT_TRUE(config->initialized);

  esp_err_t result = star_bus_config_deinit(config);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_config_deinit_not_initialized(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_FALSE(config->initialized);

  /* Deinit without init should fail with INVALID_STATE */
  esp_err_t result = star_bus_config_deinit(config);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);

  star_bus_config_destroy(config);
}

void test_config_destroy_success(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  /* Destroy should complete without error */
  esp_err_t result = star_bus_config_destroy(config);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_config_destroy_null(void)
{
  /* Destroy null config should not crash and return error */
  esp_err_t result = star_bus_config_destroy(NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

/* ============================================================================
 * RESOURCE MANAGEMENT TESTS
 * ============================================================================ */

void test_config_create_destroy_cycle_no_leak(void)
{
  /* Create and destroy multiple configs to ensure no memory leak */
  for (int i = 0; i < 10; i++) {
    char name[32];
    snprintf(name, sizeof(name), "TestI2C_%d", i);

    star_bus_config_t * config = star_bus_config_create_i2c(
      name,
      I2C_NUM_0,
      0x50 + i,
      GPIO_NUM_21,
      GPIO_NUM_22,
      100000
    );
    TEST_ASSERT_NOT_NULL(config);
    star_bus_config_destroy(config);
  }

  /* If we reach here without crashing or running out of memory, test passes */
  TEST_PASS();
}

void test_config_multiple_creates_same_port(void)
{
  /* Create multiple configs on the same I2C port with different addresses */
  star_bus_config_t * config1 = star_bus_config_create_i2c(
    "Device1",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config1);

  star_bus_config_t * config2 = star_bus_config_create_i2c(
    "Device2",
    I2C_NUM_0,
    0x51,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config2);

  star_bus_config_t * config3 = star_bus_config_create_i2c(
    "Device3",
    I2C_NUM_0,
    0x52,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config3);

  /* Verify all configs are independent */
  TEST_ASSERT_EQUAL(0x50, config1->proto.i2c.address);
  TEST_ASSERT_EQUAL(0x51, config2->proto.i2c.address);
  TEST_ASSERT_EQUAL(0x52, config3->proto.i2c.address);

  star_bus_config_destroy(config1);
  star_bus_config_destroy(config2);
  star_bus_config_destroy(config3);
}

void test_config_reuse_after_destroy(void)
{
  /* Create a config */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  /* Initialize, deinit, and destroy */
  star_bus_config_init(config, &s_manager);
  star_bus_config_deinit(config);
  star_bus_config_destroy(config);

  /* Create a new config with the same parameters - should work fine */
  star_bus_config_t * new_config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(new_config);
  TEST_ASSERT_EQUAL_STRING("TestI2C", new_config->name);
  TEST_ASSERT_FALSE(new_config->initialized);

  star_bus_config_destroy(new_config);
}

/* ============================================================================
 * TEST RUNNER
 * ============================================================================ */

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* I2C Config Creation Tests */
  RUN_TEST(test_config_create_i2c_valid);
  RUN_TEST(test_config_create_i2c_null_name);
  RUN_TEST(test_config_create_i2c_invalid_port);
  RUN_TEST(test_config_create_i2c_invalid_address);
  RUN_TEST(test_config_create_i2c_invalid_pins);

  /* SPI Config Creation Tests */
  RUN_TEST(test_config_create_spi_device_valid);
  RUN_TEST(test_config_create_spi_device_null_name);
  RUN_TEST(test_config_create_spi_device_null_dev_cfg);
  RUN_TEST(test_config_create_spi_peripheral_valid);
  RUN_TEST(test_config_create_spi_peripheral_invalid_mode);
  RUN_TEST(test_config_create_spi_peripheral_invalid_queue_size);

  /* Config Lifecycle Tests */
  RUN_TEST(test_config_init_success);
  RUN_TEST(test_config_init_already_initialized);
  RUN_TEST(test_config_deinit_success);
  RUN_TEST(test_config_deinit_not_initialized);
  RUN_TEST(test_config_destroy_success);
  RUN_TEST(test_config_destroy_null);

  /* Resource Management Tests */
  RUN_TEST(test_config_create_destroy_cycle_no_leak);
  RUN_TEST(test_config_multiple_creates_same_port);
  RUN_TEST(test_config_reuse_after_destroy);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
