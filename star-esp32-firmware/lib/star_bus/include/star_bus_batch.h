/* lib/star_bus/include/star_bus_batch.h */

#ifndef STAR_BUS_BATCH_H
#define STAR_BUS_BATCH_H

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_batch.h
 * @brief Transaction batching for atomic multi-operation sequences
 *
 * This module provides transaction batching capabilities, allowing multiple
 * bus operations to be grouped together and executed atomically. This is
 * particularly useful for:
 * - Multi-register reads/writes that must be consistent
 * - Complex device initialization sequences
 * - Atomic read-modify-write operations
 * - Reducing overhead for multiple sequential operations
 *
 * Features:
 * - Atomic execution of multiple operations
 * - Support for I2C, SPI, and SMBus operations
 * - Rollback support on error (bus-specific)
 * - Configurable operation timeouts
 * - Statistics tracking
 *
 * Use Cases:
 * - Reading multiple sensor registers atomically
 * - Multi-step device configuration
 * - Burst read/write operations
 * - Complex state machine implementations
 *
 * Example Usage:
 * @code
 * // === Basic Batch: Read Multiple IMU Registers ===
 *
 * #include "star_bus_batch.h"
 * #include "star_bus_manager.h"
 *
 * // Buffers for sensor data
 * uint8_t accel_data[6];
 * uint8_t gyro_data[6];
 * uint8_t temp_data[2];
 *
 * // Create batch with default config
 * star_batch_config_t batch_cfg = STAR_BATCH_CONFIG_DEFAULT();
 * star_batch_handle_t batch;
 *
 * esp_err_t ret = star_batch_create(&bus_manager, &batch_cfg, &batch);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to create batch");
 *     return;
 * }
 *
 * // Add I2C read operations
 * star_batch_add_i2c_read(batch, "imu_i2c", accel_data, 6, 0x3B);  // Accel
 * star_batch_add_i2c_read(batch, "imu_i2c", gyro_data, 6, 0x43);   // Gyro
 * star_batch_add_i2c_read(batch, "imu_i2c", temp_data, 2, 0x41);   // Temp
 *
 * // Execute all operations atomically
 * star_batch_stats_t stats;
 * ret = star_batch_execute(batch, &stats);
 *
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Batch complete: %lu ops in %lu ms",
 *              stats.operations_executed, stats.total_time_ms);
 * }
 *
 * // Cleanup
 * star_batch_free(batch);
 *
 *
 * // === Device Initialization Sequence ===
 *
 * star_batch_config_t init_cfg = {
 *     .timeout_ms = 2000,
 *     .mode = STAR_BATCH_MODE_SEQUENTIAL,
 *     .rollback_on_error = false,
 *     .stop_on_error = true
 * };
 *
 * star_batch_handle_t init_batch;
 * star_batch_create(&bus_manager, &init_cfg, &init_batch);
 *
 * // MPU6050 initialization sequence
 * uint8_t reset_cmd = 0x80;    // Device reset
 * uint8_t wake_cmd = 0x00;     // Wake from sleep
 * uint8_t gyro_cfg = 0x08;     // Gyro +/- 500 dps
 * uint8_t accel_cfg = 0x08;    // Accel +/- 4g
 *
 * star_batch_add_i2c_write(init_batch, "imu_i2c", &reset_cmd, 1, 0x6B);
 * star_batch_add_delay(init_batch, 100);  // Wait for reset
 * star_batch_add_i2c_write(init_batch, "imu_i2c", &wake_cmd, 1, 0x6B);
 * star_batch_add_delay(init_batch, 10);   // Stabilize
 * star_batch_add_i2c_write(init_batch, "imu_i2c", &gyro_cfg, 1, 0x1B);
 * star_batch_add_i2c_write(init_batch, "imu_i2c", &accel_cfg, 1, 0x1C);
 *
 * ret = star_batch_execute(init_batch, NULL);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "IMU initialized successfully");
 * }
 *
 * star_batch_free(init_batch);
 *
 *
 * // === Mixed I2C and SPI Batch ===
 *
 * star_batch_handle_t mixed_batch;
 * star_batch_create(&bus_manager, &batch_cfg, &mixed_batch);
 *
 * // Read from I2C sensor
 * uint8_t i2c_buffer[4];
 * star_batch_add_i2c_read(mixed_batch, "sensor_i2c", i2c_buffer, 4, 0x00);
 *
 * // Write to SPI display
 * uint8_t spi_cmd[] = {0x2A, 0x00, 0x00, 0x01, 0x3F};  // Set column
 * star_batch_add_spi_transmit(mixed_batch, "lcd_spi", spi_cmd, 5, 0);
 *
 * // Full-duplex SPI transfer
 * uint8_t tx_data[8], rx_data[8];
 * star_batch_add_spi_transfer(mixed_batch, "flash_spi", tx_data, rx_data, 8, 0);
 *
 * star_batch_execute(mixed_batch, &stats);
 * star_batch_free(mixed_batch);
 *
 *
 * // === SMBus Operations in Batch ===
 *
 * star_batch_handle_t smbus_batch;
 * star_batch_create(&bus_manager, &batch_cfg, &smbus_batch);
 *
 * uint8_t status_byte, config_byte;
 * star_batch_add_smbus_read_byte(smbus_batch, "bms_i2c", 0x0B, 0x16, &status_byte);
 * star_batch_add_smbus_write_byte(smbus_batch, "bms_i2c", 0x0B, 0x00, 0x01);
 * star_batch_add_smbus_read_byte(smbus_batch, "bms_i2c", 0x0B, 0x00, &config_byte);
 *
 * star_batch_execute(smbus_batch, NULL);
 * ESP_LOGI(TAG, "BMS status: 0x%02X, config: 0x%02X", status_byte, config_byte);
 * star_batch_free(smbus_batch);
 *
 *
 * // === Error Handling and Individual Results ===
 *
 * star_batch_handle_t err_batch;
 * star_batch_create(&bus_manager, &batch_cfg, &err_batch);
 *
 * uint8_t buf1[4], buf2[4], buf3[4];
 * star_batch_add_i2c_read(err_batch, "sensor1_i2c", buf1, 4, 0x00);
 * star_batch_add_i2c_read(err_batch, "sensor2_i2c", buf2, 4, 0x00);  // May fail
 * star_batch_add_i2c_read(err_batch, "sensor3_i2c", buf3, 4, 0x00);
 *
 * ret = star_batch_execute(err_batch, &stats);
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGW(TAG, "Batch had errors: %lu failed", stats.operations_failed);
 *
 *     // Check individual operation results
 *     uint32_t count;
 *     star_batch_get_operation_count(err_batch, &count);
 *
 *     for (uint32_t i = 0; i < count; i++) {
 *         esp_err_t op_result;
 *         star_batch_get_operation_result(err_batch, i, &op_result);
 *         if (op_result != ESP_OK) {
 *             ESP_LOGW(TAG, "Operation %lu failed: %s", i, esp_err_to_name(op_result));
 *         }
 *     }
 * }
 *
 * star_batch_free(err_batch);
 *
 *
 * // === Reusing Batches ===
 *
 * star_batch_handle_t reuse_batch;
 * star_batch_create(&bus_manager, &batch_cfg, &reuse_batch);
 *
 * // First use
 * star_batch_add_i2c_read(reuse_batch, "sensor_i2c", buffer, 4, 0x00);
 * star_batch_execute(reuse_batch, NULL);
 *
 * // Clear and reuse
 * star_batch_clear(reuse_batch);
 *
 * // Second use with different operations
 * star_batch_add_i2c_write(reuse_batch, "sensor_i2c", config, 2, 0x10);
 * star_batch_execute(reuse_batch, NULL);
 *
 * star_batch_free(reuse_batch);
 *
 *
 * // === Continue on Error Mode ===
 *
 * star_batch_config_t continue_cfg = {
 *     .timeout_ms = 5000,
 *     .mode = STAR_BATCH_MODE_SEQUENTIAL,
 *     .rollback_on_error = false,
 *     .stop_on_error = false  // Continue even if operations fail
 * };
 *
 * star_batch_handle_t robust_batch;
 * star_batch_create(&bus_manager, &continue_cfg, &robust_batch);
 *
 * // Try to read from multiple sensors (some may be disconnected)
 * star_batch_add_i2c_read(robust_batch, "temp_i2c", temp_buf, 2, 0x00);
 * star_batch_add_i2c_read(robust_batch, "humid_i2c", humid_buf, 2, 0x00);
 * star_batch_add_i2c_read(robust_batch, "press_i2c", press_buf, 3, 0x00);
 *
 * // All operations attempted regardless of failures
 * star_batch_execute(robust_batch, &stats);
 * ESP_LOGI(TAG, "Read %lu/%lu sensors", stats.operations_succeeded, stats.operations_executed);
 *
 * star_batch_free(robust_batch);
 * @endcode
 */

/* --- Constants --- */

/**
 * @brief Batch operation constants
 *
 * Using enum for type safety while maintaining compile-time constant behavior
 * required for struct initializers and array sizes in C.
 */
enum {
  /** Maximum number of operations in a batch */
  STAR_BATCH_MAX_OPERATIONS = 16,

  /** Default timeout for batch operations (ms) */
  STAR_BATCH_DEFAULT_TIMEOUT_MS = 5000,
};

/* --- Types --- */

/**
 * @brief Batch handle (opaque)
 */
typedef void* star_batch_handle_t;

/**
 * @brief Batch operation type
 */
typedef enum {
  k_star_batch_op_i2c_read,         /**< I2C read operation */
  k_star_batch_op_i2c_write,        /**< I2C write operation */
  k_star_batch_op_spi_transmit,     /**< SPI transmit operation */
  k_star_batch_op_spi_receive,      /**< SPI receive operation */
  k_star_batch_op_spi_transfer,     /**< SPI transfer operation */
  k_star_batch_op_smbus_read_byte,  /**< SMBus read byte */
  k_star_batch_op_smbus_write_byte, /**< SMBus write byte */
  k_star_batch_op_delay,            /**< Delay operation (ms) */
} star_batch_op_type_t;

/**
 * @brief Batch execution mode
 */
typedef enum {
  k_star_batch_mode_sequential, /**< Execute operations sequentially */
  k_star_batch_mode_parallel,   /**< Execute operations in parallel (if possible) */
} star_batch_mode_t;

/**
 * @brief Batch configuration
 */
typedef struct {
  uint32_t          timeout_ms;        /**< Total timeout for batch (0 = no timeout) */
  star_batch_mode_t mode;              /**< Execution mode */
  bool              rollback_on_error; /**< Rollback on error (if supported) */
  bool              stop_on_error;     /**< Stop execution on first error */
} star_batch_config_t;

/**
 * @brief Batch operation descriptor
 */
typedef struct {
  star_batch_op_type_t type;     /**< Operation type */
  const char*          bus_name; /**< Bus name for this operation */

  union {
    struct {
      uint8_t* data;    /**< Data buffer */
      size_t   length;  /**< Data length */
      uint8_t  command; /**< Command/register address */
    } i2c;

    struct {
      const uint8_t* tx_data; /**< TX data buffer */
      uint8_t*       rx_data; /**< RX data buffer */
      size_t         length;  /**< Transfer length */
      uint32_t       flags;   /**< User flags */
    } spi;

    struct {
      uint8_t  addr;    /**< SMBus device address */
      uint8_t  command; /**< Command code */
      uint8_t* data;    /**< Data byte pointer */
    } smbus;

    struct {
      uint32_t delay_ms; /**< Delay in milliseconds */
    } delay;
  } params;

  esp_err_t result; /**< Operation result (set after execution) */
} star_batch_operation_t;

/**
 * @brief Batch execution statistics
 */
typedef struct {
  uint32_t operations_executed;  /**< Number of operations executed */
  uint32_t operations_succeeded; /**< Number of operations that succeeded */
  uint32_t operations_failed;    /**< Number of operations that failed */
  uint32_t total_time_ms;        /**< Total execution time (ms) */
} star_batch_stats_t;

/* --- Batch Creation and Management --- */

/**
 * @brief Create a new batch transaction
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  config  Batch configuration
 * @param[out] handle  Output handle for the batch
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_create(star_bus_manager_t*        manager,
                            const star_batch_config_t* config,
                            star_batch_handle_t*       handle);

/**
 * @brief Free a batch transaction
 *
 * @param[in] handle Batch handle
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if handle is NULL
 *
 * @note Call this after executing or if you want to discard the batch
 */
esp_err_t star_batch_free(star_batch_handle_t handle);

/* --- Adding Operations --- */

/**
 * @brief Add an I2C write operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of I2C bus
 * @param[in] data     Pointer to data to write
 * @param[in] length   Number of bytes to write
 * @param[in] command  Command byte (register address)
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Data buffer must remain valid until batch is executed
 */
esp_err_t star_batch_add_i2c_write(star_batch_handle_t handle,
                                   const char*         bus_name,
                                   const uint8_t*      data,
                                   size_t              length,
                                   uint8_t             command);

/**
 * @brief Add an I2C read operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of I2C bus
 * @param[out] data    Pointer to buffer for received data
 * @param[in] length   Number of bytes to read
 * @param[in] command  Command byte (register address)
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Data buffer must remain valid until batch is executed
 */
esp_err_t star_batch_add_i2c_read(star_batch_handle_t handle,
                                  const char*         bus_name,
                                  uint8_t*            data,
                                  size_t              length,
                                  uint8_t             command);

/**
 * @brief Add an SPI transmit operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of SPI bus
 * @param[in] data     Pointer to data to transmit
 * @param[in] length   Number of bytes to transmit
 * @param[in] flags    User flags for the transaction
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_add_spi_transmit(star_batch_handle_t handle,
                                      const char*         bus_name,
                                      const uint8_t*      data,
                                      size_t              length,
                                      uint32_t            flags);

/**
 * @brief Add an SPI receive operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of SPI bus
 * @param[out] data    Pointer to buffer for received data
 * @param[in] length   Number of bytes to receive
 * @param[in] flags    User flags for the transaction
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_add_spi_receive(star_batch_handle_t handle,
                                     const char*         bus_name,
                                     uint8_t*            data,
                                     size_t              length,
                                     uint32_t            flags);

/**
 * @brief Add an SPI transfer operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of SPI bus
 * @param[in] tx_data  Pointer to data to transmit
 * @param[out] rx_data Pointer to buffer for received data
 * @param[in] length   Number of bytes to transfer
 * @param[in] flags    User flags for the transaction
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_add_spi_transfer(star_batch_handle_t handle,
                                      const char*         bus_name,
                                      const uint8_t*      tx_data,
                                      uint8_t*            rx_data,
                                      size_t              length,
                                      uint32_t            flags);

/**
 * @brief Add an SMBus read byte operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of I2C bus
 * @param[in] addr     SMBus device address
 * @param[in] command  SMBus command code
 * @param[out] data    Pointer to store received byte
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_add_smbus_read_byte(star_batch_handle_t handle,
                                         const char*         bus_name,
                                         uint8_t             addr,
                                         uint8_t             command,
                                         uint8_t*            data);

/**
 * @brief Add an SMBus write byte operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] bus_name Name of I2C bus
 * @param[in] addr     SMBus device address
 * @param[in] command  SMBus command code
 * @param[in] data     Byte to write
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_add_smbus_write_byte(star_batch_handle_t handle,
                                          const char*         bus_name,
                                          uint8_t             addr,
                                          uint8_t             command,
                                          uint8_t             data);

/**
 * @brief Add a delay operation to the batch
 *
 * @param[in] handle   Batch handle
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Useful for timing-sensitive sequences (e.g., device reset delays)
 */
esp_err_t star_batch_add_delay(star_batch_handle_t handle, uint32_t delay_ms);

/* --- Batch Execution --- */

/**
 * @brief Execute all operations in the batch
 *
 * Operations are executed according to the batch configuration (sequential or parallel).
 * If stop_on_error is true, execution stops at the first error.
 * If rollback_on_error is true, attempts to reverse operations (bus-specific support).
 *
 * @param[in]  handle Batch handle
 * @param[out] stats  Optional pointer to receive execution statistics
 *
 * @return ESP_OK if all operations succeeded, error code of first failure otherwise
 *
 * @note Individual operation results can be checked via star_batch_get_operation_result
 */
esp_err_t star_batch_execute(star_batch_handle_t handle, star_batch_stats_t* stats);

/**
 * @brief Get the result of a specific operation in the batch
 *
 * @param[in]  handle Batch handle
 * @param[in]  index  Operation index (0-based)
 * @param[out] result Pointer to store operation result
 *
 * @return ESP_OK if result retrieved, error code otherwise
 *
 * @note Only valid after star_batch_execute has been called
 */
esp_err_t
star_batch_get_operation_result(star_batch_handle_t handle, uint32_t index, esp_err_t* result);

/**
 * @brief Get the number of operations in the batch
 *
 * @param[in]  handle Batch handle
 * @param[out] count  Pointer to store operation count
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_get_operation_count(star_batch_handle_t handle, uint32_t* count);

/**
 * @brief Clear all operations from the batch
 *
 * Removes all operations but keeps the batch handle valid for reuse.
 *
 * @param[in] handle Batch handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_batch_clear(star_batch_handle_t handle);

/* --- Helper Macros --- */

/**
 * @brief Create default batch configuration
 */
#define STAR_BATCH_CONFIG_DEFAULT()                                                                \
  {                                                                                                \
    .timeout_ms        = STAR_BATCH_DEFAULT_TIMEOUT_MS,                                            \
    .mode              = STAR_BATCH_MODE_SEQUENTIAL,                                               \
    .rollback_on_error = false,                                                                    \
    .stop_on_error     = true,                                                                     \
  }

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_BATCH_H */
