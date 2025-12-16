/* lib/star_config/include/star_config.h - Configuration management API */

#ifndef STAR_CONFIG_H
#define STAR_CONFIG_H

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of motors in the system */
#define STAR_CONFIG_NUM_MOTORS (4)

/**
 * @brief System configuration structure
 *
 * All runtime-configurable parameters stored in NVS.
 * Persists across power cycles and firmware updates.
 */
typedef struct {
    /* Motor PID Gains (4 motors × 3 gains) */
    float motor_pid_kp[STAR_CONFIG_NUM_MOTORS];
    float motor_pid_ki[STAR_CONFIG_NUM_MOTORS];
    float motor_pid_kd[STAR_CONFIG_NUM_MOTORS];

    /* Current Sensor Calibration */
    float current_offset_ma[STAR_CONFIG_NUM_MOTORS];  /* ADC offset when motor stopped */
    float current_scale[STAR_CONFIG_NUM_MOTORS];      /* Scaling factor adjustment */

    /* Temperature Sensor Calibration */
    float temp_offset_c;  /* Temperature offset correction */

    /* Safety Thresholds */
    float    overcurrent_threshold_ma;     /* Motor overcurrent limit */
    float    thermal_shutdown_temp_c;      /* Emergency temperature cutoff */
    uint16_t cell_imbalance_threshold_mv;  /* Battery cell voltage difference */

    /* Encoder Configuration */
    uint32_t encoder_edges_per_rev;  /* Encoder resolution */

    /* Timing Parameters */
    uint32_t motor_control_period_ms;   /* Motor control loop period */
    uint32_t telemetry_period_ms;       /* Telemetry reporting period */
    uint32_t communication_timeout_ms;  /* RPi5 communication watchdog */

    /* Metadata */
    uint32_t config_version;  /* Configuration schema version */
    uint32_t config_crc;      /* CRC-32 checksum for validation */
} star_config_t;

/**
 * @brief Load configuration from NVS
 *
 * Loads configuration from non-volatile storage. If no configuration is found,
 * initializes with factory defaults.
 *
 * @param[out] config Pointer to configuration structure to populate
 * @return
 *     - ESP_OK: Configuration loaded successfully
 *     - ESP_ERR_INVALID_ARG: config is NULL
 *     - ESP_ERR_NVS_NOT_FOUND: No saved config, defaults loaded
 *     - Other ESP-IDF NVS error codes
 */
esp_err_t star_config_load(star_config_t* config);

/**
 * @brief Save configuration to NVS
 *
 * Validates and saves configuration to non-volatile storage.
 * Updates CRC before saving.
 *
 * @param[in] config Pointer to configuration to save
 * @return
 *     - ESP_OK: Configuration saved successfully
 *     - ESP_ERR_INVALID_ARG: config is NULL or validation failed
 *     - Other ESP-IDF NVS error codes
 */
esp_err_t star_config_save(const star_config_t* config);

/**
 * @brief Reset configuration to factory defaults
 *
 * Loads factory default values into configuration structure.
 * Does NOT save to NVS - call star_config_save() to persist.
 *
 * @param[out] config Pointer to configuration structure to initialize
 * @return
 *     - ESP_OK: Defaults loaded successfully
 *     - ESP_ERR_INVALID_ARG: config is NULL
 */
esp_err_t star_config_reset_to_defaults(star_config_t* config);

/**
 * @brief Validate configuration parameters
 *
 * Checks all configuration values are within acceptable ranges.
 * Used before saving to NVS.
 *
 * @param[in] config Pointer to configuration to validate
 * @return
 *     - true: All parameters valid
 *     - false: One or more parameters out of range
 */
bool star_config_validate(const star_config_t* config);

/**
 * @brief Calculate CRC-32 checksum of configuration
 *
 * Computes CRC-32 of configuration data (excluding crc field itself).
 * Used for integrity validation.
 *
 * @param[in] config Pointer to configuration
 * @return CRC-32 checksum
 */
uint32_t star_config_calculate_crc(const star_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* STAR_CONFIG_H */
