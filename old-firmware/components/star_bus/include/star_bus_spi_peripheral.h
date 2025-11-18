/* esp32-firmware/components/star_bus/include/star_bus_spi_peripheral.h */

#ifndef STAR_COMPONENT_BUS_SPI_PERIPHERAL_H
#define STAR_COMPONENT_BUS_SPI_PERIPHERAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_slave.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/* --- SPI Pin Naming: COPI/CIPO (OSHWA Standard) --- */
/* This component uses OSHWA (Open Source Hardware Association) standard SPI terminology:
 * - COPI (Controller Out, Peripheral In): Data line where the controller sends data to the peripheral
 * - CIPO (Controller In, Peripheral Out): Data line where the controller receives data from the peripheral
 *
 * This modern terminology replaces the outdated master/slave language:
 * - COPI replaces MOSI (Master Out, Slave In)
 * - CIPO replaces MISO (Master In, Slave Out)
 *
 * Note: The ESP-IDF SPI slave driver still uses mosi_io_num/miso_io_num internally, which we map to COPI/CIPO.
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
