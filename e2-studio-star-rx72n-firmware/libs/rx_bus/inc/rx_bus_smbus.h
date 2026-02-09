/* lib/rx_bus/inc/rx_bus_smbus.h */

/**
 * @file rx_bus_smbus.h
 * @brief SMBus (System Management Bus) abstraction for RX72N bus manager
 *
 * @details
 * Provides SMBus protocol implementation through the RX72N bus manager,
 * enabling reliable communication with battery fuel gauges (BQ4050), temperature
 * sensors (LM75), and other system monitoring devices with optional Packet Error
 * Checking (PEC) using CRC-8.
 *
 * ## System Architecture Context
 *
 * The SMBus adapter sits between application code and the RX72N RIIC (I2C)
 * hardware, implementing SMBus protocol requirements on top of I2C transport:
 *
 * ```
 * Application (BMS Monitor)
 *       |
 *       v
 * rx_bus_smbus (THIS MODULE)
 *       |
 *       v
 * rx_bus_manager (Bus Abstraction)
 *       |
 *       v
 * rx_bus_i2c (I2C Adapter)
 *       |
 *       v
 * RIIC Hardware (RX72N Peripheral)
 * ```
 *
 * ## SMBus Protocol Overview
 *
 * SMBus is a System Management Bus protocol built on top of I2C with additional
 * requirements and features for reliable embedded system communication.
 *
 * **SMBus vs I2C Key Differences**:
 *
 * | Feature | I2C | SMBus |
 * |---------|-----|-------|
 * | Minimum Clock | DC (0 Hz) | 10 kHz |
 * | Maximum Clock | 400 kHz (fast mode) | 100 kHz (typical) |
 * | Timeout | Optional | **REQUIRED**: 25-35 ms |
 * | Voltage Levels | Board-dependent | 3.3V or 5V (defined) |
 * | Logic Levels | CMOS/TTL | SMBus-compliant |
 * | Error Checking | None (application) | **Optional PEC (CRC-8)** |
 * | Alert Signal | Not defined | Optional SMBUSALERT# pin |
 * | Address Range | 0x00-0x7F | 0x08-0x77 (0x00-0x07 reserved) |
 *
 * **Packet Error Checking (PEC)**:
 * - CRC-8 with polynomial: x^8 + x^2 + x + 1 (0x07)
 * - Initial value: 0x00
 * - Calculated over: Address + R/W + Command + Data bytes
 * - Transmitted: As final byte after data
 * - Verification: Receiver recalculates and compares
 *
 * ## Supported SMBus Protocols
 *
 * This module implements the following SMBus 2.0 protocols:
 *
 * | Protocol | Command Byte | Data Bytes | PEC Support | Use Case |
 * |----------|--------------|------------|-------------|----------|
 * | Quick Command | No | No | No | Device presence detect |
 * | Send Byte | Yes | No | Yes | Simple commands |
 * | Receive Byte | No | 1 | Yes | Status polling |
 * | Write Byte Data | Yes | 1 | Yes | 8-bit register write |
 * | Read Byte Data | Yes | 1 | Yes | 8-bit register read |
 * | Write Word Data | Yes | 2 (LE) | Yes | 16-bit register write |
 * | Read Word Data | Yes | 2 (LE) | Yes | 16-bit register read |
 * | Block Read | Yes | 1-32 + count | Yes | Multi-byte reads |
 *
 * **Note**: LE = Little-Endian byte order (LSB first)
 *
 * ## SMBus Transaction Sequences
 *
 * **Write Byte Data with PEC**:
 * @msc
 * Controller, Device;
 *
 * --- [label="Write Byte Data (0xAA to command 0x10)"];
 * Controller => Device [label="START"];
 * Controller => Device [label="Address + W (0x16 << 1 | 0)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="Command (0x10)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="Data (0xAA)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="PEC (0xB2)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="STOP"];
 * @endmsc
 *
 * **Read Word Data with PEC**:
 * @msc
 * Controller, Device;
 *
 * --- [label="Read Word Data (command 0x09)"];
 * Controller => Device [label="START"];
 * Controller => Device [label="Address + W (0x16 << 1 | 0)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="Command (0x09)"];
 * Device => Controller [label="ACK"];
 * Controller => Device [label="REPEATED START"];
 * Controller => Device [label="Address + R (0x16 << 1 | 1)"];
 * Device => Controller [label="ACK"];
 * Device => Controller [label="Data Low (0x34)"];
 * Controller => Device [label="ACK"];
 * Device => Controller [label="Data High (0x12)"];
 * Controller => Device [label="ACK"];
 * Device => Controller [label="PEC (0x5F)"];
 * Controller => Device [label="NACK"];
 * Controller => Device [label="STOP"];
 * @endmsc
 *
 * ## Design Rationale
 *
 * **Why SMBus over I2C?**
 * - **Reliability**: PEC detects ~99.6% of transmission errors
 * - **Standardization**: Well-defined protocols reduce integration time
 * - **Battery Management**: BQ4050 fuel gauge requires SMBus with PEC
 * - **System Management**: Common in embedded monitoring applications
 *
 * **Implementation Approach**:
 * - Built on existing I2C infrastructure (rx_bus_i2c)
 * - PEC calculated in software (hardware CRC not used for SMBus CRC-8)
 * - Timeout enforcement at I2C layer (25 ms minimum)
 * - Little-endian byte order for word operations (SMBus standard)
 *
 * ## Performance Characteristics
 *
 * | Operation | Typical Time @ 100 kHz | Notes |
 * |-----------|------------------------|-------|
 * | rx_bus_smbus_init() | ~50 µs | I2C peripheral configuration |
 * | rx_bus_smbus_write_byte() | ~200 µs | 3 bytes + START/STOP overhead |
 * | rx_bus_smbus_read_byte() | ~180 µs | 2 bytes + START/STOP overhead |
 * | rx_bus_smbus_write_byte_data() | ~280 µs | 4 bytes + PEC |
 * | rx_bus_smbus_read_byte_data() | ~300 µs | 3 bytes + PEC |
 * | rx_bus_smbus_write_word_data() | ~360 µs | 5 bytes + PEC |
 * | rx_bus_smbus_read_word_data() | ~400 µs | 4 bytes + PEC |
 * | rx_bus_smbus_read_block_data() | 1-3 ms | 32 bytes max + PEC |
 *
 * **PEC Overhead**: ~10 µs per transaction (software CRC-8 calculation)
 *
 * ## Memory Usage
 *
 * **Static**: None (no module-level state, managed by bus manager)
 * **Stack per call**: 32-64 bytes (depends on operation)
 * **Per-transaction heap**: 0 bytes (no dynamic allocation)
 *
 * ## Hardware Requirements
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | RX72N RIIC | RIIC0, RIIC1, or RIIC2 | I2C peripheral |
 * | Clock Frequency | 100 kHz (typical) | SMBus standard speed |
 * | SDA Pullup | 4.7 kΩ (5V) or 10 kΩ (3.3V) | External resistor required |
 * | SCL Pullup | 4.7 kΩ (5V) or 10 kΩ (3.3V) | External resistor required |
 * | Voltage Level | 3.3V or 5V | Must match device requirements |
 * | Timeout | 25-35 ms | Enforced by I2C layer |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   smbus [label="rx_bus_smbus.h\n(THIS MODULE)", fillcolor=lightblue, style=filled];
 *   manager [label="rx_bus_manager.h"];
 *   i2c [label="rx_bus_i2c"];
 *   riic [label="RX72N RIIC HW"];
 *   err [label="rx_err.h"];
 *
 *   smbus -> manager [label="uses"];
 *   smbus -> err [label="returns"];
 *   manager -> i2c [label="delegates to"];
 *   i2c -> riic [label="controls"];
 * }
 * @enddot
 *
 * - **rx_bus_manager.h**: Bus abstraction and lookup
 * - **rx_bus_i2c**: Underlying I2C transport layer
 * - **rx_err.h**: Standard error codes
 * - **RX72N RIIC**: I2C hardware peripheral
 *
 * @par NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | ✓ | No recursion, goto, setjmp/longjmp |
 * | 2. Fixed loop bounds | ✓ | All loops have static upper bounds |
 * | 3. No dynamic memory | ✓ | No malloc/free, static/stack allocation only |
 * | 4. Functions <60 lines | ✓ | All functions <50 lines |
 * | 5. Min 2 assertions | ✓ | NULL checks + state validation per function |
 * | 6. Data at smallest scope | ✓ | No module-level state |
 * | 7. Check return values | ✓ | All functions return rx_err_t |
 * | 8. Limit preprocessor | ✓ | Only include guards and extern "C" |
 * | 9. Restrict pointers | ✓ | Single-level dereferencing |
 * | 10. Compile with warnings | ✓ | -Wall -Wextra -Werror |
 *
 * @par SOLID Principles
 *
 * **Single Responsibility**: This module has ONE job - implement SMBus protocol.
 * It does NOT manage buses (rx_bus_manager), implement I2C hardware (rx_bus_i2c),
 * or handle device-specific logic (rx_bq4050).
 *
 * **Open/Closed**: New SMBus protocols can be added as new functions without
 * modifying existing code. PEC can be enabled/disabled via configuration.
 *
 * **Liskov Substitution**: All SMBus functions follow consistent error handling
 * and parameter conventions. Any SMBus function can be used interchangeably
 * based on protocol requirements.
 *
 * **Interface Segregation**: Each SMBus protocol has a dedicated function
 * (write_byte, read_word, etc.). Clients only use the protocols they need.
 *
 * **Dependency Inversion**: This module depends on the abstract bus manager
 * interface, not concrete I2C hardware. Testing uses mock bus manager.
 *
 * @see rx_bus_config_init_smbus() Configure SMBus device before use
 * @see rx_bus_manager.h for bus management operations
 * @see rx_bq4050.h for BQ4050 fuel gauge driver (primary SMBus use case)
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright MIT License
 */

#ifndef STAR_RX72N_BUS_SMBUS_H
#define STAR_RX72N_BUS_SMBUS_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SMBUS Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize SMBus device through bus manager
 *
 * @details
 * Configures the RX72N RIIC peripheral for SMBus communication with the
 * specified device. Sets up I2C hardware with SMBus-compliant timing,
 * timeout enforcement, and optional Packet Error Checking (PEC).
 *
 * **Algorithm**:
 * 1. Validate manager pointer (NULL check)
 * 2. Validate bus_name pointer (NULL check)
 * 3. Look up bus configuration by name in bus manager
 * 4. Verify bus type is k_rx_bus_type_smbus
 * 5. Extract SMBus configuration (channel, address, pins, frequency, PEC)
 * 6. Configure RIIC hardware with SMBus timing parameters
 * 7. Set timeout to 25 ms minimum (SMBus requirement)
 * 8. Mark bus as initialized
 * 9. Return k_rx_ok
 *
 * **Control Flow**: Linear validation and initialization sequence with
 * early exit on error. No loops, no recursion.
 *
 * **SMBus Initialization Requirements**:
 * - Clock frequency: Typically 100 kHz (SMBus standard)
 * - Timeout: 25-35 ms clock low timeout (REQUIRED by SMBus spec)
 * - Rise time: <1 µs for 100 kHz with 4.7k pullups
 * - Logic levels: SMBus-compliant (3.3V or 5V)
 *
 * **Thread Safety**: NOT thread-safe. Caller must ensure exclusive access
 * to the bus during initialization. After init, bus operations use mutex.
 *
 * **Performance**: ~50 µs @ 240 MHz (parameter validation + RIIC config)
 *
 * @param[in] manager Bus manager instance.
 *                    - **Valid range**: Non-NULL, previously initialized
 *                    - **Constraints**: Must be valid bus manager instance
 *                    - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @param[in] bus_name SMBus bus name for lookup.
 *                     Must match name used in rx_bus_config_init_smbus().
 *                     - **Valid range**: Non-NULL, null-terminated string
 *                     - **Constraints**: Must be registered in bus manager
 *                     - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *
 * @return rx_err_t Error code
 *
 * @retval k_rx_ok Success. SMBus hardware configured and ready for operations.
 *                 **When**: All validation passed, RIIC configured.
 *                 **Action**: Proceed with SMBus read/write operations.
 *
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr.
 *                           **When**: Input validation at function entry.
 *                           **Cause**: Caller passed NULL pointer.
 *                           **Action**: Fix calling code - check pointers.
 *
 * @retval k_rx_err_not_found Bus name not found in manager.
 *                            **When**: Bus lookup in manager failed.
 *                            **Cause**: Typo in name, or forgot to call
 *                                       rx_bus_manager_add_bus().
 *                            **Action**: Verify name matches configuration.
 *
 * @retval k_rx_err_invalid_arg Bus exists but is not SMBus type.
 *                              **When**: Bus type check after lookup.
 *                              **Cause**: Called smbus_init on GPIO/ADC/etc bus.
 *                              **Action**: Use correct init function for bus type.
 *
 * @retval k_rx_err_timeout Mutex acquisition timeout.
 *                          **When**: Attempting to lock bus manager mutex.
 *                          **Cause**: Another thread holds lock >100ms.
 *                          **Action**: Check for deadlock or long operations.
 *
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre bus_name must be registered via rx_bus_manager_add_bus()
 * @pre Bus configuration must be created via rx_bus_config_init_smbus()
 * @pre RIIC pins must have external pullup resistors (4.7k typical)
 *
 * @post RIIC peripheral configured for SMBus operation
 * @post Bus marked as initialized (subsequent operations allowed)
 * @post Timeout set to 25 ms minimum (SMBus requirement)
 * @post PEC flag stored for use in subsequent transactions
 *
 * @note **Thread Safety**: Not thread-safe during init. Thread-safe after.
 * @note **Hardware**: Requires external pullup resistors on SDA and SCL.
 * @note **Re-entrancy**: Reentrant IF different buses. Not for same bus.
 *
 * @warning This function must complete before any SMBus operations.
 * @warning Do not call multiple times on same bus (reinitialize not supported).
 *
 * @par Example 1: BQ4050 fuel gauge initialization
 * @code{.c}
 * // Static configuration (created at startup)
 * static rx_bus_config_t fuel_gauge_config;
 * static rx_bus_manager_t bus_manager;
 *
 * // Configure BQ4050 SMBus
 * rx_err_t err = rx_bus_config_init_smbus(&fuel_gauge_config,
 *                                         "fuel_gauge",
 *                                         0,           // RIIC0
 *                                         0x0B,        // BQ4050 address
 *                                         k_rx_p1_2,   // SDA
 *                                         k_rx_p1_3,   // SCL
 *                                         100000,      // 100 kHz
 *                                         true);       // Enable PEC
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Config failed");
 *
 * // Register with bus manager
 * err = rx_bus_manager_add_bus(&bus_manager, &fuel_gauge_config);
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Register failed");
 *
 * // Initialize SMBus hardware
 * err = rx_bus_smbus_init(&bus_manager, "fuel_gauge");
 * if (err != k_rx_ok) {
 *     rx_log_error("SMBUS", "Init failed: %d", err);
 *     return err;
 * }
 *
 * rx_log_info("SMBUS", "BQ4050 SMBus initialized successfully");
 * @endcode
 *
 * @see rx_bus_config_init_smbus() Create SMBus configuration
 * @see rx_bus_manager_add_bus() Register bus with manager
 * @see rx_bus_smbus_read_word_data() Read 16-bit register after init
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 4 assertions (NULL×2, bus found, bus type)
 */
[[nodiscard]] rx_err_t rx_bus_smbus_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write single byte to SMBus device (Send Byte protocol)
 *
 * @details
 * Implements SMBus Send Byte protocol: sends a command byte to the device
 * with optional PEC. Used for simple commands like "start conversion" or
 * "reset".
 *
 * **SMBus Send Byte Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] [PEC [A]] P
 *
 * S = START condition
 * Addr = 7-bit device address
 * Wr = Write bit (0)
 * A = ACK from device
 * Command = 8-bit command code
 * PEC = Optional CRC-8 byte
 * P = STOP condition
 * ```
 *
 * **Algorithm**:
 * 1. Validate pointers (manager, bus_name)
 * 2. Look up bus in manager
 * 3. Verify bus is initialized (state check)
 * 4. Send START condition
 * 5. Send device address + Write bit
 * 6. Wait for ACK (or return NACK error)
 * 7. Send command byte
 * 8. Wait for ACK
 * 9. If PEC enabled: Calculate CRC-8, send PEC byte, wait for ACK
 * 10. Send STOP condition
 * 11. Return k_rx_ok
 *
 * **PEC Calculation** (if enabled):
 * ```c
 * uint8_t crc = 0x00;
 * crc = crc8(crc, (address << 1) | 0);  // Address + W
 * crc = crc8(crc, command);              // Command byte
 * // Send crc as PEC byte
 * ```
 *
 * **Performance**: ~200 µs @ 100 kHz (3 bytes + overhead)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Command byte to send
 *                    - **Valid range**: 0x00-0xFF
 *                    - **Units**: Dimensionless (device-specific command code)
 *                    - **Constraints**: Device-dependent
 *
 * @return rx_err_t Error code
 *
 * @retval k_rx_ok Success. Command sent and acknowledged.
 * @retval k_rx_err_null_ptr manager or bus_name is nullptr.
 * @retval k_rx_err_not_found Bus not found in manager.
 * @retval k_rx_err_invalid_state Bus not initialized (call rx_bus_smbus_init first).
 * @retval k_rx_err_timeout SMBus timeout (device not responding, >25ms low).
 * @retval k_rx_err_nack Device sent NACK (command rejected or device busy).
 * @retval k_rx_err_crc_mismatch PEC check failed (data corruption detected).
 *
 * @pre Bus must be initialized via rx_bus_smbus_init()
 * @pre Device must be powered and responsive
 *
 * @post Command sent to device
 * @post If error, bus state reset for next transaction
 *
 * @note **Thread Safety**: Thread-safe via bus manager mutex
 * @note **Performance**: ~200 µs per call @ 100 kHz
 *
 * @warning Device may NACK if busy - implement retry logic in application
 *
 * @par Example: Send reset command
 * @code{.c}
 * // Send 0xAA command (device-specific reset)
 * rx_err_t err = rx_bus_smbus_write_byte(&bus_manager, "fuel_gauge", 0xAA);
 * if (err == k_rx_err_nack) {
 *     // Device busy, retry
 *     tx_thread_sleep(10);  // 10ms delay
 *     err = rx_bus_smbus_write_byte(&bus_manager, "fuel_gauge", 0xAA);
 * }
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Reset command failed");
 * @endcode
 *
 * @see rx_bus_smbus_init() Must be called first
 * @see rx_bus_smbus_write_byte_data() Write byte to register
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_smbus_write_byte(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint8_t           command);

/**
 * @brief Read single byte from SMBus device (Receive Byte protocol)
 *
 * @details
 * Implements SMBus Receive Byte protocol: reads one byte from device with
 * optional PEC. Used for status polling or simple sensor readings.
 *
 * **SMBus Receive Byte Wire Format**:
 * ```
 * S Addr Rd [A] [Data] A [PEC] NA P
 *
 * S = START
 * Addr = 7-bit address
 * Rd = Read bit (1)
 * Data = Received byte
 * A = ACK from controller
 * PEC = Optional CRC-8
 * NA = NACK from controller (end of read)
 * P = STOP
 * ```
 *
 * **Algorithm**:
 * 1. Validate pointers
 * 2. Look up bus
 * 3. Verify initialized
 * 4. Send START
 * 5. Send address + Read bit
 * 6. Receive data byte, send ACK
 * 7. If PEC enabled: Receive PEC byte, send NACK, verify CRC
 * 8. Send STOP
 * 9. Return k_rx_ok
 *
 * **Performance**: ~180 µs @ 100 kHz
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] data Pointer to store received byte
 *                  - **Valid range**: Non-NULL
 *                  - **Constraints**: Must point to valid uint8_t
 *                  - **Null handling**: Returns k_rx_err_null_ptr
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, data written to *data
 * @retval k_rx_err_null_ptr NULL pointer
 * @retval k_rx_err_not_found Bus not found
 * @retval k_rx_err_invalid_state Not initialized
 * @retval k_rx_err_timeout Timeout
 * @retval k_rx_err_nack Device NACK
 * @retval k_rx_err_crc_mismatch PEC failed
 *
 * @pre Bus initialized
 * @post *data contains received byte (on success)
 *
 * @par Example: Poll status
 * @code{.c}
 * uint8_t status;
 * rx_err_t err = rx_bus_smbus_read_byte(&bus_manager, "sensor", &status);
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Status read failed");
 * if (status & 0x80) {
 *     rx_log_warn("SMBUS", "Device reports error");
 * }
 * @endcode
 *
 * @see rx_bus_smbus_read_byte_data() Read from specific register
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_smbus_read_byte(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                uint8_t*          data);

/**
 * @brief Write byte to SMBus register (Write Byte Data protocol)
 *
 * @details
 * Implements SMBus Write Byte Data protocol: writes one data byte to a
 * specified register/command code. Most common SMBus write operation.
 *
 * **Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] Data [A] [PEC [A]] P
 * ```
 *
 * **Algorithm**:
 * 1. Validate inputs
 * 2. START + Address + Write
 * 3. Send command byte
 * 4. Send data byte
 * 5. Calculate and send PEC (if enabled)
 * 6. STOP
 *
 * **PEC Coverage**: Address+W, Command, Data
 *
 * **Performance**: ~280 µs @ 100 kHz (4 bytes + PEC)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Register/command address (0x00-0xFF)
 * @param[in] data Data byte to write (0x00-0xFF)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr NULL pointer
 * @retval k_rx_err_timeout Timeout
 * @retval k_rx_err_nack NACK received
 * @retval k_rx_err_crc_mismatch PEC failed
 *
 * @pre Bus initialized
 * @post Register written (on success)
 *
 * @par Example: Write control register
 * @code{.c}
 * // Write 0x80 to control register 0x06
 * rx_err_t err = rx_bus_smbus_write_byte_data(&bus_manager,
 *                                             "fuel_gauge",
 *                                             0x06,  // Control register
 *                                             0x80); // Enable bit
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Control write failed");
 * @endcode
 *
 * @see rx_bus_smbus_read_byte_data() Read byte from register
 * @see rx_bus_smbus_write_word_data() Write 16-bit value
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_smbus_write_byte_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t           data);

/**
 * @brief Read byte from SMBus register (Read Byte Data protocol)
 *
 * @details
 * Implements SMBus Read Byte Data protocol: reads one byte from specified
 * register. Most common SMBus read operation.
 *
 * **Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] Sr Addr Rd [A] [Data] A [PEC] NA P
 *
 * Sr = Repeated START
 * ```
 *
 * **Algorithm**:
 * 1. Validate inputs
 * 2. START + Address + Write
 * 3. Send command byte
 * 4. Repeated START + Address + Read
 * 5. Receive data byte
 * 6. Receive and verify PEC (if enabled)
 * 7. STOP
 *
 * **Performance**: ~300 µs @ 100 kHz
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Register/command address
 * @param[out] data Pointer to store received byte
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, *data valid
 * @retval k_rx_err_null_ptr NULL pointer
 * @retval k_rx_err_timeout Timeout
 * @retval k_rx_err_crc_mismatch PEC failed
 *
 * @pre Bus initialized
 * @post *data contains register value
 *
 * @par Example: Read temperature
 * @code{.c}
 * uint8_t temp_c;
 * rx_err_t err = rx_bus_smbus_read_byte_data(&bus_manager,
 *                                            "temp_sensor",
 *                                            0x00,      // Temperature reg
 *                                            &temp_c);
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Temp read failed");
 * rx_log_info("SMBUS", "Temperature: %d C", temp_c);
 * @endcode
 *
 * @see rx_bus_smbus_write_byte_data()
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_smbus_read_byte_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint8_t*          data);

/**
 * @brief Write 16-bit word to SMBus register (Write Word Data protocol)
 *
 * @details
 * Implements SMBus Write Word Data protocol: writes 16-bit value to register
 * in **little-endian** byte order (LSB first, as per SMBus specification).
 *
 * **Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] DataLow [A] DataHigh [A] [PEC [A]] P
 * ```
 *
 * **Byte Order**: LITTLE-ENDIAN (LSB first)
 * ```c
 * uint16_t value = 0x1234;
 * // Wire: 0x34 (low byte), then 0x12 (high byte)
 * ```
 *
 * **Algorithm**:
 * 1. Validate inputs
 * 2. START + Address + Write
 * 3. Send command
 * 4. Send data low byte (value & 0xFF)
 * 5. Send data high byte (value >> 8)
 * 6. Calculate and send PEC (if enabled)
 * 7. STOP
 *
 * **PEC Coverage**: Address+W, Command, DataLow, DataHigh
 *
 * **Performance**: ~360 µs @ 100 kHz (5 bytes + PEC)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Register/command address
 * @param[in] data 16-bit data word (host byte order, converted to LE)
 *                 - **Valid range**: 0x0000-0xFFFF
 *                 - **Units**: Device-dependent
 *                 - **Byte order**: Automatically converted to little-endian
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr NULL pointer
 * @retval k_rx_err_timeout Timeout
 * @retval k_rx_err_nack NACK
 * @retval k_rx_err_crc_mismatch PEC failed
 *
 * @pre Bus initialized
 * @post Register written with word value (LE)
 *
 * @par Example: Set battery voltage threshold
 * @code{.c}
 * // Set voltage threshold to 3000 mV (0x0BB8)
 * rx_err_t err = rx_bus_smbus_write_word_data(&bus_manager,
 *                                             "fuel_gauge",
 *                                             0x0A,    // Voltage register
 *                                             3000);   // 3000 mV
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Voltage write failed");
 *
 * // Wire transmission: 0xB8 (low), then 0x0B (high)
 * @endcode
 *
 * @see rx_bus_smbus_read_word_data() Read 16-bit word
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 4 assertions (NULL×2, bus found, initialized)
 */
[[nodiscard]] rx_err_t rx_bus_smbus_write_word_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint16_t          data);

/**
 * @brief Read 16-bit word from SMBus register (Read Word Data protocol)
 *
 * @details
 * Implements SMBus Read Word Data protocol: reads 16-bit value from register
 * in **little-endian** byte order. Commonly used for voltage, current,
 * temperature, and capacity readings from battery fuel gauges.
 *
 * **Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] Sr Addr Rd [A] [DataLow] A [DataHigh] A [PEC] NA P
 * ```
 *
 * **Byte Order**: LITTLE-ENDIAN (LSB first)
 * ```
 * Wire receives: 0x34 (low), 0x12 (high)
 * Result: 0x1234
 * ```
 *
 * **Algorithm**:
 * 1. Validate inputs
 * 2. START + Address + Write
 * 3. Send command
 * 4. Repeated START + Address + Read
 * 5. Receive data low byte
 * 6. Receive data high byte
 * 7. Receive and verify PEC (if enabled)
 * 8. Combine bytes: result = low | (high << 8)
 * 9. STOP
 * 10. Return result in *data
 *
 * **Performance**: ~400 µs @ 100 kHz (4 bytes + PEC overhead)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Register/command address
 *                    - **Valid range**: 0x00-0xFF (device-dependent)
 *                    - **Units**: Dimensionless (register address)
 *
 * @param[out] data Pointer to store received 16-bit word
 *                  - **Valid range**: Non-NULL
 *                  - **Constraints**: Must point to valid uint16_t
 *                  - **Null handling**: Returns k_rx_err_null_ptr if nullptr
 *                  - **Byte order**: Little-endian converted to host order
 *
 * @return rx_err_t Error code
 *
 * @retval k_rx_ok Success. *data contains valid 16-bit value.
 * @retval k_rx_err_null_ptr manager, bus_name, or data is nullptr.
 * @retval k_rx_err_not_found Bus not found in manager.
 * @retval k_rx_err_invalid_state Bus not initialized.
 * @retval k_rx_err_timeout SMBus timeout (device not responding).
 * @retval k_rx_err_nack Device sent NACK.
 * @retval k_rx_err_crc_mismatch PEC verification failed (data corrupted).
 *
 * @pre Bus initialized via rx_bus_smbus_init()
 * @pre Device powered and ready
 *
 * @post *data contains 16-bit register value (on success)
 * @post Value converted from little-endian to host byte order
 *
 * @note **Thread Safety**: Thread-safe via bus manager mutex
 * @note **Performance**: ~400 µs @ 100 kHz
 * @note **Byte Order**: Automatic little-endian conversion
 *
 * @warning PEC errors indicate data corruption - do NOT use data value
 *
 * @par Example 1: Read battery voltage (BQ4050)
 * @code{.c}
 * uint16_t voltage_mv;
 * rx_err_t err = rx_bus_smbus_read_word_data(&bus_manager,
 *                                            "fuel_gauge",
 *                                            0x09,           // Voltage register
 *                                            &voltage_mv);
 * if (err == k_rx_err_crc_mismatch) {
 *     // PEC failed - retry
 *     rx_log_warn("SMBUS", "PEC error, retrying...");
 *     tx_thread_sleep(5);
 *     err = rx_bus_smbus_read_word_data(&bus_manager, "fuel_gauge",
 *                                       0x09, &voltage_mv);
 * }
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Voltage read failed");
 *
 * float voltage_v = voltage_mv / 1000.0f;
 * rx_log_info("SMBUS", "Battery: %.2f V", voltage_v);
 * @endcode
 *
 * @par Example 2: Read current (BQ4050)
 * @code{.c}
 * uint16_t current_raw;
 * rx_err_t err = rx_bus_smbus_read_word_data(&bus_manager,
 *                                            "fuel_gauge",
 *                                            0x0A,  // Current register
 *                                            &current_raw);
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Current read failed");
 *
 * // BQ4050 current is signed 16-bit in mA
 * int16_t current_ma = (int16_t)current_raw;
 * float current_a = current_ma / 1000.0f;
 * rx_log_info("SMBUS", "Current: %.3f A %s",
 *             fabsf(current_a),
 *             (current_ma >= 0) ? "(charging)" : "(discharging)");
 * @endcode
 *
 * @par Example 3: Read temperature with error handling
 * @code{.c}
 * uint16_t temp_raw;
 * rx_err_t err;
 *
 * // Retry up to 3 times on communication errors
 * for (uint8_t retry = 0; retry < 3; retry++) {
 *     err = rx_bus_smbus_read_word_data(&bus_manager, "fuel_gauge",
 *                                       0x08, &temp_raw);
 *     if (err == k_rx_ok) break;
 *
 *     if (err == k_rx_err_crc_mismatch || err == k_rx_err_timeout) {
 *         rx_log_warn("SMBUS", "Read error %d, retry %d/3", err, retry + 1);
 *         tx_thread_sleep(10);  // 10ms delay before retry
 *     } else {
 *         break;  // Fatal error, don't retry
 *     }
 * }
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Temperature read failed after 3 retries");
 *
 * // BQ4050 temperature is in 0.1K units
 * float temp_k = temp_raw / 10.0f;
 * float temp_c = temp_k - 273.15f;
 * rx_log_info("SMBUS", "Temperature: %.1f C", temp_c);
 * @endcode
 *
 * @see rx_bus_smbus_write_word_data() Write 16-bit word to register
 * @see rx_bus_smbus_read_byte_data() Read 8-bit value
 * @see rx_bus_smbus_read_block_data() Read multi-byte data
 * @see rx_bq4050.h for BQ4050-specific register definitions
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 5 assertions (NULL×3, bus found, initialized)
 * - Rule 7: ✓ Returns rx_err_t for caller validation
 */
[[nodiscard]] rx_err_t rx_bus_smbus_read_word_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint16_t*         data);

/**
 * @brief Read block data from SMBus device (Block Read protocol)
 *
 * @details
 * Implements SMBus Block Read protocol: reads variable-length data (1-32 bytes)
 * from device. First byte received is the block length, followed by data bytes.
 *
 * **Wire Format**:
 * ```
 * S Addr Wr [A] Command [A] Sr Addr Rd [A] [Count] A [Data1] A ... [DataN] A [PEC] NA P
 *
 * Count = Number of data bytes (1-32)
 * ```
 *
 * **Algorithm**:
 * 1. Validate inputs (manager, bus_name, data, length pointers)
 * 2. Validate max_length >= 1 and <= 32
 * 3. START + Address + Write
 * 4. Send command byte
 * 5. Repeated START + Address + Read
 * 6. Receive count byte (number of data bytes)
 * 7. Validate count <= max_length
 * 8. Loop: Receive count data bytes, send ACK after each
 * 9. If PEC enabled: Receive PEC byte, verify CRC
 * 10. STOP
 * 11. Store count in *length
 * 12. Return k_rx_ok
 *
 * **PEC Coverage**: Address+W, Command, Address+R, Count, Data[0..N-1]
 *
 * **Performance**: 1-3 ms depending on block size @ 100 kHz
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[in] command Register/command address
 * @param[out] data Pointer to buffer for received data
 *                  - **Valid range**: Non-NULL
 *                  - **Constraints**: Must have space for max_length bytes
 *
 * @param[out] length Pointer to store actual number of bytes read
 *                    - **Valid range**: Non-NULL
 *                    - **Constraints**: Will be set to 0-max_length
 *
 * @param[in] max_length Maximum buffer size (typically 32)
 *                       - **Valid range**: 1-32 bytes (SMBus spec limit)
 *                       - **Units**: bytes
 *                       - **Constraints**: Must match data buffer size
 *
 * @return rx_err_t Error code
 *
 * @retval k_rx_ok Success. *length and *data valid.
 * @retval k_rx_err_null_ptr NULL pointer parameter.
 * @retval k_rx_err_not_found Bus not found.
 * @retval k_rx_err_invalid_state Bus not initialized.
 * @retval k_rx_err_timeout SMBus timeout.
 * @retval k_rx_err_nack Device NACK.
 * @retval k_rx_err_crc_mismatch PEC verification failed.
 *
 * @pre Bus initialized
 * @pre data buffer has space for max_length bytes
 *
 * @post *length contains actual bytes read (0 to max_length)
 * @post data[0..length-1] contains received data
 *
 * @note **Thread Safety**: Thread-safe via mutex
 * @note **Performance**: ~1-3 ms depending on block size
 *
 * @warning Ensure data buffer is large enough for max_length bytes
 *
 * @par Example: Read manufacturer name (BQ4050)
 * @code{.c}
 * uint8_t name_buf[32];
 * uint8_t name_len;
 *
 * rx_err_t err = rx_bus_smbus_read_block_data(&bus_manager,
 *                                             "fuel_gauge",
 *                                             0x20,      // ManufacturerName
 *                                             name_buf,
 *                                             &name_len,
 *                                             sizeof(name_buf));
 * RX_RETURN_ON_ERROR(err, "SMBUS", "Block read failed");
 *
 * // name_buf contains ASCII string, name_len is string length
 * rx_log_info("SMBUS", "Manufacturer: %.*s", name_len, name_buf);
 * @endcode
 *
 * @see rx_bus_smbus_read_word_data() For fixed-size reads
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 2: ✓ Loop bound is statically known (max_length, max 32)
 * - Rule 5: ✓ 6 assertions (NULL×4, bus found, max_length valid)
 */
[[nodiscard]] rx_err_t rx_bus_smbus_read_block_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t*          data,
                                      uint8_t*          length,
                                      uint8_t           max_length);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_SMBUS_H */
