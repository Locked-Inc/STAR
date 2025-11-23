/**
 * @file 032_i2c_callbacks.c
 * @brief Custom on_transfer_complete and on_error callbacks
 *
 * This example demonstrates:
 * - Registering transfer complete callbacks
 * - Registering error callbacks
 * - Asynchronous operation notification
 * - Error handling via callbacks
 * - Using callbacks for logging and monitoring
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_async.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char *s_tag = "I2C_CALLBACKS";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device address */
#define DEVICE_ADDR     (0x48)

/* Clock speed */
#define I2C_CLK_SPEED   (100000)

/* Callback statistics */
typedef struct {
  uint32_t          transfers_completed;
  uint32_t          transfers_failed;
  uint32_t          total_bytes_transferred;
  int64_t           last_transfer_time_us;
  SemaphoreHandle_t mutex;
} callback_stats_t;

static callback_stats_t s_stats = {0};

/**
 * @brief Initialize callback statistics
 */
void init_callback_stats(void)
{
  s_stats.transfers_completed     = 0;
  s_stats.transfers_failed        = 0;
  s_stats.total_bytes_transferred = 0;
  s_stats.last_transfer_time_us   = 0;
  s_stats.mutex                   = xSemaphoreCreateMutex();
}

/**
 * @brief Transfer complete callback
 */
void on_transfer_complete(star_async_handle_t handle,
                          star_async_status_t status,
                          esp_err_t result,
                          void *context)
{
  if (xSemaphoreTake(s_stats.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    s_stats.transfers_completed++;
    s_stats.last_transfer_time_us = esp_timer_get_time();

    ESP_LOGI(s_tag, "[CALLBACK] Transfer complete!");
    ESP_LOGI(s_tag, "  Status: %d", status);
    ESP_LOGI(s_tag, "  Result: %s", esp_err_to_name(result));
    ESP_LOGI(s_tag, "  Total completed: %u", s_stats.transfers_completed);

    if (context) {
      const char *operation = (const char *)context;
      ESP_LOGI(s_tag, "  Operation: %s", operation);
    }

    xSemaphoreGive(s_stats.mutex);
  }
}

/**
 * @brief Error callback
 */
void on_error(star_async_handle_t handle,
              star_async_status_t status,
              esp_err_t result,
              void *context)
{
  if (xSemaphoreTake(s_stats.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    s_stats.transfers_failed++;

    ESP_LOGE(s_tag, "[ERROR CALLBACK] Transfer failed!");
    ESP_LOGE(s_tag, "  Status: %d", status);
    ESP_LOGE(s_tag, "  Error: %s", esp_err_to_name(result));
    ESP_LOGE(s_tag, "  Total failed: %u", s_stats.transfers_failed);

    if (context) {
      const char *operation = (const char *)context;
      ESP_LOGE(s_tag, "  Operation: %s", operation);
    }

    /* Could implement recovery logic here */
    ESP_LOGI(s_tag, "  Implementing error recovery...");

    xSemaphoreGive(s_stats.mutex);
  }
}

/**
 * @brief Combined callback (handles both success and error)
 */
void on_operation_complete(star_async_handle_t handle,
                           star_async_status_t status,
                           esp_err_t result,
                           void *context)
{
  if (result == ESP_OK) {
    ESP_LOGI(s_tag, "[COMBINED] Operation succeeded");
    on_transfer_complete(handle, status, result, context);
  } else {
    ESP_LOGW(s_tag, "[COMBINED] Operation failed");
    on_error(handle, status, result, context);
  }
}

void test_async_read_with_callback(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 1: Async Read with Callback ===");

  uint8_t read_data[4];

  /* Configure async operation with callback */
  star_async_config_t async_config = {
    .timeout_ms = 1000,
    .callback   = on_transfer_complete,
    .context    = (void *)"Async Read Test",
    .priority   = 0
  };

  star_async_handle_t handle = NULL;

  ESP_LOGI(s_tag, "Initiating async read operation...");
  esp_err_t ret = star_bus_i2c_read_async(manager, "device", read_data,
                                          sizeof(read_data), 0x00,
                                          &async_config, &handle);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async operation queued successfully");
    ESP_LOGI(s_tag, "Waiting for callback...");

    /* Wait for operation to complete */
    ret = star_async_wait(handle, 2000);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Operation completed successfully");
      ESP_LOGI(s_tag, "Data read: 0x%02X 0x%02X 0x%02X 0x%02X",
               read_data[0], read_data[1], read_data[2], read_data[3]);
    }

    star_async_free_handle(handle);
  } else {
    ESP_LOGE(s_tag, "Failed to queue async operation: %s",
             esp_err_to_name(ret));
  }
}

void test_async_write_with_callback(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 2: Async Write with Callback ===");

  uint8_t write_data[] = {0xAA, 0xBB, 0xCC, 0xDD};

  /* Configure async operation */
  star_async_config_t async_config = {
    .timeout_ms = 1000,
    .callback   = on_transfer_complete,
    .context    = (void *)"Async Write Test",
    .priority   = 0
  };

  star_async_handle_t handle = NULL;

  ESP_LOGI(s_tag, "Initiating async write operation...");
  esp_err_t ret = star_bus_i2c_write_async(manager, "device", write_data,
                                           sizeof(write_data), 0x00,
                                           &async_config, &handle);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Async write queued");

    /* Wait for callback */
    ret = star_async_wait(handle, 2000);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Write completed successfully");
    }

    star_async_free_handle(handle);
  } else {
    ESP_LOGE(s_tag, "Failed to queue async write: %s", esp_err_to_name(ret));
  }
}

void test_error_callback(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 3: Error Callback ===");
  ESP_LOGI(s_tag, "Attempting operation that will fail...");

  /* Create config for non-existent device */
  star_bus_config_t *bad_config = star_bus_config_create_i2c(
    "bad_device",
    I2C_NUM_0,
    0x7F,  /* Likely non-existent */
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (bad_config) {
    star_bus_manager_add_bus(manager, bad_config);
    star_bus_config_init(bad_config, manager);

    uint8_t dummy_data[2];

    /* Configure with error callback */
    star_async_config_t async_config = {
      .timeout_ms = 500,
      .callback   = on_error,
      .context    = (void *)"Error Test",
      .priority   = 0
    };

    star_async_handle_t handle = NULL;

    esp_err_t ret = star_bus_i2c_read_async(manager, "bad_device", dummy_data,
                                            2, 0x00, &async_config, &handle);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Waiting for error callback...");
      star_async_wait(handle, 2000);
      star_async_free_handle(handle);
    }

    star_bus_manager_remove_bus(manager, "bad_device");
    star_bus_config_destroy(bad_config);
  }
}

void test_multiple_async_operations(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 4: Multiple Async Operations ===");
  ESP_LOGI(s_tag, "Queueing multiple operations with callbacks...");

  star_async_handle_t handles[3];
  uint8_t             buffers[3][4];

  star_async_config_t async_config = {
    .timeout_ms = 1000,
    .callback   = on_operation_complete,
    .context    = NULL,
    .priority   = 0
  };

  /* Queue multiple reads */
  for (int i = 0; i < 3; i++) {
    char *context = malloc(32);
    snprintf(context, 32, "Operation %d", i);
    async_config.context = context;

    esp_err_t ret = star_bus_i2c_read_async(manager, "device", buffers[i], 4,
                                            i, &async_config, &handles[i]);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Queued operation %d", i);
    }
  }

  /* Wait for all to complete */
  ESP_LOGI(s_tag, "Waiting for all operations to complete...");
  for (int i = 0; i < 3; i++) {
    star_async_wait(handles[i], 3000);
    free((void *)async_config.context);
    star_async_free_handle(handles[i]);
  }

  ESP_LOGI(s_tag, "All operations completed");
}

void print_callback_statistics(void)
{
  ESP_LOGI(s_tag, "\n=== Callback Statistics ===");

  if (xSemaphoreTake(s_stats.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(s_tag, "Transfers completed: %u", s_stats.transfers_completed);
    ESP_LOGI(s_tag, "Transfers failed: %u", s_stats.transfers_failed);
    ESP_LOGI(s_tag, "Total bytes: %u", s_stats.total_bytes_transferred);

    uint32_t total = s_stats.transfers_completed + s_stats.transfers_failed;
    if (total > 0) {
      float success_rate = (float)s_stats.transfers_completed / total * 100.0f;
      ESP_LOGI(s_tag, "Success rate: %.1f%%", success_rate);
    }

    xSemaphoreGive(s_stats.mutex);
  }
}

void i2c_callbacks_example(void)
{
  esp_err_t ret;

  /* Initialize statistics */
  init_callback_stats();

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Callbacks", NULL, NULL);
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

  /* Run callback tests */
  test_async_read_with_callback(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  test_async_write_with_callback(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  test_error_callback(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  test_multiple_async_operations(&manager);

  /* Print statistics */
  print_callback_statistics();

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Callback Best Practices ===");
  ESP_LOGI(s_tag, "1. Keep callbacks short and non-blocking");
  ESP_LOGI(s_tag, "2. Use thread-safe data structures");
  ESP_LOGI(s_tag, "3. Don't call blocking I/O from callbacks");
  ESP_LOGI(s_tag, "4. Use context pointer for operation identification");
  ESP_LOGI(s_tag, "5. Handle both success and error cases");
  ESP_LOGI(s_tag, "6. Consider using event groups for synchronization");

  /* Cleanup */
  if (s_stats.mutex) {
    vSemaphoreDelete(s_stats.mutex);
  }
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nCallbacks example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Callbacks Example ===\n");
  i2c_callbacks_example();
}
