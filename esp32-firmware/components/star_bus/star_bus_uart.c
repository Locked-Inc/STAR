/* esp32-firmware/components/star_bus/star_bus_uart.c */

#include "star_bus_uart.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

/* --- Constants --- */

static const char* TAG = "STAR_UART";

#define MAX_UART_BUSES (3)

/* --- Types --- */

/**
 * @brief UART bus state
 */
typedef struct {
  char               bus_name[32];
  star_uart_config_t config;
  star_uart_stats_t  stats;
  bool               initialized;
  QueueHandle_t      event_queue;
  TaskHandle_t       event_task;
  bool               event_task_running;
} uart_state_t;

/* --- Static Storage --- */

static uart_state_t g_uart_states[MAX_UART_BUSES] = {0};
static uint8_t      g_num_uart_states             = 0;

/* --- Helper Functions --- */

/**
 * @brief Get UART state for a bus
 */
static uart_state_t* get_uart_state(const char* bus_name, bool create)
{
  for (uint8_t i = 0; i < g_num_uart_states; i++) {
    if (strcmp(g_uart_states[i].bus_name, bus_name) == 0) {
      return &g_uart_states[i];
    }
  }

  if (create && g_num_uart_states < MAX_UART_BUSES) {
    uart_state_t* state = &g_uart_states[g_num_uart_states];
    memset(state, 0, sizeof(uart_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    g_num_uart_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Event task for processing UART events
 */
static void uart_event_task(void* param)
{
  uart_state_t* state = (uart_state_t*)param;
  uart_event_t  event;

  ESP_LOGI(TAG, "UART event task started for bus '%s'", state->bus_name);

  while (state->event_task_running) {
    if (xQueueReceive(state->event_queue, &event, pdMS_TO_TICKS(100))) {
      /* Track errors in statistics */
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

      /* Call user callback if registered */
      if (state->config.event_callback != NULL) {
        state->config.event_callback(state->bus_name,
                                     event.type,
                                     event.size,
                                     state->config.event_context);
      }
    }
  }

  ESP_LOGI(TAG, "UART event task stopped for bus '%s'", state->bus_name);
  vTaskDelete(NULL);
}

/* --- Public Functions --- */

esp_err_t star_bus_uart_init(star_bus_manager_t*       manager,
                             const char*               bus_name,
                             const star_uart_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->tx_pin < 0 && config->rx_pin < 0) {
    ESP_LOGE(TAG, "At least one of TX or RX pin must be specified");
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(TAG, "Failed to create state for bus '%s'", bus_name);
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
    .source_clk          = config->use_ref_tick ? UART_SCLK_REF_TICK : UART_SCLK_DEFAULT,
  };

  esp_err_t result = uart_param_config(config->port, &uart_config);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(result));
    return result;
  }

  /* Set pins */
  result =
    uart_set_pin(config->port, config->tx_pin, config->rx_pin, config->rts_pin, config->cts_pin);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(result));
    return result;
  }

  /* Set mode if RS-485 */
  if (config->mode == STAR_UART_MODE_RS485) {
    result = uart_set_mode(config->port, (uart_mode_t)config->mode);
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set UART mode: %s", esp_err_to_name(result));
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
    ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(result));
    return result;
  }

  /* Start event task if callback is registered */
  if (config->event_callback != NULL && config->event_queue_size > 0) {
    state->event_task_running = true;
    BaseType_t task_result =
      xTaskCreate(uart_event_task, "uart_event", 4096, state, 5, &state->event_task);
    if (task_result != pdPASS) {
      ESP_LOGE(TAG, "Failed to create event task");
      uart_driver_delete(config->port);
      return ESP_ERR_NO_MEM;
    }
  }

  state->initialized = true;

  ESP_LOGI(TAG,
           "UART bus '%s' initialized on port %d at %lu baud",
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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Stop event task */
  if (state->event_task_running) {
    state->event_task_running = false;
    vTaskDelay(pdMS_TO_TICKS(200)); /* Wait for task to exit */
  }

  /* Delete UART driver */
  uart_driver_delete(state->config.port);

  state->initialized = false;

  ESP_LOGI(TAG, "UART bus '%s' deinitialized", bus_name);

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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  uint32_t start_time = esp_timer_get_time();

  int written = uart_write_bytes(state->config.port, data, length);

  uint32_t elapsed             = (uint32_t)(esp_timer_get_time() - start_time);
  state->stats.last_tx_time_us = elapsed;

  if (written < 0) {
    return ESP_FAIL;
  }

  state->stats.bytes_sent += written;
  state->stats.tx_operations++;

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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  uint32_t start_time = esp_timer_get_time();

  int read = uart_read_bytes(state->config.port, data, buffer_size, pdMS_TO_TICKS(timeout_ms));

  uint32_t elapsed             = (uint32_t)(esp_timer_get_time() - start_time);
  state->stats.last_rx_time_us = elapsed;

  if (read < 0) {
    *received = 0;
    return ESP_FAIL;
  }

  *received = read;
  state->stats.bytes_received += read;
  state->stats.rx_operations++;

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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  esp_err_t result = uart_get_buffered_data_len(state->config.port, available);

  return result;
}

esp_err_t star_bus_uart_flush(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  return uart_wait_tx_done(state->config.port, pdMS_TO_TICKS(1000));
}

esp_err_t star_bus_uart_clear_rx(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  return uart_flush_input(state->config.port);
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
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  return uart_enable_pattern_det_baud_intr(state->config.port,
                                           pattern,
                                           count,
                                           post_idle,
                                           pre_idle,
                                           pre_idle);
}

esp_err_t star_bus_uart_disable_pattern(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_state_t* state = get_uart_state(bus_name, false);
  if (state == NULL || !state->initialized) {
    return ESP_ERR_NOT_FOUND;
  }

  return uart_disable_pattern_det_intr(state->config.port);
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

  memcpy(stats, &state->stats, sizeof(star_uart_stats_t));

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

  memset(&state->stats, 0, sizeof(star_uart_stats_t));

  ESP_LOGI(TAG, "Statistics reset for UART bus '%s'", bus_name);

  return ESP_OK;
}

void star_bus_uart_print_stats(const char* bus_name, const star_uart_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== UART Statistics: %s ===\n", bus_name);
  printf("Data Transfer:\n");
  printf("  Bytes Sent:     %llu\n", stats->bytes_sent);
  printf("  Bytes Received: %llu\n", stats->bytes_received);
  printf("  TX Operations:  %llu\n", stats->tx_operations);
  printf("  RX Operations:  %llu\n", stats->rx_operations);

  printf("\nErrors:\n");
  printf("  Parity:   %llu\n", stats->parity_errors);
  printf("  Frame:    %llu\n", stats->frame_errors);
  printf("  Overrun:  %llu\n", stats->overrun_errors);
  printf("  Break:    %llu\n", stats->break_conditions);

  printf("\nTiming:\n");
  printf("  Last TX: %lu us\n", stats->last_tx_time_us);
  printf("  Last RX: %lu us\n", stats->last_rx_time_us);

  printf("===============================\n\n");
}
