/* lib/star_bus/src/star_bus_uart.c */

#include "star_bus_uart.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <string.h>

/* --- Constants --- */

static const char* s_TAG = "STAR_UART";

/* Default mutex timeout in milliseconds to prevent infinite wait deadlocks */
static const uint32_t s_uart_mutex_timeout_ms = 5000;

#define MAX_UART_BUSES (3) /* VERIFY: This prob has to be a macro, but verify */

/* --- Types --- */

/**
 * @brief UART bus state
 */
typedef struct {
  char               bus_name[STAR_BUS_NAME_MAX_LEN];
  star_uart_config_t config;
  star_uart_stats_t  stats;
  SemaphoreHandle_t  stats_mutex; /**< Mutex for stats access */
  SemaphoreHandle_t  ops_mutex;   /**< Mutex for operation access */
  bool               initialized;
  QueueHandle_t      event_queue;
  TaskHandle_t       event_task;
  bool               event_task_running;
} uart_state_t;

/* --- Static Storage --- */

static uart_state_t      g_uart_states[MAX_UART_BUSES] = {0};
static uint8_t           g_num_uart_states             = 0;
static SemaphoreHandle_t g_uart_mutex                  = NULL;
static portMUX_TYPE      g_uart_init_spinlock          = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE      g_uart_state_spinlock         = portMUX_INITIALIZER_UNLOCKED;

/* --- Helper Functions --- */

/**
 * @brief Initialize UART global mutex (thread-safe)
 */
static esp_err_t internal_init_uart_mutex(void)
{
  /* Quick check without lock for common case */
  if (g_uart_mutex != NULL) {
    return ESP_OK;
  }

  /* Use spinlock for thread-safe lazy initialization */
  portENTER_CRITICAL(&g_uart_init_spinlock);

  /* Double-check after acquiring spinlock */
  if (g_uart_mutex == NULL) {
    g_uart_mutex = xSemaphoreCreateMutex();
    if (g_uart_mutex == NULL) {
      portEXIT_CRITICAL(&g_uart_init_spinlock);
      ESP_LOGE(s_TAG, "Failed to create UART global mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  portEXIT_CRITICAL(&g_uart_init_spinlock);
  return ESP_OK;
}

/**
 * @brief Get UART state for a bus (must be called with mutex held)
 *
 * Uses spinlock to protect access to global state array during iteration
 * to prevent race conditions with concurrent array modifications.
 */
static uart_state_t* internal_get_uart_state(const char* bus_name, bool create)
{
  uart_state_t* found_state = NULL;
  uint8_t       current_count;
  uint8_t       slot_index = 0;

  /* Protect array read with spinlock */
  portENTER_CRITICAL(&g_uart_state_spinlock);
  current_count = g_num_uart_states;
  for (uint8_t i = 0; i < current_count; i++) {
    if (strcmp(g_uart_states[i].bus_name, bus_name) == 0) {
      found_state = &g_uart_states[i];
      break;
    }
  }
  portEXIT_CRITICAL(&g_uart_state_spinlock);

  if (found_state != NULL) {
    return found_state;
  }

  if (create) {
    /* Check capacity and reserve slot under spinlock */
    portENTER_CRITICAL(&g_uart_state_spinlock);
    if (g_num_uart_states >= MAX_UART_BUSES) {
      portEXIT_CRITICAL(&g_uart_state_spinlock);
      return NULL;
    }
    slot_index = g_num_uart_states;
    /* Increment count to reserve the slot */
    g_num_uart_states++;
    portEXIT_CRITICAL(&g_uart_state_spinlock);

    /* Initialize the reserved slot outside spinlock (safe since slot is reserved) */
    uart_state_t* state = &g_uart_states[slot_index];
    memset(state, 0, sizeof(uart_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    state->bus_name[sizeof(state->bus_name) - 1] = '\0';

    /* Create stats mutex */
    state->stats_mutex = xSemaphoreCreateMutex();
    if (state->stats_mutex == NULL) {
      ESP_LOGE(s_TAG, "Failed to create stats mutex for '%s'", bus_name);
      /* Decrement count to release the slot */
      portENTER_CRITICAL(&g_uart_state_spinlock);
      g_num_uart_states--;
      portEXIT_CRITICAL(&g_uart_state_spinlock);
      return NULL;
    }

    /* Create operations mutex */
    state->ops_mutex = xSemaphoreCreateMutex();
    if (state->ops_mutex == NULL) {
      ESP_LOGE(s_TAG, "Failed to create ops mutex for '%s'", bus_name);
      vSemaphoreDelete(state->stats_mutex);
      /* Decrement count to release the slot */
      portENTER_CRITICAL(&g_uart_state_spinlock);
      g_num_uart_states--;
      portEXIT_CRITICAL(&g_uart_state_spinlock);
      return NULL;
    }

    return state;
  }

  return NULL;
}

/**
 * @brief Get UART state for a bus (thread-safe)
 */
static uart_state_t* get_uart_state(const char* bus_name, bool create)
{
  if (internal_init_uart_mutex() != ESP_OK) {
    return NULL;
  }

  if (xSemaphoreTake(g_uart_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take UART mutex");
    return NULL;
  }

  uart_state_t* state = internal_get_uart_state(bus_name, create);

  xSemaphoreGive(g_uart_mutex);
  return state;
}

/**
 * @brief Event task for processing UART events
 */
static void internal_uart_event_task(void* param)
{
  uart_state_t* state = (uart_state_t*)param;
  uart_event_t  event;

  ESP_LOGI(s_TAG, "UART event task started for bus '%s'", state->bus_name);

  while (state->event_task_running) {
    if (xQueueReceive(state->event_queue, &event, pdMS_TO_TICKS(100))) {
      /* Track errors in statistics (with mutex protection) */
      if (xSemaphoreTake(state->stats_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) == pdTRUE) {
        switch (event.type) {
          case UART_PARITY_ERR:
            state->stats.parity_errors++;
            break;

          case UART_FRAME_ERR:
            state->stats.frame_errors++;
            break;

          case UART_BUFFER_FULL:
          case UART_FIFO_OVF:
            state->stats.overrun_errors++;
            break;

          case UART_BREAK:
            state->stats.break_conditions++;
            break;

          default:
            break;
        }
        xSemaphoreGive(state->stats_mutex);
      }

      /* Call user callback if registered */
      if (state->config.event_callback != NULL) {
        state->config.event_callback(state->bus_name,
                                     event.type,
                                     event.size,
                                     state->config.event_context);
      }
    }
  }

  ESP_LOGI(s_TAG, "UART event task stopped for bus '%s'", state->bus_name);
  vTaskDelete(NULL);
}

/* --- Public Functions --- */

esp_err_t star_bus_uart_init(star_bus_manager_t*       manager,
                             const char*               bus_name,
                             const star_uart_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->tx_pin < 0 && config->rx_pin < 0) {
    ESP_LOGE(s_TAG, "At least one of TX or RX pin must be specified");
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(s_TAG, "Failed to create state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  memcpy(&state->config, config, sizeof(star_uart_config_t));
  memset(&state->stats, 0, sizeof(star_uart_stats_t));

  /* Configure UART parameters */
  uart_config_t uart_config = {
    .baud_rate           = config->baud_rate,
    .data_bits           = (uart_word_length_t)config->data_bits,
    .parity              = (uart_parity_t)config->parity,
    .stop_bits           = (uart_stop_bits_t)config->stop_bits,
    .flow_ctrl           = (uart_hw_flowcontrol_t)config->flow_ctrl,
    .rx_flow_ctrl_thresh = config->rx_thresh,
#if CONFIG_IDF_TARGET_ESP32
    .source_clk = config->use_ref_tick ? UART_SCLK_REF_TICK : UART_SCLK_DEFAULT,
#else
    .source_clk = UART_SCLK_DEFAULT,
#endif
  };

  esp_err_t result = uart_param_config(config->port, &uart_config);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure UART: %s", esp_err_to_name(result));
    return result;
  }

  /* Set pins */
  result =
    uart_set_pin(config->port, config->tx_pin, config->rx_pin, config->rts_pin, config->cts_pin);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set UART pins: %s", esp_err_to_name(result));
    return result;
  }

  /* Set mode if RS-485 */
  if (config->mode == k_star_uart_mode_rs485) {
    result = uart_set_mode(config->port, (uart_mode_t)config->mode);
    if (result != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to set UART mode: %s", esp_err_to_name(result));
      return result;
    }
  }

  /* Install UART driver */
  result = uart_driver_install(config->port,
                               config->rx_buffer_size,
                               config->tx_buffer_size,
                               config->event_queue_size,
                               &state->event_queue,
                               0);

  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to install UART driver: %s", esp_err_to_name(result));
    return result;
  }

  /* Start event task if callback is registered */
  if (config->event_callback != NULL && config->event_queue_size > 0) {
    state->event_task_running = true;
    BaseType_t task_result =
      xTaskCreate(internal_uart_event_task, "uart_event", 4096, state, 5, &state->event_task);
    if (task_result != pdPASS) {
      ESP_LOGE(s_TAG, "Failed to create event task");
      uart_driver_delete(config->port);
      return ESP_ERR_NO_MEM;
    }
  }

  state->initialized = true;

  ESP_LOGI(s_TAG,
           "UART bus '%s' initialized on port %d at %" PRIu32 " baud",
           bus_name,
           config->port,
           config->baud_rate);

  return ESP_OK;
}

esp_err_t star_bus_uart_deinit(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent race with ongoing operations */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take ops mutex for deinit '%s'", bus_name);
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  /* Stop event task */
  if (state->event_task_running) {
    state->event_task_running = false;
    /* Release mutex while waiting for task to exit */
    xSemaphoreGive(state->ops_mutex);
    vTaskDelay(pdMS_TO_TICKS(200)); /* Wait for task to exit */
    /* Re-acquire mutex */
    if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
      ESP_LOGE(s_TAG, "Failed to re-take ops mutex for deinit '%s'", bus_name);
      return ESP_ERR_TIMEOUT;
    }
  }

  /* Delete UART driver */
  uart_driver_delete(state->config.port);

  state->initialized = false;

  xSemaphoreGive(state->ops_mutex);

  ESP_LOGI(s_TAG, "UART bus '%s' deinitialized", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_uart_write(star_bus_manager_t* manager,
                              const char*         bus_name,
                              const uint8_t*      data,
                              size_t              length)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take ops mutex for '%s'", bus_name);
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  uint32_t start_time = esp_timer_get_time();

  int32_t written = uart_write_bytes(state->config.port, data, length);

  uint32_t elapsed = (uint32_t)(esp_timer_get_time() - start_time);

  /* Update stats with mutex protection */
  if (xSemaphoreTake(state->stats_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) == pdTRUE) {
    state->stats.last_tx_time_us = elapsed;
    if (written >= 0) {
      state->stats.bytes_sent += written;
      state->stats.tx_operations++;
    }
    xSemaphoreGive(state->stats_mutex);
  }

  xSemaphoreGive(state->ops_mutex);

  if (written < 0) {
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t star_bus_uart_read(star_bus_manager_t* manager,
                             const char*         bus_name,
                             uint8_t*            data,
                             size_t              buffer_size,
                             size_t*             received,
                             uint32_t            timeout_ms)
{
  if (manager == NULL || bus_name == NULL || data == NULL || received == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take ops mutex for '%s'", bus_name);
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  uint32_t start_time = esp_timer_get_time();

  int32_t read = uart_read_bytes(state->config.port, data, buffer_size, pdMS_TO_TICKS(timeout_ms));

  uint32_t elapsed = (uint32_t)(esp_timer_get_time() - start_time);

  /* Update stats with mutex protection */
  if (xSemaphoreTake(state->stats_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) == pdTRUE) {
    state->stats.last_rx_time_us = elapsed;
    if (read >= 0) {
      state->stats.bytes_received += read;
      state->stats.rx_operations++;
    }
    xSemaphoreGive(state->stats_mutex);
  }

  xSemaphoreGive(state->ops_mutex);

  if (read < 0) {
    *received = 0;
    return ESP_FAIL;
  }

  *received = read;

  return ESP_OK;
}

esp_err_t
star_bus_uart_write_string(star_bus_manager_t* manager, const char* bus_name, const char* str)
{
  if (str == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_uart_write(manager, bus_name, (const uint8_t*)str, strlen(str));
}

esp_err_t star_bus_uart_read_line(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  char*               buffer,
                                  size_t              buffer_size,
                                  size_t*             length,
                                  uint32_t            timeout_ms)
{
  if (manager == NULL || bus_name == NULL || buffer == NULL || length == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Verify initialized state under mutex protection */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  xSemaphoreGive(state->ops_mutex);

  *length             = 0;
  uint32_t start_time = pdTICKS_TO_MS(xTaskGetTickCount());

  while (*length < buffer_size - 1) {
    uint8_t byte;
    size_t  received;

    uint32_t elapsed = pdTICKS_TO_MS(xTaskGetTickCount()) - start_time;
    if (elapsed >= timeout_ms) {
      break;
    }

    esp_err_t result =
      star_bus_uart_read(manager, bus_name, &byte, 1, &received, timeout_ms - elapsed);

    if (result != ESP_OK || received == 0) {
      break;
    }

    buffer[*length] = byte;
    (*length)++;

    /* Check for newline */
    if (byte == '\n' || byte == '\r') {
      break;
    }
  }

  buffer[*length] = '\0';

  return (*length > 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t star_bus_uart_get_available(const star_bus_manager_t* manager,
                                      const char*               bus_name,
                                      size_t*                   available)
{
  if (manager == NULL || bus_name == NULL || available == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_get_buffered_data_len(state->config.port, available);

  xSemaphoreGive(state->ops_mutex);

  return result;
}

esp_err_t star_bus_uart_flush(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_wait_tx_done(state->config.port, pdMS_TO_TICKS(1000));

  xSemaphoreGive(state->ops_mutex);

  return result;
}

esp_err_t star_bus_uart_clear_rx(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_flush_input(state->config.port);

  xSemaphoreGive(state->ops_mutex);

  return result;
}

esp_err_t star_bus_uart_enable_pattern(star_bus_manager_t* manager,
                                       const char*         bus_name,
                                       char                pattern,
                                       uint8_t             count,
                                       int32_t             post_idle,
                                       int32_t             pre_idle)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_enable_pattern_det_baud_intr(state->config.port,
                                                       pattern,
                                                       count,
                                                       post_idle,
                                                       pre_idle,
                                                       pre_idle);

  xSemaphoreGive(state->ops_mutex);

  return result;
}

esp_err_t star_bus_uart_disable_pattern(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Take operation mutex to prevent use-after-free */
  if (xSemaphoreTake(state->ops_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (!state->initialized) {
    xSemaphoreGive(state->ops_mutex);
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_disable_pattern_det_intr(state->config.port);

  xSemaphoreGive(state->ops_mutex);

  return result;
}

esp_err_t star_bus_uart_get_stats(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_uart_stats_t*        stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Read stats with mutex protection */
  if (xSemaphoreTake(state->stats_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take stats mutex for '%s'", bus_name);
    return ESP_ERR_TIMEOUT;
  }

  memcpy(stats, &state->stats, sizeof(star_uart_stats_t));

  xSemaphoreGive(state->stats_mutex);

  return ESP_OK;
}

esp_err_t star_bus_uart_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Reset stats with mutex protection */
  if (xSemaphoreTake(state->stats_mutex, pdMS_TO_TICKS(s_uart_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to take stats mutex for '%s'", bus_name);
    return ESP_ERR_TIMEOUT;
  }

  memset(&state->stats, 0, sizeof(star_uart_stats_t));

  xSemaphoreGive(state->stats_mutex);

  ESP_LOGI(s_TAG, "Statistics reset for UART bus '%s'", bus_name);

  return ESP_OK;
}

/* VERIFY: Should this be using printf, is there any good/valid reason for it */
