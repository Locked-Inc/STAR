/* lib/star_manufacturing/src/star_manufacturing.c - Manufacturing support implementation */

#include "star_manufacturing.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char* TAG = "star_mfg";

/* NVS namespace for manufacturing data */
static const char* NVS_NAMESPACE = "star_mfg";
static const char* NVS_KEY       = "mfg_info";

/* Current manufacturing information */
static star_mfg_info_t s_mfg_info;
static bool s_initialized = false;

esp_err_t star_mfg_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* Initialize NVS if not already done */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS init failed");

    /* Try to load from NVS */
    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (ret == ESP_OK) {
        size_t required_size = sizeof(star_mfg_info_t);
        ret = nvs_get_blob(nvs_handle, NVS_KEY, &s_mfg_info, &required_size);
        nvs_close(nvs_handle);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Manufacturing info loaded from NVS");
            s_initialized = true;
            star_mfg_print_info();
            return ESP_OK;
        }
    }

    /* No saved info - generate defaults from MAC address */
    ESP_LOGW(TAG, "No manufacturing info in NVS, generating defaults");

    memset(&s_mfg_info, 0, sizeof(star_mfg_info_t));

    /* Generate serial number from MAC address */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_mfg_info.serial_number,
             STAR_MFG_SERIAL_NUMBER_LEN,
             "STAR%02X%02X%02X%02X",
             mac[2], mac[3], mac[4], mac[5]);

    /* Default hardware revision */
    s_mfg_info.hw_revision_major = 1;
    s_mfg_info.hw_revision_minor = 0;

    /* Manufacturing date = current time (if available) */
    s_mfg_info.mfg_date = time(NULL);

    /* Not factory tested yet */
    s_mfg_info.factory_test_passed = false;

    /* Save defaults */
    star_mfg_save();

    s_initialized = true;
    star_mfg_print_info();

    return ESP_OK;
}

esp_err_t star_mfg_get_info(star_mfg_info_t* info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(star_mfg_init(), TAG, "Init failed");
    }

    memcpy(info, &s_mfg_info, sizeof(star_mfg_info_t));
    return ESP_OK;
}

esp_err_t star_mfg_set_serial_number(const char* serial_number)
{
    if (!serial_number) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(star_mfg_init(), TAG, "Init failed");
    }

    strncpy(s_mfg_info.serial_number, serial_number, STAR_MFG_SERIAL_NUMBER_LEN - 1);
    s_mfg_info.serial_number[STAR_MFG_SERIAL_NUMBER_LEN - 1] = '\0';

    ESP_LOGI(TAG, "Serial number set: %s", s_mfg_info.serial_number);

    return ESP_OK;
}

esp_err_t star_mfg_set_hw_revision(uint8_t major, uint8_t minor)
{
    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(star_mfg_init(), TAG, "Init failed");
    }

    s_mfg_info.hw_revision_major = major;
    s_mfg_info.hw_revision_minor = minor;

    ESP_LOGI(TAG, "Hardware revision set: v%u.%u", major, minor);

    return ESP_OK;
}

esp_err_t star_mfg_mark_test_passed(void)
{
    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(star_mfg_init(), TAG, "Init failed");
    }

    s_mfg_info.factory_test_passed = true;
    s_mfg_info.mfg_date = time(NULL);

    ESP_LOGI(TAG, "Factory test marked as PASSED");

    return star_mfg_save();
}

esp_err_t star_mfg_save(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to open NVS");

    ret = nvs_set_blob(nvs_handle, NVS_KEY, &s_mfg_info, sizeof(star_mfg_info_t));
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to write manufacturing info");
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Manufacturing info saved to NVS");
    }

    return ret;
}

void star_mfg_print_info(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Manufacturing info not initialized");
        return;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   STAR Manufacturing Information");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Serial Number:    %s", s_mfg_info.serial_number);
    ESP_LOGI(TAG, "Hardware Revision: v%u.%u",
             s_mfg_info.hw_revision_major,
             s_mfg_info.hw_revision_minor);
    ESP_LOGI(TAG, "Factory Test:     %s",
             s_mfg_info.factory_test_passed ? "PASSED" : "NOT TESTED");

    if (s_mfg_info.mfg_date > 0) {
        char time_str[64];
        struct tm timeinfo;
        localtime_r(&s_mfg_info.mfg_date, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "Manufacturing Date: %s", time_str);
    }

    ESP_LOGI(TAG, "========================================");
}
