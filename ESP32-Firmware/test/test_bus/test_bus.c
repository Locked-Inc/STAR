/* Test bus manager component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_types.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "driver/gpio.h"

/* Thread safety test globals */
static star_bus_manager_t *s_shared_manager = NULL;
static volatile int s_thread_errors = 0;
static volatile int s_tasks_completed = 0;

void setUp(void) {}
void tearDown(void) {
  /* Clean up pin validator between tests */
  star_free_pin_validator();
}

/* Manager Lifecycle Tests */

void test_manager_init_valid(void)
{
  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  star_bus_manager_deinit(&manager);
}

void test_manager_init_null_pointer(void)
{
  esp_err_t result = star_bus_manager_init(NULL, "TestMgr", NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_manager_init_null_tag(void)
{
  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, NULL, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_NOT_NULL(manager.tag);
  star_bus_manager_deinit(&manager);
}

void test_manager_init_empty_tag(void)
{
  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "", NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  star_bus_manager_deinit(&manager);
}

void test_manager_deinit_valid(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  esp_err_t result = star_bus_manager_deinit(&manager);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_manager_deinit_null_pointer(void)
{
  esp_err_t result = star_bus_manager_deinit(NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

/* Type String Tests */

void test_bus_type_string_i2c(void)
{
  const char * str = star_bus_type_to_string(k_star_bus_type_i2c);
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("I2C", str);
}

void test_bus_type_string_spi(void)
{
  const char * str = star_bus_type_to_string(k_star_bus_type_spi);
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("SPI", str);
}

void test_bus_type_string_none(void)
{
  const char * str = star_bus_type_to_string(k_star_bus_type_none);
  TEST_ASSERT_NOT_NULL(str);
}

/* Find Bus Tests */

void test_find_bus_null_manager(void)
{
  star_bus_config_t * config = star_bus_manager_find_bus(NULL, "test");
  TEST_ASSERT_NULL(config);
}

void test_find_bus_null_name(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  star_bus_config_t * config = star_bus_manager_find_bus(&manager, NULL);
  TEST_ASSERT_NULL(config);

  star_bus_manager_deinit(&manager);
}

void test_find_bus_not_found(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  star_bus_config_t * config = star_bus_manager_find_bus(&manager, "nonexistent");
  TEST_ASSERT_NULL(config);

  star_bus_manager_deinit(&manager);
}

/* Interface Injection Tests */

void test_manager_init_with_interfaces(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t error_iface;
  star_pin_interface_t pin_iface;

  error_handler_get_interface(&error_iface, &handler);
  pin_validator_get_interface(&pin_iface);

  star_bus_manager_t manager;
  esp_err_t result = star_bus_manager_init(&manager, "TestMgr", &error_iface, &pin_iface);

  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL_PTR(&error_iface, manager.error_iface);
  TEST_ASSERT_EQUAL_PTR(&pin_iface, manager.pin_iface);

  star_bus_manager_deinit(&manager);
  error_handler_deinit(&handler);
}

void test_manager_stores_tag(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "CustomTag", NULL, NULL);

  TEST_ASSERT_NOT_NULL(manager.tag);
  TEST_ASSERT_EQUAL_STRING("CustomTag", manager.tag);

  star_bus_manager_deinit(&manager);
}

void test_manager_mutex_created(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  TEST_ASSERT_NOT_NULL(manager.mutex);

  star_bus_manager_deinit(&manager);
}

void test_manager_buses_initially_null(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  TEST_ASSERT_NULL(manager.buses);

  star_bus_manager_deinit(&manager);
}

/* Config Creation Tests */

void test_create_i2c_config(void)
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

void test_create_i2c_config_null_name(void)
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

void test_create_spi_device_config(void)
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

  star_bus_config_destroy(config);
}

void test_create_spi_device_null_dev_cfg(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_device(
    "TestSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    NULL
  );

  TEST_ASSERT_NULL(config);
}

/* Add Bus Tests */

void test_add_bus_null_manager(void)
{
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);

  esp_err_t result = star_bus_manager_add_bus(NULL, config);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_config_destroy(config);
}

void test_add_bus_null_config(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_add_bus(&manager, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

/* Remove Bus Tests */

void test_remove_bus_null_manager(void)
{
  esp_err_t result = star_bus_manager_remove_bus(NULL, "TestBus");
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_remove_bus_null_name(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_remove_bus(&manager, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

void test_remove_bus_not_found(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_remove_bus(&manager, "nonexistent");
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);

  star_bus_manager_deinit(&manager);
}

/* SPI Host Tracking Tests */

void test_spi_host_tracking_initial_state(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  /* All SPI hosts should be uninitialized initially */
  for (int i = 0; i < SPI_HOST_MAX; i++) {
    TEST_ASSERT_FALSE(manager.spi_host_initialized[i]);
    TEST_ASSERT_EQUAL(0, manager.spi_device_count[i]);
  }

  star_bus_manager_deinit(&manager);
}

/* Config Destroy Tests */

void test_destroy_null_config(void)
{
  /* Should not crash */
  star_bus_config_destroy(NULL);
}

/* Type String Additional Tests */

void test_bus_type_string_invalid(void)
{
  const char * str = star_bus_type_to_string((star_bus_type_t)999);
  TEST_ASSERT_NOT_NULL(str);
  /* Should return something like "Unknown" */
}

/* with_bus API Tests */

static esp_err_t test_callback_simple(star_bus_config_t * bus, void * user_ctx)
{
  (void)user_ctx;
  if (bus == NULL) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t test_callback_read_name(star_bus_config_t * bus, void * user_ctx)
{
  char * name_out = (char *)user_ctx;
  if (bus && bus->name && name_out) {
    strcpy(name_out, bus->name);
    return ESP_OK;
  }
  return ESP_FAIL;
}

void test_with_bus_null_manager(void)
{
  esp_err_t result = star_bus_manager_with_bus(NULL, "test", test_callback_simple, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_with_bus_null_name(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_with_bus(&manager, NULL, test_callback_simple, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

void test_with_bus_null_callback(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_with_bus(&manager, "test", NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

  star_bus_manager_deinit(&manager);
}

void test_with_bus_not_found(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  esp_err_t result = star_bus_manager_with_bus(&manager, "nonexistent", test_callback_simple, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);

  star_bus_manager_deinit(&manager);
}

void test_with_bus_found_and_callback_invoked(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);

  /* Add a bus */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "TestI2C", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, config);

  /* Use with_bus to access it */
  char name_buffer[32] = {0};
  esp_err_t result = star_bus_manager_with_bus(&manager, "TestI2C", test_callback_read_name, name_buffer);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL_STRING("TestI2C", name_buffer);

  star_bus_manager_deinit(&manager);
}

/* Thread Safety Tests */

static void concurrent_find_task(void * arg)
{
  (void)arg;

  for (int i = 0; i < 20; i++) {
    star_bus_config_t * found = star_bus_manager_find_bus(s_shared_manager, "SharedBus");
    if (found == NULL) {
      /* Bus not found is OK during concurrent add/remove */
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  s_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_find_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_shared_manager = &manager;
  s_thread_errors = 0;
  s_tasks_completed = 0;

  /* Add a bus to find */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "SharedBus", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, config);

  /* Spawn multiple tasks that find the bus concurrently */
  xTaskCreate(concurrent_find_task, "f1", 2048, NULL, 5, NULL);
  xTaskCreate(concurrent_find_task, "f2", 2048, NULL, 5, NULL);
  xTaskCreate(concurrent_find_task, "f3", 2048, NULL, 5, NULL);

  /* Wait for tasks to complete */
  while (s_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, s_thread_errors);

  s_shared_manager = NULL;
  star_bus_manager_deinit(&manager);
}

static esp_err_t with_bus_increment_callback(star_bus_config_t * bus, void * user_ctx)
{
  (void)bus;
  int* counter = (int*)user_ctx;
  (*counter)++;
  return ESP_OK;
}

static void concurrent_with_bus_task(void * arg)
{
  int* counter = (int*)arg;

  for (int i = 0; i < 10; i++) {
    esp_err_t result = star_bus_manager_with_bus(s_shared_manager, "SharedBus",
                                                  with_bus_increment_callback, counter);
    if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
      s_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  s_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_with_bus(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_shared_manager = &manager;
  s_thread_errors = 0;
  s_tasks_completed = 0;

  /* Add a bus */
  star_bus_config_t * config = star_bus_config_create_i2c(
    "SharedBus", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, config);

  /* Counter to track callbacks */
  static volatile int callback_counter = 0;
  callback_counter = 0;

  /* Spawn multiple tasks that use with_bus concurrently */
  xTaskCreate(concurrent_with_bus_task, "w1", 2048, (void *)&callback_counter, 5, NULL);
  xTaskCreate(concurrent_with_bus_task, "w2", 2048, (void *)&callback_counter, 5, NULL);
  xTaskCreate(concurrent_with_bus_task, "w3", 2048, (void *)&callback_counter, 5, NULL);

  /* Wait for tasks to complete */
  while (s_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, s_thread_errors);
  /* Each of 3 tasks calls callback 10 times = 30 total */
  TEST_ASSERT_EQUAL(30, callback_counter);

  s_shared_manager = NULL;
  star_bus_manager_deinit(&manager);
}

static void concurrent_add_remove_task(void * arg)
{
  int task_id = (int)(intptr_t)arg;
  char name[32];

  for (int i = 0; i < 5; i++) {
    snprintf(name, sizeof(name), "Bus_%d_%d", task_id, i);

    /* Create and add a bus */
    star_bus_config_t * config = star_bus_config_create_i2c(
      name, I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);

    if (config != NULL) {
      esp_err_t result = star_bus_manager_add_bus(s_shared_manager, config);
      if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        star_bus_config_destroy(config);
        s_thread_errors++;
      } else if (result == ESP_OK) {
        /* Try to remove the bus we just added */
        vTaskDelay(pdMS_TO_TICKS(1));
        result = star_bus_manager_remove_bus(s_shared_manager, name);
        if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
          s_thread_errors++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  s_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_add_remove(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TestMgr", NULL, NULL);
  s_shared_manager = &manager;
  s_thread_errors = 0;
  s_tasks_completed = 0;

  /* Spawn multiple tasks that add and remove buses */
  xTaskCreate(concurrent_add_remove_task, "ar1", 4096, (void *)1, 5, NULL);
  xTaskCreate(concurrent_add_remove_task, "ar2", 4096, (void *)2, 5, NULL);
  xTaskCreate(concurrent_add_remove_task, "ar3", 4096, (void *)3, 5, NULL);

  /* Wait for tasks to complete */
  while (s_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, s_thread_errors);

  s_shared_manager = NULL;
  star_bus_manager_deinit(&manager);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Manager Lifecycle Tests */
  RUN_TEST(test_manager_init_valid);
  RUN_TEST(test_manager_init_null_pointer);
  RUN_TEST(test_manager_init_null_tag);
  RUN_TEST(test_manager_init_empty_tag);
  RUN_TEST(test_manager_deinit_valid);
  RUN_TEST(test_manager_deinit_null_pointer);

  /* Type String Tests */
  RUN_TEST(test_bus_type_string_i2c);
  RUN_TEST(test_bus_type_string_spi);
  RUN_TEST(test_bus_type_string_none);
  RUN_TEST(test_bus_type_string_invalid);

  /* Find Bus Tests */
  RUN_TEST(test_find_bus_null_manager);
  RUN_TEST(test_find_bus_null_name);
  RUN_TEST(test_find_bus_not_found);

  /* Interface Injection Tests */
  RUN_TEST(test_manager_init_with_interfaces);
  RUN_TEST(test_manager_stores_tag);
  RUN_TEST(test_manager_mutex_created);
  RUN_TEST(test_manager_buses_initially_null);

  /* Config Creation Tests */
  RUN_TEST(test_create_i2c_config);
  RUN_TEST(test_create_i2c_config_null_name);
  RUN_TEST(test_create_spi_device_config);
  RUN_TEST(test_create_spi_device_null_dev_cfg);

  /* Add Bus Tests */
  RUN_TEST(test_add_bus_null_manager);
  RUN_TEST(test_add_bus_null_config);

  /* Remove Bus Tests */
  RUN_TEST(test_remove_bus_null_manager);
  RUN_TEST(test_remove_bus_null_name);
  RUN_TEST(test_remove_bus_not_found);

  /* SPI Host Tracking Tests */
  RUN_TEST(test_spi_host_tracking_initial_state);

  /* Config Destroy Tests */
  RUN_TEST(test_destroy_null_config);

  /* with_bus API Tests */
  RUN_TEST(test_with_bus_null_manager);
  RUN_TEST(test_with_bus_null_name);
  RUN_TEST(test_with_bus_null_callback);
  RUN_TEST(test_with_bus_not_found);
  RUN_TEST(test_with_bus_found_and_callback_invoked);

  /* Thread Safety Tests */
  RUN_TEST(test_concurrent_find_bus);
  RUN_TEST(test_concurrent_with_bus);
  RUN_TEST(test_concurrent_add_remove);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
