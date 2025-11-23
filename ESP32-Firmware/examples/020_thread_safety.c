/**
 * @file 020_thread_safety.c
 * @brief Thread safety and multi-task bus access example
 *
 * Demonstrates:
 * - Multiple tasks accessing same bus
 * - Mutex protection
 * - Safe callback patterns
 * - Avoiding deadlocks
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char *s_tag = "THREAD_SAFETY_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)

static star_bus_manager_t s_manager;
static volatile bool      s_running = true;

/* Task to read from sensor */
static void sensor_read_task(void* pvParameters)
{
  const char* task_name = (const char*)pvParameters;
  uint8_t buffer[4];
  size_t bytes_read;
  int read_count = 0;

  ESP_LOGI(s_tag, "[%s] Started", task_name);

  while (s_running && read_count < 5) {
    /* Bus manager handles mutex internally */
    esp_err_t ret = star_bus_i2c_read(
      &s_manager,
      "shared_sensor",
      buffer,
      sizeof(buffer),
      0x00,
      &bytes_read
    );

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "[%s] Read %d: 0x%02X 0x%02X",
               task_name, read_count + 1, buffer[0], buffer[1]);
    } else {
      ESP_LOGW(s_tag, "[%s] Read failed: %s", task_name, esp_err_to_name(ret));
    }

    read_count++;
    vTaskDelay(pdMS_TO_TICKS(100 + (read_count % 3) * 50));
  }

  ESP_LOGI(s_tag, "[%s] Finished", task_name);
  vTaskDelete(NULL);
}

/* Task to write to sensor */
static void sensor_write_task(void* pvParameters)
{
  const char* task_name = (const char*)pvParameters;
  uint8_t data[2];
  int write_count = 0;

  ESP_LOGI(s_tag, "[%s] Started", task_name);

  while (s_running && write_count < 5) {
    data[0] = write_count;
    data[1] = 0xFF - write_count;

    esp_err_t ret = star_bus_i2c_write(
      &s_manager,
      "shared_sensor",
      data,
      sizeof(data),
      0x10,
      NULL
    );

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "[%s] Write %d: 0x%02X 0x%02X",
               task_name, write_count + 1, data[0], data[1]);
    } else {
      ESP_LOGW(s_tag, "[%s] Write failed: %s", task_name, esp_err_to_name(ret));
    }

    write_count++;
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  ESP_LOGI(s_tag, "[%s] Finished", task_name);
  vTaskDelete(NULL);
}

/* Callback for safe access */
static esp_err_t safe_operation_callback(star_bus_config_t* config, void* user_data)
{
  int* result = (int*)user_data;
  ESP_LOGI(s_tag, "Inside callback - mutex held");

  /* Do multiple operations atomically */
  uint8_t buffer[2];
  size_t bytes_read;

  /* Read */
  /* In real code, use the config to do I2C operations */
  buffer[0] = 0x12;
  buffer[1] = 0x34;

  *result = buffer[0] + buffer[1];

  ESP_LOGI(s_tag, "Callback complete");
  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Thread Safety Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "thread_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create shared bus */
  star_bus_config_t* shared_config = star_bus_config_create_i2c(
    "shared_sensor",
    I2C_NUM_0,
    0x48,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (shared_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  star_bus_manager_add_bus(&s_manager, shared_config);
  star_bus_config_init(shared_config, &s_manager);

  ESP_LOGI(s_tag, "Shared bus initialized");

  /* --- Mutex Timeout Configuration --- */
  ESP_LOGI(s_tag, "\n--- Mutex Configuration ---");
  ESP_LOGI(s_tag, "Bus mutex timeout: %d ms", CONFIG_STAR_KCONFIG_BUS_MUTEX_TIMEOUT_MS);
  ESP_LOGI(s_tag, "Operations will wait for mutex before proceeding");

  /* --- Multiple Tasks --- */
  ESP_LOGI(s_tag, "\n--- Starting Multiple Tasks ---");

  TaskHandle_t reader1, reader2, writer1;

  xTaskCreate(sensor_read_task, "Reader1", 4096, "Reader1", 5, &reader1);
  xTaskCreate(sensor_read_task, "Reader2", 4096, "Reader2", 5, &reader2);
  xTaskCreate(sensor_write_task, "Writer1", 4096, "Writer1", 5, &writer1);

  ESP_LOGI(s_tag, "Created 2 reader tasks and 1 writer task");
  ESP_LOGI(s_tag, "All tasks share the same bus with automatic locking");

  /* Wait for tasks to complete */
  vTaskDelay(pdMS_TO_TICKS(2000));
  s_running = false;
  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Safe Callback Pattern --- */
  ESP_LOGI(s_tag, "\n--- Safe Callback Pattern ---");
  ESP_LOGI(s_tag, "Use star_bus_manager_with_bus() for atomic operations");

  int result = 0;
  ret = star_bus_manager_with_bus(&s_manager, "shared_sensor",
                                   safe_operation_callback, &result);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Safe callback result: %d", result);
  }

  /* --- Avoiding Deadlocks --- */
  ESP_LOGI(s_tag, "\n--- Avoiding Deadlocks ---");
  ESP_LOGI(s_tag, "Rules to prevent deadlocks:");
  ESP_LOGI(s_tag, "1. Don't nest bus operations");
  ESP_LOGI(s_tag, "2. Don't call bus ops inside bus callback");
  ESP_LOGI(s_tag, "3. Keep critical sections short");
  ESP_LOGI(s_tag, "4. Use timeouts for mutex acquisition");
  ESP_LOGI(s_tag, "5. Access buses in consistent order");

  /* --- Priority Inversion --- */
  ESP_LOGI(s_tag, "\n--- Priority Inversion ---");
  ESP_LOGI(s_tag, "FreeRTOS mutexes have priority inheritance");
  ESP_LOGI(s_tag, "This prevents priority inversion issues");

  /* --- Thread-Safe Operations --- */
  ESP_LOGI(s_tag, "\n--- Thread-Safe Operations ---");
  ESP_LOGI(s_tag, "All star_bus_* operations are thread-safe:");
  ESP_LOGI(s_tag, "  - star_bus_i2c_read/write");
  ESP_LOGI(s_tag, "  - star_bus_spi_transmit/receive/transfer");
  ESP_LOGI(s_tag, "  - star_smbus_* operations");
  ESP_LOGI(s_tag, "  - star_batch_execute");
  ESP_LOGI(s_tag, "  - star_async_* operations");

  /* --- Non-Thread-Safe Operations --- */
  ESP_LOGI(s_tag, "\n--- Non-Thread-Safe Operations ---");
  ESP_LOGI(s_tag, "These require external synchronization:");
  ESP_LOGI(s_tag, "  - star_bus_config_create_*");
  ESP_LOGI(s_tag, "  - star_bus_manager_add_bus");
  ESP_LOGI(s_tag, "  - star_bus_manager_remove_bus");
  ESP_LOGI(s_tag, "  - star_bus_config_init/deinit");

  /* --- Best Practices --- */
  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Initialize buses before creating tasks");
  ESP_LOGI(s_tag, "2. Use callbacks for multi-operation sequences");
  ESP_LOGI(s_tag, "3. Handle ESP_ERR_TIMEOUT for mutex failures");
  ESP_LOGI(s_tag, "4. Minimize time holding bus mutex");
  ESP_LOGI(s_tag, "5. Consider using async operations for long transfers");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nThread safety example complete");
}
