/**
 * @file star_bus_spi_peripheral.c
 * @brief SPI peripheral mode implementation for the bus manager
 * @details
 * Implements SPI peripheral mode operations for ESP32. Allows ESP32 to act
 * as an SPI peripheral device controlled by an external SPI controller (e.g., Raspberry Pi 5).
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_bus_spi_peripheral.h"

#include "freertos/FreeRTOS.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager.h"
#include "star_bus_types.h"

/* --- Constants --- */

static const char* s_tag = "STAR_SPI_PERIPH";

/* --- Private Function Prototypes (Default Ops) --- */

static esp_err_t internal_star_bus_spi_peripheral_receive(const star_bus_config_t* config,
                                                          void*                    rx_buffer,
                                                          size_t                   len,
                                                          uint32_t                 timeout_ms);

static esp_err_t internal_star_bus_spi_peripheral_transmit(const star_bus_config_t* config,
                                                           const void*              tx_buffer,
                                                           size_t                   len,
                                                           uint32_t                 timeout_ms);

static esp_err_t internal_star_bus_spi_peripheral_transceive(const star_bus_config_t* config,
                                                             const void*              tx_buffer,
                                                             void*                    rx_buffer,
                                                             size_t                   len,
                                                             uint32_t                 timeout_ms);

/* --- Public Functions --- */

void star_bus_spi_peripheral_init_default_ops(star_spi_ops_t* ops)
{
  if (ops == NULL) {
    ESP_LOGE(s_tag, "Cannot initialize NULL ops pointer");
    return;
  }

  /* Note: SPI peripheral uses different operations than controller mode
   * We reuse the same function pointers but with different implementations */
  ops->transmit = (star_spi_transmit_fn_t)internal_star_bus_spi_peripheral_transmit;
  ops->receive  = (star_spi_receive_fn_t)internal_star_bus_spi_peripheral_receive;
  ops->transfer = (star_spi_transfer_fn_t)internal_star_bus_spi_peripheral_transceive;

  ESP_LOGD(s_tag, "Default SPI peripheral operations initialized");
}

esp_err_t star_bus_spi_peripheral_receive(const star_bus_manager_t* manager,
                                          const char*               name,
                                          void*                     rx_buffer,
                                          size_t                    len,
                                          uint32_t                  timeout_ms)
{
  ESP_RETURN_ON_FALSE(manager && name && rx_buffer,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager, name, or rx_buffer is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_tag, "Receive length must be > 0");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_spi,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not an SPI bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.spi.ops.receive,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No receive op for '%s'",
                      name);

  return internal_star_bus_spi_peripheral_receive(bus_config, rx_buffer, len, timeout_ms);
}

esp_err_t star_bus_spi_peripheral_transmit(const star_bus_manager_t* manager,
                                           const char*               name,
                                           const void*               tx_buffer,
                                           size_t                    len,
                                           uint32_t                  timeout_ms)
{
  ESP_RETURN_ON_FALSE(manager && name && tx_buffer,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager, name, or tx_buffer is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_tag, "Transmit length must be > 0");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_spi,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not an SPI bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.spi.ops.transmit,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No transmit op for '%s'",
                      name);

  return internal_star_bus_spi_peripheral_transmit(bus_config, tx_buffer, len, timeout_ms);
}

esp_err_t star_bus_spi_peripheral_transceive(const star_bus_manager_t* manager,
                                             const char*               name,
                                             const void*               tx_buffer,
                                             void*                     rx_buffer,
                                             size_t                    len,
                                             uint32_t                  timeout_ms)
{
  ESP_RETURN_ON_FALSE(manager && name, ESP_ERR_INVALID_ARG, s_tag, "Manager or name is NULL");
  ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, s_tag, "Transfer length must be > 0");
  ESP_RETURN_ON_FALSE(tx_buffer || rx_buffer,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Both tx_buffer and rx_buffer are NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_spi,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not an SPI bus",
                      name);
  ESP_RETURN_ON_FALSE(bus_config->proto.spi.ops.transfer,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No transfer op for '%s'",
                      name);

  return internal_star_bus_spi_peripheral_transceive(bus_config,
                                                     tx_buffer,
                                                     rx_buffer,
                                                     len,
                                                     timeout_ms);
}

/* --- Private Functions (Default Ops Implementations) --- */

static esp_err_t internal_star_bus_spi_peripheral_receive(const star_bus_config_t* config,
                                                          void*                    rx_buffer,
                                                          size_t                   len,
                                                          uint32_t                 timeout_ms)
{
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "Bus '%s' is not initialized",
                      config->name);

  /* Prepare transaction structure */
  spi_slave_transaction_t trans = {
    .length    = len * 8, /* Length in bits */
    .rx_buffer = rx_buffer,
    .tx_buffer = NULL, /* Receive only */
  };

  TickType_t timeout_ticks =
    (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  /* Queue and wait for transaction */
  esp_err_t ret = spi_slave_queue_trans(config->proto.spi.host, &trans, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to queue receive transaction: %s",
                      config->name,
                      esp_err_to_name(ret));

  /* Wait for transaction to complete */
  spi_slave_transaction_t* trans_out;
  ret = spi_slave_get_trans_result(config->proto.spi.host, &trans_out, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to get receive result: %s",
                      config->name,
                      esp_err_to_name(ret));

  ESP_LOGD(s_tag,
           "Peripheral '%s': Received %zu bytes (actual: %" PRIu32 " bits)",
           config->name,
           len,
           trans_out->trans_len);

  /* Call success callback */
  if (config->proto.spi.callbacks.on_transfer_complete) {
    star_bus_event_t event = {.bus_type = k_star_bus_type_spi,
                              .bus_name = config->name,
                              .spi      = {.host     = config->proto.spi.host,
                                           .cs_pin   = config->proto.spi.cs_io_num,
                                           .is_write = false,
                                           .tx_len   = 0,
                                           .rx_len   = len}};
    config->proto.spi.callbacks.on_transfer_complete(&event, config->user_ctx);
  }

  return ESP_OK;
}

static esp_err_t internal_star_bus_spi_peripheral_transmit(const star_bus_config_t* config,
                                                           const void*              tx_buffer,
                                                           size_t                   len,
                                                           uint32_t                 timeout_ms)
{
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "Bus '%s' is not initialized",
                      config->name);

  /* Prepare transaction structure */
  spi_slave_transaction_t trans = {
    .length    = len * 8, /* Length in bits */
    .rx_buffer = NULL,    /* Transmit only */
    .tx_buffer = tx_buffer,
  };

  TickType_t timeout_ticks =
    (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  /* Queue and wait for transaction */
  esp_err_t ret = spi_slave_queue_trans(config->proto.spi.host, &trans, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to queue transmit transaction: %s",
                      config->name,
                      esp_err_to_name(ret));

  /* Wait for transaction to complete */
  spi_slave_transaction_t* trans_out;
  ret = spi_slave_get_trans_result(config->proto.spi.host, &trans_out, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to get transmit result: %s",
                      config->name,
                      esp_err_to_name(ret));

  ESP_LOGD(s_tag,
           "Peripheral '%s': Transmitted %zu bytes (actual: %" PRIu32 " bits)",
           config->name,
           len,
           trans_out->trans_len);

  /* Call success callback */
  if (config->proto.spi.callbacks.on_transfer_complete) {
    star_bus_event_t event = {.bus_type = k_star_bus_type_spi,
                              .bus_name = config->name,
                              .spi      = {.host     = config->proto.spi.host,
                                           .cs_pin   = config->proto.spi.cs_io_num,
                                           .is_write = true,
                                           .tx_len   = len,
                                           .rx_len   = 0}};
    config->proto.spi.callbacks.on_transfer_complete(&event, config->user_ctx);
  }

  return ESP_OK;
}

static esp_err_t internal_star_bus_spi_peripheral_transceive(const star_bus_config_t* config,
                                                             const void*              tx_buffer,
                                                             void*                    rx_buffer,
                                                             size_t                   len,
                                                             uint32_t                 timeout_ms)
{
  ESP_RETURN_ON_FALSE(config->initialized,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "Bus '%s' is not initialized",
                      config->name);

  /* Prepare transaction structure */
  spi_slave_transaction_t trans = {
    .length    = len * 8, /* Length in bits */
    .rx_buffer = rx_buffer,
    .tx_buffer = tx_buffer,
  };

  TickType_t timeout_ticks =
    (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  /* Queue and wait for transaction */
  esp_err_t ret = spi_slave_queue_trans(config->proto.spi.host, &trans, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to queue transceive transaction: %s",
                      config->name,
                      esp_err_to_name(ret));

  /* Wait for transaction to complete */
  spi_slave_transaction_t* trans_out;
  ret = spi_slave_get_trans_result(config->proto.spi.host, &trans_out, timeout_ticks);
  ESP_RETURN_ON_ERROR(ret,
                      s_tag,
                      "Peripheral '%s': Failed to get transceive result: %s",
                      config->name,
                      esp_err_to_name(ret));

  ESP_LOGD(s_tag,
           "Peripheral '%s': Transceived %zu bytes (actual: %" PRIu32 " bits)",
           config->name,
           len,
           trans_out->trans_len);

  /* Call success callback */
  if (config->proto.spi.callbacks.on_transfer_complete) {
    star_bus_event_t event = {.bus_type = k_star_bus_type_spi,
                              .bus_name = config->name,
                              .spi      = {.host     = config->proto.spi.host,
                                           .cs_pin   = config->proto.spi.cs_io_num,
                                           .is_write = false,
                                           .tx_len   = len,
                                           .rx_len   = len}};
    config->proto.spi.callbacks.on_transfer_complete(&event, config->user_ctx);
  }

  return ESP_OK;
}
