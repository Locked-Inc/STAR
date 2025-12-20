/**
 * @file star_bus_i2c.h
 * @brief I2C controller protocol API for the bus manager
 * @details
 * Provides interface for I2C controller mode operations through the bus manager. Supports
 * standard and DMA-based transfers with configurable clock speeds, timeout handling, and
 * multi-device communication on shared SDA/SCL lines.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_I2C_H
#define STAR_COMPONENT_BUS_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_i2c.h
 * @brief I2C bus operations through the unified bus manager
 *
 * This module provides I2C read/write operations that work through the bus manager.
 * All operations find the bus configuration by name and use the configured I2C port
 * and device address.
 *
 * Key Features:
 * - Register-based read/write operations
 * - Raw read operations (no register address)
 * - Command byte writes
 * - Thread-safe through bus manager mutex
 *
 * Example Usage:
 * @code
 * // === Setup (see star_bus_manager.h for full setup) ===
 *
 * // Create I2C bus configuration for MPU6050
 * star_bus_config_t* imu_bus = star_bus_config_create_i2c(
 *     "imu_i2c",         // Unique bus name
 *     I2C_NUM_0,         // I2C port
 *     0x68,              // MPU6050 address
 *     GPIO_NUM_21,       // SDA
 *     GPIO_NUM_22,       // SCL
 *     400000             // 400kHz fast mode
 * );
 * star_bus_manager_add_bus(&bus_manager, imu_bus);
 *
 *
 * // === Reading from a Register ===
 *
 * // Read WHO_AM_I register (0x75) from MPU6050
 * uint8_t who_am_i;
 * size_t bytes_read;
 * esp_err_t ret = star_bus_i2c_read(
 *     &bus_manager,
 *     "imu_i2c",         // Bus name
 *     &who_am_i,         // Data buffer
 *     1,                 // Read 1 byte
 *     0x75,              // Register address (WHO_AM_I)
 *     &bytes_read        // Optional: bytes actually read
 * );
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", who_am_i);
 * }
 *
 * // Read 6 bytes of accelerometer data (registers 0x3B-0x40)
 * uint8_t accel_data[6];
 * star_bus_i2c_read(&bus_manager, "imu_i2c", accel_data, 6, 0x3B, NULL);
 *
 *
 * // === Writing to a Register ===
 *
 * // Write to power management register (wake up device)
 * uint8_t pwr_mgmt = 0x00;  // Clear sleep bit
 * size_t bytes_written;
 * star_bus_i2c_write(
 *     &bus_manager,
 *     "imu_i2c",
 *     &pwr_mgmt,         // Data to write
 *     1,                 // Write 1 byte
 *     0x6B,              // Register address (PWR_MGMT_1)
 *     &bytes_written     // Optional: bytes actually written
 * );
 *
 * // Configure accelerometer range
 * uint8_t accel_config = 0x08;  // +/- 4g range
 * star_bus_i2c_write(&bus_manager, "imu_i2c", &accel_config, 1, 0x1C, NULL);
 *
 *
 * // === Writing a Command Byte ===
 *
 * // Some devices accept single-byte commands without register address
 * star_bus_i2c_write_command(&bus_manager, "lcd_i2c", 0x01);  // Clear display
 *
 *
 * // === Raw Read (No Register Address) ===
 *
 * // Some devices send data continuously without register addressing
 * uint8_t raw_buffer[16];
 * star_bus_i2c_read_raw(&bus_manager, "sensor_i2c", raw_buffer, 16, &bytes_read);
 *
 *
 * // === Multiple Devices on Same I2C Port ===
 *
 * // Create separate bus configs for each device (same port, different addresses)
 * star_bus_config_t* accel_bus = star_bus_config_create_i2c(
 *     "accel_i2c", I2C_NUM_0, 0x1D, GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_config_t* mag_bus = star_bus_config_create_i2c(
 *     "mag_i2c", I2C_NUM_0, 0x1E, GPIO_NUM_21, GPIO_NUM_22, 400000);
 *
 * star_bus_manager_add_bus(&bus_manager, accel_bus);
 * star_bus_manager_add_bus(&bus_manager, mag_bus);
 *
 * // Read from each device by name
 * star_bus_i2c_read(&bus_manager, "accel_i2c", accel_data, 6, 0x32, NULL);
 * star_bus_i2c_read(&bus_manager, "mag_i2c", mag_data, 6, 0x03, NULL);
 * @endcode
 */

/* --- Public Functions --- */

/**
 * @brief Initialize default I2C operations function pointers within a config structure.
 *        Typically called internally by star_bus_config_create_i2c.
 *        Assigns default implementations for write, read, write_command, and read_raw.
 *
 * @param[out] ops Pointer to I2C operations structure to initialize. Must not be NULL.
 */
void star_bus_i2c_init_default_ops(star_i2c_ops_t* ops);

/**
 * @brief Write data to an I2C device specified by a bus configuration, preceded by a register address.
 *
 * Finds the bus configuration by name and calls its write operation.
 *
 * @param[in]  manager       Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  name          Name of the I2C bus/device configuration. Must not be NULL.
 * @param[in]  data          Pointer to the data buffer to write. Must not be NULL.
 * @param[in]  len           Number of bytes to write from the data buffer. Must be > 0.
 * @param[in]  reg_addr      The register address to write before writing data.
 * @param[out] bytes_written Pointer to store the number of data bytes actually written (excluding register address). Can be NULL if not needed.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver (e.g., timeout, NACK).
 */
esp_err_t star_bus_i2c_write(const star_bus_manager_t* manager,
                             const char*               name,
                             const uint8_t*            data,
                             size_t                    len,
                             uint8_t                   reg_addr,
                             size_t*                   bytes_written);

/**
 * @brief Read data from an I2C device specified by a bus configuration, after writing a register address.
 *
 * Finds the bus configuration by name and calls its read operation.
 *
 * @param[in]  manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  name       Name of the I2C bus/device configuration. Must not be NULL.
 * @param[out] data       Pointer to the buffer where read data will be stored. Must not be NULL.
 * @param[in]  len        Number of bytes to read into the data buffer. Must be > 0.
 * @param[in]  reg_addr   The register address to write before initiating the read.
 * @param[out] bytes_read Pointer to store the number of data bytes actually read. Can be NULL if not needed.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver (e.g., timeout, NACK).
 */
esp_err_t star_bus_i2c_read(const star_bus_manager_t* manager,
                            const char*               name,
                            uint8_t*                  data,
                            size_t                    len,
                            uint8_t                   reg_addr,
                            size_t*                   bytes_read);

/**
 * @brief Write a single command byte to an I2C device specified by a bus configuration.
 *        No register address or additional data is sent after the command.
 *
 * Finds the bus configuration by name and calls its write_command operation.
 *
 * @param[in] manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name    Name of the I2C bus/device configuration. Must not be NULL.
 * @param[in] command The command byte to write.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver.
 */
esp_err_t
star_bus_i2c_write_command(const star_bus_manager_t* manager, const char* name, uint8_t command);

/**
 * @brief Read raw bytes from an I2C device specified by a bus configuration.
 *        Does not send a register address before reading.
 *
 * Finds the bus configuration by name and calls its read_raw operation.
 *
 * @param[in]  manager    Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  name       Name of the I2C bus/device configuration. Must not be NULL.
 * @param[out] data       Pointer to the buffer where read data will be stored. Must not be NULL.
 * @param[in]  len        Number of bytes to read into the data buffer. Must be > 0.
 * @param[out] bytes_read Pointer to store the number of data bytes actually read. Can be NULL if not needed.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid, ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not initialized, or an error from the driver.
 */
esp_err_t star_bus_i2c_read_raw(const star_bus_manager_t* manager,
                                const char*               name,
                                uint8_t*                  data,
                                size_t                    len,
                                size_t*                   bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_I2C_H */
