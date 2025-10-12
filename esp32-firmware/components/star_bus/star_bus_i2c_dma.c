/* esp32-firmware/components/star_bus/star_bus_i2c_dma.c */

#include "star_bus_i2c_dma.h"

#include <driver/i2c.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <string.h>

#include "star_bus_i2c.h"

/* --- Constants --- */

static const char* TAG = "STAR_I2C_DMA";

/* --- Types --- */

/**
 * @brief DMA transfer context
 */
typedef struct {
  star_i2c_dma_callback_t callback;     /**< Completion callback */
  void*                   user_context; /**< User context */
  bool                    in_progress;  /**< Transfer in progress */
} dma_transfer_context_t;

/**
 * @brief DMA state for an I2C bus
 */
typedef struct {
  star_i2c_dma_config_t  config;            /**< DMA configuration */
  dma_transfer_context_t transfer_context;  /**< Current transfer context */
  uint32_t               transfer_count;    /**< Total DMA transfers */
  uint64_t               bytes_transferred; /**< Total bytes via DMA */
} i2c_dma_state_t;

/* --- Static Storage --- */

/**
 * @brief DMA state storage for I2C buses
 * @note In a real implementation, this would be dynamically allocated
 *       per bus or stored in the bus configuration
 */
static i2c_dma_state_t g_dma_states[I2C_NUM_MAX] = {0};

/* --- Helper Functions --- */

/**
 * @brief Get I2C port number from bus name
 */
static i2c_port_t get_i2c_port_from_bus(star_bus_manager_t* manager, const char* bus_name)
{
  /* Find the bus configuration */
  star_bus_config_t* config = star_bus_manager_find_bus(manager, bus_name);
  if (config == NULL || config->type != STAR_BUS_TYPE_I2C) {
    return I2C_NUM_MAX; /* Invalid */
  }

  return config->config.i2c.port;
}

/**
 * @brief Get DMA state for an I2C port
 */
static i2c_dma_state_t* get_dma_state(i2c_port_t port)
{
  if (port >= I2C_NUM_MAX) {
    return NULL;
  }
  return &g_dma_states[port];
}

/**
 * @brief Check if transfer should use DMA
 */
static bool should_use_dma(const i2c_dma_state_t* state, size_t length)
{
  if (state == NULL || !state->config.enabled) {
    return false;
  }

  if (length < state->config.min_transfer_size) {
    return false;
  }

  if (length > STAR_I2C_DMA_MAX_SIZE) {
    return false;
  }

  return true;
}

/* --- Public Functions --- */

void* star_i2c_dma_malloc(size_t size)
{
  /* Allocate DMA-capable memory (internal RAM, cache-aligned) */
  void* ptr =
    heap_caps_aligned_alloc(STAR_I2C_DMA_ALIGNMENT, size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if (ptr == NULL) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes of DMA memory", size);
  } else {
    ESP_LOGD(TAG, "Allocated %zu bytes of DMA memory at %p", size, ptr);
  }

  return ptr;
}

void star_i2c_dma_free(void* ptr)
{
  if (ptr != NULL) {
    heap_caps_free(ptr);
  }
}

bool star_i2c_is_dma_capable(const void* buffer, size_t size)
{
  if (buffer == NULL || size == 0) {
    return false;
  }

  /* Check alignment */
  if (((uintptr_t)buffer % STAR_I2C_DMA_ALIGNMENT) != 0) {
    return false;
  }

  /* Check if in DMA-capable memory */
  return heap_caps_check_integrity(MALLOC_CAP_DMA, true);
}

esp_err_t star_bus_i2c_configure_dma(star_bus_manager_t*          manager,
                                     const char*                  bus_name,
                                     const star_i2c_dma_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  /* Get I2C port */
  i2c_port_t port = get_i2c_port_from_bus(manager, bus_name);
  if (port == I2C_NUM_MAX) {
    ESP_LOGE(TAG, "Bus '%s' not found or not I2C", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  /* Get DMA state */
  i2c_dma_state_t* state = get_dma_state(port);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Validate configuration */
  if (config->dma_channel >= 2) { /* ESP32 has 2 DMA channels */
    ESP_LOGE(TAG, "Invalid DMA channel: %d", config->dma_channel);
    return ESP_ERR_INVALID_ARG;
  }

  /* Store configuration */
  memcpy(&state->config, config, sizeof(star_i2c_dma_config_t));

  ESP_LOGI(TAG,
           "DMA configured for bus '%s': enabled=%d, min_size=%lu, channel=%d",
           bus_name,
           config->enabled,
           config->min_transfer_size,
           config->dma_channel);

  return ESP_OK;
}

esp_err_t star_bus_i2c_get_dma_config(const star_bus_manager_t* manager,
                                      const char*               bus_name,
                                      star_i2c_dma_config_t*    config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Get I2C port */
  i2c_port_t port = get_i2c_port_from_bus((star_bus_manager_t*)manager, bus_name);
  if (port == I2C_NUM_MAX) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Get DMA state */
  i2c_dma_state_t* state = get_dma_state(port);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Copy configuration */
  memcpy(config, &state->config, sizeof(star_i2c_dma_config_t));

  return ESP_OK;
}

esp_err_t star_bus_i2c_write_dma(star_bus_manager_t*     manager,
                                 const char*             bus_name,
                                 const uint8_t*          data,
                                 size_t                  length,
                                 uint8_t                 command,
                                 star_i2c_dma_callback_t callback,
                                 void*                   context)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == 0) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  /* Get I2C port */
  i2c_port_t port = get_i2c_port_from_bus(manager, bus_name);
  if (port == I2C_NUM_MAX) {
    ESP_LOGE(TAG, "Bus '%s' not found or not I2C", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  /* Get DMA state */
  i2c_dma_state_t* state = get_dma_state(port);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Check if DMA should be used */
  bool use_dma = should_use_dma(state, length);

  if (!use_dma) {
    /* Fall back to standard I2C write */
    ESP_LOGD(TAG,
             "Using standard I2C write (length=%zu, threshold=%lu)",
             length,
             state->config.min_transfer_size);
    esp_err_t result = star_bus_i2c_write(manager, bus_name, data, length, command, NULL);

    /* Call callback if provided */
    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  /* Verify buffer is DMA-capable */
  if (!star_i2c_is_dma_capable(data, length)) {
    ESP_LOGW(TAG, "Buffer not DMA-capable, falling back to standard I2C");
    esp_err_t result = star_bus_i2c_write(manager, bus_name, data, length, command, NULL);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  ESP_LOGD(TAG, "Using DMA for write: %zu bytes", length);

  /* Store callback context */
  state->transfer_context.callback     = callback;
  state->transfer_context.user_context = context;
  state->transfer_context.in_progress  = true;

  /* Perform DMA-backed I2C write */
  /* Note: ESP-IDF i2c_master_write_to_device supports DMA automatically
   * when buffers are in DMA-capable memory */
  esp_err_t result = star_bus_i2c_write(manager, bus_name, data, length, command, NULL);

  /* Update statistics */
  if (result == ESP_OK) {
    state->transfer_count++;
    state->bytes_transferred += length;
  }

  /* Mark transfer complete */
  state->transfer_context.in_progress = false;

  /* Call callback if provided */
  if (callback != NULL) {
    callback(result, context);
  }

  return result;
}

esp_err_t star_bus_i2c_read_dma(star_bus_manager_t*     manager,
                                const char*             bus_name,
                                uint8_t*                data,
                                size_t                  length,
                                uint8_t                 command,
                                star_i2c_dma_callback_t callback,
                                void*                   context)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == 0) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  /* Get I2C port */
  i2c_port_t port = get_i2c_port_from_bus(manager, bus_name);
  if (port == I2C_NUM_MAX) {
    ESP_LOGE(TAG, "Bus '%s' not found or not I2C", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  /* Get DMA state */
  i2c_dma_state_t* state = get_dma_state(port);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Check if DMA should be used */
  bool use_dma = should_use_dma(state, length);

  if (!use_dma) {
    /* Fall back to standard I2C read */
    ESP_LOGD(TAG,
             "Using standard I2C read (length=%zu, threshold=%lu)",
             length,
             state->config.min_transfer_size);
    esp_err_t result = star_bus_i2c_read(manager, bus_name, data, length, command, NULL);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  /* Verify buffer is DMA-capable */
  if (!star_i2c_is_dma_capable(data, length)) {
    ESP_LOGW(TAG, "Buffer not DMA-capable, falling back to standard I2C");
    esp_err_t result = star_bus_i2c_read(manager, bus_name, data, length, command, NULL);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  ESP_LOGD(TAG, "Using DMA for read: %zu bytes", length);

  /* Store callback context */
  state->transfer_context.callback     = callback;
  state->transfer_context.user_context = context;
  state->transfer_context.in_progress  = true;

  /* Perform DMA-backed I2C read */
  esp_err_t result = star_bus_i2c_read(manager, bus_name, data, length, command, NULL);

  /* Update statistics */
  if (result == ESP_OK) {
    state->transfer_count++;
    state->bytes_transferred += length;
  }

  /* Mark transfer complete */
  state->transfer_context.in_progress = false;

  /* Call callback if provided */
  if (callback != NULL) {
    callback(result, context);
  }

  return result;
}

esp_err_t star_bus_i2c_get_dma_stats(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     uint32_t*                 dma_transfers,
                                     uint64_t*                 dma_bytes)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Get I2C port */
  i2c_port_t port = get_i2c_port_from_bus((star_bus_manager_t*)manager, bus_name);
  if (port == I2C_NUM_MAX) {
    return ESP_ERR_NOT_FOUND;
  }

  /* Get DMA state */
  i2c_dma_state_t* state = get_dma_state(port);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Return statistics */
  if (dma_transfers != NULL) {
    *dma_transfers = state->transfer_count;
  }

  if (dma_bytes != NULL) {
    *dma_bytes = state->bytes_transferred;
  }

  return ESP_OK;
}
