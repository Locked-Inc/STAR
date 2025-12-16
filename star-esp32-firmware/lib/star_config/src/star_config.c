/* lib/star_config/src/star_config.c - Configuration management implementation */

#include "star_config.h"

#include "esp_crc.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char* TAG = "star_config";

/* NVS namespace and key */
static const char* NVS_NAMESPACE = "star_cfg";
static const char* NVS_KEY       = "config";

/* Current configuration schema version */
#define CONFIG_VERSION (1)

/* Factory default values */
static const star_config_t k_factory_defaults = {
    /* Motor PID Gains (conservative defaults) */
    .motor_pid_kp = {1.0f, 1.0f, 1.0f, 1.0f},
    .motor_pid_ki = {0.5f, 0.5f, 0.5f, 0.5f},
    .motor_pid_kd = {0.1f, 0.1f, 0.1f, 0.1f},

    /* Current Sensor Calibration (no offset by default) */
    .current_offset_ma = {0.0f, 0.0f, 0.0f, 0.0f},
    .current_scale = {1.0f, 1.0f, 1.0f, 1.0f},

    /* Temperature Sensor Calibration */
    .temp_offset_c = 0.0f,

    /* Safety Thresholds */
    .overcurrent_threshold_ma = 2500.0f,    /* 2.5A per motor */
    .thermal_shutdown_temp_c = 85.0f,       /* 85°C emergency cutoff */
    .cell_imbalance_threshold_mv = 100,     /* 100mV cell difference */

    /* Encoder Configuration */
    .encoder_edges_per_rev = 2000,  /* 500 PPR × 4 edges */

    /* Timing Parameters */
    .motor_control_period_ms = 4,     /* 250Hz control loop */
    .telemetry_period_ms = 100,       /* 10Hz telemetry */
    .communication_timeout_ms = 500,  /* 500ms comm watchdog */

    /* Metadata */
    .config_version = CONFIG_VERSION,
    .config_crc = 0,  /* Calculated when saved */
};

uint32_t star_config_calculate_crc(const star_config_t* config)
{
    if (!config) {
        return 0;
    }

    /* Calculate CRC over entire structure except crc field */
    size_t crc_size = sizeof(star_config_t) - sizeof(config->config_crc);
    return esp_crc32_le(0, (const uint8_t*)config, crc_size);
}

bool star_config_validate(const star_config_t* config)
{
    if (!config) {
        return false;
    }

    /* Validate PID gains (must be non-negative) */
    for (int i = 0; i < STAR_CONFIG_NUM_MOTORS; i++) {
        if (config->motor_pid_kp[i] < 0.0f || config->motor_pid_kp[i] > 100.0f) {
            ESP_LOGE(TAG, "Invalid Kp[%d]: %.2f", i, config->motor_pid_kp[i]);
            return false;
        }
        if (config->motor_pid_ki[i] < 0.0f || config->motor_pid_ki[i] > 100.0f) {
            ESP_LOGE(TAG, "Invalid Ki[%d]: %.2f", i, config->motor_pid_ki[i]);
            return false;
        }
        if (config->motor_pid_kd[i] < 0.0f || config->motor_pid_kd[i] > 100.0f) {
            ESP_LOGE(TAG, "Invalid Kd[%d]: %.2f", i, config->motor_pid_kd[i]);
            return false;
        }
    }

    /* Validate current sensor calibration */
    for (int i = 0; i < STAR_CONFIG_NUM_MOTORS; i++) {
        if (config->current_offset_ma[i] < -1000.0f || config->current_offset_ma[i] > 1000.0f) {
            ESP_LOGE(TAG, "Invalid current offset[%d]: %.2f", i, config->current_offset_ma[i]);
            return false;
        }
        if (config->current_scale[i] < 0.1f || config->current_scale[i] > 10.0f) {
            ESP_LOGE(TAG, "Invalid current scale[%d]: %.2f", i, config->current_scale[i]);
            return false;
        }
    }

    /* Validate temperature offset */
    if (config->temp_offset_c < -50.0f || config->temp_offset_c > 50.0f) {
        ESP_LOGE(TAG, "Invalid temp offset: %.2f", config->temp_offset_c);
        return false;
    }

    /* Validate safety thresholds */
    if (config->overcurrent_threshold_ma < 100.0f || config->overcurrent_threshold_ma > 10000.0f) {
        ESP_LOGE(TAG, "Invalid overcurrent threshold: %.2f", config->overcurrent_threshold_ma);
        return false;
    }
    if (config->thermal_shutdown_temp_c < 40.0f || config->thermal_shutdown_temp_c > 125.0f) {
        ESP_LOGE(TAG, "Invalid thermal shutdown: %.2f", config->thermal_shutdown_temp_c);
        return false;
    }
    if (config->cell_imbalance_threshold_mv < 10 || config->cell_imbalance_threshold_mv > 1000) {
        ESP_LOGE(TAG, "Invalid cell imbalance threshold: %u", config->cell_imbalance_threshold_mv);
        return false;
    }

    /* Validate encoder configuration */
    if (config->encoder_edges_per_rev < 100 || config->encoder_edges_per_rev > 10000) {
        ESP_LOGE(TAG, "Invalid encoder edges: %lu", config->encoder_edges_per_rev);
        return false;
    }

    /* Validate timing parameters */
    if (config->motor_control_period_ms < 1 || config->motor_control_period_ms > 100) {
        ESP_LOGE(TAG, "Invalid motor control period: %lu", config->motor_control_period_ms);
        return false;
    }
    if (config->telemetry_period_ms < 10 || config->telemetry_period_ms > 10000) {
        ESP_LOGE(TAG, "Invalid telemetry period: %lu", config->telemetry_period_ms);
        return false;
    }
    if (config->communication_timeout_ms < 100 || config->communication_timeout_ms > 5000) {
        ESP_LOGE(TAG, "Invalid comm timeout: %lu", config->communication_timeout_ms);
        return false;
    }

    return true;
}

esp_err_t star_config_reset_to_defaults(star_config_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Copy factory defaults */
    memcpy(config, &k_factory_defaults, sizeof(star_config_t));

    /* Calculate and set CRC */
    config->config_crc = star_config_calculate_crc(config);

    ESP_LOGI(TAG, "Configuration reset to factory defaults");
    return ESP_OK;
}

esp_err_t star_config_load(star_config_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Initialize NVS if not already done */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Open NVS namespace */
    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        return star_config_reset_to_defaults(config);
    }

    /* Read configuration blob */
    size_t required_size = sizeof(star_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY, config, &required_size);
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Config not found in NVS, using defaults");
        return star_config_reset_to_defaults(config);
    }

    /* Verify CRC */
    uint32_t calculated_crc = star_config_calculate_crc(config);
    if (calculated_crc != config->config_crc) {
        ESP_LOGW(TAG,
                 "Config CRC mismatch (expected 0x%08lX, got 0x%08lX), using defaults",
                 calculated_crc,
                 config->config_crc);
        return star_config_reset_to_defaults(config);
    }

    /* Validate configuration */
    if (!star_config_validate(config)) {
        ESP_LOGW(TAG, "Config validation failed, using defaults");
        return star_config_reset_to_defaults(config);
    }

    ESP_LOGI(TAG, "Configuration loaded from NVS (version %lu)", config->config_version);
    return ESP_OK;
}

esp_err_t star_config_save(const star_config_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Validate before saving */
    if (!star_config_validate(config)) {
        ESP_LOGE(TAG, "Cannot save invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    /* Create mutable copy to update CRC */
    star_config_t config_copy;
    memcpy(&config_copy, config, sizeof(star_config_t));

    /* Update version and CRC */
    config_copy.config_version = CONFIG_VERSION;
    config_copy.config_crc = star_config_calculate_crc(&config_copy);

    /* Open NVS namespace for writing */
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Write configuration blob */
    ret = nvs_set_blob(nvs_handle, NVS_KEY, &config_copy, sizeof(star_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config to NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    /* Commit changes */
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Configuration saved to NVS (CRC: 0x%08lX)", config_copy.config_crc);
    return ESP_OK;
}
