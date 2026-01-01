/* lib/rx_bus/src/rx_bus_i2c.c */

/**
 * @file rx_bus_i2c.c
 * @brief I2C bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe I2C operations through bus manager.
 * Wraps low-level RIIC HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_bus_i2c.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_I2C";

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @brief Context for I2C init operation
 */
typedef struct {
  rx_err_t result; /**< Operation result */
} i2c_init_ctx_t;

/**
 * @brief Context for I2C write operation
 */
typedef struct {
  const uint8_t* data;   /**< Pointer to data to write */
  uint16_t       length; /**< Number of bytes to write */
  rx_err_t       result; /**< Operation result */
} i2c_write_ctx_t;

/**
 * @brief Context for I2C read operation
 */
typedef struct {
  uint8_t* data;   /**< Pointer to buffer for received data */
  uint16_t length; /**< Number of bytes to read */
  rx_err_t result; /**< Operation result */
} i2c_read_ctx_t;

/**
 * @brief Context for I2C write-read operation
 */
typedef struct {
  const uint8_t* write_data;   /**< Pointer to data to write */
  uint16_t       write_length; /**< Number of bytes to write */
  uint8_t*       read_data;    /**< Pointer to buffer for received data */
  uint16_t       read_length;  /**< Number of bytes to read */
  rx_err_t       result;       /**< Operation result */
} i2c_write_read_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Callback for I2C initialization
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (i2c_init_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_i2c_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_init_ctx_t* ctx = (i2c_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_i2c) {
    rx_log_error(s_tag, "Bus is not I2C type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize RIIC channel */
  rx_err_t err = riic_init(bus_config->proto.i2c.channel, bus_config->proto.i2c.frequency_hz);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "RIIC HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for I2C write operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (i2c_write_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_i2c_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_write_ctx_t* ctx = (i2c_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write I2C data */
  rx_err_t err = riic_write(bus_config->proto.i2c.channel,
                            bus_config->proto.i2c.device_addr,
                            ctx->data,
                            ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C write failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for I2C read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (i2c_read_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_i2c_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_read_ctx_t* ctx = (i2c_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read I2C data */
  rx_err_t err = riic_read(bus_config->proto.i2c.channel,
                           bus_config->proto.i2c.device_addr,
                           ctx->data,
                           ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for I2C write-read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (i2c_write_read_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_i2c_write_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  i2c_write_read_ctx_t* ctx = (i2c_write_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* I2C write-read operation */
  rx_err_t err = riic_write_read(bus_config->proto.i2c.channel,
                                 bus_config->proto.i2c.device_addr,
                                 ctx->write_data,
                                 ctx->write_length,
                                 ctx->read_data,
                                 ctx->read_length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "I2C write-read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_i2c_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  i2c_init_ctx_t ctx = {.result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_i2c_write(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          const uint8_t*    data,
                          uint16_t          length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  i2c_write_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t
rx_bus_i2c_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint16_t length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  i2c_read_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_i2c_write_read(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               const uint8_t*    write_data,
                               uint16_t          write_length,
                               uint8_t*          read_data,
                               uint16_t          read_length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(write_data, s_tag, "write_data pointer is NULL");
  RX_CHECK_NULL_PTR(read_data, s_tag, "read_data pointer is NULL");

  i2c_write_read_ctx_t ctx = {.write_data   = write_data,
                              .write_length = write_length,
                              .read_data    = read_data,
                              .read_length  = read_length,
                              .result       = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
