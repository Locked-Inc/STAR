/**
 * @file star_bus_spi.h
 * @brief SPI controller protocol API for the bus manager
 * @details
 * Provides interface for SPI controller mode operations through the bus manager. Supports
 * configurable clock speeds, COPI/CIPO pinout, chip select management, and DMA transfers
 * for high-speed communication with peripherals like displays and flash memory.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_SPI_H
#define STAR_COMPONENT_BUS_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_spi.h
 * @brief SPI bus operations through the unified bus manager
 *
 * This module provides SPI transmit, receive, and full-duplex transfer operations
 * that work through the bus manager. All operations find the bus configuration by
 * name and use the configured SPI device handle.
 *
 * Key Features:
 * - Transmit-only operations (write to device)
 * - Receive-only operations (read from device)
 * - Full-duplex transfers (simultaneous read/write)
 * - User flags for DC pin control and other custom signaling
 * - Thread-safe through bus manager mutex
 *
 * SPI Pin Naming: COPI/CIPO (OSHWA Standard)
 * This component uses OSHWA standard SPI terminology:
 * - COPI (Controller Out, Peripheral In)
 * - CIPO (Controller In, Peripheral Out)
 *
 * Example Usage:
 * @code
 * // === Setup (see star_bus_manager.h and star_bus_config.h for full setup) ===
 *
 * #include "star_bus_spi.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Configure SPI device for LCD display
 * spi_device_interface_config_t lcd_dev_cfg = {
 *     .clock_speed_hz = 26 * 1000 * 1000,  // 26 MHz
 *     .mode = 0,                            // SPI mode 0
 *     .spics_io_num = GPIO_NUM_5,           // CS pin
 *     .queue_size = 7,
 *     .flags = SPI_DEVICE_NO_DUMMY,
 * };
 *
 * star_bus_config_t* lcd_bus = star_bus_config_create_spi_device(
 *     "lcd_spi", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
 *     SPI_DMA_CH_AUTO, &lcd_dev_cfg);
 * star_bus_manager_add_bus(&bus_manager, lcd_bus);
 *
 *
 * // === Transmit Data (Write Only) ===
 *
 * // Send command byte to LCD (DC pin low via user flags)
 * uint8_t cmd = 0x2C;  // Write RAM command
 * esp_err_t ret = star_bus_spi_transmit(
 *     &bus_manager,
 *     "lcd_spi",
 *     &cmd,
 *     1,                // 1 byte
 *     0                 // User flags (could use for DC pin control)
 * );
 *
 * // Send multiple bytes
 * uint8_t init_sequence[] = {0x01, 0x11, 0x29};  // Reset, Sleep out, Display on
 * star_bus_spi_transmit(&bus_manager, "lcd_spi", init_sequence, 3, 0);
 *
 * // Send pixel data (DC pin high for data)
 * #define SPI_DC_DATA_FLAG (1 << 0)  // Custom flag for DC pin
 * uint8_t pixel_data[320 * 2];  // RGB565 row
 * star_bus_spi_transmit(&bus_manager, "lcd_spi", pixel_data, sizeof(pixel_data), SPI_DC_DATA_FLAG);
 *
 *
 * // === Receive Data (Read Only) ===
 *
 * // Read status register from SPI flash
 * uint8_t status;
 * ret = star_bus_spi_receive(
 *     &bus_manager,
 *     "flash_spi",
 *     &status,
 *     1,               // 1 byte
 *     0                // No flags
 * );
 * ESP_LOGI(TAG, "Flash status: 0x%02X", status);
 *
 * // Read multiple bytes
 * uint8_t read_buffer[256];
 * star_bus_spi_receive(&bus_manager, "flash_spi", read_buffer, 256, 0);
 *
 *
 * // === Full-Duplex Transfer (Simultaneous Read/Write) ===
 *
 * // Full-duplex transfer with SPI flash
 * uint8_t tx_cmd[] = {0x9F, 0x00, 0x00, 0x00};  // Read JEDEC ID + dummy bytes
 * uint8_t rx_data[4];
 *
 * ret = star_bus_spi_transfer(
 *     &bus_manager,
 *     "flash_spi",
 *     tx_cmd,
 *     rx_data,
 *     4,               // Transfer 4 bytes both ways
 *     0                // No flags
 * );
 *
 * // rx_data[1..3] contains manufacturer and device ID
 * ESP_LOGI(TAG, "Flash ID: %02X %02X %02X", rx_data[1], rx_data[2], rx_data[3]);
 *
 *
 * // === Working with SD Card over SPI ===
 *
 * // Send SD card command
 * uint8_t sd_cmd[] = {0x40 | 0, 0x00, 0x00, 0x00, 0x00, 0x95};  // CMD0 + CRC
 * star_bus_spi_transmit(&bus_manager, "sd_spi", sd_cmd, 6, 0);
 *
 * // Read response
 * uint8_t response;
 * do {
 *     star_bus_spi_receive(&bus_manager, "sd_spi", &response, 1, 0);
 * } while (response == 0xFF);
 *
 * ESP_LOGI(TAG, "SD response: 0x%02X", response);
 *
 *
 * // === Multiple SPI Devices ===
 *
 * // Create separate bus configs for each device (different CS pins)
 * spi_device_interface_config_t flash_cfg = {
 *     .clock_speed_hz = 40 * 1000 * 1000,
 *     .mode = 0,
 *     .spics_io_num = GPIO_NUM_15,  // Different CS pin
 *     .queue_size = 4,
 * };
 *
 * star_bus_config_t* flash_bus = star_bus_config_create_spi_device(
 *     "flash_spi", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
 *     SPI_DMA_CH_AUTO, &flash_cfg);
 *
 * star_bus_manager_add_bus(&bus_manager, flash_bus);
 *
 * // Use devices independently by name
 * star_bus_spi_transmit(&bus_manager, "lcd_spi", lcd_data, lcd_len, 0);
 * star_bus_spi_transmit(&bus_manager, "flash_spi", flash_cmd, cmd_len, 0);
 *
 *
 * // === Cleanup ===
 *
 * star_bus_manager_remove_bus(&bus_manager, "lcd_spi");
 * star_bus_config_destroy(lcd_bus);
 * @endcode
 */

/* --- SPI Pin Naming: COPI/CIPO (OSHWA Standard) --- */
/* This component uses OSHWA (Open Source Hardware Association) standard SPI terminology:
 * - COPI (Controller Out, Peripheral In): Data line where the controller sends data to the peripheral
 * - CIPO (Controller In, Peripheral Out): Data line where the controller receives data from the peripheral
 *
 *
 * Note: The ESP-IDF still uses legacy mosi_io_num/miso_io_num field names internally, which we map to COPI/CIPO.
 */

/* --- Public Functions --- */

/**
 * @brief Initialize default SPI operations function pointers within a config structure.
 *        Typically called internally by star_bus_config_create_spi_device.
 *        Assigns default implementations for transmit, receive, and transfer.
 *
 * @param[out] ops Pointer to SPI operations structure to initialize. Must not be NULL.
 */
void star_bus_spi_init_default_ops(star_spi_ops_t* ops);

/**
 * @brief Transmit data to an SPI device specified by a bus configuration.
 *
 * Finds the bus configuration by name and calls its transmit operation.
 * This function typically uses queued transactions for performance.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI device configuration. Must not be NULL.
 * @param[in] tx_buffer  Pointer to the data buffer to transmit. Must not be NULL.
 * @param[in] len        Number of bytes to transmit from the buffer.
 * @param[in] user_flags User-defined flags passed to the underlying transaction
 *                       (e.g., SPI_TRANS_MODE_CMD, SPI_TRANS_MODE_DATA for DC pin control).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver.
 */
esp_err_t star_bus_spi_transmit(const star_bus_manager_t* manager,
                                const char*               name,
                                const void*               tx_buffer,
                                size_t                    len,
                                uint32_t                  user_flags);

/**
 * @brief Receive data from an SPI device specified by a bus configuration.
 *
 * Finds the bus configuration by name and calls its receive operation.
 * This function typically uses queued transactions for performance.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI device configuration. Must not be NULL.
 * @param[out] rx_buffer Pointer to the buffer where received data will be stored. Must not be NULL.
 * @param[in] len        Number of bytes to receive into the buffer.
 * @param[in] user_flags User-defined flags passed to the underlying transaction.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver.
 */
esp_err_t star_bus_spi_receive(const star_bus_manager_t* manager,
                               const char*               name,
                               void*                     rx_buffer,
                               size_t                    len,
                               uint32_t                  user_flags);

/**
 * @brief Perform a full-duplex transfer with an SPI device specified by a bus configuration.
 *
 * Finds the bus configuration by name and calls its transfer operation.
 * This function typically uses queued transactions for performance.
 *
 * @param[in] manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name       Name of the SPI device configuration. Must not be NULL.
 * @param[in] tx_buffer  Pointer to the data buffer to transmit. Must not be NULL.
 * @param[out] rx_buffer Pointer to the buffer where received data will be stored. Must not be NULL.
 * @param[in] len        Number of bytes to transfer (both transmit and receive).
 * @param[in] user_flags User-defined flags passed to the underlying transaction.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver.
 */
esp_err_t star_bus_spi_transfer(const star_bus_manager_t* manager,
                                const char*               name,
                                const void*               tx_buffer,
                                void*                     rx_buffer,
                                size_t                    len,
                                uint32_t                  user_flags);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_SPI_H */
