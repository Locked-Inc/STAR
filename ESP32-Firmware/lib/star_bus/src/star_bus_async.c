/* esp32-firmware/components/star_bus/star_bus_async.c */

#include "star_bus_async.h"

#include <esp_log.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_i2c.h"
#include "star_bus_smbus.h"
#include "star_bus_spi.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_ASYNC";

/* --- Types --- */

/**
 * @brief Async operation context
 *
 * @note The timeout_ms field is stored but not currently enforced by the worker.
 *       Operations will run to completion regardless of timeout value.
 * @note The priority field is stored but FIFO ordering is used. Priority queue
 *       ordering is not currently implemented.
 */
typedef struct async_operation {
  star_async_op_type_t  type;           /**< Operation type */
  star_async_status_t   status;         /**< Current status */
  esp_err_t             result;         /**< Operation result */
  star_async_callback_t callback;       /**< Completion callback */
  void*                 user_context;   /**< User context */
  uint32_t              timeout_ms;     /**< Timeout (stored but not enforced) */
  uint8_t               priority;       /**< Priority (stored, FIFO used) */
  TickType_t            start_tick;     /**< Start time */
  EventGroupHandle_t    event_group;    /**< Optional event group */
  EventBits_t           complete_bit;   /**< Event bit for completion */
  EventBits_t           error_bit;      /**< Event bit for error */
  SemaphoreHandle_t     wait_semaphore; /**< Semaphore for wait function */

  /* Operation-specific data */
  star_bus_manager_t* manager;
  char                bus_name[32];

  union {
    struct {
      uint8_t* data;
      size_t   length;
      uint8_t  command;
    } i2c;

    struct {
      uint8_t* tx_data;
      uint8_t* rx_data;
      size_t   length;
    } spi;

    struct {
      uint8_t  addr;
      uint8_t  command;
      uint8_t* data;
    } smbus;
  } params;

  struct async_operation* next; /**< Next operation in queue */
} async_operation_t;

/**
 * @brief Async state for a bus
 */
typedef struct {
  char               bus_name[32];   /**< Bus name for identification */
  async_operation_t* pending_head;   /**< Head of pending operations queue */
  async_operation_t* pending_tail;   /**< Tail of pending operations queue */
  uint32_t           pending_count;  /**< Number of pending operations */
  uint64_t           completed_ops;  /**< Total completed operations */
  uint64_t           failed_ops;     /**< Total failed operations */
  uint64_t           cancelled_ops;  /**< Total cancelled operations */
  SemaphoreHandle_t  queue_mutex;    /**< Mutex for queue operations */
  TaskHandle_t       worker_task;    /**< Worker task handle */
  bool               worker_running; /**< Worker task running flag */
} async_state_t;

/* --- Static Storage --- */

#define MAX_BUSES (8)
static async_state_t     g_async_states[MAX_BUSES] = {0};
static uint8_t           g_num_async_states        = 0;
static SemaphoreHandle_t g_global_mutex            = NULL;
static portMUX_TYPE      g_init_spinlock           = portMUX_INITIALIZER_UNLOCKED;

/* --- Helper Functions --- */

/**
 * @brief Initialize global async state (thread-safe)
 */
static void priv_init_global_state(void)
{
  /* Quick check without lock for common case */
  if (g_global_mutex != NULL) {
    return;
  }

  /* Use spinlock for thread-safe lazy initialization */
  portENTER_CRITICAL(&g_init_spinlock);

  /* Double-check after acquiring spinlock */
  if (g_global_mutex == NULL) {
    g_global_mutex = xSemaphoreCreateMutex();
  }

  portEXIT_CRITICAL(&g_init_spinlock);
}

/**
 * @brief Get or create async state for a bus
 */
static async_state_t* priv_get_async_state(const char* bus_name)
{
  priv_init_global_state();

  if (g_global_mutex == NULL) {
    ESP_LOGE(s_TAG, "Global mutex not initialized");
    return NULL;
  }

  xSemaphoreTake(g_global_mutex, portMAX_DELAY);

  /* Search for existing state by bus name */
  for (uint8_t i = 0; i < g_num_async_states; i++) {
    if (strcmp(g_async_states[i].bus_name, bus_name) == 0) {
      xSemaphoreGive(g_global_mutex);
      return &g_async_states[i];
    }
  }

  /* Create new state if needed */
  if (g_num_async_states < MAX_BUSES) {
    async_state_t* state = &g_async_states[g_num_async_states];
    memset(state, 0, sizeof(async_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';
    state->queue_mutex                           = xSemaphoreCreateMutex();
    if (state->queue_mutex == NULL) {
      ESP_LOGE(s_TAG, "Failed to create queue mutex for bus '%s'", bus_name);
      xSemaphoreGive(g_global_mutex);
      return NULL;
    }
    g_num_async_states++;
    xSemaphoreGive(g_global_mutex);
    return state;
  }

  ESP_LOGE(s_TAG, "Maximum async states reached (%d)", MAX_BUSES);
  xSemaphoreGive(g_global_mutex);
  return NULL;
}

/**
 * @brief Enqueue an async operation
 */
static esp_err_t priv_enqueue_operation(async_state_t* state, async_operation_t* op)
{
  if (state == NULL || op == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  xSemaphoreTake(state->queue_mutex, portMAX_DELAY);

  if (state->pending_count >= (uint32_t)STAR_ASYNC_MAX_PENDING) {
    xSemaphoreGive(state->queue_mutex);
    ESP_LOGE(s_TAG, "Async queue full");
    return ESP_ERR_NO_MEM;
  }

  /* Add to tail of queue */
  op->next = NULL;
  if (state->pending_tail != NULL) {
    state->pending_tail->next = op;
  }
  state->pending_tail = op;

  if (state->pending_head == NULL) {
    state->pending_head = op;
  }

  state->pending_count++;

  xSemaphoreGive(state->queue_mutex);

  return ESP_OK;
}

/**
 * @brief Execute an async operation
 */
static void priv_execute_operation(async_operation_t* op)
{
  if (op == NULL) {
    return;
  }

  op->status       = STAR_ASYNC_STATUS_RUNNING;
  esp_err_t result = ESP_FAIL;

  switch (op->type) {
    case STAR_ASYNC_OP_I2C_WRITE:
      result = star_bus_i2c_write(op->manager,
                                  op->bus_name,
                                  op->params.i2c.data,
                                  op->params.i2c.length,
                                  op->params.i2c.command,
                                  NULL);
      break;

    case STAR_ASYNC_OP_I2C_READ:
      result = star_bus_i2c_read(op->manager,
                                 op->bus_name,
                                 op->params.i2c.data,
                                 op->params.i2c.length,
                                 op->params.i2c.command,
                                 NULL);
      break;

    case STAR_ASYNC_OP_SPI_TRANSMIT:
      result = star_bus_spi_transmit(op->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.length,
                                     0);
      break;

    case STAR_ASYNC_OP_SPI_RECEIVE:
      result = star_bus_spi_receive(op->manager,
                                    op->bus_name,
                                    op->params.spi.rx_data,
                                    op->params.spi.length,
                                    0);
      break;

    case STAR_ASYNC_OP_SPI_TRANSCEIVE:
      result = star_bus_spi_transfer(op->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.rx_data,
                                     op->params.spi.length,
                                     0);
      break;

    case STAR_ASYNC_OP_SMBUS_READ:
      result = star_smbus_read_byte(op->manager,
                                    op->bus_name,
                                    op->params.smbus.addr,
                                    op->params.smbus.command,
                                    op->params.smbus.data);
      break;

    case STAR_ASYNC_OP_SMBUS_WRITE:
      result = star_smbus_write_byte(op->manager,
                                     op->bus_name,
                                     op->params.smbus.addr,
                                     op->params.smbus.command,
                                     *op->params.smbus.data);
      break;

    default:
      ESP_LOGE(s_TAG, "Unknown operation type: %d", op->type);
      result = ESP_ERR_NOT_SUPPORTED;
      break;
  }

  op->result = result;
  op->status = (result == ESP_OK) ? STAR_ASYNC_STATUS_COMPLETE : STAR_ASYNC_STATUS_ERROR;

  /* Set event bits if configured */
  if (op->event_group != NULL) {
    if (result == ESP_OK && op->complete_bit != 0) {
      xEventGroupSetBits(op->event_group, op->complete_bit);
    } else if (result != ESP_OK && op->error_bit != 0) {
      xEventGroupSetBits(op->event_group, op->error_bit);
    }
  }

  /* Signal wait semaphore if waiting */
  if (op->wait_semaphore != NULL) {
    xSemaphoreGive(op->wait_semaphore);
  }

  /* Invoke callback */
  if (op->callback != NULL) {
    op->callback((star_async_handle_t)op, op->status, result, op->user_context);
  }
}

/**
 * @brief Worker task for processing async operations
 */
static void priv_async_worker_task(void* param)
{
  async_state_t* state = (async_state_t*)param;

  ESP_LOGI(s_TAG, "Async worker task started");

  while (state->worker_running) {
    async_operation_t* op = NULL;

    /* Dequeue next operation */
    xSemaphoreTake(state->queue_mutex, portMAX_DELAY);

    if (state->pending_head != NULL) {
      op                  = state->pending_head;
      state->pending_head = op->next;

      if (state->pending_head == NULL) {
        state->pending_tail = NULL;
      }

      state->pending_count--;
    }

    xSemaphoreGive(state->queue_mutex);

    if (op != NULL) {
      /* Check if operation was cancelled while in queue */
      if (op->status == STAR_ASYNC_STATUS_CANCELLED) {
        state->cancelled_ops++;
        /* Don't execute, callback was already invoked during cancel */
      } else {
        /* Execute the operation */
        priv_execute_operation(op);

        /* Update statistics */
        if (op->status == STAR_ASYNC_STATUS_COMPLETE) {
          state->completed_ops++;
        } else if (op->status == STAR_ASYNC_STATUS_ERROR) {
          state->failed_ops++;
        } else if (op->status == STAR_ASYNC_STATUS_CANCELLED) {
          state->cancelled_ops++;
        }
      }
    } else {
      /* No operations pending, sleep briefly */
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  vTaskDelete(NULL);
}

/**
 * @brief Ensure worker task is running (thread-safe)
 */
static esp_err_t priv_ensure_worker_running(async_state_t* state)
{
  if (state == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Quick check without lock for common case */
  if (state->worker_running) {
    return ESP_OK;
  }

  /* Use queue_mutex to prevent race condition */
  if (xSemaphoreTake(state->queue_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take queue mutex for worker check");
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t result = ESP_OK;

  /* Double-check after acquiring mutex */
  if (!state->worker_running) {
    state->worker_running = true;

    BaseType_t created =
      xTaskCreate(priv_async_worker_task, "async_worker", 4096, state, 5, &state->worker_task);

    if (created != pdPASS) {
      state->worker_running = false;
      ESP_LOGE(s_TAG, "Failed to create worker task");
      result = ESP_ERR_NO_MEM;
    }
  }

  xSemaphoreGive(state->queue_mutex);
  return result;
}

/* --- Public Functions --- */

esp_err_t star_bus_i2c_write_async(star_bus_manager_t*        manager,
                                   const char*                bus_name,
                                   const uint8_t*             data,
                                   size_t                     length,
                                   uint8_t                    command,
                                   const star_async_config_t* config,
                                   star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  /* Allocate operation */
  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_I2C_WRITE;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.i2c.data    = (uint8_t*)data;
  op->params.i2c.length  = length;
  op->params.i2c.command = command;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

esp_err_t star_bus_i2c_read_async(star_bus_manager_t*        manager,
                                  const char*                bus_name,
                                  uint8_t*                   data,
                                  size_t                     length,
                                  uint8_t                    command,
                                  const star_async_config_t* config,
                                  star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_I2C_READ;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.i2c.data    = data;
  op->params.i2c.length  = length;
  op->params.i2c.command = command;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

star_async_status_t star_async_get_status(star_async_handle_t handle)
{
  if (handle == NULL) {
    return STAR_ASYNC_STATUS_ERROR;
  }

  async_operation_t* op = (async_operation_t*)handle;
  return op->status;
}

esp_err_t star_async_wait(star_async_handle_t handle, uint32_t timeout_ms)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_operation_t* op = (async_operation_t*)handle;

  /* Create semaphore if not already created (thread-safe) */
  if (op->wait_semaphore == NULL) {
    portENTER_CRITICAL(&g_init_spinlock);
    if (op->wait_semaphore == NULL) {
      op->wait_semaphore = xSemaphoreCreateBinary();
    }
    portEXIT_CRITICAL(&g_init_spinlock);

    if (op->wait_semaphore == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }

  /* Wait for completion */
  TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  if (xSemaphoreTake(op->wait_semaphore, ticks) == pdTRUE) {
    return op->result;
  }

  return ESP_ERR_TIMEOUT;
}

esp_err_t star_async_cancel(star_async_handle_t handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_operation_t* op = (async_operation_t*)handle;

  if (op->status == STAR_ASYNC_STATUS_COMPLETE || op->status == STAR_ASYNC_STATUS_ERROR) {
    return ESP_ERR_INVALID_STATE;
  }

  op->status = STAR_ASYNC_STATUS_CANCELLED;
  op->result = ESP_ERR_INVALID_STATE;

  /* Invoke callback */
  if (op->callback != NULL) {
    op->callback(handle, op->status, op->result, op->user_context);
  }

  return ESP_OK;
}

esp_err_t star_async_get_result(star_async_handle_t handle, esp_err_t* result)
{
  if (handle == NULL || result == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_operation_t* op = (async_operation_t*)handle;
  *result               = op->result;

  return ESP_OK;
}

void star_async_free_handle(star_async_handle_t handle)
{
  if (handle != NULL) {
    async_operation_t* op = (async_operation_t*)handle;

    if (op->wait_semaphore != NULL) {
      vSemaphoreDelete(op->wait_semaphore);
    }

    /* Free SMBus write data if allocated */
    if (op->type == STAR_ASYNC_OP_SMBUS_WRITE && op->params.smbus.data != NULL) {
      free(op->params.smbus.data);
    }

    free(op);
  }
}

esp_err_t star_async_set_event_bits(star_async_handle_t handle,
                                    EventGroupHandle_t  event_group,
                                    EventBits_t         complete_bit,
                                    EventBits_t         error_bit)
{
  if (handle == NULL || event_group == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_operation_t* op = (async_operation_t*)handle;
  op->event_group       = event_group;
  op->complete_bit      = complete_bit;
  op->error_bit         = error_bit;

  return ESP_OK;
}

esp_err_t star_async_get_stats(const star_bus_manager_t* manager,
                               const char*               bus_name,
                               uint32_t*                 pending_ops,
                               uint64_t*                 completed_ops,
                               uint64_t*                 failed_ops,
                               uint64_t*                 cancelled_ops)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take mutex to read stats atomically */
  if (xSemaphoreTake(state->queue_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take mutex for stats");
    return ESP_ERR_TIMEOUT;
  }

  if (pending_ops != NULL) {
    *pending_ops = state->pending_count;
  }

  if (completed_ops != NULL) {
    *completed_ops = state->completed_ops;
  }

  if (failed_ops != NULL) {
    *failed_ops = state->failed_ops;
  }

  if (cancelled_ops != NULL) {
    *cancelled_ops = state->cancelled_ops;
  }

  xSemaphoreGive(state->queue_mutex);

  return ESP_OK;
}

/* --- SPI Async Operations --- */

esp_err_t star_bus_spi_transmit_async(star_bus_manager_t*        manager,
                                      const char*                bus_name,
                                      const uint8_t*             data,
                                      size_t                     length,
                                      const star_async_config_t* config,
                                      star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_SPI_TRANSMIT;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.spi.tx_data = (uint8_t*)data;
  op->params.spi.length  = length;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

esp_err_t star_bus_spi_receive_async(star_bus_manager_t*        manager,
                                     const char*                bus_name,
                                     uint8_t*                   data,
                                     size_t                     length,
                                     const star_async_config_t* config,
                                     star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_SPI_RECEIVE;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.spi.rx_data = data;
  op->params.spi.length  = length;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

esp_err_t star_bus_spi_transceive_async(star_bus_manager_t*        manager,
                                        const char*                bus_name,
                                        const uint8_t*             tx_data,
                                        uint8_t*                   rx_data,
                                        size_t                     length,
                                        const star_async_config_t* config,
                                        star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || tx_data == NULL || rx_data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_SPI_TRANSCEIVE;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.spi.tx_data = (uint8_t*)tx_data;
  op->params.spi.rx_data = rx_data;
  op->params.spi.length  = length;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

/* --- SMBus Async Operations --- */

esp_err_t star_smbus_read_byte_async(star_bus_manager_t*        manager,
                                     const char*                bus_name,
                                     uint8_t                    addr,
                                     uint8_t                    command,
                                     uint8_t*                   data,
                                     const star_async_config_t* config,
                                     star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || data == NULL || config == NULL ||
      config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_SMBUS_READ;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  op->params.smbus.addr    = addr;
  op->params.smbus.command = command;
  op->params.smbus.data    = data;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}

esp_err_t star_smbus_write_byte_async(star_bus_manager_t*        manager,
                                      const char*                bus_name,
                                      uint8_t                    addr,
                                      uint8_t                    command,
                                      uint8_t                    data,
                                      const star_async_config_t* config,
                                      star_async_handle_t*       handle)
{
  if (manager == NULL || bus_name == NULL || config == NULL || config->callback == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_state_t* state = priv_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = priv_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = STAR_ASYNC_OP_SMBUS_WRITE;
  op->status       = STAR_ASYNC_STATUS_PENDING;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);

  /* Allocate persistent storage for the data byte */
  op->params.smbus.data = (uint8_t*)malloc(sizeof(uint8_t));
  if (op->params.smbus.data == NULL) {
    free(op);
    return ESP_ERR_NO_MEM;
  }

  op->params.smbus.addr    = addr;
  op->params.smbus.command = command;
  *op->params.smbus.data   = data;

  result = priv_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op->params.smbus.data);
    free(op);
    return result;
  }

  if (handle != NULL) {
    *handle = (star_async_handle_t)op;
  }

  return ESP_OK;
}
