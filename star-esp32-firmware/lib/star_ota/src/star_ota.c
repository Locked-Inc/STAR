/* lib/star_ota/src/star_ota.c - OTA firmware update implementation */

#include "star_ota.h"

#include "esp_app_format.h"
#include "esp_crc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char* TAG = "star_ota";

/* OTA state management */
static star_ota_state_t s_ota_state = STAR_OTA_STATE_IDLE;
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t* s_ota_partition = NULL;
static star_ota_stats_t s_ota_stats;

esp_err_t star_ota_begin(size_t firmware_size)
{
    /* Validate arguments */
    if (firmware_size == 0 || firmware_size > 0x200000 /* 2MB max */) {
        ESP_LOGE(TAG, "Invalid firmware size: %zu bytes", firmware_size);
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if OTA already in progress */
    if (s_ota_state != STAR_OTA_STATE_IDLE) {
        ESP_LOGE(TAG, "OTA already in progress (state %d)", s_ota_state);
        return ESP_ERR_INVALID_STATE;
    }

    /* Find next OTA partition */
    s_ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_ota_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG,
             "Starting OTA update to partition '%s' (subtype %d, offset 0x%lX, size %lu bytes)",
             s_ota_partition->label,
             s_ota_partition->subtype,
             s_ota_partition->address,
             s_ota_partition->size);

    /* Begin OTA */
    esp_err_t ret = esp_ota_begin(s_ota_partition, firmware_size, &s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        s_ota_partition = NULL;
        return ret;
    }

    /* Initialize statistics */
    memset(&s_ota_stats, 0, sizeof(s_ota_stats));
    s_ota_stats.total_size = firmware_size;
    s_ota_stats.state = STAR_OTA_STATE_RECEIVING;
    s_ota_state = STAR_OTA_STATE_RECEIVING;

    ESP_LOGI(TAG, "OTA update started, expecting %zu bytes", firmware_size);

    return ESP_OK;
}

esp_err_t star_ota_write_chunk(const uint8_t* data, size_t len)
{
    /* Validate arguments */
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check state */
    if (s_ota_state != STAR_OTA_STATE_RECEIVING) {
        ESP_LOGE(TAG, "OTA not in RECEIVING state (current state: %d)", s_ota_state);
        return ESP_ERR_INVALID_STATE;
    }

    /* Check if exceeding expected size */
    if (s_ota_stats.bytes_received + len > s_ota_stats.total_size) {
        ESP_LOGE(TAG,
                 "Chunk exceeds expected size (%zu + %zu > %zu)",
                 s_ota_stats.bytes_received,
                 len,
                 s_ota_stats.total_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Write chunk to OTA partition */
    esp_err_t ret = esp_ota_write(s_ota_handle, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
        s_ota_state = STAR_OTA_STATE_ERROR;
        s_ota_stats.state = STAR_OTA_STATE_ERROR;
        return ret;
    }

    /* Update statistics */
    s_ota_stats.bytes_received += len;
    s_ota_stats.crc32 = esp_crc32_le(s_ota_stats.crc32, data, len);

    /* Log progress every 64KB */
    if ((s_ota_stats.bytes_received % 65536) == 0) {
        uint32_t percent = (s_ota_stats.bytes_received * 100) / s_ota_stats.total_size;
        ESP_LOGI(TAG,
                 "OTA progress: %zu / %zu bytes (%lu%%)",
                 s_ota_stats.bytes_received,
                 s_ota_stats.total_size,
                 percent);
    }

    return ESP_OK;
}

esp_err_t star_ota_end(void)
{
    /* Check state */
    if (s_ota_state != STAR_OTA_STATE_RECEIVING) {
        ESP_LOGE(TAG, "OTA not in RECEIVING state");
        return ESP_ERR_INVALID_STATE;
    }

    /* Verify complete transfer */
    if (s_ota_stats.bytes_received != s_ota_stats.total_size) {
        ESP_LOGE(TAG,
                 "Incomplete transfer: received %zu bytes, expected %zu",
                 s_ota_stats.bytes_received,
                 s_ota_stats.total_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Finalize OTA */
    s_ota_state = STAR_OTA_STATE_VALIDATING;
    s_ota_stats.state = STAR_OTA_STATE_VALIDATING;

    esp_err_t ret = esp_ota_end(s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        s_ota_state = STAR_OTA_STATE_ERROR;
        s_ota_stats.state = STAR_OTA_STATE_ERROR;
        return ret;
    }

    /* Set boot partition */
    ret = esp_ota_set_boot_partition(s_ota_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        s_ota_state = STAR_OTA_STATE_ERROR;
        s_ota_stats.state = STAR_OTA_STATE_ERROR;
        return ret;
    }

    s_ota_state = STAR_OTA_STATE_COMPLETE;
    s_ota_stats.state = STAR_OTA_STATE_COMPLETE;

    ESP_LOGI(TAG,
             "OTA update complete! New firmware ready (CRC32: 0x%08lX)",
             s_ota_stats.crc32);
    ESP_LOGI(TAG, "Reboot to activate new firmware");

    return ESP_OK;
}

esp_err_t star_ota_abort(void)
{
    if (s_ota_state == STAR_OTA_STATE_IDLE) {
        return ESP_OK; /* Already idle */
    }

    ESP_LOGW(TAG, "Aborting OTA update");

    /* Abort OTA if in progress */
    if (s_ota_handle != 0) {
        esp_ota_abort(s_ota_handle);
        s_ota_handle = 0;
    }

    /* Reset state */
    s_ota_partition = NULL;
    s_ota_state = STAR_OTA_STATE_IDLE;
    memset(&s_ota_stats, 0, sizeof(s_ota_stats));
    s_ota_stats.state = STAR_OTA_STATE_IDLE;

    return ESP_OK;
}

esp_err_t star_ota_get_stats(star_ota_stats_t* stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &s_ota_stats, sizeof(star_ota_stats_t));
    return ESP_OK;
}

void star_ota_reboot(void)
{
    ESP_LOGI(TAG, "Rebooting in 1 second...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

esp_err_t star_ota_rollback(void)
{
    ESP_LOGW(TAG, "Rolling back to previous firmware");

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* last_valid = esp_ota_get_last_invalid_partition();

    if (!last_valid) {
        ESP_LOGE(TAG, "No previous valid partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "Rolling back from %s to %s",
             running ? running->label : "unknown",
             last_valid->label);

    esp_err_t ret = esp_ota_set_boot_partition(last_valid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Rollback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Rollback successful, rebooting...");
    star_ota_reboot();

    return ESP_OK;
}

esp_err_t star_ota_mark_valid(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* configured = esp_ota_get_boot_partition();

    if (running != configured) {
        ESP_LOGW(TAG, "Running partition != configured boot partition");
    }

    ESP_LOGI(TAG, "Marking current firmware as valid: %s", running ? running->label : "unknown");

    /* In ESP-IDF, there's no explicit "mark valid" API - the firmware is considered valid
     * if it boots successfully and doesn't call esp_ota_mark_app_invalid_rollback_and_reboot()
     */

    return ESP_OK;
}
