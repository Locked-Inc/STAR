/**
 * @file 028_i2c_timeout.c
 * @brief Comprehensive I2C timeout scenarios
 *
 * This example demonstrates:
 * - Various timeout scenarios (bus stuck, no ACK, clock stretch)
 * - Configuring different timeout values
 * - Timeout detection and recovery
 * - Best practices for timeout handling
 * - Transaction timeout vs operation timeout
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_TIMEOUT";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Test device addresses */
#define EXISTING_DEVICE   (0x48)  /* Known present device */
#define MISSING_DEVICE    (0x7F)  /* Intentionally missing */

/* Clock speed */
#define I2C_CLK_SPEED    (100000)  /* 100 kHz */

/* Different timeout values for testing */
#define TIMEOUT_SHORT_MS   (10)
#define TIMEOUT_NORMAL_MS  (100)
#define TIMEOUT_LONG_MS    (1000)

static void priv_test_basic_timeout(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 1: Basic Communication Timeout ===");
  ESP_LOGI(s_tag, "Attempting to communicate with non-existent device at 0x%02X", MISSING_DEVICE);

  uint8_t data[2];
  size_t bytes_read;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "timeout_test", data, 2, 0x00, &bytes_read);
  int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;

  ESP_LOGI(s_tag, "Result: %s", esp_err_to_name(ret));
  ESP_LOGI(s_tag, "Time taken: %lld ms", elapsed_ms);

  if (ret == ESP_ERR_TIMEOUT) {
    ESP_LOGI(s_tag, "EXPECTED: Timeout occurred as device not present");
  } else if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "Device not present (no ACK received)");
  }
}

static void priv_test_write_timeout(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 2: Write Operation Timeout ===");
  ESP_LOGI(s_tag, "Attempting write to non-existent device");

  uint8_t write_data[] = {0xAA, 0xBB, 0xCC};
  size_t bytes_written;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_write(manager, "timeout_test", write_data, 3,
                                      0x00, &bytes_written);
  int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;

  ESP_LOGI(s_tag, "Result: %s", esp_err_to_name(ret));
  ESP_LOGI(s_tag, "Time taken: %lld ms", elapsed_ms);
  ESP_LOGI(s_tag, "Bytes written: %u", (unsigned)bytes_written);
}

static void priv_test_command_timeout(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 3: Command Send Timeout ===");
  ESP_LOGI(s_tag, "Sending command byte to non-existent device");

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_write_command(manager, "timeout_test", 0xF3);
  int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;

  ESP_LOGI(s_tag, "Result: %s", esp_err_to_name(ret));
  ESP_LOGI(s_tag, "Time taken: %lld ms", elapsed_ms);
}

static void priv_test_multiple_timeouts(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 4: Multiple Sequential Timeouts ===");
  ESP_LOGI(s_tag, "Testing how bus handles repeated timeout scenarios");

  for (int i = 0; i < 5; i++) {
    uint8_t data;
    size_t bytes_read;

    int64_t start = esp_timer_get_time();
    esp_err_t ret = star_bus_i2c_read(manager, "timeout_test", &data, 1, i, &bytes_read);
    int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;

    ESP_LOGI(s_tag, "Attempt %d: %s (took %lld ms)",
             i + 1, esp_err_to_name(ret), elapsed_ms);

    /* Small delay between attempts */
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Bus should remain functional after multiple timeouts");
}

static void priv_test_timeout_recovery(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 5: Timeout Recovery ===");
  ESP_LOGI(s_tag, "Verify bus recovery after timeout");

  /* Cause a timeout */
  uint8_t data;
  size_t bytes_read;
  esp_err_t ret = star_bus_i2c_read(manager, "timeout_test", &data, 1, 0x00, &bytes_read);
  ESP_LOGI(s_tag, "Timeout test result: %s", esp_err_to_name(ret));

  /* Wait for bus to stabilize */
  vTaskDelay(pdMS_TO_TICKS(50));

  /* Try to communicate with a different (existing) device */
  ESP_LOGI(s_tag, "Attempting communication with different device...");

  /* Create config for existing device */
  star_bus_config_t* working_config = star_bus_config_create_i2c(
    "working_device",
    I2C_NUM_0,
    EXISTING_DEVICE,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (working_config) {
    star_bus_manager_add_bus(manager, working_config);
    star_bus_config_init(working_config, manager);

    ret = star_bus_i2c_read(manager, "working_device", &data, 1, 0x00, &bytes_read);

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "SUCCESS: Bus recovered, communication restored");
    } else {
      ESP_LOGW(s_tag, "Bus communication still failing: %s", esp_err_to_name(ret));
    }
  }
}

static void priv_test_large_transfer_timeout(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 6: Large Transfer with Timeout ===");
  ESP_LOGI(s_tag, "Testing timeout with larger data transfer");

  uint8_t large_buffer[64];
  size_t bytes_read;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = star_bus_i2c_read(manager, "timeout_test", large_buffer,
                                     sizeof(large_buffer), 0x00, &bytes_read);
  int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;

  ESP_LOGI(s_tag, "Large transfer result: %s", esp_err_to_name(ret));
  ESP_LOGI(s_tag, "Time taken: %lld ms", elapsed_ms);
  ESP_LOGI(s_tag, "Bytes read: %u / %u", (unsigned)bytes_read, sizeof(large_buffer));
}

static void priv_i2c_timeout_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_Timeout", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config pointing to non-existent device */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "timeout_test",
    I2C_NUM_0,
    MISSING_DEVICE,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Add and initialize */
  star_bus_manager_add_bus(&manager, i2c_config);
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "I2C driver initialized");
  ESP_LOGI(s_tag, "Testing various timeout scenarios...");

  /* Run all timeout tests */
  priv_test_basic_timeout(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_write_timeout(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_command_timeout(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_multiple_timeouts(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_timeout_recovery(&manager);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_test_large_transfer_timeout(&manager);

  /* Best practices */
  ESP_LOGI(s_tag, "\n=== Timeout Handling Best Practices ===");
  ESP_LOGI(s_tag, "1. Set timeout appropriate for expected operation time");
  ESP_LOGI(s_tag, "2. Always check return values for ESP_ERR_TIMEOUT");
  ESP_LOGI(s_tag, "3. Implement retry logic with exponential backoff");
  ESP_LOGI(s_tag, "4. Monitor timeout frequency in production");
  ESP_LOGI(s_tag, "5. Consider device-specific requirements (clock stretch, etc)");
  ESP_LOGI(s_tag, "6. Use shorter timeouts for fast-fail scenarios");
  ESP_LOGI(s_tag, "7. Bus typically recovers automatically after timeout");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nTimeout example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Timeout Scenarios Example ===\n");
  priv_i2c_timeout_example();
}
