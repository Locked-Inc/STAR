/**
 * @file 03_error_handler.c
 * @brief Error handler and retry logic example
 *
 * This example demonstrates:
 * - Creating an error handler with retry logic
 * - Recording errors with exponential backoff
 * - Checking if retries are available
 * - Using a custom reset function
 * - Integrating with bus manager
 */

#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_error_interface.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ERROR_HANDLER_EXAMPLE";

/* Simulated device state for reset function */
static bool s_device_ready = false;

/**
 * @brief Custom reset function called when retrying after errors
 */
static esp_err_t priv_device_reset_function(void *context)
{
  ESP_LOGI(s_tag, "Reset function called - attempting to recover device");

  /* Simulate device reset */
  vTaskDelay(pdMS_TO_TICKS(100));

  /* Simulate success on third attempt */
  static int reset_attempts = 0;
  reset_attempts++;

  if (reset_attempts >= 3) {
    s_device_ready = true;
    ESP_LOGI(s_tag, "Device reset successful!");
    return ESP_OK;
  }

  ESP_LOGW(s_tag, "Device reset attempt %d failed", reset_attempts);
  return ESP_ERR_TIMEOUT;
}

void error_handler_example(void)
{
  esp_err_t ret;

  /* Step 1: Initialize error handler */
  error_handler_t handler;
  ret = error_handler_init(
    &handler,
    5,      /* max_retries */
    100,    /* base_retry_delay (ms) */
    5000,   /* max_retry_delay (ms) */
    priv_device_reset_function,  /* reset function */
    NULL    /* reset context */
  );

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init error handler: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "Error handler initialized");
  ESP_LOGI(s_tag, "  Max retries: %lu", (unsigned long)handler.max_retries);
  ESP_LOGI(s_tag, "  Base delay: %lu ms", (unsigned long)handler.base_retry_delay);
  ESP_LOGI(s_tag, "  Max delay: %lu ms", (unsigned long)handler.max_retry_delay);

  /* Step 2: Simulate an operation with errors */
  ESP_LOGI(s_tag, "\n--- Simulating errors with retry ---");

  for (int attempt = 1; attempt <= 6; attempt++) {
    ESP_LOGI(s_tag, "Attempt %d...", attempt);

    /* Simulate an operation that fails */
    esp_err_t operation_result;
    if (s_device_ready) {
      operation_result = ESP_OK;
    } else {
      operation_result = ESP_ERR_TIMEOUT;
    }

    if (operation_result != ESP_OK) {
      /* Record the error using the macro */
      RECORD_ERROR(&handler, operation_result, "Device communication timeout");

      /* Check if we can retry */
      if (error_handler_can_retry(&handler)) {
        ESP_LOGI(s_tag, "  Retry available - waiting %lu ms",
                 (unsigned long)handler.current_retry_delay);
        vTaskDelay(pdMS_TO_TICKS(handler.current_retry_delay));
      } else {
        ESP_LOGE(s_tag, "  No more retries available!");
        break;
      }
    } else {
      ESP_LOGI(s_tag, "  Operation succeeded!");
      break;
    }
  }

  /* Step 3: Check error state */
  ESP_LOGI(s_tag, "\n--- Error Handler State ---");
  ESP_LOGI(s_tag, "Total errors: %lu", (unsigned long)handler.error_count);
  ESP_LOGI(s_tag, "In error state: %s", handler.in_error_state ? "yes" : "no");
  ESP_LOGI(s_tag, "Last error: %s", esp_err_to_name(handler.last_error));
  ESP_LOGI(s_tag, "Current retry: %lu", (unsigned long)handler.current_retry);

  /* Step 4: Reset error state for new operations */
  ESP_LOGI(s_tag, "\n--- Resetting Error State ---");
  ret = error_handler_reset_state(&handler);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Error state reset successfully");
    ESP_LOGI(s_tag, "Total errors: %lu", (unsigned long)handler.error_count);
    ESP_LOGI(s_tag, "In error state: %s", handler.in_error_state ? "yes" : "no");
  }

  /* Step 5: Cleanup */
  error_handler_deinit(&handler);
  ESP_LOGI(s_tag, "Error handler example complete");
}

/* Example: Using error handler with bus manager */
void error_handler_with_bus_example(void)
{
  esp_err_t ret;

  /* Initialize error handler */
  error_handler_t handler;
  ret = error_handler_init(&handler, 3, 50, 1000, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init error handler");
    return;
  }

  /* Create error interface adapter */
  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &handler);

  /* Initialize bus manager with error interface */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "ErrorDemo", &error_iface, NULL);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Bus manager initialized with error handling");

    /* Now all bus operations will automatically use the error handler */
    /* for recording errors and managing retries */

    /* Example: The bus operations will call the error interface */
    /* when errors occur during I2C/SPI transactions */
  }

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  error_handler_deinit(&handler);
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== Error Handler Example ===\n");
  error_handler_example();

  ESP_LOGI(s_tag, "\n=== Error Handler with Bus Manager ===\n");
  error_handler_with_bus_example();
}
