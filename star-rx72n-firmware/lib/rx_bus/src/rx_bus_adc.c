/* lib/rx_bus/src/rx_bus_adc.c */

/**
 * @file rx_bus_adc.c
 * @brief ADC bus abstraction implementation for RX72N
 * @details
 * Provides thread-safe ADC operations through bus manager.
 * Wraps low-level ADC HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_bus_adc.h"

#include "hardware.h"
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_ADC";

/* =============================================================================
 * Callback Context Structures
 * =============================================================================
 */

/**
 * @brief Context for ADC init operation
 */
typedef struct {
  rx_err_t result; /**< Operation result */
} adc_init_ctx_t;

/**
 * @brief Context for ADC read operation
 */
typedef struct {
  uint16_t* value;  /**< Pointer to store ADC value */
  rx_err_t  result; /**< Operation result */
} adc_read_ctx_t;

/**
 * @brief Context for ADC voltage read operation
 */
typedef struct {
  uint32_t* voltage_mv; /**< Pointer to store voltage in millivolts */
  uint8_t   bits;       /**< ADC resolution in bits */
  rx_err_t  result;     /**< Operation result */
} adc_voltage_ctx_t;

/* =============================================================================
 * Private Callback Functions
 * =============================================================================
 */

/**
 * @brief Callback for ADC initialization
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (adc_init_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_adc_init_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  adc_init_ctx_t* ctx = (adc_init_ctx_t*)user_ctx;

  /* Validate bus type */
  if (bus_config->type != k_bus_type_adc) {
    RX_LOG_ERROR(s_tag, "Bus is not ADC type");
    ctx->result = k_rx_err_invalid_arg;
    return k_rx_err_invalid_arg;
  }

  /* Initialize ADC channel */
  rx_err_t err =
    adc_init(bus_config->proto.adc.unit, bus_config->proto.adc.channel, bus_config->proto.adc.bits);

  if (err != k_rx_ok) {
    RX_LOG_ERROR(s_tag, "ADC HAL initialization failed");
    ctx->result = err;
    return err;
  }

  /* Mark ADC unit as initialized in bus manager */
  if (bus_config->proto.adc.unit < k_adc_unit_count) {
    /* Track initialization in manager (bus_config has parent pointer) */
    /* Note: This is handled by the ADC HAL internally */
  }

  /* Mark bus as initialized */
  bus_config->initialized = true;

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for ADC read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (adc_read_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_adc_read_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  adc_read_ctx_t* ctx = (adc_read_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read ADC value */
  rx_err_t err = adc_read(bus_config->proto.adc.unit, bus_config->proto.adc.channel, ctx->value);

  if (err != k_rx_ok) {
    RX_LOG_ERROR(s_tag, "ADC read failed");
    ctx->result = err;
    return err;
  }

  ctx->result = k_rx_ok;
  return k_rx_ok;
}

/**
 * @brief Callback for ADC voltage read operation
 *
 * @param[in] bus_config Bus configuration
 * @param[in] user_ctx User context (adc_voltage_ctx_t*)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_adc_voltage_callback(rx_bus_config_t* bus_config, void* user_ctx)
{
  adc_voltage_ctx_t* ctx = (adc_voltage_ctx_t*)user_ctx;

  /* Validate bus is initialized */
  if (!bus_config->initialized) {
    RX_LOG_ERROR(s_tag, "Bus not initialized");
    ctx->result = k_rx_err_invalid_state;
    return k_rx_err_invalid_state;
  }

  /* Read ADC voltage */
  rx_err_t err = adc_read_voltage_mv(bus_config->proto.adc.unit,
                                     bus_config->proto.adc.channel,
                                     bus_config->proto.adc.bits,
                                     ctx->voltage_mv);

  if (err != k_rx_ok) {
    RX_LOG_ERROR(s_tag, "ADC voltage read failed");
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

rx_err_t rx_bus_adc_init(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  adc_init_ctx_t ctx = {.result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_adc_init_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t rx_bus_adc_read(rx_bus_manager_t* manager, const char* bus_name, uint16_t* value)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(value, s_tag, "value pointer is NULL");

  adc_read_ctx_t ctx = {.value = value, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_adc_read_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}

rx_err_t
rx_bus_adc_read_voltage_mv(rx_bus_manager_t* manager, const char* bus_name, uint32_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "voltage_mv pointer is NULL");

  adc_voltage_ctx_t ctx = {.voltage_mv = voltage_mv, .result = k_rx_err_hw_error};

  rx_err_t err = rx_bus_manager_with_bus(manager, bus_name, internal_adc_voltage_callback, &ctx);

  if (err != k_rx_ok) {
    return err;
  }

  return ctx.result;
}
