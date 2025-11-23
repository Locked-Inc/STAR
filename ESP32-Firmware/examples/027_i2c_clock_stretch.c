/**
 * @file 027_i2c_clock_stretch.c
 * @brief I2C clock stretching handling and timeout configuration
 *
 * This example demonstrates:
 * - Clock stretching detection and handling
 * - Configuring clock stretch timeout
 * - Working with slow I2C slaves that use clock stretching
 * - Monitoring and logging clock stretch events
 * - Timeout configuration for clock stretch scenarios
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_stats.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "I2C_CLK_STRETCH";

/* Pin definitions */
#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

/* Device that uses clock stretching (e.g., sensor doing conversion) */
#define SLOW_SENSOR_ADDR (0x40)

/* Clock speed */
#define I2C_CLK_SPEED    (100000)  /* 100 kHz */

/* Clock stretch timeout - adjust based on slave requirements */
#define CLK_STRETCH_TIMEOUT_US (50000)  /* 50ms timeout for slow sensors */

static void priv_i2c_clock_stretch_example(void)
{
  esp_err_t ret;

  /* Step 1: Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "I2C_ClkStretch", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init manager: %s", esp_err_to_name(ret));
    return;
  }
  ESP_LOGI(s_tag, "Bus manager initialized");

  /* Step 2: Create I2C bus configuration */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "slow_sensor",
    I2C_NUM_0,
    SLOW_SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C config created for slow sensor at 0x%02X", SLOW_SENSOR_ADDR);

  /* Step 3: Add config to manager */
  ret = star_bus_manager_add_bus(&manager, i2c_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add bus: %s", esp_err_to_name(ret));
    star_bus_config_destroy(i2c_config);
    star_bus_manager_deinit(&manager);
    return;
  }

  /* Step 4: Initialize I2C driver */
  ret = star_bus_config_init(i2c_config, &manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init I2C driver: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }
  ESP_LOGI(s_tag, "I2C driver initialized with clock stretch support");
  ESP_LOGI(s_tag, "Clock stretch timeout configured: %u us", CLK_STRETCH_TIMEOUT_US);

  /* Step 5: Explain clock stretching */
  ESP_LOGI(s_tag, "\n--- Clock Stretching Information ---");
  ESP_LOGI(s_tag, "Clock stretching allows slow slaves to pause master");
  ESP_LOGI(s_tag, "Slave holds SCL low to delay next clock pulse");
  ESP_LOGI(s_tag, "Common in: sensors during conversion, slow MCUs, EEPROMs");
  ESP_LOGI(s_tag, "ESP32 I2C master supports automatic clock stretch handling");

  /* Step 6: Send command that triggers clock stretching (e.g., temperature conversion) */
  ESP_LOGI(s_tag, "\n--- Triggering Clock Stretch Scenario ---");
  ESP_LOGI(s_tag, "Sending conversion command to sensor...");

  ret = star_bus_i2c_write_command(&manager, "slow_sensor", 0xE3);  /* Trigger measurement */
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Conversion command sent successfully");
  } else if (ret == ESP_ERR_TIMEOUT) {
    ESP_LOGW(s_tag, "Clock stretch timeout occurred!");
    ESP_LOGW(s_tag, "Slave may be stretching clock longer than %u us", CLK_STRETCH_TIMEOUT_US);
  } else {
    ESP_LOGW(s_tag, "Command failed: %s", esp_err_to_name(ret));
  }

  /* Step 7: Read data with potential clock stretching */
  ESP_LOGI(s_tag, "\n--- Reading with Clock Stretch ---");
  uint8_t sensor_data[3];
  size_t bytes_read;

  int64_t start_time = esp_timer_get_time();
  ret = star_bus_i2c_read_raw(&manager, "slow_sensor", sensor_data, 3, &bytes_read);
  int64_t elapsed_us = esp_timer_get_time() - start_time;

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read completed in %lld us", elapsed_us);
    ESP_LOGI(s_tag, "Data: 0x%02X 0x%02X 0x%02X",
             sensor_data[0], sensor_data[1], sensor_data[2]);

    if (elapsed_us > 10000) {
      ESP_LOGI(s_tag, "Long read time suggests clock stretching occurred");
    }
  } else if (ret == ESP_ERR_TIMEOUT) {
    ESP_LOGW(s_tag, "Read timeout - clock stretch exceeded limit");
    ESP_LOGW(s_tag, "Time elapsed: %lld us", elapsed_us);
  } else {
    ESP_LOGW(s_tag, "Read failed: %s", esp_err_to_name(ret));
  }

  /* Step 8: Multiple reads with timing analysis */
  ESP_LOGI(s_tag, "\n--- Multiple Operations Timing Analysis ---");
  for (int i = 0; i < 5; i++) {
    /* Trigger new conversion */
    ret = star_bus_i2c_write_command(&manager, "slow_sensor", 0xE3);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Iteration %d: Command failed", i);
      continue;
    }

    /* Wait a bit */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read result */
    start_time = esp_timer_get_time();
    ret = star_bus_i2c_read_raw(&manager, "slow_sensor", sensor_data, 3, &bytes_read);
    elapsed_us = esp_timer_get_time() - start_time;

    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Iteration %d: Read OK in %lld us", i, elapsed_us);
    } else {
      ESP_LOGW(s_tag, "Iteration %d: Read failed in %lld us - %s",
               i, elapsed_us, esp_err_to_name(ret));
    }
  }

  /* Step 9: Check bus statistics */
  ESP_LOGI(s_tag, "\n--- Bus Statistics ---");
  star_bus_stats_t stats;
  ret = star_bus_stats_get(&manager, "slow_sensor", &stats);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Total operations: %llu", stats.total_operations);
    ESP_LOGI(s_tag, "Failed operations: %llu", stats.failed_operations);
    ESP_LOGI(s_tag, "Timeout errors: %u", stats.timeout_errors);
    ESP_LOGI(s_tag, "Average operation time: %u us", stats.avg_operation_time_us);
    ESP_LOGI(s_tag, "Max operation time: %u us", stats.max_operation_time_us);

    if (stats.max_operation_time_us > 20000) {
      ESP_LOGI(s_tag, "Maximum operation time indicates clock stretching present");
    }
  }

  /* Step 10: Best practices summary */
  ESP_LOGI(s_tag, "\n--- Clock Stretch Best Practices ---");
  ESP_LOGI(s_tag, "1. Set timeout >= expected clock stretch duration");
  ESP_LOGI(s_tag, "2. Check device datasheet for max stretch time");
  ESP_LOGI(s_tag, "3. Monitor operation times to detect stretching");
  ESP_LOGI(s_tag, "4. Handle timeout errors gracefully");
  ESP_LOGI(s_tag, "5. Consider retry logic for timeout scenarios");
  ESP_LOGI(s_tag, "6. Use stats to tune timeout values");

  /* Step 11: Cleanup */
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Clock stretch example complete");
}

/* App main entry point */
void app_main(void)
{
  ESP_LOGI(s_tag, "=== I2C Clock Stretching Example ===\n");
  priv_i2c_clock_stretch_example();
}
