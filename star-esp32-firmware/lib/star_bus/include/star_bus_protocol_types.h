/**
 * @file star_bus_protocol_types.h
 * @brief Protocol-specific configuration structures for all bus types
 * @details
 * This file defines configuration structures for each supported bus protocol,
 * including callbacks and operation function pointers.
 *
 * Key structures defined:
 * - I2C: star_i2c_bus_config_t, star_i2c_callbacks_t, star_i2c_ops_t
 * - SPI: star_spi_bus_config_t, star_spi_callbacks_t, star_spi_ops_t
 * - GPIO: star_gpio_bus_config_t, star_gpio_ops_t
 * - ADC: star_adc_bus_config_t, star_adc_ops_t
 * - OneWire: star_onewire_bus_config_t, star_onewire_ops_t
 *
 * Each protocol structure contains:
 * - ESP-IDF native configuration (i2c_config_t, spi_bus_config_t, etc.)
 * - User callbacks for events (optional)
 * - Operation function pointers (defaults provided by bus manager)
 *
 * Note: SPI pin naming follows OSHWA standards (COPI/CIPO instead of MOSI/MISO).
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_COMPONENT_BUS_PROTOCOL_TYPES_H
#define STAR_COMPONENT_BUS_PROTOCOL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h" /* Added for SPI */

#include <stdbool.h>
#include <stdint.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "star_bus_common_types.h"
#include "star_bus_event_types.h"
#include "star_bus_function_types.h"

/* --- Protocol Configuration Structures --- */

/**
 * @brief I2C callback functions structure.
 */
typedef struct star_i2c_callbacks {
  star_i2c_transfer_complete_cb_t
                      on_transfer_complete; /**< Optional: Called after successful read/write */
  star_i2c_error_cb_t on_error;             /**< Optional: Called on I2C transaction error */
} star_i2c_callbacks_t;

/**
 * @brief I2C operation functions structure.
 */
typedef struct star_i2c_ops {
  star_i2c_write_fn_t         write;         /**< Write function pointer (reg_addr + data) */
  star_i2c_read_fn_t          read;          /**< Read function pointer (reg_addr + data) */
  star_i2c_write_command_fn_t write_command; /**< Write function pointer (command byte only) */
  star_i2c_read_raw_fn_t      read_raw;      /**< Read function pointer (raw bytes, no reg_addr) */
} star_i2c_ops_t;

/**
 * @brief I2C bus configuration structure (part of star_bus_config_t).
 */
typedef struct star_i2c_bus_config {
  i2c_port_t           port;      /**< I2C port number */
  i2c_config_t         config;    /**< Underlying ESP-IDF I2C configuration */
  uint8_t              address;   /**< 7-bit I2C device address for this config */
  star_i2c_callbacks_t callbacks; /**< User-provided callback functions */
  star_i2c_ops_t       ops;       /**< Operation function pointers (defaults provided) */
} star_i2c_bus_config_t;

/**
 * @brief SPI callback functions structure.
 */
typedef struct star_spi_callbacks {
  star_spi_transfer_complete_cb_t
                      on_transfer_complete; /**< Optional: Called after successful transfer */
  star_spi_error_cb_t on_error;             /**< Optional: Called on SPI transaction error */
} star_spi_callbacks_t;

/**
 * @brief SPI operation functions structure.
 */
typedef struct star_spi_ops {
  star_spi_transmit_fn_t transmit; /**< Transmit function pointer */
  star_spi_receive_fn_t  receive;  /**< Receive function pointer */
  star_spi_transfer_fn_t transfer; /**< Full-duplex transfer function pointer */
} star_spi_ops_t;

/**
 * @brief SPI bus configuration structure (part of star_bus_config_t).
 */
typedef struct star_spi_bus_config {
  spi_host_device_t host;          /**< SPI host number */
  bool              is_peripheral; /**< True if operating as SPI peripheral, false if controller */
  spi_bus_config_t  bus_cfg;       /**< Underlying ESP-IDF SPI bus configuration */
                                   /* Note: bus_cfg members like mosi_io_num are renamed below */
  spi_device_interface_config_t
    dev_cfg; /**< Underlying ESP-IDF SPI device configuration (controller mode only) */
  star_spi_callbacks_t callbacks; /**< User-provided callback functions */
  star_spi_ops_t       ops;       /**< Operation function pointers (defaults provided) */

  /* --- Renamed members within spi_bus_config_t for internal use --- */
  /* These correspond to the fields in the ESP-IDF spi_bus_config_t */
  /* SPI pin naming follows OSHWA (Open Source Hardware Association) standard:
   * - COPI (Controller Out, Peripheral In)
   * - CIPO (Controller In, Peripheral Out)
   * The ESP-IDF still uses legacy mosi_io_num/miso_io_num field names internally, which we map here.
   */
  gpio_num_t copi_io_num;     /**< GPIO pin for COPI (Controller Out, Peripheral In) */
  gpio_num_t cipo_io_num;     /**< GPIO pin for CIPO (Controller In, Peripheral Out) */
  gpio_num_t sclk_io_num;     /**< GPIO pin for SCLK (Serial Clock) */
  gpio_num_t cs_io_num;       /**< GPIO pin for CS (Chip Select) - used in peripheral mode */
  gpio_num_t quadwp_io_num;   /**< GPIO pin for Quad SPI WP (Write Protect) */
  gpio_num_t quadhd_io_num;   /**< GPIO pin for Quad SPI HD (Hold) */
  int32_t    max_transfer_sz; /**< Maximum transfer size, in bytes */
  uint8_t    queue_size;      /**< Transaction queue size (peripheral mode) */
  uint8_t    mode;            /**< SPI mode 0-3 (peripheral mode) */
  /* flags and intr_flags are also part of spi_bus_config_t if needed */

} star_spi_bus_config_t;

/**
 * @brief GPIO operation functions structure.
 */
typedef struct star_gpio_ops {
  esp_err_t (*write_digital)(const struct star_bus_config* config,
                             uint8_t                       pin_index,
                             uint8_t                       value);
  esp_err_t (*read_digital)(const struct star_bus_config* config,
                            uint8_t                       pin_index,
                            uint8_t*                      value);
} star_gpio_ops_t;

/**
 * @brief GPIO bus configuration structure.
 */
typedef struct star_gpio_bus_config {
  gpio_num_t*     pins;          /**< Array of GPIO pins */
  uint8_t         pin_count;     /**< Number of pins in array */
  bool            isr_installed; /**< Whether ISR service is installed */
  star_gpio_ops_t ops;           /**< Operation function pointers */
} star_gpio_bus_config_t;

/**
 * @brief ADC operation functions structure.
 *
 * @note Uses int32_t* for output parameters in our API. Implementations should
 *       use 'int' when calling ESP-IDF functions and perform explicit conversions.
 */
typedef struct star_adc_ops {
  esp_err_t (*read_raw)(const struct star_bus_config* config, int32_t* out_raw);
  esp_err_t (*read_voltage)(const struct star_bus_config* config, int32_t* out_voltage_mv);
} star_adc_ops_t;

/**
 * @brief ADC bus configuration structure.
 */
typedef struct star_adc_bus_config {
  adc_oneshot_unit_handle_t unit_handle; /**< ADC unit handle (shared across channels) */
  adc_cali_handle_t         cali_handle; /**< ADC calibration handle */
  adc_unit_t                unit;        /**< ADC unit ID (for reference counting) */
  adc_channel_t             channel;     /**< ADC channel */
  adc_bitwidth_t            bitwidth;    /**< ADC bit width */
  adc_atten_t               atten;       /**< ADC attenuation */
  star_adc_ops_t            ops;         /**< Operation function pointers */
} star_adc_bus_config_t;

/**
 * @brief OneWire operation functions structure.
 */
typedef struct star_onewire_ops {
  esp_err_t (*reset)(const struct star_bus_config* config, bool* present);
  esp_err_t (*write_byte)(const struct star_bus_config* config, uint8_t byte);
  esp_err_t (*read_byte)(const struct star_bus_config* config, uint8_t* byte);
  esp_err_t (*write_bytes)(const struct star_bus_config* config,
                           uint64_t                      rom,
                           const uint8_t*                data,
                           size_t                        len);
  esp_err_t (*read_bytes)(const struct star_bus_config* config, uint8_t* data, size_t len);
  esp_err_t (*search)(const struct star_bus_config* config,
                      uint64_t*                     roms,
                      size_t                        max_devices,
                      size_t*                       count);
} star_onewire_ops_t;

/**
 * @brief OneWire bus configuration structure.
 */
typedef struct star_onewire_bus_config {
  gpio_num_t         gpio_pin;            /**< Single 1-Wire data pin (open-drain) */
  bool               use_parasitic_power; /**< Enable parasite power mode */
  bool               use_strong_pullup;   /**< Enable strong pullup for power-hungry operations */
  uint8_t            speed;               /**< Speed mode (0=standard, 1=overdrive) */
  uint32_t           search_timeout_ms;   /**< Timeout for device search operations */
  star_onewire_ops_t ops;                 /**< Operation function pointers (defaults provided) */
} star_onewire_bus_config_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_PROTOCOL_TYPES_H */
