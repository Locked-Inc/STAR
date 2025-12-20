/**
 * @file star_arq.h
 * @brief Automatic Repeat Request (ARQ) protocol API with Stop-and-Wait strategy
 * @details
 * Provides interface for reliable packet delivery over SPI peripheral using Stop-and-Wait ARQ.
 * Implements automatic retransmission, acknowledgment handling, duplicate detection, and
 * configurable timeout/retry parameters for robust communication between ESP32 and RPi5.
 *
 * Stop-and-Wait Protocol:
 * - Send one frame at a time
 * - Wait for ACK before sending next frame
 * - Retry up to 3 times on timeout (10ms default)
 * - Symmetric protocol (both sides use identical logic)
 *
 * Key Features:
 * - Automatic retransmission on packet loss
 * - Duplicate detection and filtering
 * - CRC-32 validation via frame layer
 * - Configurable timeout and retry count
 * - Static allocation (no malloc)
 *
 * Example Usage:
 * @code
 * // === Initialize ARQ Layer ===
 *
 * #include "star_arq.h"
 * #include "star_bus_manager.h"
 *
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", NULL, NULL);
 *
 * // Create SPI peripheral for RPi5 communication
 * star_bus_config_t* spi = star_bus_config_create_spi_peripheral(
 *     "rpi_spi", SPI3_HOST,
 *     GPIO_NUM_11,  // COPI
 *     GPIO_NUM_13,  // CIPO
 *     GPIO_NUM_12,  // SCLK
 *     GPIO_NUM_10,  // CS
 *     3, 0
 * );
 * star_bus_manager_add_bus(&bus_manager, spi);
 *
 * // Initialize ARQ layer
 * star_arq_handle_t arq;
 * star_arq_config_t config = {
 *     .bus_manager = &bus_manager,
 *     .spi_bus_name = "rpi_spi",
 *     .max_retries = 3,
 *     .retry_timeout_ms = 10,
 * };
 * star_arq_init(&arq, &config);
 *
 *
 * // === Send Data with Automatic Retries ===
 *
 * uint8_t tx_data[] = "Hello, RPi5!";
 * esp_err_t ret = star_arq_send(&arq, tx_data, sizeof(tx_data), 1000);
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Data sent successfully");
 * } else if (ret == ESP_ERR_TIMEOUT) {
 *     ESP_LOGE(TAG, "Max retries exceeded");
 * }
 *
 *
 * // === Receive Data with Duplicate Filtering ===
 *
 * uint8_t rx_data[256];
 * size_t rx_len;
 *
 * ret = star_arq_receive(&arq, rx_data, sizeof(rx_data), &rx_len, portMAX_DELAY);
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Received %zu bytes", rx_len);
 *     // Process rx_data
 * }
 *
 *
 * // === Cleanup ===
 *
 * star_arq_deinit(&arq);
 * @endcode
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_ARQ_H
#define STAR_ARQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_bus_manager.h"
#include "star_frame.h"

/* --- Type Definitions --- */

/**
 * @brief ARQ protocol constants
 */
typedef enum {
    k_star_arq_default_max_retries    = 3,    /**< Default maximum retry attempts */
    k_star_arq_default_timeout_ms     = 10,   /**< Default retry timeout (10ms) */
    k_star_arq_max_data_size          = 1024, /**< Maximum data payload size */
} star_arq_constants_t;

/**
 * @brief ARQ configuration structure
 */
typedef struct {
    star_bus_manager_t* bus_manager;      /**< Bus manager instance (required) */
    const char*         spi_bus_name;     /**< SPI bus name for communication (required) */
    uint8_t             max_retries;      /**< Maximum retry attempts (default: 3) */
    uint32_t            retry_timeout_ms; /**< Retry timeout in milliseconds (default: 10) */
} star_arq_config_t;

/**
 * @brief ARQ handle for managing protocol state
 *
 * Contains TX and RX state machines, retry buffer, and frame handles.
 * All memory is statically allocated (no malloc).
 */
typedef struct {
    /* Configuration */
    star_bus_manager_t* bus_manager;      /**< Bus manager instance */
    const char*         spi_bus_name;     /**< SPI bus name */
    uint8_t             max_retries;      /**< Maximum retry attempts */
    uint32_t            retry_timeout_ms; /**< Retry timeout in milliseconds */

    /* TX state (transmit) */
    uint16_t tx_seq;                      /**< Next sequence number to send */
    uint8_t  retry_buffer[k_star_frame_max_size]; /**< Last sent frame for retries */
    size_t   retry_buffer_len;            /**< Length of data in retry buffer */
    uint8_t  retry_count;                 /**< Current retry attempt (0-max_retries) */

    /* RX state (receive) */
    uint16_t rx_last_seq;                 /**< Last successfully received sequence number */
    bool     rx_initialized;              /**< RX sequence tracking initialized */

    /* Frame layer handles */
    star_frame_handle_t tx_frame;         /**< Frame handle for building TX frames */
    star_frame_handle_t rx_frame;         /**< Frame handle for parsing RX frames */

    /* Initialization flag */
    bool initialized;                     /**< ARQ layer initialized */
} star_arq_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize ARQ layer
 *
 * Initializes the ARQ protocol state machine with the provided configuration.
 * Must be called before using star_arq_send() or star_arq_receive().
 *
 * @param[in,out] handle  Pointer to ARQ handle to initialize
 * @param[in]     config  Pointer to configuration structure
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle or config is NULL
 *     - ESP_ERR_INVALID_ARG if bus_manager or spi_bus_name is NULL
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t star_arq_init(star_arq_handle_t* handle, const star_arq_config_t* config);

/**
 * @brief Deinitialize ARQ layer
 *
 * Clears the ARQ handle and releases resources. After calling this function,
 * the handle must be reinitialized before use.
 *
 * @param[in,out] handle  Pointer to ARQ handle to deinitialize
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t star_arq_deinit(star_arq_handle_t* handle);

/**
 * @brief Send data with automatic retries (Stop-and-Wait)
 *
 * Sends data using Stop-and-Wait ARQ protocol:
 * 1. Build frame with current sequence number
 * 2. Send frame via SPI
 * 3. Wait for ACK with timeout
 * 4. On timeout or NACK: retry (up to max_retries)
 * 5. On ACK: increment sequence number and return
 *
 * This function blocks until ACK is received or max retries exceeded.
 *
 * @param[in,out] handle      Pointer to ARQ handle
 * @param[in]     data        Pointer to data to send
 * @param[in]     len         Data length in bytes (max 1024)
 * @param[in]     timeout_ms  Total timeout in milliseconds (0 = wait forever)
 *
 * @return
 *     - ESP_OK on success (ACK received)
 *     - ESP_ERR_INVALID_ARG if handle or data is NULL
 *     - ESP_ERR_INVALID_ARG if len > k_star_arq_max_data_size
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_ERR_TIMEOUT if max retries exceeded
 *     - ESP_FAIL on SPI communication error
 */
esp_err_t star_arq_send(
    star_arq_handle_t* handle,
    const uint8_t* data,
    size_t len,
    uint32_t timeout_ms
);

/**
 * @brief Receive data with duplicate filtering
 *
 * Receives data using Stop-and-Wait ARQ protocol:
 * 1. Receive frame via SPI
 * 2. Validate CRC (via frame layer)
 * 3. Check sequence number:
 *    - Expected (seq == last+1): Accept, send ACK, return data
 *    - Duplicate (seq == last): Send ACK, discard, wait for next
 *    - Out-of-order: Send NACK, wait for retransmission
 * 4. Return payload data
 *
 * This function blocks until valid data is received or timeout occurs.
 *
 * @param[in,out] handle      Pointer to ARQ handle
 * @param[out]    data        Pointer to buffer for received data
 * @param[in]     max_len     Maximum buffer size in bytes
 * @param[out]    out_len     Pointer to store actual received length
 * @param[in]     timeout_ms  Receive timeout in milliseconds (portMAX_DELAY = wait forever)
 *
 * @return
 *     - ESP_OK on success (new data received)
 *     - ESP_ERR_INVALID_ARG if any pointer is NULL
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_ERR_TIMEOUT if timeout occurs
 *     - ESP_FAIL on SPI communication error
 */
esp_err_t star_arq_receive(
    star_arq_handle_t* handle,
    uint8_t* data,
    size_t max_len,
    size_t* out_len,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif

#endif /* STAR_ARQ_H */
