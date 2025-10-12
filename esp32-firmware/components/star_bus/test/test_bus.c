/* esp32-firmware/components/star_bus/test/test_bus.c */

/**
 * @file test_bus.c
 * @brief STAR_TEST comprehensive tests for star_bus component (122 tests)
 *
 * Tests cover:
 * - Bus manager lifecycle (8 tests)
 * - Bus configuration creation (10 tests)
 * - Bus operations (12 tests)
 * - Type utilities (8 tests)
 * - I2C/SPI API validation (8 tests)
 * - Pin validator integration (10 tests)
 * - Error handler integration (8 tests)
 * - SPI peripheral mode (21 tests)
 * - SMBus protocol (37 tests)
 */

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"
#include "star_bus_spi.h"
#include "star_bus_spi_peripheral.h"
#include "star_bus_types.h"
#include "star_pin_validator.h"
#include "star_test.h"

/* Test pin definitions (safe GPIOs for ESP32) */
#define TEST_I2C_SDA_PIN (GPIO_NUM_21)
#define TEST_I2C_SCL_PIN (GPIO_NUM_22)
#define TEST_SPI_COPI_PIN (GPIO_NUM_23)
#define TEST_SPI_CIPO_PIN (GPIO_NUM_19)
#define TEST_SPI_SCLK_PIN (GPIO_NUM_18)
#define TEST_SPI_CS_PIN (GPIO_NUM_5)
#define TEST_I2C_ADDR (0x3C)
#define TEST_I2C_CLOCK (100000)
#define TEST_SPI_CLOCK (1000000)

/* ========================================================================
 * Bus Manager Lifecycle Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(bus_manager, init_valid)
{
  star_bus_manager_t manager;
  esp_err_t          result = star_bus_manager_init(&manager, "TestMgr");
  STAR_ASSERT_EQUAL(ESP_OK, result);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_manager, init_null_pointer)
{
  esp_err_t result = star_bus_manager_init(NULL, "TestMgr");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(bus_manager, init_null_tag)
{
  star_bus_manager_t manager;
  esp_err_t          result = star_bus_manager_init(&manager, NULL);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(manager.tag);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_manager, init_empty_tag)
{
  star_bus_manager_t manager;
  esp_err_t          result = star_bus_manager_init(&manager, "");
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(manager.tag);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_manager, init_custom_tag)
{
  star_bus_manager_t manager;
  esp_err_t          result = star_bus_manager_init(&manager, "CustomTag");
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(manager.tag);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_manager, deinit_valid)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");
  esp_err_t result = star_bus_manager_deinit(&manager);
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(bus_manager, deinit_null_pointer)
{
  esp_err_t result = star_bus_manager_deinit(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(bus_manager, init_deinit_multiple_times)
{
  star_bus_manager_t manager;
  for (int i = 0; i < 3; i++) {
    esp_err_t init_result = star_bus_manager_init(&manager, "TestMgr");
    STAR_ASSERT_EQUAL(ESP_OK, init_result);
    esp_err_t deinit_result = star_bus_manager_deinit(&manager);
    STAR_ASSERT_EQUAL(ESP_OK, deinit_result);
  }
}

/* ========================================================================
 * Bus Configuration Creation Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(bus_config, create_i2c_valid)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_i2c, config->type);
  STAR_ASSERT_STR_EQUAL("test_i2c", config->name);
  STAR_ASSERT_FALSE(config->initialized);
  star_bus_config_destroy(config);
}

STAR_TEST_CASE(bus_config, create_i2c_null_name)
{
  star_bus_config_t* config =
    star_bus_config_create_i2c(NULL, I2C_NUM_0, TEST_I2C_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(bus_config, create_i2c_various_speeds)
{
  uint32_t speeds[] = {10000, 50000, 100000, 400000};
  for (int i = 0; i < 4; i++) {
    star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                           I2C_NUM_0,
                                                           TEST_I2C_ADDR,
                                                           TEST_I2C_SDA_PIN,
                                                           TEST_I2C_SCL_PIN,
                                                           speeds[i]);
    STAR_ASSERT_NOT_NULL(config);
    STAR_ASSERT_EQUAL(speeds[i], config->proto.i2c.config.master.clk_speed);
    star_bus_config_destroy(config);
  }
}

STAR_TEST_CASE(bus_config, create_spi_valid)
{
  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device("test_spi",
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  STAR_ASSERT_STR_EQUAL("test_spi", config->name);
  STAR_ASSERT_FALSE(config->initialized);
  star_bus_config_destroy(config);
}

STAR_TEST_CASE(bus_config, create_spi_null_name)
{
  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device(NULL,
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(bus_config, create_spi_null_dev_cfg)
{
  star_bus_config_t* config = star_bus_config_create_spi_device("test_spi",
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                NULL);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(bus_config, create_spi_invalid_host)
{
  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device("test_spi",
                                                                (spi_host_device_t)999,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(bus_config, destroy_valid_config)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  esp_err_t result = star_bus_config_destroy(config);
  STAR_ASSERT_EQUAL(ESP_OK, result);
}

STAR_TEST_CASE(bus_config, destroy_null_config)
{
  esp_err_t result = star_bus_config_destroy(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(bus_config, deinit_uninitialized_config)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  esp_err_t result = star_bus_config_deinit(config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
  star_bus_config_destroy(config);
}

/* ========================================================================
 * Bus Manager Operations Tests (12 tests)
 * ======================================================================== */

STAR_TEST_CASE(bus_operations, add_i2c_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_TRUE(config->initialized);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, add_spi_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device("test_spi",
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NOT_NULL(config);

  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_TRUE(config->initialized);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, add_null_manager)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  esp_err_t          result = star_bus_manager_add_bus(NULL, config);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  star_bus_config_destroy(config);
}

STAR_TEST_CASE(bus_operations, add_null_config)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");
  esp_err_t result = star_bus_manager_add_bus(&manager, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, add_duplicate_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config1 = star_bus_config_create_i2c("duplicate",
                                                          I2C_NUM_0,
                                                          TEST_I2C_ADDR,
                                                          TEST_I2C_SDA_PIN,
                                                          TEST_I2C_SCL_PIN,
                                                          TEST_I2C_CLOCK);
  star_bus_config_t* config2 = star_bus_config_create_i2c("duplicate",
                                                          I2C_NUM_0,
                                                          TEST_I2C_ADDR,
                                                          GPIO_NUM_25,
                                                          GPIO_NUM_26,
                                                          TEST_I2C_CLOCK);

  esp_err_t result1 = star_bus_manager_add_bus(&manager, config1);
  STAR_ASSERT_EQUAL(ESP_OK, result1);

  esp_err_t result2 = star_bus_manager_add_bus(&manager, config2);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result2);

  star_bus_config_destroy(config2);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, find_existing_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_i2c("findme",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  star_bus_manager_add_bus(&manager, config);

  star_bus_config_t* found = star_bus_manager_find_bus(&manager, "findme");
  STAR_ASSERT_NOT_NULL(found);
  STAR_ASSERT_EQUAL(config, found);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, find_nonexistent_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* found = star_bus_manager_find_bus(&manager, "nonexistent");
  STAR_ASSERT_NULL(found);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, find_null_manager)
{
  star_bus_config_t* found = star_bus_manager_find_bus(NULL, "test");
  STAR_ASSERT_NULL(found);
}

STAR_TEST_CASE(bus_operations, find_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* found = star_bus_manager_find_bus(&manager, NULL);
  STAR_ASSERT_NULL(found);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, remove_existing_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_i2c("removeme",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  star_bus_manager_add_bus(&manager, config);

  esp_err_t result = star_bus_manager_remove_bus(&manager, "removeme");
  STAR_ASSERT_EQUAL(ESP_OK, result);

  star_bus_config_t* found = star_bus_manager_find_bus(&manager, "removeme");
  STAR_ASSERT_NULL(found);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, remove_nonexistent_bus)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_manager_remove_bus(&manager, "nonexistent");
  STAR_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_operations, add_multiple_buses)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* ESP32 has 2 I2C ports, so we can test 2 buses */
  i2c_port_t ports[] = {I2C_NUM_0, I2C_NUM_1};
  for (int i = 0; i < 2; i++) {
    char name[16];
    snprintf(name, sizeof(name), "i2c_bus_%d", i);
    star_bus_config_t* config = star_bus_config_create_i2c(name,
                                                           ports[i],
                                                           TEST_I2C_ADDR + i,
                                                           TEST_I2C_SDA_PIN,
                                                           TEST_I2C_SCL_PIN,
                                                           TEST_I2C_CLOCK);
    esp_err_t          result = star_bus_manager_add_bus(&manager, config);
    STAR_ASSERT_EQUAL(ESP_OK, result);
  }

  /* Verify all buses can be found */
  for (int i = 0; i < 2; i++) {
    char name[16];
    snprintf(name, sizeof(name), "i2c_bus_%d", i);
    star_bus_config_t* found = star_bus_manager_find_bus(&manager, name);
    STAR_ASSERT_NOT_NULL(found);
  }

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * Bus Type Utility Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(bus_type, to_string_none)
{
  const char* str = star_bus_type_to_string(k_star_bus_type_none);
  STAR_ASSERT_STR_EQUAL("None", str);
}

STAR_TEST_CASE(bus_type, to_string_i2c)
{
  const char* str = star_bus_type_to_string(k_star_bus_type_i2c);
  STAR_ASSERT_STR_EQUAL("I2C", str);
}

STAR_TEST_CASE(bus_type, to_string_spi)
{
  const char* str = star_bus_type_to_string(k_star_bus_type_spi);
  STAR_ASSERT_STR_EQUAL("SPI", str);
}

STAR_TEST_CASE(bus_type, to_string_invalid_low)
{
  const char* str = star_bus_type_to_string((star_bus_type_t)-1);
  STAR_ASSERT_STR_EQUAL("Unknown", str);
}

STAR_TEST_CASE(bus_type, to_string_invalid_high)
{
  const char* str = star_bus_type_to_string((star_bus_type_t)999);
  STAR_ASSERT_STR_EQUAL("Unknown", str);
}

STAR_TEST_CASE(bus_type, to_string_count_boundary)
{
  const char* str = star_bus_type_to_string(k_star_bus_type_count);
  STAR_ASSERT_STR_EQUAL("Unknown", str);
}

STAR_TEST_CASE(bus_type, config_type_verification_i2c)
{
  star_bus_config_t* config = star_bus_config_create_i2c("test",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_i2c, config->type);
  const char* str = star_bus_type_to_string(config->type);
  STAR_ASSERT_STR_EQUAL("I2C", str);
  star_bus_config_destroy(config);
}

STAR_TEST_CASE(bus_type, config_type_verification_spi)
{
  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device("test",
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  const char* str = star_bus_type_to_string(config->type);
  STAR_ASSERT_STR_EQUAL("SPI", str);
  star_bus_config_destroy(config);
}

/* ========================================================================
 * I2C/SPI API Parameter Validation Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(bus_api, i2c_write_null_manager)
{
  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_i2c_write(NULL, "test", &data, 1, 0x00, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(bus_api, i2c_write_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_i2c_write(&manager, NULL, &data, 1, 0x00, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_api, i2c_write_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_i2c_write(&manager, "test", NULL, 1, 0x00, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_api, i2c_write_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_i2c_write(&manager, "test", &data, 0, 0x00, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_api, spi_transmit_null_manager)
{
  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_transmit(NULL, "test", &data, 1, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(bus_api, spi_transmit_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_transmit(&manager, NULL, &data, 1, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_api, spi_transmit_null_buffer)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_spi_transmit(&manager, "test", NULL, 1, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(bus_api, spi_transmit_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_transmit(&manager, "test", &data, 0, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * Pin Validator Integration Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(pin_validation, i2c_pins_registered_on_add)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Add bus - should register pins */
  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Verify pins are valid (no conflicts) */
  esp_err_t validation = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, spi_pins_registered_on_add)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config = star_bus_config_create_spi_device("test_spi",
                                                                SPI2_HOST,
                                                                TEST_SPI_COPI_PIN,
                                                                TEST_SPI_CIPO_PIN,
                                                                TEST_SPI_SCLK_PIN,
                                                                0,
                                                                &dev_cfg);
  STAR_ASSERT_NOT_NULL(config);

  /* Add bus - should register pins */
  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Verify pins are valid (no conflicts) */
  esp_err_t validation = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, i2c_shared_pins_allowed)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Add first I2C bus on port 0 */
  star_bus_config_t* config1 = star_bus_config_create_i2c("i2c_0",
                                                          I2C_NUM_0,
                                                          0x3C,
                                                          TEST_I2C_SDA_PIN,
                                                          TEST_I2C_SCL_PIN,
                                                          TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config1);
  esp_err_t result1 = star_bus_manager_add_bus(&manager, config1);
  STAR_ASSERT_EQUAL(ESP_OK, result1);

  /* Add second I2C bus on port 1 using SAME pins (should work - I2C pins are shareable) */
  star_bus_config_t* config2 = star_bus_config_create_i2c("i2c_1",
                                                          I2C_NUM_1,
                                                          0x3D,
                                                          TEST_I2C_SDA_PIN,
                                                          TEST_I2C_SCL_PIN,
                                                          TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config2);
  esp_err_t result2 = star_bus_manager_add_bus(&manager, config2);
  STAR_ASSERT_EQUAL(ESP_OK, result2);

  /* Validation should pass - shared pins are allowed */
  esp_err_t validation = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, spi_cs_not_shareable)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Add first SPI device with CS on pin 5 */
  spi_device_interface_config_t dev_cfg1 = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config1 = star_bus_config_create_spi_device("spi_dev_0",
                                                                 SPI2_HOST,
                                                                 TEST_SPI_COPI_PIN,
                                                                 TEST_SPI_CIPO_PIN,
                                                                 TEST_SPI_SCLK_PIN,
                                                                 0,
                                                                 &dev_cfg1);
  STAR_ASSERT_NOT_NULL(config1);
  esp_err_t result1 = star_bus_manager_add_bus(&manager, config1);
  STAR_ASSERT_EQUAL(ESP_OK, result1);

  /* Try to add second SPI device with SAME CS pin (should register but fail validation) */
  spi_device_interface_config_t dev_cfg2 = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN, /* Same CS pin! */
    .queue_size     = 1,
  };

  star_bus_config_t* config2 = star_bus_config_create_spi_device("spi_dev_1",
                                                                 SPI2_HOST,
                                                                 TEST_SPI_COPI_PIN,
                                                                 TEST_SPI_CIPO_PIN,
                                                                 TEST_SPI_SCLK_PIN,
                                                                 0,
                                                                 &dev_cfg2);
  STAR_ASSERT_NOT_NULL(config2);

  /* star_bus_manager_add_bus will try to register the pin, which should fail */
  /* The pin registration will fail because CS pins are not shareable */
  esp_err_t result2 = star_bus_manager_add_bus(&manager, config2);

  /* If add failed, config2 is not managed and we need to destroy it */
  if (result2 != ESP_OK) {
    star_bus_config_destroy(config2);
  }
  /* Otherwise, manager will handle cleanup */

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, pins_unregistered_on_remove)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Add I2C bus */
  star_bus_config_t* config = star_bus_config_create_i2c("test_i2c",
                                                         I2C_NUM_0,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);
  esp_err_t add_result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, add_result);

  /* Verify pins are registered */
  esp_err_t validation1 = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation1);

  /* Remove bus - should unregister pins */
  esp_err_t remove_result = star_bus_manager_remove_bus(&manager, "test_i2c");
  STAR_ASSERT_EQUAL(ESP_OK, remove_result);

  /* Try to register same pin again - should work since pins were freed */
  esp_err_t rereg = star_register_pin(TEST_I2C_SDA_PIN, "test: reuse", false);
  STAR_ASSERT_EQUAL(ESP_OK, rereg);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, pins_cleaned_on_deinit)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Add multiple buses */
  star_bus_config_t* config1 = star_bus_config_create_i2c("i2c_test",
                                                          I2C_NUM_0,
                                                          TEST_I2C_ADDR,
                                                          TEST_I2C_SDA_PIN,
                                                          TEST_I2C_SCL_PIN,
                                                          TEST_I2C_CLOCK);
  star_bus_manager_add_bus(&manager, config1);

  spi_device_interface_config_t dev_cfg = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = TEST_SPI_CS_PIN,
    .queue_size     = 1,
  };

  star_bus_config_t* config2 = star_bus_config_create_spi_device("spi_test",
                                                                 SPI2_HOST,
                                                                 TEST_SPI_COPI_PIN,
                                                                 TEST_SPI_CIPO_PIN,
                                                                 TEST_SPI_SCLK_PIN,
                                                                 0,
                                                                 &dev_cfg);
  star_bus_manager_add_bus(&manager, config2);

  /* Deinit should unregister all pins */
  esp_err_t deinit_result = star_bus_manager_deinit(&manager);
  STAR_ASSERT_EQUAL(ESP_OK, deinit_result);

  /* Try to register pin after deinit - should work */
  esp_err_t rereg = star_register_pin(TEST_I2C_SDA_PIN, "test: after_deinit", false);
  STAR_ASSERT_EQUAL(ESP_OK, rereg);

  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, spi_bus_pins_shareable)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Add first SPI device */
  spi_device_interface_config_t dev_cfg1 = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = GPIO_NUM_5,
    .queue_size     = 1,
  };

  star_bus_config_t* config1 = star_bus_config_create_spi_device("spi_dev_0",
                                                                 SPI2_HOST,
                                                                 TEST_SPI_COPI_PIN,
                                                                 TEST_SPI_CIPO_PIN,
                                                                 TEST_SPI_SCLK_PIN,
                                                                 0,
                                                                 &dev_cfg1);
  STAR_ASSERT_NOT_NULL(config1);
  esp_err_t result1 = star_bus_manager_add_bus(&manager, config1);
  STAR_ASSERT_EQUAL(ESP_OK, result1);

  /* Add second device on same SPI host with different CS */
  spi_device_interface_config_t dev_cfg2 = {
    .mode           = 0,
    .clock_speed_hz = TEST_SPI_CLOCK,
    .spics_io_num   = GPIO_NUM_17, /* Different CS pin */
    .queue_size     = 1,
  };

  star_bus_config_t* config2 = star_bus_config_create_spi_device("spi_dev_1",
                                                                 SPI2_HOST,
                                                                 TEST_SPI_COPI_PIN, /* Same COPI */
                                                                 TEST_SPI_CIPO_PIN, /* Same CIPO */
                                                                 TEST_SPI_SCLK_PIN, /* Same SCLK */
                                                                 0,
                                                                 &dev_cfg2);
  STAR_ASSERT_NOT_NULL(config2);
  esp_err_t result2 = star_bus_manager_add_bus(&manager, config2);
  STAR_ASSERT_EQUAL(ESP_OK, result2);

  /* Validation should pass - bus pins are shareable, CS pins are different */
  esp_err_t validation = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, invalid_pin_rejected)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  /* Try to register invalid GPIO number */
  esp_err_t result = star_register_pin(GPIO_NUM_MAX, "invalid_pin", false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, null_description_rejected)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  /* Try to register with NULL description */
  esp_err_t result = star_register_pin(TEST_I2C_SDA_PIN, NULL, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_free_pin_validator();
}

STAR_TEST_CASE(pin_validation, unregister_nonexistent_pin)
{
  /* Clean up any previous pin registrations */
  star_free_pin_validator();

  /* Try to unregister a pin that was never registered */
  esp_err_t result = star_unregister_pin(TEST_I2C_SDA_PIN, "nonexistent");
  STAR_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);

  star_free_pin_validator();
}

/* ========================================================================
 * Error Handler Integration Tests (8 tests)
 * ======================================================================== */

/* Test reset callback that succeeds */
static esp_err_t test_reset_success(void* context)
{
  (void)context;
  return ESP_OK;
}

/* Test reset callback that tracks call count */
static int       reset_call_count = 0;
static esp_err_t test_reset_counter(void* context)
{
  (void)context;
  reset_call_count++;
  return ESP_OK;
}

STAR_TEST_CASE(error_handler, initialized_on_manager_init)
{
  star_bus_manager_t manager;
  esp_err_t          result = star_bus_manager_init(&manager, "TestMgr");
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Verify error handler was initialized */
  STAR_ASSERT_NOT_NULL(manager.error_handler.mutex);
  STAR_ASSERT_EQUAL(0, manager.error_handler.error_count);
  STAR_ASSERT_FALSE(manager.error_handler.in_error_state);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(error_handler, deinitialized_on_manager_deinit)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Verify mutex exists before deinit */
  STAR_ASSERT_NOT_NULL(manager.error_handler.mutex);

  esp_err_t result = star_bus_manager_deinit(&manager);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* After deinit, mutex should be NULL */
  STAR_ASSERT_NULL(manager.error_handler.mutex);
}

STAR_TEST_CASE(error_handler, set_reset_function_success)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Initially, reset function should be NULL */
  STAR_ASSERT_NULL(manager.error_handler.reset_fn);

  /* Set a reset function */
  esp_err_t result = star_bus_manager_set_error_reset_fn(&manager, test_reset_success, NULL);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(manager.error_handler.reset_fn);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(error_handler, clear_reset_function)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Set a reset function */
  star_bus_manager_set_error_reset_fn(&manager, test_reset_success, NULL);
  STAR_ASSERT_NOT_NULL(manager.error_handler.reset_fn);

  /* Clear the reset function */
  esp_err_t result = star_bus_manager_set_error_reset_fn(&manager, NULL, NULL);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NULL(manager.error_handler.reset_fn);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(error_handler, set_reset_function_null_manager)
{
  esp_err_t result = star_bus_manager_set_error_reset_fn(NULL, test_reset_success, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(error_handler, error_recorded_on_bus_init_failure)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Create I2C config with invalid port to force initialization failure */
  star_bus_config_t* config = star_bus_config_create_i2c("bad_i2c",
                                                         (i2c_port_t)999, /* Invalid port */
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  STAR_ASSERT_NOT_NULL(config);

  /* Check error handler state before add attempt */
  STAR_ASSERT_FALSE(manager.error_handler.in_error_state);
  STAR_ASSERT_EQUAL(0, manager.error_handler.error_count);

  /* Try to add bus - should fail and record error */
  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* Should fail */

  /* Verify error was recorded */
  STAR_ASSERT_TRUE(manager.error_handler.in_error_state);
  STAR_ASSERT_EQUAL(1, manager.error_handler.error_count);
  STAR_ASSERT_NOT_EQUAL(ESP_OK, manager.error_handler.last_error);

  star_bus_config_destroy(config);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(error_handler, reset_state_clears_error)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Simulate an error by trying to add invalid bus */
  star_bus_config_t* config = star_bus_config_create_i2c("bad_i2c",
                                                         (i2c_port_t)999,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);
  star_bus_manager_add_bus(&manager, config);

  /* Verify error state is set */
  STAR_ASSERT_TRUE(manager.error_handler.in_error_state);

  /* Reset error state */
  esp_err_t result = error_handler_reset_state(&manager.error_handler);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  /* Verify error state is cleared */
  STAR_ASSERT_FALSE(manager.error_handler.in_error_state);
  STAR_ASSERT_EQUAL(0, manager.error_handler.error_count);
  STAR_ASSERT_EQUAL(ESP_OK, manager.error_handler.last_error);

  star_bus_config_destroy(config);
  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(error_handler, reset_callback_can_clear_errors)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Set a successful reset function and track calls */
  reset_call_count = 0;
  star_bus_manager_set_error_reset_fn(&manager, test_reset_counter, NULL);

  /* Trigger multiple errors to exhaust retries */
  star_bus_config_t* config = star_bus_config_create_i2c("bad_i2c",
                                                         (i2c_port_t)999,
                                                         TEST_I2C_ADDR,
                                                         TEST_I2C_SDA_PIN,
                                                         TEST_I2C_SCL_PIN,
                                                         TEST_I2C_CLOCK);

  /* Attempt to add multiple times to trigger retry exhaustion */
  for (int i = 0; i < 5; i++) {
    star_bus_manager_add_bus(&manager, config);
  }

  /* Verify reset was called at least once (after max retries) */
  STAR_ASSERT_TRUE(reset_call_count > 0);

  star_bus_config_destroy(config);
  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * SPI Peripheral Mode Tests (18 tests)
 * ======================================================================== */

STAR_TEST_CASE(spi_peripheral, create_valid_config)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    0);
  STAR_ASSERT_NOT_NULL(config);
  STAR_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  STAR_ASSERT_STR_EQUAL("test_peripheral", config->name);
  STAR_ASSERT_TRUE(config->proto.spi.is_peripheral);
  STAR_ASSERT_EQUAL(3, config->proto.spi.queue_size);
  STAR_ASSERT_EQUAL(0, config->proto.spi.mode);
  STAR_ASSERT_FALSE(config->initialized);
  star_bus_config_destroy(config);
}

STAR_TEST_CASE(spi_peripheral, create_null_name)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral(NULL,
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    0);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(spi_peripheral, create_invalid_host)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    (spi_host_device_t)999,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    0);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(spi_peripheral, create_invalid_queue_size_zero)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    0,
                                                                    0);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(spi_peripheral, create_invalid_queue_size_large)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    11,
                                                                    0);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(spi_peripheral, create_invalid_mode)
{
  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    4);
  STAR_ASSERT_NULL(config);
}

STAR_TEST_CASE(spi_peripheral, create_all_modes)
{
  for (uint8_t mode = 0; mode <= 3; mode++) {
    star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                      SPI2_HOST,
                                                                      TEST_SPI_COPI_PIN,
                                                                      TEST_SPI_CIPO_PIN,
                                                                      TEST_SPI_SCLK_PIN,
                                                                      TEST_SPI_CS_PIN,
                                                                      3,
                                                                      mode);
    STAR_ASSERT_NOT_NULL(config);
    STAR_ASSERT_EQUAL(mode, config->proto.spi.mode);
    star_bus_config_destroy(config);
  }
}

STAR_TEST_CASE(spi_peripheral, add_to_manager)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    0);
  STAR_ASSERT_NOT_NULL(config);

  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_TRUE(config->initialized);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, pins_registered_on_add)
{
  star_free_pin_validator();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  star_bus_config_t* config = star_bus_config_create_spi_peripheral("test_peripheral",
                                                                    SPI2_HOST,
                                                                    TEST_SPI_COPI_PIN,
                                                                    TEST_SPI_CIPO_PIN,
                                                                    TEST_SPI_SCLK_PIN,
                                                                    TEST_SPI_CS_PIN,
                                                                    3,
                                                                    0);
  STAR_ASSERT_NOT_NULL(config);

  esp_err_t result = star_bus_manager_add_bus(&manager, config);
  STAR_ASSERT_EQUAL(ESP_OK, result);

  esp_err_t validation = star_validate_pins();
  STAR_ASSERT_EQUAL(ESP_OK, validation);

  star_bus_manager_deinit(&manager);
  star_free_pin_validator();
}

STAR_TEST_CASE(spi_peripheral, receive_null_manager)
{
  uint8_t   buffer[4];
  esp_err_t result = star_bus_spi_peripheral_receive(NULL, "test", buffer, sizeof(buffer), 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(spi_peripheral, receive_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   buffer[4];
  esp_err_t result = star_bus_spi_peripheral_receive(&manager, NULL, buffer, sizeof(buffer), 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, receive_null_buffer)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_spi_peripheral_receive(&manager, "test", NULL, 4, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, receive_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   buffer[4];
  esp_err_t result = star_bus_spi_peripheral_receive(&manager, "test", buffer, 0, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transmit_null_manager)
{
  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_peripheral_transmit(NULL, "test", &data, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(spi_peripheral, transmit_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_peripheral_transmit(&manager, NULL, &data, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transmit_null_buffer)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_spi_peripheral_transmit(&manager, "test", NULL, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transmit_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data   = 0x42;
  esp_err_t result = star_bus_spi_peripheral_transmit(&manager, "test", &data, 0, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transceive_null_manager)
{
  uint8_t   tx_data = 0x42;
  uint8_t   rx_data = 0x00;
  esp_err_t result  = star_bus_spi_peripheral_transceive(NULL, "test", &tx_data, &rx_data, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(spi_peripheral, transceive_null_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   tx_data = 0x42;
  uint8_t   rx_data = 0x00;
  esp_err_t result = star_bus_spi_peripheral_transceive(&manager, NULL, &tx_data, &rx_data, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transceive_both_buffers_null)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_bus_spi_peripheral_transceive(&manager, "test", NULL, NULL, 1, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(spi_peripheral, transceive_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   tx_data = 0x42;
  uint8_t   rx_data = 0x00;
  esp_err_t result =
    star_bus_spi_peripheral_transceive(&manager, "test", &tx_data, &rx_data, 0, 100);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* ========================================================================
 * SMBus Protocol Tests (37 tests)
 * ======================================================================== */

/* --- Quick Command Tests (2 tests) --- */

STAR_TEST_CASE(smbus, quick_command_write)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* No I2C bus available for actual communication, just test parameter validation */
  esp_err_t result = star_smbus_quick_command(&manager, "nonexistent", 0x3C, true);
  STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* Should fail - no bus */

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, quick_command_null_manager)
{
  esp_err_t result = star_smbus_quick_command(NULL, "test", 0x3C, true);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

/* --- Send/Receive Byte Tests (4 tests) --- */

STAR_TEST_CASE(smbus, send_byte_null_manager)
{
  esp_err_t result = star_smbus_send_byte(NULL, "test", 0x3C, 0x42);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, send_byte_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_send_byte(&manager, NULL, 0x3C, 0x42);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, receive_byte_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_receive_byte(&manager, "test", 0x3C, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, receive_byte_null_manager)
{
  uint8_t   data;
  esp_err_t result = star_smbus_receive_byte(NULL, "test", 0x3C, &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

/* --- Write/Read Byte Tests (6 tests) --- */

STAR_TEST_CASE(smbus, write_byte_null_manager)
{
  esp_err_t result = star_smbus_write_byte(NULL, "test", 0x3C, 0x10, 0x42);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, write_byte_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_write_byte(&manager, NULL, 0x3C, 0x10, 0x42);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, write_byte_various_commands)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Test different command codes (should all fail - no bus, but validates params) */
  uint8_t commands[] = {0x00, 0x10, 0x7F, 0xFF};
  for (int i = 0; i < 4; i++) {
    esp_err_t result = star_smbus_write_byte(&manager, "test", 0x3C, commands[i], 0x42);
    STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* No bus exists */
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, read_byte_null_manager)
{
  uint8_t   data;
  esp_err_t result = star_smbus_read_byte(NULL, "test", 0x3C, 0x10, &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, read_byte_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_read_byte(&manager, "test", 0x3C, 0x10, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, read_byte_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data;
  esp_err_t result = star_smbus_read_byte(&manager, NULL, 0x3C, 0x10, &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* --- Write/Read Word Tests (6 tests) --- */

STAR_TEST_CASE(smbus, write_word_null_manager)
{
  esp_err_t result = star_smbus_write_word(NULL, "test", 0x3C, 0x10, 0x1234);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, write_word_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_write_word(&manager, NULL, 0x3C, 0x10, 0x1234);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, write_word_little_endian)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Test various word values (validates params, no actual I2C) */
  uint16_t words[] = {0x0000, 0x1234, 0x5678, 0xABCD, 0xFFFF};
  for (int i = 0; i < 5; i++) {
    esp_err_t result = star_smbus_write_word(&manager, "test", 0x3C, 0x10, words[i]);
    STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* No bus exists */
  }

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, read_word_null_manager)
{
  uint16_t  data;
  esp_err_t result = star_smbus_read_word(NULL, "test", 0x3C, 0x10, &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, read_word_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_read_word(&manager, "test", 0x3C, 0x10, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, read_word_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint16_t  data;
  esp_err_t result = star_smbus_read_word(&manager, NULL, 0x3C, 0x10, &data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* --- Process Call Tests (4 tests) --- */

STAR_TEST_CASE(smbus, process_call_null_manager)
{
  uint16_t  read_data;
  esp_err_t result = star_smbus_process_call(NULL, "test", 0x3C, 0x10, 0x1234, &read_data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, process_call_null_read_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_process_call(&manager, "test", 0x3C, 0x10, 0x1234, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, process_call_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint16_t  read_data;
  esp_err_t result = star_smbus_process_call(&manager, NULL, 0x3C, 0x10, 0x1234, &read_data);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, process_call_various_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  /* Test various data values */
  uint16_t write_values[] = {0x0000, 0xAAAA, 0x5555, 0xFFFF};
  for (int i = 0; i < 4; i++) {
    uint16_t  read_data;
    esp_err_t result =
      star_smbus_process_call(&manager, "test", 0x3C, 0x10, write_values[i], &read_data);
    STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* No bus exists */
  }

  star_bus_manager_deinit(&manager);
}

/* --- Block Write Tests (5 tests) --- */

STAR_TEST_CASE(smbus, block_write_null_manager)
{
  uint8_t   data[] = {0x01, 0x02, 0x03};
  esp_err_t result = star_smbus_block_write(NULL, "test", 0x3C, 0x10, data, 3);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, block_write_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  esp_err_t result = star_smbus_block_write(&manager, "test", 0x3C, 0x10, NULL, 3);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_write_zero_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data[] = {0x01, 0x02, 0x03};
  esp_err_t result = star_smbus_block_write(&manager, "test", 0x3C, 0x10, data, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_write_oversized)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data[33];
  esp_err_t result = star_smbus_block_write(&manager, "test", 0x3C, 0x10, data, 33);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_write_max_size)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t data[STAR_SMBUS_MAX_BLOCK_SIZE];
  for (int i = 0; i < STAR_SMBUS_MAX_BLOCK_SIZE; i++) {
    data[i] = i;
  }
  esp_err_t result =
    star_smbus_block_write(&manager, "test", 0x3C, 0x10, data, STAR_SMBUS_MAX_BLOCK_SIZE);
  STAR_ASSERT_NOT_EQUAL(ESP_OK, result); /* No bus exists, but params are valid */

  star_bus_manager_deinit(&manager);
}

/* --- Block Read Tests (5 tests) --- */

STAR_TEST_CASE(smbus, block_read_null_manager)
{
  uint8_t   data[32];
  uint8_t   length;
  esp_err_t result = star_smbus_block_read(NULL, "test", 0x3C, 0x10, data, 32, &length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, block_read_null_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   length;
  esp_err_t result = star_smbus_block_read(&manager, "test", 0x3C, 0x10, NULL, 32, &length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_read_null_length)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data[32];
  esp_err_t result = star_smbus_block_read(&manager, "test", 0x3C, 0x10, data, 32, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_read_buffer_too_small)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data[16]; /* Too small! */
  uint8_t   length;
  esp_err_t result = star_smbus_block_read(&manager, "test", 0x3C, 0x10, data, 16, &length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_read_null_bus_name)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   data[32];
  uint8_t   length;
  esp_err_t result = star_smbus_block_read(&manager, NULL, 0x3C, 0x10, data, 32, &length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* --- Block Process Call Tests (5 tests) --- */

STAR_TEST_CASE(smbus, block_process_call_null_manager)
{
  uint8_t   write_data[] = {0x01, 0x02};
  uint8_t   read_data[32];
  uint8_t   read_length;
  esp_err_t result = star_smbus_block_process_call(NULL,
                                                   "test",
                                                   0x3C,
                                                   0x10,
                                                   write_data,
                                                   2,
                                                   read_data,
                                                   32,
                                                   &read_length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(smbus, block_process_call_null_write_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   read_data[32];
  uint8_t   read_length;
  esp_err_t result = star_smbus_block_process_call(&manager,
                                                   "test",
                                                   0x3C,
                                                   0x10,
                                                   NULL,
                                                   2,
                                                   read_data,
                                                   32,
                                                   &read_length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_process_call_null_read_data)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   write_data[] = {0x01, 0x02};
  uint8_t   read_length;
  esp_err_t result = star_smbus_block_process_call(&manager,
                                                   "test",
                                                   0x3C,
                                                   0x10,
                                                   write_data,
                                                   2,
                                                   NULL,
                                                   32,
                                                   &read_length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_process_call_write_oversized)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   write_data[33];
  uint8_t   read_data[32];
  uint8_t   read_length;
  esp_err_t result = star_smbus_block_process_call(&manager,
                                                   "test",
                                                   0x3C,
                                                   0x10,
                                                   write_data,
                                                   33,
                                                   read_data,
                                                   32,
                                                   &read_length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

STAR_TEST_CASE(smbus, block_process_call_read_buffer_too_small)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr");

  uint8_t   write_data[] = {0x01, 0x02};
  uint8_t   read_data[16]; /* Too small! */
  uint8_t   read_length;
  esp_err_t result = star_smbus_block_process_call(&manager,
                                                   "test",
                                                   0x3C,
                                                   0x10,
                                                   write_data,
                                                   2,
                                                   read_data,
                                                   16,
                                                   &read_length);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* --- PEC Calculation Tests (3 tests) --- */

STAR_TEST_CASE(smbus, pec_empty_data)
{
  uint8_t pec = star_smbus_calculate_pec(NULL, 0, 0);
  STAR_ASSERT_EQUAL(0, pec); /* Should return initial CRC unchanged */
}

STAR_TEST_CASE(smbus, pec_single_byte)
{
  uint8_t data = 0x42;
  uint8_t pec  = star_smbus_calculate_pec(&data, 1, 0);
  STAR_ASSERT_NOT_EQUAL(0, pec);    /* Should produce non-zero CRC */
  STAR_ASSERT_NOT_EQUAL(0x42, pec); /* Should not equal input */
}

STAR_TEST_CASE(smbus, pec_multiple_bytes_and_chaining)
{
  /* Test CRC calculation and chaining */
  uint8_t data1[] = {0x01, 0x02, 0x03};
  uint8_t data2[] = {0x04, 0x05, 0x06};

  /* Calculate in one pass */
  uint8_t full_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  uint8_t pec_full    = star_smbus_calculate_pec(full_data, 6, 0);

  /* Calculate in two passes (chained) */
  uint8_t pec_chain = star_smbus_calculate_pec(data1, 3, 0);
  pec_chain         = star_smbus_calculate_pec(data2, 3, pec_chain);

  /* Chained calculation should match full calculation */
  STAR_ASSERT_EQUAL(pec_full, pec_chain);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()
/* Bus Manager Lifecycle Tests */
STAR_TEST_REF(bus_manager, init_valid)
STAR_TEST_REF(bus_manager, init_null_pointer)
STAR_TEST_REF(bus_manager, init_null_tag)
STAR_TEST_REF(bus_manager, init_empty_tag)
STAR_TEST_REF(bus_manager, init_custom_tag)
STAR_TEST_REF(bus_manager, deinit_valid)
STAR_TEST_REF(bus_manager, deinit_null_pointer)
STAR_TEST_REF(bus_manager, init_deinit_multiple_times)

/* Bus Configuration Creation Tests */
STAR_TEST_REF(bus_config, create_i2c_valid)
STAR_TEST_REF(bus_config, create_i2c_null_name)
STAR_TEST_REF(bus_config, create_i2c_various_speeds)
STAR_TEST_REF(bus_config, create_spi_valid)
STAR_TEST_REF(bus_config, create_spi_null_name)
STAR_TEST_REF(bus_config, create_spi_null_dev_cfg)
STAR_TEST_REF(bus_config, create_spi_invalid_host)
STAR_TEST_REF(bus_config, destroy_valid_config)
STAR_TEST_REF(bus_config, destroy_null_config)
STAR_TEST_REF(bus_config, deinit_uninitialized_config)

/* Bus Manager Operations Tests */
STAR_TEST_REF(bus_operations, add_i2c_bus)
STAR_TEST_REF(bus_operations, add_spi_bus)
STAR_TEST_REF(bus_operations, add_null_manager)
STAR_TEST_REF(bus_operations, add_null_config)
STAR_TEST_REF(bus_operations, add_duplicate_name)
STAR_TEST_REF(bus_operations, find_existing_bus)
STAR_TEST_REF(bus_operations, find_nonexistent_bus)
STAR_TEST_REF(bus_operations, find_null_manager)
STAR_TEST_REF(bus_operations, find_null_name)
STAR_TEST_REF(bus_operations, remove_existing_bus)
STAR_TEST_REF(bus_operations, remove_nonexistent_bus)
STAR_TEST_REF(bus_operations, add_multiple_buses)

/* Bus Type Utility Tests */
STAR_TEST_REF(bus_type, to_string_none)
STAR_TEST_REF(bus_type, to_string_i2c)
STAR_TEST_REF(bus_type, to_string_spi)
STAR_TEST_REF(bus_type, to_string_invalid_low)
STAR_TEST_REF(bus_type, to_string_invalid_high)
STAR_TEST_REF(bus_type, to_string_count_boundary)
STAR_TEST_REF(bus_type, config_type_verification_i2c)
STAR_TEST_REF(bus_type, config_type_verification_spi)

/* I2C/SPI API Parameter Validation Tests */
STAR_TEST_REF(bus_api, i2c_write_null_manager)
STAR_TEST_REF(bus_api, i2c_write_null_name)
STAR_TEST_REF(bus_api, i2c_write_null_data)
STAR_TEST_REF(bus_api, i2c_write_zero_length)
STAR_TEST_REF(bus_api, spi_transmit_null_manager)
STAR_TEST_REF(bus_api, spi_transmit_null_name)
STAR_TEST_REF(bus_api, spi_transmit_null_buffer)
STAR_TEST_REF(bus_api, spi_transmit_zero_length)

/* Pin Validator Integration Tests */
STAR_TEST_REF(pin_validation, i2c_pins_registered_on_add)
STAR_TEST_REF(pin_validation, spi_pins_registered_on_add)
STAR_TEST_REF(pin_validation, i2c_shared_pins_allowed)
STAR_TEST_REF(pin_validation, spi_cs_not_shareable)
STAR_TEST_REF(pin_validation, pins_unregistered_on_remove)
STAR_TEST_REF(pin_validation, pins_cleaned_on_deinit)
STAR_TEST_REF(pin_validation, spi_bus_pins_shareable)
STAR_TEST_REF(pin_validation, invalid_pin_rejected)
STAR_TEST_REF(pin_validation, null_description_rejected)
STAR_TEST_REF(pin_validation, unregister_nonexistent_pin)

/* Error Handler Integration Tests */
STAR_TEST_REF(error_handler, initialized_on_manager_init)
STAR_TEST_REF(error_handler, deinitialized_on_manager_deinit)
STAR_TEST_REF(error_handler, set_reset_function_success)
STAR_TEST_REF(error_handler, clear_reset_function)
STAR_TEST_REF(error_handler, set_reset_function_null_manager)
STAR_TEST_REF(error_handler, error_recorded_on_bus_init_failure)
STAR_TEST_REF(error_handler, reset_state_clears_error)
STAR_TEST_REF(error_handler, reset_callback_can_clear_errors)

/* SPI Peripheral Mode Tests */
STAR_TEST_REF(spi_peripheral, create_valid_config)
STAR_TEST_REF(spi_peripheral, create_null_name)
STAR_TEST_REF(spi_peripheral, create_invalid_host)
STAR_TEST_REF(spi_peripheral, create_invalid_queue_size_zero)
STAR_TEST_REF(spi_peripheral, create_invalid_queue_size_large)
STAR_TEST_REF(spi_peripheral, create_invalid_mode)
STAR_TEST_REF(spi_peripheral, create_all_modes)
STAR_TEST_REF(spi_peripheral, add_to_manager)
STAR_TEST_REF(spi_peripheral, pins_registered_on_add)
STAR_TEST_REF(spi_peripheral, receive_null_manager)
STAR_TEST_REF(spi_peripheral, receive_null_name)
STAR_TEST_REF(spi_peripheral, receive_null_buffer)
STAR_TEST_REF(spi_peripheral, receive_zero_length)
STAR_TEST_REF(spi_peripheral, transmit_null_manager)
STAR_TEST_REF(spi_peripheral, transmit_null_name)
STAR_TEST_REF(spi_peripheral, transmit_null_buffer)
STAR_TEST_REF(spi_peripheral, transmit_zero_length)
STAR_TEST_REF(spi_peripheral, transceive_null_manager)
STAR_TEST_REF(spi_peripheral, transceive_null_name)
STAR_TEST_REF(spi_peripheral, transceive_both_buffers_null)
STAR_TEST_REF(spi_peripheral, transceive_zero_length)

/* SMBus Protocol Tests */
STAR_TEST_REF(smbus, quick_command_write)
STAR_TEST_REF(smbus, quick_command_null_manager)
STAR_TEST_REF(smbus, send_byte_null_manager)
STAR_TEST_REF(smbus, send_byte_null_bus_name)
STAR_TEST_REF(smbus, receive_byte_null_data)
STAR_TEST_REF(smbus, receive_byte_null_manager)
STAR_TEST_REF(smbus, write_byte_null_manager)
STAR_TEST_REF(smbus, write_byte_null_bus_name)
STAR_TEST_REF(smbus, write_byte_various_commands)
STAR_TEST_REF(smbus, read_byte_null_manager)
STAR_TEST_REF(smbus, read_byte_null_data)
STAR_TEST_REF(smbus, read_byte_null_bus_name)
STAR_TEST_REF(smbus, write_word_null_manager)
STAR_TEST_REF(smbus, write_word_null_bus_name)
STAR_TEST_REF(smbus, write_word_little_endian)
STAR_TEST_REF(smbus, read_word_null_manager)
STAR_TEST_REF(smbus, read_word_null_data)
STAR_TEST_REF(smbus, read_word_null_bus_name)
STAR_TEST_REF(smbus, process_call_null_manager)
STAR_TEST_REF(smbus, process_call_null_read_data)
STAR_TEST_REF(smbus, process_call_null_bus_name)
STAR_TEST_REF(smbus, process_call_various_data)
STAR_TEST_REF(smbus, block_write_null_manager)
STAR_TEST_REF(smbus, block_write_null_data)
STAR_TEST_REF(smbus, block_write_zero_length)
STAR_TEST_REF(smbus, block_write_oversized)
STAR_TEST_REF(smbus, block_write_max_size)
STAR_TEST_REF(smbus, block_read_null_manager)
STAR_TEST_REF(smbus, block_read_null_data)
STAR_TEST_REF(smbus, block_read_null_length)
STAR_TEST_REF(smbus, block_read_buffer_too_small)
STAR_TEST_REF(smbus, block_read_null_bus_name)
STAR_TEST_REF(smbus, block_process_call_null_manager)
STAR_TEST_REF(smbus, block_process_call_null_write_data)
STAR_TEST_REF(smbus, block_process_call_null_read_data)
STAR_TEST_REF(smbus, block_process_call_write_oversized)
STAR_TEST_REF(smbus, block_process_call_read_buffer_too_small)
STAR_TEST_REF(smbus, pec_empty_data)
STAR_TEST_REF(smbus, pec_single_byte)
STAR_TEST_REF(smbus, pec_multiple_bytes_and_chaining)
STAR_TEST_LIST_END()
