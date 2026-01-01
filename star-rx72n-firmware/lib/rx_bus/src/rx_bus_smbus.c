/* lib/rx_bus/src/rx_bus_smbus.c */

/**
 * @file rx_bus_smbus.c
 * @brief SMBUS bus abstraction implementation for RX72N
 * @details
 * Implements SMBUS protocol on top of I2C with CRC-8 PEC support.
 * Used for fuel gauge communication following SMBUS 2.0 specification.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_bus_smbus.h"

#include "hardware.h"
#include "rx_bus_i2c.h"
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_SMBUS";

/* =============================================================================
 * CRC-8 for SMBUS PEC
 * =============================================================================
 */

/**
 * @brief CRC-8 polynomial for SMBUS (x^8 + x^2 + x + 1)
 */
typedef enum {
  k_smbus_crc8_poly = 0x07,
  k_smbus_crc8_init = 0x00,
} smbus_crc8_constants_t;

/**
 * @brief Calculate CRC-8 for SMBUS PEC
 *
 * @param[in] crc Initial CRC value
 * @param[in] data Pointer to data
 * @param[in] length Number of bytes
 *
 * @return Updated CRC-8 value
 */
static uint8_t internal_crc8(uint8_t crc, const uint8_t* data, uint16_t length)
{
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < k_bits_per_byte; bit++) {
      if (crc & k_byte_msb_mask) {
        crc = (crc << k_i2c_addr_shift) ^ k_smbus_crc8_poly;
      } else {
        crc = (crc << k_i2c_addr_shift);
      }
    }
  }
  return crc;
}

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @brief Context for SMBUS init operation
 */
typedef struct {
  rx_err_t result; /**< Operation result */
} smbus_init_ctx_t;

/**
 * @brief Context for SMBUS write byte operation
 */
typedef struct {
  uint8_t  command; /**< Command byte to write */
  rx_err_t result;  /**< Operation result */
} smbus_write_byte_ctx_t;

/**
 * @brief Context for SMBUS read byte operation
 */
typedef struct {
  uint8_t* data;   /**< Pointer to store received byte */
  rx_err_t result; /**< Operation result */
} smbus_read_byte_ctx_t;

/**
 * @brief Context for SMBUS write byte data operation
 */
typedef struct {
  uint8_t  command; /**< Register/command code */
  uint8_t  data;    /**< Data byte to write */
  rx_err_t result;  /**< Operation result */
} smbus_write_byte_data_ctx_t;

/**
 * @brief Context for SMBUS read byte data operation
 */
typedef struct {
  uint8_t  command; /**< Register/command code */
  uint8_t* data;    /**< Pointer to store received byte */
  rx_err_t result;  /**< Operation result */
} smbus_read_byte_data_ctx_t;

/**
 * @brief Context for SMBUS write word data operation
 */
typedef struct {
  uint8_t  command; /**< Register/command code */
  uint16_t data;    /**< Data word to write */
  rx_err_t result;  /**< Operation result */
} smbus_write_word_data_ctx_t;

/**
 * @brief Context for SMBUS read word data operation
 */
typedef struct {
  uint8_t   command; /**< Register/command code */
  uint16_t* data;    /**< Pointer to store received word */
  rx_err_t  result;  /**< Operation result */
} smbus_read_word_data_ctx_t;

/**
 * @brief Context for SMBUS read block data operation
 */
typedef struct {
  uint8_t  command;    /**< Register/command code */
  uint8_t* data;       /**< Pointer to buffer for received data */
  uint8_t* length;     /**< Pointer to store number of bytes read */
  uint8_t  max_length; /**< Maximum buffer size */
  rx_err_t result;     /**< Operation result */
} smbus_read_block_data_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

static rx_err_t internal_smbus_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  smbus_init_ctx_t* ctx = (smbus_init_ctx_t*)user_ctx;

  if (bus_config->type != k_bus_type_smbus) {
    RX_LOG_ERROR(s_tag, "Bus is not SMBUS type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize underlying I2C channel */
  rx_err_t err = riic_init(bus_config->proto.smbus.i2c_config.channel,
                           bus_config->proto.smbus.i2c_config.frequency_hz);

  if (err != k_rx_ok) {
    RX_LOG_ERROR(s_tag, "RIIC initialization failed");
    ctx->result = err;
    return err;
  }

  bus_config->initialized = true;
  ctx->result             = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_smbus_write_byte_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  smbus_write_byte_ctx_t* ctx = (smbus_write_byte_ctx_t*)user_ctx;

  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  uint8_t data[k_smbus_byte_buf_size];
  uint8_t length = k_smbus_single_byte;

  data[k_smbus_byte_data] = ctx->command;

  /* Calculate PEC if enabled */
  if (bus_config->proto.smbus.use_pec) {
    uint8_t crc = k_smbus_crc8_init;
    uint8_t addr_byte =
      (bus_config->proto.smbus.i2c_config.device_addr << k_i2c_addr_shift) | k_i2c_write_bit;
    crc                    = internal_crc8(crc, &addr_byte, k_smbus_single_byte);
    crc                    = internal_crc8(crc, data, k_smbus_single_byte);
    data[k_smbus_byte_pec] = crc;
    length                 = k_smbus_byte_buf_size;
  }

  rx_err_t err = riic_write(bus_config->proto.smbus.i2c_config.channel,
                            bus_config->proto.smbus.i2c_config.device_addr,
                            data,
                            length);

  ctx->result = err;
  return err;
}

static rx_err_t internal_smbus_read_byte_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  smbus_read_byte_ctx_t* ctx = (smbus_read_byte_ctx_t*)user_ctx;

  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  uint8_t  data[k_smbus_byte_buf_size];
  uint8_t  length = bus_config->proto.smbus.use_pec ? k_smbus_byte_buf_size : k_smbus_single_byte;
  rx_err_t err    = riic_read(bus_config->proto.smbus.i2c_config.channel,
                           bus_config->proto.smbus.i2c_config.device_addr,
                           data,
                           length);

  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  /* Verify PEC if enabled */
  if (bus_config->proto.smbus.use_pec) {
    uint8_t crc = k_smbus_crc8_init;
    uint8_t addr_byte =
      (bus_config->proto.smbus.i2c_config.device_addr << k_i2c_addr_shift) | k_i2c_read_bit;
    crc = internal_crc8(crc, &addr_byte, k_smbus_single_byte);
    crc = internal_crc8(crc, data, k_smbus_single_byte);

    if (crc != data[k_smbus_byte_pec]) {
      RX_LOG_ERROR(s_tag, "PEC mismatch");
      ctx->result = k_rx_err_crc_mismatch;
      return k_rx_err_crc_mismatch;
    }
  }

  *ctx->data  = data[k_smbus_byte_data];
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

static rx_err_t internal_smbus_read_word_data_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  smbus_read_word_data_ctx_t* ctx = (smbus_read_word_data_ctx_t*)user_ctx;

  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  uint8_t write_data = ctx->command;
  uint8_t read_data[k_smbus_word_buf_size];
  uint8_t read_length =
    bus_config->proto.smbus.use_pec ? k_smbus_word_buf_size : k_smbus_word_data_bytes;
  rx_err_t err = riic_write_read(bus_config->proto.smbus.i2c_config.channel,
                                 bus_config->proto.smbus.i2c_config.device_addr,
                                 &write_data,
                                 k_smbus_single_byte,
                                 read_data,
                                 read_length);

  if (err != k_rx_ok) {
    ctx->result = err;
    return err;
  }

  /* Verify PEC if enabled */
  if (bus_config->proto.smbus.use_pec) {
    uint8_t crc = k_smbus_crc8_init;
    uint8_t addr_byte =
      (bus_config->proto.smbus.i2c_config.device_addr << k_i2c_addr_shift) | k_i2c_write_bit;
    crc = internal_crc8(crc, &addr_byte, k_smbus_single_byte);
    crc = internal_crc8(crc, &write_data, k_smbus_single_byte);
    addr_byte =
      (bus_config->proto.smbus.i2c_config.device_addr << k_i2c_addr_shift) | k_i2c_read_bit;
    crc = internal_crc8(crc, &addr_byte, k_smbus_single_byte);
    crc = internal_crc8(crc, read_data, k_smbus_word_data_bytes);

    if (crc != read_data[k_smbus_word_pec]) {
      RX_LOG_ERROR(s_tag, "PEC mismatch");
      ctx->result = k_rx_err_crc_mismatch;
      return k_rx_err_crc_mismatch;
    }
  }

  /* Little-endian */
  *ctx->data = (uint16_t)read_data[k_smbus_word_lsb] |
               ((uint16_t)read_data[k_smbus_word_msb] << k_bits_per_byte);
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_smbus_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  smbus_init_ctx_t ctx = {.result = k_rx_err_hw_error};
  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_smbus_init_callback, &ctx);

  return (err != k_rx_ok) ? err : ctx.result;
}

rx_err_t rx_bus_smbus_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t command)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  smbus_write_byte_ctx_t ctx = {.command = command, .result = k_rx_err_hw_error};
  rx_err_t               err =
    rx_bus_manager_with_bus(manager, bus_name, internal_smbus_write_byte_callback, &ctx);

  return (err != k_rx_ok) ? err : ctx.result;
}

rx_err_t rx_bus_smbus_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  smbus_read_byte_ctx_t ctx = {.data = data, .result = k_rx_err_hw_error};
  rx_err_t              err =
    rx_bus_manager_with_bus(manager, bus_name, internal_smbus_read_byte_callback, &ctx);

  return (err != k_rx_ok) ? err : ctx.result;
}

rx_err_t rx_bus_smbus_write_byte_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t           data)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Use I2C write for byte data (command + data) */
  uint8_t write_buf[k_smbus_byte_buf_size] = {command, data};
  return rx_bus_i2c_write(manager, bus_name, write_buf, k_smbus_byte_buf_size);
}

rx_err_t rx_bus_smbus_read_byte_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint8_t*          data)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  /* Use I2C write-read for byte data */
  return rx_bus_i2c_write_read(manager,
                               bus_name,
                               &command,
                               k_smbus_single_byte,
                               data,
                               k_smbus_single_byte);
}

rx_err_t rx_bus_smbus_write_word_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint16_t          data)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Little-endian */
  uint8_t write_buf[k_smbus_word_buf_size] = {command,
                                              (uint8_t)(data & k_byte_mask),
                                              (uint8_t)(data >> k_bits_per_byte)};
  return rx_bus_i2c_write(manager, bus_name, write_buf, k_smbus_word_buf_size);
}

rx_err_t rx_bus_smbus_read_word_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint16_t*         data)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  smbus_read_word_data_ctx_t ctx = {.command = command, .data = data, .result = k_rx_err_hw_error};
  rx_err_t                   err =
    rx_bus_manager_with_bus(manager, bus_name, internal_smbus_read_word_data_callback, &ctx);

  return (err != k_rx_ok) ? err : ctx.result;
}

rx_err_t rx_bus_smbus_read_block_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t*          data,
                                      uint8_t*          length,
                                      uint8_t           max_length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");
  RX_CHECK_NULL_PTR(length, s_tag, "length pointer is NULL");

  /* Read length byte first, then data */
  uint8_t  len_byte;
  rx_err_t err = rx_bus_i2c_write_read(manager,
                                       bus_name,
                                       &command,
                                       k_smbus_single_byte,
                                       &len_byte,
                                       k_smbus_single_byte);
  if (err != k_rx_ok) {
    return err;
  }

  if (len_byte > max_length) {
    RX_LOG_ERROR(s_tag, "Block length exceeds buffer");
    return k_rx_err_invalid_size;
  }

  /* Read data bytes */
  err = rx_bus_i2c_read(manager, bus_name, data, len_byte);
  if (err != k_rx_ok) {
    return err;
  }

  *length = len_byte;
  return k_rx_ok;
}
