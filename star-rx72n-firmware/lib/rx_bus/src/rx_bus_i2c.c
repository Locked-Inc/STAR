/* lib/rx_bus/src/rx_bus_i2c.c */

/**
 * @file rx_bus_i2c.c
 * @brief I2C bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe I2C operations through bus manager.
 * Wraps low-level RIIC HAL with bus abstraction pattern.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_i2c.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_I2C";

/**
 * @brief I2C validation constants
 */
typedef enum : uint16_t {
  k_i2c_length_zero = 0, /**< Zero length value for comparisons */
} i2c_validation_constants_t;

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
  rx_err_t        err;
  riic_channel_t  riic_channel;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_i2c) {
    rx_log_error(s_tag, "Bus is not I2C type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize RIIC channel */
  riic_channel.value = bus_config->proto.i2c.channel;
  err                = riic_init(riic_channel, bus_config->proto.i2c.frequency_hz);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "RIIC HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify I2C channel is responsive (check configuration) */
  if (bus_config->proto.i2c.device_addr > k_i2c_addr_max_7bit) {
    rx_log_warn(s_tag, "I2C device address exceeds 7-bit maximum");
    /* Continue anyway - HAL should validate, but flag if misconfigured */
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
  i2c_write_ctx_t*  ctx = (i2c_write_ctx_t*)user_ctx;
  riic_channel_t    riic_channel;
  i2c_device_addr_t device_addr;
  rx_err_t          err;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate data pointer when length > 0 */
  if (ctx->length > k_i2c_length_zero && ctx->data == NULL) {
    rx_log_error(s_tag, "I2C write data pointer is NULL");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Write I2C data */
  riic_channel.value = bus_config->proto.i2c.channel;
  device_addr.value  = bus_config->proto.i2c.device_addr;
  err                = riic_write(riic_channel, device_addr, ctx->data, ctx->length);

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
  i2c_read_ctx_t*   ctx = (i2c_read_ctx_t*)user_ctx;
  riic_channel_t    riic_channel;
  i2c_device_addr_t device_addr;
  rx_err_t          err;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate data pointer when length > 0 */
  if (ctx->length > k_i2c_length_zero && ctx->data == NULL) {
    rx_log_error(s_tag, "I2C read data pointer is NULL");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Read I2C data */
  riic_channel.value = bus_config->proto.i2c.channel;
  device_addr.value  = bus_config->proto.i2c.device_addr;
  err                = riic_read(riic_channel, device_addr, ctx->data, ctx->length);

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
  riic_channel_t        riic_channel;
  i2c_device_addr_t     device_addr;
  rx_err_t              err;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate write data pointer when write_length > 0 */
  if (ctx->write_length > 0 && ctx->write_data == NULL) {
    rx_log_error(s_tag, "I2C write-read write_data pointer is NULL");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: Validate read data pointer when read_length > 0 */
  if (ctx->read_length > 0 && ctx->read_data == NULL) {
    rx_log_error(s_tag, "I2C write-read read_data pointer is NULL");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* I2C write-read operation */
  riic_channel.value = bus_config->proto.i2c.channel;
  device_addr.value  = bus_config->proto.i2c.device_addr;
  err                = riic_write_read(riic_channel,
                                                device_addr,
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
  i2c_init_ctx_t ctx;
  rx_err_t       err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  ctx.result = k_rx_err_hw_error;

  err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_i2c_write(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          const uint8_t*    data,
                          const uint16_t    length)
{
  i2c_write_ctx_t ctx;
  rx_err_t        err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  ctx.data   = data;
  ctx.length = length;
  ctx.result = k_rx_err_hw_error;

  err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t
rx_bus_i2c_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint16_t length)
{
  i2c_read_ctx_t ctx;
  rx_err_t       err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  ctx.data   = data;
  ctx.length = length;
  ctx.result = k_rx_err_hw_error;

  err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_i2c_write_read(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               const uint8_t*    write_data,
                               const uint16_t    write_length,
                               uint8_t*          read_data,
                               const uint16_t    read_length)
{
  i2c_write_read_ctx_t ctx;
  rx_err_t             err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(write_data, s_tag, "write_data pointer is NULL");
  RX_CHECK_NULL_PTR(read_data, s_tag, "read_data pointer is NULL");

  ctx.write_data   = write_data;
  ctx.write_length = write_length;
  ctx.read_data    = read_data;
  ctx.read_length  = read_length;
  ctx.result       = k_rx_err_hw_error;

  err = rx_bus_manager_with_bus(manager, bus_name, internal_i2c_write_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
