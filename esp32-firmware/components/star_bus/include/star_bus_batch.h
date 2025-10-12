/* esp32-firmware/components/star_bus/include/star_bus_batch.h */

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
 */

/* --- Constants --- */

/**
 * @brief Maximum number of operations in a batch
 */
#define STAR_BATCH_MAX_OPERATIONS (16)

/**
 * @brief Default timeout for batch operations (ms)
 */
#define STAR_BATCH_DEFAULT_TIMEOUT_MS (5000)

/* --- Types --- */

/**
 * @brief Batch handle (opaque)
 */
typedef void* star_batch_handle_t;

/**
 * @brief Batch operation type
 */
typedef enum {
  STAR_BATCH_OP_I2C_READ,         /**< I2C read operation */
  STAR_BATCH_OP_I2C_WRITE,        /**< I2C write operation */
  STAR_BATCH_OP_SPI_TRANSMIT,     /**< SPI transmit operation */
  STAR_BATCH_OP_SPI_RECEIVE,      /**< SPI receive operation */
  STAR_BATCH_OP_SPI_TRANSFER,     /**< SPI transfer operation */
  STAR_BATCH_OP_SMBUS_READ_BYTE,  /**< SMBus read byte */
  STAR_BATCH_OP_SMBUS_WRITE_BYTE, /**< SMBus write byte */
  STAR_BATCH_OP_DELAY,            /**< Delay operation (ms) */
} star_batch_op_type_t;

/**
 * @brief Batch execution mode
 */
typedef enum {
  STAR_BATCH_MODE_SEQUENTIAL, /**< Execute operations sequentially */
  STAR_BATCH_MODE_PARALLEL,   /**< Execute operations in parallel (if possible) */
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
 * @note Call this after executing or if you want to discard the batch
 */
void star_batch_free(star_batch_handle_t handle);

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
    .timeout_ms = STAR_BATCH_DEFAULT_TIMEOUT_MS, .mode = STAR_BATCH_MODE_SEQUENTIAL,               \
    .rollback_on_error = false, .stop_on_error = true,                                             \
  }

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_BATCH_H */
