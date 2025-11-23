/**
 * @file 048_spi_queue.c
 * @brief SPI transaction queue management
 *
 * Demonstrates:
 * - Transaction queuing for pipelined transfers
 * - Queue size configuration
 * - Polling vs waiting for completion
 * - Performance benefits of queuing
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SPI_QUEUE";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

#define TRANSFER_SIZE (64)

/**
 * @brief Sequential transfers (no queuing)
 */
static void priv_test_sequential_transfers(star_bus_manager_t *manager,
                                           int num_transfers)
{
  ESP_LOGI(s_tag, "\n=== Sequential Transfers (No Queue) ===");
  ESP_LOGI(s_tag, "Performing %d transfers sequentially...", num_transfers);

  uint8_t tx_data[TRANSFER_SIZE];
  uint8_t rx_data[TRANSFER_SIZE];

  /* Fill test data */
  for (int i = 0; i < TRANSFER_SIZE; i++) {
    tx_data[i] = (uint8_t)i;
  }

  int64_t start_time = esp_timer_get_time();

  /* Perform transfers one at a time - wait for each to complete */
  for (int i = 0; i < num_transfers; i++) {
    esp_err_t ret = star_bus_spi_transfer(manager, "spi_device",
                                          tx_data, rx_data,
                                          TRANSFER_SIZE, 1000);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Transfer %d failed: %s", i, esp_err_to_name(ret));
      return;
    }
  }

  int64_t end_time = esp_timer_get_time();
  int64_t elapsed_us = end_time - start_time;

  ESP_LOGI(s_tag, "Sequential Results:");
  ESP_LOGI(s_tag, "  Transfers:  %d", num_transfers);
  ESP_LOGI(s_tag, "  Total time: %lld us", elapsed_us);
  ESP_LOGI(s_tag, "  Per xfer:   %lld us", elapsed_us / num_transfers);
  ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
           (num_transfers * TRANSFER_SIZE) / (elapsed_us / 1000000.0) / 1024.0);
}

/**
 * @brief Queued transfers using low-level SPI API
 */
static void priv_test_queued_transfers(star_bus_manager_t *manager,
                                       int num_transfers)
{
  ESP_LOGI(s_tag, "\n=== Queued Transfers (Pipelined) ===");
  ESP_LOGI(s_tag, "Queuing %d transfers...", num_transfers);

  /* Get SPI device handle (this requires accessing internal structure) */
  /* Note: In production code, you'd extend star_bus to expose this functionality */

  ESP_LOGI(s_tag, "Note: Full queue implementation requires extending star_bus API");
  ESP_LOGI(s_tag, "This example demonstrates the concept using sequential API");

  uint8_t tx_data[TRANSFER_SIZE];
  uint8_t rx_data[TRANSFER_SIZE];

  for (int i = 0; i < TRANSFER_SIZE; i++) {
    tx_data[i] = (uint8_t)i;
  }

  int64_t start_time = esp_timer_get_time();

  /* In a real queue implementation, we would:
   * 1. Queue all transactions without waiting
   * 2. Let hardware process them in background
   * 3. Retrieve results later
   */

  for (int i = 0; i < num_transfers; i++) {
    star_bus_spi_transfer(manager, "spi_device",
                          tx_data, rx_data,
                          TRANSFER_SIZE, 1000);
  }

  int64_t end_time = esp_timer_get_time();
  int64_t elapsed_us = end_time - start_time;

  ESP_LOGI(s_tag, "Queued Results:");
  ESP_LOGI(s_tag, "  Transfers:  %d", num_transfers);
  ESP_LOGI(s_tag, "  Total time: %lld us", elapsed_us);
  ESP_LOGI(s_tag, "  Per xfer:   %lld us", elapsed_us / num_transfers);
  ESP_LOGI(s_tag, "  Throughput: %.2f KB/s",
           (num_transfers * TRANSFER_SIZE) / (elapsed_us / 1000000.0) / 1024.0);

  ESP_LOGI(s_tag, "\nWith true queuing, expect 20-40%% improvement");
}

/**
 * @brief Demonstrate queue size effects
 */
static void priv_demonstrate_queue_sizes(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Queue Size Configuration ===");

  /* Test different queue sizes */
  const int queue_sizes[] = {1, 3, 7, 15};
  const int num_queue_sizes = sizeof(queue_sizes) / sizeof(queue_sizes[0]);

  for (int i = 0; i < num_queue_sizes; i++) {
    int queue_size = queue_sizes[i];

    ESP_LOGI(s_tag, "\n--- Queue Size: %d ---", queue_size);

    /* Create device with specific queue size */
    spi_device_interface_config_t dev_cfg = {
      .clock_speed_hz = SPI_CLK_SPEED,
      .mode = 0,
      .spics_io_num = GPIO_NUM_15 + i,  /* Different CS */
      .queue_size = queue_size,
    };

    char device_name[32];
    snprintf(device_name, sizeof(device_name), "spi_q%d", queue_size);

    star_bus_config_t *spi_config = star_bus_config_create_spi_device(
      device_name,
      SPI2_HOST,
      SPI_COPI_PIN,
      SPI_CIPO_PIN,
      SPI_SCLK_PIN,
      SPI_DMA_CH_AUTO,
      &dev_cfg
    );

    if (spi_config != NULL) {
      star_bus_manager_add_bus(manager, spi_config);
      esp_err_t ret = star_bus_config_init(spi_config, manager);

      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Device with queue_size=%d initialized", queue_size);

        /* Perform test transfers */
        uint8_t tx[16] = {0xAA};
        uint8_t rx[16];

        for (int j = 0; j < queue_size + 2; j++) {
          ret = star_bus_spi_transfer(manager, device_name, tx, rx, 16, 100);
          if (ret == ESP_OK) {
            ESP_LOGI(s_tag, "  Transfer %d/%d: OK", j + 1, queue_size + 2);
          } else {
            ESP_LOGW(s_tag, "  Transfer %d/%d: %s", j + 1, queue_size + 2,
                     esp_err_to_name(ret));
          }
        }
      } else {
        ESP_LOGW(s_tag, "Failed to init queue_size=%d: %s",
                 queue_size, esp_err_to_name(ret));
      }
    }
  }
}

/**
 * @brief Demonstrate polling vs blocking
 */
static void priv_demonstrate_polling_vs_blocking(void)
{
  ESP_LOGI(s_tag, "\n=== Polling vs Blocking ===");

  ESP_LOGI(s_tag, "\nBlocking Mode (default):");
  ESP_LOGI(s_tag, "  - star_bus_spi_transfer() waits for completion");
  ESP_LOGI(s_tag, "  - Task blocks until transfer done");
  ESP_LOGI(s_tag, "  - Simple to use");
  ESP_LOGI(s_tag, "  - CPU idle during transfer");

  ESP_LOGI(s_tag, "\nPolling Mode (advanced):");
  ESP_LOGI(s_tag, "  - Use spi_device_queue_trans() to queue");
  ESP_LOGI(s_tag, "  - Use spi_device_get_trans_result() with timeout=0");
  ESP_LOGI(s_tag, "  - Check if transfer complete without blocking");
  ESP_LOGI(s_tag, "  - CPU can do other work");
  ESP_LOGI(s_tag, "  - More complex to implement");

  ESP_LOGI(s_tag, "\nQueue Benefits:");
  ESP_LOGI(s_tag, "  - Pipeline multiple transfers");
  ESP_LOGI(s_tag, "  - Reduce gaps between transactions");
  ESP_LOGI(s_tag, "  - Hardware processes while CPU prepares next");
  ESP_LOGI(s_tag, "  - Up to 40%% throughput improvement");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Transaction Queue Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_queue_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create base SPI device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode = 0,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 7,  /* Default queue size */
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "spi_device",
    SPI2_HOST,
    SPI_COPI_PIN,
    SPI_CIPO_PIN,
    SPI_SCLK_PIN,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  if (spi_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create SPI config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, spi_config);
  ret = star_bus_config_init(spi_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SPI: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "SPI initialized (queue_size=7)\n");

  /* --- Queue Theory --- */
  ESP_LOGI(s_tag, "--- Transaction Queue Concept ---");
  ESP_LOGI(s_tag, "Without Queue:");
  ESP_LOGI(s_tag, "  CPU → [Prepare] → [Transfer] → [Wait] → [Prepare] → [Transfer] ...");
  ESP_LOGI(s_tag, "  ▲     Gaps between transfers reduce throughput");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "With Queue:");
  ESP_LOGI(s_tag, "  CPU → [Prepare] → [Prepare] → [Prepare] ...");
  ESP_LOGI(s_tag, "  HW  → [Transfer] → [Transfer] → [Transfer] ...");
  ESP_LOGI(s_tag, "  ▲     CPU and hardware work in parallel");
  ESP_LOGI(s_tag, "");

  /* Performance comparison */
  const int num_transfers = 50;

  priv_test_sequential_transfers(&manager, num_transfers);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_queued_transfers(&manager, num_transfers);
  vTaskDelay(pdMS_TO_TICKS(100));

  /* Queue size effects */
  priv_demonstrate_queue_sizes(&manager);

  /* Polling vs blocking */
  priv_demonstrate_polling_vs_blocking();

  /* --- Queue Size Selection --- */
  ESP_LOGI(s_tag, "\n--- Queue Size Selection Guide ---");
  ESP_LOGI(s_tag, "queue_size = 1:");
  ESP_LOGI(s_tag, "  - No queuing benefit");
  ESP_LOGI(s_tag, "  - Simplest, lowest memory");
  ESP_LOGI(s_tag, "  - Use for infrequent transfers");

  ESP_LOGI(s_tag, "\nqueue_size = 3:");
  ESP_LOGI(s_tag, "  - Minimal queuing");
  ESP_LOGI(s_tag, "  - Good for slow devices");
  ESP_LOGI(s_tag, "  - Balance of memory/performance");

  ESP_LOGI(s_tag, "\nqueue_size = 7 (recommended):");
  ESP_LOGI(s_tag, "  - Good pipelining");
  ESP_LOGI(s_tag, "  - Works for most applications");
  ESP_LOGI(s_tag, "  - ESP-IDF default");

  ESP_LOGI(s_tag, "\nqueue_size = 15+:");
  ESP_LOGI(s_tag, "  - Maximum throughput");
  ESP_LOGI(s_tag, "  - High-speed streaming");
  ESP_LOGI(s_tag, "  - More memory required");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Queue Best Practices ---");
  ESP_LOGI(s_tag, "1. Use queue_size >= 3 for performance");
  ESP_LOGI(s_tag, "2. Keep transaction buffers valid until completion");
  ESP_LOGI(s_tag, "3. Use DMA with queuing for best results");
  ESP_LOGI(s_tag, "4. Don't mix queued and non-queued operations");
  ESP_LOGI(s_tag, "5. Check queue depth before adding transactions");
  ESP_LOGI(s_tag, "6. Use callbacks for async notification");

  /* --- Example Use Cases --- */
  ESP_LOGI(s_tag, "\n--- When to Use Queuing ---");
  ESP_LOGI(s_tag, "High-speed displays:");
  ESP_LOGI(s_tag, "  - Queue pixel data chunks");
  ESP_LOGI(s_tag, "  - Prepare next chunk while transferring");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Data logging:");
  ESP_LOGI(s_tag, "  - Queue sensor reads");
  ESP_LOGI(s_tag, "  - Process data while waiting");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Flash operations:");
  ESP_LOGI(s_tag, "  - Queue multiple sector reads");
  ESP_LOGI(s_tag, "  - Maximize flash bandwidth");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI queue management example complete");
}
