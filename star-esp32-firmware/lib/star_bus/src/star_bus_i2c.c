/* lib/star_bus/src/star_bus_i2c.c */


#include "star_bus_i2c.h"

#include "freertos/FreeRTOS.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager.h" /* Needed for star_bus_manager_find_bus */
#include "star_bus_types.h"   /* Needed for star_bus_config_t and union */

/* --- Constants --- */

static const char* s_TAG = "STAR_BUS_I2C";

/* Use constant instead of macro for type safety */
static const uint32_t s_i2c_timeout_ms = 1000; /* Default timeout for I2C operations */

/* --- Private Function Prototypes (Default Ops) --- */

static esp_err_t internal_star_bus_i2c_write(const star_bus_config_t* config,
                                             const uint8_t*           data,
                                             size_t                   len,
                                             uint8_t                  reg_addr,
                                             size_t*                  bytes_written);

static esp_err_t internal_star_bus_i2c_read(const star_bus_config_t* config,
                                            uint8_t*                 data,
                                            size_t                   len,
                                            uint8_t                  reg_addr,
                                            size_t*                  bytes_read);

static esp_err_t internal_star_bus_i2c_write_command(const star_bus_config_t* config,
                                                     uint8_t                  command);

static esp_err_t internal_star_bus_i2c_read_raw(const star_bus_config_t* config,
                                                uint8_t*                 data,
                                                size_t                   len,
                                                size_t*                  bytes_read);

/**
 * @brief Helper: Execute I2C command with retry logic
 *
 * Wraps i2c_master_cmd_begin() with automatic retry for transient errors.
 * Distinguishes between transient errors (timeout, general failure) that
 * should be retried and permanent errors (invalid parameters) that shouldn't.
 *
 * @param[in] config Pointer to I2C bus configuration
 * @param[in] port I2C port number
 * @param[in] cmd I2C command handle
 * @param[in] operation_name Name of operation for logging
 * @return esp_err_t Final result after retries
 */
static esp_err_t internal_i2c_execute_with_retry(const star_bus_config_t* config,
                                                 i2c_port_t               port,
                                                 i2c_cmd_handle_t         cmd,
                                                 const char*              operation_name);

/* --- Public Functions --- */

void star_bus_i2c_init_default_ops(star_i2c_ops_t* ops)
{
  if (ops == NULL) {
    ESP_LOGE(s_TAG, "Cannot initialize NULL ops pointer");
    return;
  }
  ops->write         = internal_star_bus_i2c_write;
  ops->read          = internal_star_bus_i2c_read;
  ops->write_command = internal_star_bus_i2c_write_command;
  ops->read_raw      = internal_star_bus_i2c_read_raw;
  ESP_LOGD(s_TAG, "Default I2C operations initialized");
}

esp_err_t star_bus_i2c_write(const star_bus_manager_t* manager,
                             const char*               name,
                             const uint8_t*            data,
                             size_t                    len,
                             uint8_t                   reg_addr,
                             size_t*                   bytes_written)
{
  ESP_RETURN_ON_FALSE(manager && name && data,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager, name, or data is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_TAG, "Write length must be > 0");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_i2c,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an I2C bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.i2c.ops.write,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No write op for '%s'",
                      name);

  /* Clear bytes_written initially */
  if (bytes_written) {
    *bytes_written = 0;
  }

  return bus_config->proto.i2c.ops.write(bus_config, data, len, reg_addr, bytes_written);
}

esp_err_t star_bus_i2c_read(const star_bus_manager_t* manager,
                            const char*               name,
                            uint8_t*                  data,
                            size_t                    len,
                            uint8_t                   reg_addr,
                            size_t*                   bytes_read)
{
  ESP_RETURN_ON_FALSE(manager && name && data,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager, name, or data is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_TAG, "Read length must be > 0");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_i2c,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an I2C bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.i2c.ops.read,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No read op for '%s'",
                      name);

  /* Clear bytes_read initially */
  if (bytes_read) {
    *bytes_read = 0;
  }

  return bus_config->proto.i2c.ops.read(bus_config, data, len, reg_addr, bytes_read);
}

esp_err_t
star_bus_i2c_write_command(const star_bus_manager_t* manager, const char* name, uint8_t command)
{
  ESP_RETURN_ON_FALSE(manager && name, ESP_ERR_INVALID_ARG, s_TAG, "Manager or name is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_i2c,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an I2C bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.i2c.ops.write_command,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No write_command op for '%s'",
                      name);

  return bus_config->proto.i2c.ops.write_command(bus_config, command);
}

esp_err_t star_bus_i2c_read_raw(const star_bus_manager_t* manager,
                                const char*               name,
                                uint8_t*                  data,
                                size_t                    len,
                                size_t*                   bytes_read)
{
  ESP_RETURN_ON_FALSE(manager && name && data,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager, name, or data is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_TAG, "Read length must be > 0");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_i2c,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an I2C bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.i2c.ops.read_raw,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No read_raw op for '%s'",
                      name);

  /* Clear bytes_read initially */
  if (bytes_read) {
    *bytes_read = 0;
  }

  return bus_config->proto.i2c.ops.read_raw(bus_config, data, len, bytes_read);
}

/* --- Private Functions (Default Ops Implementations) --- */

/**
 * @brief Helper: Execute I2C command
 *
 * Note: Retry logic has been removed as error handling is now done at the manager level
 * through the injected error_iface. This function performs a single attempt.
 */
static esp_err_t internal_i2c_execute_with_retry(const star_bus_config_t* config,
                                                 i2c_port_t               port,
                                                 i2c_cmd_handle_t         cmd,
                                                 const char*              operation_name)
{
  esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(s_i2c_timeout_ms));

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "%s '%s': Error %s", operation_name, config->name, esp_err_to_name(ret));
  }

  return ret;
}

static esp_err_t internal_star_bus_i2c_write(const star_bus_config_t* config,
                                             const uint8_t*           data,
                                             size_t                   len,
                                             uint8_t                  reg_addr,
                                             size_t*                  bytes_written)
{
  /* Basic validation (config, data assumed non-NULL, len > 0 by caller) */
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus '%s' is not initialized",
                      config->name);

  i2c_port_t       port    = config->proto.i2c.port;
  uint8_t          address = config->proto.i2c.address;
  esp_err_t        ret     = ESP_OK;
  i2c_cmd_handle_t cmd     = NULL;

  cmd = i2c_cmd_link_create();
  ESP_RETURN_ON_FALSE(cmd,
                      ESP_ERR_NO_MEM,
                      s_TAG,
                      "Failed to create I2C command link for write '%s'",
                      config->name);

  ret = i2c_master_start(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    write_fail,
                    s_TAG,
                    "Write '%s': START failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  ESP_GOTO_ON_ERROR(ret,
                    write_fail,
                    s_TAG,
                    "Write '%s': Address (W) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Write register address */
  ret = i2c_master_write_byte(cmd, reg_addr, true);
  ESP_GOTO_ON_ERROR(ret,
                    write_fail,
                    s_TAG,
                    "Write '%s': Reg Addr failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Write data */
  ret = i2c_master_write(cmd, data, len, true);
  ESP_GOTO_ON_ERROR(ret,
                    write_fail,
                    s_TAG,
                    "Write '%s': Data write failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_stop(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    write_fail,
                    s_TAG,
                    "Write '%s': STOP failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Execute with retry logic */
  ret = internal_i2c_execute_with_retry(config, port, cmd, "Write");
  ESP_GOTO_ON_ERROR(ret, write_fail, s_TAG, "Write '%s': CMD failed after retries", config->name);

  /* Success path */
  i2c_cmd_link_delete(cmd);
  cmd = NULL; /* Prevent double delete in fail path */
  ESP_LOGD(s_TAG,
           "Write Success: %zu bytes to '%s' (Addr 0x%02X, Reg 0x%02X)",
           len,
           config->name,
           address,
           reg_addr);
  if (bytes_written) {
    *bytes_written = len;
  }

  /* Call success callback */
  if (config->proto.i2c.callbacks.on_transfer_complete) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = true, .len = len}};
    config->proto.i2c.callbacks.on_transfer_complete(&event, config->user_ctx);
  }
  return ESP_OK;

write_fail:
  if (cmd) {
    i2c_cmd_link_delete(cmd);
  }
  ESP_LOGE(s_TAG, "Write failed for '%s': %s", config->name, esp_err_to_name(ret));
  /* Call error callback */
  if (config->proto.i2c.callbacks.on_error) {
    star_bus_event_t event = {.bus_type = k_star_bus_type_i2c,
                              .bus_name = config->name,
                              .i2c      = {.port     = port,
                                           .address  = address,
                                           .is_write = true,
                                           .len      = 0}}; /* Len is 0 on error */
    config->proto.i2c.callbacks.on_error(&event, ret, config->user_ctx);
  }
  return ret;
}

static esp_err_t internal_star_bus_i2c_read(const star_bus_config_t* config,
                                            uint8_t*                 data,
                                            size_t                   len,
                                            uint8_t                  reg_addr,
                                            size_t*                  bytes_read)
{
  /* Basic validation (config, data assumed non-NULL, len > 0 by caller) */
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus '%s' is not initialized",
                      config->name);

  i2c_port_t       port    = config->proto.i2c.port;
  uint8_t          address = config->proto.i2c.address;
  esp_err_t        ret     = ESP_OK;
  i2c_cmd_handle_t cmd     = NULL;

  cmd = i2c_cmd_link_create();
  ESP_RETURN_ON_FALSE(cmd,
                      ESP_ERR_NO_MEM,
                      s_TAG,
                      "Failed to create I2C command link for read '%s'",
                      config->name);

  /* --- Write Phase (Device Address + Register Address) --- */
  ret = i2c_master_start(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': START failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': Address (W) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, reg_addr, true);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': Reg Addr failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* --- Read Phase (Repeated Start + Device Address + Data) --- */
  ret = i2c_master_start(cmd); /* Repeated start */
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': Repeated START failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': Address (R) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  if (len > 1) {
    ret = i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    ESP_GOTO_ON_ERROR(ret,
                      read_fail,
                      s_TAG,
                      "Read '%s': Data read (ACK) failed: %s",
                      config->name,
                      esp_err_to_name(ret));
  }

  /* Read last byte with NACK */
  ret = i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': Data read (NACK) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_stop(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    read_fail,
                    s_TAG,
                    "Read '%s': STOP failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Execute with retry logic */
  ret = internal_i2c_execute_with_retry(config, port, cmd, "Read");
  ESP_GOTO_ON_ERROR(ret, read_fail, s_TAG, "Read '%s': CMD failed after retries", config->name);

  /* Success path */
  i2c_cmd_link_delete(cmd);
  cmd = NULL; /* Prevent double delete in fail path */
  ESP_LOGD(s_TAG,
           "Read Success: %zu bytes from '%s' (Addr 0x%02X, Reg 0x%02X)",
           len,
           config->name,
           address,
           reg_addr);
  if (bytes_read) {
    *bytes_read = len;
  }

  /* Call success callback */
  if (config->proto.i2c.callbacks.on_transfer_complete) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = false, .len = len}};
    config->proto.i2c.callbacks.on_transfer_complete(&event, config->user_ctx);
  }
  return ESP_OK;

read_fail:
  if (cmd) {
    i2c_cmd_link_delete(cmd);
  }
  ESP_LOGE(s_TAG, "Read failed for '%s': %s", config->name, esp_err_to_name(ret));
  /* Call error callback */
  if (config->proto.i2c.callbacks.on_error) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = false, .len = 0}};
    config->proto.i2c.callbacks.on_error(&event, ret, config->user_ctx);
  }
  return ret;
}

static esp_err_t internal_star_bus_i2c_write_command(const star_bus_config_t* config,
                                                     uint8_t                  command)
{
  /* Basic validation (config assumed non-NULL by caller) */
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus '%s' is not initialized",
                      config->name);

  i2c_port_t       port    = config->proto.i2c.port;
  uint8_t          address = config->proto.i2c.address;
  esp_err_t        ret     = ESP_OK;
  i2c_cmd_handle_t cmd     = NULL;

  cmd = i2c_cmd_link_create();
  ESP_RETURN_ON_FALSE(cmd,
                      ESP_ERR_NO_MEM,
                      s_TAG,
                      "Failed to create I2C command link for write_command '%s'",
                      config->name);

  ret = i2c_master_start(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    write_cmd_fail,
                    s_TAG,
                    "WriteCmd '%s': START failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  ESP_GOTO_ON_ERROR(ret,
                    write_cmd_fail,
                    s_TAG,
                    "WriteCmd '%s': Address (W) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Write command byte */
  ret = i2c_master_write_byte(cmd, command, true);
  ESP_GOTO_ON_ERROR(ret,
                    write_cmd_fail,
                    s_TAG,
                    "WriteCmd '%s': Command byte failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_stop(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    write_cmd_fail,
                    s_TAG,
                    "WriteCmd '%s': STOP failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Execute with retry logic */
  ret = internal_i2c_execute_with_retry(config, port, cmd, "WriteCmd");
  ESP_GOTO_ON_ERROR(ret,
                    write_cmd_fail,
                    s_TAG,
                    "WriteCmd '%s': CMD failed after retries",
                    config->name);

  /* Success path */
  i2c_cmd_link_delete(cmd);
  cmd = NULL; /* Prevent double delete in fail path */
  ESP_LOGD(s_TAG,
           "Write Command Success: 0x%02X to '%s' (Addr 0x%02X)",
           command,
           config->name,
           address);

  /* Call success callback (treat as write with len=0?) */
  if (config->proto.i2c.callbacks.on_transfer_complete) {
    star_bus_event_t event = {.bus_type = k_star_bus_type_i2c,
                              .bus_name = config->name,
                              .i2c      = {.port     = port,
                                           .address  = address,
                                           .is_write = true,
                                           .len      = 0}}; /* Indicate command write */
    config->proto.i2c.callbacks.on_transfer_complete(&event, config->user_ctx);
  }
  return ESP_OK;

write_cmd_fail:
  if (cmd) {
    i2c_cmd_link_delete(cmd);
  }
  ESP_LOGE(s_TAG, "Write Command failed for '%s': %s", config->name, esp_err_to_name(ret));
  /* Call error callback */
  if (config->proto.i2c.callbacks.on_error) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = true, .len = 0}};
    config->proto.i2c.callbacks.on_error(&event, ret, config->user_ctx);
  }
  return ret;
}

static esp_err_t internal_star_bus_i2c_read_raw(const star_bus_config_t* config,
                                                uint8_t*                 data,
                                                size_t                   len,
                                                size_t*                  bytes_read)
{
  /* Basic validation (config, data assumed non-NULL, len > 0 by caller) */
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "Bus '%s' is not initialized",
                      config->name);

  i2c_port_t       port    = config->proto.i2c.port;
  uint8_t          address = config->proto.i2c.address;
  esp_err_t        ret     = ESP_OK;
  i2c_cmd_handle_t cmd     = NULL;

  cmd = i2c_cmd_link_create();
  ESP_RETURN_ON_FALSE(cmd,
                      ESP_ERR_NO_MEM,
                      s_TAG,
                      "Failed to create I2C command link for read_raw '%s'",
                      config->name);

  /* --- Read Phase (Start + Device Address + Data) --- */
  ret = i2c_master_start(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    read_raw_fail,
                    s_TAG,
                    "ReadRaw '%s': START failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
  ESP_GOTO_ON_ERROR(ret,
                    read_raw_fail,
                    s_TAG,
                    "ReadRaw '%s': Address (R) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  if (len > 1) {
    ret = i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    ESP_GOTO_ON_ERROR(ret,
                      read_raw_fail,
                      s_TAG,
                      "ReadRaw '%s': Data read (ACK) failed: %s",
                      config->name,
                      esp_err_to_name(ret));
  }

  /* Read last byte with NACK */
  ret = i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
  ESP_GOTO_ON_ERROR(ret,
                    read_raw_fail,
                    s_TAG,
                    "ReadRaw '%s': Data read (NACK) failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  ret = i2c_master_stop(cmd);
  ESP_GOTO_ON_ERROR(ret,
                    read_raw_fail,
                    s_TAG,
                    "ReadRaw '%s': STOP failed: %s",
                    config->name,
                    esp_err_to_name(ret));

  /* Execute with retry logic */
  ret = internal_i2c_execute_with_retry(config, port, cmd, "ReadRaw");
  ESP_GOTO_ON_ERROR(ret,
                    read_raw_fail,
                    s_TAG,
                    "ReadRaw '%s': CMD failed after retries",
                    config->name);

  /* Success path */
  i2c_cmd_link_delete(cmd);
  cmd = NULL; /* Prevent double delete in fail path */
  ESP_LOGD(s_TAG,
           "Read Raw Success: %zu bytes from '%s' (Addr 0x%02X)",
           len,
           config->name,
           address);
  if (bytes_read) {
    *bytes_read = len;
  }

  /* Call success callback */
  if (config->proto.i2c.callbacks.on_transfer_complete) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = false, .len = len}};
    config->proto.i2c.callbacks.on_transfer_complete(&event, config->user_ctx);
  }
  return ESP_OK;

read_raw_fail:
  if (cmd) {
    i2c_cmd_link_delete(cmd);
  }
  ESP_LOGE(s_TAG, "Read Raw failed for '%s': %s", config->name, esp_err_to_name(ret));
  /* Call error callback */
  if (config->proto.i2c.callbacks.on_error) {
    star_bus_event_t event = {
      .bus_type = k_star_bus_type_i2c,
      .bus_name = config->name,
      .i2c      = {.port = port, .address = address, .is_write = false, .len = 0}};
    config->proto.i2c.callbacks.on_error(&event, ret, config->user_ctx);
  }
  return ret;
}
