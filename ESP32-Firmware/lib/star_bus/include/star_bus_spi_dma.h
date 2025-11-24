/* esp32-firmware/components/star_bus/include/star_bus_spi_dma.h */

#ifndef STAR_BUS_SPI_DMA_H
#define STAR_BUS_SPI_DMA_H

#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_spi_dma.h
 * @brief DMA-accelerated SPI operations for high-speed data transfers
 *
 * This module provides DMA-backed SPI operations for maximum throughput
 * when transferring large amounts of data. SPI DMA is especially beneficial
 * for high-speed peripherals like displays, flash memory, and SD cards.
 *
 * Features:
 * - Zero-copy buffer management
 * - Full-duplex DMA transfers
 * - Automatic fallback to non-DMA for small transfers
 * - Configurable DMA threshold
 * - Support for transmit, receive, and transceive operations
 *
 * Performance:
 * - Up to 1000x faster for large transfers at high SPI speeds
 * - Near-zero CPU usage during transfer
 * - Supports up to 80MHz SPI clock with DMA
 *
 * Limitations:
 * - DMA buffers must be in DMA-capable memory
 * - 4-byte alignment requirement
 * - Maximum single transfer: 4092 bytes
 */

/* --- Constants --- */

/**
 * @brief Minimum transfer size to use DMA (bytes)
 */
#define STAR_SPI_DMA_MIN_SIZE (64)

/**
 * @brief Maximum DMA transfer size (bytes)
 */
#define STAR_SPI_DMA_MAX_SIZE (4092)

/**
 * @brief DMA buffer alignment requirement (bytes)
 */
#define STAR_SPI_DMA_ALIGNMENT (4)

/* --- Types --- */

/**
 * @brief DMA transfer complete callback function type
 *
 * @param result    Transfer result (ESP_OK on success)
 * @param context   User context pointer
 */
typedef void (*star_spi_dma_callback_t)(esp_err_t result, void* context);

/**
 * @brief DMA configuration for SPI bus
 */
typedef struct {
  bool     enabled;           /**< Enable/disable DMA for this bus */
  uint32_t min_transfer_size; /**< Minimum size to use DMA (bytes) */
  uint8_t  dma_channel;       /**< DMA channel to use (1-2 for ESP32) */
} star_spi_dma_config_t;

/* --- DMA Memory Allocation --- */

/**
 * @brief Allocate DMA-capable memory for SPI transfers
 *
 * Allocates memory that can be used for DMA transfers. This memory
 * is properly aligned and located in DMA-accessible regions.
 *
 * @param[in] size  Size of buffer to allocate (bytes)
 *
 * @return Pointer to allocated buffer, or NULL on failure
 *
 * @note Must be freed with star_spi_dma_free()
 * @note Buffer is cache-aligned and DMA-capable
 */
void* star_spi_dma_malloc(size_t size);

/**
 * @brief Free DMA-capable memory
 *
 * @param[in] ptr  Pointer to buffer allocated with star_spi_dma_malloc()
 */
void star_spi_dma_free(void* ptr);

/* --- DMA Operations --- */

/**
 * @brief Transmit data via SPI using DMA
 *
 * Performs a DMA-accelerated SPI transmit. Automatically falls back
 * to non-DMA operation if transfer is too small or DMA is not available.
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of SPI bus
 * @param[in] data      Pointer to transmit buffer (must be DMA-capable for large transfers)
 * @param[in] length    Number of bytes to transmit
 * @param[in] callback  Optional completion callback (NULL for blocking)
 * @param[in] context   User context for callback
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note If callback is NULL, operation is blocking
 * @note If callback is provided, operation is asynchronous
 * @note Data buffer must remain valid until callback is called
 */
esp_err_t star_bus_spi_transmit_dma(star_bus_manager_t*     manager,
                                    const char*             bus_name,
                                    const uint8_t*          data,
                                    size_t                  length,
                                    star_spi_dma_callback_t callback,
                                    void*                   context);

/**
 * @brief Receive data via SPI using DMA
 *
 * Performs a DMA-accelerated SPI receive. Automatically falls back
 * to non-DMA operation if transfer is too small or DMA is not available.
 *
 * @param[in]  manager   Pointer to initialized bus manager
 * @param[in]  bus_name  Name of SPI bus
 * @param[out] data      Pointer to receive buffer (must be DMA-capable for large transfers)
 * @param[in]  length    Number of bytes to receive
 * @param[in]  callback  Optional completion callback (NULL for blocking)
 * @param[in]  context   User context for callback
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note If callback is NULL, operation is blocking
 * @note If callback is provided, operation is asynchronous
 * @note Data buffer must remain valid until callback is called
 */
esp_err_t star_bus_spi_receive_dma(star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   uint8_t*                data,
                                   size_t                  length,
                                   star_spi_dma_callback_t callback,
                                   void*                   context);

/**
 * @brief Perform full-duplex SPI transceive using DMA
 *
 * Simultaneously transmits and receives data using DMA. This is the
 * most efficient way to communicate with full-duplex SPI devices.
 *
 * @param[in]  manager   Pointer to initialized bus manager
 * @param[in]  bus_name  Name of SPI bus
 * @param[in]  tx_data   Pointer to transmit buffer (must be DMA-capable)
 * @param[out] rx_data   Pointer to receive buffer (must be DMA-capable)
 * @param[in]  length    Number of bytes to transfer
 * @param[in]  callback  Optional completion callback (NULL for blocking)
 * @param[in]  context   User context for callback
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Both buffers must be DMA-capable for large transfers
 * @note If callback is NULL, operation is blocking
 * @note Buffers must remain valid until callback is called
 */
esp_err_t star_bus_spi_transceive_dma(star_bus_manager_t*     manager,
                                      const char*             bus_name,
                                      const uint8_t*          tx_data,
                                      uint8_t*                rx_data,
                                      size_t                  length,
                                      star_spi_dma_callback_t callback,
                                      void*                   context);

/* --- Configuration --- */

/**
 * @brief Configure DMA for an SPI bus
 *
 * Enables or configures DMA operation for a specific SPI bus.
 *
 * @param[in] manager  Pointer to initialized bus manager
 * @param[in] bus_name Name of SPI bus to configure
 * @param[in] config   Pointer to DMA configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Must be called before using DMA operations
 * @note Can be called multiple times to reconfigure
 */
esp_err_t star_bus_spi_configure_dma(star_bus_manager_t*          manager,
                                     const char*                  bus_name,
                                     const star_spi_dma_config_t* config);

/**
 * @brief Get DMA configuration for an SPI bus
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of SPI bus
 * @param[out] config   Pointer to store current DMA configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_spi_get_dma_config(const star_bus_manager_t* manager,
                                      const char*               bus_name,
                                      star_spi_dma_config_t*    config);

/* --- Utility Functions --- */

/**
 * @brief Check if a buffer is DMA-capable
 *
 * Verifies that a buffer is properly aligned and located in
 * DMA-accessible memory.
 *
 * @param[in] buffer  Pointer to buffer to check
 * @param[in] size    Size of buffer (bytes)
 *
 * @return true if buffer is DMA-capable, false otherwise
 */
bool star_spi_is_dma_capable(const void* buffer, size_t size);

/**
 * @brief Get DMA transfer statistics
 *
 * @param[in]  manager       Pointer to initialized bus manager
 * @param[in]  bus_name      Name of SPI bus
 * @param[out] dma_transfers Total number of DMA transfers
 * @param[out] dma_bytes     Total bytes transferred via DMA
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_spi_get_dma_stats(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     uint32_t*                 dma_transfers,
                                     uint64_t*                 dma_bytes);

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_SPI_DMA_H */
