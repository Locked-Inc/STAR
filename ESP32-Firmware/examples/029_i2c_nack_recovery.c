/**
 * @file 029_i2c_nack_recovery.c
 * @brief NACK error detection and recovery
 *
 * This example demonstrates:
 * - Detecting NACK (Not Acknowledge) conditions
 * - Understanding why NACK occurs
 * - Recovery strategies after NACK
 * - Retry logic with backoff
 * - Distinguishing NACK from other I2C errors
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_NACK";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Test device addresses */
#define DEVICE_ADDR     (0x50)  /* Example EEPROM address */
#define WRONG_ADDR      (0x51)  /* Intentionally wrong address */

/* Clock speed */
#define I2C_CLK_SPEED    (100000)

/* Retry configuration */
#define MAX_RETRIES     (5)
#define RETRY_DELAY_MS  (50)

/**
 * @brief Retry logic with exponential backoff
 */
static esp_err_t priv_i2c_write_with_retry(star_bus_manager_t *manager,
                                           const char *bus_name,
                                           const uint8_t *data,
                                           size_t length,
                                           uint8_t reg,
                                           int max_retries)
{
  esp_err_t ret = ESP_FAIL;
  int retry_count = 0;
  uint32_t delay_ms = RETRY_DELAY_MS;

  while (retry_count < max_retries) {
    size_t bytes_written;
    ret = star_bus_i2c_write(manager, bus_name, data, length, reg, &bytes_written);

    if (ret == ESP_OK) {
      if (retry_count > 0) {
        ESP_LOGI(s_tag, "Write succeeded after %d retries", retry_count);
      }
      return ESP_OK;
    }

    /* Check if error is recoverable */
    if (ret == ESP_ERR_TIMEOUT || ret == ESP_FAIL) {
      retry_count++;
      ESP_LOGW(s_tag, "Write failed (attempt %d/%d): %s",
               retry_count, max_retries, esp_err_to_name(ret));

      if (retry_count < max_retries) {
        ESP_LOGI(s_tag, "Retrying in %u ms...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        /* Exponential backoff */
        delay_ms *= 2;
        if (delay_ms > 1000) {
          delay_ms = 1000;  /* Cap at 1 second */
        }
      }
    } else {
      /* Non-recoverable error */
      ESP_LOGE(s_tag, "Non-recoverable error: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  ESP_LOGE(s_tag, "Write failed after %d retries", max_retries);
  return ret;
}

/**
 * @brief Detect and classify I2C errors
 */
static void priv_classify_i2c_error(esp_err_t error)
{
  ESP_LOGI(s_tag, "\n--- Error Classification ---");
  ESP_LOGI(s_tag, "Error code: %s", esp_err_to_name(error));

  switch (error) {
    case ESP_OK:
      ESP_LOGI(s_tag, "Type: Success (no error)");
      break;

    case ESP_FAIL:
      ESP_LOGI(s_tag, "Type: NACK received");
      ESP_LOGI(s_tag, "Possible causes:");
      ESP_LOGI(s_tag, "  - Device not present at address");
      ESP_LOGI(s_tag, "  - Device not ready (busy with internal operation)");
      ESP_LOGI(s_tag, "  - Invalid register address");
      ESP_LOGI(s_tag, "  - Write protect enabled");
      ESP_LOGI(s_tag, "Recovery: Retry after delay, check address/register");
      break;

    case ESP_ERR_TIMEOUT:
      ESP_LOGI(s_tag, "Type: Timeout");
      ESP_LOGI(s_tag, "Possible causes:");
      ESP_LOGI(s_tag, "  - Clock stretching too long");
      ESP_LOGI(s_tag, "  - Bus stuck (SDA/SCL held low)");
      ESP_LOGI(s_tag, "  - Electrical issue");
      ESP_LOGI(s_tag, "Recovery: Check bus lines, try bus reset");
      break;

    case ESP_ERR_INVALID_STATE:
      ESP_LOGI(s_tag, "Type: Invalid state");
      ESP_LOGI(s_tag, "Possible causes:");
      ESP_LOGI(s_tag, "  - Driver not initialized");
      ESP_LOGI(s_tag, "  - Bus already in use");
      ESP_LOGI(s_tag, "Recovery: Re-initialize driver");
      break;

    default:
      ESP_LOGI(s_tag, "Type: Other error");
      ESP_LOGI(s_tag, "Recovery: Check error code documentation");
      break;
  }
}

static void priv_test_nack_on_address(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 1: NACK on Address Phase ===");
  ESP_LOGI(s_tag, "Attempting to access non-existent device");

  uint8_t data;
  size_t bytes_read;

  esp_err_t ret = star_bus_i2c_read(manager, "nack_test", &data, 1, 0x00, &bytes_read);

  ESP_LOGI(s_tag, "Result: %s", esp_err_to_name(ret));

  if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "EXPECTED: Device NACKed during address phase");
    priv_classify_i2c_error(ret);
  }
}

static void priv_test_nack_on_data(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 2: NACK on Data Phase ===");
  ESP_LOGI(s_tag, "Writing to invalid register address");

  /* Try to write to potentially invalid register */
  uint8_t write_data = 0xAA;
  size_t bytes_written;

  esp_err_t ret = star_bus_i2c_write(manager, "nack_test", &write_data, 1,
                                      0xFF, &bytes_written);  /* 0xFF might be invalid */

  ESP_LOGI(s_tag, "Result: %s", esp_err_to_name(ret));

  if (ret != ESP_OK) {
    ESP_LOGI(s_tag, "Device may have NACKed invalid register address");
    priv_classify_i2c_error(ret);
  }
}

static void priv_test_busy_device_nack(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 3: Busy Device NACK ===");
  ESP_LOGI(s_tag, "Simulating access to busy device (EEPROM during write cycle)");

  /* Write some data */
  uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
  esp_err_t ret = priv_i2c_write_with_retry(manager, "nack_test", write_data,
                                             sizeof(write_data), 0x00, 1);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Initial write successful");

    /* Immediately try to write again - EEPROM might be busy */
    ESP_LOGI(s_tag, "Attempting immediate second write (device may be busy)...");

    uint8_t write_data2[] = {0x55, 0x66};
    size_t bytes_written;
    ret = star_bus_i2c_write(manager, "nack_test", write_data2,
                             sizeof(write_data2), 0x04, &bytes_written);

    if (ret != ESP_OK) {
      ESP_LOGI(s_tag, "Device NACKed - likely busy with previous write");
      ESP_LOGI(s_tag, "This is NORMAL for EEPROM during write cycle");

      /* Retry with delay */
      vTaskDelay(pdMS_TO_TICKS(10));  /* EEPROM write cycle time */
      ret = star_bus_i2c_write(manager, "nack_test", write_data2,
                               sizeof(write_data2), 0x04, &bytes_written);

      if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Write succeeded after waiting for device");
      }
    }
  }
}

static void priv_test_recovery_strategies(star_bus_manager_t *manager)
{
  ESP_LOGI(s_tag, "\n=== Test 4: Recovery Strategies ===");

  /* Strategy 1: Simple retry */
  ESP_LOGI(s_tag, "\nStrategy 1: Simple Retry");
  uint8_t data = 0xAB;
  esp_err_t ret = priv_i2c_write_with_retry(manager, "nack_test", &data, 1,
                                             0x00, 3);
  ESP_LOGI(s_tag, "Simple retry result: %s", esp_err_to_name(ret));

  /* Strategy 2: Exponential backoff (already implemented in i2c_write_with_retry) */
  ESP_LOGI(s_tag, "\nStrategy 2: Exponential Backoff");
  ESP_LOGI(s_tag, "Implemented in retry function - delays: 50ms, 100ms, 200ms...");

  /* Strategy 3: Bus reset (if needed) */
  ESP_LOGI(s_tag, "\nStrategy 3: Bus Reset");
  ESP_LOGI(s_tag, "If repeated NACKs occur, consider:");
  ESP_LOGI(s_tag, "  - Deinit and reinit I2C driver");
  ESP_LOGI(s_tag, "  - Check physical connections");
  ESP_LOGI(s_tag, "  - Verify device power");
  ESP_LOGI(s_tag, "  - Check pull-up resistors");
}

static void priv_i2c_nack_recovery_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_NACK", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Create I2C config */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "nack_test",
    I2C_NUM_0,
    WRONG_ADDR,  /* Using wrong address to trigger NACK */
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
  ESP_LOGI(s_tag, "Testing NACK scenarios with device at 0x%02X", WRONG_ADDR);

  /* Run NACK tests */
  priv_test_nack_on_address(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_nack_on_data(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_busy_device_nack(&manager);
  vTaskDelay(pdMS_TO_TICKS(200));

  priv_test_recovery_strategies(&manager);

  /* Summary */
  ESP_LOGI(s_tag, "\n=== NACK Handling Summary ===");
  ESP_LOGI(s_tag, "NACK indicates:");
  ESP_LOGI(s_tag, "  1. Device not present at address");
  ESP_LOGI(s_tag, "  2. Device busy (e.g., EEPROM writing)");
  ESP_LOGI(s_tag, "  3. Invalid register/command");
  ESP_LOGI(s_tag, "  4. Write protection active");
  ESP_LOGI(s_tag, "\nRecovery strategies:");
  ESP_LOGI(s_tag, "  1. Retry with delay (exponential backoff)");
  ESP_LOGI(s_tag, "  2. Check device address and register");
  ESP_LOGI(s_tag, "  3. Wait for device to complete operation");
  ESP_LOGI(s_tag, "  4. Verify device power and connections");
  ESP_LOGI(s_tag, "  5. Consider bus reset if persistent");

  /* Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "\nNACK recovery example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C NACK Detection and Recovery Example ===\n");
  priv_i2c_nack_recovery_example();
}
