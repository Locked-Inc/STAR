/* include/rx_bus_types.h */

/**
 * @file rx_bus_types.h
 * @brief Bus manager type definitions for RX72N
 * @details
 * Defines core data structures for the bus manager:
 * - Bus types (GPIO, ADC, I2C, SPI, etc.)
 * - Bus configuration nodes with protocol-specific data
 * - Bus manager structure with ThreadX mutex for thread safety
 *
 * Follows Dependency Inversion Principle by injecting error and pin
 * validation interfaces for testability.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BUS_TYPES_H
#define STAR_RX72N_BUS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_pin_interface.h"
#include "tx_api.h" /* ThreadX */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bus Type Enumeration
 * =============================================================================
 */

/**
 * @brief Supported bus/protocol types
 */
typedef enum {
  k_bus_type_gpio = 0, /* GPIO (digital I/O) */
  k_bus_type_adc,      /* ADC (analog input) */
  k_bus_type_i2c,      /* I2C (RIIC peripheral) */
  k_bus_type_smbus,    /* SMBUS (I2C variant for fuel gauge) */
  k_bus_type_spi,      /* SPI (RSPI peripheral) */
  k_bus_type_uart,     /* UART (SCI peripheral) */
  k_bus_type_onewire,  /* 1-Wire (GPIO bit-bang) */
  k_bus_type_max,      /* Sentinel value */
} rx_bus_type_t;

/* =============================================================================
 * Protocol-Specific Configuration Structures
 * =============================================================================
 */

/**
 * @brief GPIO bus configuration
 */
typedef struct {
  uint8_t port; /* GPIO port number (0-J = 0-9) */
  uint8_t pin;  /* Pin number (0-7) */
} rx_gpio_bus_config_t;

/**
 * @brief ADC bus configuration
 */
typedef struct {
  uint8_t unit;    /* ADC unit (0 or 1) */
  uint8_t channel; /* ADC channel (0-20 depending on unit) */
  uint8_t bits;    /* Resolution: 8, 10, or 12 bits */
} rx_adc_bus_config_t;

/**
 * @brief I2C bus configuration
 */
typedef struct {
  uint8_t  channel;      /* RIIC channel (0-2) */
  uint8_t  sda_port;     /* SDA pin port */
  uint8_t  sda_pin;      /* SDA pin number */
  uint8_t  scl_port;     /* SCL pin port */
  uint8_t  scl_pin;      /* SCL pin number */
  uint32_t frequency_hz; /* Clock frequency (100kHz, 400kHz, 1MHz) */
  uint8_t  device_addr;  /* 7-bit device address */
} rx_i2c_bus_config_t;

/**
 * @brief SMBUS bus configuration (extends I2C)
 */
typedef struct {
  rx_i2c_bus_config_t i2c_config; /* Base I2C configuration */
  bool                use_pec;    /* Use Packet Error Checking (CRC-8) */
} rx_smbus_bus_config_t;

/**
 * @brief SPI bus configuration
 */
typedef struct {
  uint8_t  channel;      /* RSPI channel (0-2) */
  uint8_t  copi_port;    /* COPI (MOSI) pin port */
  uint8_t  copi_pin;     /* COPI pin number */
  uint8_t  cipo_port;    /* CIPO (MISO) pin port */
  uint8_t  cipo_pin;     /* CIPO pin number */
  uint8_t  sck_port;     /* SCK pin port */
  uint8_t  sck_pin;      /* SCK pin number */
  uint8_t  cs_port;      /* CS pin port */
  uint8_t  cs_pin;       /* CS pin number */
  uint32_t frequency_hz; /* SPI clock frequency */
  uint8_t  mode;         /* SPI mode (0-3) */
} rx_spi_bus_config_t;

/**
 * @brief UART bus configuration
 */
typedef struct {
  uint8_t  channel;  /* SCI channel (0-12) */
  uint8_t  tx_port;  /* TX pin port */
  uint8_t  tx_pin;   /* TX pin number */
  uint8_t  rx_port;  /* RX pin port */
  uint8_t  rx_pin;   /* RX pin number */
  uint32_t baudrate; /* Baud rate (9600, 115200, etc.) */
} rx_uart_bus_config_t;

/**
 * @brief 1-Wire bus configuration
 */
typedef struct {
  uint8_t port; /* 1-Wire data pin port */
  uint8_t pin;  /* 1-Wire data pin number */
} rx_onewire_bus_config_t;

/* =============================================================================
 * Hardware Limits (RX72N)
 * =============================================================================
 */

/**
 * @brief RX72N RIIC channel count
 */
typedef enum {
  k_riic_channel_count = 3, /* RIIC channels 0-2 */
} rx_riic_limits_t;

/**
 * @brief RX72N RSPI channel count
 */
typedef enum {
  k_rspi_channel_count = 3, /* RSPI channels 0-2 */
} rx_rspi_limits_t;

/**
 * @brief RX72N ADC unit count
 */
typedef enum {
  k_adc_unit_count = 2, /* ADC units 0-1 */
} rx_adc_limits_t;

/**
 * @brief RX72N GPIO port limits
 */
typedef enum {
  k_gpio_port_max_numeric = 9,    /* Numeric ports 0-9 */
  k_gpio_port_max_alpha   = 0x10, /* Alpha ports A-G (0xA-0x10) */
  k_gpio_port_min_alpha   = 0xA,  /* First alpha port (A) */
} rx_gpio_port_limits_t;

/**
 * @brief RX72N GPIO pin count per port
 */
typedef enum {
  k_gpio_pin_count = 8, /* Pins 0-7 per port */
} rx_gpio_pin_limits_t;

/**
 * @brief RX72N ADC channel limits
 */
typedef enum {
  k_adc_channel_max = 7, /* ADC channels 0-7 */
} rx_adc_channel_limits_t;

/**
 * @brief RX72N ADC resolution options
 */
typedef enum {
  k_adc_resolution_8bit  = 8,  /* 8-bit resolution */
  k_adc_resolution_10bit = 10, /* 10-bit resolution */
  k_adc_resolution_12bit = 12, /* 12-bit resolution */
} rx_adc_resolution_t;

/**
 * @brief I2C/SMBUS protocol constants
 */
typedef enum {
  k_i2c_write_bit        = 0,    /* I2C write bit (R/W = 0) */
  k_i2c_read_bit         = 1,    /* I2C read bit (R/W = 1) */
  k_i2c_addr_shift       = 1,    /* Bit shift for 7-bit address */
  k_i2c_addr_max_7bit    = 0x7F, /* Maximum 7-bit I2C address (127) */
  k_bits_per_byte        = 8,    /* Bits per byte */
  k_smbus_max_block_size = 32,   /* SMBUS maximum block transfer size */
} rx_i2c_constants_t;

/**
 * @brief Bit manipulation masks
 */
typedef enum {
  k_byte_mask     = 0xFF, /* Full byte mask (all 8 bits) */
  k_byte_msb_mask = 0x80, /* Most significant bit of a byte (bit 7) */
} bit_masks_t;

/**
 * @brief SMBUS buffer and transfer sizes
 */
typedef enum {
  k_smbus_single_byte      = 1, /* Single byte transfer size */
  k_smbus_byte_buf_size    = 2, /* Byte operation buffer (data + PEC) */
  k_smbus_word_data_bytes  = 2, /* Word data size (LSB + MSB) */
  k_smbus_word_buf_size    = 3, /* Word operation buffer (LSB + MSB + PEC) */
} smbus_sizes_t;

/**
 * @brief SMBUS byte operation buffer indices (data + optional PEC)
 */
typedef enum {
  k_smbus_byte_data = 0, /* Data byte index */
  k_smbus_byte_pec  = 1, /* PEC (CRC-8) index */
} smbus_byte_pec_idx_t;

/**
 * @brief SMBUS word operation buffer indices (LSB + MSB + optional PEC)
 */
typedef enum {
  k_smbus_word_lsb = 0, /* Low byte (LSB) index */
  k_smbus_word_msb = 1, /* High byte (MSB) index */
  k_smbus_word_pec = 2, /* PEC (CRC-8) index */
} smbus_word_pec_idx_t;

/* =============================================================================
 * Bus Configuration Node (Linked List)
 * =============================================================================
 */

/**
 * @brief Bus configuration structure (linked list node)
 *
 * Contains configuration for a specific bus instance. Organized as a
 * linked list managed by the bus manager.
 */
typedef struct rx_bus_config {
  const char*   name;        /* Unique bus name */
  rx_bus_type_t type;        /* Bus type */
  bool          initialized; /* Is bus initialized? */
  void*         handle;      /* Driver-specific handle */
  void*         user_ctx;    /* User context for callbacks */

  /* Protocol-specific configuration union */
  union {
    rx_gpio_bus_config_t    gpio;
    rx_adc_bus_config_t     adc;
    rx_i2c_bus_config_t     i2c;
    rx_smbus_bus_config_t   smbus;
    rx_spi_bus_config_t     spi;
    rx_uart_bus_config_t    uart;
    rx_onewire_bus_config_t onewire;
  } proto;

  struct rx_bus_config* next; /* Next bus in linked list */
} rx_bus_config_t;

/* =============================================================================
 * Bus Manager Structure
 * =============================================================================
 */

/**
 * @brief Maximum buses supported
 */
typedef enum {
  k_max_buses = 32, /* Maximum number of buses */
} bus_manager_limits_t;

/**
 * @brief Bus manager structure (thread-safe)
 *
 * Maintains a linked list of bus configurations with ThreadX mutex
 * protection for concurrent access.
 */
typedef struct {
  rx_bus_config_t*      buses;       /* Linked list of bus configurations */
  TX_MUTEX              mutex;       /* ThreadX mutex for thread safety */
  const char*           tag;         /* Logging tag */
  rx_error_interface_t* error_iface; /* Error handler (DIP) */
  rx_pin_interface_t*   pin_iface;   /* Pin validator (DIP) */

  /* Resource tracking */
  bool riic_initialized[k_riic_channel_count];     /* RIIC channels 0-2 initialized? */
  bool rspi_initialized[k_rspi_channel_count];     /* RSPI channels 0-2 initialized? */
  bool adc_unit_initialized[k_adc_unit_count];     /* ADC units 0-1 initialized? */

  uint8_t bus_count; /* Number of buses registered */
} rx_bus_manager_t;

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Bus manager configuration constants
 */
typedef enum {
  k_bus_manager_mutex_timeout_ms = 1000, /* Default mutex timeout (ms) */
} rx_bus_manager_config_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_TYPES_H */
