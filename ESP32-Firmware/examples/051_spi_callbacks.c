/**
 * @file 051_spi_callbacks.c
 * @brief SPI transfer callbacks and completion notifications
 *
 * Demonstrates:
 * - Pre-transfer callbacks
 * - Post-transfer callbacks
 * - Async completion notification
 * - Callback timing and overhead
 * - Practical callback use cases
 */

#include "star_bus_async.h"
#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "SPI_CALLBACKS";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (1000000)  /* 1 MHz */

/* Callback statistics */
static volatile uint32_t s_pre_cb_count  = 0;
static volatile uint32_t s_post_cb_count = 0;
static volatile uint32_t s_async_cb_count    = 0;
static volatile int64_t  s_last_callback_time = 0;

/**
 * @brief Pre-transfer callback
 * Called before CS is asserted and data transfer begins
 */
static void priv_spi_pre_transfer_callback(spi_transaction_t *trans)
{
  s_pre_cb_count++;
  s_last_callback_time = esp_timer_get_time();

  ESP_LOGI(s_tag, "[PRE-CB] Transfer starting (count=%lu)", s_pre_cb_count);

  /* Common use cases:
   * - Assert additional control pins (DC, RESET)
   * - Start timing measurements
   * - Set device-specific state
   * - Log transfer details
   */

  if (trans != NULL) {
    ESP_LOGI(s_tag, "[PRE-CB] Length: %zu bytes", trans->length / 8);
  }
}

/**
 * @brief Post-transfer callback
 * Called after CS is deasserted and transfer completes
 */
static void priv_spi_post_transfer_callback(spi_transaction_t *trans)
{
  s_post_cb_count++;
  int64_t now     = esp_timer_get_time();
  int64_t elapsed = now - s_last_callback_time;

  ESP_LOGI(s_tag, "[POST-CB] Transfer complete (count=%lu, elapsed=%lld us)",
           s_post_cb_count, elapsed);

  /* Common use cases:
   * - Deassert control pins
   * - Process received data
   * - Update transfer statistics
   * - Trigger next operation
   * - Signal completion to other tasks
   */

  if (trans != NULL) {
    ESP_LOGI(s_tag, "[POST-CB] Flags: 0x%lx", trans->flags);
  }
}

/**
 * @brief Async operation completion callback
 */
static void priv_async_complete_callback(star_async_handle_t handle,
                                         star_async_status_t status,
                                         esp_err_t result,
                                         void *context)
{
  s_async_cb_count++;
  const char *operation = (const char *)context;

  ESP_LOGI(s_tag, "[ASYNC-CB] Operation '%s' completed: %s (count=%lu)",
           operation, esp_err_to_name(result), s_async_cb_count);

  /* Common use cases:
   * - Process transfer results
   * - Trigger next async operation
   * - Update application state
   * - Signal semaphore/event group
   * - Free resources
   */
}

/**
 * @brief Demonstrate pre/post callbacks
 */
static void priv_demonstrate_sync_callbacks(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Synchronous Callbacks (Pre/Post) ===\n");

  uint8_t tx_data[8] = {0x9F, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04};
  uint8_t rx_data[8] = {0};

  ESP_LOGI(s_tag, "Performing transfer with pre/post callbacks...");

  esp_err_t ret = star_bus_spi_transfer(manager, "callback_device",
                                        tx_data, rx_data,
                                        sizeof(tx_data), 1000);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Transfer successful");
    ESP_LOGI(s_tag, "Pre-callbacks:  %lu", s_pre_cb_count);
    ESP_LOGI(s_tag, "Post-callbacks: %lu", s_post_cb_count);
  } else {
    ESP_LOGE(s_tag, "Transfer failed: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief Demonstrate async callbacks
 */
static void priv_demonstrate_async_callbacks(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Asynchronous Callbacks ===\n");

  uint8_t tx_data[16];
  uint8_t rx_data[16];

  /* Initialize test data */
  for (int i = 0; i < 16; i++) {
    tx_data[i] = (uint8_t)i;
  }

  /* Test 1: Async transmit with callback */
  ESP_LOGI(s_tag, "--- Async Transmit ---");

  star_async_handle_t tx_handle;
  star_async_config_t tx_config = {
    .timeout_ms = 1000,
    .callback   = priv_async_complete_callback,
    .context = "transmit",
    .priority = 0
  };

  esp_err_t ret = star_bus_spi_transmit_async(
    manager,
    "callback_device",
    tx_data,
    sizeof(tx_data),
    &tx_config,
    &tx_handle
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async transmit queued, waiting...");
    ret = star_async_wait(tx_handle, 1000);
    ESP_LOGI(s_tag, "Wait result: %s", esp_err_to_name(ret));
    star_async_free_handle(tx_handle);
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  /* Test 2: Async receive with callback */
  ESP_LOGI(s_tag, "\n--- Async Receive ---");

  star_async_handle_t rx_handle;
  star_async_config_t rx_config = {
    .timeout_ms = 1000,
    .callback   = priv_async_complete_callback,
    .context = "receive",
    .priority = 0
  };

  ret = star_bus_spi_receive_async(
    manager,
    "callback_device",
    rx_data,
    sizeof(rx_data),
    &rx_config,
    &rx_handle
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async receive queued, waiting...");
    ret = star_async_wait(rx_handle, 1000);
    ESP_LOGI(s_tag, "Wait result: %s", esp_err_to_name(ret));
    star_async_free_handle(rx_handle);
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  /* Test 3: Async transceive with callback */
  ESP_LOGI(s_tag, "\n--- Async Transceive ---");

  star_async_handle_t xfer_handle;
  star_async_config_t xfer_config = {
    .timeout_ms = 1000,
    .callback   = priv_async_complete_callback,
    .context = "transceive",
    .priority = 0
  };

  ret = star_bus_spi_transceive_async(
    manager,
    "callback_device",
    tx_data,
    rx_data,
    sizeof(tx_data),
    &xfer_config,
    &xfer_handle
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async transceive queued, polling status...");

    /* Demonstrate status polling */
    star_async_status_t status;
    int poll_count = 0;

    do {
      status = star_async_get_status(xfer_handle);
      poll_count++;
      vTaskDelay(pdMS_TO_TICKS(1));
    } while (status == STAR_ASYNC_STATUS_PENDING);

    ESP_LOGI(s_tag, "Status polling completed after %d checks", poll_count);
    ESP_LOGI(s_tag, "Final status: %d", status);

    star_async_free_handle(xfer_handle);
  }

  ESP_LOGI(s_tag, "\nTotal async callbacks: %lu", s_async_cb_count);
}

/**
 * @brief Demonstrate callback timing overhead
 */
static void priv_measure_callback_overhead(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Callback Timing Overhead ===\n");

  const int num_transfers = 20;
  uint8_t tx_data[32];
  uint8_t rx_data[32];

  for (int i = 0; i < 32; i++) {
    tx_data[i] = (uint8_t)i;
  }

  /* Reset counters */
  uint32_t pre_start  = s_pre_cb_count;
  uint32_t post_start = s_post_cb_count;

  int64_t start_time = esp_timer_get_time();

  for (int i = 0; i < num_transfers; i++) {
    star_bus_spi_transfer(manager, "callback_device",
                          tx_data, rx_data, sizeof(tx_data), 1000);
  }

  int64_t end_time   = esp_timer_get_time();
  int64_t total_time = end_time - start_time;

  uint32_t pre_total  = s_pre_cb_count - pre_start;
  uint32_t post_total = s_post_cb_count - post_start;

  ESP_LOGI(s_tag, "Timing Results:");
  ESP_LOGI(s_tag, "  Transfers:      %d", num_transfers);
  ESP_LOGI(s_tag, "  Total time:     %lld us", total_time);
  ESP_LOGI(s_tag, "  Per transfer:   %lld us", total_time / num_transfers);
  ESP_LOGI(s_tag, "  Pre-callbacks:  %lu", pre_total);
  ESP_LOGI(s_tag, "  Post-callbacks: %lu", post_total);
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Note: Callback overhead is typically < 1 us per callback");
}

/**
 * @brief Demonstrate practical callback use cases
 */
static void priv_demonstrate_use_cases(void)
{
  ESP_LOGI(s_tag, "\n=== Practical Callback Use Cases ===\n");

  ESP_LOGI(s_tag, "1. Display D/C Pin Control:");
  ESP_LOGI(s_tag, "   pre_cb:  Set D/C=0 for command, D/C=1 for data");
  ESP_LOGI(s_tag, "   post_cb: Restore D/C pin state");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. GPIO Timing Measurement:");
  ESP_LOGI(s_tag, "   pre_cb:  Set GPIO high");
  ESP_LOGI(s_tag, "   post_cb: Set GPIO low (measure with oscilloscope)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Multi-Device Synchronization:");
  ESP_LOGI(s_tag, "   pre_cb:  Acquire mutex/semaphore");
  ESP_LOGI(s_tag, "   post_cb: Release mutex/semaphore");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Transfer Statistics:");
  ESP_LOGI(s_tag, "   pre_cb:  Start timestamp, increment counter");
  ESP_LOGI(s_tag, "   post_cb: Calculate duration, update stats");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. Data Processing Pipeline:");
  ESP_LOGI(s_tag, "   async_cb: Process received data");
  ESP_LOGI(s_tag, "   async_cb: Queue next transfer");
  ESP_LOGI(s_tag, "   async_cb: Signal completion to application");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "6. Error Recovery:");
  ESP_LOGI(s_tag, "   post_cb: Check transfer result");
  ESP_LOGI(s_tag, "   post_cb: Retry on failure");
  ESP_LOGI(s_tag, "   async_cb: Log errors, update error counters");
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Callbacks Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_callback_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create SPI device with callbacks */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
    .pre_cb         = priv_spi_pre_transfer_callback,  /* Pre-transfer callback */
    .post_cb        = priv_spi_post_transfer_callback, /* Post-transfer callback */
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "callback_device",
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

  ESP_LOGI(s_tag, "SPI initialized with callbacks\n");

  /* Demonstrate different callback types */
  priv_demonstrate_sync_callbacks(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_async_callbacks(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_measure_callback_overhead(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_use_cases();

  /* Final statistics */
  ESP_LOGI(s_tag, "\n=== Final Statistics ===");
  ESP_LOGI(s_tag, "Pre-transfer callbacks:  %lu", s_pre_cb_count);
  ESP_LOGI(s_tag, "Post-transfer callbacks: %lu", s_post_cb_count);
  ESP_LOGI(s_tag, "Async callbacks:         %lu", s_async_cb_count);

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Callback Best Practices ===");
  ESP_LOGI(s_tag, "1. Keep callbacks SHORT - they run in ISR context");
  ESP_LOGI(s_tag, "2. Avoid blocking operations (no delays, prints, etc)");
  ESP_LOGI(s_tag, "3. Use volatile for shared variables");
  ESP_LOGI(s_tag, "4. Prefer async callbacks for complex processing");
  ESP_LOGI(s_tag, "5. Don't call SPI functions from callbacks");
  ESP_LOGI(s_tag, "6. Use callbacks for timing-critical operations only");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI callbacks example complete");
}
