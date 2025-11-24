/* esp32-firmware/components/star_bus/include/star_bus_debug.h */

#ifndef STAR_COMPONENT_BUS_DEBUG_H
#define STAR_COMPONENT_BUS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_bus_manager.h"

/**
 * @file star_bus_debug.h
 * @brief Debug and diagnostic utilities for STAR Bus Manager
 *
 * This module provides tools for debugging I2C bus issues, scanning for devices,
 * monitoring transactions, and diagnosing communication problems.
 */

/* --- I2C Bus Scanning --- */

/** Callback function for I2C scan results */
typedef void (*star_bus_scan_callback_t)(uint8_t address, void* user_data);

/**
 * @brief Scan I2C bus for devices
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus to scan
 * @param callback Callback function called for each device found (can be NULL)
 * @param user_data User data passed to callback
 * @param devices_found Output array of found device addresses (can be NULL)
 * @param max_devices Maximum number of devices to store in devices_found array
 * @param num_found Output number of devices found
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_scan_i2c(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_bus_scan_callback_t  callback,
                                  void*                     user_data,
                                  uint8_t*                  devices_found,
                                  size_t                    max_devices,
                                  size_t*                   num_found);

/**
 * @brief Print I2C scan results to console
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus to scan
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_print_scan(const star_bus_manager_t* manager, const char* bus_name);

/* --- Bus Information --- */

/**
 * @brief Print information about all configured buses
 * @param manager Bus manager
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_print_buses(const star_bus_manager_t* manager);

/**
 * @brief Print detailed information about a specific bus
 * @param manager Bus manager
 * @param bus_name Name of the bus
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_print_bus_info(const star_bus_manager_t* manager, const char* bus_name);

/* --- Register Operations --- */

/**
 * @brief Read and print register value
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @param reg_addr Register address
 * @return ESP_OK on success
 */
esp_err_t
star_bus_debug_read_reg(const star_bus_manager_t* manager, const char* bus_name, uint8_t reg_addr);

/**
 * @brief Write register value
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @param reg_addr Register address
 * @param value Value to write
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_write_reg(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   uint8_t                   reg_addr,
                                   uint8_t                   value);

/**
 * @brief Dump multiple registers
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @param start_reg Starting register address
 * @param count Number of registers to read
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_dump_regs(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   uint8_t                   start_reg,
                                   size_t                    count);

/* --- Transaction Logging --- */

/** Transaction log entry */
typedef struct {
  uint64_t timestamp_us; /**< Timestamp in microseconds */
  uint8_t  bus_index;    /**< Bus index */
  uint8_t  address;      /**< Device address */
  uint8_t  reg_addr;     /**< Register address */
  uint16_t length;       /**< Data length */
  bool     is_write;     /**< true=write, false=read */
  bool     success;      /**< Transaction success */
  int32_t  error_code;   /**< Error code if failed */
} star_bus_transaction_log_t;

/** Transaction logger configuration */
typedef struct {
  star_bus_transaction_log_t* buffer;      /**< Log buffer */
  size_t                      buffer_size; /**< Buffer size */
  size_t                      write_index; /**< Current write index */
  size_t                      count;       /**< Number of entries */
  bool                        wrap_around; /**< Enable wrap-around */
  bool                        enabled;     /**< Logger enabled */
} star_bus_transaction_logger_t;

/**
 * @brief Initialize transaction logger
 * @param logger Logger structure
 * @param buffer Log buffer
 * @param buffer_size Buffer size
 * @param wrap_around Enable wrap-around when buffer is full
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_init(star_bus_transaction_logger_t* logger,
                                     star_bus_transaction_log_t*    buffer,
                                     size_t                         buffer_size,
                                     bool                           wrap_around);

/**
 * @brief Enable/disable transaction logger
 * @param logger Logger structure
 * @param enabled true to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_set_enabled(star_bus_transaction_logger_t* logger, bool enabled);

/**
 * @brief Clear transaction log
 * @param logger Logger structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_clear(star_bus_transaction_logger_t* logger);

/**
 * @brief Log a transaction
 * @param logger Logger structure
 * @param bus_index Bus index
 * @param address Device address
 * @param reg_addr Register address
 * @param length Data length
 * @param is_write true for write, false for read
 * @param success Transaction success
 * @param error_code Error code if failed
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_log(star_bus_transaction_logger_t* logger,
                                    uint8_t                        bus_index,
                                    uint8_t                        address,
                                    uint8_t                        reg_addr,
                                    uint16_t                       length,
                                    bool                           is_write,
                                    bool                           success,
                                    int32_t                        error_code);

/**
 * @brief Print transaction log to console
 * @param logger Logger structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_print(const star_bus_transaction_logger_t* logger);

/**
 * @brief Get transaction log entry
 * @param logger Logger structure
 * @param index Entry index
 * @param entry Output entry
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_logger_get_entry(const star_bus_transaction_logger_t* logger,
                                          size_t                               index,
                                          star_bus_transaction_log_t*          entry);

/* --- Error Analysis --- */

/** Error statistics structure */
typedef struct {
  uint32_t total_transactions;  /**< Total number of transactions */
  uint32_t failed_transactions; /**< Number of failed transactions */
  uint32_t timeout_errors;      /**< Number of timeout errors */
  uint32_t nack_errors;         /**< Number of NACK errors */
  uint32_t bus_errors;          /**< Number of bus errors */
  uint32_t other_errors;        /**< Number of other errors */
  uint64_t last_error_time_us;  /**< Timestamp of last error */
  int32_t  last_error_code;     /**< Last error code */
} star_bus_error_stats_t;

/**
 * @brief Initialize error statistics
 * @param stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_stats_init(star_bus_error_stats_t* stats);

/**
 * @brief Record transaction result
 * @param stats Statistics structure
 * @param success Transaction success
 * @param error_code Error code if failed
 * @return ESP_OK on success
 */
esp_err_t
star_bus_debug_stats_record(star_bus_error_stats_t* stats, bool success, int32_t error_code);

/**
 * @brief Print error statistics
 * @param stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_stats_print(const star_bus_error_stats_t* stats);

/**
 * @brief Reset error statistics
 * @param stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t star_bus_debug_stats_reset(star_bus_error_stats_t* stats);

/* --- Diagnostic Tests --- */

/**
 * @brief Test I2C bus communication
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @return ESP_OK if bus is functioning correctly
 */
esp_err_t star_bus_debug_test_bus(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Test device connectivity
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @return ESP_OK if device responds
 */
esp_err_t star_bus_debug_test_device(const star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Verify register read/write operation
 * @param manager Bus manager
 * @param bus_name Name of the I2C bus
 * @param reg_addr Register address to test
 * @param test_value Value to write and verify
 * @return ESP_OK if read/write works correctly
 */
esp_err_t star_bus_debug_test_reg_rw(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     uint8_t                   reg_addr,
                                     uint8_t                   test_value);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_DEBUG_H */
