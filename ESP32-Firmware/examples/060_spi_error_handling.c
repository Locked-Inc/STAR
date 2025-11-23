/**
 * @file 060_spi_error_handling.c
 * @brief Comprehensive SPI error handling patterns
 *
 * Demonstrates:
 * - Common SPI error conditions
 * - Error detection and recovery
 * - Retry strategies
 * - Timeout handling
 * - Graceful degradation
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_spi.h"

#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *s_tag = "SPI_ERROR_HANDLING";

#define SPI_COPI_PIN   (GPIO_NUM_23)
#define SPI_CIPO_PIN   (GPIO_NUM_19)
#define SPI_SCLK_PIN   (GPIO_NUM_18)
#define SPI_CS_PIN     (GPIO_NUM_5)
#define SPI_CLK_SPEED  (10000000)  /* 10 MHz */

/* Error counters */
typedef struct {
  uint32_t timeout_errors;
  uint32_t invalid_arg_errors;
  uint32_t no_mem_errors;
  uint32_t not_found_errors;
  uint32_t other_errors;
  uint32_t total_transfers;
  uint32_t successful_transfers;
  uint32_t retries_performed;
} error_stats_t;

static error_stats_t s_stats = {0};

/**
 * @brief Common error codes
 */
static void priv_explain_error_codes(void)
{
  ESP_LOGI(s_tag, "\n=== Common SPI Error Codes ===\n");

  ESP_LOGI(s_tag, "ESP_OK (0):");
  ESP_LOGI(s_tag, "  - Transfer successful");
  ESP_LOGI(s_tag, "  - No errors occurred");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP_ERR_INVALID_ARG:");
  ESP_LOGI(s_tag, "  - NULL pointer passed");
  ESP_LOGI(s_tag, "  - Invalid buffer address");
  ESP_LOGI(s_tag, "  - Transfer size = 0");
  ESP_LOGI(s_tag, "  - Buffer not DMA-capable");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP_ERR_TIMEOUT:");
  ESP_LOGI(s_tag, "  - Transfer didn't complete in time");
  ESP_LOGI(s_tag, "  - Device not responding");
  ESP_LOGI(s_tag, "  - Bus stuck/hung");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP_ERR_NO_MEM:");
  ESP_LOGI(s_tag, "  - Buffer allocation failed");
  ESP_LOGI(s_tag, "  - Out of DMA memory");
  ESP_LOGI(s_tag, "  - Queue full");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP_ERR_NOT_FOUND:");
  ESP_LOGI(s_tag, "  - Device name not registered");
  ESP_LOGI(s_tag, "  - Invalid device handle");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "ESP_ERR_INVALID_STATE:");
  ESP_LOGI(s_tag, "  - Bus not initialized");
  ESP_LOGI(s_tag, "  - Device already in use");
  ESP_LOGI(s_tag, "  - Conflicting operation");
}

/**
 * @brief Handle error code and update statistics
 */
static void priv_handle_error(esp_err_t err)
{
  switch (err) {
    case ESP_OK:
      s_stats.successful_transfers++;
      break;

    case ESP_ERR_TIMEOUT:
      s_stats.timeout_errors++;
      ESP_LOGW(s_tag, "Timeout error detected");
      break;

    case ESP_ERR_INVALID_ARG:
      s_stats.invalid_arg_errors++;
      ESP_LOGE(s_tag, "Invalid argument error");
      break;

    case ESP_ERR_NO_MEM:
      s_stats.no_mem_errors++;
      ESP_LOGE(s_tag, "Out of memory error");
      break;

    case ESP_ERR_NOT_FOUND:
      s_stats.not_found_errors++;
      ESP_LOGE(s_tag, "Device not found error");
      break;

    default:
      s_stats.other_errors++;
      ESP_LOGW(s_tag, "Other error: %s", esp_err_to_name(err));
      break;
  }

  s_stats.total_transfers++;
}

/**
 * @brief Transfer with retry logic
 */
static esp_err_t priv_transfer_with_retry(star_bus_manager_t *manager,
                                      const char *device_name,
                                      uint8_t *tx_data,
                                      uint8_t *rx_data,
                                      size_t length,
                                      int max_retries)
{
  esp_err_t ret = ESP_FAIL;
  int attempt = 0;

  for (attempt = 0; attempt <= max_retries; attempt++) {
    ret = star_bus_spi_transfer(manager, device_name,
                                tx_data, rx_data,
                                length, 1000);

    if (ret == ESP_OK) {
      if (attempt > 0) {
        ESP_LOGI(s_tag, "Transfer succeeded on retry %d", attempt);
        s_stats.retries_performed += attempt;
      }
      return ESP_OK;
    }

    /* Log retry */
    if (attempt < max_retries) {
      ESP_LOGW(s_tag, "Transfer failed: %s (retry %d/%d)",
               esp_err_to_name(ret), attempt + 1, max_retries);

      /* Wait before retry */
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  /* All retries failed */
  ESP_LOGE(s_tag, "Transfer failed after %d retries: %s",
           max_retries, esp_err_to_name(ret));

  return ret;
}

/**
 * @brief Demonstrate retry strategies
 */
static void priv_demonstrate_retry_strategies(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Retry Strategies ===\n");

  uint8_t tx_data[32] = {0x9F};
  uint8_t rx_data[32];

  /* Strategy 1: No retry */
  ESP_LOGI(s_tag, "1. No Retry:");
  esp_err_t ret = star_bus_spi_transfer(manager, "error_device",
                                        tx_data, rx_data, 4, 100);
  priv_handle_error(ret);
  ESP_LOGI(s_tag, "   Result: %s\n", esp_err_to_name(ret));

  /* Strategy 2: Simple retry */
  ESP_LOGI(s_tag, "2. Simple Retry (3 attempts):");
  ret = priv_transfer_with_retry(manager, "error_device",
                            tx_data, rx_data, 4, 3);
  priv_handle_error(ret);
  ESP_LOGI(s_tag, "   Final result: %s\n", esp_err_to_name(ret));

  /* Strategy 3: Exponential backoff */
  ESP_LOGI(s_tag, "3. Exponential Backoff:");
  int backoff_ms = 10;
  for (int i = 0; i < 3; i++) {
    ret = star_bus_spi_transfer(manager, "error_device",
                                tx_data, rx_data, 4, 100);
    if (ret == ESP_OK) break;

    ESP_LOGW(s_tag, "   Attempt %d failed, waiting %d ms", i + 1, backoff_ms);
    vTaskDelay(pdMS_TO_TICKS(backoff_ms));
    backoff_ms *= 2;  /* Double the wait time */
  }
  priv_handle_error(ret);
  ESP_LOGI(s_tag, "");
}

/**
 * @brief Demonstrate timeout handling
 */
static void priv_demonstrate_timeout_handling(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Timeout Handling ===\n");

  uint8_t tx_data[64];
  uint8_t rx_data[64];

  memset(tx_data, 0xAA, sizeof(tx_data));

  /* Test different timeout values */
  const int timeouts[] = {10, 100, 1000, 5000};

  for (int i = 0; i < 4; i++) {
    ESP_LOGI(s_tag, "Testing with %d ms timeout...", timeouts[i]);

    int64_t start = esp_timer_get_time();

    esp_err_t ret = star_bus_spi_transfer(manager, "error_device",
                                          tx_data, rx_data,
                                          sizeof(tx_data), timeouts[i]);

    int64_t elapsed = esp_timer_get_time() - start;

    ESP_LOGI(s_tag, "  Result: %s (took %lld us)",
             esp_err_to_name(ret), elapsed);

    priv_handle_error(ret);

    if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGW(s_tag, "  Timeout occurred - check device connection");
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Demonstrate input validation
 */
static void priv_demonstrate_input_validation(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Input Validation ===\n");

  uint8_t valid_buf[32];

  /* Test 1: NULL buffer */
  ESP_LOGI(s_tag, "1. NULL TX buffer:");
  esp_err_t ret = star_bus_spi_transfer(manager, "error_device",
                                        NULL, valid_buf, 32, 1000);
  ESP_LOGI(s_tag, "   Result: %s", esp_err_to_name(ret));
  priv_handle_error(ret);

  /* Test 2: Zero length */
  ESP_LOGI(s_tag, "\n2. Zero length transfer:");
  ret = star_bus_spi_transfer(manager, "error_device",
                               valid_buf, valid_buf, 0, 1000);
  ESP_LOGI(s_tag, "   Result: %s", esp_err_to_name(ret));
  priv_handle_error(ret);

  /* Test 3: Invalid device name */
  ESP_LOGI(s_tag, "\n3. Invalid device name:");
  ret = star_bus_spi_transfer(manager, "nonexistent_device",
                               valid_buf, valid_buf, 32, 1000);
  ESP_LOGI(s_tag, "   Result: %s", esp_err_to_name(ret));
  priv_handle_error(ret);

  /* Test 4: Stack buffer (should fail with DMA) */
  ESP_LOGI(s_tag, "\n4. Stack buffer (non-DMA memory):");
  uint8_t stack_buf[32];
  ret = star_bus_spi_transfer(manager, "error_device",
                               stack_buf, stack_buf, 32, 1000);
  ESP_LOGI(s_tag, "   Result: %s", esp_err_to_name(ret));
  ESP_LOGW(s_tag, "   WARNING: Stack buffers can crash with DMA!");
  priv_handle_error(ret);
}

/**
 * @brief Demonstrate graceful degradation
 */
static void priv_demonstrate_graceful_degradation(void)
{
  ESP_LOGI(s_tag, "\n=== Graceful Degradation ===\n");

  ESP_LOGI(s_tag, "Strategies for handling persistent errors:\n");

  ESP_LOGI(s_tag, "1. Reduce Clock Speed:");
  ESP_LOGI(s_tag, "   if (errors > threshold) {");
  ESP_LOGI(s_tag, "     clock_speed = clock_speed / 2;");
  ESP_LOGI(s_tag, "     reinit_device(clock_speed);");
  ESP_LOGI(s_tag, "   }");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Disable DMA:");
  ESP_LOGI(s_tag, "   if (dma_errors > threshold) {");
  ESP_LOGI(s_tag, "     use_dma = false;");
  ESP_LOGI(s_tag, "     recreate_device(SPI_DMA_DISABLED);");
  ESP_LOGI(s_tag, "   }");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Reduce Transfer Size:");
  ESP_LOGI(s_tag, "   if (large_transfer_fails) {");
  ESP_LOGI(s_tag, "     chunk_size = chunk_size / 2;");
  ESP_LOGI(s_tag, "     retry_with_chunks();");
  ESP_LOGI(s_tag, "   }");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Fallback Mode:");
  ESP_LOGI(s_tag, "   if (quad_mode_fails) {");
  ESP_LOGI(s_tag, "     switch_to_standard_spi();");
  ESP_LOGI(s_tag, "   }");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. Circuit Breaker:");
  ESP_LOGI(s_tag, "   if (consecutive_failures > MAX) {");
  ESP_LOGI(s_tag, "     disable_device();");
  ESP_LOGI(s_tag, "     report_hardware_failure();");
  ESP_LOGI(s_tag, "   }");
}

/**
 * @brief Best practices for error handling
 */
static void priv_print_best_practices(void)
{
  ESP_LOGI(s_tag, "\n=== Error Handling Best Practices ===\n");

  ESP_LOGI(s_tag, "1. Always Check Return Values:");
  ESP_LOGI(s_tag, "   esp_err_t ret = star_bus_spi_transfer(...);");
  ESP_LOGI(s_tag, "   if (ret != ESP_OK) {");
  ESP_LOGI(s_tag, "     // Handle error");
  ESP_LOGI(s_tag, "   }");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "2. Validate Inputs:");
  ESP_LOGI(s_tag, "   - Check pointers != NULL");
  ESP_LOGI(s_tag, "   - Verify lengths > 0");
  ESP_LOGI(s_tag, "   - Confirm DMA-capable buffers");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "3. Use Appropriate Timeouts:");
  ESP_LOGI(s_tag, "   - Short (100ms): Commands");
  ESP_LOGI(s_tag, "   - Medium (1000ms): Data transfers");
  ESP_LOGI(s_tag, "   - Long (5000ms): Large DMA");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "4. Implement Retry Logic:");
  ESP_LOGI(s_tag, "   - Retry 2-3 times for transient errors");
  ESP_LOGI(s_tag, "   - Use exponential backoff");
  ESP_LOGI(s_tag, "   - Log retry attempts");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "5. Log Errors Properly:");
  ESP_LOGI(s_tag, "   - Use ESP_LOGE for critical errors");
  ESP_LOGI(s_tag, "   - Use ESP_LOGW for retryable errors");
  ESP_LOGI(s_tag, "   - Include esp_err_to_name(ret)");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "6. Track Statistics:");
  ESP_LOGI(s_tag, "   - Count error types");
  ESP_LOGI(s_tag, "   - Monitor success rate");
  ESP_LOGI(s_tag, "   - Detect trends");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "7. Graceful Degradation:");
  ESP_LOGI(s_tag, "   - Reduce speed on errors");
  ESP_LOGI(s_tag, "   - Disable problematic features");
  ESP_LOGI(s_tag, "   - Provide fallback modes");
}

/**
 * @brief Print error statistics
 */
static void priv_print_error_statistics(void)
{
  ESP_LOGI(s_tag, "\n=== Error Statistics ===\n");

  ESP_LOGI(s_tag, "Total transfers:     %lu", s_stats.total_transfers);
  ESP_LOGI(s_tag, "Successful:          %lu (%.1f%%)",
           s_stats.successful_transfers,
           (s_stats.successful_transfers * 100.0) / s_stats.total_transfers);
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Error Breakdown:");
  ESP_LOGI(s_tag, "  Timeout:           %lu", s_stats.timeout_errors);
  ESP_LOGI(s_tag, "  Invalid argument:  %lu", s_stats.invalid_arg_errors);
  ESP_LOGI(s_tag, "  Out of memory:     %lu", s_stats.no_mem_errors);
  ESP_LOGI(s_tag, "  Not found:         %lu", s_stats.not_found_errors);
  ESP_LOGI(s_tag, "  Other:             %lu", s_stats.other_errors);
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Retries performed:   %lu", s_stats.retries_performed);

  uint32_t total_errors = s_stats.timeout_errors +
                          s_stats.invalid_arg_errors +
                          s_stats.no_mem_errors +
                          s_stats.not_found_errors +
                          s_stats.other_errors;

  if (total_errors > 0) {
    ESP_LOGI(s_tag, "\nError rate: %.2f%%",
             (total_errors * 100.0) / s_stats.total_transfers);
  }
}

void app_main(void)
{
  esp_err_t ret;
  star_bus_manager_t manager;

  ESP_LOGI(s_tag, "=== SPI Error Handling Example ===\n");

  /* Initialize manager */
  ret = star_bus_manager_init(&manager, "spi_error_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }

  /* Create SPI device */
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = SPI_CLK_SPEED,
    .mode           = 0,
    .spics_io_num   = SPI_CS_PIN,
    .queue_size     = 7,
  };

  star_bus_config_t *spi_config = star_bus_config_create_spi_device(
    "error_device",
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

  ESP_LOGI(s_tag, "SPI initialized for error testing\n");

  /* Demonstrate error handling patterns */
  priv_explain_error_codes();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_retry_strategies(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_timeout_handling(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_input_validation(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_demonstrate_graceful_degradation();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_print_best_practices();
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_print_error_statistics();

  /* Summary */
  ESP_LOGI(s_tag, "\n=== Error Handling Summary ===");
  ESP_LOGI(s_tag, "");
  ESP_LOGI(s_tag, "Key Takeaways:");
  ESP_LOGI(s_tag, "  1. Always check return values");
  ESP_LOGI(s_tag, "  2. Validate inputs before transfer");
  ESP_LOGI(s_tag, "  3. Use appropriate timeouts");
  ESP_LOGI(s_tag, "  4. Implement retry with backoff");
  ESP_LOGI(s_tag, "  5. Log errors for debugging");
  ESP_LOGI(s_tag, "  6. Track statistics for trends");
  ESP_LOGI(s_tag, "  7. Degrade gracefully on persistent errors");
  ESP_LOGI(s_tag, "");

  ESP_LOGI(s_tag, "Common Causes of Errors:");
  ESP_LOGI(s_tag, "  - Poor connections/wiring");
  ESP_LOGI(s_tag, "  - Clock speed too high");
  ESP_LOGI(s_tag, "  - Non-DMA buffers");
  ESP_LOGI(s_tag, "  - Device not powered");
  ESP_LOGI(s_tag, "  - Wrong SPI mode");
  ESP_LOGI(s_tag, "  - Insufficient timeout");

  /* Cleanup */
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nSPI error handling example complete");
}
