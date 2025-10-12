/* esp32-firmware/components/star_bus/star_bus_debug.c */

#include "star_bus_debug.h"

#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_types.h"

static const char* TAG = "star_bus_debug";

/* --- I2C Bus Scanning --- */

esp_err_t star_bus_debug_scan_i2c(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_bus_scan_callback_t  callback,
                                  void*                     user_data,
                                  uint8_t*                  devices_found,
                                  size_t                    max_devices,
                                  size_t*                   num_found)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Find the bus configuration
  star_bus_config_t* config = star_bus_manager_find_bus(manager, bus_name);
  if (config == NULL) {
    ESP_LOGE(TAG, "Bus '%s' not found", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  if (config->type != k_star_bus_type_i2c) {
    ESP_LOGE(TAG, "Bus '%s' is not an I2C bus", bus_name);
    return ESP_ERR_INVALID_ARG;
  }

  size_t found_count = 0;

  ESP_LOGI(TAG, "Scanning I2C bus '%s'...", bus_name);

  // Scan all valid I2C addresses (0x08-0x77)
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    // Try to read from the device using raw read
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(config->proto.i2c.port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
      found_count++;

      // Call callback if provided
      if (callback != NULL) {
        callback(addr, user_data);
      }

      // Store in devices_found array if provided
      if (devices_found != NULL && found_count <= max_devices) {
        devices_found[found_count - 1] = addr;
      }

      ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
    }
  }

  if (num_found != NULL) {
    *num_found = found_count;
  }

  ESP_LOGI(TAG, "Scan complete. Found %zu device(s)", found_count);
  return ESP_OK;
}

esp_err_t star_bus_debug_print_scan(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  size_t num_found = 0;
  return star_bus_debug_scan_i2c(manager, bus_name, NULL, NULL, NULL, 0, &num_found);
}

/* --- Bus Information --- */

esp_err_t star_bus_debug_print_buses(const star_bus_manager_t* manager)
{
  if (manager == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "=== Configured Buses ===");

  // Iterate through linked list of buses
  int                count  = 0;
  star_bus_config_t* config = manager->buses;

  while (config != NULL) {
    count++;

    const char* type_str = star_bus_type_to_string(config->type);

    ESP_LOGI(TAG,
             "  [%d] %s (%s) - initialized=%d",
             count,
             config->name,
             type_str,
             config->initialized);

    if (config->type == k_star_bus_type_i2c) {
      ESP_LOGI(TAG,
               "      Address: 0x%02X, Port: %d, SDA: %d, SCL: %d",
               config->proto.i2c.address,
               config->proto.i2c.port,
               config->proto.i2c.config.sda_io_num,
               config->proto.i2c.config.scl_io_num);
    }

    config = config->next;
  }

  ESP_LOGI(TAG, "Total: %d bus(es) configured", count);
  return ESP_OK;
}

esp_err_t star_bus_debug_print_bus_info(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_config_t* config = star_bus_manager_find_bus(manager, bus_name);
  if (config == NULL) {
    ESP_LOGE(TAG, "Bus '%s' not found", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "=== Bus Info: %s ===", bus_name);

  const char* type_str = star_bus_type_to_string(config->type);

  ESP_LOGI(TAG, "  Type: %s", type_str);
  ESP_LOGI(TAG, "  Initialized: %s", config->initialized ? "Yes" : "No");

  if (config->type == k_star_bus_type_i2c) {
    ESP_LOGI(TAG, "  I2C Address: 0x%02X", config->proto.i2c.address);
    ESP_LOGI(TAG, "  I2C Port: %d", config->proto.i2c.port);
    ESP_LOGI(TAG, "  SDA Pin: %d", config->proto.i2c.config.sda_io_num);
    ESP_LOGI(TAG, "  SCL Pin: %d", config->proto.i2c.config.scl_io_num);
    ESP_LOGI(TAG, "  Clock Speed: %lu Hz", config->proto.i2c.config.master.clk_speed);
  }

  return ESP_OK;
}

/* --- Register Operations --- */

esp_err_t
star_bus_debug_read_reg(const star_bus_manager_t* manager, const char* bus_name, uint8_t reg_addr)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t   value;
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, bus_name, &value, 1, reg_addr, &bytes_read);

  if (ret == ESP_OK && bytes_read == 1) {
    ESP_LOGI(TAG, "Read register 0x%02X: 0x%02X", reg_addr, value);
  } else {
    ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t star_bus_debug_write_reg(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   uint8_t                   reg_addr,
                                   uint8_t                   value)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  size_t    bytes_written;
  esp_err_t ret = star_bus_i2c_write(manager, bus_name, &value, 1, reg_addr, &bytes_written);

  if (ret == ESP_OK && bytes_written == 1) {
    ESP_LOGI(TAG, "Write register 0x%02X: 0x%02X", reg_addr, value);
  } else {
    ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t star_bus_debug_dump_regs(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   uint8_t                   start_reg,
                                   size_t                    count)
{
  if (manager == NULL || bus_name == NULL || count == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG,
           "=== Register Dump: %s (0x%02X - 0x%02X) ===",
           bus_name,
           start_reg,
           start_reg + count - 1);

  uint8_t values[256];
  size_t  max_read = (count > sizeof(values)) ? sizeof(values) : count;

  for (size_t i = 0; i < max_read; i++) {
    uint8_t   reg = start_reg + i;
    size_t    bytes_read;
    esp_err_t ret = star_bus_i2c_read(manager, bus_name, &values[i], 1, reg, &bytes_read);

    if (ret != ESP_OK || bytes_read != 1) {
      ESP_LOGW(TAG, "  0x%02X: [READ ERROR: %s]", reg, esp_err_to_name(ret));
      values[i] = 0;
    }
  }

  // Print in rows of 16
  for (size_t i = 0; i < max_read; i += 16) {
    char line[128] = {0};
    int  offset    = 0;

    offset += snprintf(line + offset, sizeof(line) - offset, "  0x%02X: ", start_reg + i);

    for (size_t j = 0; j < 16 && (i + j) < max_read; j++) {
      offset += snprintf(line + offset, sizeof(line) - offset, "%02X ", values[i + j]);
    }

    ESP_LOGI(TAG, "%s", line);
  }

  return ESP_OK;
}

/* --- Transaction Logging --- */

esp_err_t star_bus_debug_logger_init(star_bus_transaction_logger_t* logger,
                                     star_bus_transaction_log_t*    buffer,
                                     size_t                         buffer_size,
                                     bool                           wrap_around)
{
  if (logger == NULL || buffer == NULL || buffer_size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  logger->buffer      = buffer;
  logger->buffer_size = buffer_size;
  logger->write_index = 0;
  logger->count       = 0;
  logger->wrap_around = wrap_around;
  logger->enabled     = false;

  memset(buffer, 0, buffer_size * sizeof(star_bus_transaction_log_t));

  return ESP_OK;
}

esp_err_t star_bus_debug_logger_set_enabled(star_bus_transaction_logger_t* logger, bool enabled)
{
  if (logger == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  logger->enabled = enabled;
  return ESP_OK;
}

esp_err_t star_bus_debug_logger_clear(star_bus_transaction_logger_t* logger)
{
  if (logger == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  logger->write_index = 0;
  logger->count       = 0;
  memset(logger->buffer, 0, logger->buffer_size * sizeof(star_bus_transaction_log_t));

  return ESP_OK;
}

esp_err_t star_bus_debug_logger_log(star_bus_transaction_logger_t* logger,
                                    uint8_t                        bus_index,
                                    uint8_t                        address,
                                    uint8_t                        reg_addr,
                                    uint16_t                       length,
                                    bool                           is_write,
                                    bool                           success,
                                    int32_t                        error_code)
{
  if (logger == NULL || !logger->enabled) {
    return ESP_ERR_INVALID_STATE;
  }

  if (logger->write_index >= logger->buffer_size) {
    if (!logger->wrap_around) {
      return ESP_ERR_NO_MEM;
    }
    logger->write_index = 0;
  }

  star_bus_transaction_log_t* entry = &logger->buffer[logger->write_index];
  entry->timestamp_us               = esp_timer_get_time();
  entry->bus_index                  = bus_index;
  entry->address                    = address;
  entry->reg_addr                   = reg_addr;
  entry->length                     = length;
  entry->is_write                   = is_write;
  entry->success                    = success;
  entry->error_code                 = error_code;

  logger->write_index++;
  if (logger->count < logger->buffer_size) {
    logger->count++;
  }

  return ESP_OK;
}

esp_err_t star_bus_debug_logger_print(const star_bus_transaction_logger_t* logger)
{
  if (logger == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "=== Transaction Log (%zu entries) ===", logger->count);

  if (logger->count == 0) {
    ESP_LOGI(TAG, "  (empty)");
    return ESP_OK;
  }

  for (size_t i = 0; i < logger->count; i++) {
    const star_bus_transaction_log_t* entry = &logger->buffer[i];

    const char* type   = entry->is_write ? "WRITE" : "READ";
    const char* result = entry->success ? "OK" : "FAIL";

    if (entry->success) {
      ESP_LOGI(TAG,
               "  [%llu] Bus=%d Addr=0x%02X Reg=0x%02X Len=%u %s %s",
               entry->timestamp_us,
               entry->bus_index,
               entry->address,
               entry->reg_addr,
               entry->length,
               type,
               result);
    } else {
      ESP_LOGI(TAG,
               "  [%llu] Bus=%d Addr=0x%02X Reg=0x%02X Len=%u %s %s (error=%ld)",
               entry->timestamp_us,
               entry->bus_index,
               entry->address,
               entry->reg_addr,
               entry->length,
               type,
               result,
               entry->error_code);
    }
  }

  return ESP_OK;
}

esp_err_t star_bus_debug_logger_get_entry(const star_bus_transaction_logger_t* logger,
                                          size_t                               index,
                                          star_bus_transaction_log_t*          entry)
{
  if (logger == NULL || entry == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (index >= logger->count) {
    return ESP_ERR_INVALID_ARG;
  }

  memcpy(entry, &logger->buffer[index], sizeof(star_bus_transaction_log_t));
  return ESP_OK;
}

/* --- Error Analysis --- */

esp_err_t star_bus_debug_stats_init(star_bus_error_stats_t* stats)
{
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(stats, 0, sizeof(star_bus_error_stats_t));
  return ESP_OK;
}

esp_err_t
star_bus_debug_stats_record(star_bus_error_stats_t* stats, bool success, int32_t error_code)
{
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stats->total_transactions++;

  if (!success) {
    stats->failed_transactions++;
    stats->last_error_time_us = esp_timer_get_time();
    stats->last_error_code    = error_code;

    // Categorize error
    switch (error_code) {
      case ESP_ERR_TIMEOUT:
        stats->timeout_errors++;
        break;
      case ESP_FAIL: // NACK
        stats->nack_errors++;
        break;
      case ESP_ERR_INVALID_STATE:
        stats->bus_errors++;
        break;
      default:
        stats->other_errors++;
        break;
    }
  }

  return ESP_OK;
}

esp_err_t star_bus_debug_stats_print(const star_bus_error_stats_t* stats)
{
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "=== Error Statistics ===");
  ESP_LOGI(TAG, "  Total transactions: %lu", stats->total_transactions);
  ESP_LOGI(TAG, "  Failed transactions: %lu", stats->failed_transactions);

  if (stats->total_transactions > 0) {
    float error_rate = (float)stats->failed_transactions / stats->total_transactions * 100.0f;
    ESP_LOGI(TAG, "  Error rate: %.2f%%", error_rate);
  }

  ESP_LOGI(TAG, "  Timeout errors: %lu", stats->timeout_errors);
  ESP_LOGI(TAG, "  NACK errors: %lu", stats->nack_errors);
  ESP_LOGI(TAG, "  Bus errors: %lu", stats->bus_errors);
  ESP_LOGI(TAG, "  Other errors: %lu", stats->other_errors);

  if (stats->failed_transactions > 0) {
    ESP_LOGI(TAG,
             "  Last error: %ld at %llu us",
             stats->last_error_code,
             stats->last_error_time_us);
  }

  return ESP_OK;
}

esp_err_t star_bus_debug_stats_reset(star_bus_error_stats_t* stats)
{
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(stats, 0, sizeof(star_bus_error_stats_t));
  return ESP_OK;
}

/* --- Diagnostic Tests --- */

esp_err_t star_bus_debug_test_bus(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_config_t* config = star_bus_manager_find_bus(manager, bus_name);
  if (config == NULL) {
    ESP_LOGE(TAG, "Bus '%s' not found", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "=== Bus Test: %s ===", bus_name);

  if (!config->initialized) {
    ESP_LOGE(TAG, "  FAIL: Bus not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "  PASS: Bus initialized");

  // Try a scan to verify bus is working
  size_t    num_found;
  esp_err_t ret = star_bus_debug_scan_i2c(manager, bus_name, NULL, NULL, NULL, 0, &num_found);

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "  PASS: Bus scan completed (%zu device(s) found)", num_found);
  } else {
    ESP_LOGE(TAG, "  FAIL: Bus scan failed");
    return ret;
  }

  ESP_LOGI(TAG, "=== Bus Test PASSED ===");
  return ESP_OK;
}

esp_err_t star_bus_debug_test_device(const star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "=== Device Test: %s ===", bus_name);

  // Try to read from device
  uint8_t   data;
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read_raw(manager, bus_name, &data, 1, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "  PASS: Device responds (read 0x%02X)", data);
  } else {
    ESP_LOGE(TAG, "  FAIL: Device does not respond: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "=== Device Test PASSED ===");
  return ESP_OK;
}

esp_err_t star_bus_debug_test_reg_rw(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     uint8_t                   reg_addr,
                                     uint8_t                   test_value)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "=== Register R/W Test: %s (reg=0x%02X) ===", bus_name, reg_addr);

  // Read original value
  uint8_t   original;
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, bus_name, &original, 1, reg_addr, &bytes_read);

  if (ret != ESP_OK || bytes_read != 1) {
    ESP_LOGE(TAG, "  FAIL: Could not read original value: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "  Original value: 0x%02X", original);

  // Write test value
  size_t bytes_written;
  ret = star_bus_i2c_write(manager, bus_name, &test_value, 1, reg_addr, &bytes_written);

  if (ret != ESP_OK || bytes_written != 1) {
    ESP_LOGE(TAG, "  FAIL: Could not write test value: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "  Wrote test value: 0x%02X", test_value);

  // Read back
  uint8_t readback;
  ret = star_bus_i2c_read(manager, bus_name, &readback, 1, reg_addr, &bytes_read);

  if (ret != ESP_OK || bytes_read != 1) {
    ESP_LOGE(TAG, "  FAIL: Could not read back value: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "  Read back value: 0x%02X", readback);

  // Restore original value
  ret = star_bus_i2c_write(manager, bus_name, &original, 1, reg_addr, &bytes_written);

  if (ret != ESP_OK || bytes_written != 1) {
    ESP_LOGW(TAG, "  WARNING: Could not restore original value: %s", esp_err_to_name(ret));
  }

  // Verify
  if (readback == test_value) {
    ESP_LOGI(TAG, "  PASS: Register R/W verified");
    ESP_LOGI(TAG, "=== Register R/W Test PASSED ===");
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "  FAIL: Read back 0x%02X, expected 0x%02X", readback, test_value);
    return ESP_FAIL;
  }
}
