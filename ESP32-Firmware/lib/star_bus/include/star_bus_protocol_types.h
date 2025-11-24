/* esp32-firmware/components/star_bus/include/star_bus_protocol_types.h */

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
  spi_host_device_t host; /**< SPI host number */
  bool
    is_peripheral; /**< True if operating as SPI peripheral (slave), false if controller (master) */
  spi_bus_config_t bus_cfg; /**< Underlying ESP-IDF SPI bus configuration */
                            /* Note: bus_cfg members like mosi_io_num are renamed below */
  spi_device_interface_config_t
    dev_cfg; /**< Underlying ESP-IDF SPI device configuration (controller mode only) */
  star_spi_callbacks_t callbacks; /**< User-provided callback functions */
  star_spi_ops_t       ops;       /**< Operation function pointers (defaults provided) */

  /* --- Renamed members within spi_bus_config_t for internal use --- */
  /* These correspond to the fields in the ESP-IDF spi_bus_config_t */
  /* SPI pin naming follows OSHWA (Open Source Hardware Association) standard:
   * - COPI (Controller Out, Peripheral In) - replaces MOSI (Master Out, Slave In)
   * - CIPO (Controller In, Peripheral Out) - replaces MISO (Master In, Slave Out)
   * This modern terminology removes master/slave language while maintaining clarity.
   * The ESP-IDF still uses mosi_io_num/miso_io_num internally, which we map here.
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
 * @brief GPIO callback functions structure.
 */
typedef struct star_gpio_callbacks {
  star_gpio_state_change_cb_t on_state_change; /**< Optional: Called after GPIO state changes */
  star_gpio_error_cb_t        on_error;        /**< Optional: Called on GPIO operation error */
} star_gpio_callbacks_t;

/**
 * @brief GPIO operation functions structure.
 */
typedef struct star_gpio_ops {
  star_gpio_configure_fn_t     configure;     /**< Configure function pointer (mode, pull) */
  star_gpio_write_fn_t         write;         /**< Write function pointer (set level) */
  star_gpio_read_fn_t          read;          /**< Read function pointer (get level) */
  star_gpio_set_interrupt_fn_t set_interrupt; /**< Set interrupt function pointer */
} star_gpio_ops_t;

/**
 * @brief GPIO bus configuration structure (part of star_bus_config_t).
 *
 * Manages a collection of GPIO pins as a logical "bus" for device control.
 * Useful for sensors like HC-SR04 that use GPIO pins directly rather than
 * a traditional communication bus (I2C/SPI).
 */
typedef struct star_gpio_bus_config {
  gpio_num_t            pins[8];       /**< Array of GPIO pins managed by this bus (up to 8) */
  uint8_t               pin_count;     /**< Number of pins in use */
  star_gpio_callbacks_t callbacks;     /**< User-provided callback functions */
  star_gpio_ops_t       ops;           /**< Operation function pointers (defaults provided) */
  bool                  isr_installed; /**< True if ISR service is installed */
} star_gpio_bus_config_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_PROTOCOL_TYPES_H */
