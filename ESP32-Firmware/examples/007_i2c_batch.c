/**
 * @file 007_i2c_batch.c
 * @brief Batch I2C operations example
 *
 * Demonstrates:
 * - Creating batch operations
 * - Adding I2C read/write to batch
 * - Executing batches
 * - Getting operation results
 * - Sequential vs parallel modes
 * - Error handling in batches
 */

#include "star_bus_batch.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>

static const char *s_tag = "I2C_BATCH_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define SENSOR_ADDR   (0x48)
#define EEPROM_ADDR   (0x50)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Batch I2C Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "batch_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configurations for two devices */
  star_bus_config_t *sensor_config = star_bus_config_create_i2c(
    "sensor",
    I2C_NUM_0,
    SENSOR_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  star_bus_config_t *eeprom_config = star_bus_config_create_i2c(
    "eeprom",
    I2C_NUM_0,
    EEPROM_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (sensor_config == NULL || eeprom_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create configs");
    return;
  }

  star_bus_manager_add_bus(&s_manager, sensor_config);
  star_bus_manager_add_bus(&s_manager, eeprom_config);
  star_bus_config_init(sensor_config, &s_manager);
  star_bus_config_init(eeprom_config, &s_manager);

  ESP_LOGI(s_tag, "I2C buses initialized");

  /* --- Basic Batch Operations --- */
  ESP_LOGI(s_tag, "\n--- Basic Batch ---");

  /* Create batch with default config */
  star_batch_config_t batch_config = STAR_BATCH_CONFIG_DEFAULT();
  star_batch_handle_t batch;
  ret = star_batch_create(&s_manager, &batch_config, &batch);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create batch");
    return;
  }

  /* Add write operations */
  uint8_t write_data1[] = {0x55};
  uint8_t write_data2[] = {0xAA, 0xBB};

  ret = star_batch_add_i2c_write(batch, "sensor", write_data1, 1, 0x00);
  ESP_LOGI(s_tag, "Added sensor write: %s", esp_err_to_name(ret));

  ret = star_batch_add_i2c_write(batch, "eeprom", write_data2, 2, 0x10);
  ESP_LOGI(s_tag, "Added eeprom write: %s", esp_err_to_name(ret));

  /* Add read operations */
  uint8_t read_buffer1[2];
  uint8_t read_buffer2[4];

  ret = star_batch_add_i2c_read(batch, "sensor", read_buffer1, 2, 0x00);
  ESP_LOGI(s_tag, "Added sensor read: %s", esp_err_to_name(ret));

  ret = star_batch_add_i2c_read(batch, "eeprom", read_buffer2, 4, 0x10);
  ESP_LOGI(s_tag, "Added eeprom read: %s", esp_err_to_name(ret));

  /* Add delay between operations */
  ret = star_batch_add_delay(batch, 10);  /* 10ms delay */
  ESP_LOGI(s_tag, "Added delay: %s", esp_err_to_name(ret));

  uint32_t op_count;
  star_batch_get_operation_count(batch, &op_count);
  ESP_LOGI(s_tag, "Total operations: %d", (int)op_count);

  /* Execute batch */
  ESP_LOGI(s_tag, "\nExecuting batch...");
  star_batch_stats_t batch_stats;
  ret = star_batch_execute(batch, &batch_stats);
  ESP_LOGI(s_tag, "Batch execution: %s", esp_err_to_name(ret));

  /* Check individual results */
  for (uint32_t i = 0; i < op_count; i++) {
    esp_err_t op_result;
    star_batch_get_operation_result(batch, i, &op_result);
    ESP_LOGI(s_tag, "Operation %d result: %s", (int)i, esp_err_to_name(op_result));
  }

  /* Show read data */
  ESP_LOGI(s_tag, "Sensor data: 0x%02X 0x%02X", read_buffer1[0], read_buffer1[1]);
  ESP_LOGI(s_tag, "EEPROM data: 0x%02X 0x%02X 0x%02X 0x%02X",
           read_buffer2[0], read_buffer2[1], read_buffer2[2], read_buffer2[3]);

  star_batch_free(batch);

  /* --- Sequential Mode with Stop on Error --- */
  ESP_LOGI(s_tag, "\n--- Sequential Mode ---");

  star_batch_config_t seq_config = {
    .timeout_ms        = STAR_BATCH_DEFAULT_TIMEOUT_MS,
    .mode              = STAR_BATCH_MODE_SEQUENTIAL,
    .stop_on_error     = true,
    .rollback_on_error = false
  };

  ret = star_batch_create(&s_manager, &seq_config, &batch);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create sequential batch");
    return;
  }

  uint8_t seq_data[3];
  star_batch_add_i2c_read(batch, "sensor", seq_data, 1, 0x00);
  star_batch_add_i2c_read(batch, "sensor", seq_data + 1, 1, 0x01);
  star_batch_add_i2c_read(batch, "sensor", seq_data + 2, 1, 0x02);

  ret = star_batch_execute(batch, NULL);
  ESP_LOGI(s_tag, "Sequential batch: %s", esp_err_to_name(ret));
  ESP_LOGI(s_tag, "Read registers: 0x%02X 0x%02X 0x%02X",
           seq_data[0], seq_data[1], seq_data[2]);

  star_batch_free(batch);

  /* --- Parallel Mode --- */
  ESP_LOGI(s_tag, "\n--- Parallel Mode ---");

  star_batch_config_t par_config = {
    .timeout_ms        = STAR_BATCH_DEFAULT_TIMEOUT_MS,
    .mode              = STAR_BATCH_MODE_PARALLEL,
    .stop_on_error     = false,
    .rollback_on_error = false
  };

  ret = star_batch_create(&s_manager, &par_config, &batch);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create parallel batch");
    return;
  }

  uint8_t par_data1[2], par_data2[2];
  star_batch_add_i2c_read(batch, "sensor", par_data1, 2, 0x00);
  star_batch_add_i2c_read(batch, "eeprom", par_data2, 2, 0x00);

  ret = star_batch_execute(batch, NULL);
  ESP_LOGI(s_tag, "Parallel batch: %s", esp_err_to_name(ret));

  star_batch_free(batch);

  /* --- Reusing Batch --- */
  ESP_LOGI(s_tag, "\n--- Reusing Batch ---");

  ret = star_batch_create(&s_manager, &batch_config, &batch);
  uint8_t reuse_data[2];

  /* First use */
  star_batch_add_i2c_read(batch, "sensor", reuse_data, 2, 0x00);
  star_batch_execute(batch, NULL);
  ESP_LOGI(s_tag, "First execution: 0x%02X 0x%02X", reuse_data[0], reuse_data[1]);

  /* Clear and reuse */
  star_batch_clear(batch);

  star_batch_add_i2c_read(batch, "sensor", reuse_data, 2, 0x10);
  star_batch_execute(batch, NULL);
  ESP_LOGI(s_tag, "Second execution: 0x%02X 0x%02X", reuse_data[0],
           reuse_data[1]);

  star_batch_free(batch);

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBatch I2C example complete");
}
