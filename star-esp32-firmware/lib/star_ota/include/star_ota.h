/* lib/star_ota/include/star_ota.h - OTA firmware update API */

#ifndef STAR_OTA_H
#define STAR_OTA_H

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update state
 */
typedef enum {
    STAR_OTA_STATE_IDLE,       /**< No OTA in progress */
    STAR_OTA_STATE_RECEIVING,  /**< Receiving firmware data */
    STAR_OTA_STATE_VALIDATING, /**< Validating received firmware */
    STAR_OTA_STATE_COMPLETE,   /**< OTA complete, ready to reboot */
    STAR_OTA_STATE_ERROR       /**< Error occurred during OTA */
} star_ota_state_t;

/**
 * @brief OTA update statistics
 */
typedef struct {
    size_t total_size;         /**< Total firmware size in bytes */
    size_t bytes_received;     /**< Bytes received so far */
    uint32_t crc32;            /**< Running CRC-32 checksum */
    star_ota_state_t state;    /**< Current OTA state */
} star_ota_stats_t;

/**
 * @brief Begin OTA update process
 *
 * Prepares the OTA partition for receiving firmware data.
 *
 * @param firmware_size Expected firmware size in bytes
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if size is invalid
 *         - ESP_ERR_INVALID_STATE if OTA already in progress
 *         - ESP_ERR_NO_MEM if partition not found
 */
esp_err_t star_ota_begin(size_t firmware_size);

/**
 * @brief Write firmware data chunk
 *
 * Writes a chunk of firmware data to the OTA partition.
 * Must be called after star_ota_begin().
 *
 * @param data Firmware data buffer
 * @param len Length of data in bytes (recommend 1KB chunks)
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if data is NULL or len is 0
 *         - ESP_ERR_INVALID_STATE if OTA not started
 *         - ESP_ERR_INVALID_SIZE if exceeds expected size
 */
esp_err_t star_ota_write_chunk(const uint8_t* data, size_t len);

/**
 * @brief Finalize OTA update
 *
 * Validates the firmware image and sets the new boot partition.
 * After successful completion, call star_ota_reboot() to activate.
 *
 * @return esp_err_t
 *         - ESP_OK on success (ready to reboot)
 *         - ESP_ERR_INVALID_STATE if OTA not in progress
 *         - ESP_ERR_INVALID_SIZE if incomplete transfer
 *         - ESP_ERR_INVALID_CRC if validation fails
 */
esp_err_t star_ota_end(void);

/**
 * @brief Abort OTA update
 *
 * Cancels the current OTA update and cleans up resources.
 * Safe to call at any time.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_ota_abort(void);

/**
 * @brief Get OTA update statistics
 *
 * @param stats Output buffer for statistics
 * @return esp_err_t
 *         - ESP_OK on success
 *         - ESP_ERR_INVALID_ARG if stats is NULL
 */
esp_err_t star_ota_get_stats(star_ota_stats_t* stats);

/**
 * @brief Reboot to new firmware
 *
 * Reboots the ESP32 to activate the new firmware.
 * Only call after successful star_ota_end().
 */
void star_ota_reboot(void) __attribute__((noreturn));

/**
 * @brief Roll back to previous firmware
 *
 * Marks the current firmware as invalid and reboots to the previous version.
 * Use this if the new firmware fails to boot properly.
 *
 * @return esp_err_t ESP_OK on success (will reboot)
 */
esp_err_t star_ota_rollback(void);

/**
 * @brief Mark current firmware as valid
 *
 * Call this after successful boot with new firmware to prevent automatic rollback.
 * Should be called during first boot after OTA update.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t star_ota_mark_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_OTA_H */
