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
 *     timing [label="Precise us timing\nCMT-based delays"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     bus_line [label="1-Wire Bus\n4.7kOhm pullup"];
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
 * Controller -> Bus [label="Drive LOW 480us"];
 * Controller -> Bus [label="Release"];
 * Bus -> Device [label="Presence pulse (60-240us)"];
 *
 * --- [label="Write 1"];
 * Controller -> Bus [label="Drive LOW 10us"];
 * Controller -> Bus [label="Release (55us high)"];
 *
 * --- [label="Write 0"];
 * Controller -> Bus [label="Drive LOW 60us"];
 * Controller -> Bus [label="Release (10us recovery)"];
 *
 * --- [label="Read Bit"];
 * Controller -> Bus [label="Drive LOW 6us"];
 * Controller -> Bus [label="Release"];
 * Device -> Controller [label="Sample at 9us from slot start"];
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
 * | Reset | Drive LOW | 480 us | min |
 * | Reset | Presence sample | 70 us | 60-75 us |
 * | Write 1 | LOW pulse | 10 us | 6-15 us |
 * | Write 0 | LOW pulse | 60 us | min |
 * | Read | Init LOW | 6 us | 1-15 us |
 * | Read | Sample | 9 us | within 15 us |
 * | Slot | Duration | 65 us | 60 us min |
 *
 * @par Performance Characteristics
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | Bit rate | ~15.4 kbit/s | 65 us per bit |
 * | Byte time | ~520 us | 8 bits + overhead |
 * | Reset cycle | ~960 us | 480 + 480 us |
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
 * | Pullup | 4.7 kOhm to VDD (external) |
 * | VDD | 3.0V - 5.5V |
 * | Timing | CMT timer for us delays |
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - GPIO HAL: Port configuration and control
 * - CMT: Microsecond timing delays
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded (8 bits, ROM bytes, max devices)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Functions under 60 lines
 * - Rule 5: [OK] Input validation with RX_CHECK_NULL_PTR
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] C23 typed enums for constants
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
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
 * @author Locked, Inc.
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

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
 * All values in microseconds (us). These timings are critical for
 * reliable communication - timing violations cause bus errors.
 *
 * @par Timing Diagram Reference
 * ```
 * Reset/Presence (960 us total slot):
 *   ___      ______________________________
 *      |____|        |_______             |
 *      |<480>|<70>|<--- 410 remaining --->|
 *                    Device presence pulse  ^
 *                    lasts 60-240 us       Slot complete
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
 * - Reset pulse: 480 us minimum
 * - Write 1 LOW: 6-15 us (centered at 10 us)
 * - Write 0 LOW: 60 us minimum
 * - Read sample: within 15 us of slot start
 * - Slot duration: 60 us minimum + 1 us recovery
 *
 * @invariant Slot timing must be >= 60 us for reliable communication
 * @invariant Sample timing must be within 15 us window
 *
 * @see Dallas/Maxim Application Note AN126 for timing details
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Reset and presence detection timing */
  k_onewire_reset_pulse_us = 480, /**< Reset pulse duration.
                                       Controller drives bus LOW for this duration.
                                       @par Value: 480 us (minimum per spec)
                                       @note Longer pulses (up to 960 us) are allowed */

  k_onewire_presence_wait_us = 70, /**< Wait time before sampling presence.
                                        After releasing reset, wait before sampling.
                                        @par Value: 70 us (within 60-75 us window)
                                        @note Device pulls low 15-60 us after reset release */

  k_onewire_presence_tail_us = 410, /**< Remaining presence window after sampling.
                                         Wait after sampling to complete the full 960 us reset slot.
                                         @par Value: 410 us (= 480 us recovery - 70 us pre-sample wait)
                                         @note Total reset slot: 480 + 70 + 410 = 960 us (per spec)
                                         @note Device presence pulse is 60-240 us; this wait covers
                                               the remaining recovery window regardless of pulse width */

  k_onewire_reset_slot_total_us = 960, /**< Total reset slot duration in microseconds.
                                            Sum of reset pulse, presence wait, and presence tail:
                                            k_onewire_reset_pulse_us + k_onewire_presence_wait_us +
                                            k_onewire_presence_tail_us = 480 + 70 + 410 = 960 us.
                                            @par Value: 960 us (per Dallas/Maxim 1-Wire spec) */

  /* Write bit timing */
  k_onewire_write_1_low_us = 10, /**< Write-1 LOW pulse duration.
                                      Short pulse for logic 1.
                                      @par Value: 10 us (within 6-15 us window) */

  k_onewire_write_1_high_us = 55, /**< Write-1 recovery time.
                                       Bus HIGH after LOW pulse.
                                       @par Value: 55 us (slot = 65 us total) */

  k_onewire_write_0_low_us = 60, /**< Write-0 LOW pulse duration.
                                      Long pulse for logic 0.
                                      @par Value: 60 us (minimum per spec) */

  k_onewire_write_0_high_us = 10, /**< Write-0 recovery time.
                                       Bus HIGH after LOW pulse.
                                       @par Value: 10 us (short recovery) */

  /* Read bit timing */
  k_onewire_read_init_us = 6, /**< Read initiation LOW pulse.
                                   Controller pulls LOW to start read slot.
                                   @par Value: 6 us (within 1-15 us window) */

  k_onewire_read_sample_us = 9, /**< Read sample time from slot start.
                                     Sample bus state at this time.
                                     @par Value: 9 us (within 15 us window)
                                     @note Must sample before device releases bus */

  k_onewire_read_recovery_us = 55, /**< Read slot recovery time.
                                        Wait for slot completion.
                                        @par Value: 55 us */

  /* Slot timing */
  k_onewire_slot_duration_us = 65, /**< Total time slot duration.
                                        Minimum slot time + recovery.
                                        @par Value: 65 us (60 us min + 5 us margin) */

  k_onewire_recovery_us = 1, /**< Inter-slot recovery time.
                                  Minimum time between slots.
                                  @par Value: 1 us (minimum per spec) */
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
 * Reset -> ROM Command -> [ROM Address] -> Function Command -> Data
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
 * @brief Deinitialize OneWire bus and release state pool entry
 *
 * Releases the runtime state allocated by rx_bus_onewire_init() back to the
 * static pool. Must be called before destroying the bus manager to prevent
 * state pool exhaustion.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is nullptr
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not OneWire type
 * @return k_rx_err_timeout if mutex timeout
 *
 * @pre Bus was initialized via rx_bus_onewire_init()
 * @post State pool entry released for reuse
 * @post bus_config->initialized set to false
 *
 * @see rx_bus_onewire_init() Initialization counterpart
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bus_onewire_deinit(rx_bus_manager_t* manager, const char* bus_name);

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
[[nodiscard]] rx_err_t
rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence);

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
[[nodiscard]] rx_err_t
rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit);

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
[[nodiscard]] rx_err_t
rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit);

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
[[nodiscard]] rx_err_t
rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte);

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
[[nodiscard]] rx_err_t
rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte);

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

#ifdef UNIT_TEST

/**
 * @defgroup onewire_unit_test_internals OneWire Internal API (Unit Test Only)
 * @brief Internal types and functions exposed for unit testing via RX_STATIC_TESTABLE.
 * @{
 */

/* Internal context structs for callback functions (exposed for direct test invocation) */

/** @brief Simple callback context with only a result field. */
typedef struct {
  rx_err_t result; /**< Operation result code */
} onewire_simple_ctx_t;

/** @brief Context for reset callback (presence pointer + result). */
typedef struct {
  bool*    presence; /**< Output: true if device presence detected */
  rx_err_t result;   /**< Operation result code */
} onewire_reset_ctx_t;

/** @brief Context for write_bit callback. */
typedef struct {
  bool     bit;    /**< Bit value to write */
  rx_err_t result; /**< Operation result code */
} onewire_write_bit_ctx_t;

/** @brief Context for read_bit callback. */
typedef struct {
  bool*    bit;    /**< Output bit pointer */
  rx_err_t result; /**< Operation result code */
} onewire_read_bit_ctx_t;

/** @brief Context for write_byte callback. */
typedef struct {
  uint8_t  byte;   /**< Byte value to write */
  rx_err_t result; /**< Operation result code */
} onewire_write_byte_ctx_t;

/** @brief Context for read_byte callback. */
typedef struct {
  uint8_t* byte;   /**< Output byte pointer */
  rx_err_t result; /**< Operation result code */
} onewire_read_byte_ctx_t;

/** @brief Context for write_buffer callback. */
typedef struct {
  const uint8_t* data;   /**< Data buffer to write */
  uint32_t       length; /**< Number of bytes to write */
  rx_err_t       result; /**< Operation result code */
} onewire_write_buf_ctx_t;

/** @brief Context for read_buffer callback. */
typedef struct {
  uint8_t* data;   /**< Output data buffer */
  uint32_t length; /**< Number of bytes to read */
  rx_err_t result; /**< Operation result code */
} onewire_read_buf_ctx_t;

/** @brief Context for match_rom callback. */
typedef struct {
  const uint8_t* rom;    /**< 8-byte ROM address */
  rx_err_t       result; /**< Operation result code */
} onewire_match_rom_ctx_t;

/** @brief Context for read_rom callback. */
typedef struct {
  uint8_t* rom;    /**< Output 8-byte ROM buffer */
  rx_err_t result; /**< Operation result code */
} onewire_read_rom_ctx_t;

/** @brief Context for search callback. */
typedef struct {
  uint8_t*  roms;        /**< Output ROM array */
  uint32_t  max_devices; /**< Maximum number of devices to find */
  uint32_t* num_devices; /**< Output: number of devices found */
  rx_err_t  result;      /**< Operation result code */
} onewire_search_ctx_t;

/**
 * @brief Runtime state per OneWire bus (exposed for testing).
 */
typedef struct {
  uint8_t last_rom[k_onewire_rom_bytes]; /**< Last ROM discovered */
  uint8_t last_discrepancy;              /**< Search tree discrepancy bit */
  bool    last_device_flag;              /**< True when last device enumerated */
  bool    line_is_output;                /**< Current GPIO drive mode */
} onewire_runtime_state_t;

/* Internal functions exposed for unit testing */
void     internal_delay_timer_init(void);
void     internal_delay_timer_reset(void);
void     internal_delay_us(uint32_t microseconds);
rx_err_t internal_set_drive_mode(const rx_bus_config_t*   bus_config,
                                 onewire_runtime_state_t* state,
                                 bool                     output);
rx_err_t internal_drive_low(const rx_bus_config_t* bus_config, onewire_runtime_state_t* state);
rx_err_t internal_release_line(const rx_bus_config_t* bus_config, onewire_runtime_state_t* state);
rx_err_t internal_read_line(const rx_bus_config_t* bus_config, bool* high);
rx_err_t
internal_reset_pulse(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool* presence);
rx_err_t
internal_write_bit(const rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool bit);
rx_err_t
internal_read_bit(const rx_bus_config_t* bus_config, onewire_runtime_state_t* state, bool* bit);
rx_err_t
internal_write_byte(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, uint8_t byte);
rx_err_t
internal_read_byte(rx_bus_config_t* bus_config, onewire_runtime_state_t* state, uint8_t* byte);
rx_err_t internal_search_step(rx_bus_config_t*         bus_config,
                              onewire_runtime_state_t* state,
                              uint8_t                  rom[k_onewire_rom_bytes],
                              uint8_t                  bit_number,
                              uint8_t*                 rom_byte_index,
                              uint8_t*                 rom_bit_mask,
                              uint8_t*                 last_zero);
rx_err_t internal_search_bits(rx_bus_config_t*         bus_config,
                              onewire_runtime_state_t* state,
                              uint8_t                  rom[k_onewire_rom_bytes],
                              uint8_t*                 last_zero);
rx_err_t internal_search_iteration(rx_bus_config_t*         bus_config,
                                   onewire_runtime_state_t* state,
                                   uint8_t                  rom[k_onewire_rom_bytes],
                                   bool*                    device_found);
rx_err_t internal_onewire_init_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_deinit_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_reset_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_write_bit_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_read_bit_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_write_byte_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_read_byte_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_write_buffer_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_read_buffer_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_skip_rom_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_match_rom_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_read_rom_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_onewire_search_callback(rx_bus_config_t* bus_config, void* user_ctx);

/** @} */ /* end of onewire_unit_test_internals */

#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
