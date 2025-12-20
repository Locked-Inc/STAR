/**
 * @file star_bus_spi_peripheral.h
 * @brief SPI peripheral mode API for the bus manager
 * @details
 * Provides interface for SPI peripheral mode operations through the bus manager.
 * Allows ESP32 to act as an SPI peripheral controlled by an external controller (e.g., RPi5).
 * Supports DMA transfers and event-driven callbacks for efficient bidirectional communication.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_SPI_PERIPHERAL_H
#define STAR_COMPONENT_BUS_SPI_PERIPHERAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_slave.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_spi_peripheral.h
 * @brief SPI peripheral mode operations
 *
 * This module allows the ESP32 to act as an SPI peripheral device,
 * receiving commands and data from an external SPI controller.
 *
 * Example Usage:
 * @code
 * // === Setup ESP32 as SPI Peripheral ===
 *
 * #include "star_bus_spi_peripheral.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Create SPI peripheral bus configuration
 * // Note: Use star_bus_config_create_spi_peripheral() from star_bus_config.h
 * star_bus_config_t* spi_periph = star_bus_config_create_spi_peripheral(
 *     "spi_peripheral",
 *     SPI2_HOST,
 *     GPIO_NUM_23,  // COPI (data in from controller)
 *     GPIO_NUM_19,  // CIPO (data out to controller)
 *     GPIO_NUM_18,  // SCLK
 *     GPIO_NUM_5    // CS
 * );
 * star_bus_manager_add_bus(&bus_manager, spi_periph);
 *
 *
 * // === Receive Data from Controller ===
 *
 * uint8_t rx_buffer[64];
 * esp_err_t ret = star_bus_spi_peripheral_receive(
 *     &bus_manager,
 *     "spi_peripheral",
 *     rx_buffer,
 *     sizeof(rx_buffer),
 *     5000  // 5 second timeout
 * );
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Received %d bytes from controller", sizeof(rx_buffer));
 *     // Process received data
 * } else if (ret == ESP_ERR_TIMEOUT) {
 *     ESP_LOGW(TAG, "No data received within timeout");
 * }
 *
 *
 * // === Transmit Data to Controller ===
 *
 * uint8_t tx_buffer[64];
 * memset(tx_buffer, 0xAA, sizeof(tx_buffer));  // Prepare response data
 *
 * ret = star_bus_spi_peripheral_transmit(
 *     &bus_manager,
 *     "spi_peripheral",
 *     tx_buffer,
 *     sizeof(tx_buffer),
 *     5000  // 5 second timeout
 * );
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Data queued for transmission");
 * }
 *
 *
 * // === Full-Duplex Transceive ===
 *
 * // Simultaneously receive and transmit (most common usage)
 * uint8_t tx_data[32] = {0};
 * uint8_t rx_data[32] = {0};
 *
 * // Prepare response before controller initiates transaction
 * tx_data[0] = 0x01;  // Status byte
 * tx_data[1] = 0x23;  // Sensor data
 *
 * ret = star_bus_spi_peripheral_transceive(
 *     &bus_manager,
 *     "spi_peripheral",
 *     tx_data,      // Data to send to controller
 *     rx_data,      // Data received from controller
 *     32,           // Transfer length
 *     portMAX_DELAY // Wait indefinitely
 * );
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Transaction complete");
 *     ESP_LOGI(TAG, "Received command: 0x%02X", rx_data[0]);
 *     // Process command from controller
 * }
 *
 *
 * // === Command/Response Pattern ===
 *
 * // Typical pattern: receive command, send response
 * void spi_peripheral_task(void* arg) {
 *     uint8_t cmd_buffer[4];
 *     uint8_t resp_buffer[64];
 *
 *     while (1) {
 *         // Wait for command from controller
 *         esp_err_t ret = star_bus_spi_peripheral_receive(
 *             &bus_manager, "spi_peripheral",
 *             cmd_buffer, sizeof(cmd_buffer),
 *             portMAX_DELAY
 *         );
 *
 *         if (ret == ESP_OK) {
 *             // Parse command
 *             uint8_t cmd = cmd_buffer[0];
 *
 *             // Prepare response based on command
 *             switch (cmd) {
 *                 case 0x01:  // Read sensor
 *                     resp_buffer[0] = 0xAA;  // Sensor value
 *                     break;
 *                 case 0x02:  // Read status
 *                     resp_buffer[0] = 0x00;  // OK status
 *                     break;
 *                 default:
 *                     resp_buffer[0] = 0xFF;  // Unknown command
 *             }
 *
 *             // Send response
 *             star_bus_spi_peripheral_transmit(
 *                 &bus_manager, "spi_peripheral",
 *                 resp_buffer, 1, 1000
 *             );
 *         }
 *     }
 * }
 *
 *
 * // === Initialize Default Operations ===
 *
 * // If creating custom SPI peripheral operations
 * star_spi_ops_t custom_ops;
 * star_bus_spi_peripheral_init_default_ops(&custom_ops);
 * // Now custom_ops has default implementations
 * @endcode
 */

/* --- SPI Pin Naming: COPI/CIPO (OSHWA Standard) --- */
/* This component uses OSHWA (Open Source Hardware Association) standard SPI terminology:
 * - COPI (Controller Out, Peripheral In): Data line where the controller sends data to the peripheral
 * - CIPO (Controller In, Peripheral Out): Data line where the controller receives data from the peripheral
 *
 *
 * Note: The ESP-IDF SPI peripheral driver (spi_slave.h) still uses legacy mosi_io_num/miso_io_num field names internally, which we map to COPI/CIPO.
 */

/* --- Public Functions --- */

/**
 * @brief Initialize default SPI peripheral operations function pointers within a config structure.
 *        Typically called internally by star_bus_config_create_spi_peripheral.
 *        Assigns default implementations for receive and transmit operations.
 *
 * @param[out] ops Pointer to SPI operations structure to initialize. Must not be NULL.
 */
void star_bus_spi_peripheral_init_default_ops(star_spi_ops_t* ops);

/**
 * @brief Receive data from the SPI controller (this device acts as peripheral).
 *
 * This function blocks until data is received or timeout occurs.
 * The peripheral must be configured and initialized before calling this function.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI peripheral configuration. Must not be NULL.
 * @param[out] rx_buffer Pointer to the buffer where received data will be stored. Must not be NULL.
 * @param[in] len        Maximum number of bytes to receive into the buffer.
 * @param[in] timeout_ms Timeout in milliseconds to wait for data. Use portMAX_DELAY for infinite wait.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, ESP_ERR_TIMEOUT if timeout occurs, or an error from the driver.
 */
esp_err_t star_bus_spi_peripheral_receive(const star_bus_manager_t* manager,
                                          const char*               name,
                                          void*                     rx_buffer,
                                          size_t                    len,
                                          uint32_t                  timeout_ms);

/**
 * @brief Transmit data to the SPI controller (this device acts as peripheral).
 *
 * This function queues data for transmission. The actual transmission occurs when
 * the controller initiates a transaction.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI peripheral configuration. Must not be NULL.
 * @param[in] tx_buffer  Pointer to the data buffer to transmit. Must not be NULL.
 * @param[in] len        Number of bytes to transmit from the buffer.
 * @param[in] timeout_ms Timeout in milliseconds to wait for buffer to be ready. Use portMAX_DELAY for infinite wait.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, ESP_ERR_TIMEOUT if timeout occurs, or an error from the driver.
 */
esp_err_t star_bus_spi_peripheral_transmit(const star_bus_manager_t* manager,
                                           const char*               name,
                                           const void*               tx_buffer,
                                           size_t                    len,
                                           uint32_t                  timeout_ms);

/**
 * @brief Perform a full-duplex transaction (receive and transmit simultaneously).
 *
 * This function blocks until the controller completes a transaction.
 * Both buffers can be NULL if only receive or transmit is needed.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI peripheral configuration. Must not be NULL.
 * @param[in] tx_buffer  Pointer to the data buffer to transmit. Can be NULL for receive-only.
 * @param[out] rx_buffer Pointer to the buffer where received data will be stored. Can be NULL for transmit-only.
 * @param[in] len        Number of bytes to transfer (both transmit and receive).
 * @param[in] timeout_ms Timeout in milliseconds. Use portMAX_DELAY for infinite wait.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, ESP_ERR_TIMEOUT if timeout occurs, or an error from the driver.
 */
esp_err_t star_bus_spi_peripheral_transceive(const star_bus_manager_t* manager,
                                             const char*               name,
                                             const void*               tx_buffer,
                                             void*                     rx_buffer,
                                             size_t                    len,
                                             uint32_t                  timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_SPI_PERIPHERAL_H */
