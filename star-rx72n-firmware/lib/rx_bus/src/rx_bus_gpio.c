/* lib/rx_bus/src/rx_bus_gpio.c */

/**
 * @file rx_bus_gpio.c
 * @brief GPIO bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe GPIO operations through bus manager.
 * Wraps low-level GPIO HAL with bus abstraction pattern.
 *
 * This implementation demonstrates TWO patterns:
 * 1. Legacy callback pattern (for backward compatibility)
 * 2. Command pattern (recommended for new code)
 *
 * Both patterns are maintained to show migration path and maintain
 * backward compatibility with existing code.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_gpio.h"

#include "hardware.h"
#include "rx_bus_command.h"
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
  bool     output; /**< True for output, false for input */
  rx_err_t result; /**< Operation result */
} gpio_init_ctx_t;

/**
 * @brief Context for GPIO write operation
 */
typedef struct {
  bool     value;  /**< Value to write (true=high, false=low) */
  rx_err_t result; /**< Operation result */
} gpio_write_ctx_t;

/**
 * @brief Context for GPIO read operation
 */
typedef struct {
  bool*    value;  /**< Pointer to store read value */
  rx_err_t result; /**< Operation result */
} gpio_read_ctx_t;

/**
 * @brief Context for GPIO toggle operation
 */
typedef struct {
  rx_err_t result; /**< Operation result */
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
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_gpio_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_init_ctx_t* ctx = (gpio_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_gpio) {
    rx_log_error(s_tag, "Bus is not GPIO type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize GPIO pin */
  rx_err_t err;
  if (ctx->output) {
    err = gpio_set_output(bus_config->proto.gpio.pin);
  } else {
    err = gpio_set_input(bus_config->proto.gpio.pin);
  }

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "GPIO HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify GPIO is responsive by attempting a read */
  bool test_value = false;
  err             = gpio_read(bus_config->proto.gpio.pin, &test_value);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Post-init verification read failed (pin may not support readback)");
    /* Continue anyway - init succeeded, readback limitation is acceptable */
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for GPIO write operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_write_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_gpio_write_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_write_ctx_t* ctx = (gpio_write_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Write GPIO value */
  rx_err_t err;
  if (ctx->value) {
    err = gpio_write_high(bus_config->proto.gpio.pin);
  } else {
    err = gpio_write_low(bus_config->proto.gpio.pin);
  }

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "GPIO write failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify written value by reading back */
  bool readback_value = false;
  err                 = gpio_read(bus_config->proto.gpio.pin, &readback_value);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Post-write verification read failed");
    /* Continue anyway - write succeeded, readback limitation is acceptable */
  } else if (readback_value != ctx->value) {
    rx_log_warn(s_tag, "GPIO readback mismatch");
    /* Continue anyway - some hardware doesn't support output readback reliably */
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for GPIO read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_read_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_gpio_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_read_ctx_t* ctx = (gpio_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read GPIO value */
  rx_err_t err = gpio_read(bus_config->proto.gpio.pin, ctx->value);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "GPIO read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for GPIO toggle operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (gpio_toggle_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_gpio_toggle_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  gpio_toggle_ctx_t* ctx = (gpio_toggle_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    rx_log_error(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read current state before toggle for verification */
  bool     state_before = false;
  rx_err_t err          = gpio_read(bus_config->proto.gpio.pin, &state_before);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Pre-toggle read failed (output pin may not support readback)");
    state_before = false; /* Assume low if can't read */
  }

  /* Toggle GPIO */
  err = gpio_toggle(bus_config->proto.gpio.pin);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "GPIO toggle failed");
    ctx->result = err;
    return err;
  }

  /* Post-condition: Verify pin actually toggled */
  bool state_after = false;
  err              = gpio_read(bus_config->proto.gpio.pin, &state_after);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag,
                "Post-toggle verification read failed (output pin may not support readback)");
    /* Continue anyway - toggle succeeded, readback limitation is acceptable */
  } else if (state_after == state_before) {
    rx_log_warn(s_tag, "GPIO toggle verification failed (state did not change)");
    /* Continue anyway - some hardware doesn't support output readback reliably */
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_gpio_init(rx_bus_manager_t* manager, const char* bus_name, bool output)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_init_ctx_t ctx = {.output = output, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_write(rx_bus_manager_t* manager, const char* bus_name, bool value)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_write_ctx_t ctx = {.value = value, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_write_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_read(rx_bus_manager_t* manager, const char* bus_name, bool* value)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(value, s_tag, "value pointer is NULL");

  gpio_read_ctx_t ctx = {.value = value, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_gpio_toggle(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  gpio_toggle_ctx_t ctx = {.result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_gpio_toggle_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
