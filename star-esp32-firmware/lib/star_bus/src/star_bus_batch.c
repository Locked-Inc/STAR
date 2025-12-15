/* lib/star_bus/src/star_bus_batch.c */

#include "star_bus_batch.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <string.h>

#include "star_bus_i2c.h"
#include "star_bus_smbus.h"
#include "star_bus_spi.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_BATCH";

/* --- Types --- */

/**
 * @brief Batch transaction context
 */
typedef struct {
  star_bus_manager_t*    manager;                               /**< Bus manager reference */
  star_batch_config_t    config;                                /**< Batch configuration */
  star_batch_operation_t operations[STAR_BATCH_MAX_OPERATIONS]; /**< Operations array */
  uint32_t               operation_count;                       /**< Number of operations */
  bool                   executed;   /**< Whether batch has been executed */
  TickType_t             start_tick; /**< Execution start time */
  TickType_t             end_tick;   /**< Execution end time */
} batch_context_t;

/* --- Helper Functions --- */

/**
 * @brief Execute a single batch operation
 */
static esp_err_t internal_execute_operation(batch_context_t* ctx, uint32_t index)
{
  if (ctx == NULL || index >= ctx->operation_count) {
    return ESP_ERR_INVALID_ARG;
  }

  star_batch_operation_t* op     = &ctx->operations[index];
  esp_err_t               result = ESP_FAIL;

  switch (op->type) {
    case k_star_batch_op_i2c_write:
      result = star_bus_i2c_write(ctx->manager,
                                  op->bus_name,
                                  op->params.i2c.data,
                                  op->params.i2c.length,
                                  op->params.i2c.command,
                                  NULL);
      break;

    case k_star_batch_op_i2c_read:
      result = star_bus_i2c_read(ctx->manager,
                                 op->bus_name,
                                 op->params.i2c.data,
                                 op->params.i2c.length,
                                 op->params.i2c.command,
                                 NULL);
      break;

    case k_star_batch_op_spi_transmit:
      result = star_bus_spi_transmit(ctx->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.length,
                                     op->params.spi.flags);
      break;

    case k_star_batch_op_spi_receive:
      result = star_bus_spi_receive(ctx->manager,
                                    op->bus_name,
                                    op->params.spi.rx_data,
                                    op->params.spi.length,
                                    op->params.spi.flags);
      break;

    case k_star_batch_op_spi_transfer:
      result = star_bus_spi_transfer(ctx->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.rx_data,
                                     op->params.spi.length,
                                     op->params.spi.flags);
      break;

    case k_star_batch_op_smbus_read_byte:
      result = star_smbus_read_byte(ctx->manager,
                                    op->bus_name,
                                    op->params.smbus.addr,
                                    op->params.smbus.command,
                                    op->params.smbus.data);
      break;

    case k_star_batch_op_smbus_write_byte:
      result = star_smbus_write_byte(ctx->manager,
                                     op->bus_name,
                                     op->params.smbus.addr,
                                     op->params.smbus.command,
                                     *op->params.smbus.data);
      break;

    case k_star_batch_op_delay:
      vTaskDelay(pdMS_TO_TICKS(op->params.delay.delay_ms));
      result = ESP_OK;
      break;

    default:
      ESP_LOGE(s_TAG, "Unknown operation type: %d", op->type);
      result = ESP_ERR_NOT_SUPPORTED;
      break;
  }

  op->result = result;
  return result;
}

/* --- Public Functions --- */

esp_err_t star_batch_create(star_bus_manager_t*        manager,
                            const star_batch_config_t* config,
                            star_batch_handle_t*       handle)
{
  if (manager == NULL || config == NULL || handle == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)malloc(sizeof(batch_context_t));
  if (ctx == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate batch context");
    return ESP_ERR_NO_MEM;
  }

  memset(ctx, 0, sizeof(batch_context_t));
  ctx->manager = manager;
  memcpy(&ctx->config, config, sizeof(star_batch_config_t));

  *handle = (star_batch_handle_t)ctx;

  ESP_LOGD(s_TAG,
           "Batch created: mode=%d, timeout=%" PRIu32 " ms",
           config->mode,
           config->timeout_ms);

  return ESP_OK;
}

esp_err_t star_batch_free(star_batch_handle_t handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  /* Free any allocated SMBus write data */
  for (uint32_t i = 0; i < ctx->operation_count; i++) {
    star_batch_operation_t* op = &ctx->operations[i];
    if (op->type == k_star_batch_op_smbus_write_byte && op->params.smbus.data != NULL) {
      free(op->params.smbus.data);
      op->params.smbus.data = NULL;
    }
  }

  free(ctx);

  return ESP_OK;
}

esp_err_t star_batch_add_i2c_write(star_batch_handle_t handle,
                                   const char*         bus_name,
                                   const uint8_t*      data,
                                   size_t              length,
                                   uint8_t             command)
{
  if (handle == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type               = k_star_batch_op_i2c_write;
  op->bus_name           = bus_name;
  op->params.i2c.data    = (uint8_t*)data;
  op->params.i2c.length  = length;
  op->params.i2c.command = command;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_i2c_read(star_batch_handle_t handle,
                                  const char*         bus_name,
                                  uint8_t*            data,
                                  size_t              length,
                                  uint8_t             command)
{
  if (handle == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type               = k_star_batch_op_i2c_read;
  op->bus_name           = bus_name;
  op->params.i2c.data    = data;
  op->params.i2c.length  = length;
  op->params.i2c.command = command;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_spi_transmit(star_batch_handle_t handle,
                                      const char*         bus_name,
                                      const uint8_t*      data,
                                      size_t              length,
                                      uint32_t            flags)
{
  if (handle == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type               = k_star_batch_op_spi_transmit;
  op->bus_name           = bus_name;
  op->params.spi.tx_data = data;
  op->params.spi.length  = length;
  op->params.spi.flags   = flags;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_spi_receive(star_batch_handle_t handle,
                                     const char*         bus_name,
                                     uint8_t*            data,
                                     size_t              length,
                                     uint32_t            flags)
{
  if (handle == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type               = k_star_batch_op_spi_receive;
  op->bus_name           = bus_name;
  op->params.spi.rx_data = data;
  op->params.spi.length  = length;
  op->params.spi.flags   = flags;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_spi_transfer(star_batch_handle_t handle,
                                      const char*         bus_name,
                                      const uint8_t*      tx_data,
                                      uint8_t*            rx_data,
                                      size_t              length,
                                      uint32_t            flags)
{
  if (handle == NULL || bus_name == NULL || tx_data == NULL || rx_data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type               = k_star_batch_op_spi_transfer;
  op->bus_name           = bus_name;
  op->params.spi.tx_data = tx_data;
  op->params.spi.rx_data = rx_data;
  op->params.spi.length  = length;
  op->params.spi.flags   = flags;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_smbus_read_byte(star_batch_handle_t handle,
                                         const char*         bus_name,
                                         uint8_t             addr,
                                         uint8_t             command,
                                         uint8_t*            data)
{
  if (handle == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type                 = k_star_batch_op_smbus_read_byte;
  op->bus_name             = bus_name;
  op->params.smbus.addr    = addr;
  op->params.smbus.command = command;
  op->params.smbus.data    = data;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_smbus_write_byte(star_batch_handle_t handle,
                                          const char*         bus_name,
                                          uint8_t             addr,
                                          uint8_t             command,
                                          uint8_t             data)
{
  if (handle == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type                 = k_star_batch_op_smbus_write_byte;
  op->bus_name             = bus_name;
  op->params.smbus.addr    = addr;
  op->params.smbus.command = command;

  /* Allocate persistent storage for the data byte */
  op->params.smbus.data = (uint8_t*)malloc(sizeof(uint8_t));
  if (op->params.smbus.data == NULL) {
    return ESP_ERR_NO_MEM;
  }
  *op->params.smbus.data = data;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_add_delay(star_batch_handle_t handle, uint32_t delay_ms)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count >= STAR_BATCH_MAX_OPERATIONS) {
    ESP_LOGE(s_TAG, "Batch full (max %d operations)", STAR_BATCH_MAX_OPERATIONS);
    return ESP_ERR_NO_MEM;
  }

  star_batch_operation_t* op = &ctx->operations[ctx->operation_count];
  memset(op, 0, sizeof(star_batch_operation_t));

  op->type                  = k_star_batch_op_delay;
  op->params.delay.delay_ms = delay_ms;

  ctx->operation_count++;

  return ESP_OK;
}

esp_err_t star_batch_execute(star_batch_handle_t handle, star_batch_stats_t* stats)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (ctx->operation_count == 0) {
    ESP_LOGW(s_TAG, "Batch is empty");
    return ESP_ERR_INVALID_STATE;
  }

  if (ctx->executed) {
    ESP_LOGW(s_TAG, "Batch already executed");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(s_TAG, "Executing batch with %" PRIu32 " operations", ctx->operation_count);

  ctx->start_tick       = xTaskGetTickCount();
  esp_err_t first_error = ESP_OK;
  uint32_t  succeeded   = 0;
  uint32_t  failed      = 0;

  /* Execute operations sequentially */
  for (uint32_t i = 0; i < ctx->operation_count; i++) {
    esp_err_t result = internal_execute_operation(ctx, i);

    if (result == ESP_OK) {
      succeeded++;
    } else {
      failed++;
      ESP_LOGE(s_TAG, "Operation %" PRIu32 " failed: %s", i, esp_err_to_name(result));

      if (first_error == ESP_OK) {
        first_error = result;
      }

      if (ctx->config.stop_on_error) {
        ESP_LOGW(s_TAG, "Stopping batch execution on error");
        break;
      }
    }

    /* Check timeout */
    if (ctx->config.timeout_ms > 0) {
      TickType_t elapsed = xTaskGetTickCount() - ctx->start_tick;
      if (pdTICKS_TO_MS(elapsed) >= ctx->config.timeout_ms) {
        ESP_LOGE(s_TAG, "Batch execution timeout");
        first_error = ESP_ERR_TIMEOUT;
        break;
      }
    }
  }

  ctx->end_tick = xTaskGetTickCount();
  ctx->executed = true;

  /* Fill in statistics if requested */
  if (stats != NULL) {
    stats->operations_executed  = succeeded + failed;
    stats->operations_succeeded = succeeded;
    stats->operations_failed    = failed;
    stats->total_time_ms        = pdTICKS_TO_MS(ctx->end_tick - ctx->start_tick);
  }

  ESP_LOGI(s_TAG,
           "Batch execution complete: %" PRIu32 " succeeded, %" PRIu32 " failed",
           succeeded,
           failed);

  return first_error;
}

esp_err_t
star_batch_get_operation_result(star_batch_handle_t handle, uint32_t index, esp_err_t* result)
{
  if (handle == NULL || result == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  if (!ctx->executed) {
    return ESP_ERR_INVALID_STATE;
  }

  if (index >= ctx->operation_count) {
    return ESP_ERR_INVALID_ARG;
  }

  *result = ctx->operations[index].result;
  return ESP_OK;
}

esp_err_t star_batch_get_operation_count(star_batch_handle_t handle, uint32_t* count)
{
  if (handle == NULL || count == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;
  *count               = ctx->operation_count;

  return ESP_OK;
}

esp_err_t star_batch_clear(star_batch_handle_t handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  batch_context_t* ctx = (batch_context_t*)handle;

  /* Free any allocated SMBus write data */
  for (uint32_t i = 0; i < ctx->operation_count; i++) {
    if (ctx->operations[i].type == k_star_batch_op_smbus_write_byte &&
        ctx->operations[i].params.smbus.data != NULL) {
      free(ctx->operations[i].params.smbus.data);
    }
  }

  ctx->operation_count = 0;
  ctx->executed        = false;

  return ESP_OK;
}
