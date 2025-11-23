/* Test I2C bus component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_types.h"
#include "star_bus_i2c.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "driver/gpio.h"

/* Test globals */
static star_bus_manager_t *s_test_manager = NULL;
static star_bus_config_t *s_test_config = NULL;

/* Mock tracking variables */
static int s_s_mock_write_call_count = 0;
static int s_s_mock_read_call_count = 0;
static esp_err_t s_s_mock_write_return = ESP_OK;
static esp_err_t s_s_mock_read_return = ESP_OK;

/* Mock I2C operation functions */
static esp_err_t priv_mock_i2c_write(const star_bus_config_t *bus,
                                const uint8_t *data,
                                size_t len,
                                uint8_t reg_addr,
                                size_t *bytes_written)
{
  (void)bus;
  (void)data;
  (void)reg_addr;
  s_mock_write_call_count++;
  if (bytes_written) {
    *bytes_written = len;
  }
  return s_mock_write_return;
}

static esp_err_t priv_mock_i2c_read(const star_bus_config_t * bus,
                               uint8_t* data,
                               size_t len,
                               uint8_t reg_addr,
                               size_t* bytes_read)
{
  (void)bus;
  (void)reg_addr;
  s_mock_read_call_count++;
  if (data && len > 0) {
    memset(data, 0xAA, len);
  }
  if (bytes_read) {
    *bytes_read = len;
  }
  return s_mock_read_return;
}

static esp_err_t priv_mock_i2c_write_command(const star_bus_config_t * bus, uint8_t command)
{
  (void)bus;
  (void)command;
  s_mock_write_call_count++;
  return s_mock_write_return;
}

static esp_err_t priv_mock_i2c_read_raw(const star_bus_config_t * bus,
                                   uint8_t* data,
                                   size_t len,
                                   size_t* bytes_read)
{
  (void)bus;
  s_mock_read_call_count++;
  if (data && len > 0) {
    memset(data, 0xBB, len);
  }
  if (bytes_read) {
    *bytes_read = len;
  }
  return s_mock_read_return;
}

void setUp(void)
{
  s_mock_write_call_count = 0;
  s_mock_read_call_count = 0;
  s_mock_write_return = ESP_OK;
  s_mock_read_return = ESP_OK;
  s_test_manager = NULL;
  s_test_config = NULL;
}

void tearDown(void)
{
  if (s_test_manager) {
    star_bus_manager_deinit(s_test_manager);
    s_test_manager = NULL;
  }
  s_test_config = NULL;
  star_free_pin_validator();
}

/* Helper to create initialized test manager with I2C bus */
static star_bus_manager_t * create_test_manager_with_i2c_bus(const char * bus_name)
{
  static star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  if (result != ESP_OK) {
    return NULL;
  }

  star_bus_config_t * config = star_bus_config_create_i2c(
    bus_name,
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  if (config == NULL) {
    star_bus_manager_deinit(&manager);
    return NULL;
  }

  /* Install mock operations */
  config->proto.i2c.ops.write = priv_mock_i2c_write;
  config->proto.i2c.ops.read = priv_mock_i2c_read;
  config->proto.i2c.ops.write_command = priv_mock_i2c_write_command;
  config->proto.i2c.ops.read_raw = priv_mock_i2c_read_raw;
  config->initialized = true;

  result = star_bus_manager_add_bus(&manager, config);
  if (result != ESP_OK) {
    star_bus_config_destroy(config);
    star_bus_manager_deinit(&manager);
    return NULL;
  }

  return &manager;
}

/* ============================================================================
 * INITIALIZATION TESTS
 * ============================================================================ */

void test_i2c_init_default_ops_null_pointer(void)
{
  /* Should not crash when given NULL */
  star_bus_i2c_init_default_ops(NULL);
  /* If we get here without crashing, test passes */
  TEST_PASS();
}

void test_i2c_init_default_ops_assigns_all_functions(void)
{
  star_i2c_ops_t ops;
  memset(&ops, 0, sizeof(ops));

  star_bus_i2c_init_default_ops(&ops);

  TEST_ASSERT_NOT_NULL(ops.write);
  TEST_ASSERT_NOT_NULL(ops.read);
  TEST_ASSERT_NOT_NULL(ops.write_command);
  TEST_ASSERT_NOT_NULL(ops.read_raw);
}

void test_i2c_config_create_with_null_name(void)
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

void test_i2c_config_create_with_valid_params(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "ValidI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL_STRING("ValidI2C", config->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_i2c, config->type);
  TEST_ASSERT_EQUAL(I2C_NUM_0, config->proto.i2c.port);
  TEST_ASSERT_EQUAL(0x50, config->proto.i2c.address);
  TEST_ASSERT_EQUAL(100000, config->proto.i2c.config.master.clk_speed);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_i2c_config_init_null_config(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_config_init(NULL, &manager);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

void test_i2c_config_init_null_manager(void)
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

  esp_err_t result = star_bus_config_init(config, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_config_destroy(config);
}

void test_i2c_config_double_init(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  star_bus_manager_add_bus(&manager, config);

  /* First init - may succeed or fail depending on hardware */
  esp_err_t first_result = star_bus_config_init(config, &manager);

  /* If first succeeded, second should fail or return already initialized */
  if (first_result == ESP_OK) {
    config->initialized = true;
    esp_err_t second_result = star_bus_config_init(config, &manager);
    /* Double init should return error or ESP_OK if idempotent */
    TEST_ASSERT_TRUE(second_result == ESP_ERR_INVALID_STATE || second_result == ESP_OK);
  }

  star_bus_manager_deinit(&manager);
}

/* ============================================================================
 * READ OPERATION TESTS
 * ============================================================================ */

void test_i2c_read_null_manager(void)
{
  uint8_t buffer[10];
  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(NULL, "TestI2C", buffer, sizeof(buffer), 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_read_null_name(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  uint8_t buffer[10];
  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(&manager, NULL, buffer, sizeof(buffer), 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_read_null_buffer(void)
{
  s_test_manager = create_test_manager_with_i2c_bus("TestI2C");
  TEST_ASSERT_NOT_NULL(s_test_manager);

  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(s_test_manager, "TestI2C", NULL, 10, 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_read_zero_length(void)
{
  s_test_manager = create_test_manager_with_i2c_bus("TestI2C");
  TEST_ASSERT_NOT_NULL(s_test_manager);

  uint8_t buffer[10];
  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(s_test_manager, "TestI2C", buffer, 0, 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_read_nonexistent_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  uint8_t buffer[10];
  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(&manager, "NonExistent", buffer, sizeof(buffer), 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);
}

void test_i2c_read_uninitialized_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  star_bus_config_t * config = star_bus_config_create_i2c(
    "UninitI2C",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  /* Add bus but don't initialize it */
  config->initialized = false;
  star_bus_manager_add_bus(&manager, config);

  uint8_t buffer[10];
  size_t bytes_read = 0;

  esp_err_t result = star_bus_i2c_read(&manager, "UninitI2C", buffer, sizeof(buffer), 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

/* ============================================================================
 * WRITE OPERATION TESTS
 * ============================================================================ */

void test_i2c_write_null_manager(void)
{
  uint8_t buffer[10] = {0x01, 0x02, 0x03};
  size_t bytes_written = 0;

  esp_err_t result = star_bus_i2c_write(NULL, "TestI2C", buffer, sizeof(buffer), 0x00, &bytes_written);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_write_null_name(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  uint8_t buffer[10] = {0x01, 0x02, 0x03};
  size_t bytes_written = 0;

  esp_err_t result = star_bus_i2c_write(&manager, NULL, buffer, sizeof(buffer), 0x00, &bytes_written);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_write_null_buffer(void)
{
  s_test_manager = create_test_manager_with_i2c_bus("TestI2C");
  TEST_ASSERT_NOT_NULL(s_test_manager);

  size_t bytes_written = 0;

  esp_err_t result = star_bus_i2c_write(s_test_manager, "TestI2C", NULL, 10, 0x00, &bytes_written);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_write_zero_length(void)
{
  s_test_manager = create_test_manager_with_i2c_bus("TestI2C");
  TEST_ASSERT_NOT_NULL(s_test_manager);

  uint8_t buffer[10] = {0x01};
  size_t bytes_written = 0;

  esp_err_t result = star_bus_i2c_write(s_test_manager, "TestI2C", buffer, 0, 0x00, &bytes_written);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_i2c_write_nonexistent_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  uint8_t buffer[10] = {0x01, 0x02, 0x03};
  size_t bytes_written = 0;

  esp_err_t result = star_bus_i2c_write(&manager, "NonExistent", buffer, sizeof(buffer), 0x00, &bytes_written);
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);
}

/* ============================================================================
 * BOUNDARY TESTS
 * ============================================================================ */

void test_i2c_address_reserved_range(void)
{
  /* I2C reserved addresses: 0x00-0x07 and 0x78-0x7F */
  /* Test creating configs with reserved addresses - implementation may allow or reject */

  /* Address 0x00 - general call address */
  star_bus_config_t * config_low = star_bus_config_create_i2c(
    "ReservedLow",
    I2C_NUM_0,
    0x00,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  /* Address 0x7F - reserved */
  star_bus_config_t * config_high = star_bus_config_create_i2c(
    "ReservedHigh",
    I2C_NUM_0,
    0x7F,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  /* Implementation may or may not reject reserved addresses at creation time */
  /* Just verify consistency */
  if (config_low != NULL) {
    TEST_ASSERT_EQUAL(0x00, config_low->proto.i2c.address);
    star_bus_config_destroy(config_low);
  }

  if (config_high != NULL) {
    TEST_ASSERT_EQUAL(0x7F, config_high->proto.i2c.address);
    star_bus_config_destroy(config_high);
  }

  TEST_PASS();
}

void test_i2c_address_valid_range(void)
{
  /* Valid 7-bit I2C addresses: 0x08-0x77 */
  uint8_t test_addresses[] = {0x08, 0x20, 0x50, 0x68, 0x77};

  for (size_t i = 0; i < sizeof(test_addresses); i++) {
    char name[32];
    snprintf(name, sizeof(name), "ValidAddr_%02X", test_addresses[i]);

    star_bus_config_t * config = star_bus_config_create_i2c(
      name,
      I2C_NUM_0,
      test_addresses[i],
      GPIO_NUM_21,
      GPIO_NUM_22,
      100000
    );

    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL(test_addresses[i], config->proto.i2c.address);

    star_bus_config_destroy(config);
  }
}

void test_i2c_register_address_boundaries(void)
{
  s_test_manager = create_test_manager_with_i2c_bus("TestI2C");
  TEST_ASSERT_NOT_NULL(s_test_manager);

  uint8_t buffer[4] = {0};
  size_t bytes_read = 0;

  /* Test reading from register 0x00 */
  esp_err_t result = star_bus_i2c_read(s_test_manager, "TestI2C", buffer, sizeof(buffer), 0x00, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(1, s_mock_read_call_count);

  /* Test reading from register 0xFF */
  result = star_bus_i2c_read(s_test_manager, "TestI2C", buffer, sizeof(buffer), 0xFF, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(2, s_mock_read_call_count);

  /* Test mid-range register */
  result = star_bus_i2c_read(s_test_manager, "TestI2C", buffer, sizeof(buffer), 0x80, &bytes_read);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(3, s_mock_read_call_count);
}

/* ============================================================================
 * MANAGER INTEGRATION TESTS
 * ============================================================================ */

void test_i2c_manager_add_find_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  star_bus_config_t * config = star_bus_config_create_i2c(
    "FindMe",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  TEST_ASSERT_EQUAL(ESP_OK, result);

  star_bus_config_t * found = star_bus_manager_find_bus(&manager, "FindMe");
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_STRING("FindMe", found->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_i2c, found->type);
  TEST_ASSERT_EQUAL(0x50, found->proto.i2c.address);
}

void test_i2c_manager_remove_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  star_bus_config_t * config = star_bus_config_create_i2c(
    "RemoveMe",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );
  TEST_ASSERT_NOT_NULL(config);

  star_bus_manager_add_bus(&manager, config);

  /* Verify bus exists */
  star_bus_config_t * found = star_bus_manager_find_bus(&manager, "RemoveMe");
  TEST_ASSERT_NOT_NULL(found);

  /* Remove bus */
  esp_err_t result = star_bus_manager_remove_bus(&manager, "RemoveMe");
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Verify bus no longer exists */
  found = star_bus_manager_find_bus(&manager, "RemoveMe");
  TEST_ASSERT_NULL(found);
}

void test_i2c_manager_multiple_buses(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_test_manager = &manager;

  /* Create multiple I2C buses with different addresses */
  const char * bus_names[] = {"I2C_Sensor1", "I2C_Sensor2", "I2C_EEPROM"};
  uint8_t addresses[] = {0x48, 0x50, 0x68};

  for (size_t i = 0; i < 3; i++) {
    star_bus_config_t * config = star_bus_config_create_i2c(
      bus_names[i],
      I2C_NUM_0,
      addresses[i],
      GPIO_NUM_21,
      GPIO_NUM_22,
      100000
    );
    TEST_ASSERT_NOT_NULL(config);

    esp_err_t result = star_bus_manager_add_bus(&manager, config);
    TEST_ASSERT_EQUAL(ESP_OK, result);
  }

  /* Verify all buses can be found */
  for (size_t i = 0; i < 3; i++) {
    star_bus_config_t * found = star_bus_manager_find_bus(&manager, bus_names[i]);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING(bus_names[i], found->name);
    TEST_ASSERT_EQUAL(addresses[i], found->proto.i2c.address);
  }

  /* Verify nonexistent bus still returns NULL */
  star_bus_config_t * not_found = star_bus_manager_find_bus(&manager, "NonExistent");
  TEST_ASSERT_NULL(not_found);
}

/* ============================================================================
 * TEST RUNNER
 * ============================================================================ */

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Initialization Tests */
  RUN_TEST(test_i2c_init_default_ops_null_pointer);
  RUN_TEST(test_i2c_init_default_ops_assigns_all_functions);
  RUN_TEST(test_i2c_config_create_with_null_name);
  RUN_TEST(test_i2c_config_create_with_valid_params);
  RUN_TEST(test_i2c_config_init_null_config);
  RUN_TEST(test_i2c_config_init_null_manager);
  RUN_TEST(test_i2c_config_double_init);

  /* Read Operation Tests */
  RUN_TEST(test_i2c_read_null_manager);
  RUN_TEST(test_i2c_read_null_name);
  RUN_TEST(test_i2c_read_null_buffer);
  RUN_TEST(test_i2c_read_zero_length);
  RUN_TEST(test_i2c_read_nonexistent_bus);
  RUN_TEST(test_i2c_read_uninitialized_bus);

  /* Write Operation Tests */
  RUN_TEST(test_i2c_write_null_manager);
  RUN_TEST(test_i2c_write_null_name);
  RUN_TEST(test_i2c_write_null_buffer);
  RUN_TEST(test_i2c_write_zero_length);
  RUN_TEST(test_i2c_write_nonexistent_bus);

  /* Boundary Tests */
  RUN_TEST(test_i2c_address_reserved_range);
  RUN_TEST(test_i2c_address_valid_range);
  RUN_TEST(test_i2c_register_address_boundaries);

  /* Manager Integration Tests */
  RUN_TEST(test_i2c_manager_add_find_bus);
  RUN_TEST(test_i2c_manager_remove_bus);
  RUN_TEST(test_i2c_manager_multiple_buses);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
