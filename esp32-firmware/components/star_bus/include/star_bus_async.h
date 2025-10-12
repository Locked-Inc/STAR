/* esp32-firmware/components/star_bus/include/star_bus_async.h */

#ifndef STAR_BUS_ASYNC_H
#define STAR_BUS_ASYNC_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stddef.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_async.h
 * @brief Asynchronous (non-blocking) bus operations with callback support
 *
 * This module provides asynchronous operations for all bus types, enabling
 * non-blocking I/O and event-driven programming patterns. Operations complete
 * in the background and invoke user callbacks upon completion.
 *
 * Features:
 * - Non-blocking I2C, SPI, SMBus, and PMBus operations
 * - Callback-based completion notification
 * - Event group integration for synchronization
 * - Queue-based operation management
 * - Support for concurrent async operations
 * - Timeout handling
 *
 * Benefits:
 * - Better system responsiveness
 * - Concurrent operations on different buses
 * - Integration with FreeRTOS event loops
 * - Reduced blocking time in critical sections
 *
 * Use Cases:
 * - UI applications requiring responsiveness
 * - Multi-sensor data acquisition
 * - High-throughput data logging
 * - Real-time control systems
 */

/* --- Constants --- */

/**
 * @brief Maximum number of pending async operations per bus
 */
#define STAR_ASYNC_MAX_PENDING (8)

/**
 * @brief Default timeout for async operations (ms)
 */
#define STAR_ASYNC_DEFAULT_TIMEOUT_MS (5000)

/* --- Types --- */

/**
 * @brief Async operation handle
 */
typedef void* star_async_handle_t;

/**
 * @brief Async operation type
 */
typedef enum {
  STAR_ASYNC_OP_I2C_READ,       /**< I2C read operation */
  STAR_ASYNC_OP_I2C_WRITE,      /**< I2C write operation */
  STAR_ASYNC_OP_SPI_TRANSMIT,   /**< SPI transmit operation */
  STAR_ASYNC_OP_SPI_RECEIVE,    /**< SPI receive operation */
  STAR_ASYNC_OP_SPI_TRANSCEIVE, /**< SPI transceive operation */
  STAR_ASYNC_OP_SMBUS_READ,     /**< SMBus read operation */
  STAR_ASYNC_OP_SMBUS_WRITE,    /**< SMBus write operation */
} star_async_op_type_t;

/**
 * @brief Async operation status
 */
typedef enum {
  STAR_ASYNC_STATUS_PENDING,   /**< Operation pending */
  STAR_ASYNC_STATUS_RUNNING,   /**< Operation in progress */
  STAR_ASYNC_STATUS_COMPLETE,  /**< Operation completed successfully */
  STAR_ASYNC_STATUS_ERROR,     /**< Operation failed */
  STAR_ASYNC_STATUS_TIMEOUT,   /**< Operation timed out */
  STAR_ASYNC_STATUS_CANCELLED, /**< Operation was cancelled */
} star_async_status_t;

/**
 * @brief Async operation completion callback
 *
 * Called when an async operation completes, fails, or times out.
 *
 * @param handle  Handle to the completed operation
 * @param status  Final status of the operation
 * @param result  ESP error code (ESP_OK on success)
 * @param context User context pointer
 *
 * @note Callback is invoked from a FreeRTOS task context
 * @note Keep callback execution time short
 * @note Do not call blocking functions from callback
 */
typedef void (*star_async_callback_t)(star_async_handle_t handle,
                                      star_async_status_t status,
                                      esp_err_t           result,
                                      void*               context);

/**
 * @brief Async operation configuration
 */
typedef struct {
  uint32_t              timeout_ms; /**< Operation timeout (0 = no timeout) */
  star_async_callback_t callback;   /**< Completion callback (required) */
  void*                 context;    /**< User context for callback */
  uint8_t               priority;   /**< Operation priority (0-255, higher = more priority) */
} star_async_config_t;

/* --- Async I2C Operations --- */

/**
 * @brief Async I2C write operation
 *
 * Initiates a non-blocking I2C write. Callback is invoked upon completion.
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[in]  data     Pointer to data to write
 * @param[in]  length   Number of bytes to write
 * @param[in]  command  Command byte (register address)
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional, can be NULL)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 *
 * @note Data buffer must remain valid until callback is invoked
 * @note Handle can be used to cancel or query operation status
 */
esp_err_t star_bus_i2c_write_async(star_bus_manager_t*        manager,
                                   const char*                bus_name,
                                   const uint8_t*             data,
                                   size_t                     length,
                                   uint8_t                    command,
                                   const star_async_config_t* config,
                                   star_async_handle_t*       handle);

/**
 * @brief Async I2C read operation
 *
 * Initiates a non-blocking I2C read. Callback is invoked upon completion.
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[out] data     Pointer to buffer for received data
 * @param[in]  length   Number of bytes to read
 * @param[in]  command  Command byte (register address)
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 *
 * @note Data buffer must remain valid until callback is invoked
 */
esp_err_t star_bus_i2c_read_async(star_bus_manager_t*        manager,
                                  const char*                bus_name,
                                  uint8_t*                   data,
                                  size_t                     length,
                                  uint8_t                    command,
                                  const star_async_config_t* config,
                                  star_async_handle_t*       handle);

/* --- Async SPI Operations --- */

/**
 * @brief Async SPI transmit operation
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of SPI bus
 * @param[in]  data     Pointer to data to transmit
 * @param[in]  length   Number of bytes to transmit
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 */
esp_err_t star_bus_spi_transmit_async(star_bus_manager_t*        manager,
                                      const char*                bus_name,
                                      const uint8_t*             data,
                                      size_t                     length,
                                      const star_async_config_t* config,
                                      star_async_handle_t*       handle);

/**
 * @brief Async SPI receive operation
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of SPI bus
 * @param[out] data     Pointer to buffer for received data
 * @param[in]  length   Number of bytes to receive
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 */
esp_err_t star_bus_spi_receive_async(star_bus_manager_t*        manager,
                                     const char*                bus_name,
                                     uint8_t*                   data,
                                     size_t                     length,
                                     const star_async_config_t* config,
                                     star_async_handle_t*       handle);

/**
 * @brief Async SPI transceive operation
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of SPI bus
 * @param[in]  tx_data  Pointer to data to transmit
 * @param[out] rx_data  Pointer to buffer for received data
 * @param[in]  length   Number of bytes to transfer
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 */
esp_err_t star_bus_spi_transceive_async(star_bus_manager_t*        manager,
                                        const char*                bus_name,
                                        const uint8_t*             tx_data,
                                        uint8_t*                   rx_data,
                                        size_t                     length,
                                        const star_async_config_t* config,
                                        star_async_handle_t*       handle);

/* --- Async SMBus Operations --- */

/**
 * @brief Async SMBus read byte operation
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[in]  addr     SMBus device address
 * @param[in]  command  SMBus command code
 * @param[out] data     Pointer to store received byte
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 */
esp_err_t star_smbus_read_byte_async(star_bus_manager_t*        manager,
                                     const char*                bus_name,
                                     uint8_t                    addr,
                                     uint8_t                    command,
                                     uint8_t*                   data,
                                     const star_async_config_t* config,
                                     star_async_handle_t*       handle);

/**
 * @brief Async SMBus write byte operation
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[in]  addr     SMBus device address
 * @param[in]  command  SMBus command code
 * @param[in]  data     Byte to write
 * @param[in]  config   Async operation configuration
 * @param[out] handle   Output handle to track operation (optional)
 *
 * @return ESP_OK if operation queued successfully, error code otherwise
 */
esp_err_t star_smbus_write_byte_async(star_bus_manager_t*        manager,
                                      const char*                bus_name,
                                      uint8_t                    addr,
                                      uint8_t                    command,
                                      uint8_t                    data,
                                      const star_async_config_t* config,
                                      star_async_handle_t*       handle);

/* --- Operation Management --- */

/**
 * @brief Get status of an async operation
 *
 * @param[in] handle  Handle to async operation
 *
 * @return Current status of the operation
 */
star_async_status_t star_async_get_status(star_async_handle_t handle);

/**
 * @brief Wait for an async operation to complete
 *
 * Blocks until the operation completes or timeout expires.
 *
 * @param[in] handle     Handle to async operation
 * @param[in] timeout_ms Timeout in milliseconds (0 = wait forever)
 *
 * @return ESP_OK if operation completed successfully, error code otherwise
 *
 * @note Callback will still be invoked even when using this function
 */
esp_err_t star_async_wait(star_async_handle_t handle, uint32_t timeout_ms);

/**
 * @brief Cancel a pending or running async operation
 *
 * @param[in] handle  Handle to async operation
 *
 * @return ESP_OK if operation cancelled, ESP_ERR_INVALID_STATE if already complete
 *
 * @note Callback will be invoked with STAR_ASYNC_STATUS_CANCELLED
 */
esp_err_t star_async_cancel(star_async_handle_t handle);

/**
 * @brief Get result of completed async operation
 *
 * @param[in]  handle  Handle to async operation
 * @param[out] result  Pointer to store ESP error code result
 *
 * @return ESP_OK if result retrieved, error code otherwise
 */
esp_err_t star_async_get_result(star_async_handle_t handle, esp_err_t* result);

/**
 * @brief Free async operation handle
 *
 * Releases resources associated with the operation handle.
 *
 * @param[in] handle  Handle to async operation
 *
 * @note Only call after operation is complete
 * @note Not required if handle was NULL in start function
 */
void star_async_free_handle(star_async_handle_t handle);

/* --- Event Group Integration --- */

/**
 * @brief Set event group bit when operation completes
 *
 * Allows integration with FreeRTOS event groups for synchronization.
 *
 * @param[in] handle         Handle to async operation
 * @param[in] event_group    FreeRTOS event group handle
 * @param[in] complete_bit   Bit to set on successful completion
 * @param[in] error_bit      Bit to set on error (optional, 0 = none)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_async_set_event_bits(star_async_handle_t handle,
                                    EventGroupHandle_t  event_group,
                                    EventBits_t         complete_bit,
                                    EventBits_t         error_bit);

/* --- Statistics --- */

/**
 * @brief Get async operation statistics for a bus
 *
 * @param[in]  manager         Pointer to initialized bus manager
 * @param[in]  bus_name        Name of bus
 * @param[out] pending_ops     Number of currently pending operations
 * @param[out] completed_ops   Total completed operations
 * @param[out] failed_ops      Total failed operations
 * @param[out] cancelled_ops   Total cancelled operations
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_async_get_stats(const star_bus_manager_t* manager,
                               const char*               bus_name,
                               uint32_t*                 pending_ops,
                               uint64_t*                 completed_ops,
                               uint64_t*                 failed_ops,
                               uint64_t*                 cancelled_ops);

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_ASYNC_H */
