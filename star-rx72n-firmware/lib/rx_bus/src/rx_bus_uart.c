/* lib/rx_bus/src/rx_bus_uart.c */

/**
 * @file rx_bus_uart.c
 * @brief UART bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe UART operations through bus manager.
 * Wraps low-level SCI UART HAL with bus abstraction pattern.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_uart.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_UART";

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @brief Context for UART init operation
 */
typedef struct {
  rx_err_t result; /**< Operation result */
} uart_init_ctx_t;

/**
 * @brief Context for UART write operation
 */
typedef struct {
  const uint8_t* data;   /**< Pointer to data to write */
  uint16_t       length; /**< Number of bytes to write */
  rx_err_t       result; /**< Operation result */
} uart_write_ctx_t;

/**
 * @brief Context for UART read operation
 */
typedef struct {
  uint8_t* data;       /**< Pointer to buffer for received data */
  uint16_t length;     /**< Maximum bytes to read */
  uint16_t bytes_read; /**< Actual bytes read */
  rx_err_t result;     /**< Operation result */
} uart_read_ctx_t;

/**
 * @brief Context for UART putc operation
 */
typedef struct {
  char     c;      /**< Character to write */
  rx_err_t result; /**< Operation result */
} uart_putc_ctx_t;

/**
 * @brief Context for UART puts operation
 */
typedef struct {
  const char* str;    /**< String to write */
  rx_err_t    result; /**< Operation result */
} uart_puts_ctx_t;

/**
 * @brief Context for UART getc operation
 */
typedef struct {
  char     c;      /**< Character received */
  rx_err_t result; /**< Operation result */
} uart_getc_ctx_t;

/**
 * @brief Context for UART rx_available operation
 */
typedef struct {
  bool     available; /**< True if data available */
  rx_err_t result;    /**< Operation result */
} uart_rx_avail_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Callback for UART initialization
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_init_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_init_ctx_t* ctx = (uart_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_uart) {
    rx_log_error(s_tag, "Bus is not UART type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize SCI channel with full hardware configuration */
  rx_err_t err = uart_init_channel(bus_config->proto.uart.channel,
                                   bus_config->proto.uart.baudrate,
                                   bus_config->proto.uart.tx_port,
                                   bus_config->proto.uart.tx_pin,
                                   bus_config->proto.uart.rx_port,
                                   bus_config->proto.uart.rx_pin);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART HAL initialization failed");
    ctx->result = k_rx_err_hw_init_failed;
    return k_rx_err_hw_init_failed;
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  rx_log_debug(s_tag, "UART bus initialized");
  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for UART write operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_write_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_write_ctx_t* ctx = (uart_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write UART data */
  rx_err_t err =
      uart_write_channel(bus_config->proto.uart.channel, ctx->data, ctx->length);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART write failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for UART read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_read_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_read_ctx_t* ctx = (uart_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read UART data */
  rx_err_t err = uart_read_channel(bus_config->proto.uart.channel,
                                   ctx->data,
                                   ctx->length,
                                   &ctx->bytes_read);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for UART putc operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_putc_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_putc_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_putc_ctx_t* ctx = (uart_putc_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write single character */
  rx_err_t err = uart_putc_channel(bus_config->proto.uart.channel, ctx->c);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART putc failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for UART puts operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_puts_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_puts_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_puts_ctx_t* ctx = (uart_puts_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write string */
  rx_err_t err = uart_puts_channel(bus_config->proto.uart.channel, ctx->str);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART puts failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for UART getc operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_getc_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_getc_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_getc_ctx_t* ctx = (uart_getc_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read single character */
  rx_err_t err = uart_getc_channel(bus_config->proto.uart.channel, &ctx->c);

  /* k_rx_err_empty is a valid return (no data available) */
  ctx->result = err;
  return k_rx_ok;
}

/**
 * @brief Callback for UART rx_available operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (uart_rx_avail_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_uart_rx_avail_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  uart_rx_avail_ctx_t* ctx = (uart_rx_avail_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Check RX data availability */
  rx_err_t err = uart_rx_available(bus_config->proto.uart.channel, &ctx->available);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART rx_available failed");
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

rx_err_t rx_bus_uart_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  uart_init_ctx_t ctx = {.result = k_rx_err_hw_init_failed};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_write(rx_bus_manager_t* manager,
                           const char*       bus_name,
                           const uint8_t*    data,
                           uint16_t          length)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");

  uart_write_ctx_t ctx = {.data = data, .length = length, .result = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_read(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          uint8_t*          data,
                          uint16_t          length,
                          uint16_t*         bytes_read)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "data pointer is NULL");
  RX_CHECK_NULL_PTR(bytes_read, s_tag, "bytes_read pointer is NULL");

  uart_read_ctx_t ctx = {.data       = data,
                         .length     = length,
                         .bytes_read = 0,
                         .result     = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_read_callback, &ctx);

  *bytes_read = ctx.bytes_read;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_putc(rx_bus_manager_t* manager, const char* bus_name, char c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  uart_putc_ctx_t ctx = {.c = c, .result = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_putc_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_puts(rx_bus_manager_t* manager, const char* bus_name, const char* str)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(str, s_tag, "str pointer is NULL");

  uart_puts_ctx_t ctx = {.str = str, .result = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_puts_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_getc(rx_bus_manager_t* manager, const char* bus_name, char* c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(c, s_tag, "c pointer is NULL");

  uart_getc_ctx_t ctx = {.c = '\0', .result = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_getc_callback, &ctx);

  *c = ctx.c;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_uart_rx_available(rx_bus_manager_t* manager, const char* bus_name, bool* available)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(available, s_tag, "available pointer is NULL");

  uart_rx_avail_ctx_t ctx = {.available = false, .result = k_rx_err_uart_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_uart_rx_avail_callback, &ctx);

  *available = ctx.available;

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
