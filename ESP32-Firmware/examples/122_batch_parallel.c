/**
 * @file 122_batch_parallel.c
 * @brief Parallel batch execution with result aggregation
 *
 * Demonstrates:
 * - Multiple operations in parallel
 * - Wait for all to complete
 * - Aggregate results
 */

#include "star_bus_batch.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "BATCH_PARALLEL";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)

#define SENSOR1_ADDR     (0x48)
#define SENSOR2_ADDR     (0x49)
#define SENSOR3_ADDR     (0x4A)
#define EEPROM_ADDR      (0x50)

static star_bus_manager_t g_manager;

static void print_batch_stats(const star_batch_stats_t* stats)
{
    ESP_LOGI(TAG, "Batch Statistics:");
    ESP_LOGI(TAG, "  Executed: %lu", (unsigned long)stats->operations_executed);
    ESP_LOGI(TAG, "  Succeeded: %lu", (unsigned long)stats->operations_succeeded);
    ESP_LOGI(TAG, "  Failed: %lu", (unsigned long)stats->operations_failed);
    ESP_LOGI(TAG, "  Total time: %lu ms", (unsigned long)stats->total_time_ms);
    if (stats->operations_executed > 0) {
        float success_rate = 100.0f * stats->operations_succeeded / stats->operations_executed;
        ESP_LOGI(TAG, "  Success rate: %.1f%%", success_rate);
    }
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Parallel Batch Execution Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "batch_parallel", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create I2C configurations for multiple devices */
    star_bus_config_t* sensor1 = star_bus_config_create_i2c(
        "sensor1", I2C_NUM_0, SENSOR1_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    star_bus_config_t* sensor2 = star_bus_config_create_i2c(
        "sensor2", I2C_NUM_0, SENSOR2_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    star_bus_config_t* sensor3 = star_bus_config_create_i2c(
        "sensor3", I2C_NUM_0, SENSOR3_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    star_bus_config_t* eeprom = star_bus_config_create_i2c(
        "eeprom", I2C_NUM_0, EEPROM_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    star_bus_manager_add_bus(&g_manager, sensor1);
    star_bus_manager_add_bus(&g_manager, sensor2);
    star_bus_manager_add_bus(&g_manager, sensor3);
    star_bus_manager_add_bus(&g_manager, eeprom);

    star_bus_config_init(sensor1, &g_manager);
    star_bus_config_init(sensor2, &g_manager);
    star_bus_config_init(sensor3, &g_manager);
    star_bus_config_init(eeprom, &g_manager);

    ESP_LOGI(TAG, "4 I2C devices initialized");

    /* --- Parallel Read from Multiple Sensors --- */
    ESP_LOGI(TAG, "\n--- Parallel Read from Multiple Sensors ---");

    star_batch_config_t par_config = {
        .timeout_ms = 5000,
        .mode = STAR_BATCH_MODE_PARALLEL,
        .stop_on_error = false,
        .rollback_on_error = false
    };

    star_batch_handle_t batch;
    ret = star_batch_create(&g_manager, &par_config, &batch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create batch");
        return;
    }

    uint8_t sensor1_data[2], sensor2_data[2], sensor3_data[2], eeprom_data[4];

    star_batch_add_i2c_read(batch, "sensor1", sensor1_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor2", sensor2_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor3", sensor3_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "eeprom", eeprom_data, 4, 0x00);

    int64_t start = esp_timer_get_time();
    star_batch_stats_t stats;
    ret = star_batch_execute(batch, &stats);
    int64_t elapsed = esp_timer_get_time() - start;

    ESP_LOGI(TAG, "Parallel batch completed in %lld us", elapsed);
    print_batch_stats(&stats);

    /* Print collected data */
    if (stats.operations_succeeded > 0) {
        ESP_LOGI(TAG, "Sensor data collected:");
        ESP_LOGI(TAG, "  Sensor1: 0x%02X%02X", sensor1_data[0], sensor1_data[1]);
        ESP_LOGI(TAG, "  Sensor2: 0x%02X%02X", sensor2_data[0], sensor2_data[1]);
        ESP_LOGI(TAG, "  Sensor3: 0x%02X%02X", sensor3_data[0], sensor3_data[1]);
        ESP_LOGI(TAG, "  EEPROM: 0x%02X%02X%02X%02X",
                 eeprom_data[0], eeprom_data[1], eeprom_data[2], eeprom_data[3]);
    }

    star_batch_free(batch);

    /* --- Parallel Write Operations --- */
    ESP_LOGI(TAG, "\n--- Parallel Write Operations ---");

    ret = star_batch_create(&g_manager, &par_config, &batch);

    uint8_t write1[] = {0xAA};
    uint8_t write2[] = {0xBB};
    uint8_t write3[] = {0xCC};

    star_batch_add_i2c_write(batch, "sensor1", write1, 1, 0x10);
    star_batch_add_i2c_write(batch, "sensor2", write2, 1, 0x10);
    star_batch_add_i2c_write(batch, "sensor3", write3, 1, 0x10);

    start = esp_timer_get_time();
    ret = star_batch_execute(batch, &stats);
    elapsed = esp_timer_get_time() - start;

    ESP_LOGI(TAG, "Parallel writes completed in %lld us: %s", elapsed, esp_err_to_name(ret));

    star_batch_free(batch);

    /* --- Mixed Read/Write Parallel Operations --- */
    ESP_LOGI(TAG, "\n--- Mixed Read/Write Parallel ---");

    ret = star_batch_create(&g_manager, &par_config, &batch);

    uint8_t mixed_write[] = {0x55};
    uint8_t mixed_read[2];

    star_batch_add_i2c_write(batch, "sensor1", mixed_write, 1, 0x00);
    star_batch_add_i2c_read(batch, "sensor2", mixed_read, 2, 0x00);
    star_batch_add_i2c_write(batch, "sensor3", mixed_write, 1, 0x00);

    ret = star_batch_execute(batch, &stats);
    ESP_LOGI(TAG, "Mixed parallel batch: %s", esp_err_to_name(ret));

    star_batch_free(batch);

    /* --- Compare Sequential vs Parallel Performance --- */
    ESP_LOGI(TAG, "\n--- Performance Comparison ---");

    /* Sequential */
    star_batch_config_t seq_config = {
        .timeout_ms = 5000,
        .mode = STAR_BATCH_MODE_SEQUENTIAL,
        .stop_on_error = false,
        .rollback_on_error = false
    };

    ret = star_batch_create(&g_manager, &seq_config, &batch);
    star_batch_add_i2c_read(batch, "sensor1", sensor1_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor2", sensor2_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor3", sensor3_data, 2, 0x00);

    start = esp_timer_get_time();
    star_batch_execute(batch, NULL);
    int64_t seq_time = esp_timer_get_time() - start;
    star_batch_free(batch);

    /* Parallel */
    ret = star_batch_create(&g_manager, &par_config, &batch);
    star_batch_add_i2c_read(batch, "sensor1", sensor1_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor2", sensor2_data, 2, 0x00);
    star_batch_add_i2c_read(batch, "sensor3", sensor3_data, 2, 0x00);

    start = esp_timer_get_time();
    star_batch_execute(batch, NULL);
    int64_t par_time = esp_timer_get_time() - start;
    star_batch_free(batch);

    ESP_LOGI(TAG, "Sequential time: %lld us", seq_time);
    ESP_LOGI(TAG, "Parallel time: %lld us", par_time);
    if (par_time > 0) {
        ESP_LOGI(TAG, "Speedup: %.2fx", (float)seq_time / par_time);
    }

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nParallel batch example complete");
}
