/**
 * @file 121_batch_sequential.c
 * @brief Sequential batch operations with error handling and rollback
 *
 * Demonstrates:
 * - Create batch with multiple I2C/SPI operations
 * - Execute sequentially with error handling
 * - Show rollback on failure
 */

#include "star_bus_batch.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char* TAG = "BATCH_SEQUENTIAL";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)

#define SENSOR_ADDR      (0x48)
#define EEPROM_ADDR      (0x50)

static star_bus_manager_t g_manager;

/* Rollback state for recovery */
typedef struct {
    uint8_t original_values[8];
    uint8_t registers[8];
    size_t count;
    bool valid;
} rollback_state_t;

static rollback_state_t g_rollback;

/* Save state before modification for rollback */
static esp_err_t save_rollback_state(star_bus_manager_t* manager, const char* bus_name,
                                      uint8_t reg, size_t len)
{
    if (g_rollback.count + len > sizeof(g_rollback.original_values)) {
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read;
    esp_err_t ret = star_bus_i2c_read(manager, bus_name,
                                       &g_rollback.original_values[g_rollback.count],
                                       len, reg, &bytes_read);
    if (ret == ESP_OK) {
        for (size_t i = 0; i < len; i++) {
            g_rollback.registers[g_rollback.count + i] = reg + i;
        }
        g_rollback.count += len;
        g_rollback.valid = true;
    }
    return ret;
}

/* Perform rollback of all saved state */
static esp_err_t perform_rollback(star_bus_manager_t* manager, const char* bus_name)
{
    if (!g_rollback.valid) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Performing rollback of %zu bytes", g_rollback.count);

    for (size_t i = 0; i < g_rollback.count; i++) {
        size_t bytes_written;
        esp_err_t ret = star_bus_i2c_write(manager, bus_name,
                                            &g_rollback.original_values[i], 1,
                                            g_rollback.registers[i], &bytes_written);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Rollback failed at register 0x%02X", g_rollback.registers[i]);
            return ret;
        }
    }

    ESP_LOGI(TAG, "Rollback completed successfully");
    g_rollback.valid = false;
    g_rollback.count = 0;
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Sequential Batch Operations Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "batch_seq", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create I2C configurations */
    star_bus_config_t* sensor_config = star_bus_config_create_i2c(
        "sensor", I2C_NUM_0, SENSOR_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    star_bus_config_t* eeprom_config = star_bus_config_create_i2c(
        "eeprom", I2C_NUM_0, EEPROM_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    if (sensor_config == NULL || eeprom_config == NULL) {
        ESP_LOGE(TAG, "Failed to create configs");
        return;
    }

    star_bus_manager_add_bus(&g_manager, sensor_config);
    star_bus_manager_add_bus(&g_manager, eeprom_config);
    star_bus_config_init(sensor_config, &g_manager);
    star_bus_config_init(eeprom_config, &g_manager);

    /* --- Sequential Batch with Stop-on-Error --- */
    ESP_LOGI(TAG, "\n--- Sequential Batch with Stop-on-Error ---");

    star_batch_config_t seq_config = {
        .timeout_ms = 5000,
        .mode = STAR_BATCH_MODE_SEQUENTIAL,
        .stop_on_error = true,
        .rollback_on_error = false
    };

    star_batch_handle_t batch;
    ret = star_batch_create(&g_manager, &seq_config, &batch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create batch");
        return;
    }

    /* Add sequential operations */
    uint8_t write_data1[] = {0x01};
    uint8_t write_data2[] = {0x02};
    uint8_t read_buf1[2], read_buf2[2];

    /* Operation 0: Write to sensor */
    star_batch_add_i2c_write(batch, "sensor", write_data1, 1, 0x00);
    /* Operation 1: Read back from sensor */
    star_batch_add_i2c_read(batch, "sensor", read_buf1, 2, 0x00);
    /* Operation 2: Write to eeprom */
    star_batch_add_i2c_write(batch, "eeprom", write_data2, 1, 0x10);
    /* Operation 3: Read back from eeprom */
    star_batch_add_i2c_read(batch, "eeprom", read_buf2, 2, 0x10);

    uint32_t op_count;
    star_batch_get_operation_count(batch, &op_count);
    ESP_LOGI(TAG, "Batch has %lu operations", (unsigned long)op_count);

    int64_t start_time = esp_timer_get_time();
    star_batch_stats_t stats;
    ret = star_batch_execute(batch, &stats);
    int64_t elapsed = esp_timer_get_time() - start_time;

    ESP_LOGI(TAG, "Batch execution: %s (took %lld us)", esp_err_to_name(ret), elapsed);
    ESP_LOGI(TAG, "Stats: executed=%lu, succeeded=%lu, failed=%lu",
             (unsigned long)stats.operations_executed,
             (unsigned long)stats.operations_succeeded,
             (unsigned long)stats.operations_failed);

    /* Check individual operation results */
    for (uint32_t i = 0; i < op_count; i++) {
        esp_err_t op_ret;
        star_batch_get_operation_result(batch, i, &op_ret);
        ESP_LOGI(TAG, "Operation %lu result: %s", (unsigned long)i, esp_err_to_name(op_ret));
    }

    star_batch_free(batch);

    /* --- Sequential with Manual Rollback --- */
    ESP_LOGI(TAG, "\n--- Sequential with Manual Rollback ---");

    memset(&g_rollback, 0, sizeof(g_rollback));

    /* Save original state before modifications */
    save_rollback_state(&g_manager, "sensor", 0x00, 1);

    ret = star_batch_create(&g_manager, &seq_config, &batch);

    uint8_t mod_data[] = {0xAA};
    star_batch_add_i2c_write(batch, "sensor", mod_data, 1, 0x00);

    ret = star_batch_execute(batch, &stats);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Batch failed, initiating rollback...");
        perform_rollback(&g_manager, "sensor");
    } else {
        ESP_LOGI(TAG, "Batch succeeded - no rollback needed");
        g_rollback.valid = false;
    }

    star_batch_free(batch);

    /* --- Multi-Step Transaction Pattern --- */
    ESP_LOGI(TAG, "\n--- Multi-Step Transaction Pattern ---");

    bool transaction_success = true;
    uint8_t step_data[4] = {0x11, 0x22, 0x33, 0x44};

    for (int step = 0; step < 4; step++) {
        ret = star_batch_create(&g_manager, &seq_config, &batch);
        star_batch_add_i2c_write(batch, "sensor", &step_data[step], 1, step);

        ret = star_batch_execute(batch, NULL);
        star_batch_free(batch);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Transaction failed at step %d", step);
            transaction_success = false;
            break;
        }
        ESP_LOGI(TAG, "Step %d completed", step);
    }

    ESP_LOGI(TAG, "Transaction %s", transaction_success ? "COMMITTED" : "ABORTED");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nSequential batch example complete");
}
