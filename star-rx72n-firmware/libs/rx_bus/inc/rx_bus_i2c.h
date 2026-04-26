/**
 * @file rx_bus_i2c.h
 * @brief I2C (Inter-Integrated Circuit) Bus Abstraction for RX72N
 *
 * @details
 * Provides bus manager integration for Renesas RIIC (I2C) peripheral operations.
 * Wraps the low-level RIIC HAL with the bus abstraction pattern for consistent
 * API across all bus types (I2C, SPI, 1-Wire, UART) with mutex-protected access.
 *
 * @par System Architecture
 * @dot
 * digraph i2c_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="I2C Bus API (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     init [label="rx_bus_i2c_init()"];
 *     write [label="rx_bus_i2c_write()"];
 *     read [label="rx_bus_i2c_read()"];
 *     write_read [label="rx_bus_i2c_write_read()"];
 *   }
 *
 *   subgraph cluster_bus {
 *     label="Bus Manager";
 *     style=filled;
 *     color=lightblue;
 *     manager [label="rx_bus_manager_t\nMutex protection\nBus lookup"];
 *     config [label="rx_bus_config_t\nChannel, Address\nSpeed config"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="RIIC HAL Layer";
 *     style=filled;
 *     color=lightgreen;
 *     riic [label="riic_init()\nriic_write()\nriic_read()"];
 *     regs [label="RIIC0/1/2\nRegister Access"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="I2C Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     scl [label="SCL (Clock)\nOpen-drain"];
 *     sda [label="SDA (Data)\nOpen-drain"];
 *     other [label="Other I2C\nPeripherals"];
 *   }
 *
 *   init -> manager;
 *   write -> manager;
 *   read -> manager;
 *   write_read -> manager;
 *   manager -> config;
 *   config -> riic;
 *   riic -> regs;
 *   regs -> scl;
 *   regs -> sda;
 *   scl -> other;
 *   sda -> other;
 * }
 * @enddot
 *
 * @par Protocol Overview
 * I2C (Inter-Integrated Circuit) is a synchronous, multi-controller, multi-peripheral
 * serial communication bus:
 * - **Two-wire**: SCL (clock) + SDA (data), both open-drain with pullups
 * - **Controller-initiated**: Controller generates clock and initiates transactions
 * - **7/10-bit addressing**: Device address in first byte(s) after START
 * - **ACK/NACK**: Receiver acknowledges each byte
 * - **Multi-controller**: Bus arbitration and clock stretching supported
 *
 * @par Transaction Sequence
 * @msc
 * Controller, SDA, SCL, Peripheral;
 *
 * ... [label="Write Transaction"];
 * Controller -> SDA [label="START"];
 * Controller -> SCL [label="Clock 8 bits"];
 * Controller -> SDA [label="Address + W (0)"];
 * Peripheral -> SDA [label="ACK"];
 * Controller -> SCL [label="Clock 8 bits"];
 * Controller -> SDA [label="Data byte"];
 * Peripheral -> SDA [label="ACK"];
 * Controller -> SDA [label="STOP"];
 *
 * ... [label="Read Transaction"];
 * Controller -> SDA [label="START"];
 * Controller -> SDA [label="Address + R (1)"];
 * Peripheral -> SDA [label="ACK"];
 * Peripheral -> SDA [label="Data byte"];
 * Controller -> SDA [label="ACK/NACK"];
 * Controller -> SDA [label="STOP"];
 *
 * ... [label="Write-Read (Register Read)"];
 * Controller -> SDA [label="START"];
 * Controller -> SDA [label="Address + W"];
 * Peripheral -> SDA [label="ACK"];
 * Controller -> SDA [label="Register address"];
 * Peripheral -> SDA [label="ACK"];
 * Controller -> SDA [label="REPEATED START"];
 * Controller -> SDA [label="Address + R"];
 * Peripheral -> SDA [label="ACK + Data"];
 * Controller -> SDA [label="NACK + STOP"];
 * @endmsc
 *
 * @par Supported Devices (STAR Project)
 * | Device | Address | Description | Bus |
 * |--------|---------|-------------|-----|
 * | EEPROM | 0x50-0x57 | Configuration storage | RIIC1 |
 * | IMU | 0x68/0x69 | Inertial measurement unit | RIIC1 |
 *
 * @par Speed Modes (RX72N RIIC)
 * | Mode | Bit Rate | SCL High | SCL Low | Notes |
 * |------|----------|----------|---------|-------|
 * | Standard | 100 kHz | 4.0 us | 4.7 us | Default |
 * | Fast | 400 kHz | 0.6 us | 1.3 us | Most devices |
 * | Fast-Plus | 1 MHz | 0.26 us | 0.5 us | Limited support |
 *
 * @par Bit Rate Calculation
 * The RIIC bit rate is calculated from PCLKB:
 * @f[
 * BRR = \frac{PCLKB}{2 \times BitRate \times (CKS + 1)} - 1
 * @f]
 * Where:
 * - PCLKB = 60 MHz (peripheral clock B)
 * - CKS = clock divider select (0-7)
 * - BRR = bit rate register value (ICBRL/ICBRH)
 *
 * @par Performance Characteristics
 * | Metric | Standard | Fast | Fast-Plus |
 * |--------|----------|------|-----------|
 * | Byte time | 90 us | 22.5 us | 9 us |
 * | 8-byte transfer | ~1 ms | ~250 us | ~100 us |
 * | Overhead (START/STOP) | ~20 us | ~5 us | ~2 us |
 * | Typical latency | 100 us | 30 us | 15 us |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~1 KB | All API functions |
 * | Stack | 64 bytes | Maximum call depth |
 * | No heap | 0 | Static allocation only |
 *
 * @par Hardware Requirements
 * | Requirement | Specification |
 * |-------------|---------------|
 * | GPIO | Open-drain or external pullups |
 * | SCL Pullup | 2.2-10 kOhm to VDD |
 * | SDA Pullup | 2.2-10 kOhm to VDD |
 * | VDD | 1.8V - 5.5V (level dependent) |
 * | Bus capacitance | < 400 pF (standard mode) |
 *
 * @par RIIC Channel Assignments (STAR Project)
 * | Channel | SCL Pin | SDA Pin | Usage |
 * |---------|---------|---------|-------|
 * | RIIC0 | P16 | P17 | General purpose I2C |
 * | RIIC1 | P52 | P50 | Sensor expansion |
 * | RIIC2 | Reserved | Reserved | Future use |
 *
 * @par Error Handling Strategy
 * | Error | Cause | Recovery |
 * |-------|-------|----------|
 * | k_rx_err_nack | Device not responding | Check address, retry |
 * | k_rx_err_timeout | Clock stretching exceeded | Reset bus, check device |
 * | k_rx_err_busy | Bus arbitration lost | Retry after delay |
 * | k_rx_err_invalid_state | Bus not initialized | Call rx_bus_i2c_init() |
 *
 * @par Thread Safety
 * All functions are thread-safe when used with the bus manager:
 * - Bus manager provides mutex protection
 * - Timeout on mutex acquisition prevents deadlock
 * - Safe to call from multiple ThreadX threads
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - riic.c: Low-level RIIC hardware driver
 * - rx_mpc.h: Pin function selection
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded by data length
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Functions under 60 lines
 * - Rule 5: [OK] Input validation via RX_CHECK_NULL_PTR
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Limited preprocessor (typed enums preferred)
 * - Rule 9: [OK] Function pointers only for bus abstraction (DIP)
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - I2C bus operations only
 * - **O**: Open/Closed - Extensible via bus manager config
 * - **L**: Liskov Substitution - Implements rx_bus_interface_t
 * - **I**: Interface Segregation - Minimal 4-function API
 * - **D**: Dependency Inversion - Depends on abstractions (bus manager)
 *
 * @par Usage Example
 * @code
 * // Initialize bus manager and I2C bus
 * rx_bus_manager_t manager;
 * rx_bus_manager_init(&manager);
 *
 * // Register I2C bus with configuration
 * rx_bus_config_t config = {
 *     .type = k_rx_bus_type_i2c,
 *     .channel = 0,
 *     .i2c = {
 *         .address = 0x68,      // IMU address
 *         .speed_hz = 400000,   // 400 kHz fast mode
 *     }
 * };
 * rx_bus_register(&manager, "imu_i2c", &config);
 *
 * // Initialize I2C bus
 * rx_err_t err = rx_bus_i2c_init(&manager, "imu_i2c");
 * if (err != k_rx_ok) {
 *     // Handle initialization error
 *     return err;
 * }
 *
 * // Read IMU register (0x3B = ACCEL_XOUT_H)
 * uint8_t reg_addr = 0x3B;
 * uint8_t accel_data[2];
 * err = rx_bus_i2c_write_read(&manager, "imu_i2c",
 *                              &reg_addr, 1,
 *                              accel_data, 2);
 * if (err == k_rx_ok) {
 *     int16_t accel_x = (int16_t)((accel_data[0] << 8) | accel_data[1]);
 *     // Process accelerometer reading
 * }
 * @endcode
 *
 * @see rx_bus_manager.h Bus manager API for registration and lookup
 * @see rx_bus_types.h Bus type definitions and configurations
 * @see riic.c Low-level RIIC hardware driver implementation
 *
 * @version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * I2C Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize I2C bus through bus manager
 *
 * @details
 * Configures the RIIC peripheral channel and associated GPIO pins for I2C
 * communication. This function must be called before any read/write operations.
 * The initialization process:
 *
 * 1. Acquires bus manager mutex (thread-safe)
 * 2. Looks up bus configuration by name
 * 3. Validates bus type is I2C
 * 4. Configures MPC for SCL/SDA pin functions
 * 5. Initializes RIIC peripheral with configured speed
 * 6. Releases mutex
 *
 * @par Initialization Sequence
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="rx_bus_i2c_init()", shape=ellipse];
 *   mutex [label="Acquire mutex"];
 *   lookup [label="Lookup bus by name"];
 *   validate [label="Validate bus type"];
 *   mpc [label="Configure MPC pins"];
 *   riic [label="Initialize RIIC"];
 *   release [label="Release mutex"];
 *   done [label="Return", shape=ellipse];
 *
 *   start -> mutex;
 *   mutex -> lookup;
 *   lookup -> validate;
 *   validate -> mpc;
 *   mpc -> riic;
 *   riic -> release;
 *   release -> done;
 * }
 * @enddot
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must have I2C bus registered via rx_bus_register()
 * @param[in] bus_name I2C bus name
 *   - Null-terminated string
 *   - Must match name used in rx_bus_register()
 *   - Maximum 31 characters
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, bus initialized and ready
 * @retval k_rx_err_null_ptr nullptr in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_arg Bus is not I2C type
 * @retval k_rx_err_timeout Mutex acquisition timeout (default 1000ms)
 * @retval k_rx_err_invalid_state Bus already initialized
 *
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre Bus must be registered via rx_bus_register() with type k_rx_bus_type_i2c
 *
 * @post RIIC channel configured and ready for transactions
 * @post SCL/SDA pins configured as open-drain with internal pullups disabled
 * @post Bus state set to initialized
 *
 * @note Thread-safe via bus manager mutex
 * @note Call only once per bus; subsequent calls return k_rx_err_invalid_state
 *
 * @warning External pullup resistors (2.2-10 kOhm) required on SCL/SDA
 * @warning Do not call from interrupt context (mutex wait)
 *
 * @par Performance:
 * - Execution time: ~100 us typical (RIIC initialization)
 * - Mutex timeout: 1000 ms (configurable via bus manager)
 *
 * @see rx_bus_manager_init() Initialize bus manager first
 * @see rx_bus_register() Register bus before initialization
 * @see rx_bus_i2c_write() Write data after initialization
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_i2c_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write data to I2C device through bus manager
 *
 * @details
 * Transmits data bytes to the I2C peripheral device configured for this bus.
 * The device address is taken from the bus configuration. This function
 * handles the complete I2C write transaction:
 *
 * 1. Acquire bus manager mutex
 * 2. Generate START condition
 * 3. Send device address with W bit (0)
 * 4. Wait for ACK from peripheral
 * 5. Send each data byte, waiting for ACK
 * 6. Generate STOP condition
 * 7. Release mutex
 *
 * @par I2C Write Transaction
 * @verbatim
 *   ____       ___ ___ ___ ___ ___ ___ ___ ___   ___       ____
 *  SDA  \_____/   |   |   |   |   |   |   |   \_/   \_____/
 *                 |   Address + W   | ACK |   Data   | ACK |
 *        START    |<---- 8 bits --->|     |<- 8 bits->|     | STOP
 *   ___     _   _   _   _   _   _   _   _   _   _   _   _   ___
 *  SCL  \__/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
 * @endverbatim
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must not be nullptr
 * @param[in] bus_name I2C bus name
 *   - Null-terminated string matching registered bus
 *   - Must not be nullptr
 * @param[in] data Pointer to data buffer to write
 *   - Must not be nullptr
 *   - Must remain valid for duration of call
 * @param[in] length Number of bytes to write
 *   - Range: 1-65535 bytes
 *   - 0 is valid (address-only probe)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, all bytes transmitted and ACKed
 * @retval k_rx_err_null_ptr nullptr in manager, bus_name, or data
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_state Bus not initialized via rx_bus_i2c_init()
 * @retval k_rx_err_timeout I2C transaction timeout or mutex timeout
 * @retval k_rx_err_nack Device NACK received (address or data)
 * @retval k_rx_err_busy Bus arbitration lost
 *
 * @pre Bus must be initialized via rx_bus_i2c_init()
 * @pre data buffer must be valid for duration of call
 *
 * @post All bytes transmitted to device on success
 * @post Bus returned to idle state (STOP sent)
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocks until transaction complete or timeout
 *
 * @warning Do not call from interrupt context
 * @warning NACK on address byte indicates device not responding
 *
 * @par Performance (100 kHz standard mode):
 * - 1 byte: ~100 us
 * - 8 bytes: ~800 us
 * - Overhead: ~20 us (START/STOP/address)
 *
 * @par Example:
 * @code
 * // Write register address and data to device
 * uint8_t write_buf[3] = {0x00, 0x12, 0x34};  // Reg addr + 2 data bytes
 * rx_err_t err = rx_bus_i2c_write(&manager, "eeprom", write_buf, 3);
 * if (err == k_rx_err_nack) {
 *     // Device not responding - check address/connection
 * }
 * @endcode
 *
 * @see rx_bus_i2c_init() Initialize bus first
 * @see rx_bus_i2c_read() Read data from device
 * @see rx_bus_i2c_write_read() Combined write-read for register access
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_i2c_write(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        const uint8_t*    data,
                                        uint16_t          length);

/**
 * @brief Read data from I2C device through bus manager
 *
 * @details
 * Receives data bytes from the I2C peripheral device configured for this bus.
 * The device address is taken from the bus configuration. This function
 * handles the complete I2C read transaction:
 *
 * 1. Acquire bus manager mutex
 * 2. Generate START condition
 * 3. Send device address with R bit (1)
 * 4. Wait for ACK from peripheral
 * 5. Receive each data byte, sending ACK (except last byte)
 * 6. Send NACK on last byte to signal end
 * 7. Generate STOP condition
 * 8. Release mutex
 *
 * @par I2C Read Transaction
 * @verbatim
 *   ____       ___ ___ ___ ___ ___ ___ ___ ___   ___       ____
 *  SDA  \_____/   |   |   |   |   |   |   |   \_/   \_____/
 *                 |   Address + R   | ACK |   Data   |NACK |
 *        START    |<---- 8 bits --->|     |<- 8 bits->|     | STOP
 *                 |   Controller    | Per |Peripheral| Ctrl|
 *   ___     _   _   _   _   _   _   _   _   _   _   _   _   ___
 *  SCL  \__/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
 * @endverbatim
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must not be nullptr
 * @param[in] bus_name I2C bus name
 *   - Null-terminated string matching registered bus
 *   - Must not be nullptr
 * @param[out] data Pointer to buffer for received data
 *   - Must not be nullptr
 *   - Must have capacity for 'length' bytes
 * @param[in] length Number of bytes to read
 *   - Range: 1-65535 bytes
 *   - 0 returns immediately with k_rx_ok
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, all bytes received
 * @retval k_rx_err_null_ptr nullptr in manager, bus_name, or data
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_state Bus not initialized via rx_bus_i2c_init()
 * @retval k_rx_err_timeout I2C transaction timeout or mutex timeout
 * @retval k_rx_err_nack Device NACK on address (device not responding)
 * @retval k_rx_err_busy Bus arbitration lost
 *
 * @pre Bus must be initialized via rx_bus_i2c_init()
 * @pre data buffer must have capacity for 'length' bytes
 *
 * @post data buffer contains received bytes on success
 * @post Bus returned to idle state (STOP sent)
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocks until transaction complete or timeout
 * @note For register reads, use rx_bus_i2c_write_read() instead
 *
 * @warning Do not call from interrupt context
 * @warning Raw read without prior register address write may return unexpected data
 *
 * @par Performance (100 kHz standard mode):
 * - 1 byte: ~100 us
 * - 8 bytes: ~800 us
 * - Overhead: ~20 us (START/STOP/address)
 *
 * @see rx_bus_i2c_init() Initialize bus first
 * @see rx_bus_i2c_write() Write data to device
 * @see rx_bus_i2c_write_read() Preferred for register reads
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bus_i2c_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint16_t length);

/**
 * @brief Write then read from I2C device through bus manager
 *
 * @details
 * Performs a combined write-read transaction, which is the standard pattern
 * for reading device registers. The write phase sends the register address,
 * then a repeated START initiates the read phase without releasing the bus.
 *
 * Transaction sequence:
 * 1. Acquire bus manager mutex
 * 2. Generate START condition
 * 3. Send device address with W bit (0)
 * 4. Send write_data bytes (typically register address)
 * 5. Generate REPEATED START (no STOP)
 * 6. Send device address with R bit (1)
 * 7. Receive read_data bytes
 * 8. Generate STOP condition
 * 9. Release mutex
 *
 * @par Combined Write-Read Transaction
 * @verbatim
 *   ____    _______________     _______________    ____
 *  SDA  \__/ Addr+W | Data |\__/ Addr+R | Data \__/
 *        S          |Wr Reg| Sr         |Rd Reg| P
 *                   |      |            |      |
 *   ___    _ _ _ _ _ _ _ _   _ _ _ _ _ _ _ _ _ _   ___
 *  SCL  \_/ - - - - - - - \_/ - - - - - - - - - \_/
 *
 *  S  = START condition
 *  Sr = REPEATED START (no STOP between)
 *  P  = STOP condition
 * @endverbatim
 *
 * @par Common Use Cases
 * | Use Case | Write Data | Read Length | Example |
 * |----------|------------|-------------|---------|
 * | Read 8-bit register | 1 byte (reg addr) | 1 byte | Temperature |
 * | Read 16-bit register | 1 byte (reg addr) | 2 bytes | Voltage |
 * | Read block | 1 byte (reg addr) | N bytes | EEPROM page |
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must not be nullptr
 * @param[in] bus_name I2C bus name
 *   - Null-terminated string matching registered bus
 *   - Must not be nullptr
 * @param[in] write_data Pointer to data to write (register address)
 *   - Must not be nullptr
 *   - Typically 1-2 bytes for register address
 * @param[in] write_length Number of bytes to write
 *   - Range: 1-65535 bytes (typically 1-2)
 * @param[out] read_data Pointer to buffer for received data
 *   - Must not be nullptr
 *   - Must have capacity for 'read_length' bytes
 * @param[in] read_length Number of bytes to read
 *   - Range: 1-65535 bytes
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, transaction completed
 * @retval k_rx_err_null_ptr nullptr in any parameter
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_state Bus not initialized via rx_bus_i2c_init()
 * @retval k_rx_err_timeout I2C transaction timeout or mutex timeout
 * @retval k_rx_err_nack Device NACK on address or write data
 * @retval k_rx_err_busy Bus arbitration lost
 *
 * @pre Bus must be initialized via rx_bus_i2c_init()
 * @pre write_data buffer must be valid for duration of call
 * @pre read_data buffer must have capacity for 'read_length' bytes
 *
 * @post read_data buffer contains received register data on success
 * @post Bus returned to idle state (STOP sent)
 *
 * @invariant Bus remains locked for entire write+read sequence
 *
 * @note Thread-safe via bus manager mutex
 * @note Uses REPEATED START to maintain bus ownership
 * @note Preferred method for register reads (atomic operation)
 *
 * @warning Do not call from interrupt context
 * @warning Some devices require delays between write and read phases
 *
 * @par Performance (100 kHz standard mode):
 * - 1 write + 1 read: ~200 us
 * - 1 write + 8 read: ~900 us
 * - Overhead: ~40 us (START/RESTART/STOP/addresses)
 *
 * @par Example - Reading IMU Register:
 * @code
 * // Read accelerometer X-axis from IMU (register 0x3B, 2 bytes)
 * uint8_t reg_addr = 0x3B;  // ACCEL_XOUT_H register
 * uint8_t accel_raw[2];
 *
 * rx_err_t err = rx_bus_i2c_write_read(&manager, "imu_i2c",
 *                                       &reg_addr, 1,
 *                                       accel_raw, 2);
 * if (err == k_rx_ok) {
 *     int16_t accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
 * }
 * @endcode
 *
 * @par Example - Reading EEPROM Block:
 * @code
 * // Read 16 bytes from EEPROM starting at address 0x0100
 * uint8_t eeprom_addr[2] = {0x01, 0x00};  // Big-endian address
 * uint8_t data[16];
 *
 * rx_err_t err = rx_bus_i2c_write_read(&manager, "eeprom",
 *                                       eeprom_addr, 2,
 *                                       data, 16);
 * @endcode
 *
 * @see rx_bus_i2c_init() Initialize bus first
 * @see rx_bus_i2c_write() Write-only operations
 * @see rx_bus_i2c_read() Read-only operations
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_i2c_write_read(rx_bus_manager_t* manager,
                                             const char*       bus_name,
                                             const uint8_t*    write_data,
                                             uint16_t          write_length,
                                             uint8_t*          read_data,
                                             uint16_t          read_length);

#ifdef UNIT_TEST
/* =============================================================================
 * Internal callback context types exposed for unit testing.
 * These types are defined in rx_bus_i2c.c and only declared here when
 * UNIT_TEST is defined so that test files can call the callbacks directly
 * to exercise defensive branches that are unreachable through the public API.
 * =============================================================================
 */

/** @brief Init callback context (mirrors i2c_init_ctx_t in rx_bus_i2c.c) */
typedef struct {
  rx_err_t          result;  /**< Operation result */
  rx_bus_manager_t* manager; /**< Bus manager pointer */
} i2c_init_ctx_t;

/** @brief Write callback context (mirrors i2c_write_ctx_t in rx_bus_i2c.c) */
typedef struct {
  const uint8_t* data;   /**< Data to write */
  uint16_t       length; /**< Number of bytes */
  rx_err_t       result; /**< Operation result */
} i2c_write_ctx_t;

/** @brief Read callback context (mirrors i2c_read_ctx_t in rx_bus_i2c.c) */
typedef struct {
  uint8_t* data;   /**< Buffer to fill */
  uint16_t length; /**< Number of bytes to read */
  rx_err_t result; /**< Operation result */
} i2c_read_ctx_t;

/** @brief Write-read callback context (mirrors i2c_write_read_ctx_t in rx_bus_i2c.c) */
typedef struct {
  const uint8_t* write_data;   /**< Data to write */
  uint16_t       write_length; /**< Write byte count */
  uint8_t*       read_data;    /**< Buffer to fill */
  uint16_t       read_length;  /**< Read byte count */
  rx_err_t       result;       /**< Operation result */
} i2c_write_read_ctx_t;

rx_err_t internal_i2c_init_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_i2c_write_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_i2c_read_callback(rx_bus_config_t* bus_config, void* user_ctx);
rx_err_t internal_i2c_write_read_callback(rx_bus_config_t* bus_config, void* user_ctx);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
