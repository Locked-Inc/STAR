/**
 * @file 013_bus_statistics.c
 * @brief Bus statistics and monitoring example
 *
 * Demonstrates:
 * - Enabling/disabling statistics
 * - Getting statistics for buses
 * - Snapshots and differentials
 * - Error rate and utilization calculations
 * - JSON export
 * - Throughput monitoring
 */

#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"
#include "star_bus_stats.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BUS_STATS_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (400000)
#define SENSOR_ADDR   (0x48)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Bus Statistics Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "stats_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration */
  star_bus_config_t* i2c_config = star_bus_config_create_i2c(
    "stats_sensor",
    I2C_NUM_0,
    SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (i2c_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  star_bus_manager_add_bus(&s_manager, i2c_config);
  star_bus_config_init(i2c_config, &s_manager);

  ESP_LOGI(s_tag, "I2C bus initialized");

  /* --- Enable Statistics --- */
  ESP_LOGI(s_tag, "\n--- Enabling Statistics ---");

  ret = star_bus_stats_enable(&s_manager, "stats_sensor", NULL);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Statistics enabled for 'stats_sensor'");
  }

  bool enabled;
  ret = star_bus_stats_is_enabled(&s_manager, "stats_sensor", &enabled);
  ESP_LOGI(s_tag, "Statistics enabled: %s", enabled ? "Yes" : "No");

  /* --- Perform Operations --- */
  ESP_LOGI(s_tag, "\n--- Performing Operations ---");

  uint8_t write_data[] = {0x55};
  uint8_t read_buffer[4];
  size_t bytes_read;

  /* Do some I2C operations */
  for (int i = 0; i < 10; i++) {
    star_bus_i2c_write(&s_manager, "stats_sensor", write_data, 1, 0x00, NULL);
    star_bus_i2c_read(&s_manager, "stats_sensor", read_buffer, 4, 0x00, &bytes_read);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Completed 10 write + 10 read operations");

  /* --- Get Statistics --- */
  ESP_LOGI(s_tag, "\n--- Current Statistics ---");

  star_bus_stats_t stats;
  ret = star_bus_stats_get(&s_manager, "stats_sensor", &stats);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Total operations: %lu", (unsigned long)stats.total_operations);
    ESP_LOGI(s_tag, "Read operations: %lu", (unsigned long)stats.read_operations);
    ESP_LOGI(s_tag, "Write operations: %lu", (unsigned long)stats.write_operations);
    ESP_LOGI(s_tag, "Failed: %lu", (unsigned long)stats.failed_operations);
    ESP_LOGI(s_tag, "Bytes written: %lu", (unsigned long)stats.bytes_written);
    ESP_LOGI(s_tag, "Bytes read: %lu", (unsigned long)stats.bytes_read);
    ESP_LOGI(s_tag, "Min time: %lu us", (unsigned long)stats.min_operation_time_us);
    ESP_LOGI(s_tag, "Max time: %lu us", (unsigned long)stats.max_operation_time_us);
    ESP_LOGI(s_tag, "Avg time: %lu us", (unsigned long)stats.avg_operation_time_us);
  }

  /* --- Print Formatted Statistics --- */
  ESP_LOGI(s_tag, "\n--- Formatted Statistics ---");
  star_bus_stats_print("stats_sensor", &stats);

  /* --- Snapshot and Differential --- */
  ESP_LOGI(s_tag, "\n--- Snapshot and Differential ---");

  /* Take a snapshot */
  star_stats_snapshot_t snapshot;
  ret = star_bus_stats_snapshot(&s_manager, "stats_sensor", &snapshot);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Snapshot taken");
  }

  /* Do more operations */
  for (int i = 0; i < 5; i++) {
    star_bus_i2c_read(&s_manager, "stats_sensor", read_buffer, 2, 0x00, &bytes_read);
  }

  /* Calculate differential */
  star_bus_stats_t diff;
  ret = star_bus_stats_diff(&s_manager, "stats_sensor", &snapshot, &diff);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Operations since snapshot: %lu", (unsigned long)diff.total_operations);
    ESP_LOGI(s_tag, "Bytes read since snapshot: %lu", (unsigned long)diff.bytes_read);
  }

  /* --- Calculated Metrics --- */
  ESP_LOGI(s_tag, "\n--- Calculated Metrics ---");

  /* Get current stats for calculations */
  star_bus_stats_get(&s_manager, "stats_sensor", &stats);

  /* Error rate */
  float error_rate = star_bus_stats_error_rate(&stats);
  ESP_LOGI(s_tag, "Error rate: %.2f%%", error_rate);

  /* Throughput */
  uint32_t throughput = star_bus_stats_throughput(&stats);
  ESP_LOGI(s_tag, "Throughput: %lu bytes/sec", (unsigned long)throughput);

  /* Operations per second */
  uint32_t ops_per_sec = star_bus_stats_ops_per_second(&stats);
  ESP_LOGI(s_tag, "Operations/sec: %lu", (unsigned long)ops_per_sec);

  /* Bus utilization */
  float utilization = star_bus_stats_utilization(&stats);
  ESP_LOGI(s_tag, "Bus utilization: %.2f%%", utilization);

  /* --- JSON Export --- */
  ESP_LOGI(s_tag, "\n--- JSON Export ---");

  char json_buffer[512];
  ret = star_bus_stats_to_json(&stats, json_buffer, sizeof(json_buffer));
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "JSON:\n%s", json_buffer);
  }

  /* --- Reset Statistics --- */
  ESP_LOGI(s_tag, "\n--- Reset Statistics ---");

  ret = star_bus_stats_reset(&s_manager, "stats_sensor");
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Statistics reset for 'stats_sensor'");
  }

  /* Verify reset */
  star_bus_stats_get(&s_manager, "stats_sensor", &stats);
  ESP_LOGI(s_tag, "Operations after reset: %lu", (unsigned long)stats.total_operations);

  /* --- Multiple Bus Statistics --- */
  ESP_LOGI(s_tag, "\n--- Multiple Bus Statistics ---");

  /* Create another bus */
  star_bus_config_t* second_config = star_bus_config_create_i2c(
    "second_sensor",
    I2C_NUM_0,
    0x50,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (second_config != NULL) {
    star_bus_manager_add_bus(&s_manager, second_config);
    star_bus_config_init(second_config, &s_manager);
    star_bus_stats_enable(&s_manager, "second_sensor", NULL);

    /* Do operations on both */
    star_bus_i2c_read(&s_manager, "stats_sensor", read_buffer, 2, 0x00, &bytes_read);
    star_bus_i2c_read(&s_manager, "second_sensor", read_buffer, 2, 0x00, &bytes_read);

    /* Print all statistics */
    ESP_LOGI(s_tag, "All bus statistics:");
    star_bus_stats_print_all(&s_manager);

    /* Reset all */
    star_bus_stats_reset_all(&s_manager);
    ESP_LOGI(s_tag, "All statistics reset");
  }

  /* --- Disable Statistics --- */
  ESP_LOGI(s_tag, "\n--- Disabling Statistics ---");

  ret = star_bus_stats_disable(&s_manager, "stats_sensor");
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Statistics disabled for 'stats_sensor'");
  }

  /* Operations won't be tracked now */
  star_bus_i2c_read(&s_manager, "stats_sensor", read_buffer, 2, 0x00, &bytes_read);

  ret = star_bus_stats_is_enabled(&s_manager, "stats_sensor", &enabled);
  ESP_LOGI(s_tag, "Statistics enabled: %s", enabled ? "Yes" : "No");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBus statistics example complete");
}
