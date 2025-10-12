/* esp32-firmware/components/star_bus/test/test_debug.c */

#include <string.h>

#include "star_bus_config.h"
#include "star_bus_debug.h"
#include "star_bus_manager.h"
#include "star_test.h"

/* Test fixtures */
static star_bus_manager_t test_manager;

/* Setup/teardown helpers */
static void setup_test_manager(void)
{
  esp_err_t ret = star_bus_manager_init(&test_manager, "test_debug");
  STAR_ASSERT_EQUAL(ESP_OK, ret);
}

static void teardown_test_manager(void)
{
  esp_err_t ret = star_bus_manager_deinit(&test_manager);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
}

/* --- Scan Tests --- */

STAR_TEST_CASE(debug_scan, null_manager)
{
  size_t    num_found;
  esp_err_t ret = star_bus_debug_scan_i2c(NULL, "test", NULL, NULL, NULL, 0, &num_found);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_scan, null_bus_name)
{
  setup_test_manager();
  size_t    num_found;
  esp_err_t ret = star_bus_debug_scan_i2c(&test_manager, NULL, NULL, NULL, NULL, 0, &num_found);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

STAR_TEST_CASE(debug_scan, print_null_manager)
{
  esp_err_t ret = star_bus_debug_print_scan(NULL, "test");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- Bus Info Tests --- */

STAR_TEST_CASE(debug_info, print_buses_null_manager)
{
  esp_err_t ret = star_bus_debug_print_buses(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_info, print_bus_info_null_manager)
{
  esp_err_t ret = star_bus_debug_print_bus_info(NULL, "test");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_info, print_bus_info_null_name)
{
  setup_test_manager();
  esp_err_t ret = star_bus_debug_print_bus_info(&test_manager, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

/* --- Register Operation Tests --- */

STAR_TEST_CASE(debug_reg, read_null_manager)
{
  esp_err_t ret = star_bus_debug_read_reg(NULL, "test", 0x00);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_reg, write_null_manager)
{
  esp_err_t ret = star_bus_debug_write_reg(NULL, "test", 0x00, 0xFF);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_reg, dump_null_manager)
{
  esp_err_t ret = star_bus_debug_dump_regs(NULL, "test", 0x00, 10);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_reg, dump_zero_count)
{
  setup_test_manager();
  esp_err_t ret = star_bus_debug_dump_regs(&test_manager, "test", 0x00, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
  teardown_test_manager();
}

/* --- Transaction Logger Tests --- */

STAR_TEST_CASE(debug_logger, init_null_logger)
{
  star_bus_transaction_log_t buffer[10];
  esp_err_t                  ret = star_bus_debug_logger_init(NULL, buffer, 10, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, init_null_buffer)
{
  star_bus_transaction_logger_t logger;
  esp_err_t                     ret = star_bus_debug_logger_init(&logger, NULL, 10, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, init_zero_size)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  esp_err_t                     ret = star_bus_debug_logger_init(&logger, buffer, 0, false);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, init_success)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  esp_err_t                     ret = star_bus_debug_logger_init(&logger, buffer, 10, false);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(10, logger.buffer_size);
  STAR_ASSERT_EQUAL(0, logger.write_index);
  STAR_ASSERT_EQUAL(0, logger.count);
  STAR_ASSERT_FALSE(logger.enabled);
}

STAR_TEST_CASE(debug_logger, set_enabled_null)
{
  esp_err_t ret = star_bus_debug_logger_set_enabled(NULL, true);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, set_enabled_success)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_debug_logger_init(&logger, buffer, 10, false);

  esp_err_t ret = star_bus_debug_logger_set_enabled(&logger, true);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_TRUE(logger.enabled);
}

STAR_TEST_CASE(debug_logger, clear_null)
{
  esp_err_t ret = star_bus_debug_logger_clear(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, clear_success)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_debug_logger_init(&logger, buffer, 10, false);
  logger.count       = 5;
  logger.write_index = 5;

  esp_err_t ret = star_bus_debug_logger_clear(&logger);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(0, logger.write_index);
  STAR_ASSERT_EQUAL(0, logger.count);
}

STAR_TEST_CASE(debug_logger, log_null)
{
  esp_err_t ret = star_bus_debug_logger_log(NULL, 0, 0x48, 0x00, 1, true, true, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

STAR_TEST_CASE(debug_logger, log_disabled)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_debug_logger_init(&logger, buffer, 10, false);

  esp_err_t ret = star_bus_debug_logger_log(&logger, 0, 0x48, 0x00, 1, true, true, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

STAR_TEST_CASE(debug_logger, log_success)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_debug_logger_init(&logger, buffer, 10, false);
  star_bus_debug_logger_set_enabled(&logger, true);

  esp_err_t ret = star_bus_debug_logger_log(&logger, 0, 0x48, 0x00, 1, true, true, 0);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(1, logger.count);
  STAR_ASSERT_EQUAL(1, logger.write_index);
}

STAR_TEST_CASE(debug_logger, print_null)
{
  esp_err_t ret = star_bus_debug_logger_print(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, get_entry_null_logger)
{
  star_bus_transaction_log_t entry;
  esp_err_t                  ret = star_bus_debug_logger_get_entry(NULL, 0, &entry);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, get_entry_null_entry)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_debug_logger_init(&logger, buffer, 10, false);

  esp_err_t ret = star_bus_debug_logger_get_entry(&logger, 0, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_logger, get_entry_invalid_index)
{
  star_bus_transaction_logger_t logger;
  star_bus_transaction_log_t    buffer[10];
  star_bus_transaction_log_t    entry;
  star_bus_debug_logger_init(&logger, buffer, 10, false);

  esp_err_t ret = star_bus_debug_logger_get_entry(&logger, 0, &entry);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- Error Stats Tests --- */

STAR_TEST_CASE(debug_stats, init_null)
{
  esp_err_t ret = star_bus_debug_stats_init(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_stats, init_success)
{
  star_bus_error_stats_t stats;
  esp_err_t              ret = star_bus_debug_stats_init(&stats);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(0, stats.total_transactions);
  STAR_ASSERT_EQUAL(0, stats.failed_transactions);
}

STAR_TEST_CASE(debug_stats, record_null)
{
  esp_err_t ret = star_bus_debug_stats_record(NULL, true, 0);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_stats, record_success)
{
  star_bus_error_stats_t stats;
  star_bus_debug_stats_init(&stats);

  esp_err_t ret = star_bus_debug_stats_record(&stats, true, 0);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(1, stats.total_transactions);
  STAR_ASSERT_EQUAL(0, stats.failed_transactions);
}

STAR_TEST_CASE(debug_stats, record_failure)
{
  star_bus_error_stats_t stats;
  star_bus_debug_stats_init(&stats);

  esp_err_t ret = star_bus_debug_stats_record(&stats, false, ESP_ERR_TIMEOUT);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(1, stats.total_transactions);
  STAR_ASSERT_EQUAL(1, stats.failed_transactions);
  STAR_ASSERT_EQUAL(1, stats.timeout_errors);
}

STAR_TEST_CASE(debug_stats, print_null)
{
  esp_err_t ret = star_bus_debug_stats_print(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_stats, reset_null)
{
  esp_err_t ret = star_bus_debug_stats_reset(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_stats, reset_success)
{
  star_bus_error_stats_t stats;
  star_bus_debug_stats_init(&stats);
  star_bus_debug_stats_record(&stats, true, 0);

  esp_err_t ret = star_bus_debug_stats_reset(&stats);
  STAR_ASSERT_EQUAL(ESP_OK, ret);
  STAR_ASSERT_EQUAL(0, stats.total_transactions);
}

/* --- Diagnostic Tests --- */

STAR_TEST_CASE(debug_diag, test_bus_null_manager)
{
  esp_err_t ret = star_bus_debug_test_bus(NULL, "test");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_diag, test_device_null_manager)
{
  esp_err_t ret = star_bus_debug_test_device(NULL, "test");
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

STAR_TEST_CASE(debug_diag, test_reg_rw_null_manager)
{
  esp_err_t ret = star_bus_debug_test_reg_rw(NULL, "test", 0x00, 0xFF);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* Test registration */
STAR_TEST_LIST_BEGIN()
STAR_TEST_REF(debug_scan, null_manager)
STAR_TEST_REF(debug_scan, null_bus_name)
STAR_TEST_REF(debug_scan, print_null_manager)
STAR_TEST_REF(debug_info, print_buses_null_manager)
STAR_TEST_REF(debug_info, print_bus_info_null_manager)
STAR_TEST_REF(debug_info, print_bus_info_null_name)
STAR_TEST_REF(debug_reg, read_null_manager)
STAR_TEST_REF(debug_reg, write_null_manager)
STAR_TEST_REF(debug_reg, dump_null_manager)
STAR_TEST_REF(debug_reg, dump_zero_count)
STAR_TEST_REF(debug_logger, init_null_logger)
STAR_TEST_REF(debug_logger, init_null_buffer)
STAR_TEST_REF(debug_logger, init_zero_size)
STAR_TEST_REF(debug_logger, init_success)
STAR_TEST_REF(debug_logger, set_enabled_null)
STAR_TEST_REF(debug_logger, set_enabled_success)
STAR_TEST_REF(debug_logger, clear_null)
STAR_TEST_REF(debug_logger, clear_success)
STAR_TEST_REF(debug_logger, log_null)
STAR_TEST_REF(debug_logger, log_disabled)
STAR_TEST_REF(debug_logger, log_success)
STAR_TEST_REF(debug_logger, print_null)
STAR_TEST_REF(debug_logger, get_entry_null_logger)
STAR_TEST_REF(debug_logger, get_entry_null_entry)
STAR_TEST_REF(debug_logger, get_entry_invalid_index)
STAR_TEST_REF(debug_stats, init_null)
STAR_TEST_REF(debug_stats, init_success)
STAR_TEST_REF(debug_stats, record_null)
STAR_TEST_REF(debug_stats, record_success)
STAR_TEST_REF(debug_stats, record_failure)
STAR_TEST_REF(debug_stats, print_null)
STAR_TEST_REF(debug_stats, reset_null)
STAR_TEST_REF(debug_stats, reset_success)
STAR_TEST_REF(debug_diag, test_bus_null_manager)
STAR_TEST_REF(debug_diag, test_device_null_manager)
STAR_TEST_REF(debug_diag, test_reg_rw_null_manager)
STAR_TEST_LIST_END()
