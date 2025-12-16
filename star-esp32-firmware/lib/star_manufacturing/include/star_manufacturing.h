/* lib/star_manufacturing/include/star_manufacturing.h - Manufacturing support API */

#ifndef STAR_MANUFACTURING_H
#define STAR_MANUFACTURING_H

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STAR_MFG_SERIAL_NUMBER_LEN 16 /**< Serial number string length */

/**
 * @brief Manufacturing information
 */
typedef struct {
    char serial_number[STAR_MFG_SERIAL_NUMBER_LEN]; /**< Unique serial number */
    uint8_t hw_revision_major;                      /**< Hardware revision (major) */
    uint8_t hw_revision_minor;                      /**< Hardware revision (minor) */
    time_t mfg_date;                                /**< Manufacturing date (Unix timestamp) */
    uint32_t calibration_crc;                       /**< Calibration data CRC */
    bool factory_test_passed;                       /**< Factory test result */
} star_mfg_info_t;

/**
 * @brief Initialize manufacturing support
 *
 * Loads manufacturing info from NVS or generates defaults.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_mfg_init(void);

/**
 * @brief Get manufacturing information
 *
 * @param info Output buffer for manufacturing info
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if info is NULL
 */
esp_err_t star_mfg_get_info(star_mfg_info_t* info);

/**
 * @brief Set serial number
 *
 * @param serial_number Serial number string (max 15 characters + null)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_mfg_set_serial_number(const char* serial_number);

/**
 * @brief Set hardware revision
 *
 * @param major Major revision (e.g., 1 for v1.x)
 * @param minor Minor revision (e.g., 2 for vX.2)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_mfg_set_hw_revision(uint8_t major, uint8_t minor);

/**
 * @brief Mark factory test as passed
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_mfg_mark_test_passed(void);

/**
 * @brief Save manufacturing info to NVS
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_mfg_save(void);

/**
 * @brief Print manufacturing info to console
 */
void star_mfg_print_info(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_MANUFACTURING_H */
