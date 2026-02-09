/* lib/rx_bus/inc/rx_bus_onewire.h */

/**
 * @file rx_bus_onewire.h
 * @brief OneWire (1-Wire) Bus Abstraction for RX72N
 *
 * @details
 * Provides bus manager integration for Dallas/Maxim OneWire (1-Wire) protocol
 * operations. Implements bit-banging protocol using GPIO with precise
 * microsecond-level timing control for reliable device communication.
 *
 * @par System Architecture
 * @dot
 * digraph onewire_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="OneWire API (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     init [label="rx_bus_onewire_init()"];
 *     reset [label="rx_bus_onewire_reset()"];
 *     read_write [label="rx_bus_onewire_read/write()"];
 *     rom_cmds [label="skip_rom() / match_rom()\nread_rom() / search()"];
 *   }
 *
 *   subgraph cluster_bus {
 *     label="Bus Manager";
 *     style=filled;
 *     color=lightblue;
 *     manager [label="rx_bus_manager_t\nMutex protection"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="GPIO HAL";
 *     style=filled;
 *     color=lightgreen;
 *     gpio [label="GPIO Port\nOpen-drain mode"];
 *     timing [label="Precise µs timing\nCMT-based delays"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     bus_line [label="1-Wire Bus\n4.7kΩ pullup"];
 *     ds18b20 [label="DS18B20\nTemp Sensor"];
 *     other [label="Other 1-Wire\nDevices"];
 *   }
 *
 *   init -> manager;
 *   reset -> manager;
 *   read_write -> manager;
 *   rom_cmds -> manager;
 *   manager -> gpio;
 *   gpio -> timing;
 *   timing -> bus_line;
 *   bus_line -> ds18b20;
 *   bus_line -> other;
 * }
 * @enddot
 *
 * @par Protocol Overview
 * OneWire is a bidirectional single-wire protocol using open-drain GPIO:
 * - **Single wire** for data + power (parasitic) or data only
 * - **Controller-initiated** all transactions start with reset/presence
 * - **LSB first** data transmission order
 * - **64-bit ROM** unique device address with family code and CRC
 *
 * @par Timing Diagram
 * @msc
 * Controller, Bus, Device;
 *
 * --- [label="Reset/Presence"];
 * Controller -> Bus [label="Drive LOW 480µs"];
 * Controller -> Bus [label="Release"];
 * Bus -> Device [label="Presence pulse (60-240µs)"];
 *
 * --- [label="Write 1"];
 * Controller -> Bus [label="Drive LOW 10µs"];
 * Controller -> Bus [label="Release (55µs high)"];
 *
 * --- [label="Write 0"];
 * Controller -> Bus [label="Drive LOW 60µs"];
 * Controller -> Bus [label="Release (10µs recovery)"];
 *
 * --- [label="Read Bit"];
 * Controller -> Bus [label="Drive LOW 6µs"];
 * Controller -> Bus [label="Release"];
 * Device -> Controller [label="Sample at 9µs from slot start"];
 * @endmsc
 *
 * @par Supported Devices
 * | Device | Family Code | Description |
 * |--------|-------------|-------------|
 * | DS18B20 | 0x28 | Digital temperature sensor |
 * | DS18S20 | 0x10 | Temperature sensor (older) |
 * | DS2401 | 0x01 | Silicon serial number |
 * | DS2411 | 0x01 | Serial number with 1k EEPROM |
 *
 * @par Timing Parameters
 * | Operation | Parameter | Value | Tolerance |
 * |-----------|-----------|-------|-----------|
 * | Reset | Drive LOW | 480 µs | min |
 * | Reset | Presence sample | 70 µs | 60-75 µs |
 * | Write 1 | LOW pulse | 10 µs | 6-15 µs |
 * | Write 0 | LOW pulse | 60 µs | min |
 * | Read | Init LOW | 6 µs | 1-15 µs |
 * | Read | Sample | 9 µs | within 15 µs |
 * | Slot | Duration | 65 µs | 60 µs min |
 *
 * @par Performance Characteristics
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | Bit rate | ~15.4 kbit/s | 65 µs per bit |
 * | Byte time | ~520 µs | 8 bits + overhead |
 * | Reset cycle | ~960 µs | 480 + 480 µs |
 * | ROM read | ~5 ms | Reset + 9 bytes |
 * | Search (N devices) | ~5*N ms | Per device |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~2 KB | All API functions |
 * | ROM buffer | 8 bytes | Per device address |
 * | Search state | ~20 bytes | Search algorithm state |
 *
 * @par Hardware Requirements
 * | Requirement | Specification |
 * |-------------|---------------|
 * | GPIO | Open-drain capable or external FET |
 * | Pullup | 4.7 kΩ to VDD (external) |
 * | VDD | 3.0V - 5.5V |
 * | Timing | CMT timer for µs delays |
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - GPIO HAL: Port configuration and control
 * - CMT: Microsecond timing delays
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: ✓ No goto, setjmp, or recursion
 * - Rule 2: ✓ All loops bounded (8 bits, ROM bytes, max devices)
 * - Rule 3: ✓ No dynamic memory allocation
 * - Rule 4: ✓ Functions under 60 lines
 * - Rule 5: ✓ Input validation with RX_CHECK_NULL_PTR
 * - Rule 6: ✓ Variables at smallest scope
 * - Rule 7: ✓ All return values checked
 * - Rule 8: ✓ C23 typed enums for constants
 * - Rule 10: ✓ Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** OneWire protocol only, no device-specific logic
 * - **O (OCP):** Extensible to new devices via ROM commands
 * - **L (LSP):** Consistent error semantics across all functions
 * - **I (ISP):** Focused API (bit, byte, ROM operations)
 * - **D (DIP):** Depends on bus_manager abstraction, not direct GPIO
 *
 * @par Thread Safety
 * All functions are **thread-safe** via bus manager mutex:
 * - Mutex acquired at function entry
 * - Released on function exit (success or error)
 * - Timeout on mutex acquisition returns k_rx_err_timeout
 *
 * @see rx_ds18b20.h DS18B20 temperature sensor driver
 * @see rx_bus_manager.h Bus abstraction layer
 * @see rx_crc8.h CRC-8 for ROM validation
 * @see docs/sections/03_hardware_pinout.tex Pin assignments
 *
 * @author STAR Team
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
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
 * @enum onewire_timing_us_t
 * @brief OneWire protocol timing parameters in microseconds
 *
 * @details
 * Precise timing values based on Dallas/Maxim OneWire specification.
 * All values in microseconds (µs). These timings are critical for
 * reliable communication - timing violations cause bus errors.
 *
 * @par Timing Diagram Reference
 * ```
 * Reset/Presence:
 *   ___      ________
 *      |____|        |_______  <-- presence pulse from device
 *      |<480>|<70>|<240 max>|
 *
 * Write 1:
 *   ___    __________
 *      |__|
 *      |<10>|<55>|
 *
 * Write 0:
 *   ___           ___
 *      |_________|
 *      |<--60-->|<10>|
 *
 * Read:
 *   ___  ____________
 *      |_|    ^ sample
 *      |<6>|<9>|<55>|
 * ```
 *
 * @par Timing Tolerance
 * - Reset pulse: 480 µs minimum
 * - Write 1 LOW: 6-15 µs (centered at 10 µs)
 * - Write 0 LOW: 60 µs minimum
 * - Read sample: within 15 µs of slot start
 * - Slot duration: 60 µs minimum + 1 µs recovery
 *
 * @invariant Slot timing must be >= 60 µs for reliable communication
 * @invariant Sample timing must be within 15 µs window
 *
 * @see Dallas/Maxim Application Note AN126 for timing details
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Reset and presence detection timing */
  k_onewire_reset_pulse_us = 480, /**< Reset pulse duration.
                                       Controller drives bus LOW for this duration.
                                       @par Value: 480 µs (minimum per spec)
                                       @note Longer pulses (up to 960 µs) are allowed */

  k_onewire_presence_wait_us = 70, /**< Wait time before sampling presence.
                                        After releasing reset, wait before sampling.
                                        @par Value: 70 µs (within 60-75 µs window)
                                        @note Device pulls low 15-60 µs after reset release */

  k_onewire_presence_timeout_us = 240, /**< Maximum presence pulse duration.
                                            Presence pulse ends within this time.
                                            @par Value: 240 µs (maximum per spec) */

  /* Write bit timing */
  k_onewire_write_1_low_us = 10, /**< Write-1 LOW pulse duration.
                                      Short pulse for logic 1.
                                      @par Value: 10 µs (within 6-15 µs window) */

  k_onewire_write_1_high_us = 55, /**< Write-1 recovery time.
                                       Bus HIGH after LOW pulse.
                                       @par Value: 55 µs (slot = 65 µs total) */

  k_onewire_write_0_low_us = 60, /**< Write-0 LOW pulse duration.
                                      Long pulse for logic 0.
                                      @par Value: 60 µs (minimum per spec) */

  k_onewire_write_0_high_us = 10, /**< Write-0 recovery time.
                                       Bus HIGH after LOW pulse.
                                       @par Value: 10 µs (short recovery) */

  /* Read bit timing */
  k_onewire_read_init_us = 6, /**< Read initiation LOW pulse.
                                   Controller pulls LOW to start read slot.
                                   @par Value: 6 µs (within 1-15 µs window) */

  k_onewire_read_sample_us = 9, /**< Read sample time from slot start.
                                     Sample bus state at this time.
                                     @par Value: 9 µs (within 15 µs window)
                                     @note Must sample before device releases bus */

  k_onewire_read_recovery_us = 55, /**< Read slot recovery time.
                                        Wait for slot completion.
                                        @par Value: 55 µs */

  /* Slot timing */
  k_onewire_slot_duration_us = 65, /**< Total time slot duration.
                                        Minimum slot time + recovery.
                                        @par Value: 65 µs (60 µs min + 5 µs margin) */

  k_onewire_recovery_us = 1, /**< Inter-slot recovery time.
                                  Minimum time between slots.
                                  @par Value: 1 µs (minimum per spec) */
} onewire_timing_us_t;

/**
 * @enum onewire_rom_command_t
 * @brief OneWire ROM command codes per Dallas/Maxim specification
 *
 * @details
 * Standard ROM commands used for device addressing on the OneWire bus.
 * All transactions start with reset, followed by a ROM command to select
 * which device(s) will respond.
 *
 * @par ROM Command Flow
 * ```
 * Reset → ROM Command → [ROM Address] → Function Command → Data
 * ```
 *
 * @par Command Usage
 * | Command | When to Use |
 * |---------|-------------|
 * | SEARCH_ROM | Enumerate all devices |
 * | READ_ROM | Single device on bus |
 * | MATCH_ROM | Multiple devices, address one |
 * | SKIP_ROM | Single device or broadcast |
 * | ALARM_SEARCH | Find devices with alerts |
 *
 * @see rx_bus_onewire_skip_rom() Skip ROM implementation
 * @see rx_bus_onewire_match_rom() Match ROM implementation
 * @see rx_bus_onewire_search() Search ROM implementation
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_onewire_cmd_search_rom = 0xF0, /**< Search ROM command.
                                        Enumerate all devices using binary search.
                                        @par Value: 0xF0
                                        @see rx_bus_onewire_search() */

  k_onewire_cmd_read_rom = 0x33, /**< Read ROM command.
                                      Read 64-bit ROM from single device.
                                      @par Value: 0x33
                                      @warning Only use with single device on bus
                                      @see rx_bus_onewire_read_rom() */

  k_onewire_cmd_match_rom = 0x55, /**< Match ROM command.
                                       Address specific device by ROM code.
                                       @par Value: 0x55
                                       @note Followed by 8-byte ROM address
                                       @see rx_bus_onewire_match_rom() */

  k_onewire_cmd_skip_rom = 0xCC, /**< Skip ROM command.
                                      Address all devices (broadcast).
                                      @par Value: 0xCC
                                      @note Use with single device or broadcast
                                      @see rx_bus_onewire_skip_rom() */

  k_onewire_cmd_alarm_search = 0xEC, /**< Alarm search command.
                                          Find devices with alarm condition.
                                          @par Value: 0xEC
                                          @note Device-specific alarm criteria */
} onewire_rom_command_t;

/**
 * @enum onewire_rom_size_t
 * @brief OneWire ROM code size constant
 *
 * @details
 * ROM code structure (8 bytes / 64 bits):
 * - Byte 0: Family code (device type)
 * - Bytes 1-6: Serial number (48 bits, unique)
 * - Byte 7: CRC-8 of bytes 0-6
 *
 * @par ROM Structure
 * | Byte | Content | Description |
 * |------|---------|-------------|
 * | 0 | Family | Device type (0x28 = DS18B20) |
 * | 1-6 | Serial | 48-bit unique ID |
 * | 7 | CRC | CRC-8 validation |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_onewire_rom_bytes = 8, /**< ROM code size in bytes.
                                64-bit address = 8 bytes.
                                @par Value: 8 */
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
 * @return k_rx_err_null_ptr if manager or bus_name is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not OneWire type
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name);

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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence);

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
 * @return k_rx_err_null_ptr if manager or bus_name is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit);

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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit);

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
 * @return k_rx_err_null_ptr if manager or bus_name is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte);

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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte);

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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_read(rx_bus_manager_t* manager,
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
 * @return k_rx_err_null_ptr if manager or bus_name is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name);

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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
[[nodiscard]] rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 * @return k_rx_err_hw_error if multiple devices present (ROM collision)
 */
[[nodiscard]] rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
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
 * @return k_rx_err_null_ptr if any pointer parameter is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_invalid_size if more devices found than max_devices
 * @return k_rx_err_timeout if mutex timeout
 * @return k_rx_err_crc if ROM CRC check fails
 */
[[nodiscard]] rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               uint32_t          max_devices,
                               uint32_t*         num_devices);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_ONEWIRE_H */
