/**
 * @file 040_i2c_error_interface.c
 * @brief Integration with error_handler interface
 *
 * This example demonstrates:
 * - Using star_error_interface with I2C operations
 * - Recording I2C errors through error handler
 * - Automatic retry with exponential backoff
 * - Error state management
 * - Integration patterns for robust I2C communication
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_error_interface.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_ERR_IFACE";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Test device addresses */
#define WORKING_DEVICE  (0x48)  /* Known working device */
#define FAILING_DEVICE  (0x7F)  /* Intentionally wrong for error testing */

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/* Error handler configuration */
#define MAX_RETRIES         (5)
#define BASE_RETRY_DELAY_MS (50)
#define MAX_RETRY_DELAY_MS  (1000)

/**
 * @brief I2C context with error handler
 */
typedef struct {
  star_bus_manager_t     *manager;
  error_handler_t         error_handler;
  star_error_interface_t  error_interface;
  const char             *bus_name;
  uint32_t                successful_ops;
  uint32_t                recovered_errors;
} i2c_error_context_t;

/**
 * @brief Reset function for I2C error recovery
 */
static esp_err_t priv_i2c_reset_fn(void *context)
{
  i2c_error_context_t *ctx = (i2c_error_context_t *)context;

  ESP_LOGI(s_tag, "[RESET] Attempting I2C bus recovery...");

  /* In a real application, you might:
   * - Reset the I2C peripheral
   * - Toggle bus lines to clear stuck conditions
   * - Re-initialize the device
   */

  /* Simulate reset delay */
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "[RESET] Recovery attempt complete");

  return ESP_OK;
}

/**
 * @brief Initialize I2C error context
 */
static void priv_init_i2c_error_context(i2c_error_context_t *ctx,
                                        star_bus_manager_t *manager,
                                        const char *bus_name)
{
  ctx->manager          = manager;
  ctx->bus_name         = bus_name;
  ctx->successful_ops   = 0;
  ctx->recovered_errors = 0;

  /* Initialize error handler */
  esp_err_t ret = error_handler_init(
    &ctx->error_handler,
    MAX_RETRIES,
    BASE_RETRY_DELAY_MS,
    MAX_RETRY_DELAY_MS,
    priv_i2c_reset_fn,
    ctx  /* Pass context to reset function */
  );

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Error handler initialized");
    ESP_LOGI(s_tag, "  Max retries: %d", MAX_RETRIES);
    ESP_LOGI(s_tag, "  Base delay: %d ms", BASE_RETRY_DELAY_MS);
    ESP_LOGI(s_tag, "  Max delay: %d ms", MAX_RETRY_DELAY_MS);

    /* Get error interface */
    error_handler_get_interface(&ctx->error_interface, &ctx->error_handler);
  } else {
    ESP_LOGE(s_tag, "Failed to init error handler: %s", esp_err_to_name(ret));
  }
}

/**
 * @brief I2C read with error handling
 */
static esp_err_t priv_i2c_read_with_error_handling(i2c_error_context_t *ctx,
                                                   uint8_t *data, size_t len,
                                                   uint8_t reg)
{
  esp_err_t ret;
  size_t    bytes_read;

  do {
    /* Attempt the I2C read */
    ret = star_bus_i2c_read(ctx->manager, ctx->bus_name, data, len,
                            reg, &bytes_read);

    if (ret == ESP_OK) {
      ctx->successful_ops++;

      /* Reset error state on success */
      if (ctx->error_handler.in_error_state) {
        ESP_LOGI(s_tag, "Communication recovered!");
        ctx->recovered_errors++;
        error_handler_reset_state(&ctx->error_handler);
      }

      return ESP_OK;
    }

    /* Record error through interface */
    ESP_LOGW(s_tag, "I2C read failed: %s", esp_err_to_name(ret));
    ret = STAR_IFACE_RECORD_ERROR(&ctx->error_interface, ret, "I2C read failed");

    /* Check if we can retry */
    if (!error_handler_can_retry(&ctx->error_handler)) {
      ESP_LOGE(s_tag, "Max retries exceeded - giving up");
      return ret;
    }

    ESP_LOGI(s_tag, "Retrying... (attempt %u/%u)",
             ctx->error_handler.current_retry + 1, MAX_RETRIES);

    /* Wait before retry (delay already applied by error handler) */
    vTaskDelay(pdMS_TO_TICKS(ctx->error_handler.current_retry_delay));

  } while (error_handler_can_retry(&ctx->error_handler));

  return ret;
}

/**
 * @brief I2C write with error handling
 */
static esp_err_t priv_i2c_write_with_error_handling(i2c_error_context_t *ctx,
                                                    const uint8_t *data,
                                                    size_t len, uint8_t reg)
{
  esp_err_t ret;
  size_t    bytes_written;

  do {
    /* Attempt the I2C write */
    ret = star_bus_i2c_write(ctx->manager, ctx->bus_name, data, len,
                             reg, &bytes_written);

    if (ret == ESP_OK) {
      ctx->successful_ops++;

      /* Reset error state on success */
      if (ctx->error_handler.in_error_state) {
        ESP_LOGI(s_tag, "Communication recovered!");
        ctx->recovered_errors++;
        error_handler_reset_state(&ctx->error_handler);
      }

      return ESP_OK;
    }

    /* Record error */
    ESP_LOGW(s_tag, "I2C write failed: %s", esp_err_to_name(ret));
    ret = STAR_IFACE_RECORD_ERROR(&ctx->error_interface, ret,
                                  "I2C write failed");

    if (!error_handler_can_retry(&ctx->error_handler)) {
      ESP_LOGE(s_tag, "Max retries exceeded - giving up");
      return ret;
    }

    ESP_LOGI(s_tag, "Retrying... (attempt %u/%u)",
             ctx->error_handler.current_retry + 1, MAX_RETRIES);

    vTaskDelay(pdMS_TO_TICKS(ctx->error_handler.current_retry_delay));

  } while (error_handler_can_retry(&ctx->error_handler));

  return ret;
}

void test_successful_operations(i2c_error_context_t *ctx)
{
  ESP_LOGI(s_tag, "\n=== Test 1: Successful Operations ===");

  uint8_t data[4];

  ESP_LOGI(s_tag, "Performing read from working device...");
  esp_err_t ret = priv_i2c_read_with_error_handling(ctx, data, 4, 0x00);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SUCCESS: Read completed");
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X 0x%02X",
             data[0], data[1], data[2], data[3]);
  } else {
    ESP_LOGW(s_tag, "Read failed (device may not be present)");
  }
}

void test_failing_operations(i2c_error_context_t *ctx)
{
  ESP_LOGI(s_tag, "\n=== Test 2: Failing Operations (Error Recovery) ===");

  /* Temporarily change to failing device */
  const char *original_bus = ctx->bus_name;
  ctx->bus_name = "failing_device";

  uint8_t data[2];

  ESP_LOGI(s_tag, "Attempting read from non-existent device...");
  ESP_LOGI(s_tag, "Error handler will automatically retry with backoff");

  esp_err_t ret = priv_i2c_read_with_error_handling(ctx, data, 2, 0x00);

  if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "EXPECTED: Operation failed after retries");
    ESP_LOGI(s_tag, "Error handler state:");
    ESP_LOGI(s_tag, "  Error count: %u", ctx->error_handler.error_count);
    ESP_LOGI(s_tag, "  Current retry: %u", ctx->error_handler.current_retry);
    ESP_LOGI(s_tag, "  In error state: %s",
             ctx->error_handler.in_error_state ? "YES" : "NO");
  }

  /* Restore original bus */
  ctx->bus_name = original_bus;

  /* Reset error state for next test */
  error_handler_reset_state(&ctx->error_handler);
}

void test_intermittent_errors(i2c_error_context_t *ctx)
{
  ESP_LOGI(s_tag, "\n=== Test 3: Intermittent Errors ===");
  ESP_LOGI(s_tag, "Simulating occasional failures with recovery");

  for (int i = 0; i < 10; i++) {
    uint8_t data;

    /* Simulate intermittent failures by using wrong device occasionally */
    if (i % 3 == 0) {
      /* This will likely fail */
      const char *original = ctx->bus_name;
      ctx->bus_name = "failing_device";

      esp_err_t ret = priv_i2c_read_with_error_handling(ctx, &data, 1, i);

      if (ret != ESP_OK) {
        ESP_LOGI(s_tag, "Iteration %d: Failed (expected)", i);
      }

      ctx->bus_name = original;
      error_handler_reset_state(&ctx->error_handler);
    } else {
      /* This should succeed */
      esp_err_t ret = priv_i2c_read_with_error_handling(ctx, &data, 1, i);

      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Iteration %d: Success", i);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void print_error_statistics(i2c_error_context_t *ctx)
{
  ESP_LOGI(s_tag, "\n=== Error Handling Statistics ===");
  ESP_LOGI(s_tag, "Successful operations: %u", ctx->successful_ops);
  ESP_LOGI(s_tag, "Recovered errors: %u", ctx->recovered_errors);
  ESP_LOGI(s_tag, "Total errors recorded: %u", ctx->error_handler.error_count);
  ESP_LOGI(s_tag, "Currently in error state: %s",
           ctx->error_handler.in_error_state ? "YES" : "NO");

  if (ctx->successful_ops + ctx->error_handler.error_count > 0) {
    float success_rate = (float)ctx->successful_ops /
                        (ctx->successful_ops + ctx->error_handler.error_count)
                        * 100.0f;
    ESP_LOGI(s_tag, "Success rate: %.1f%%", success_rate);
  }
}

void i2c_error_interface_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_ErrIface", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config for working device */
  star_bus_config_t *working_config = star_bus_config_create_i2c(
    "working_device",
    I2C_NUM_0,
    WORKING_DEVICE,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  /* Create I2C config for failing device (for testing) */
  star_bus_config_t *failing_config = star_bus_config_create_i2c(
    "failing_device",
    I2C_NUM_0,
    FAILING_DEVICE,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (working_config == NULL || failing_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C configs");
    if (working_config) {
      star_bus_config_destroy(working_config);
    }
    if (failing_config) {
      star_bus_config_destroy(failing_config);
    }
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Add both configs */
  star_bus_manager_add_bus(&manager, working_config);
  star_bus_config_init(working_config, &manager);

  star_bus_manager_add_bus(&manager, failing_config);
  star_bus_config_init(failing_config, &manager);

  ESP_LOGI(s_tag, "I2C buses initialized");

  /* Initialize error context */
  i2c_error_context_t ctx;
  priv_init_i2c_error_context(&ctx, &manager, "working_device");

  /* Run tests */
  test_successful_operations(&ctx);
  vTaskDelay(pdMS_TO_TICKS(500));

  test_failing_operations(&ctx);
  vTaskDelay(pdMS_TO_TICKS(500));

  test_intermittent_errors(&ctx);
  vTaskDelay(pdMS_TO_TICKS(500));

  print_error_statistics(&ctx);

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Error Handling Best Practices ===");
  ESP_LOGI(s_tag, "1. Always use error interface for I2C operations");
  ESP_LOGI(s_tag, "2. Configure retry params based on device characteristics");
  ESP_LOGI(s_tag, "3. Implement appropriate reset/recovery functions");
  ESP_LOGI(s_tag, "4. Monitor error statistics in production");
  ESP_LOGI(s_tag, "5. Log errors with context for debugging");
  ESP_LOGI(s_tag, "6. Reset error state after successful recovery");
  ESP_LOGI(s_tag, "7. Use exponential backoff for retries");

  /* Cleanup */
  error_handler_deinit(&ctx.error_handler);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nError interface example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Error Handler Interface Example ===\n");
  i2c_error_interface_example();
}
