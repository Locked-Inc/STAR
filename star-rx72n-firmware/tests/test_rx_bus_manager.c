/* tests/test_rx_bus_manager.c */

/**
 * @file test_rx_bus_manager.c
 * @brief Unit Tests for rx_bus_manager Bus Manager Component
 *
 * Tests the bus manager initialization, bus registration, bus lookup,
 * thread-safe callback execution, command pattern, and deinitialization.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

#include <string.h>

#include "hardware_pinout.h"
#include "rx_bus_command.h"
#include "rx_bus_config.h"
#include "rx_bus_manager.h"
#include "rx_bus_types.h"
#include "rx_err.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test constants
 */
typedef enum {
  k_test_max_buses_to_add = 35, /**< More than k_max_buses to test overflow */
} test_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/** @brief Static bus manager for tests */
static rx_bus_manager_t s_test_manager;

/** @brief Static bus configurations for testing */
static rx_bus_config_t s_gpio_config;
static rx_bus_config_t s_onewire_config;
static rx_bus_config_t s_uart_config;

/**
 * @brief Set up test fixtures before each test
 */
void setUp(void)
{
  memset(&s_test_manager, 0, sizeof(s_test_manager));
  memset(&s_gpio_config, 0, sizeof(s_gpio_config));
  memset(&s_onewire_config, 0, sizeof(s_onewire_config));
  memset(&s_uart_config, 0, sizeof(s_uart_config));
}

/**
 * @brief Tear down test fixtures after each test
 */
void tearDown(void)
{
  /* Clean up manager if initialized */
  if (s_test_manager.mutex.tx_mutex_id != 0) {
    rx_bus_manager_deinit(&s_test_manager);
  }
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful bus manager initialization with minimal parameters
 */
void test_rx_bus_manager_init_success(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify manager state */
  TEST_ASSERT_NULL(s_test_manager.buses);
  TEST_ASSERT_EQUAL(0, s_test_manager.bus_count);
  TEST_ASSERT_EQUAL_STRING("TEST", s_test_manager.tag);
  TEST_ASSERT_NULL(s_test_manager.error_iface);
  TEST_ASSERT_NULL(s_test_manager.pin_iface);
}

/**
 * @brief Test bus manager init with NULL manager pointer
 */
void test_rx_bus_manager_init_null_manager(void)
{
  rx_err_t err = rx_bus_manager_init(NULL, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test bus manager init with NULL tag pointer
 */
void test_rx_bus_manager_init_null_tag(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, NULL, NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test bus manager deinit success
 */
void test_rx_bus_manager_deinit_success(void)
{
  /* Initialize first */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Deinitialize */
  err = rx_bus_manager_deinit(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify state cleared */
  TEST_ASSERT_NULL(s_test_manager.buses);
  TEST_ASSERT_EQUAL(0, s_test_manager.bus_count);
}

/**
 * @brief Test bus manager deinit with NULL pointer
 */
void test_rx_bus_manager_deinit_null_manager(void)
{
  rx_err_t err = rx_bus_manager_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * Bus Registration Tests (rx_bus_manager_add_bus)
 * =============================================================================
 */

/**
 * @brief Test successful bus addition
 */
void test_rx_bus_manager_add_bus_success(void)
{
  /* Initialize manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create GPIO bus config */
  err = rx_bus_config_init_gpio(&s_gpio_config, "test_gpio", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add bus */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify bus count */
  TEST_ASSERT_EQUAL(1, s_test_manager.bus_count);
  TEST_ASSERT_NOT_NULL(s_test_manager.buses);
}

/**
 * @brief Test adding multiple buses
 */
void test_rx_bus_manager_add_multiple_buses(void)
{
  /* Initialize manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create and add GPIO bus */
  err = rx_bus_config_init_gpio(&s_gpio_config, "gpio_bus", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create and add OneWire bus */
  err = rx_bus_config_init_onewire(&s_onewire_config, "onewire_bus", k_gpio_p05);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_onewire_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create and add UART bus */
  err = rx_bus_config_init_uart(&s_uart_config, "uart_bus", 9, k_gpio_pb7, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_uart_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify bus count */
  TEST_ASSERT_EQUAL(3, s_test_manager.bus_count);
}

/**
 * @brief Test add bus with NULL manager
 */
void test_rx_bus_manager_add_bus_null_manager(void)
{
  rx_err_t err = rx_bus_config_init_gpio(&s_gpio_config, "test_gpio", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(NULL, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test add bus with NULL config
 */
void test_rx_bus_manager_add_bus_null_config(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test add bus with NULL name
 */
void test_rx_bus_manager_add_bus_null_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Manually create config with NULL name */
  memset(&s_gpio_config, 0, sizeof(s_gpio_config));
  s_gpio_config.name = NULL;
  s_gpio_config.type = k_bus_type_gpio;

  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test add bus with empty name
 */
void test_rx_bus_manager_add_bus_empty_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Manually create config with empty name */
  memset(&s_gpio_config, 0, sizeof(s_gpio_config));
  s_gpio_config.name = "";
  s_gpio_config.type = k_bus_type_gpio;

  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test add bus with duplicate name
 */
void test_rx_bus_manager_add_bus_duplicate_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add first bus */
  err = rx_bus_config_init_gpio(&s_gpio_config, "duplicate_name", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Try to add second bus with same name */
  err = rx_bus_config_init_onewire(&s_onewire_config, "duplicate_name", k_gpio_p05);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_onewire_config);
  TEST_ASSERT_EQUAL(k_rx_err_exists, err);

  /* Verify bus count (only first should be added) */
  TEST_ASSERT_EQUAL(1, s_test_manager.bus_count);
}

/**
 * @brief Test add bus when max buses reached
 */
void test_rx_bus_manager_add_bus_max_reached(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Use an array of bus configs to add k_max_buses buses */
  static rx_bus_config_t configs[k_max_buses];
  static char            names[k_max_buses][16];

  for (uint8_t i = 0; i < k_max_buses; ++i) {
    /* Create unique name for each bus */
    names[i][0] = 'b';
    names[i][1] = 'u';
    names[i][2] = 's';
    names[i][3] = '_';
    names[i][4] = (char)('0' + (i / 10));
    names[i][5] = (char)('0' + (i % 10));
    names[i][6] = '\0';

    memset(&configs[i], 0, sizeof(rx_bus_config_t));
    configs[i].name = names[i];
    configs[i].type = k_bus_type_gpio;

    err = rx_bus_manager_add_bus(&s_test_manager, &configs[i]);
    TEST_ASSERT_EQUAL_MESSAGE(k_rx_ok, err, names[i]);
  }

  /* Verify we hit the limit */
  TEST_ASSERT_EQUAL(k_max_buses, s_test_manager.bus_count);

  /* Try to add one more - should fail */
  err = rx_bus_config_init_gpio(&s_gpio_config, "overflow_bus", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_err_no_mem, err);
}

/* =============================================================================
 * Bus Removal Tests (rx_bus_manager_remove_bus)
 * =============================================================================
 */

/**
 * @brief Test successful bus removal
 */
void test_rx_bus_manager_remove_bus_success(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add a bus */
  err = rx_bus_config_init_gpio(&s_gpio_config, "test_gpio", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(1, s_test_manager.bus_count);

  /* Remove the bus */
  err = rx_bus_manager_remove_bus(&s_test_manager, "test_gpio");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, s_test_manager.bus_count);
}

/**
 * @brief Test remove bus with NULL manager
 */
void test_rx_bus_manager_remove_bus_null_manager(void)
{
  rx_err_t err = rx_bus_manager_remove_bus(NULL, "test_gpio");
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test remove bus with NULL name
 */
void test_rx_bus_manager_remove_bus_null_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_remove_bus(&s_test_manager, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test remove non-existent bus
 */
void test_rx_bus_manager_remove_bus_not_found(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_remove_bus(&s_test_manager, "nonexistent_bus");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test remove bus from multiple buses (middle of list)
 */
void test_rx_bus_manager_remove_bus_from_middle(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add multiple buses */
  err = rx_bus_config_init_gpio(&s_gpio_config, "bus_1", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_onewire(&s_onewire_config, "bus_2", k_gpio_p05);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_onewire_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_uart(&s_uart_config, "bus_3", 9, k_gpio_pb7, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_uart_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(3, s_test_manager.bus_count);

  /* Remove middle bus (bus_2) */
  err = rx_bus_manager_remove_bus(&s_test_manager, "bus_2");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, s_test_manager.bus_count);

  /* Verify bus_2 is gone but bus_1 and bus_3 remain */
  rx_bus_config_t* found = NULL;
  err                    = rx_bus_manager_find_bus(&s_test_manager, "bus_1", &found);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_find_bus(&s_test_manager, "bus_2", &found);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);

  err = rx_bus_manager_find_bus(&s_test_manager, "bus_3", &found);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Bus Lookup Tests (rx_bus_manager_find_bus)
 * =============================================================================
 */

/**
 * @brief Test successful bus lookup
 */
void test_rx_bus_manager_find_bus_success(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add a bus */
  err = rx_bus_config_init_gpio(&s_gpio_config, "find_me", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Find the bus */
  rx_bus_config_t* found = NULL;
  err                    = rx_bus_manager_find_bus(&s_test_manager, "find_me", &found);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_STRING("find_me", found->name);
  TEST_ASSERT_EQUAL(k_bus_type_gpio, found->type);
}

/**
 * @brief Test find bus with NULL manager
 */
void test_rx_bus_manager_find_bus_null_manager(void)
{
  rx_bus_config_t* found = NULL;
  rx_err_t         err   = rx_bus_manager_find_bus(NULL, "test", &found);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test find bus with NULL name
 */
void test_rx_bus_manager_find_bus_null_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_bus_config_t* found = NULL;
  err                    = rx_bus_manager_find_bus(&s_test_manager, NULL, &found);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test find bus with NULL output pointer
 */
void test_rx_bus_manager_find_bus_null_output(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_find_bus(&s_test_manager, "test", NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test find non-existent bus
 */
void test_rx_bus_manager_find_bus_not_found(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_bus_config_t* found = NULL;
  err                    = rx_bus_manager_find_bus(&s_test_manager, "nonexistent", &found);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_NULL(found);
}

/* =============================================================================
 * Thread-Safe Callback Tests (rx_bus_manager_with_bus)
 * =============================================================================
 */

/** @brief Callback test context */
typedef struct {
  bool     callback_called;
  rx_err_t callback_return;
  char     bus_name_seen[32];
} callback_ctx_t;

/**
 * @brief Test callback function
 */
static rx_err_t test_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  callback_ctx_t* ctx = (callback_ctx_t*)user_ctx;

  ctx->callback_called = true;
  if (bus_config->name != NULL) {
    size_t len = strlen(bus_config->name);
    if (len >= sizeof(ctx->bus_name_seen)) {
      len = sizeof(ctx->bus_name_seen) - 1;
    }
    memcpy(ctx->bus_name_seen, bus_config->name, len);
    ctx->bus_name_seen[len] = '\0';
  }

  return ctx->callback_return;
}

/**
 * @brief Test successful callback execution
 */
void test_rx_bus_manager_with_bus_success(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add a bus */
  err = rx_bus_config_init_gpio(&s_gpio_config, "callback_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Execute callback */
  callback_ctx_t ctx = {.callback_called = false, .callback_return = k_rx_ok, .bus_name_seen = ""};

  err = rx_bus_manager_with_bus(&s_test_manager, "callback_test", test_callback, &ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(ctx.callback_called);
  TEST_ASSERT_EQUAL_STRING("callback_test", ctx.bus_name_seen);
}

/**
 * @brief Test callback returns error
 */
void test_rx_bus_manager_with_bus_callback_error(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "error_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Execute callback that returns error */
  callback_ctx_t ctx = {
    .callback_called = false, .callback_return = k_rx_err_hw_error, .bus_name_seen = ""};

  err = rx_bus_manager_with_bus(&s_test_manager, "error_test", test_callback, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
  TEST_ASSERT_TRUE(ctx.callback_called);
}

/**
 * @brief Test with_bus with NULL manager
 */
void test_rx_bus_manager_with_bus_null_manager(void)
{
  callback_ctx_t ctx = {.callback_called = false, .callback_return = k_rx_ok, .bus_name_seen = ""};

  rx_err_t err = rx_bus_manager_with_bus(NULL, "test", test_callback, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
  TEST_ASSERT_FALSE(ctx.callback_called);
}

/**
 * @brief Test with_bus with NULL name
 */
void test_rx_bus_manager_with_bus_null_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  callback_ctx_t ctx = {.callback_called = false, .callback_return = k_rx_ok, .bus_name_seen = ""};

  err = rx_bus_manager_with_bus(&s_test_manager, NULL, test_callback, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
  TEST_ASSERT_FALSE(ctx.callback_called);
}

/**
 * @brief Test with_bus with NULL callback
 */
void test_rx_bus_manager_with_bus_null_callback(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "null_cb_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_with_bus(&s_test_manager, "null_cb_test", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test with_bus on non-existent bus
 */
void test_rx_bus_manager_with_bus_not_found(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  callback_ctx_t ctx = {.callback_called = false, .callback_return = k_rx_ok, .bus_name_seen = ""};

  err = rx_bus_manager_with_bus(&s_test_manager, "nonexistent", test_callback, &ctx);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_FALSE(ctx.callback_called);
}

/* =============================================================================
 * Command Pattern Tests (rx_bus_manager_execute_command)
 * =============================================================================
 */

/** @brief Command test data */
typedef struct {
  uint32_t value;
  bool     executed;
} command_test_data_t;

/**
 * @brief Test command execute function
 */
static rx_err_t test_command_execute(rx_bus_config_t* bus, void* data)
{
  command_test_data_t* cmd_data = (command_test_data_t*)data;
  cmd_data->executed            = true;
  cmd_data->value               = 42;

  (void)bus;

  return k_rx_ok;
}

/**
 * @brief Test command execute function that returns error
 */
static rx_err_t test_command_execute_error(rx_bus_config_t* bus, void* data)
{
  command_test_data_t* cmd_data = (command_test_data_t*)data;
  cmd_data->executed            = true;

  (void)bus;

  return k_rx_err_hw_error;
}

/**
 * @brief Test successful command execution
 */
void test_rx_bus_manager_execute_command_success(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "cmd_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create and execute command */
  command_test_data_t data = {.value = 0, .executed = false};
  rx_bus_command_t    cmd;
  rx_bus_command_init(&cmd, test_command_execute, &data);

  err = rx_bus_manager_execute_command(&s_test_manager, "cmd_test", &cmd);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(data.executed);
  TEST_ASSERT_EQUAL(42, data.value);
  TEST_ASSERT_EQUAL(k_rx_ok, cmd.result);
}

/**
 * @brief Test command execution with error return
 */
void test_rx_bus_manager_execute_command_error(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "cmd_err_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create and execute command that returns error */
  command_test_data_t data = {.value = 0, .executed = false};
  rx_bus_command_t    cmd;
  rx_bus_command_init(&cmd, test_command_execute_error, &data);

  err = rx_bus_manager_execute_command(&s_test_manager, "cmd_err_test", &cmd);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
  TEST_ASSERT_TRUE(data.executed);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, cmd.result);
}

/**
 * @brief Test execute command with NULL manager
 */
void test_rx_bus_manager_execute_command_null_manager(void)
{
  command_test_data_t data = {.value = 0, .executed = false};
  rx_bus_command_t    cmd;
  rx_bus_command_init(&cmd, test_command_execute, &data);

  rx_err_t err = rx_bus_manager_execute_command(NULL, "test", &cmd);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
  TEST_ASSERT_FALSE(data.executed);
}

/**
 * @brief Test execute command with NULL name
 */
void test_rx_bus_manager_execute_command_null_name(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  command_test_data_t data = {.value = 0, .executed = false};
  rx_bus_command_t    cmd;
  rx_bus_command_init(&cmd, test_command_execute, &data);

  err = rx_bus_manager_execute_command(&s_test_manager, NULL, &cmd);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
  TEST_ASSERT_FALSE(data.executed);
}

/**
 * @brief Test execute command with NULL command
 */
void test_rx_bus_manager_execute_command_null_command(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "null_cmd_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_execute_command(&s_test_manager, "null_cmd_test", NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test execute command with NULL execute function
 */
void test_rx_bus_manager_execute_command_null_execute(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_config_init_gpio(&s_gpio_config, "null_exec_test", k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create command with NULL execute function */
  rx_bus_command_t cmd;
  cmd.execute = NULL;
  cmd.data    = NULL;
  cmd.result  = k_rx_ok;

  err = rx_bus_manager_execute_command(&s_test_manager, "null_exec_test", &cmd);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test execute command on non-existent bus
 */
void test_rx_bus_manager_execute_command_not_found(void)
{
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  command_test_data_t data = {.value = 0, .executed = false};
  rx_bus_command_t    cmd;
  rx_bus_command_init(&cmd, test_command_execute, &data);

  err = rx_bus_manager_execute_command(&s_test_manager, "nonexistent", &cmd);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_FALSE(data.executed);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization Tests */
  RUN_TEST(test_rx_bus_manager_init_success);
  RUN_TEST(test_rx_bus_manager_init_null_manager);
  RUN_TEST(test_rx_bus_manager_init_null_tag);
  RUN_TEST(test_rx_bus_manager_deinit_success);
  RUN_TEST(test_rx_bus_manager_deinit_null_manager);

  /* Bus Registration Tests */
  RUN_TEST(test_rx_bus_manager_add_bus_success);
  RUN_TEST(test_rx_bus_manager_add_multiple_buses);
  RUN_TEST(test_rx_bus_manager_add_bus_null_manager);
  RUN_TEST(test_rx_bus_manager_add_bus_null_config);
  RUN_TEST(test_rx_bus_manager_add_bus_null_name);
  RUN_TEST(test_rx_bus_manager_add_bus_empty_name);
  RUN_TEST(test_rx_bus_manager_add_bus_duplicate_name);
  RUN_TEST(test_rx_bus_manager_add_bus_max_reached);

  /* Bus Removal Tests */
  RUN_TEST(test_rx_bus_manager_remove_bus_success);
  RUN_TEST(test_rx_bus_manager_remove_bus_null_manager);
  RUN_TEST(test_rx_bus_manager_remove_bus_null_name);
  RUN_TEST(test_rx_bus_manager_remove_bus_not_found);
  RUN_TEST(test_rx_bus_manager_remove_bus_from_middle);

  /* Bus Lookup Tests */
  RUN_TEST(test_rx_bus_manager_find_bus_success);
  RUN_TEST(test_rx_bus_manager_find_bus_null_manager);
  RUN_TEST(test_rx_bus_manager_find_bus_null_name);
  RUN_TEST(test_rx_bus_manager_find_bus_null_output);
  RUN_TEST(test_rx_bus_manager_find_bus_not_found);

  /* Thread-Safe Callback Tests */
  RUN_TEST(test_rx_bus_manager_with_bus_success);
  RUN_TEST(test_rx_bus_manager_with_bus_callback_error);
  RUN_TEST(test_rx_bus_manager_with_bus_null_manager);
  RUN_TEST(test_rx_bus_manager_with_bus_null_name);
  RUN_TEST(test_rx_bus_manager_with_bus_null_callback);
  RUN_TEST(test_rx_bus_manager_with_bus_not_found);

  /* Command Pattern Tests */
  RUN_TEST(test_rx_bus_manager_execute_command_success);
  RUN_TEST(test_rx_bus_manager_execute_command_error);
  RUN_TEST(test_rx_bus_manager_execute_command_null_manager);
  RUN_TEST(test_rx_bus_manager_execute_command_null_name);
  RUN_TEST(test_rx_bus_manager_execute_command_null_command);
  RUN_TEST(test_rx_bus_manager_execute_command_null_execute);
  RUN_TEST(test_rx_bus_manager_execute_command_not_found);

  return UNITY_END();
}
