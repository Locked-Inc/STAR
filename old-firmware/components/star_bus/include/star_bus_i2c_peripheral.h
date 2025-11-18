/* esp32-firmware/components/star_bus/include/star_bus_i2c_peripheral.h */

#ifndef STAR_BUS_I2C_PERIPHERAL_H
#define STAR_BUS_I2C_PERIPHERAL_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_i2c_peripheral.h
 * @brief I2C peripheral (slave) mode implementation
 *
 * This module enables the ESP32 to act as an I2C peripheral device, responding
 * to master requests. It provides a flexible callback-based interface for
 * handling read and write requests.
 *
 * Features:
 * - 7-bit and 10-bit peripheral addressing
 * - Configurable RX/TX buffer sizes
 * - Callback-based request handling
 * - General call address support
 * - Clock stretching support
 * - Statistics tracking
 *
 * Use Cases:
 * - Emulating I2C sensors
 * - Building I2C-controlled devices
 * - Creating I2C bridges
 * - Testing I2C master implementations
 * - Multi-master I2C systems
 *
 * Example:
 * @code
 * // Peripheral read callback - master is reading from us
 * static esp_err_t on_read(uint8_t* data, size_t* length, void* ctx) {
 *   uint8_t sensor_value = read_sensor();
 *   data[0] = sensor_value;
 *   *length = 1;
 *   return ESP_OK;
 * }
 *
 * // Peripheral write callback - master is writing to us
 * static esp_err_t on_write(const uint8_t* data, size_t length, void* ctx) {
 *   if (length > 0) {
 *     process_command(data[0]);
 *   }
 *   return ESP_OK;
 * }
 *
 * // Setup peripheral
 * star_i2c_peripheral_config_t config = STAR_I2C_PERIPHERAL_CONFIG_DEFAULT();
 * config.address = 0x42;
 * config.on_read = on_read;
 * config.on_write = on_write;
 *
 * star_bus_i2c_peripheral_enable(manager, "i2c0", &config);
 * @endcode
 */

/* --- Types --- */

/**
 * @brief I2C peripheral address mode
 */
typedef enum {
  STAR_I2C_ADDR_7BIT  = 0, /**< 7-bit addressing (default) */
  STAR_I2C_ADDR_10BIT = 1, /**< 10-bit addressing */
} star_i2c_addr_mode_t;

/**
 * @brief I2C peripheral read callback
 *
 * Called when master requests data from peripheral.
 *
 * @param[out] data Buffer to write response data
 * @param[in,out] length On entry: buffer size. On exit: bytes written
 * @param[in] context User context pointer
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note If error is returned, NACK will be sent to master
 * @note Maximum *length is limited by rx_buffer_size in config
 */
typedef esp_err_t (*star_i2c_peripheral_read_cb_t)(uint8_t* data, size_t* length, void* context);

/**
 * @brief I2C peripheral write callback
 *
 * Called when master sends data to peripheral.
 *
 * @param[in] data Data received from master
 * @param[in] length Number of bytes received
 * @param[in] context User context pointer
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Return value does not affect NACK (write already completed)
 */
typedef esp_err_t (*star_i2c_peripheral_write_cb_t)(const uint8_t* data,
                                                    size_t         length,
                                                    void*          context);

/**
 * @brief I2C peripheral error callback
 *
 * Called when peripheral encounters an error.
 *
 * @param[in] error Error code
 * @param[in] context User context pointer
 */
typedef void (*star_i2c_peripheral_error_cb_t)(esp_err_t error, void* context);

/**
 * @brief I2C peripheral configuration
 */
typedef struct {
  uint16_t             address;   /**< Peripheral address (7-bit or 10-bit) */
  star_i2c_addr_mode_t addr_mode; /**< Address mode */

  /* Callbacks */
  star_i2c_peripheral_read_cb_t  on_read;  /**< Read request callback (required) */
  star_i2c_peripheral_write_cb_t on_write; /**< Write request callback (required) */
  star_i2c_peripheral_error_cb_t on_error; /**< Error callback (optional) */
  void*                          context;  /**< User context for callbacks */

  /* Buffer Configuration */
  size_t rx_buffer_size; /**< Receive buffer size (bytes) */
  size_t tx_buffer_size; /**< Transmit buffer size (bytes) */

  /* Timing */
  uint32_t timeout_ms; /**< Operation timeout (milliseconds) */

  /* Options */
  bool enable_general_call; /**< Respond to general call address (0x00) */
  bool enable_10bit_addr;   /**< Enable 10-bit addressing */
} star_i2c_peripheral_config_t;

/**
 * @brief I2C peripheral statistics
 */
typedef struct {
  uint64_t  total_read_requests;  /**< Total read requests received */
  uint64_t  total_write_requests; /**< Total write requests received */
  uint64_t  total_bytes_sent;     /**< Total bytes sent to master */
  uint64_t  total_bytes_received; /**< Total bytes received from master */
  uint64_t  failed_reads;         /**< Failed read requests */
  uint64_t  failed_writes;        /**< Failed write requests */
  uint64_t  callback_errors;      /**< Errors from callbacks */
  uint32_t  last_request_time_us; /**< Last request processing time */
  esp_err_t last_error;           /**< Last error code */
} star_i2c_peripheral_stats_t;

/* --- Peripheral Management --- */

/**
 * @brief Enable I2C peripheral mode on a bus
 *
 * Configures the bus to operate in peripheral mode, responding to master requests.
 * The bus cannot be used for master operations while in peripheral mode.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C bus
 * @param[in] config Peripheral configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Peripheral mode is mutually exclusive with master mode
 * @note on_read and on_write callbacks are required
 */
esp_err_t star_bus_i2c_peripheral_enable(star_bus_manager_t*                 manager,
                                         const char*                         bus_name,
                                         const star_i2c_peripheral_config_t* config);

/**
 * @brief Disable I2C peripheral mode on a bus
 *
 * Returns the bus to master mode.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_i2c_peripheral_disable(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Check if I2C peripheral mode is enabled
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[out] enabled Pointer to store enabled status
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_i2c_peripheral_is_enabled(const star_bus_manager_t* manager,
                                             const char*               bus_name,
                                             bool*                     enabled);

/* --- Buffer Management --- */

/**
 * @brief Update peripheral response data
 *
 * Allows updating the data that will be sent on next read request.
 * Useful for pre-loading sensor data or status information.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C bus
 * @param[in] data Data to send on next read
 * @param[in] length Number of bytes
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Data is copied to internal buffer
 * @note Length must not exceed tx_buffer_size
 */
esp_err_t star_bus_i2c_peripheral_set_response(star_bus_manager_t* manager,
                                               const char*         bus_name,
                                               const uint8_t*      data,
                                               size_t              length);

/**
 * @brief Get last received data
 *
 * Retrieves the most recent data written by master.
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[out] data Buffer to store received data
 * @param[in,out] length On entry: buffer size. On exit: bytes copied
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_i2c_peripheral_get_last_write(const star_bus_manager_t* manager,
                                                 const char*               bus_name,
                                                 uint8_t*                  data,
                                                 size_t*                   length);

/* --- Statistics --- */

/**
 * @brief Get peripheral statistics
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_i2c_peripheral_get_stats(const star_bus_manager_t*    manager,
                                            const char*                  bus_name,
                                            star_i2c_peripheral_stats_t* stats);

/**
 * @brief Reset peripheral statistics
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_i2c_peripheral_reset_stats(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Print peripheral statistics
 *
 * @param[in] bus_name Name of I2C bus
 * @param[in] stats Pointer to statistics
 */
void star_bus_i2c_peripheral_print_stats(const char*                        bus_name,
                                         const star_i2c_peripheral_stats_t* stats);

/* --- Helper Macros --- */

/**
 * @brief Create default I2C peripheral configuration
 */
#define STAR_I2C_PERIPHERAL_CONFIG_DEFAULT()                                                       \
  {                                                                                                \
    .address             = 0x00,                                                                   \
    .addr_mode           = STAR_I2C_ADDR_7BIT,                                                     \
    .on_read             = NULL,                                                                   \
    .on_write            = NULL,                                                                   \
    .on_error            = NULL,                                                                   \
    .context             = NULL,                                                                   \
    .rx_buffer_size      = 256,                                                                    \
    .tx_buffer_size      = 256,                                                                    \
    .timeout_ms          = 1000,                                                                   \
    .enable_general_call = false,                                                                  \
    .enable_10bit_addr   = false,                                                                  \
  }

/**
 * @brief Validate 7-bit I2C address
 */
#define STAR_I2C_ADDR_7BIT_VALID(addr) ((addr) > 0x07 && (addr) < 0x78)

/**
 * @brief Validate 10-bit I2C address
 */
#define STAR_I2C_ADDR_10BIT_VALID(addr) ((addr) < 0x400)

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_I2C_PERIPHERAL_H */
