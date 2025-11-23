/**
 * @file 038_i2c_buffer_mgmt.c
 * @brief Buffer allocation and management strategies
 *
 * This example demonstrates:
 * - Different buffer allocation strategies
 * - Static vs dynamic buffers
 * - Buffer pools for efficiency
 * - DMA buffer requirements
 * - Memory-efficient large transfers
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "I2C_BUFFER";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x50)  /* EEPROM */

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/* Buffer sizes */
#define SMALL_BUFFER    (16)
#define MEDIUM_BUFFER   (256)
#define LARGE_BUFFER    (1024)

/**
 * @brief Buffer pool entry
 */
typedef struct {
  uint8_t *data;
  size_t   size;
  bool     in_use;
} buffer_pool_entry_t;

/**
 * @brief Simple buffer pool
 */
#define POOL_SIZE (4)
static buffer_pool_entry_t s_buffer_pool[POOL_SIZE];

void demonstrate_static_buffers(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Strategy 1: Static Buffers ===");
  ESP_LOGI(s_tag, "Pros: Fast, deterministic, no fragmentation");
  ESP_LOGI(s_tag, "Cons: Fixed size, wastes RAM if oversized");

  /* Static buffers allocated at compile time */
  static uint8_t static_buffer[MEDIUM_BUFFER];

  ESP_LOGI(s_tag, "\nReading %d bytes into static buffer...", SMALL_BUFFER);

  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, "device", static_buffer,
                                    SMALL_BUFFER, 0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read %u bytes successfully", (unsigned)bytes_read);
    ESP_LOGI(s_tag, "Buffer address: %p (likely in .bss section)",
             static_buffer);
    ESP_LOGI(s_tag, "Memory type: Static (compile-time allocation)");
  } else {
    ESP_LOGW(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }

  ESP_LOGI(s_tag, "\nStatic buffer best for:");
  ESP_LOGI(s_tag, "  - Fixed-size transactions");
  ESP_LOGI(s_tag, "  - Performance-critical code");
  ESP_LOGI(s_tag, "  - Simple embedded systems");
}

void demonstrate_dynamic_buffers(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Strategy 2: Dynamic Buffers ===");
  ESP_LOGI(s_tag, "Pros: Flexible size, memory efficient");
  ESP_LOGI(s_tag, "Cons: malloc overhead, fragmentation risk");

  /* Allocate buffer at runtime */
  size_t   buffer_size    = MEDIUM_BUFFER;
  uint8_t *dynamic_buffer = (uint8_t *)malloc(buffer_size);

  if (dynamic_buffer == NULL) {
    ESP_LOGE(s_tag, "Failed to allocate %u bytes", buffer_size);
    return;
  }

  ESP_LOGI(s_tag, "\nAllocated %u bytes dynamically", buffer_size);
  ESP_LOGI(s_tag, "Buffer address: %p (heap allocation)", dynamic_buffer);

  /* Use the buffer */
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, "device", dynamic_buffer,
                                    buffer_size, 0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read %u bytes successfully", (unsigned)bytes_read);
  }

  /* Always free when done */
  free(dynamic_buffer);
  ESP_LOGI(s_tag, "Buffer freed");

  ESP_LOGI(s_tag, "\nDynamic buffer best for:");
  ESP_LOGI(s_tag, "  - Variable-size data");
  ESP_LOGI(s_tag, "  - Temporary operations");
  ESP_LOGI(s_tag, "  - Large but infrequent transfers");
}

void demonstrate_dma_buffers(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Strategy 3: DMA Buffers ===");
  ESP_LOGI(s_tag, "Pros: Hardware acceleration, CPU-free transfers");
  ESP_LOGI(s_tag, "Cons: Must be in specific memory regions");

  /* Allocate DMA-capable buffer */
  size_t   buffer_size = LARGE_BUFFER;
  uint8_t *dma_buffer  = (uint8_t *)heap_caps_malloc(buffer_size,
                                                      MALLOC_CAP_DMA);

  if (dma_buffer == NULL) {
    ESP_LOGE(s_tag, "Failed to allocate DMA buffer");
    return;
  }

  ESP_LOGI(s_tag, "\nAllocated %u bytes of DMA-capable memory", buffer_size);
  ESP_LOGI(s_tag, "Buffer address: %p (DMA region)", dma_buffer);

  /* Check if address is DMA-capable */
  if (heap_caps_get_allocated_size(dma_buffer) > 0) {
    ESP_LOGI(s_tag, "Buffer confirmed in DMA-capable region");
  }

  /* Use for I2C transfer */
  size_t    bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, "device", dma_buffer,
                                    buffer_size, 0x00, &bytes_read);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "DMA transfer completed: %u bytes", (unsigned)bytes_read);
  }

  /* Free DMA buffer */
  heap_caps_free(dma_buffer);
  ESP_LOGI(s_tag, "DMA buffer freed");

  ESP_LOGI(s_tag, "\nDMA buffer best for:");
  ESP_LOGI(s_tag, "  - Large transfers (>1KB)");
  ESP_LOGI(s_tag, "  - Concurrent CPU operations");
  ESP_LOGI(s_tag, "  - High-performance applications");
}

static void priv_init_buffer_pool(void)
{
  ESP_LOGI(s_tag, "\n=== Initializing Buffer Pool ===");

  for (int i = 0; i < POOL_SIZE; i++) {
    s_buffer_pool[i].size   = MEDIUM_BUFFER;
    s_buffer_pool[i].data   = (uint8_t *)malloc(MEDIUM_BUFFER);
    s_buffer_pool[i].in_use = false;

    if (s_buffer_pool[i].data) {
      ESP_LOGI(s_tag, "Pool entry %d: %u bytes at %p",
               i, MEDIUM_BUFFER, s_buffer_pool[i].data);
    } else {
      ESP_LOGE(s_tag, "Failed to allocate pool entry %d", i);
    }
  }

  ESP_LOGI(s_tag, "Buffer pool initialized with %d buffers", POOL_SIZE);
}

static uint8_t *priv_acquire_buffer_from_pool(size_t required_size)
{
  for (int i = 0; i < POOL_SIZE; i++) {
    if (!s_buffer_pool[i].in_use &&
        s_buffer_pool[i].size >= required_size) {
      s_buffer_pool[i].in_use = true;
      ESP_LOGI(s_tag, "Acquired buffer %d (%u bytes)",
               i, s_buffer_pool[i].size);
      return s_buffer_pool[i].data;
    }
  }

  ESP_LOGW(s_tag, "No available buffer in pool");
  return NULL;
}

static void priv_release_buffer_to_pool(uint8_t *buffer)
{
  for (int i = 0; i < POOL_SIZE; i++) {
    if (s_buffer_pool[i].data == buffer) {
      s_buffer_pool[i].in_use = false;
      ESP_LOGI(s_tag, "Released buffer %d back to pool", i);
      return;
    }
  }

  ESP_LOGW(s_tag, "Buffer %p not found in pool", buffer);
}

static void priv_cleanup_buffer_pool(void)
{
  ESP_LOGI(s_tag, "\nCleaning up buffer pool...");

  for (int i = 0; i < POOL_SIZE; i++) {
    if (s_buffer_pool[i].data) {
      free(s_buffer_pool[i].data);
      s_buffer_pool[i].data = NULL;
    }
  }

  ESP_LOGI(s_tag, "Buffer pool cleaned up");
}

void demonstrate_buffer_pool(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Strategy 4: Buffer Pool ===");
  ESP_LOGI(s_tag, "Pros: Fast allocation, no fragmentation, reusable");
  ESP_LOGI(s_tag, "Cons: Upfront memory cost, may waste space");

  priv_init_buffer_pool();

  /* Acquire buffer from pool */
  uint8_t *buffer1 = priv_acquire_buffer_from_pool(SMALL_BUFFER);
  if (buffer1) {
    size_t bytes_read;
    star_bus_i2c_read(manager, "device", buffer1, SMALL_BUFFER,
                      0x00, &bytes_read);
    priv_release_buffer_to_pool(buffer1);
  }

  /* Acquire multiple buffers */
  ESP_LOGI(s_tag, "\nTesting concurrent buffer acquisition...");
  uint8_t *buffer2 = priv_acquire_buffer_from_pool(SMALL_BUFFER);
  uint8_t *buffer3 = priv_acquire_buffer_from_pool(SMALL_BUFFER);
  uint8_t *buffer4 = priv_acquire_buffer_from_pool(SMALL_BUFFER);

  ESP_LOGI(s_tag, "Acquired 3 buffers simultaneously");

  priv_release_buffer_to_pool(buffer2);
  priv_release_buffer_to_pool(buffer3);
  priv_release_buffer_to_pool(buffer4);

  priv_cleanup_buffer_pool();

  ESP_LOGI(s_tag, "\nBuffer pool best for:");
  ESP_LOGI(s_tag, "  - Frequent allocations");
  ESP_LOGI(s_tag, "  - Known max concurrent buffers");
  ESP_LOGI(s_tag, "  - Real-time systems");
}

void demonstrate_streaming_large_data(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Strategy 5: Streaming (Chunked Transfers) ===");
  ESP_LOGI(s_tag, "Pros: Minimal memory, handles unlimited data");
  ESP_LOGI(s_tag, "Cons: Multiple transactions, slower");

  const size_t chunk_size = 64;
  const size_t total_size = 512;
  uint8_t      chunk_buffer[64];

  ESP_LOGI(s_tag, "\nReading %u bytes in %u-byte chunks...",
           total_size, chunk_size);

  size_t   offset   = 0;
  uint32_t checksum = 0;

  while (offset < total_size) {
    size_t to_read = (total_size - offset < chunk_size)
                       ? (total_size - offset) : chunk_size;
    size_t bytes_read;

    esp_err_t ret = star_bus_i2c_read(manager, "device", chunk_buffer, to_read,
                                       (uint8_t)offset, &bytes_read);

    if (ret == ESP_OK) {
      /* Process chunk (e.g., calculate checksum) */
      for (size_t i = 0; i < bytes_read; i++) {
        checksum += chunk_buffer[i];
      }

      offset += bytes_read;
      ESP_LOGI(s_tag, "  Chunk at offset %u: %u bytes (total: %u/%u)",
               offset - bytes_read, (unsigned)bytes_read, offset, total_size);
    } else {
      ESP_LOGE(s_tag, "Failed to read chunk at offset %u", offset);
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }

  ESP_LOGI(s_tag, "Streaming complete. Checksum: 0x%08X", checksum);

  ESP_LOGI(s_tag, "\nStreaming best for:");
  ESP_LOGI(s_tag, "  - Very large data (>RAM size)");
  ESP_LOGI(s_tag, "  - Memory-constrained systems");
  ESP_LOGI(s_tag, "  - Processing data on-the-fly");
}

void print_memory_info(void)
{
  ESP_LOGI(s_tag, "\n=== Memory Information ===");

  size_t free_heap     = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t dma_free      = heap_caps_get_free_size(MALLOC_CAP_DMA);

  ESP_LOGI(s_tag, "Free heap: %u bytes", free_heap);
  ESP_LOGI(s_tag, "Largest block: %u bytes", largest_block);
  ESP_LOGI(s_tag, "DMA-capable free: %u bytes", dma_free);
}

void i2c_buffer_mgmt_example(void)
{
  esp_err_t ret;

  /* Print initial memory state */
  print_memory_info();

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Buffer", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config */
  star_bus_config_t *i2c_config = star_bus_config_create_i2c(
    "device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");

  /* Demonstrate different buffer strategies */
  demonstrate_static_buffers(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  demonstrate_dynamic_buffers(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  demonstrate_dma_buffers(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  demonstrate_buffer_pool(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  demonstrate_streaming_large_data(&manager);

  /* Print final memory state */
  print_memory_info();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Buffer Management Summary ===");
  ESP_LOGI(s_tag, "Choose strategy based on:");
  ESP_LOGI(s_tag, "  1. Data size predictability");
  ESP_LOGI(s_tag, "  2. Memory constraints");
  ESP_LOGI(s_tag, "  3. Performance requirements");
  ESP_LOGI(s_tag, "  4. Frequency of operations");
  ESP_LOGI(s_tag, "  5. DMA requirements");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nBuffer management example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Buffer Management Example ===\n");
  i2c_buffer_mgmt_example();
}
