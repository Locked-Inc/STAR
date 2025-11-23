/**
 * @file 018_error_retry.c
 * @brief Error handler retry logic example
 *
 * Demonstrates:
 * - Exponential backoff configuration
 * - Recording errors
 * - Retry decision logic
 * - Custom reset functions
 * - Error statistics
 */

#include "star_error_handler.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ERROR_RETRY_EXAMPLE";

/* Simulated device state */
static int  s_device_failures  = 0;
static bool s_device_connected = true;

/* Custom reset function */
static esp_err_t device_reset(void* context)
{
  const char* device_name = (const char*)context;
  ESP_LOGI(s_tag, "Resetting device: %s", device_name);

  /* Simulate reset delay */
  vTaskDelay(pdMS_TO_TICKS(100));

  /* Reset device state */
  s_device_connected = true;
  s_device_failures = 0;

  ESP_LOGI(s_tag, "Device reset complete");
  return ESP_OK;
}

/* Simulate unreliable device operation */
static esp_err_t unreliable_operation(void)
{
  s_device_failures++;

  /* Fail for first few attempts, then succeed */
  if (s_device_failures < 3) {
    ESP_LOGW(s_tag, "Operation failed (attempt %d)", s_device_failures);
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(s_tag, "Operation succeeded (attempt %d)", s_device_failures);
  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Error Retry Example ===\n");

  /* --- Basic Error Handler --- */
  ESP_LOGI(s_tag, "--- Basic Error Handler ---");

  error_handler_t handler;
  ret = error_handler_init(
    &handler,
    5,      /* max_retries */
    100,    /* base_retry_delay_ms */
    5000,   /* max_retry_delay_ms */
    NULL,   /* reset_fn */
    NULL    /* reset_context */
  );

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init error handler");
    return;
  }

  ESP_LOGI(s_tag, "Error handler initialized:");
  ESP_LOGI(s_tag, "  Max retries: 5");
  ESP_LOGI(s_tag, "  Base delay: 100ms");
  ESP_LOGI(s_tag, "  Max delay: 5000ms");

  /* --- Retry Loop Pattern --- */
  ESP_LOGI(s_tag, "\n--- Retry Loop Pattern ---");

  s_device_failures = 0;

  do {
    ret = unreliable_operation();
    if (ret != ESP_OK) {
      RECORD_ERROR(&handler, ret, "Device communication failed");

      if (error_handler_can_retry(&handler)) {
        ESP_LOGI(s_tag, "Retrying...");
        /* Delay handled internally by can_retry */
      } else {
        ESP_LOGE(s_tag, "Max retries exceeded");
        break;
      }
    }
  } while (ret != ESP_OK);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Operation eventually succeeded");
  }

  /* Reset state for next example */
  error_handler_reset_state(&handler);

  /* --- Exponential Backoff --- */
  ESP_LOGI(s_tag, "\n--- Exponential Backoff Timing ---");

  ESP_LOGI(s_tag, "Delay calculation: min(base * 2^attempt, max)");
  ESP_LOGI(s_tag, "  Attempt 0: %d ms", 100);
  ESP_LOGI(s_tag, "  Attempt 1: %d ms", 200);
  ESP_LOGI(s_tag, "  Attempt 2: %d ms", 400);
  ESP_LOGI(s_tag, "  Attempt 3: %d ms", 800);
  ESP_LOGI(s_tag, "  Attempt 4: %d ms", 1600);
  ESP_LOGI(s_tag, "  Attempt 5: %d ms (capped at 5000)", 3200);

  /* --- Handler with Reset Function --- */
  ESP_LOGI(s_tag, "\n--- Handler with Reset Function ---");

  error_handler_deinit(&handler);

  ret = error_handler_init(
    &handler,
    3,
    200,
    2000,
    device_reset,
    "SensorDevice"
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Handler with reset function initialized");
  }

  /* Simulate failures that trigger reset */
  s_device_failures = 0;

  for (int i = 0; i < 4; i++) {
    ret = unreliable_operation();
    if (ret != ESP_OK) {
      RECORD_ERROR(&handler, ret, "Sensor read failed");

      if (!error_handler_can_retry(&handler)) {
        ESP_LOGW(s_tag, "Retries exhausted - reset will be called");
        break;
      }
    } else {
      break;
    }
  }

  /* --- RECORD_ERROR Macro --- */
  ESP_LOGI(s_tag, "\n--- RECORD_ERROR Macro ---");
  ESP_LOGI(s_tag, "Usage: RECORD_ERROR(&handler, error_code, \"message\")");
  ESP_LOGI(s_tag, "- Records error with code and message");
  ESP_LOGI(s_tag, "- Logs error automatically");
  ESP_LOGI(s_tag, "- Updates internal state");

  error_handler_reset_state(&handler);

  /* Record various error types */
  RECORD_ERROR(&handler, ESP_ERR_TIMEOUT, "Communication timeout");
  RECORD_ERROR(&handler, ESP_ERR_INVALID_RESPONSE, "Invalid response from device");
  RECORD_ERROR(&handler, ESP_FAIL, "General failure");

  /* --- Interface Adapter --- */
  ESP_LOGI(s_tag, "\n--- Interface Adapter ---");

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  ESP_LOGI(s_tag, "Interface provides:");
  ESP_LOGI(s_tag, "  record_error() - Record an error");
  ESP_LOGI(s_tag, "  can_retry() - Check if retry is possible");
  ESP_LOGI(s_tag, "  reset_state() - Reset error state");

  /* Use through interface */
  if (iface.can_retry != NULL) {
    bool can_retry = iface.can_retry(iface.ctx);
    ESP_LOGI(s_tag, "Can retry (via interface): %s", can_retry ? "Yes" : "No");
  }

  /* --- Multiple Handlers --- */
  ESP_LOGI(s_tag, "\n--- Multiple Handlers ---");

  error_handler_t sensor_handler, comms_handler;

  error_handler_init(&sensor_handler, 3, 50, 500, NULL, NULL);
  error_handler_init(&comms_handler, 5, 1000, 30000, NULL, NULL);

  ESP_LOGI(s_tag, "Sensor handler: 3 retries, 50-500ms delay");
  ESP_LOGI(s_tag, "Comms handler: 5 retries, 1000-30000ms delay");

  /* Different retry strategies for different operations */
  RECORD_ERROR(&sensor_handler, ESP_ERR_TIMEOUT, "Sensor timeout");
  RECORD_ERROR(&comms_handler, ESP_ERR_TIMEOUT, "Network timeout");

  ESP_LOGI(s_tag, "Sensor can retry: %s",
           error_handler_can_retry(&sensor_handler) ? "Yes" : "No");
  ESP_LOGI(s_tag, "Comms can retry: %s",
           error_handler_can_retry(&comms_handler) ? "Yes" : "No");

  error_handler_deinit(&sensor_handler);
  error_handler_deinit(&comms_handler);

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Choose retry count based on operation criticality");
  ESP_LOGI(s_tag, "2. Set base delay based on device recovery time");
  ESP_LOGI(s_tag, "3. Set max delay to avoid blocking too long");
  ESP_LOGI(s_tag, "4. Provide reset function for hardware recovery");
  ESP_LOGI(s_tag, "5. Reset state after successful operation");
  ESP_LOGI(s_tag, "6. Use interface adapters for modularity");

  /* Cleanup */
  error_handler_deinit(&handler);

  ESP_LOGI(s_tag, "\nError retry example complete");
}
