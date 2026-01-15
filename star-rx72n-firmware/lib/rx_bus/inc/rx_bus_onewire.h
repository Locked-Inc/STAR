/* lib/rx_bus/inc/rx_bus_onewire.h */

/**
 * @file rx_bus_onewire.h
 * @brief OneWire (1-Wire) bus abstraction for RX72N
 * @details
 * Provides bus manager integration for OneWire (1-Wire) protocol operations.
 * Implements bit-banging protocol using GPIO with precise timing control.
 *
 * OneWire is a bidirectional single-wire protocol commonly used for:
 * - Temperature sensors (DS18B20, DS18S20)
 * - iButton authentication chips
 * - Serial number chips (DS2401, DS2411)
 *
 * The protocol requires precise microsecond-level timing:
 * - Reset pulse: 480us low, sample after 70us
 * - Write 1: 10us low, 55us high
 * - Write 0: 60us low, 5us high
 * - Read: 6us low, sample at 9us, wait 55us
 *
 * Implementation uses GPIO bit-banging with timing control.
 *
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BUS_ONEWIRE_H
#define STAR_RX72N_BUS_ONEWIRE_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * OneWire Timing Constants
 * =============================================================================
 */

/**
 * @brief OneWire timing parameters (microseconds)
 *
 * These values are based on the Dallas/Maxim OneWire specification.
 * All timing is in microseconds.
 */
typedef enum {
  /* Reset and presence detection timing */
  k_onewire_reset_pulse_us      = 480, /**< Reset pulse duration (480us min) */
  k_onewire_presence_wait_us    = 70,  /**< Wait before sampling presence (60-75us) */
  k_onewire_presence_timeout_us = 240, /**< Presence detection timeout (240us) */

  /* Write bit timing */
  k_onewire_write_1_low_us  = 10, /**< Write 1: low pulse (6-15us) */
  k_onewire_write_1_high_us = 55, /**< Write 1: recovery time (remainder of 60us slot) */
  k_onewire_write_0_low_us  = 60, /**< Write 0: low pulse (60us min) */
  k_onewire_write_0_high_us = 10, /**< Write 0: recovery time */

  /* Read bit timing */
  k_onewire_read_init_us     = 6,  /**< Read: initial low pulse (>1us, <15us) */
  k_onewire_read_sample_us   = 9,  /**< Read: sample time from start (within 15us of slot start) */
  k_onewire_read_recovery_us = 55, /**< Read: recovery time (remainder of 60us slot) */

  /* Slot timing */
  k_onewire_slot_duration_us = 65, /**< Total time slot duration (60us + recovery) */
  k_onewire_recovery_us      = 1,  /**< Inter-slot recovery time (>1us) */
} onewire_timing_us_t;

/**
 * @brief OneWire ROM command codes
 *
 * Standard ROM commands for device addressing.
 */
typedef enum {
  k_onewire_cmd_search_rom   = 0xF0, /**< Search ROM - enumerate all devices */
  k_onewire_cmd_read_rom     = 0x33, /**< Read ROM - read single device address */
  k_onewire_cmd_match_rom    = 0x55, /**< Match ROM - address specific device */
  k_onewire_cmd_skip_rom     = 0xCC, /**< Skip ROM - address all devices */
  k_onewire_cmd_alarm_search = 0xEC, /**< Alarm search - find devices with alarm condition */
} onewire_rom_command_t;

/**
 * @brief OneWire device ROM size
 */
typedef enum {
  k_onewire_rom_bytes = 8, /**< ROM code is 8 bytes (64 bits) */
} onewire_rom_size_t;

/* =============================================================================
 * OneWire Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize OneWire bus through bus manager
 *
 * Configures GPIO pin for OneWire communication (open-drain mode).
 * The pin should have an external 4.7k pullup resistor.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not OneWire type
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Perform OneWire reset and check for presence pulse
 *
 * Sends reset pulse (480us low) and samples for presence pulse from device.
 * This is the first step in any OneWire transaction.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] presence Presence detected (true if device present)
 *
 * @return k_rx_ok on success (check presence output for device detection)
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence);

/**
 * @brief Write a single bit to OneWire bus
 *
 * Timing:
 * - Write 1: 10us low, 55us high
 * - Write 0: 60us low, 10us high
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] bit Bit value to write (true = 1, false = 0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit);

/**
 * @brief Read a single bit from OneWire bus
 *
 * Timing:
 * - 6us low pulse to initiate read
 * - Sample at 9us from start
 * - 55us recovery time
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] bit Bit value read (true = 1, false = 0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit);

/**
 * @brief Write a byte to OneWire bus
 *
 * Writes 8 bits LSB first.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] byte Byte value to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte);

/**
 * @brief Read a byte from OneWire bus
 *
 * Reads 8 bits LSB first.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] byte Byte value read
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte);

/**
 * @brief Write multiple bytes to OneWire bus
 *
 * Convenience function for writing buffers.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              uint32_t          length);

/**
 * @brief Read multiple bytes from OneWire bus
 *
 * Convenience function for reading buffers.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] data Pointer to receive buffer
 * @param[in] length Number of bytes to read
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_read(rx_bus_manager_t* manager,
                             const char*       bus_name,
                             uint8_t*          data,
                             uint32_t          length);

/**
 * @brief Send Skip ROM command
 *
 * Addresses all devices on the bus (broadcast).
 * Use when only one device is present or for global commands.
 *
 * Sequence:
 * 1. Reset
 * 2. Write SKIP_ROM command (0xCC)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Send Match ROM command with specific device address
 *
 * Addresses a specific device by its 64-bit ROM code.
 *
 * Sequence:
 * 1. Reset
 * 2. Write MATCH_ROM command (0x55)
 * 3. Write 8-byte ROM code
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] rom 8-byte ROM code of target device
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
                                  const char*       bus_name,
                                  const uint8_t     rom[k_onewire_rom_bytes]);

/**
 * @brief Read ROM code from single device
 *
 * Reads the 64-bit ROM code from the only device on the bus.
 * Only works when exactly one device is connected.
 *
 * Sequence:
 * 1. Reset
 * 2. Write READ_ROM command (0x33)
 * 3. Read 8 bytes (ROM code)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] rom 8-byte buffer for ROM code
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 * @return k_rx_err_hw_error if multiple devices present (ROM collision)
 */
rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint8_t           rom[k_onewire_rom_bytes]);

/**
 * @brief Search for all devices on the OneWire bus
 *
 * Implements the OneWire search algorithm to enumerate all devices.
 * The search algorithm uses binary tree traversal to discover all
 * unique 64-bit ROM codes on the bus.
 *
 * Sequence:
 * 1. Reset
 * 2. Write SEARCH_ROM command (0xF0)
 * 3. For each bit position (64 bits):
 *    - Read bit
 *    - Read complement
 *    - Write search direction
 * 4. Repeat until all devices found
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] roms Array of ROM codes (8 bytes each)
 * @param[in] max_devices Maximum number of devices to search for
 * @param[out] num_devices Number of devices found
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_invalid_size if more devices found than max_devices
 * @return k_rx_err_timeout if mutex timeout
 * @return k_rx_err_crc if ROM CRC check fails
 */
rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               uint32_t          max_devices,
                               uint32_t*         num_devices);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_ONEWIRE_H */
