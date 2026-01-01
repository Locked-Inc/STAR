/* src/rx_bus_gpio.c */

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
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
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

/* =============================================================================
 * Command Pattern Implementation (RECOMMENDED for new code)
 * =============================================================================
 */

/**
 * @brief Command data structure for GPIO write operation
 *
 * This demonstrates how to define command-specific data structures
 * for the command pattern. Each operation defines its own data type.
 */
typedef struct {
  bool value; /* GPIO value to write (true = high, false = low) */
} gpio_write_command_data_t;

/**
 * @brief Command execution function for GPIO write
 *
 * This function encapsulates the GPIO write operation logic.
 * It demonstrates the command pattern approach where operations
 * are self-contained and can be added without modifying the bus manager.
 *
 * @note This is example/demonstration code showing how to implement
 * the command pattern. It's intentionally not called directly but serves
 * as a template for actual implementations.
 *
 * @param[in,out] bus Bus configuration
 * @param[in] data Command data (gpio_write_command_data_t*)
 *
 * @return RX_OK on success, error code on failure
 */
__attribute__((unused)) static rx_err_t gpio_write_command_execute(rx_bus_config_t* bus, void* data)
{
  gpio_write_command_data_t* write_data = (gpio_write_command_data_t*)data;

  /* Validate bus type */
  if (bus->type != k_bus_type_gpio) {
    RX_LOG_ERROR(s_tag, "Bus is not GPIO type");
    return RX_ERR_INVALID_ARG;
  }

  /* Validate bus is initialized */
  if (!bus->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Execute GPIO write operation */
  rx_err_t err;
  if (write_data->value) {
    err = gpio_write_high(bus->proto.gpio.port, bus->proto.gpio.pin);
  } else {
    err = gpio_write_low(bus->proto.gpio.port, bus->proto.gpio.pin);
  }

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO write failed");
    return err;
  }

  return RX_OK;
}

/**
 * @brief Command data structure for GPIO read operation
 */
typedef struct {
  bool* value; /* Pointer to store read value */
} gpio_read_command_data_t;

/**
 * @brief Command execution function for GPIO read
 *
 * @note This is example/demonstration code showing how to implement
 * the command pattern. It's intentionally not called directly but serves
 * as a template for actual implementations.
 *
 * @param[in,out] bus Bus configuration
 * @param[in] data Command data (gpio_read_command_data_t*)
 *
 * @return RX_OK on success, error code on failure
 */
__attribute__((unused)) static rx_err_t gpio_read_command_execute(rx_bus_config_t* bus, void* data)
{
  gpio_read_command_data_t* read_data = (gpio_read_command_data_t*)data;

  /* Validate bus type */
  if (bus->type != k_bus_type_gpio) {
    RX_LOG_ERROR(s_tag, "Bus is not GPIO type");
    return RX_ERR_INVALID_ARG;
  }

  /* Validate bus is initialized */
  if (!bus->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Execute GPIO read operation */
  rx_err_t err = gpio_read(bus->proto.gpio.port, bus->proto.gpio.pin, read_data->value);

  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "GPIO read failed");
    return err;
  }

  return RX_OK;
}

/* =============================================================================
 * Command Pattern Example Usage
 * =============================================================================
 *
 * The functions above demonstrate how GPIO operations can be implemented
 * using the command pattern. Here's how you would use them:
 *
 * Example 1: GPIO Write using Command Pattern
 * @code
 * // Define command data
 * gpio_write_command_data_t write_data = { .value = true };
 *
 * // Create and initialize command
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_write_command_execute, &write_data);
 *
 * // Execute command
 * rx_err_t err = rx_bus_manager_execute_command(manager, "led", &cmd);
 * if (err == RX_OK && cmd.result == RX_OK) {
 *   // GPIO write successful
 * }
 * @endcode
 *
 * Example 2: GPIO Read using Command Pattern
 * @code
 * bool button_state;
 * gpio_read_command_data_t read_data = { .value = &button_state };
 *
 * rx_bus_command_t cmd;
 * rx_bus_command_init(&cmd, gpio_read_command_execute, &read_data);
 *
 * rx_err_t err = rx_bus_manager_execute_command(manager, "button", &cmd);
 * if (err == RX_OK && cmd.result == RX_OK) {
 *   // button_state now contains the GPIO value
 * }
 * @endcode
 *
 * Benefits of Command Pattern:
 * 1. Open/Closed Principle: New operations can be added without modifying
 *    the bus manager code
 * 2. Single Responsibility: Each command encapsulates one operation
 * 3. Testability: Commands can be unit tested independently
 * 4. Consistency: All operations follow the same interface pattern
 *
 * Migration Path:
 * - Existing code can continue using rx_bus_gpio_write(), rx_bus_gpio_read()
 * - New code should use the command pattern for better extensibility
 * - The legacy functions internally use the callback pattern which is also
 *   valid but requires more boilerplate for each new operation
 *
 * =============================================================================
 */
