/**
 * @file 078_smbus_timeout.c
 * @brief SMBus timeout requirements (25-35ms) example
 *
 * Demonstrates:
 * - SMBus timeout detection (25-35ms requirement)
 * - Clock low timeout (TLOW:SEXT > 25ms)
 * - Timeout recovery procedures
 * - Bus reset after timeout
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "SMBUS_TIMEOUT";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define DEVICE_ADDR    (0x50)

// SMBus timeout specifications
#define SMBUS_TLOW_SEXT_MIN_MS (25)
#define SMBUS_TIMEOUT_MIN_MS (25)
#define SMBUS_TIMEOUT_TYP_MS (30)
#define SMBUS_TIMEOUT_MAX_MS (35)

static star_bus_manager_t g_manager;

/**
 * @brief Timeout statistics
 */
typedef struct {
  uint32_t total_transactions;
  uint32_t timeout_count;
  uint32_t successful_ops;
} timeout_stats_t;

static timeout_stats_t g_stats = {0};

/**
 * @brief Execute SMBus transaction with timeout monitoring
 */
static esp_err_t execute_transaction(const char *name,
                                     esp_err_t (*operation)(void)) {
  g_stats.total_transactions++;

  int64_t start = esp_timer_get_time();
  esp_err_t ret = operation();
  int64_t elapsed_us = esp_timer_get_time() - start;
  uint32_t elapsed_ms = elapsed_us / 1000;

  if (ret == ESP_ERR_TIMEOUT) {
    g_stats.timeout_count++;
    ESP_LOGW(TAG, "%s: Timeout after %u ms", name, elapsed_ms);
  } else if (ret == ESP_OK) {
    g_stats.successful_ops++;
    ESP_LOGI(TAG, "%s: Success (%u ms)", name, elapsed_ms);
  } else {
    ESP_LOGE(TAG, "%s: Error %s (%u ms)", name, esp_err_to_name(ret), elapsed_ms);
  }

  return ret;
}

/* Operation wrappers for function pointers */
static esp_err_t op_quick_command(void) {
  return star_smbus_quick_command(&g_manager, "smbus_device", DEVICE_ADDR, true);
}

static esp_err_t op_write_byte(void) {
  return star_smbus_write_byte(&g_manager, "smbus_device", DEVICE_ADDR, 0x10, 0xA5);
}

static esp_err_t op_read_byte(void) {
  uint8_t data;
  return star_smbus_read_byte(&g_manager, "smbus_device", DEVICE_ADDR, 0x10, &data);
}

static esp_err_t op_write_word(void) {
  return star_smbus_write_word(&g_manager, "smbus_device", DEVICE_ADDR, 0x20, 0x1234);
}

static esp_err_t op_read_word(void) {
  uint16_t data;
  return star_smbus_read_word(&g_manager, "smbus_device", DEVICE_ADDR, 0x20, &data);
}

static esp_err_t op_block_write(void) {
  uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  return star_smbus_block_write(&g_manager, "smbus_device", DEVICE_ADDR, 0x30,
                                 data, sizeof(data));
}

static esp_err_t op_block_read(void) {
  uint8_t buffer[32];
  uint8_t length;
  return star_smbus_block_read(&g_manager, "smbus_device", DEVICE_ADDR, 0x30,
                                buffer, 32, &length);
}

/**
 * @brief Test SMBus commands with timeout monitoring
 */
static void test_smbus_commands(void) {
  ESP_LOGI(TAG, "\n=== Testing SMBus Commands with Timeout Monitoring ===");

  execute_transaction("Quick Command", op_quick_command);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Write Byte", op_write_byte);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Read Byte", op_read_byte);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Write Word", op_write_word);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Read Word", op_read_word);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Block Write", op_block_write);
  vTaskDelay(pdMS_TO_TICKS(100));

  execute_transaction("Block Read", op_block_read);
}

/**
 * @brief Print timeout statistics
 */
static void print_statistics(void) {
  ESP_LOGI(TAG, "\n=== Timeout Statistics ===");
  ESP_LOGI(TAG, "Total transactions: %u", g_stats.total_transactions);
  ESP_LOGI(TAG, "Successful operations: %u", g_stats.successful_ops);
  ESP_LOGI(TAG, "Timeout occurrences: %u", g_stats.timeout_count);

  if (g_stats.total_transactions > 0) {
    float timeout_rate = (float)g_stats.timeout_count * 100.0f /
                         g_stats.total_transactions;
    ESP_LOGI(TAG, "Timeout rate: %.2f%%", timeout_rate);
  }
}

/**
 * @brief Print recommendations
 */
static void print_recommendations(void) {
  ESP_LOGI(TAG, "\n=== Recommendations ===");

  if (g_stats.timeout_count == 0) {
    ESP_LOGI(TAG, "- No timeouts detected, device is SMBus compliant");
  } else {
    ESP_LOGI(TAG, "- Timeouts detected (%u occurrences)", g_stats.timeout_count);
    ESP_LOGI(TAG, "- Device exceeds 35ms clock low extension");
    ESP_LOGI(TAG, "- Consider using standard I2C instead of SMBus");
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "=== SMBus Timeout Requirements Example ===\n");
  ESP_LOGI(TAG, "SMBus Timeout Specification:");
  ESP_LOGI(TAG, "  Minimum: %d ms", SMBUS_TIMEOUT_MIN_MS);
  ESP_LOGI(TAG, "  Typical: %d ms", SMBUS_TIMEOUT_TYP_MS);
  ESP_LOGI(TAG, "  Maximum: %d ms\n", SMBUS_TIMEOUT_MAX_MS);

  /* Initialize bus manager */
  ret = star_bus_manager_init(&g_manager, "smbus_timeout_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t* smbus_config = star_bus_config_create_i2c(
    "smbus_device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (smbus_config == NULL) {
    ESP_LOGE(TAG, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&g_manager, smbus_config);
  ret = star_bus_config_init(smbus_config, &g_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init SMBus");
    return;
  }

  ESP_LOGI(TAG, "SMBus initialized with %d ms timeout\n", SMBUS_TIMEOUT_TYP_MS);

  /* Reset statistics */
  memset(&g_stats, 0, sizeof(g_stats));

  /* Test SMBus commands */
  test_smbus_commands();

  /* Print statistics */
  print_statistics();

  /* Print recommendations */
  print_recommendations();

  /* Information */
  ESP_LOGI(TAG, "\n=== Timeout Information ===");
  ESP_LOGI(TAG, "SMBus requires devices to complete operations within 25-35ms");
  ESP_LOGI(TAG, "Clock low extension (TLOW:SEXT) must not exceed 25ms cumulative");
  ESP_LOGI(TAG, "Devices exceeding these limits are not SMBus compliant");

  /* Cleanup */
  star_bus_manager_deinit(&g_manager);

  ESP_LOGI(TAG, "\nSMBus timeout example complete");
}
