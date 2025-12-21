/* src/rx_bus_gpio.c */

/**
 * @file rx_bus_gpio.c
 * @brief GPIO bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe GPIO operations through bus manager.
 * Wraps low-level GPIO HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_bus_gpio.h"

#include "hardware.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_GPIO";

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @brief Context for GPIO init operation
 */
typedef struct {
  bool     output;
  rx_err_t result;
} gpio_init_ctx_t;

/**
 * @brief Context for GPIO write operation
 */
typedef struct {
  bool     value;
  rx_err_t result;
} gpio_write_ctx_t;

/**
 * @brief Context for GPIO read operation
 */
typedef struct {
  bool*    value;
  rx_err_t result;
} gpio_read_ctx_t;

/**
 * @brief Context for GPIO toggle operation
 */
typedef struct {
  rx_err_t result;
} gpio_toggle_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Callback for GPIO initialization
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_init_ctx_t*)
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_gpio_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_init_ctx_t* ctx = (gpio_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_gpio) {
    RX_LOG_ERROR(s_tag, "Bus is not GPIO type");
    ctx->result = RX_ERR_INVALID_ARG;
    return RX_ERR_INVALID_ARG;
  }

  /* Initialize GPIO pin */
  rx_err_t err;
  if (ctx->output) {
    err = gpio_set_output(bus_config->proto.gpio.port, bus_config->proto.gpio.pin);
  } else {
    err = gpio_set_input(bus_config->proto.gpio.port, bus_config->proto.gpio.pin);
  }

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  ctx->result = RX_OK;
  return RX_OK;
}

/**
 * @brief Callback for GPIO write operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_write_ctx_t*)
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_gpio_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_write_ctx_t* ctx = (gpio_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = RX_ERR_INVALID_STATE;
    return RX_ERR_INVALID_STATE;
  }

  /* Write GPIO value */
  rx_err_t err;
  if (ctx->value) {
    err = gpio_write_high(bus_config->proto.gpio.port, bus_config->proto.gpio.pin);
  } else {
    err = gpio_write_low(bus_config->proto.gpio.port, bus_config->proto.gpio.pin);
  }

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO write failed");
    ctx->result = err;
    return err;
  }

  ctx->result = RX_OK;
  return RX_OK;
}

/**
 * @brief Callback for GPIO read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_read_ctx_t*)
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_gpio_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_read_ctx_t* ctx = (gpio_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = RX_ERR_INVALID_STATE;
    return RX_ERR_INVALID_STATE;
  }

  /* Read GPIO value */
  rx_err_t err = gpio_read(bus_config->proto.gpio.port, bus_config->proto.gpio.pin, ctx->value);

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = RX_OK;
  return RX_OK;
}

/**
 * @brief Callback for GPIO toggle operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_toggle_ctx_t*)
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_gpio_toggle_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_toggle_ctx_t* ctx = (gpio_toggle_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = RX_ERR_INVALID_STATE;
    return RX_ERR_INVALID_STATE;
  }

  /* Toggle GPIO */
  rx_err_t err = gpio_toggle(bus_config->proto.gpio.port, bus_config->proto.gpio.pin);

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO toggle failed");
    ctx->result = err;
    return err;
  }

  ctx->result = RX_OK;
  return RX_OK;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_gpio_init(rx_bus_manager_t* manager, const char* bus_name, bool output)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_init_ctx_t ctx = {.output = output, .result = RX_ERR_HW_ERROR};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_init_callback, &ctx);

  if (err != RX_OK) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_write(rx_bus_manager_t* manager, const char* bus_name, bool value)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_write_ctx_t ctx = {.value = value, .result = RX_ERR_HW_ERROR};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_write_callback, &ctx);

  if (err != RX_OK) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_read(rx_bus_manager_t* manager, const char* bus_name, bool* value)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(value, s_tag, "value pointer is NULL");

  gpio_read_ctx_t ctx = {.value = value, .result = RX_ERR_HW_ERROR};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_read_callback, &ctx);

  if (err != RX_OK) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_toggle(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_toggle_ctx_t ctx = {.result = RX_ERR_HW_ERROR};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_toggle_callback, &ctx);

  if (err != RX_OK) {
    return err;
  }

  return ctx.result;
}
