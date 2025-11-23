/**
 * @file 123_batch_rollback.c
 * @brief Batch operations with rollback on error
 *
 * Demonstrates:
 * - Transaction-like behavior
 * - Undo successful operations on failure
 * - State recovery
 */

#include "star_bus_batch.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <string.h>

static const char* TAG = "BATCH_ROLLBACK";

#define I2C_SDA_PIN      (GPIO_NUM_21)
#define I2C_SCL_PIN      (GPIO_NUM_22)
#define EEPROM_ADDR      (0x50)

static star_bus_manager_t g_manager;

/* Transaction journal for rollback */
#define MAX_JOURNAL_ENTRIES (16)

typedef struct {
    uint8_t reg;
    uint8_t old_value;
    uint8_t new_value;
    bool written;
} journal_entry_t;

typedef struct {
    journal_entry_t entries[MAX_JOURNAL_ENTRIES];
    size_t count;
    bool active;
} transaction_journal_t;

static transaction_journal_t g_journal;

/* Begin transaction */
static void transaction_begin(void)
{
    memset(&g_journal, 0, sizeof(g_journal));
    g_journal.active = true;
    ESP_LOGI(TAG, "Transaction started");
}

/* Add entry to journal */
static esp_err_t transaction_journal_add(uint8_t reg, uint8_t old_val, uint8_t new_val)
{
    if (!g_journal.active) return ESP_ERR_INVALID_STATE;
    if (g_journal.count >= MAX_JOURNAL_ENTRIES) return ESP_ERR_NO_MEM;

    journal_entry_t* entry = &g_journal.entries[g_journal.count++];
    entry->reg = reg;
    entry->old_value = old_val;
    entry->new_value = new_val;
    entry->written = false;

    return ESP_OK;
}

/* Mark entry as written */
static void transaction_mark_written(size_t index)
{
    if (index < g_journal.count) {
        g_journal.entries[index].written = true;
    }
}

/* Rollback all written entries */
static esp_err_t transaction_rollback(void)
{
    if (!g_journal.active) return ESP_ERR_INVALID_STATE;

    ESP_LOGW(TAG, "Rolling back %zu journal entries...", g_journal.count);

    /* Rollback in reverse order */
    for (int i = g_journal.count - 1; i >= 0; i--) {
        journal_entry_t* entry = &g_journal.entries[i];
        if (entry->written) {
            size_t bytes_written;
            esp_err_t ret = star_bus_i2c_write(&g_manager, "eeprom",
                                                &entry->old_value, 1,
                                                entry->reg, &bytes_written);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  Restored reg 0x%02X: 0x%02X -> 0x%02X",
                         entry->reg, entry->new_value, entry->old_value);
            } else {
                ESP_LOGE(TAG, "  Failed to restore reg 0x%02X", entry->reg);
            }
        }
    }

    g_journal.active = false;
    return ESP_OK;
}

/* Commit transaction */
static void transaction_commit(void)
{
    ESP_LOGI(TAG, "Transaction committed (%zu changes)", g_journal.count);
    g_journal.active = false;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Batch Rollback Example ===\n");

    /* Initialize bus manager */
    ret = star_bus_manager_init(&g_manager, "rollback_demo", NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bus manager");
        return;
    }

    /* Create I2C configuration for EEPROM */
    star_bus_config_t* eeprom_config = star_bus_config_create_i2c(
        "eeprom", I2C_NUM_0, EEPROM_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 100000);

    if (eeprom_config == NULL) {
        ESP_LOGE(TAG, "Failed to create config");
        return;
    }

    star_bus_manager_add_bus(&g_manager, eeprom_config);
    star_bus_config_init(eeprom_config, &g_manager);

    /* --- Batch with Built-in Rollback --- */
    ESP_LOGI(TAG, "\n--- Batch with Rollback on Error ---");

    star_batch_config_t rollback_config = {
        .timeout_ms = 5000,
        .mode = STAR_BATCH_MODE_SEQUENTIAL,
        .stop_on_error = true,
        .rollback_on_error = true
    };

    star_batch_handle_t batch;
    ret = star_batch_create(&g_manager, &rollback_config, &batch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create batch");
        return;
    }

    uint8_t data1[] = {0x11};
    uint8_t data2[] = {0x22};
    uint8_t data3[] = {0x33};

    star_batch_add_i2c_write(batch, "eeprom", data1, 1, 0x00);
    star_batch_add_i2c_write(batch, "eeprom", data2, 1, 0x01);
    star_batch_add_i2c_write(batch, "eeprom", data3, 1, 0x02);

    star_batch_stats_t stats;
    ret = star_batch_execute(batch, &stats);
    ESP_LOGI(TAG, "Batch with rollback: %s", esp_err_to_name(ret));

    star_batch_free(batch);

    /* --- Manual Transaction with Journal --- */
    ESP_LOGI(TAG, "\n--- Manual Transaction with Journal ---");

    /* Read current values */
    uint8_t current_values[4];
    size_t bytes_read;
    ret = star_bus_i2c_read(&g_manager, "eeprom", current_values, 4, 0x10, &bytes_read);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current values at 0x10-0x13: %02X %02X %02X %02X",
                 current_values[0], current_values[1], current_values[2], current_values[3]);
    }

    /* Begin transaction */
    transaction_begin();

    /* Journal our intended changes */
    uint8_t new_values[] = {0xAA, 0xBB, 0xCC, 0xDD};
    for (int i = 0; i < 4; i++) {
        transaction_journal_add(0x10 + i, current_values[i], new_values[i]);
    }

    /* Execute writes with journal tracking */
    bool success = true;
    for (size_t i = 0; i < g_journal.count && success; i++) {
        size_t written;
        ret = star_bus_i2c_write(&g_manager, "eeprom",
                                  &g_journal.entries[i].new_value, 1,
                                  g_journal.entries[i].reg, &written);
        if (ret == ESP_OK) {
            transaction_mark_written(i);
            ESP_LOGI(TAG, "Wrote 0x%02X to reg 0x%02X",
                     g_journal.entries[i].new_value, g_journal.entries[i].reg);
        } else {
            ESP_LOGE(TAG, "Write failed at reg 0x%02X", g_journal.entries[i].reg);
            success = false;
        }
    }

    if (success) {
        transaction_commit();
    } else {
        transaction_rollback();
    }

    /* --- Simulated Failure with Rollback --- */
    ESP_LOGI(TAG, "\n--- Simulated Failure Scenario ---");

    /* Read current state */
    ret = star_bus_i2c_read(&g_manager, "eeprom", current_values, 4, 0x20, &bytes_read);
    ESP_LOGI(TAG, "Before: 0x%02X 0x%02X 0x%02X 0x%02X",
             current_values[0], current_values[1], current_values[2], current_values[3]);

    transaction_begin();

    /* Write first two successfully */
    uint8_t val1 = 0x11, val2 = 0x22;
    transaction_journal_add(0x20, current_values[0], val1);
    transaction_journal_add(0x21, current_values[1], val2);

    size_t written;
    star_bus_i2c_write(&g_manager, "eeprom", &val1, 1, 0x20, &written);
    transaction_mark_written(0);
    star_bus_i2c_write(&g_manager, "eeprom", &val2, 1, 0x21, &written);
    transaction_mark_written(1);

    /* Simulate failure on third write */
    ESP_LOGW(TAG, "Simulating failure - initiating rollback");
    transaction_rollback();

    /* Verify rollback */
    ret = star_bus_i2c_read(&g_manager, "eeprom", current_values, 4, 0x20, &bytes_read);
    ESP_LOGI(TAG, "After rollback: 0x%02X 0x%02X 0x%02X 0x%02X",
             current_values[0], current_values[1], current_values[2], current_values[3]);

    /* --- Checkpoint and Recovery --- */
    ESP_LOGI(TAG, "\n--- Checkpoint Pattern ---");

    /* Save checkpoint */
    uint8_t checkpoint[8];
    star_bus_i2c_read(&g_manager, "eeprom", checkpoint, 8, 0x00, &bytes_read);
    ESP_LOGI(TAG, "Checkpoint saved");

    /* Make some changes */
    uint8_t changes[] = {0xFF, 0xFE, 0xFD, 0xFC};
    size_t total_written;
    star_bus_i2c_write(&g_manager, "eeprom", changes, 4, 0x00, &total_written);
    ESP_LOGI(TAG, "Changes applied");

    /* Restore from checkpoint */
    star_bus_i2c_write(&g_manager, "eeprom", checkpoint, 8, 0x00, &total_written);
    ESP_LOGI(TAG, "Restored from checkpoint");

    /* Cleanup */
    star_bus_manager_deinit(&g_manager);
    ESP_LOGI(TAG, "\nBatch rollback example complete");
}
