/**
 * @file 159_dip_error_handling.c
 * @brief Dependency Inversion Principle error handling patterns
 *
 * Demonstrates:
 * - Custom error handler implementation
 * - Interface injection for testability
 * - Error recovery strategies
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_core.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "DIP_ERROR";

/* Custom error tracking context */
typedef struct {
  uint32_t total_errors;
  uint32_t recoverable_errors;
  uint32_t fatal_errors;
  esp_err_t last_error;
  const char *last_error_desc;
  uint32_t retry_count;
  uint32_t max_retries;
} custom_error_ctx_t;

/* Custom error handler functions */
static esp_err_t custom_record_error(void *ctx, esp_err_t err, const char *desc,
                                      const char *file, int line, const char *func)
{
  custom_error_ctx_t* ectx = (custom_error_ctx_t*)ctx;
  ectx->total_errors++;
  ectx->last_error = err;
  ectx->last_error_desc = desc;

  /* Classify error */
  if (err == ESP_ERR_TIMEOUT || err == ESP_ERR_INVALID_RESPONSE) {
    ectx->recoverable_errors++;
    ectx->retry_count++;
    ESP_LOGW(s_tag, "Recoverable error [%d]: %s at %s:%d", err, desc, file, line);
  } else {
    ectx->fatal_errors++;
    ectx->retry_count = 0;
    ESP_LOGE(s_tag, "Fatal error [%d]: %s at %s:%d", err, desc, file, line);
  }

  return err;
}

static bool custom_can_retry(void *ctx)
{
  custom_error_ctx_t* ectx = (custom_error_ctx_t*)ctx;
  return ectx->retry_count > 0 && ectx->retry_count <= ectx->max_retries;
}

static esp_err_t custom_reset_state(void *ctx)
{
  custom_error_ctx_t* ectx = (custom_error_ctx_t*)ctx;
  ectx->retry_count = 0;
  ectx->last_error = ESP_OK;
  ectx->last_error_desc = NULL;
  ESP_LOGI(s_tag, "Error state reset");
  return ESP_OK;
}

static uint32_t custom_get_retry_delay(void *ctx)
{
  custom_error_ctx_t* ectx = (custom_error_ctx_t*)ctx;
  /* Exponential backoff: 100ms, 200ms, 400ms, 800ms... */
  return 100 * (1 << (ectx->retry_count - 1));
}

/* Simulate operation that might fail */
static esp_err_t unreliable_operation(int attempt, star_error_interface_t* error_iface)
{
  /* Simulate 50% failure rate for first few attempts */
  if (attempt < 3 && (esp_random() % 2) == 0) {
    STAR_IFACE_RECORD_ERROR(error_iface, ESP_ERR_TIMEOUT, "Simulated timeout");
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

void dip_error_handling_example(void)
{
  /* Create custom error context */
  custom_error_ctx_t error_ctx = {
    .total_errors = 0,
    .recoverable_errors = 0,
    .fatal_errors = 0,
    .retry_count = 0,
    .max_retries = 5
  };

  /* Build interface from custom functions */
  star_error_interface_t custom_iface = {
    .ctx = &error_ctx,
    .record_error = custom_record_error,
    .can_retry = custom_can_retry,
    .reset_state = custom_reset_state
  };

  ESP_LOGI(s_tag, "Custom error interface created");
  if (!star_error_interface_is_valid(&custom_iface)) {
    ESP_LOGE(s_tag, "Error interface validation failed");
    return;
  }

  /* Initialize bus manager with custom error handler */
  star_bus_manager_t manager;
  esp_err_t ret = star_bus_manager_init(&manager, "DIP_Demo", &custom_iface, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Bus manager initialization failed: %d", ret);
    return;
  }

  ESP_LOGI(s_tag, "Testing retry mechanism...");

  /* Demonstrate retry pattern */
  for (int op = 0; op < 10; op++) {
    int attempt = 0;
    esp_err_t result;

    do {
      attempt++;
      result = unreliable_operation(attempt, &custom_iface);

      if (result != ESP_OK && STAR_IFACE_CAN_RETRY(&custom_iface)) {
        uint32_t delay = custom_get_retry_delay(custom_iface.ctx);
        ESP_LOGI(s_tag, "Operation %d failed, retrying in %lu ms...", op, delay);
        vTaskDelay(pdMS_TO_TICKS(delay));
      }
    } while (result != ESP_OK && STAR_IFACE_CAN_RETRY(&custom_iface));

    if (result == ESP_OK) {
      ESP_LOGI(s_tag, "Operation %d succeeded after %d attempts", op, attempt);
      STAR_IFACE_RESET_STATE(&custom_iface);
    } else {
      ESP_LOGE(s_tag, "Operation %d failed permanently", op);
      STAR_IFACE_RESET_STATE(&custom_iface);
    }
  }

  /* Print statistics */
  ESP_LOGI(s_tag, "=== Error Statistics ===");
  ESP_LOGI(s_tag, "Total errors: %lu", error_ctx.total_errors);
  ESP_LOGI(s_tag, "Recoverable: %lu", error_ctx.recoverable_errors);
  ESP_LOGI(s_tag, "Fatal: %lu", error_ctx.fatal_errors);

  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { dip_error_handling_example(); }
