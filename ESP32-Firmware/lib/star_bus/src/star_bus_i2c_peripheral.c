/* esp32-firmware/components/star_bus/star_bus_i2c_peripheral.c */

#include "star_bus_i2c_peripheral.h"

#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <string.h>

/* --- Constants --- */

static const char* s_TAG = "STAR_I2C_PERIPH";

#define MAX_PERIPHERAL_BUSES (4)

/* --- Types --- */

/**
 * @brief I2C peripheral state
 */
typedef struct {
  char                         bus_name[32];
  star_i2c_peripheral_config_t config;
  star_i2c_peripheral_stats_t  stats;
  bool                         enabled;
  i2c_port_t                   port;

  /* Buffers */
  uint8_t* rx_buffer;
  uint8_t* tx_buffer;
  size_t   tx_data_length; /**< Valid data in tx_buffer */

  /* Last write data */
  uint8_t* last_write_data;
  size_t   last_write_length;

  /* Task handle for peripheral processing */
  TaskHandle_t task_handle;
  bool         task_running;
} peripheral_state_t;

/* --- Static Storage --- */

static peripheral_state_t g_peripheral_states[MAX_PERIPHERAL_BUSES] = {0};
static uint8_t            g_num_peripheral_states                   = 0;

/* --- Helper Functions --- */

/**
 * @brief Get peripheral state for a bus
 */
static peripheral_state_t* get_peripheral_state(const char* bus_name, bool create)
{
  /* Search for existing state */
  for (uint8_t i = 0; i < g_num_peripheral_states; i++) {
    if (strcmp(g_peripheral_states[i].bus_name, bus_name) == 0) {
      return &g_peripheral_states[i];
    }
  }

  /* Create new state if requested and space available */
  if (create && g_num_peripheral_states < MAX_PERIPHERAL_BUSES) {
    peripheral_state_t* state = &g_peripheral_states[g_num_peripheral_states];
    memset(state, 0, sizeof(peripheral_state_t));
    strncpy(state->bus_name, bus_name, sizeof(state->bus_name) - 1);
    g_num_peripheral_states++;
    return state;
  }

  return NULL;
}

/**
 * @brief Get I2C port number from bus name
 */
static esp_err_t internal_get_i2c_port(const char* bus_name, i2c_port_t* port)
{
  if (bus_name == NULL || port == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Parse bus name to extract port number */
  if (strncmp(bus_name, "i2c", 3) == 0) {
    int32_t port_num = bus_name[3] - '0';
    if (port_num >= 0 && port_num < I2C_NUM_MAX) {
      *port = (i2c_port_t)port_num;
      return ESP_OK;
    }
  }

  return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Peripheral task - handles I2C slave communication
 */
static void internal_peripheral_task(void* param)
{
  peripheral_state_t* state = (peripheral_state_t*)param;

  ESP_LOGI(s_TAG, "Peripheral task started for bus '%s'", state->bus_name);

  while (state->task_running) {
    /* Read from I2C slave buffer */
    int32_t len = i2c_slave_read_buffer(state->port,
                                        state->rx_buffer,
                                        state->config.rx_buffer_size,
                                        pdMS_TO_TICKS(100));

    if (len > 0) {
      /* Master wrote data to us */
      state->stats.total_write_requests++;
      state->stats.total_bytes_received += len;

      /* Store last write data */
      if (state->last_write_data != NULL && len <= state->config.rx_buffer_size) {
        memcpy(state->last_write_data, state->rx_buffer, len);
        state->last_write_length = len;
      }

      /* Call write callback */
      if (state->config.on_write != NULL) {
        uint32_t  start_time = esp_timer_get_time();
        esp_err_t result     = state->config.on_write(state->rx_buffer, len, state->config.context);
        uint32_t  elapsed    = (uint32_t)(esp_timer_get_time() - start_time);
        state->stats.last_request_time_us = elapsed;

        if (result != ESP_OK) {
          state->stats.failed_writes++;
          state->stats.callback_errors++;
          state->stats.last_error = result;

          if (state->config.on_error != NULL) {
            state->config.on_error(result, state->config.context);
          }
        }
      }
    }

    /* Check if master is reading from us */
    /* Note: For ESP32 I2C slave, we pre-load data into the tx buffer
     * The driver will send this data when master requests it */
    if (state->tx_data_length > 0) {
      /* Data is already loaded, just track stats when it gets sent */
      /* ESP32 I2C slave driver doesn't provide easy way to detect when
       * data was actually sent, so we estimate based on buffer state */
    }

    /* Small delay to prevent task from hogging CPU */
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_TAG, "Peripheral task stopped for bus '%s'", state->bus_name);
  vTaskDelete(NULL);
}

/**
 * @brief Start peripheral task
 */
static esp_err_t internal_start_peripheral_task(peripheral_state_t* state)
{
  if (state->task_running) {
    return ESP_OK; /* Already running */
  }

  state->task_running = true;

  BaseType_t result =
    xTaskCreate(internal_peripheral_task, "i2c_periph", 4096, state, 5, &state->task_handle);

  if (result != pdPASS) {
    state->task_running = false;
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

/**
 * @brief Stop peripheral task
 */
static void internal_stop_peripheral_task(peripheral_state_t* state)
{
  if (!state->task_running) {
    return;
  }

  state->task_running = false;

  /* Wait for task to exit */
  if (state->task_handle != NULL) {
    /* Task will delete itself */
    vTaskDelay(pdMS_TO_TICKS(200)); /* Give time to exit */
    state->task_handle = NULL;
  }
}

/* --- Public Functions --- */

esp_err_t star_bus_i2c_peripheral_enable(star_bus_manager_t*                 manager,
                                         const char*                         bus_name,
                                         const star_i2c_peripheral_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->on_read == NULL || config->on_write == NULL) {
    ESP_LOGE(s_TAG, "Read and write callbacks are required");
    return ESP_ERR_INVALID_ARG;
  }

  /* Validate address */
  if (config->addr_mode == STAR_I2C_ADDR_7BIT) {
    if (!STAR_I2C_ADDR_7BIT_VALID(config->address)) {
      ESP_LOGE(s_TAG, "Invalid 7-bit address: 0x%02x", config->address);
      return ESP_ERR_INVALID_ARG;
    }
  } else {
    if (!STAR_I2C_ADDR_10BIT_VALID(config->address)) {
      ESP_LOGE(s_TAG, "Invalid 10-bit address: 0x%03x", config->address);
      return ESP_ERR_INVALID_ARG;
    }
  }

  /* Get or create peripheral state */
  peripheral_state_t* state = get_peripheral_state(bus_name, true);
  if (state == NULL) {
    ESP_LOGE(s_TAG, "Failed to create peripheral state for bus '%s'", bus_name);
    return ESP_ERR_NO_MEM;
  }

  /* Get I2C port number */
  esp_err_t result = internal_get_i2c_port(bus_name, &state->port);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to get I2C port for bus '%s'", bus_name);
    return result;
  }

  /* Store configuration */
  memcpy(&state->config, config, sizeof(star_i2c_peripheral_config_t));

  /* Allocate buffers */
  state->rx_buffer       = malloc(config->rx_buffer_size);
  state->tx_buffer       = malloc(config->tx_buffer_size);
  state->last_write_data = malloc(config->rx_buffer_size);

  if (state->rx_buffer == NULL || state->tx_buffer == NULL || state->last_write_data == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate buffers");
    free(state->rx_buffer);
    free(state->tx_buffer);
    free(state->last_write_data);
    return ESP_ERR_NO_MEM;
  }

  memset(state->rx_buffer, 0, config->rx_buffer_size);
  memset(state->tx_buffer, 0, config->tx_buffer_size);
  memset(state->last_write_data, 0, config->rx_buffer_size);
  state->tx_data_length    = 0;
  state->last_write_length = 0;

  /* Configure I2C peripheral mode */
  i2c_config_t i2c_config = {
    .mode                = I2C_MODE_SLAVE,
    .sda_pullup_en       = GPIO_PULLUP_ENABLE,
    .scl_pullup_en       = GPIO_PULLUP_ENABLE,
    .slave.addr_10bit_en = config->enable_10bit_addr ? 1 : 0,
    .slave.slave_addr    = config->address,
    .slave.maximum_speed = 100000, /* 100 kHz default */
  };

  /* Note: SDA and SCL pins must already be configured in master mode setup */
  /* This function only changes the mode to slave */

  result = i2c_param_config(state->port, &i2c_config);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure I2C peripheral: %s", esp_err_to_name(result));
    free(state->rx_buffer);
    free(state->tx_buffer);
    free(state->last_write_data);
    return result;
  }

  /* Install I2C driver in slave mode */
  result = i2c_driver_install(state->port,
                              I2C_MODE_SLAVE,
                              config->rx_buffer_size,
                              config->tx_buffer_size,
                              0);

  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to install I2C peripheral driver: %s", esp_err_to_name(result));
    free(state->rx_buffer);
    free(state->tx_buffer);
    free(state->last_write_data);
    return result;
  }

  /* Start peripheral task */
  result = internal_start_peripheral_task(state);
  if (result != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start peripheral task");
    i2c_driver_delete(state->port);
    free(state->rx_buffer);
    free(state->tx_buffer);
    free(state->last_write_data);
    return result;
  }

  state->enabled = true;
  memset(&state->stats, 0, sizeof(star_i2c_peripheral_stats_t));

  ESP_LOGI(s_TAG,
           "I2C peripheral enabled on bus '%s' at address 0x%02x",
           bus_name,
           config->address);

  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_disable(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Stop peripheral task */
  internal_stop_peripheral_task(state);

  /* Delete I2C driver */
  i2c_driver_delete(state->port);

  /* Free buffers */
  free(state->rx_buffer);
  free(state->tx_buffer);
  free(state->last_write_data);

  state->rx_buffer       = NULL;
  state->tx_buffer       = NULL;
  state->last_write_data = NULL;
  state->enabled         = false;

  ESP_LOGI(s_TAG, "I2C peripheral disabled on bus '%s'", bus_name);

  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_is_enabled(const star_bus_manager_t* manager,
                                             const char*               bus_name,
                                             bool*                     enabled)
{
  if (manager == NULL || bus_name == NULL || enabled == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL) {
    *enabled = false;
    return ESP_OK;
  }

  *enabled = state->enabled;
  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_set_response(star_bus_manager_t* manager,
                                               const char*         bus_name,
                                               const uint8_t*      data,
                                               size_t              length)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  if (length > state->config.tx_buffer_size) {
    ESP_LOGE(s_TAG, "Response data too large: %d > %d", length, state->config.tx_buffer_size);
    return ESP_ERR_INVALID_SIZE;
  }

  /* Copy data to tx buffer */
  memcpy(state->tx_buffer, data, length);
  state->tx_data_length = length;

  /* Write to I2C slave tx buffer */
  int32_t written =
    i2c_slave_write_buffer(state->port, data, length, pdMS_TO_TICKS(state->config.timeout_ms));

  if (written < 0) {
    ESP_LOGE(s_TAG, "Failed to write to slave tx buffer");
    return ESP_FAIL;
  }

  state->stats.total_read_requests++;
  state->stats.total_bytes_sent += written;

  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_get_last_write(const star_bus_manager_t* manager,
                                                 const char*               bus_name,
                                                 uint8_t*                  data,
                                                 size_t*                   length)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  size_t copy_len = state->last_write_length < *length ? state->last_write_length : *length;

  if (copy_len > 0 && state->last_write_data != NULL) {
    memcpy(data, state->last_write_data, copy_len);
  }

  *length = copy_len;

  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_get_stats(const star_bus_manager_t*    manager,
                                            const char*                  bus_name,
                                            star_i2c_peripheral_stats_t* stats)
{
  if (manager == NULL || bus_name == NULL || stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memcpy(stats, &state->stats, sizeof(star_i2c_peripheral_stats_t));

  return ESP_OK;
}

esp_err_t star_bus_i2c_peripheral_reset_stats(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  peripheral_state_t* state = get_peripheral_state(bus_name, false);
  if (state == NULL || !state->enabled) {
    return ESP_ERR_NOT_FOUND;
  }

  memset(&state->stats, 0, sizeof(star_i2c_peripheral_stats_t));

  ESP_LOGI(s_TAG, "Statistics reset for peripheral bus '%s'", bus_name);

  return ESP_OK;
}

void star_bus_i2c_peripheral_print_stats(const char*                        bus_name,
                                         const star_i2c_peripheral_stats_t* stats)
{
  if (bus_name == NULL || stats == NULL) {
    return;
  }

  printf("\n=== I2C Peripheral Statistics: %s ===\n", bus_name);
  printf("Requests:\n");
  printf("  Read:  %llu\n", stats->total_read_requests);
  printf("  Write: %llu\n", stats->total_write_requests);
  printf("  Total: %llu\n", stats->total_read_requests + stats->total_write_requests);

  printf("\nData Transfer:\n");
  printf("  Bytes Sent:     %llu\n", stats->total_bytes_sent);
  printf("  Bytes Received: %llu\n", stats->total_bytes_received);

  printf("\nErrors:\n");
  printf("  Failed Reads:  %llu\n", stats->failed_reads);
  printf("  Failed Writes: %llu\n", stats->failed_writes);
  printf("  Callback Errors: %llu\n", stats->callback_errors);
  printf("  Last Error: %s\n", esp_err_to_name(stats->last_error));

  printf("\nTiming:\n");
  printf("  Last Request: %" PRIu32 " us\n", stats->last_request_time_us);

  printf("======================================\n\n");
}
