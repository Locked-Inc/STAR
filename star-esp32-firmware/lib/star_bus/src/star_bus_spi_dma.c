/* lib/star_bus/src/star_bus_spi_dma.c */

#include "star_bus_spi_dma.h"

#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <inttypes.h>
#include <string.h>

#include "star_bus_spi.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_SPI_DMA";

/**
 * @brief Number of SPI peripherals on ESP32 (SPI1, SPI2, SPI3)
 * Note: SPI0 is reserved for flash and not available for general use
 */
#define STAR_SPI_NUM_PERIPHERALS (3)

/* --- Types --- */

/**
 * @brief DMA transfer context
 */
typedef struct {
  star_spi_dma_callback_t callback;     /**< Completion callback */
  void*                   user_context; /**< User context */
  bool                    in_progress;  /**< Transfer in progress */
} spi_dma_transfer_context_t;

/**
 * @brief DMA state for an SPI bus
 */
typedef struct {
  star_spi_dma_config_t      config;            /**< DMA configuration */
  spi_dma_transfer_context_t transfer_context;  /**< Current transfer context */
  uint32_t                   transfer_count;    /**< Total DMA transfers */
  uint64_t                   bytes_transferred; /**< Total bytes via DMA */
} spi_dma_state_t;

/* --- Static Storage --- */

/**
 * @brief DMA state storage for SPI buses
 * @note ESP32 has SPI2_HOST (HSPI) and SPI3_HOST (VSPI)
 */
static spi_dma_state_t g_spi_dma_states[STAR_SPI_NUM_PERIPHERALS] = {0}; /* SPI1, SPI2, SPI3 */

/* --- Helper Functions --- */

/**
 * @brief Get SPI host from bus name
 */
static spi_host_device_t get_spi_host_from_bus(star_bus_manager_t* manager, const char* bus_name)
{
  star_bus_config_t* config = star_bus_manager_find_bus(manager, bus_name);
  if (config == NULL || config->type != k_star_bus_type_spi) {
    return SPI_HOST_MAX;
  }

  return config->proto.spi.host;
}

/**
 * @brief Get DMA state for an SPI host
 */
static spi_dma_state_t* get_spi_dma_state(spi_host_device_t host)
{
  if (host >= SPI_HOST_MAX) {
    return NULL;
  }
  return &g_spi_dma_states[host];
}

/**
 * @brief Check if transfer should use DMA
 */
static bool internal_should_use_spi_dma(const spi_dma_state_t* state, size_t length)
{
  if (state == NULL || !state->config.enabled) {
    return false;
  }

  if (length < state->config.min_transfer_size) {
    return false;
  }

  if (length > STAR_SPI_DMA_MAX_SIZE) {
    return false;
  }

  return true;
}

/* --- Public Functions --- */

void* star_spi_dma_malloc(size_t size)
{
  void* ptr =
    heap_caps_aligned_alloc(STAR_SPI_DMA_ALIGNMENT, size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if (ptr == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate %zu bytes of DMA memory", size);
  } else {
    ESP_LOGD(s_TAG, "Allocated %zu bytes of DMA memory at %p", size, ptr);
  }

  return ptr;
}

void star_spi_dma_free(void* ptr)
{
  if (ptr != NULL) {
    heap_caps_free(ptr);
  }
}

bool star_spi_is_dma_capable(const void* buffer, size_t size)
{
  if (buffer == NULL || size == 0) {
    return false;
  }

  /* Check alignment */
  if (((uintptr_t)buffer % STAR_SPI_DMA_ALIGNMENT) != 0) {
    return false;
  }

  /* Check if buffer is in internal RAM (DMA-capable on ESP32) */
  if (!esp_ptr_internal(buffer)) {
    return false;
  }

  /* Verify the buffer region is in DMA-capable memory */
  return esp_ptr_dma_capable(buffer);
}

esp_err_t star_bus_spi_configure_dma(star_bus_manager_t*          manager,
                                     const char*                  bus_name,
                                     const star_spi_dma_config_t* config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus(manager, bus_name);
  if (host == SPI_HOST_MAX) {
    ESP_LOGE(s_TAG, "Bus '%s' not found or not SPI", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Validate DMA channel */
  if (config->dma_channel < 1 || config->dma_channel > 2) {
    ESP_LOGE(s_TAG, "Invalid DMA channel: %d (must be 1-2)", config->dma_channel);
    return ESP_ERR_INVALID_ARG;
  }

  /* Store configuration */
  memcpy(&state->config, config, sizeof(star_spi_dma_config_t));

  ESP_LOGI(s_TAG,
           "DMA configured for bus '%s': enabled=%d, min_size=%" PRIu32 ", channel=%d",
           bus_name,
           config->enabled,
           config->min_transfer_size,
           config->dma_channel);

  return ESP_OK;
}

esp_err_t star_bus_spi_get_dma_config(const star_bus_manager_t* manager,
                                      const char*               bus_name,
                                      star_spi_dma_config_t*    config)
{
  if (manager == NULL || bus_name == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus((star_bus_manager_t*)manager, bus_name);
  if (host == SPI_HOST_MAX) {
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  memcpy(config, &state->config, sizeof(star_spi_dma_config_t));

  return ESP_OK;
}

esp_err_t star_bus_spi_transmit_dma(star_bus_manager_t*     manager,
                                    const char*             bus_name,
                                    const uint8_t*          data,
                                    size_t                  length,
                                    star_spi_dma_callback_t callback,
                                    void*                   context)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == 0) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus(manager, bus_name);
  if (host == SPI_HOST_MAX) {
    ESP_LOGE(s_TAG, "Bus '%s' not found or not SPI", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  bool use_dma = internal_should_use_spi_dma(state, length);

  if (!use_dma) {
    ESP_LOGD(s_TAG, "Using standard SPI transmit (length=%zu)", length);
    esp_err_t result = star_bus_spi_transmit(manager, bus_name, data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  if (!star_spi_is_dma_capable(data, length)) {
    ESP_LOGW(s_TAG, "Buffer not DMA-capable, falling back to standard SPI");
    esp_err_t result = star_bus_spi_transmit(manager, bus_name, data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  ESP_LOGD(s_TAG, "Using DMA for transmit: %zu bytes", length);

  state->transfer_context.callback     = callback;
  state->transfer_context.user_context = context;
  state->transfer_context.in_progress  = true;

  esp_err_t result = star_bus_spi_transmit(manager, bus_name, data, length, 0);

  if (result == ESP_OK) {
    state->transfer_count++;
    state->bytes_transferred += length;
  }

  state->transfer_context.in_progress = false;

  if (callback != NULL) {
    callback(result, context);
  }

  return result;
}

esp_err_t star_bus_spi_receive_dma(star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   uint8_t*                data,
                                   size_t                  length,
                                   star_spi_dma_callback_t callback,
                                   void*                   context)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == 0) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus(manager, bus_name);
  if (host == SPI_HOST_MAX) {
    ESP_LOGE(s_TAG, "Bus '%s' not found or not SPI", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  bool use_dma = internal_should_use_spi_dma(state, length);

  if (!use_dma) {
    ESP_LOGD(s_TAG, "Using standard SPI receive (length=%zu)", length);
    esp_err_t result = star_bus_spi_receive(manager, bus_name, data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  if (!star_spi_is_dma_capable(data, length)) {
    ESP_LOGW(s_TAG, "Buffer not DMA-capable, falling back to standard SPI");
    esp_err_t result = star_bus_spi_receive(manager, bus_name, data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  ESP_LOGD(s_TAG, "Using DMA for receive: %zu bytes", length);

  state->transfer_context.callback     = callback;
  state->transfer_context.user_context = context;
  state->transfer_context.in_progress  = true;

  esp_err_t result = star_bus_spi_receive(manager, bus_name, data, length, 0);

  if (result == ESP_OK) {
    state->transfer_count++;
    state->bytes_transferred += length;
  }

  state->transfer_context.in_progress = false;

  if (callback != NULL) {
    callback(result, context);
  }

  return result;
}

esp_err_t star_bus_spi_transceive_dma(star_bus_manager_t*     manager,
                                      const char*             bus_name,
                                      const uint8_t*          tx_data,
                                      uint8_t*                rx_data,
                                      size_t                  length,
                                      star_spi_dma_callback_t callback,
                                      void*                   context)
{
  if (manager == NULL || bus_name == NULL || tx_data == NULL || rx_data == NULL || length == 0) {
    ESP_LOGE(s_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus(manager, bus_name);
  if (host == SPI_HOST_MAX) {
    ESP_LOGE(s_TAG, "Bus '%s' not found or not SPI", bus_name);
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  bool use_dma = internal_should_use_spi_dma(state, length);

  if (!use_dma) {
    ESP_LOGD(s_TAG, "Using standard SPI transceive (length=%zu)", length);
    esp_err_t result = star_bus_spi_transfer(manager, bus_name, tx_data, rx_data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  if (!star_spi_is_dma_capable(tx_data, length) || !star_spi_is_dma_capable(rx_data, length)) {
    ESP_LOGW(s_TAG, "Buffers not DMA-capable, falling back to standard SPI");
    esp_err_t result = star_bus_spi_transfer(manager, bus_name, tx_data, rx_data, length, 0);

    if (callback != NULL) {
      callback(result, context);
    }

    return result;
  }

  ESP_LOGD(s_TAG, "Using DMA for transceive: %zu bytes", length);

  state->transfer_context.callback     = callback;
  state->transfer_context.user_context = context;
  state->transfer_context.in_progress  = true;

  esp_err_t result = star_bus_spi_transfer(manager, bus_name, tx_data, rx_data, length, 0);

  if (result == ESP_OK) {
    state->transfer_count++;
    state->bytes_transferred += length * 2; /* TX + RX */
  }

  state->transfer_context.in_progress = false;

  if (callback != NULL) {
    callback(result, context);
  }

  return result;
}

esp_err_t star_bus_spi_get_dma_stats(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     uint32_t*                 dma_transfers,
                                     uint64_t*                 dma_bytes)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  spi_host_device_t host = get_spi_host_from_bus((star_bus_manager_t*)manager, bus_name);
  if (host == SPI_HOST_MAX) {
    return ESP_ERR_NOT_FOUND;
  }

  spi_dma_state_t* state = get_spi_dma_state(host);
  if (state == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (dma_transfers != NULL) {
    *dma_transfers = state->transfer_count;
  }

  if (dma_bytes != NULL) {
    *dma_bytes = state->bytes_transferred;
  }

  return ESP_OK;
}
