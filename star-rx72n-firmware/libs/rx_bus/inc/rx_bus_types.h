/**
 * @file rx_bus_types.h
 * @brief Bus Manager Type Definitions for RX72N Multi-Protocol Communication
 *
 * @details
 * # Overview
 *
 * This header defines all data structures for the RX72N bus manager, which provides
 * a unified interface for managing multiple communication protocols (GPIO, ADC, I2C,
 * SPI, UART, 1-Wire). The bus manager enables centralized configuration,
 * thread-safe access, and consistent error handling across all bus types.
 *
 * ## Architecture Overview
 *
 * @dot
 * digraph bus_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_manager {
 *     label="Bus Manager (Thread-Safe)";
 *     style=filled;
 *     fillcolor=lightblue;
 *
 *     mutex [label="ThreadX Mutex"];
 *     list [label="Bus Config\nLinked List"];
 *     tracking [label="Resource\nTracking"];
 *   }
 *
 *   subgraph cluster_buses {
 *     label="Bus Configurations";
 *     style=filled;
 *     fillcolor=lightgreen;
 *
 *     gpio [label="GPIO\n(Port Pins)"];
 *     adc [label="ADC\n(S12ADFa)"];
 *     i2c [label="I2C\n(RIIC)"];
 *     spi [label="SPI\n(RSPI)"];
 *     uart [label="UART\n(SCI)"];
 *     onewire [label="1-Wire\n(Bit-bang)"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="RX72N Hardware";
 *     style=filled;
 *     fillcolor=lightyellow;
 *
 *     riic [label="RIIC\n(3 channels)"];
 *     rspi [label="RSPI\n(3 channels)"];
 *     sci [label="SCI\n(13 channels)"];
 *     s12ad [label="S12ADFa\n(2 units)"];
 *     ports [label="I/O Ports\n(10 ports)"];
 *   }
 *
 *   list -> gpio -> ports;
 *   list -> adc -> s12ad;
 *   list -> i2c -> riic;
 *   list -> spi -> rspi;
 *   list -> uart -> sci;
 *   list -> onewire -> ports;
 * }
 * @enddot
 *
 * ## Supported Bus Types
 *
 * | Bus Type | RX72N Peripheral | Max Instances | Typical Use |
 * |----------|------------------|---------------|-------------|
 * | **GPIO** | I/O Ports A-J | ~80 pins | LEDs, buttons, chip selects |
 * | **ADC** | S12ADFa | 2 units x 8 ch | Current sensing, analog inputs |
 * | **I2C** | RIIC | 3 channels | IMU, temperature, pressure sensors |
 * | **SPI** | RSPI | 3 channels | RPi5 communication, sensors |
 * | **UART** | SCI | 13 channels | Debug console, RS-485 |
 * | **1-Wire** | GPIO bit-bang | Unlimited | Temperature sensors (DS18B20) |
 *
 * ## Data Flow: Bus Registration and Access
 *
 * @msc
 * msc {
 *   width=700;
 *   App, Manager, Config, Hardware;
 *
 *   --- [label="Registration Phase"];
 *   App => Manager [label="rx_bus_manager_add_bus(config)"];
 *   Manager => Manager [label="Acquire mutex"];
 *   Manager => Config [label="Add to linked list"];
 *   Manager => Manager [label="Release mutex"];
 *   Manager >> App [label="k_rx_ok"];
 *
 *   --- [label="Access Phase"];
 *   App => Manager [label="rx_bus_manager_with_bus(name, callback)"];
 *   Manager => Manager [label="Acquire mutex"];
 *   Manager => Config [label="Find by name"];
 *   Manager => App [label="callback(bus_config)"];
 *   App => Hardware [label="Read/Write"];
 *   Hardware >> App [label="Data"];
 *   App >> Manager [label="k_rx_ok"];
 *   Manager => Manager [label="Release mutex"];
 *   Manager >> App [label="k_rx_ok"];
 * }
 * @endmsc
 *
 * ## Memory Layout Overview
 *
 * | Structure | Size (bytes) | Alignment | Notes |
 * |-----------|--------------|-----------|-------|
 * | rx_bus_type_t | 1 | 1 | C23 typed enum |
 * | rx_bus_config_t | ~64 | 4 | Union of protocol configs |
 * | rx_bus_manager_t | ~128 | 4 | Includes TX_MUTEX |
 * | Protocol configs | 4-24 | 4 | Per-protocol structs |
 *
 * ## Hardware Requirements (RX72N)
 *
 * | Resource | Available | Notes |
 * |----------|-----------|-------|
 * | **RIIC channels** | 0, 1, 2 | 100 kHz, 400 kHz, 1 MHz |
 * | **RSPI channels** | 0, 1, 2 | Up to 15 Mbps |
 * | **SCI channels** | 0-12 | UART, SPI, I2C modes |
 * | **ADC units** | 0, 1 | 12-bit, 8 channels each |
 * | **GPIO pins** | ~80 | Port A-J |
 *
 * ## Design Patterns Applied
 *
 * | Pattern | Application |
 * |---------|-------------|
 * | **Dependency Injection** | Error/pin interfaces injected at init |
 * | **Command Pattern** | Bus operations via rx_bus_command_t |
 * | **Strategy Pattern** | Protocol-specific config in union |
 * | **Registry Pattern** | Bus manager as central registry |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | **Rule 1** | [PASS] | No goto, setjmp, recursion |
 * | **Rule 2** | [PASS] | Loop bounds via k_max_buses |
 * | **Rule 3** | [PASS] | Static allocation only (bus pool) |
 * | **Rule 4** | [PASS] | Structures, not functions |
 * | **Rule 5** | [PASS] | Field constraints documented |
 * | **Rule 6** | [PASS] | Smallest scope (struct-local) |
 * | **Rule 7** | [PASS] | Return checks via rx_err_t |
 * | **Rule 8** | [PASS] | C23 typed enums, no macros |
 * | **Rule 9** | [PASS] | Single-level pointers |
 * | **Rule 10** | [PASS] | -Wall -Wextra -Werror clean |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S - Single Responsibility** | Each struct handles one concern |
 * | **O - Open/Closed** | New protocols via union extension |
 * | **L - Liskov Substitution** | All configs substitutable in manager |
 * | **I - Interface Segregation** | Protocol-specific structs |
 * | **D - Dependency Inversion** | error_iface, pin_iface injected |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=LR;
 *   node [shape=box];
 *
 *   bus_types [label="rx_bus_types.h", fillcolor=lightgreen, style=filled];
 *   rx_err [label="rx_err.h"];
 *   error_iface [label="rx_error_interface.h"];
 *   pin_iface [label="rx_pin_interface.h"];
 *   gpio_const [label="rx_gpio_constants.h"];
 *   port_const [label="rx_port_constants.h"];
 *   tx_api [label="tx_api.h (ThreadX)"];
 *
 *   bus_types -> rx_err;
 *   bus_types -> error_iface;
 *   bus_types -> pin_iface;
 *   bus_types -> gpio_const;
 *   bus_types -> port_const;
 *   bus_types -> tx_api;
 * }
 * @enddot
 *
 * @see rx_bus_manager.h Bus manager API
 * @see rx_bus_command.h Command pattern interface
 * @see rx_error_interface.h Error handler abstraction
 * @see rx_pin_interface.h Pin validator abstraction
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_gpio_constants.h"
#include "rx_pin_interface.h"
#include "rx_port_constants.h"
#include "tx_api.h" /* ThreadX */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bus Type Enumeration
 * =============================================================================
 */

/**
 * @enum rx_bus_type_t
 * @brief Supported bus/protocol types for RX72N bus manager
 *
 * @details
 * Enumerates all communication protocols supported by the bus manager.
 * Each type corresponds to a specific RX72N peripheral or software
 * bit-bang implementation.
 *
 * ## Protocol Comparison
 *
 * | Type | Peripheral | Speed | Wires | Typical Use |
 * |------|------------|-------|-------|-------------|
 * | GPIO | I/O Ports | N/A | 1 | Digital I/O, chip selects |
 * | ADC | S12ADFa | ~1 us/sample | 1 | Analog sensing |
 * | I2C | RIIC | 100-1000 kHz | 2 | Sensors, EEPROMs |
 * | SPI | RSPI | 1-15 MHz | 4 | RPi5 communication, sensors |
 * | UART | SCI | 9.6-921.6 kbps | 2 | Debug console, RS-485 |
 * | 1-Wire | GPIO | ~15 kbps | 1 | Temperature sensors |
 *
 * @par Usage Example:
 * @code{.c}
 * rx_bus_config_t config = {
 *     .name = "rpi5_spi",
 *     .type = k_bus_type_spi,  // Use SPI protocol
 *     .proto.spi = {
 *         .channel = k_rspi_channel_0,
 *         .frequency_hz = k_rspi_freq_10mhz,
 *         .mode = k_rspi_mode_0
 *     }
 * };
 * @endcode
 *
 * @see rx_bus_config_t Configuration structure using this type
 * @see rx_bus_manager_add_bus() Register bus with manager
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief General Purpose Input/Output (digital I/O)
   * @details
   * Direct GPIO pin control for digital signals. Used for:
   * - LED indicators
   * - Button inputs
   * - SPI chip selects (manual control)
   * - Motor enable/disable signals
   * @par RX72N Peripheral: I/O Ports (PORT0-PORT9, PORTA-PORTJ)
   * @see rx_gpio_bus_config_t GPIO configuration structure
   */
  k_bus_type_gpio = 0,

  /**
   * @brief Analog-to-Digital Converter (analog input)
   * @details
   * Analog voltage measurement. Used for:
   * - Motor current sensing
   * - Analog sensor inputs
   * - Temperature sensing (analog sensors)
   * @par RX72N Peripheral: S12ADFa (12-bit ADC)
   * @par Resolution: 8, 10, or 12 bits
   * @par Conversion time: ~1 us (12-bit @ 60 MHz PCLKD)
   * @see rx_adc_bus_config_t ADC configuration structure
   */
  k_bus_type_adc,

  /**
   * @brief Inter-Integrated Circuit (I2C/IIC/TWI)
   * @details
   * Two-wire synchronous serial communication. Used for:
   * - IMU sensors (MPU6050)
   * - Temperature sensors (LM75)
   * - EEPROMs
   * - RTC
   * @par RX72N Peripheral: RIIC (3 channels)
   * @par Speeds: 100 kHz (standard), 400 kHz (fast), 1 MHz (fast-plus)
   * @par Addressing: 7-bit (0x00-0x7F)
   * @see rx_i2c_bus_config_t I2C configuration structure
   */
  k_bus_type_i2c,

  /**
   * @brief Serial Peripheral Interface (4-wire synchronous)
   * @details
   * High-speed synchronous serial communication. Used for:
   * - RPi5 command/telemetry link (RSPI0 peripheral mode)
   * - High-speed sensors
   * - Flash memory
   * @par RX72N Peripheral: RSPI (3 channels)
   * @par Speed: Up to 15 MHz
   * @par Modes: 0, 1, 2, 3 (CPOL/CPHA combinations)
   * @see rx_spi_bus_config_t SPI configuration structure
   */
  k_bus_type_spi,

  /**
   * @brief Universal Asynchronous Receiver/Transmitter
   * @details
   * Asynchronous serial communication. Used for:
   * - Debug console
   * - RS-485 bus
   * - GPS modules
   * - Bluetooth modules
   * @par RX72N Peripheral: SCI (13 channels)
   * @par Baud rates: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
   * @see rx_uart_bus_config_t UART configuration structure
   */
  k_bus_type_uart,

  /**
   * @brief Dallas/Maxim 1-Wire protocol (single-wire bidirectional)
   * @details
   * Single-wire communication with parasitic power. Used for:
   * - Temperature sensors (DS18B20)
   * - iButton devices
   * - Serial number EEPROMs
   * @par Implementation: GPIO bit-banging (software)
   * @par Speed: ~15 kbps (standard timing)
   * @par Topology: Multi-drop (multiple devices on one wire)
   * @see rx_onewire_bus_config_t 1-Wire configuration structure
   */
  k_bus_type_onewire,

  /**
   * @brief Sentinel value for bounds checking
   * @details
   * Used to validate bus type values. Always the last enum value.
   * @warning Do not use as an actual bus type
   * @invariant k_bus_type_max > all valid bus types
   */
  k_bus_type_max,
} rx_bus_type_t;

/* =============================================================================
 * Protocol-Specific Configuration Structures
 * =============================================================================
 */

/**
 * @brief GPIO bus configuration
 */
typedef struct {
  rx_port_pin_t pin; /**< GPIO pin (rx_port_pin_t) */
} rx_gpio_bus_config_t;

/**
 * @brief ADC bus configuration
 */
typedef struct {
  uint8_t unit;    /**< ADC unit (0 or 1) */
  uint8_t channel; /**< ADC channel (0-20 depending on unit) */
  uint8_t bits;    /**< Resolution: 8, 10, or 12 bits */
} rx_adc_bus_config_t;

/**
 * @brief I2C bus configuration
 */
typedef struct {
  uint8_t       channel;      /**< RIIC channel (0-2) */
  rx_port_pin_t sda_pin;      /**< SDA pin (type-safe enum) */
  rx_port_pin_t scl_pin;      /**< SCL pin (type-safe enum) */
  uint32_t      frequency_hz; /**< Clock frequency (100kHz, 400kHz, 1MHz) */
  uint8_t       device_addr;  /**< 7-bit device address */
} rx_i2c_bus_config_t;

/**
 * @brief SPI bus configuration
 */
typedef struct {
  uint8_t       channel;      /**< RSPI channel (0-2) */
  rx_port_pin_t copi_pin;     /**< COPI pin (type-safe enum) */
  rx_port_pin_t cipo_pin;     /**< CIPO pin (type-safe enum) */
  rx_port_pin_t sck_pin;      /**< SCK pin (type-safe enum) */
  rx_port_pin_t cs_pin;       /**< CS pin (type-safe enum) */
  uint32_t      frequency_hz; /**< SPI clock frequency */
  uint8_t       mode;         /**< SPI mode (0-3) */
} rx_spi_bus_config_t;

/**
 * @brief UART bus configuration
 */
typedef struct {
  uint8_t       channel;  /**< SCI channel (0-12) */
  rx_port_pin_t tx_pin;   /**< TX pin (type-safe enum) */
  rx_port_pin_t rx_pin;   /**< RX pin (type-safe enum) */
  uint32_t      baudrate; /**< Baud rate (9600, 115200, etc.) */
} rx_uart_bus_config_t;

/**
 * @brief 1-Wire bus configuration
 */
typedef struct {
  rx_port_pin_t pin; /**< 1-Wire data pin (type-safe enum) */
} rx_onewire_bus_config_t;

/* =============================================================================
 * Hardware Limits (RX72N)
 * =============================================================================
 */

/**
 * @brief RX72N RIIC channel count
 */
typedef enum : uint8_t {
  k_riic_channel_count = 3, /**< RIIC channels 0-2 */
} rx_riic_limits_t;

/**
 * @brief RX72N RSPI channel count
 */
typedef enum : uint8_t {
  k_rspi_channel_count = 3, /**< RSPI channels 0-2 */
} rx_rspi_limits_t;

/**
 * @brief RX72N ADC unit count
 */
typedef enum : uint8_t {
  k_adc_unit_count = 2, /**< ADC units 0-1 */
} rx_adc_limits_t;

/**
 * @brief RX72N SCI channel count
 */
typedef enum : uint8_t {
  k_sci_channel_count = 13, /**< SCI channels 0-12 */
} rx_sci_limits_t;

/**
 * @brief RX72N ADC channel limits
 */
typedef enum : uint8_t {
  k_adc_channel_max = 7, /**< ADC channels 0-7 */
} rx_adc_channel_limits_t;

/**
 * @brief RX72N ADC resolution options
 */
typedef enum : uint8_t {
  k_adc_resolution_8bit  = 8,  /**< 8-bit resolution */
  k_adc_resolution_10bit = 10, /**< 10-bit resolution */
  k_adc_resolution_12bit = 12, /**< 12-bit resolution */
} rx_adc_resolution_t;

/**
 * @brief I2C protocol constants
 */
typedef enum : uint8_t {
  k_i2c_write_bit     = 0,   /**< I2C write bit (R/W = 0) */
  k_i2c_read_bit      = 1,   /**< I2C read bit (R/W = 1) */
  k_i2c_addr_shift    = 1,   /**< Bit shift for 7-bit address */
  k_i2c_addr_max_7bit = 127, /**< Maximum 7-bit I2C address (127) */
  k_bits_per_byte     = 8,   /**< Bits per byte */
} rx_i2c_constants_t;

/**
 * @brief Bit manipulation masks
 */
typedef enum : uint8_t {
  k_byte_mask     = 255, /**< Full byte mask (all 8 bits) */
  k_byte_msb_mask = 128, /**< Most significant bit of a byte (bit 7) */
} bit_masks_t;

/* =============================================================================
 * Bus Configuration Node (Linked List)
 * =============================================================================
 */

/**
 * @struct rx_bus_config_t
 * @brief Bus configuration structure (linked list node)
 *
 * @details
 * Contains complete configuration for a specific bus instance. Each bus has:
 * - Unique name for lookup
 * - Protocol type and protocol-specific settings
 * - Runtime state (initialized flag, driver handle)
 * - User context for callbacks
 * - Link to next bus in manager's linked list
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Type | Notes |
 * |--------|------|-------|------|-------|
 * | 0 | 4 | name | const char* | Pointer to name string |
 * | 4 | 1 | type | rx_bus_type_t | Bus protocol type |
 * | 5 | 1 | initialized | bool | Runtime state flag |
 * | 6 | 2 | (padding) | - | Alignment padding |
 * | 8 | 4 | handle | void* | Driver-specific handle |
 * | 12 | 4 | user_ctx | void* | User callback context |
 * | 16 | 24 | proto | union | Protocol-specific config |
 * | 40 | 4 | next | struct* | Linked list pointer |
 * | **Total** | **44** | - | - | Actual may vary with alignment |
 *
 * ## Bus Lifecycle State Machine
 *
 * @startuml
 * [*] --> Created : Allocate config
 * Created --> Registered : rx_bus_manager_add_bus()
 * Registered --> Initialized : First access
 * Initialized --> Initialized : Read/Write operations
 * Initialized --> Deinitialized : rx_bus_manager_remove_bus()
 * Registered --> Deinitialized : rx_bus_manager_remove_bus()
 * Deinitialized --> [*] : Release config reference (static, no free)
 * @enduml
 *
 * ## Field Constraints
 *
 * | Field | Constraint | Validation |
 * |-------|------------|------------|
 * | name | Non-NULL, <=31 chars | rx_bus_manager_add_bus() |
 * | type | 0 <= type < k_bus_type_max | rx_bus_manager_add_bus() |
 * | proto.* | Type-specific | Protocol validators |
 *
 * @par Usage Example - SPI Bus Configuration:
 * @code{.c}
 * // Statically allocate SPI bus config for RPi5 communication (zero dynamic allocation)
 * static rx_bus_config_t s_rpi5_spi = {
 *     .name = "rpi5_spi",
 *     .type = k_bus_type_spi,
 * };
 *
 * s_rpi5_spi.proto.spi = (rx_spi_bus_config_t){
 *     .channel = k_rspi_channel_0,
 *     .copi_pin = k_port_pin_p26,  // RSPI0 COPI
 *     .cipo_pin = k_port_pin_p30,  // RSPI0 CIPO
 *     .sck_pin = k_port_pin_p27,   // RSPI0 SCK
 *     .cs_pin = k_port_pin_p54,    // GPIO chip select
 *     .frequency_hz = k_rspi_freq_10mhz,
 *     .mode = k_rspi_mode_0        // CPOL=0, CPHA=0
 * };
 *
 * // Register with bus manager
 * rx_err_t err = rx_bus_manager_add_bus(&manager, &s_rpi5_spi);
 * @endcode
 *
 * @par Usage Example - I2C Bus Configuration:
 * @code{.c}
 * // Statically allocate I2C bus config for IMU sensor (zero dynamic allocation)
 * static rx_bus_config_t s_imu_i2c = {
 *     .name = "imu",
 *     .type = k_bus_type_i2c,
 * };
 *
 * s_imu_i2c.proto.i2c = (rx_i2c_bus_config_t){
 *     .channel = k_riic_channel_0,
 *     .sda_pin = k_port_pin_p16,
 *     .scl_pin = k_port_pin_p15,
 *     .frequency_hz = k_riic_freq_400khz,  // Fast mode
 *     .device_addr = k_mpu6050_i2c_addr     // MPU6050 7-bit address
 * };
 *
 * rx_bus_manager_add_bus(&manager, &s_imu_i2c);
 * @endcode
 *
 * @invariant name must be unique within bus manager
 * @invariant type must be valid (< k_bus_type_max)
 * @invariant proto union interpretation depends on type field
 *
 * @note Bus manager holds a reference to config; caller owns statically-allocated config for its entire lifetime
 * @warning Do not modify config after registration without mutex
 *
 * @see rx_bus_manager_add_bus() Register bus with manager
 * @see rx_bus_manager_find_bus() Lookup bus by name
 * @see rx_bus_type_t Bus type enumeration
 *
 * @since Version 1.0.0
 */
typedef struct rx_bus_config {
  /**
   * @brief Unique bus name for lookup
   * @details
   * Human-readable identifier for the bus instance.
   * Used by rx_bus_manager_find_bus() and logging.
   * @par Valid Range: Non-NULL, <=31 characters (k_max_bus_name_len)
   * @par Examples: "rpi5_spi", "imu", "temp_sensor", "debug_uart"
   * @invariant Must be unique within bus manager
   * @warning Must remain valid for lifetime of bus config
   */
  const char* name;

  /**
   * @brief Bus protocol type
   * @details
   * Determines which member of proto union to use and which
   * driver functions to invoke for operations.
   * @par Valid Range: 0 <= type < k_bus_type_max
   * @see rx_bus_type_t Protocol type enumeration
   */
  rx_bus_type_t type;

  /**
   * @brief Bus initialization status
   * @details
   * Runtime state tracking. Set true after successful hardware init.
   * - false: Config registered but hardware not initialized
   * - true: Hardware initialized and ready for operations
   * @par Default: false (set by memset during allocation)
   * @note Set by bus manager during first access, not by user
   */
  bool initialized;

  /**
   * @brief Driver-specific handle (opaque pointer)
   * @details
   * Holds driver state after initialization. Contents depend on bus type:
   * - GPIO: NULL (stateless)
   * - I2C: RIIC state structure
   * - SPI: RSPI state structure
   * - ADC: ADC unit state
   * @par Default: NULL (set by memset during allocation)
   * @note Managed by driver, do not modify directly
   */
  void* handle;

  /**
   * @brief User context for callbacks
   * @details
   * Opaque pointer passed to user callbacks during bus operations.
   * Allows user to associate application-specific data with bus.
   * @par Default: NULL (set by memset during allocation)
   * @note Bus manager does not dereference this pointer
   */
  void* user_ctx;

  /**
   * @brief Protocol-specific configuration union
   * @details
   * Contains configuration data specific to the bus protocol.
   * The active member is determined by the type field.
   *
   * | type Value | Active Member | Purpose |
   * |------------|---------------|---------|
   * | k_bus_type_gpio | proto.gpio | GPIO pin config |
   * | k_bus_type_adc | proto.adc | ADC channel config |
   * | k_bus_type_i2c | proto.i2c | I2C bus config |
   * | k_bus_type_spi | proto.spi | SPI bus config |
   * | k_bus_type_uart | proto.uart | UART config |
   * | k_bus_type_onewire | proto.onewire | 1-Wire config |
   *
   * @warning Reading wrong union member causes undefined behavior
   * @invariant Must interpret based on type field
   */
  union {
    rx_gpio_bus_config_t    gpio;    /**< GPIO pin configuration */
    rx_adc_bus_config_t     adc;     /**< ADC channel configuration */
    rx_i2c_bus_config_t     i2c;     /**< I2C bus configuration */
    rx_spi_bus_config_t     spi;     /**< SPI bus configuration */
    rx_uart_bus_config_t    uart;    /**< UART configuration */
    rx_onewire_bus_config_t onewire; /**< 1-Wire bus configuration */
  } proto;

  /**
   * @brief Next bus in linked list
   * @details
   * Points to next bus config in manager's internal list.
   * NULL if this is the last (or only) bus.
   * @par Default: NULL (set by memset during allocation)
   * @note Managed by bus manager, do not modify directly
   */
  struct rx_bus_config* next;
} rx_bus_config_t;

/* =============================================================================
 * Bus Manager Structure
 * =============================================================================
 */

/**
 * @enum bus_manager_limits_t
 * @brief Bus manager capacity and name limits
 *
 * @details
 * Defines compile-time limits for the bus manager to enable
 * static memory allocation and bounds checking.
 *
 * @par Rationale:
 * - k_max_buses: 32 buses sufficient for typical robot (4 motors x 2 buses + sensors)
 * - k_max_bus_name_len: 31 chars allows descriptive names like "motor_driver_front_left"
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Maximum number of buses that can be registered
   * @details
   * Hard limit to prevent unbounded memory allocation.
   * Exceeding returns k_rx_err_no_mem from rx_bus_manager_add_bus().
   * @par Derivation: RPi5 SPI + I2C + 4 motors (GPIO) + sensors + spare = 32
   */
  k_max_buses = 32,

  /**
   * @brief Maximum bus name length (excluding null terminator)
   * @details
   * Limits name storage requirements. Names are not copied by manager
   * (pointer to caller's string), but length is validated.
   * @par Examples: "motor_drv_fl" (12), "temp_sensor0" (12), "imu_mpu6050" (11)
   */
  k_max_bus_name_len = 31,
} bus_manager_limits_t;

/**
 * @struct rx_bus_manager_t
 * @brief Thread-safe bus manager for multi-protocol communication
 *
 * @details
 * Central registry maintaining a linked list of bus configurations with
 * ThreadX mutex protection for concurrent access from multiple threads.
 * Follows Dependency Inversion Principle with injected interfaces.
 *
 * ## Architecture Overview
 *
 * @dot
 * digraph manager_structure {
 *   rankdir=TB;
 *   node [shape=record];
 *
 *   manager [label="rx_bus_manager_t|{buses: rx_bus_config_t*|mutex: TX_MUTEX|tag: const char*|error_iface: rx_error_interface_t*|pin_iface: rx_pin_interface_t*|riic_initialized[3]|riic_freq_hz[3]|rspi_initialized[3]|adc_unit_initialized[2]|bus_count: uint8_t}"];
 *
 *   bus1 [label="rx_bus_config_t|name=\"rpi5_spi\"|type=k_bus_type_spi|next"];
 *   bus2 [label="rx_bus_config_t|name=\"imu_i2c\"|type=k_bus_type_i2c|next"];
 *   bus3 [label="rx_bus_config_t|name=\"debug_uart\"|type=k_bus_type_uart|next=NULL"];
 *
 *   manager -> bus1 [label="buses"];
 *   bus1 -> bus2 [label="next"];
 *   bus2 -> bus3 [label="next"];
 * }
 * @enddot
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Notes |
 * |--------|------|-------|-------|
 * | 0 | 4 | buses | Linked list head |
 * | 4 | ~80 | mutex | TX_MUTEX (ThreadX) |
 * | ~84 | 4 | tag | Logging tag pointer |
 * | ~88 | 4 | error_iface | DIP error handler |
 * | ~92 | 4 | pin_iface | DIP pin validator |
 * | ~96 | 3 | riic_initialized | RIIC tracking |
 * | ~99 | 12 | riic_freq_hz | RIIC per-channel freq |
 * | ~111 | 3 | rspi_initialized | RSPI tracking |
 * | ~114 | 2 | adc_unit_initialized | ADC tracking |
 * | ~116 | 1 | bus_count | Registration count |
 * | **Total** | **~140** | - | Platform-dependent |
 *
 * ## Thread Safety Model
 *
 * All public API functions acquire mutex before accessing internal state:
 *
 * @msc
 * msc {
 *   width=500;
 *   Thread1, Mutex, Manager, Thread2;
 *
 *   Thread1 => Mutex [label="tx_mutex_get()"];
 *   Mutex >> Thread1 [label="TX_SUCCESS"];
 *   Thread1 => Manager [label="Access buses"];
 *
 *   Thread2 => Mutex [label="tx_mutex_get()"];
 *   Mutex note Mutex [label="Thread2 blocks"];
 *
 *   Thread1 => Mutex [label="tx_mutex_put()"];
 *   Mutex >> Thread2 [label="TX_SUCCESS"];
 *   Thread2 => Manager [label="Access buses"];
 * }
 * @endmsc
 *
 * ## Resource Tracking
 *
 * Manager tracks which hardware peripherals are initialized to:
 * - Prevent double-initialization
 * - Enable proper cleanup
 * - Support hardware resource sharing
 *
 * | Array | Size | Purpose |
 * |-------|------|---------|
 * | riic_initialized | 3 | RIIC channels 0-2 |
 * | riic_freq_hz | 3 | Frequency per RIIC channel |
 * | rspi_initialized | 3 | RSPI channels 0-2 |
 * | adc_unit_initialized | 2 | ADC units 0-1 |
 *
 * ## Dependency Injection (SOLID Principle D)
 *
 * Interfaces can be NULL (use defaults) or injected for testing:
 *
 * @code{.c}
 * // Production: Use real interfaces
 * rx_bus_manager_init(&manager, "MAIN", &real_error_iface, &real_pin_iface);
 *
 * // Testing: Use mock interfaces
 * rx_bus_manager_init(&manager, "TEST", &mock_error_iface, &mock_pin_iface);
 * @endcode
 *
 * @par Usage Example - Basic Initialization:
 * @code{.c}
 * rx_bus_manager_t manager;
 * memset(&manager, 0, sizeof(manager));
 *
 * rx_err_t err = rx_bus_manager_init(&manager, "MOTOR", nullptr, nullptr);
 * if (err != k_rx_ok) {
 *     rx_log_error("MAIN", "Bus manager init failed: %d", err);
 *     return err;
 * }
 *
 * // Register buses...
 * rx_bus_manager_add_bus(&manager, rpi5_spi_config);
 * rx_bus_manager_add_bus(&manager, imu_i2c_config);
 *
 * // Access buses...
 * rx_bus_manager_with_bus(&manager, "rpi5_spi", spi_callback, &ctx);
 *
 * // Cleanup
 * rx_bus_manager_deinit(&manager);
 * @endcode
 *
 * @invariant bus_count <= k_max_buses
 * @invariant mutex valid after rx_bus_manager_init()
 * @invariant buses list consistent (no cycles, proper NULL termination)
 *
 * @note Initialize with rx_bus_manager_init() before use
 * @note Cleanup with rx_bus_manager_deinit() when done
 * @warning Not safe to use before initialization
 * @warning Not safe to use after deinitialization
 *
 * @see rx_bus_manager_init() Initialize manager
 * @see rx_bus_manager_deinit() Cleanup manager
 * @see rx_bus_manager_add_bus() Register bus
 * @see rx_bus_manager_with_bus() Thread-safe bus access
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Head of linked list of bus configurations
   * @details
   * Points to first registered bus, or nullptr if no buses registered.
   * List traversal protected by mutex.
   * @par Default: NULL (no buses)
   * @invariant NULL only when bus_count == 0
   */
  rx_bus_config_t* buses;

  /**
   * @brief ThreadX mutex for thread-safe access
   * @details
   * All public API functions acquire this mutex before accessing
   * internal state. Timeout is k_bus_manager_mutex_timeout_ms.
   * @par Initialization: Created by rx_bus_manager_init()
   * @par Cleanup: Deleted by rx_bus_manager_deinit()
   * @warning Do not access directly, use API functions
   */
  TX_MUTEX mutex;

  /**
   * @brief Logging tag for debug messages
   * @details
   * Identifies log messages from this manager instance.
   * Passed to rx_log_*() functions.
   * @par Examples: "MOTOR", "SENSOR", "MAIN"
   * @par Valid Range: Non-NULL, typically <=8 characters
   */
  const char* tag;

  /**
   * @brief Injected error handler interface (DIP)
   * @details
   * Custom error handler for bus operation failures.
   * Can be NULL to use default error handling.
   * @par Usage: Called on bus operation errors
   * @par Testing: Inject mock for unit tests
   * @see rx_error_interface_t Error handler interface
   */
  rx_error_interface_t* error_iface;

  /**
   * @brief Injected pin validator interface (DIP)
   * @details
   * Custom pin validator for verifying GPIO pin configurations.
   * Can be NULL to skip pin validation.
   * @par Usage: Called during bus configuration validation
   * @par Testing: Inject mock for unit tests
   * @see rx_pin_interface_t Pin validator interface
   */
  rx_pin_interface_t* pin_iface;

  /* Resource tracking arrays - prevent double-init of shared hardware */

  /**
   * @brief RIIC channel initialization status
   * @details
   * Tracks which RIIC channels have been initialized.
   * Prevents multiple buses from reinitializing same channel.
   * @par Index: Channel number (0, 1, 2)
   * @par Value: true = initialized, false = not initialized
   */
  bool riic_initialized[k_riic_channel_count];

  /**
   * @brief RIIC channel configured frequency in Hz
   * @details
   * Stores the frequency_hz used when each RIIC channel was first initialized.
   * Used by subsequent rx_bus_i2c_init() calls on the same physical channel to
   * verify that shared buses request the same frequency as the already-active channel.
   * @par Index: Channel number (0, 1, 2)
   * @par Value: frequency_hz from the first successful riic_init() call; 0 if not initialized
   */
  uint32_t riic_freq_hz[k_riic_channel_count];

  /**
   * @brief RSPI channel initialization status
   * @details
   * Tracks which RSPI channels have been initialized.
   * Multiple SPI buses can share a channel with different CS pins.
   * @par Index: Channel number (0, 1, 2)
   * @par Value: true = initialized, false = not initialized
   */
  bool rspi_initialized[k_rspi_channel_count];

  /**
   * @brief ADC unit initialization status
   * @details
   * Tracks which ADC units have been initialized.
   * Multiple ADC buses can share a unit with different channels.
   * @par Index: Unit number (0, 1)
   * @par Value: true = initialized, false = not initialized
   */
  bool adc_unit_initialized[k_adc_unit_count];

  /**
   * @brief Number of buses currently registered
   * @details
   * Count of buses in linked list. Used for limit checking.
   * @par Valid Range: [0, k_max_buses]
   * @invariant Equals length of buses linked list
   */
  uint8_t bus_count;
} rx_bus_manager_t;

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Bus manager configuration constants
 */
/**
 * @brief Default mutex timeout for bus operations
 */
typedef enum : uint16_t {
  k_bus_manager_mutex_timeout_ms = 1000U, /**< Default mutex timeout in milliseconds */
} bus_manager_timeout_t;

#ifdef __cplusplus
}
#endif
