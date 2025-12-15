/* lib/star_bus/include/star_bus_async.h */

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
 *
 * Example Usage:
 * @code
 * // === Basic Async I2C Read with Callback ===
 *
 * #include "star_bus_async.h"
 * #include "star_bus_manager.h"
 *
 * // Callback function for async completion
 * void sensor_read_complete(star_async_handle_t handle,
 *                           star_async_status_t status,
 *                           esp_err_t result,
 *                           void* context) {
 *     uint8_t* sensor_data = (uint8_t*)context;
 *
 *     if (status == STAR_ASYNC_STATUS_COMPLETE && result == ESP_OK) {
 *         ESP_LOGI(TAG, "Sensor data: %02X %02X", sensor_data[0], sensor_data[1]);
 *     } else if (status == STAR_ASYNC_STATUS_TIMEOUT) {
 *         ESP_LOGW(TAG, "Sensor read timed out");
 *     } else {
 *         ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(result));
 *     }
 *
 *     // Free handle when done
 *     star_async_free_handle(handle);
 * }
 *
 * // Start async read
 * static uint8_t sensor_buffer[6];
 * star_async_config_t async_cfg = {
 *     .timeout_ms = 1000,
 *     .callback = sensor_read_complete,
 *     .context = sensor_buffer,
 *     .priority = 0
 * };
 *
 * star_async_handle_t handle;
 * esp_err_t ret = star_bus_i2c_read_async(
 *     &bus_manager,
 *     "imu_i2c",
 *     sensor_buffer,
 *     6,               // Read 6 bytes
 *     0x3B,            // Accel data register
 *     &async_cfg,
 *     &handle
 * );
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to start async read");
 * }
 * // Callback will be invoked when operation completes
 *
 *
 * // === Async I2C Write ===
 *
 * uint8_t config_data[] = {0x00};  // Wake up MPU6050
 * star_async_config_t write_cfg = {
 *     .timeout_ms = 500,
 *     .callback = config_complete_callback,
 *     .context = NULL,
 *     .priority = 0
 * };
 *
 * star_bus_i2c_write_async(
 *     &bus_manager,
 *     "imu_i2c",
 *     config_data,
 *     1,
 *     0x6B,            // PWR_MGMT_1 register
 *     &write_cfg,
 *     NULL             // Don't need handle
 * );
 *
 *
 * // === Async SPI Operations ===
 *
 * // Async SPI transmit
 * uint8_t lcd_command[] = {0x2C};  // Write RAM
 * star_bus_spi_transmit_async(
 *     &bus_manager,
 *     "lcd_spi",
 *     lcd_command,
 *     1,
 *     &async_cfg,
 *     NULL
 * );
 *
 * // Async SPI receive
 * static uint8_t flash_id[4];
 * star_bus_spi_receive_async(
 *     &bus_manager,
 *     "flash_spi",
 *     flash_id,
 *     4,
 *     &async_cfg,
 *     &handle
 * );
 *
 * // Async full-duplex SPI
 * static uint8_t tx_buf[8], rx_buf[8];
 * star_bus_spi_transceive_async(
 *     &bus_manager,
 *     "flash_spi",
 *     tx_buf,
 *     rx_buf,
 *     8,
 *     &async_cfg,
 *     &handle
 * );
 *
 *
 * // === Wait for Async Operation ===
 *
 * // Start operation
 * star_async_handle_t op_handle;
 * star_bus_i2c_read_async(&bus_manager, "sensor_i2c", buffer, 2, 0x00, &async_cfg, &op_handle);
 *
 * // Do other work while operation runs...
 * process_ui_events();
 *
 * // Now wait for completion (with timeout)
 * ret = star_async_wait(op_handle, 2000);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Operation completed");
 * }
 *
 * // Check status
 * star_async_status_t status = star_async_get_status(op_handle);
 * if (status == STAR_ASYNC_STATUS_COMPLETE) {
 *     // Process data
 * }
 *
 * star_async_free_handle(op_handle);
 *
 *
 * // === Cancel Pending Operation ===
 *
 * star_async_handle_t cancel_handle;
 * star_bus_i2c_read_async(&bus_manager, "slow_sensor", buffer, 100, 0x00, &async_cfg, &cancel_handle);
 *
 * // Changed our mind - cancel it
 * ret = star_async_cancel(cancel_handle);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Operation cancelled");
 * }
 * // Callback will be invoked with STAR_ASYNC_STATUS_CANCELLED
 *
 *
 * // === Event Group Integration ===
 *
 * #define SENSOR_COMPLETE_BIT (1 << 0)
 * #define SENSOR_ERROR_BIT    (1 << 1)
 *
 * EventGroupHandle_t sensor_events = xEventGroupCreate();
 *
 * star_async_handle_t event_handle;
 * star_bus_i2c_read_async(&bus_manager, "sensor_i2c", buffer, 6, 0x00, &async_cfg, &event_handle);
 *
 * // Set event bits on completion
 * star_async_set_event_bits(event_handle, sensor_events, SENSOR_COMPLETE_BIT, SENSOR_ERROR_BIT);
 *
 * // Wait on event group (can wait for multiple operations)
 * EventBits_t bits = xEventGroupWaitBits(
 *     sensor_events,
 *     SENSOR_COMPLETE_BIT | SENSOR_ERROR_BIT,
 *     pdTRUE,   // Clear bits on exit
 *     pdFALSE,  // Wait for any bit
 *     pdMS_TO_TICKS(5000)
 * );
 *
 * if (bits & SENSOR_COMPLETE_BIT) {
 *     ESP_LOGI(TAG, "Sensor read complete");
 * }
 *
 *
 * // === Concurrent Multi-Sensor Reads ===
 *
 * // Start multiple async operations in parallel
 * static uint8_t accel_data[6], gyro_data[6], mag_data[6];
 *
 * star_bus_i2c_read_async(&bus_manager, "accel_i2c", accel_data, 6, 0x32, &async_cfg, NULL);
 * star_bus_i2c_read_async(&bus_manager, "gyro_i2c", gyro_data, 6, 0x43, &async_cfg, NULL);
 * star_bus_i2c_read_async(&bus_manager, "mag_i2c", mag_data, 6, 0x03, &async_cfg, NULL);
 *
 * // All three reads happen concurrently!
 * // Callbacks will be invoked as each completes
 *
 *
 * // === Get Async Statistics ===
 *
 * uint32_t pending;
 * uint64_t completed, failed, cancelled;
 *
 * star_async_get_stats(&bus_manager, "imu_i2c", &pending, &completed, &failed, &cancelled);
 * ESP_LOGI(TAG, "I2C stats: pending=%lu, completed=%llu, failed=%llu",
 *          pending, completed, failed);
 * @endcode
 */

/* --- Constants --- */

/**
 * @brief Async operation constants
 *
 * Using enum for type safety while maintaining compile-time constant behavior
 * required for struct initializers and array sizes in C.
 */
enum {
  /** Maximum number of pending async operations per bus */
  STAR_ASYNC_MAX_PENDING = 8,

  /** Default timeout for async operations (ms) */
  STAR_ASYNC_DEFAULT_TIMEOUT_MS = 5000,

  /** Special value for star_async_wait() to wait indefinitely */
  STAR_ASYNC_WAIT_FOREVER = UINT32_MAX,
};

/* --- Types --- */

/**
 * @brief Async operation handle
 */
typedef void* star_async_handle_t;

/**
 * @brief Async operation type
 */
typedef enum {
  k_star_async_op_i2c_read,       /**< I2C read operation */
  k_star_async_op_i2c_write,      /**< I2C write operation */
  k_star_async_op_spi_transmit,   /**< SPI transmit operation */
  k_star_async_op_spi_receive,    /**< SPI receive operation */
  k_star_async_op_spi_transceive, /**< SPI transceive operation */
  k_star_async_op_smbus_read,     /**< SMBus read operation */
  k_star_async_op_smbus_write,    /**< SMBus write operation */
} star_async_op_type_t;

/**
 * @brief Async operation status
 */
typedef enum {
  k_star_async_status_pending,   /**< Operation pending */
  k_star_async_status_running,   /**< Operation in progress */
  k_star_async_status_complete,  /**< Operation completed successfully */
  k_star_async_status_error,     /**< Operation failed */
  k_star_async_status_timeout,   /**< Operation timed out */
  k_star_async_status_cancelled, /**< Operation was cancelled */
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
 * @param[in] timeout_ms Timeout in milliseconds:
 *                       - 0 = no wait (immediate return, returns ESP_ERR_TIMEOUT if not ready)
 *                       - STAR_ASYNC_WAIT_FOREVER = wait indefinitely
 *                       - other = wait for specified milliseconds
 *
 * @return ESP_OK if operation completed successfully, ESP_ERR_TIMEOUT if timeout
 *         expired before completion, or other error code on failure
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
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if handle is NULL
 *
 * @note Only call after operation is complete
 * @note Not required if handle was NULL in start function
 */
esp_err_t star_async_free_handle(star_async_handle_t handle);

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
