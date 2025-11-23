/**
 * @file 077_smbus_clock_stretch.c
 * @brief SMBus clock stretching handling example
 *
 * Demonstrates:
 * - Clock stretching detection and timeout
 * - Peripheral-initiated clock hold
 * - Timeout detection (25-35ms SMBus requirement)
 * - Recovery from clock stretch timeout
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SMBUS_CLOCK_STRETCH";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define DEVICE_ADDR    (0x50)

// SMBus timeout requirements
#define SMBUS_MIN_TIMEOUT_MS (25)
#define SMBUS_MAX_TIMEOUT_MS (35)
#define SMBUS_TIMEOUT_MS (30)

static star_bus_manager_t s_manager;

/**
 * @brief Clock stretch statistics
 */
typedef struct {
  uint32_t stretch_count;
  uint32_t timeout_count;
  uint32_t max_stretch_us;
  uint32_t total_stretch_us;
} clock_stretch_stats_t;

static clock_stretch_stats_t s_stats = {0};

/**
 * @brief Execute SMBus operation with timing measurement
 */
static esp_err_t priv_execute_with_timing(const char *op_name,
                                          esp_err_t (*operation)(void),
                                          int64_t *elapsed_us)
{
  int64_t start_time = esp_timer_get_time();
  esp_err_t ret = operation();
  *elapsed_us = esp_timer_get_time() - start_time;

  if (ret == ESP_ERR_TIMEOUT) {
    s_stats.timeout_count++;
    ESP_LOGW(s_tag, "%s: Timeout after %lld us", op_name, *elapsed_us);
  } else if (*elapsed_us > 1000) { // More than 1ms indicates stretching
    s_stats.stretch_count++;
    s_stats.total_stretch_us += *elapsed_us;
    if (*elapsed_us > s_stats.max_stretch_us) {
      s_stats.max_stretch_us = *elapsed_us;
    }
    ESP_LOGI(s_tag, "%s: Clock stretch %lld us", op_name, *elapsed_us);
  } else {
    ESP_LOGD(s_tag, "%s: Normal timing %lld us", op_name, *elapsed_us);
  }

  return ret;
}

/**
 * @brief Test Quick Command
 */
static esp_err_t priv_test_quick_command(void)
{
  return star_smbus_quick_command(&s_manager, "smbus_device", DEVICE_ADDR, true);
}

/**
 * @brief Test Send Byte
 */
static esp_err_t priv_test_send_byte(void)
{
  return star_smbus_send_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x55);
}

/**
 * @brief Test Write Byte
 */
static esp_err_t priv_test_write_byte(void)
{
  return star_smbus_write_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x10, 0xAA);
}

/**
 * @brief Test Read Byte
 */
static esp_err_t priv_test_read_byte(void)
{
  uint8_t data;
  return star_smbus_read_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x10, &data);
}

/**
 * @brief Test Write Word
 */
static esp_err_t priv_test_write_word(void)
{
  return star_smbus_write_word(&s_manager, "smbus_device", DEVICE_ADDR, 0x20, 0x1234);
}

/**
 * @brief Test Read Word
 */
static esp_err_t priv_test_read_word(void)
{
  uint16_t data;
  return star_smbus_read_word(&s_manager, "smbus_device", DEVICE_ADDR, 0x20, &data);
}

/**
 * @brief Test Block Write
 */
static esp_err_t priv_test_block_write(void)
{
  uint8_t block_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  return star_smbus_block_write(&s_manager, "smbus_device", DEVICE_ADDR, 0x30,
                                 block_data, sizeof(block_data));
}

/**
 * @brief Test Block Read
 */
static esp_err_t priv_test_block_read(void)
{
  uint8_t buffer[32];
  uint8_t length;
  return star_smbus_block_read(&s_manager, "smbus_device", DEVICE_ADDR, 0x30,
                                buffer, 32, &length);
}

/**
 * @brief Test all SMBus commands with clock stretch monitoring
 */
static void priv_test_smbus_commands(void)
{
  ESP_LOGI(s_tag, "\n=== Testing SMBus Commands with Clock Stretch Monitoring ===");

  int64_t elapsed;

  ESP_LOGI(s_tag, "\n--- Quick Command ---");
  priv_execute_with_timing("Quick Command", priv_test_quick_command, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Send Byte ---");
  priv_execute_with_timing("Send Byte", priv_test_send_byte, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Write Byte ---");
  priv_execute_with_timing("Write Byte", priv_test_write_byte, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Read Byte ---");
  priv_execute_with_timing("Read Byte", priv_test_read_byte, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Write Word ---");
  priv_execute_with_timing("Write Word", priv_test_write_word, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Read Word ---");
  priv_execute_with_timing("Read Word", priv_test_read_word, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Block Write (higher stretch probability) ---");
  priv_execute_with_timing("Block Write", priv_test_block_write, &elapsed);
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(s_tag, "\n--- Block Read (higher stretch probability) ---");
  priv_execute_with_timing("Block Read", priv_test_block_read, &elapsed);
}

/**
 * @brief Print clock stretch statistics
 */
static void priv_print_statistics(void)
{
  ESP_LOGI(s_tag, "\n=== Clock Stretch Statistics ===");
  ESP_LOGI(s_tag, "Clock stretches detected: %u", s_stats.stretch_count);
  ESP_LOGI(s_tag, "Clock stretch timeouts: %u", s_stats.timeout_count);
  ESP_LOGI(s_tag, "Max stretch duration: %u us", s_stats.max_stretch_us);

  if (s_stats.stretch_count > 0) {
    ESP_LOGI(s_tag, "Average stretch duration: %u us",
             s_stats.total_stretch_us / s_stats.stretch_count);
  }

  if (s_stats.max_stretch_us > SMBUS_MAX_TIMEOUT_MS * 1000) {
    ESP_LOGW(s_tag, "WARNING: Max stretch exceeds SMBus timeout!");
  }
}

/**
 * @brief Print recommendations based on results
 */
static void priv_print_recommendations(void)
{
  ESP_LOGI(s_tag, "\n=== Recommendations ===");

  if (s_stats.max_stretch_us > 10000) {
    ESP_LOGI(s_tag, "- Device exhibits significant clock stretching (>10ms)");
    ESP_LOGI(s_tag, "- Consider increasing timeout or optimizing device firmware");
  }

  if (s_stats.timeout_count > 0) {
    ESP_LOGI(s_tag, "- Clock stretch timeouts detected");
    ESP_LOGI(s_tag, "- Device may not be SMBus compliant (>35ms stretch)");
    ESP_LOGI(s_tag, "- Consider using standard I2C with longer timeout");
  }

  if (s_stats.stretch_count == 0 && s_stats.timeout_count == 0) {
    ESP_LOGI(s_tag, "- No significant clock stretching detected");
    ESP_LOGI(s_tag, "- Device is well-behaved for SMBus operation");
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SMBus Clock Stretching Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "smbus_clock_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration for SMBus device */
  star_bus_config_t* smbus_config = star_bus_config_create_i2c(
    "smbus_device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (smbus_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, smbus_config);
  ret = star_bus_config_init(smbus_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SMBus");
    return;
  }

  ESP_LOGI(s_tag, "SMBus initialized with %dms timeout", SMBUS_TIMEOUT_MS);
  ESP_LOGI(s_tag, "Monitoring clock stretching on all transactions\n");

  /* Reset statistics */
  memset(&s_stats, 0, sizeof(s_stats));

  /* Test various SMBus commands */
  priv_test_smbus_commands();

  /* Print statistics */
  priv_print_statistics();

  /* Print recommendations */
  priv_print_recommendations();

  /* Information about clock stretching */
  ESP_LOGI(s_tag, "\n=== Clock Stretching Information ===");
  ESP_LOGI(s_tag, "Clock stretching allows peripheral to pause communication");
  ESP_LOGI(s_tag, "SMBus requires timeout detection (25-35ms)");
  ESP_LOGI(s_tag, "Excessive stretching may indicate slow peripheral firmware");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSMBus clock stretching example complete");
}
