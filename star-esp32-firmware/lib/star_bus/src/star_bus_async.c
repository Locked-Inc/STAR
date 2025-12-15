/* lib/star_bus/src/star_bus_async.c */

#include "star_bus_async.h"

#include <esp_log.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <string.h>

#include "star_bus_i2c.h"
#include "star_bus_smbus.h"
#include "star_bus_spi.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_ASYNC";

/* Default mutex timeout in milliseconds to prevent infinite wait deadlocks */
static const uint32_t s_async_mutex_timeout_ms = 5000;

/* --- Types --- */

/**
 * @brief Async operation context
 *
 * @note The timeout_ms field is enforced by the worker task. Operations that
 *       exceed their timeout while waiting in the queue or during execution
 *       will be completed with ESP_ERR_TIMEOUT.
 * @note The priority field is stored but FIFO ordering is used. Priority queue
 *       ordering is not currently implemented.
 */
typedef struct async_operation {
  star_async_op_type_t  type;           /**< Operation type */
  star_async_status_t   status;         /**< Current status */
  esp_err_t             result;         /**< Operation result */
  star_async_callback_t callback;       /**< Completion callback */
  void*                 user_context;   /**< User context */
  uint32_t              timeout_ms;     /**< Timeout in ms (0 = no timeout) */
  uint8_t               priority;       /**< Priority (stored, FIFO used) */
  TickType_t            start_tick;     /**< Start time (when operation was enqueued) */
  EventGroupHandle_t    event_group;    /**< Optional event group */
  EventBits_t           complete_bit;   /**< Event bit for completion */
  EventBits_t           error_bit;      /**< Event bit for error */
  SemaphoreHandle_t     wait_semaphore; /**< Semaphore for wait function */

  /* Operation-specific data */
  star_bus_manager_t* manager;
  char                bus_name[32]; /* XXX: here again */

  union {
    struct {
      uint8_t* data;
      size_t   length;
      uint8_t  command;
      bool     owns_data; /**< True if data buffer is owned by operation */
    } i2c;

    struct {
      uint8_t* tx_data;
      uint8_t* rx_data;
      size_t   length;
      bool     owns_tx_data; /**< True if tx_data buffer is owned by operation */
    } spi;

    struct {
      uint8_t  addr;
      uint8_t  command;
      uint8_t* data;
      bool     owns_data; /**< True if data buffer is owned by operation */
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
 * @brief Check if an operation has exceeded its timeout
 *
 * @param op Pointer to the async operation
 * @return true if the operation has timed out, false otherwise
 */
static bool internal_check_timeout(const async_operation_t* op)
{
  if (op == NULL || op->timeout_ms == 0) {
    return false; /* No timeout configured */
  }

  TickType_t elapsed_ticks = xTaskGetTickCount() - op->start_tick;
  TickType_t timeout_ticks = pdMS_TO_TICKS(op->timeout_ms);

  return elapsed_ticks > timeout_ticks;
}

/**
 * @brief Complete an operation with timeout error
 *
 * @param op Pointer to the async operation
 */
static void internal_complete_with_timeout(async_operation_t* op)
{
  if (op == NULL) {
    return;
  }

  ESP_LOGW(s_TAG, "Operation timed out after %" PRIu32 "ms", op->timeout_ms);

  op->result = ESP_ERR_TIMEOUT;
  op->status = k_star_async_status_error;

  /* Set event bits if configured */
  if (op->event_group != NULL && op->error_bit != 0) {
    xEventGroupSetBits(op->event_group, op->error_bit);
  }

  /* Signal wait semaphore if waiting */
  if (op->wait_semaphore != NULL) {
    xSemaphoreGive(op->wait_semaphore);
  }

  /* Invoke callback */
  if (op->callback != NULL) {
    op->callback((star_async_handle_t)op, op->status, op->result, op->user_context);
  }
}

/**
 * @brief Initialize global async state (thread-safe)
 */
static void internal_init_global_state(void)
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
static async_state_t* internal_get_async_state(const char* bus_name)
{
  internal_init_global_state();

  if (g_global_mutex == NULL) {
    ESP_LOGE(s_TAG, "Global mutex not initialized");
    return NULL;
  }

  if (xSemaphoreTake(g_global_mutex, pdMS_TO_TICKS(s_async_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take global mutex");
    return NULL;
  }

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
static esp_err_t internal_enqueue_operation(async_state_t* state, async_operation_t* op)
{
  if (state == NULL || op == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(state->queue_mutex, pdMS_TO_TICKS(s_async_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take queue mutex for enqueue");
    return ESP_ERR_TIMEOUT;
  }

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
 *
 * @note This function checks for timeout after the operation completes.
 *       If the operation took longer than the configured timeout, the result
 *       is overridden with ESP_ERR_TIMEOUT even if the operation succeeded.
 */
static void internal_execute_operation(async_operation_t* op)
{
  if (op == NULL) {
    return;
  }

  op->status       = k_star_async_status_running;
  esp_err_t result = ESP_FAIL;

  switch (op->type) {
    case k_star_async_op_i2c_write:
      result = star_bus_i2c_write(op->manager,
                                  op->bus_name,
                                  op->params.i2c.data,
                                  op->params.i2c.length,
                                  op->params.i2c.command,
                                  NULL);
      break;

    case k_star_async_op_i2c_read:
      result = star_bus_i2c_read(op->manager,
                                 op->bus_name,
                                 op->params.i2c.data,
                                 op->params.i2c.length,
                                 op->params.i2c.command,
                                 NULL);
      break;

    case k_star_async_op_spi_transmit:
      result = star_bus_spi_transmit(op->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.length,
                                     0);
      break;

    case k_star_async_op_spi_receive:
      result = star_bus_spi_receive(op->manager,
                                    op->bus_name,
                                    op->params.spi.rx_data,
                                    op->params.spi.length,
                                    0);
      break;

    case k_star_async_op_spi_transceive:
      result = star_bus_spi_transfer(op->manager,
                                     op->bus_name,
                                     op->params.spi.tx_data,
                                     op->params.spi.rx_data,
                                     op->params.spi.length,
                                     0);
      break;

    case k_star_async_op_smbus_read:
      result = star_smbus_read_byte(op->manager,
                                    op->bus_name,
                                    op->params.smbus.addr,
                                    op->params.smbus.command,
                                    op->params.smbus.data);
      break;

    case k_star_async_op_smbus_write:
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

  /* Check if operation exceeded its timeout during execution */
  if (internal_check_timeout(op)) {
    ESP_LOGW(s_TAG, "Operation exceeded timeout (%" PRIu32 "ms) during execution", op->timeout_ms);
    result = ESP_ERR_TIMEOUT;
  }

  op->result = result;
  op->status = (result == ESP_OK) ? k_star_async_status_complete : k_star_async_status_error;

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
static void internal_async_worker_task(void* param)
{
  async_state_t* state = (async_state_t*)param;

  ESP_LOGI(s_TAG, "Async worker task started");

  while (state->worker_running) {
    async_operation_t* op = NULL;

    /* Dequeue next operation */
    if (xSemaphoreTake(state->queue_mutex, pdMS_TO_TICKS(s_async_mutex_timeout_ms)) != pdTRUE) {
      ESP_LOGE(s_TAG, "Failed to take queue mutex in worker");
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

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
      if (op->status == k_star_async_status_cancelled) {
        state->cancelled_ops++;
        /* Don't execute, callback was already invoked during cancel */
      } else if (internal_check_timeout(op)) {
        /* Operation timed out while waiting in queue */
        internal_complete_with_timeout(op);
        state->failed_ops++;
      } else {
        /* Execute the operation */
        internal_execute_operation(op);

        /* Update statistics */
        if (op->status == k_star_async_status_complete) {
          state->completed_ops++;
        } else if (op->status == k_star_async_status_error) {
          state->failed_ops++;
        } else if (op->status == k_star_async_status_cancelled) {
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
static esp_err_t internal_ensure_worker_running(async_state_t* state)
{
  if (state == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Quick check without lock for common case */
  if (state->worker_running) {
    return ESP_OK;
  }

  /* Use queue_mutex to prevent race condition */
  if (xSemaphoreTake(state->queue_mutex, pdMS_TO_TICKS(s_async_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take queue mutex for worker check");
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t result = ESP_OK;

  /* Double-check after acquiring mutex */
  if (!state->worker_running) {
    state->worker_running = true;

    BaseType_t created =
      xTaskCreate(internal_async_worker_task, "async_worker", 4096, state, 5, &state->worker_task);

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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  /* Allocate operation */
  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_i2c_write;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  /* Allocate and copy user data to prevent use-after-free if user buffer goes out of scope */
  op->params.i2c.data = (uint8_t*)malloc(length);
  if (op->params.i2c.data == NULL) {
    free(op);
    return ESP_ERR_NO_MEM;
  }
  memcpy(op->params.i2c.data, data, length);
  op->params.i2c.length    = length;
  op->params.i2c.command   = command;
  op->params.i2c.owns_data = true;

  result = internal_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op->params.i2c.data);
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_i2c_read;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  op->params.i2c.data    = data;
  op->params.i2c.length  = length;
  op->params.i2c.command = command;

  result = internal_enqueue_operation(state, op);
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
    return k_star_async_status_error;
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

  /* Wait for completion
   * Timeout semantics:
   *   0 = no wait (immediate return if not ready)
   *   STAR_ASYNC_WAIT_FOREVER (UINT32_MAX) = wait indefinitely
   *   other = wait for specified milliseconds
   */
  TickType_t ticks;
  if (timeout_ms == 0) {
    ticks = 0; /* No wait - immediate return */
  } else if (timeout_ms == UINT32_MAX) {
    ticks = portMAX_DELAY; /* Wait forever */
  } else {
    ticks = pdMS_TO_TICKS(timeout_ms);
  }

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

  if (op->status == k_star_async_status_complete || op->status == k_star_async_status_error) {
    return ESP_ERR_INVALID_STATE;
  }

  op->status = k_star_async_status_cancelled;
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

esp_err_t star_async_free_handle(star_async_handle_t handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  async_operation_t* op = (async_operation_t*)handle;

  if (op->wait_semaphore != NULL) {
    vSemaphoreDelete(op->wait_semaphore);
  }

  /* Free owned data buffers based on operation type */
  switch (op->type) {
    case k_star_async_op_i2c_write:
      if (op->params.i2c.owns_data && op->params.i2c.data != NULL) {
        free(op->params.i2c.data);
      }
      break;

    case k_star_async_op_spi_transmit:
    case k_star_async_op_spi_transceive:
      if (op->params.spi.owns_tx_data && op->params.spi.tx_data != NULL) {
        free(op->params.spi.tx_data);
      }
      break;

    case k_star_async_op_smbus_write:
      if (op->params.smbus.owns_data && op->params.smbus.data != NULL) {
        free(op->params.smbus.data);
      }
      break;

    default:
      /* No owned buffers for read operations */
      break;
  }

  free(op);

  return ESP_OK;
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take mutex to read stats atomically */
  if (xSemaphoreTake(state->queue_mutex, pdMS_TO_TICKS(s_async_mutex_timeout_ms)) != pdTRUE) {
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_spi_transmit;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  /* Allocate and copy user data to prevent use-after-free if user buffer goes out of scope */
  op->params.spi.tx_data = (uint8_t*)malloc(length);
  if (op->params.spi.tx_data == NULL) {
    free(op);
    return ESP_ERR_NO_MEM;
  }
  memcpy(op->params.spi.tx_data, data, length);
  op->params.spi.length       = length;
  op->params.spi.owns_tx_data = true;

  result = internal_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op->params.spi.tx_data);
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_spi_receive;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  op->params.spi.rx_data = data;
  op->params.spi.length  = length;

  result = internal_enqueue_operation(state, op);
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_spi_transceive;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  /* Allocate and copy tx_data to prevent use-after-free if user buffer goes out of scope */
  /* Note: rx_data must remain valid as it is written to by the operation */
  op->params.spi.tx_data = (uint8_t*)malloc(length);
  if (op->params.spi.tx_data == NULL) {
    free(op);
    return ESP_ERR_NO_MEM;
  }
  memcpy(op->params.spi.tx_data, tx_data, length);
  op->params.spi.rx_data      = rx_data;
  op->params.spi.length       = length;
  op->params.spi.owns_tx_data = true;

  result = internal_enqueue_operation(state, op);
  if (result != ESP_OK) {
    free(op->params.spi.tx_data);
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_smbus_read;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  op->params.smbus.addr    = addr;
  op->params.smbus.command = command;
  op->params.smbus.data    = data;

  result = internal_enqueue_operation(state, op);
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

  async_state_t* state = internal_get_async_state(bus_name);
  if (state == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t result = internal_ensure_worker_running(state);
  if (result != ESP_OK) {
    return result;
  }

  async_operation_t* op = (async_operation_t*)malloc(sizeof(async_operation_t));
  if (op == NULL) {
    return ESP_ERR_NO_MEM;
  }

  memset(op, 0, sizeof(async_operation_t));

  op->type         = k_star_async_op_smbus_write;
  op->status       = k_star_async_status_pending;
  op->callback     = config->callback;
  op->user_context = config->context;
  op->timeout_ms   = config->timeout_ms;
  op->priority     = config->priority;
  op->start_tick   = xTaskGetTickCount();
  op->manager      = manager;
  strncpy(op->bus_name, bus_name, sizeof(op->bus_name) - 1);
  op->bus_name[sizeof(op->bus_name) - 1] = '\0';

  /* Allocate persistent storage for the data byte */
  op->params.smbus.data = (uint8_t*)malloc(sizeof(uint8_t));
  if (op->params.smbus.data == NULL) {
    free(op);
    return ESP_ERR_NO_MEM;
  }

  op->params.smbus.addr      = addr;
  op->params.smbus.command   = command;
  *op->params.smbus.data     = data;
  op->params.smbus.owns_data = true;

  result = internal_enqueue_operation(state, op);
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
