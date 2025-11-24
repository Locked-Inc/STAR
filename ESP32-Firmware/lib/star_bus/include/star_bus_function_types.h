/* esp32-firmware/components/star_bus/include/star_bus_function_types.h */

#ifndef STAR_COMPONENT_BUS_FUNCTION_TYPES_H
#define STAR_COMPONENT_BUS_FUNCTION_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "driver/spi_master.h" /* Added for SPI */

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star_bus_common_types.h"
#include "star_bus_event_types.h"

/* --- Callback Function Types --- */
/* Note: Callbacks return void. They should handle errors internally and avoid blocking. */

/* I2C Callback Function Types */
typedef void (*star_i2c_transfer_complete_cb_t)(const star_bus_event_t* event, void* user_ctx);
typedef void (*star_i2c_error_cb_t)(const star_bus_event_t* event, esp_err_t error, void* user_ctx);

/* SPI Callback Function Types */
typedef void (*star_spi_transfer_complete_cb_t)(const star_bus_event_t* event, void* user_ctx);
typedef void (*star_spi_error_cb_t)(const star_bus_event_t* event, esp_err_t error, void* user_ctx);

/* GPIO Callback Function Types */
typedef void (*star_gpio_state_change_cb_t)(const star_bus_event_t* event, void* user_ctx);
typedef void (*star_gpio_error_cb_t)(const star_bus_event_t* event,
                                     esp_err_t               error,
                                     void*                   user_ctx);

/* --- Operation Function Types --- */

/* I2C Operation Function Types */

/**
 * @brief Function pointer type for writing data preceded by a register address.
 * @param config Pointer to the initialized I2C bus configuration.
 * @param data Pointer to the data buffer to write.
 * @param len Number of bytes to write from the data buffer. Must be > 0.
 * @param reg_addr The register address to write before the data.
 * @param bytes_written Optional pointer to store the number of data bytes written (excluding reg_addr).
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_i2c_write_fn_t)(const star_bus_config_t* config,
                                         const uint8_t*           data,
                                         size_t                   len,
                                         uint8_t                  reg_addr,
                                         size_t*                  bytes_written);

/**
 * @brief Function pointer type for reading data after writing a register address.
 * @param config Pointer to the initialized I2C bus configuration.
 * @param data Pointer to the buffer where read data will be stored.
 * @param len Number of bytes to read into the data buffer. Must be > 0.
 * @param reg_addr The register address to write before initiating the read.
 * @param bytes_read Optional pointer to store the number of data bytes actually read.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_i2c_read_fn_t)(const star_bus_config_t* config,
                                        uint8_t*                 data,
                                        size_t                   len,
                                        uint8_t                  reg_addr,
                                        size_t*                  bytes_read);

/**
 * @brief Function pointer type for writing a single command byte (no register address, no data).
 *        Useful for devices like BH1750.
 * @param config Pointer to the initialized I2C bus configuration.
 * @param command The command byte to write.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_i2c_write_command_fn_t)(const star_bus_config_t* config, uint8_t command);

/**
 * @brief Function pointer type for reading raw bytes without sending a register address first.
 *        Useful for devices like BH1750 after a command has been sent.
 * @param config Pointer to the initialized I2C bus configuration.
 * @param data Pointer to the buffer where read data will be stored.
 * @param len Number of bytes to read into the data buffer. Must be > 0.
 * @param bytes_read Optional pointer to store the number of data bytes actually read.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_i2c_read_raw_fn_t)(const star_bus_config_t* config,
                                            uint8_t*                 data,
                                            size_t                   len,
                                            size_t*                  bytes_read);

/* SPI Operation Function Types */

/**
 * @brief Function pointer type for transmitting SPI data (command or general data).
 * @param config Pointer to the initialized SPI device configuration.
 * @param tx_buffer Pointer to the data buffer to transmit.
 * @param len Number of bytes to transmit.
 * @param user_flags User-defined flags (e.g., to control DC pin via callback).
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_spi_transmit_fn_t)(const star_bus_config_t* config,
                                            const void*              tx_buffer,
                                            size_t                   len,
                                            uint32_t                 user_flags);

/**
 * @brief Function pointer type for receiving SPI data.
 * @param config Pointer to the initialized SPI device configuration.
 * @param rx_buffer Pointer to the buffer where received data will be stored.
 * @param len Number of bytes to receive.
 * @param user_flags User-defined flags.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_spi_receive_fn_t)(const star_bus_config_t* config,
                                           void*                    rx_buffer,
                                           size_t                   len,
                                           uint32_t                 user_flags);

/**
 * @brief Function pointer type for full-duplex SPI transfer.
 * @param config Pointer to the initialized SPI device configuration.
 * @param tx_buffer Pointer to the data buffer to transmit.
 * @param rx_buffer Pointer to the buffer where received data will be stored.
 * @param len Number of bytes to transfer.
 * @param user_flags User-defined flags.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_spi_transfer_fn_t)(const star_bus_config_t* config,
                                            const void*              tx_buffer,
                                            void*                    rx_buffer,
                                            size_t                   len,
                                            uint32_t                 user_flags);

/* GPIO Operation Function Types */

/**
 * @brief Function pointer type for configuring a GPIO pin (mode, pull-up/down).
 * @param config Pointer to the initialized GPIO bus configuration.
 * @param pin GPIO pin number to configure.
 * @param mode GPIO mode (input, output, etc.).
 * @param pull GPIO pull mode (pull-up, pull-down, floating).
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_gpio_configure_fn_t)(const star_bus_config_t* config,
                                              gpio_num_t               pin,
                                              gpio_mode_t              mode,
                                              gpio_pull_mode_t         pull);

/**
 * @brief Function pointer type for writing (setting output level) to a GPIO pin.
 * @param config Pointer to the initialized GPIO bus configuration.
 * @param pin GPIO pin number to write.
 * @param level Level to set (0 = low, 1 = high).
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_gpio_write_fn_t)(const star_bus_config_t* config,
                                          gpio_num_t               pin,
                                          uint32_t                 level);

/**
 * @brief Function pointer type for reading (getting input level) from a GPIO pin.
 * @param config Pointer to the initialized GPIO bus configuration.
 * @param pin GPIO pin number to read.
 * @param level Pointer to store the read level (0 = low, 1 = high).
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_gpio_read_fn_t)(const star_bus_config_t* config,
                                         gpio_num_t               pin,
                                         uint32_t*                level);

/**
 * @brief Function pointer type for setting up GPIO interrupt.
 * @param config Pointer to the initialized GPIO bus configuration.
 * @param pin GPIO pin number to configure interrupt for.
 * @param intr_type Interrupt type (rising edge, falling edge, etc.).
 * @param isr_handler ISR handler function. Can be NULL to disable interrupt.
 * @param args Arguments to pass to ISR handler. Can be NULL.
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*star_gpio_set_interrupt_fn_t)(const star_bus_config_t* config,
                                                  gpio_num_t               pin,
                                                  gpio_int_type_t          intr_type,
                                                  gpio_isr_t               isr_handler,
                                                  void*                    args);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_FUNCTION_TYPES_H */
