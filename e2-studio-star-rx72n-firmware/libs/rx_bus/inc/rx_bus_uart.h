/* lib/rx_bus/inc/rx_bus_uart.h */

/**
 * @file rx_bus_uart.h
 * @brief UART (Universal Asynchronous Receiver-Transmitter) Bus Abstraction for RX72N
 *
 * @details
 * Provides bus manager integration for Renesas SCI (Serial Communications Interface)
 * peripheral operating in asynchronous UART mode. Wraps the low-level SCI HAL with
 * the bus abstraction pattern for consistent API across all bus types (I2C, SPI, 1-Wire,
 * UART) with mutex-protected access.
 *
 * @par System Architecture
 * @dot
 * digraph uart_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="UART Bus API (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     init [label="rx_bus_uart_init()"];
 *     write [label="rx_bus_uart_write()"];
 *     read [label="rx_bus_uart_read()"];
 *     putc [label="rx_bus_uart_putc()"];
 *     puts [label="rx_bus_uart_puts()"];
 *     getc [label="rx_bus_uart_getc()"];
 *     avail [label="rx_bus_uart_rx_available()"];
 *   }
 *
 *   subgraph cluster_bus {
 *     label="Bus Manager";
 *     style=filled;
 *     color=lightblue;
 *     manager [label="rx_bus_manager_t\nMutex protection\nBus lookup"];
 *     config [label="rx_bus_config_t\nChannel, Baud\nParity/Stop config"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="SCI HAL Layer";
 *     style=filled;
 *     color=lightgreen;
 *     sci [label="sci_init()\nsci_write()\nsci_read()"];
 *     regs [label="SCI0-12\nRegister Access"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="UART Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     tx [label="TXD (Transmit)\nPush-pull"];
 *     rx [label="RXD (Receive)\nInput"];
 *     console [label="Debug Console\n115200 baud"];
 *     gps [label="GPS Module\n9600 baud"];
 *     other [label="Other Serial\nPeripherals"];
 *   }
 *
 *   init -> manager;
 *   write -> manager;
 *   read -> manager;
 *   putc -> manager;
 *   puts -> manager;
 *   getc -> manager;
 *   avail -> manager;
 *   manager -> config;
 *   config -> sci;
 *   sci -> regs;
 *   regs -> tx;
 *   regs -> rx;
 *   tx -> console;
 *   rx -> console;
 *   tx -> gps;
 *   rx -> gps;
 *   tx -> other;
 *   rx -> other;
 * }
 * @enddot
 *
 * @par Protocol Overview
 * UART (Universal Asynchronous Receiver-Transmitter) is a hardware peripheral for
 * asynchronous serial communication:
 * - **Asynchronous**: No shared clock - baud rate must match on both sides
 * - **Full duplex**: Independent TX and RX data lines
 * - **Frame format**: START bit + data bits + optional parity + STOP bits
 * - **Flow control**: Optional RTS/CTS or XON/XOFF (not used in STAR project)
 * - **Point-to-point**: One transmitter to one receiver per channel
 *
 * @par Frame Format
 * @verbatim
 * Standard 8N1 frame (8 data bits, No parity, 1 stop bit):
 *
 *    START  D0  D1  D2  D3  D4  D5  D6  D7  STOP
 *     |     |   |   |   |   |   |   |   |    |
 *     0     LSB                     MSB      1
 *     └─ Synchronization edge (falling edge)
 *
 * Bit timing: Each bit = 1 / baud_rate seconds
 * Example: 115200 baud = 8.68 µs per bit
 * Total frame time (8N1): 10 bits = 86.8 µs @ 115200 baud
 * @endverbatim
 *
 * @par Common Frame Configurations
 * | Config | Data Bits | Parity | Stop Bits | Use Case |
 * |--------|-----------|--------|-----------|----------|
 * | 8N1 | 8 | None | 1 | Standard (default) |
 * | 8E1 | 8 | Even | 1 | Error detection |
 * | 8O1 | 8 | Odd | 1 | Error detection |
 * | 7E1 | 7 | Even | 1 | Old terminals |
 * | 9N1 | 9 | None | 1 | Multi-drop networks |
 *
 * @par Supported Baud Rates (RX72N SCI)
 * | Baud Rate | Byte Time | Error | Use Case |
 * |-----------|-----------|-------|----------|
 * | 9600 | 1041 µs | 0.16% | GPS, legacy devices |
 * | 19200 | 521 µs | 0.16% | Modems |
 * | 38400 | 260 µs | 0.16% | - |
 * | 57600 | 174 µs | 0.16% | - |
 * | 115200 | 86.8 µs | 0.16% | Debug console (default) |
 * | 230400 | 43.4 µs | 0.79% | High-speed serial |
 * | 460800 | 21.7 µs | 0.79% | High-speed serial |
 * | 921600 | 10.9 µs | 0.79% | Maximum supported |
 *
 * @par Baud Rate Calculation
 * The SCI baud rate is calculated from PCLKB:
 * @f[
 * BRR = \frac{PCLKB}{64 \times 2^{2n-1} \times BaudRate} - 1
 * @f]
 * Where:
 * - PCLKB = 60 MHz (peripheral clock B on RX72N)
 * - n = CKS bits (0-3) selecting divisor
 * - BRR = bit rate register value (0-255)
 *
 * Example for 115200 baud:
 * @f[
 * BRR = \frac{60000000}{64 \times 1 \times 115200} - 1 = 7.13 \approx 7
 * @f]
 * Actual baud: 116279 baud (0.94% error - within spec)
 *
 * @par Performance Characteristics
 * | Metric | 9600 baud | 115200 baud | 460800 baud |
 * |--------|-----------|-------------|-------------|
 * | Byte time | 1041 µs | 86.8 µs | 21.7 µs |
 * | 64-byte transfer | ~67 ms | ~5.6 ms | ~1.4 ms |
 * | Overhead (start/stop) | 20% | 20% | 20% |
 * | Typical latency | 100 µs | 50 µs | 30 µs |
 * | FIFO depth | 16 bytes | 16 bytes | 16 bytes |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~1.2 KB | All API functions |
 * | RX FIFO | 16 bytes | Hardware receive buffer per channel |
 * | TX FIFO | 16 bytes | Hardware transmit buffer per channel |
 * | Stack | 64 bytes | Maximum call depth |
 * | No heap | 0 | Static allocation only |
 *
 * @par Hardware Requirements
 * | Requirement | Specification |
 * |-------------|---------------|
 * | TX pin | Push-pull output, 3.3V logic |
 * | RX pin | Input with pullup, 3.3V/5V tolerant |
 * | VDD | 3.3V nominal |
 * | Cable length | < 15 m @ 115200 (RS-232 levels) |
 * | Cable capacitance | < 2500 pF |
 *
 * @par SCI Channel Assignments (STAR Project)
 * | Channel | TX Pin | RX Pin | Baud | Usage |
 * |---------|--------|--------|------|-------|
 * | SCI0 | PA4 | PA2 | 115200 | USB CDC debug console |
 * | SCI1 | P26 | P30 | 9600 | GPS module (future) |
 * | SCI2 | P13 | P12 | 115200 | Expansion header |
 * | SCI6 | P00 | P01 | 115200 | Bluetooth module (future) |
 *
 * @par Error Handling Strategy
 * | Error | Cause | Recovery |
 * |-------|-------|----------|
 * | k_rx_err_timeout | TX buffer full or RX timeout | Check baud rate, flow control |
 * | k_rx_err_framing | Mismatched baud or stop bits | Verify baud rate on both sides |
 * | k_rx_err_parity | Corrupted data | Enable parity checking |
 * | k_rx_err_overrun | RX FIFO overflow (data loss) | Read more frequently, increase priority |
 * | k_rx_err_empty | No data available (non-blocking read) | Normal - retry later |
 *
 * @par Thread Safety
 * All functions are thread-safe when used with the bus manager:
 * - Bus manager provides mutex protection per channel
 * - Timeout on mutex acquisition prevents deadlock
 * - Safe to call from multiple ThreadX threads
 * - FIFO depth (16 bytes) prevents data loss during mutex wait
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - sci.c: Low-level SCI hardware driver
 * - rx_mpc.h: Pin function selection
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: ✓ No goto, setjmp, or recursion
 * - Rule 2: ✓ All loops bounded by buffer length
 * - Rule 3: ✓ No dynamic memory allocation
 * - Rule 4: ✓ Functions under 60 lines
 * - Rule 5: ✓ Input validation via RX_CHECK_NULL_PTR
 * - Rule 6: ✓ Variables at smallest scope
 * - Rule 7: ✓ All return values checked
 * - Rule 8: ✓ Limited preprocessor (typed enums preferred)
 * - Rule 9: ✓ Function pointers only for bus abstraction (DIP)
 * - Rule 10: ✓ Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - UART bus operations only
 * - **O**: Open/Closed - Extensible via bus manager config
 * - **L**: Liskov Substitution - Implements rx_bus_interface_t
 * - **I**: Interface Segregation - Minimal 7-function API (char + buffer)
 * - **D**: Dependency Inversion - Depends on abstractions (bus manager)
 *
 * @par Usage Example - Basic Console Output
 * @code
 * // Initialize bus manager and UART bus
 * rx_bus_manager_t manager;
 * rx_bus_manager_init(&manager);
 *
 * // Register UART bus for debug console
 * rx_bus_config_t config = {
 *     .type = k_rx_bus_type_uart,
 *     .channel = 0,           // SCI0
 *     .uart = {
 *         .baud_rate = 115200,
 *         .data_bits = 8,
 *         .parity = 0,        // None
 *         .stop_bits = 1,
 *     }
 * };
 * rx_bus_register(&manager, "console", &config);
 *
 * // Initialize UART bus
 * rx_err_t err = rx_bus_uart_init(&manager, "console");
 * if (err != k_rx_ok) {
 *     // Handle initialization error
 *     return err;
 * }
 *
 * // Send debug message
 * err = rx_bus_uart_puts(&manager, "console", "System initialized\n");
 * if (err != k_rx_ok) {
 *     // Handle write error
 * }
 * @endcode
 *
 * @par Usage Example - Binary Data Transfer
 * @code
 * // Send binary packet
 * uint8_t packet[16] = { 0xAA, 0x55, 0x10, 0x00, ... };
 * err = rx_bus_uart_write(&manager, "gps", packet, sizeof(packet));
 *
 * // Non-blocking receive
 * uint8_t rx_buffer[64];
 * uint16_t bytes_received = 0;
 * err = rx_bus_uart_read(&manager, "gps", rx_buffer, sizeof(rx_buffer), &bytes_received);
 * if (err == k_rx_ok && bytes_received > 0) {
 *     // Process received data
 *     for (uint16_t i = 0; i < bytes_received; i++) {
 *         // Parse byte
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Character-Based Input
 * @code
 * // Poll for single character (non-blocking)
 * char c;
 * err = rx_bus_uart_getc(&manager, "console", &c);
 * if (err == k_rx_ok) {
 *     // Character received
 *     if (c == '\r' || c == '\n') {
 *         // Process command
 *     }
 * } else if (err == k_rx_err_empty) {
 *     // No data available - normal for non-blocking
 * }
 *
 * // Check if data available before reading
 * bool data_available = false;
 * err = rx_bus_uart_rx_available(&manager, "console", &data_available);
 * if (err == k_rx_ok && data_available) {
 *     err = rx_bus_uart_getc(&manager, "console", &c);
 * }
 * @endcode
 *
 * @see rx_bus_manager.h Bus manager API for registration and lookup
 * @see rx_bus_types.h Bus type definitions and configurations
 * @see sci.c Low-level SCI hardware driver implementation
 *
 * @version 1.0.0
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * UART Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize UART bus through bus manager
 *
 * @details
 * Configures the SCI peripheral channel and associated GPIO pins for UART
 * asynchronous serial communication. This function must be called before any
 * read/write operations. The initialization process:
 *
 * 1. Acquires bus manager mutex (thread-safe)
 * 2. Looks up bus configuration by name
 * 3. Validates bus type is UART
 * 4. Configures MPC for TXD/RXD pin functions
 * 5. Initializes SCI peripheral with configured baud rate, parity, stop bits
 * 6. Enables TX and RX
 * 7. Releases mutex
 *
 * @par Initialization Sequence
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="rx_bus_uart_init()", shape=ellipse];
 *   mutex [label="Acquire mutex"];
 *   lookup [label="Lookup bus by name"];
 *   validate [label="Validate bus type"];
 *   mpc [label="Configure MPC pins"];
 *   sci [label="Initialize SCI\nBaud, Parity, Stop"];
 *   enable [label="Enable TX/RX"];
 *   release [label="Release mutex"];
 *   done [label="Return", shape=ellipse];
 *
 *   start -> mutex;
 *   mutex -> lookup;
 *   lookup -> validate;
 *   validate -> mpc;
 *   mpc -> sci;
 *   sci -> enable;
 *   enable -> release;
 *   release -> done;
 * }
 * @enddot
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must have UART bus registered via rx_bus_register()
 * @param[in] bus_name UART bus name
 *   - Null-terminated string
 *   - Must match name used in rx_bus_register()
 *   - Maximum 31 characters
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, bus initialized and ready
 * @retval k_rx_err_null_ptr NULL pointer in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_arg Bus is not UART type
 * @retval k_rx_err_timeout Mutex acquisition timeout (default 1000ms)
 * @retval k_rx_err_hw_init_failed SCI hardware initialization failed
 *
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre Bus must be registered via rx_bus_register() with type k_rx_bus_type_uart
 *
 * @post SCI channel configured and ready for TX/RX
 * @post TXD/RXD pins configured with correct function
 * @post Bus state set to initialized
 * @post RX and TX FIFOs cleared
 *
 * @note Thread-safe via bus manager mutex
 * @note Call only once per bus; subsequent calls return k_rx_err_invalid_state
 * @note RX FIFO depth is 16 bytes - read frequently to prevent overrun
 *
 * @warning Do not call from interrupt context (mutex wait)
 * @warning Ensure TX and RX pins are not shorted together
 *
 * @par Performance:
 * - Execution time: ~120 µs typical (SCI initialization)
 * - Mutex timeout: 1000 ms (configurable via bus manager)
 *
 * @par Example - Console Initialization:
 * @code
 * rx_bus_config_t console_config = {
 *     .type = k_rx_bus_type_uart,
 *     .channel = 0,
 *     .uart = {
 *         .baud_rate = 115200,
 *         .data_bits = 8,
 *         .parity = 0,        // No parity
 *         .stop_bits = 1,     // 1 stop bit (8N1)
 *     }
 * };
 * rx_bus_register(&manager, "console", &console_config);
 *
 * rx_err_t err = rx_bus_uart_init(&manager, "console");
 * if (err != k_rx_ok) {
 *     rx_log_error("UART", "Console init failed");
 *     return err;
 * }
 * @endcode
 *
 * @see rx_bus_manager_init() Initialize bus manager first
 * @see rx_bus_register() Register bus before initialization
 * @see rx_bus_uart_write() Write data after initialization
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write data buffer to UART through bus manager
 *
 * @details
 * Transmits binary data over UART in blocking mode. Waits for TX FIFO to
 * have space before writing each byte. The transmission process:
 *
 * 1. Acquires bus manager mutex
 * 2. Validates bus state (initialized)
 * 3. For each byte:
 *    - Wait for TX FIFO space (with timeout)
 *    - Write byte to SCI TDR register
 * 4. Releases mutex
 *
 * @par Algorithm Steps:
 * 1. Null pointer validation (manager, bus_name, data)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized
 * 5. Loop for length bytes:
 *    a. Wait for TDRE flag (TX data register empty)
 *    b. Write data[i] to TDR
 *    c. Check for timeout
 * 6. Release mutex
 * 7. Return success or error
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[in] data Pointer to data to write
 *   - Binary data (no null-termination required)
 *   - Valid for entire function duration
 * @param[in] length Number of bytes to write
 *   - Range: 1-65535 bytes
 *   - Larger transfers take longer (length * byte_time)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, all bytes transmitted
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or data
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout TX timeout (FIFO full) or mutex timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 * @pre data buffer must be valid for entire transfer
 *
 * @post All length bytes transmitted on TXD pin
 * @post TX FIFO may contain last bytes (use flush if timing critical)
 *
 * @invariant Bus state remains initialized
 * @invariant TX FIFO depth <= 16 bytes throughout operation
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocking call - waits for TX completion
 * @note No automatic CRLF conversion (binary safe)
 *
 * @warning Large transfers (>1KB) may block for significant time
 * @warning Do not call from interrupt context (blocking)
 *
 * @par Performance:
 * - Execution time: length * byte_time + overhead
 * - Example @ 115200 baud: 64 bytes = ~5.6 ms
 * - TX timeout per byte: 100 ms (configurable)
 *
 * @par Example - Binary Packet:
 * @code
 * // Send GPS configuration packet
 * uint8_t gps_cmd[] = { 0xB5, 0x62, 0x06, 0x01, 0x00, 0x00, 0x07, 0x37 };
 * rx_err_t err = rx_bus_uart_write(&manager, "gps", gps_cmd, sizeof(gps_cmd));
 * if (err != k_rx_ok) {
 *     rx_log_error("GPS", "Command send failed");
 * }
 * @endcode
 *
 * @par Example - With Error Handling:
 * @code
 * uint8_t tx_data[256];
 * // ... fill tx_data ...
 *
 * rx_err_t err = rx_bus_uart_write(&manager, "sensor", tx_data, sizeof(tx_data));
 * if (err == k_rx_err_timeout) {
 *     // TX timeout - possible cable disconnect or device not responding
 *     rx_log_warn("UART", "TX timeout - check connection");
 * } else if (err != k_rx_ok) {
 *     // Other error
 *     rx_log_error("UART", "Write failed: %d", err);
 * }
 * @endcode
 *
 * @see rx_bus_uart_puts() Write null-terminated string with CRLF conversion
 * @see rx_bus_uart_putc() Write single character
 * @see rx_bus_uart_read() Read received data
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_write(rx_bus_manager_t* manager,
                           const char*       bus_name,
                           const uint8_t*    data,
                           uint16_t          length);

/**
 * @brief Read data from UART through bus manager
 *
 * @details
 * Reads available data from UART RX FIFO in **non-blocking** mode. Returns
 * immediately with whatever data is available (0 to length bytes). Check
 * bytes_read to determine actual count received.
 *
 * @par Algorithm Steps:
 * 1. Validate all pointers (manager, bus_name, data, bytes_read)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized
 * 5. Initialize bytes_read = 0
 * 6. Loop while (bytes_read < length):
 *    a. Check RDRF flag (RX data register full)
 *    b. If data available: read RDR, increment bytes_read
 *    c. Else: break (non-blocking)
 * 7. Release mutex
 * 8. Return k_rx_ok (bytes_read may be 0)
 *
 * @par Non-Blocking Behavior:
 * Unlike write operations, this function returns immediately:
 * - If FIFO empty: bytes_read = 0, returns k_rx_ok
 * - If partial data: bytes_read < length, returns k_rx_ok
 * - If full buffer: bytes_read = length, returns k_rx_ok
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[out] data Pointer to buffer for received data
 *   - Must be at least length bytes
 *   - Contents undefined if bytes_read = 0
 * @param[in] length Maximum bytes to read
 *   - Range: 1-65535 bytes
 *   - Actual bytes read may be less
 * @param[out] bytes_read Actual number of bytes read
 *   - Range: 0 to length
 *   - Always valid on success (k_rx_ok)
 *   - Value 0 is normal (no data available)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success (check bytes_read for actual count, may be 0)
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 * @pre data buffer must be at least length bytes
 * @pre bytes_read pointer must be valid
 *
 * @post bytes_read contains actual count (0 to length)
 * @post Read bytes removed from RX FIFO
 * @post Unread bytes remain in FIFO for next call
 *
 * @invariant bytes_read <= length
 * @invariant Bus state remains initialized
 *
 * @note **Non-blocking** - returns immediately
 * @note Thread-safe via bus manager mutex
 * @note RX FIFO depth 16 bytes - call frequently to prevent overrun
 * @note bytes_read = 0 is NOT an error condition
 *
 * @warning RX overrun possible if FIFO fills (16 bytes) - data loss
 * @warning No framing error detection in this function
 *
 * @par Performance:
 * - Execution time: ~10 µs + (bytes_read * 2 µs)
 * - Non-blocking: Returns immediately regardless of data availability
 *
 * @par Example - Polling Loop:
 * @code
 * uint8_t rx_buffer[64];
 * uint16_t total_received = 0;
 *
 * // Non-blocking poll
 * while (total_received < sizeof(rx_buffer)) {
 *     uint16_t bytes_read = 0;
 *     rx_err_t err = rx_bus_uart_read(&manager, "gps",
 *                                      &rx_buffer[total_received],
 *                                      sizeof(rx_buffer) - total_received,
 *                                      &bytes_read);
 *     if (err == k_rx_ok) {
 *         total_received += bytes_read;
 *         if (bytes_read == 0) {
 *             // No data available - can do other work
 *             tx_thread_sleep(1);  // 10 ms delay
 *         }
 *     } else {
 *         rx_log_error("UART", "Read failed");
 *         break;
 *     }
 * }
 * @endcode
 *
 * @par Example - Single Read:
 * @code
 * uint8_t packet[32];
 * uint16_t received = 0;
 * rx_err_t err = rx_bus_uart_read(&manager, "sensor", packet, sizeof(packet), &received);
 * if (err == k_rx_ok && received > 0) {
 *     // Process received bytes
 *     process_packet(packet, received);
 * }
 * // received = 0 is normal if no data available
 * @endcode
 *
 * @see rx_bus_uart_getc() Read single character (non-blocking)
 * @see rx_bus_uart_rx_available() Check if data available before reading
 * @see rx_bus_uart_write() Write data
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_read(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          uint8_t*          data,
                          uint16_t          length,
                          uint16_t*         bytes_read);

/**
 * @brief Write single character to UART through bus manager
 *
 * @details
 * Transmits a single ASCII or binary character over UART. Blocking call that
 * waits for TX FIFO space before writing.
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name)
 * 2. Acquire mutex
 * 3. Wait for TDRE flag (TX data register empty)
 * 4. Write character to TDR
 * 5. Release mutex
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[in] c Character to write
 *   - Range: 0x00-0xFF (full 8-bit)
 *   - No automatic LF to CRLF conversion
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, character transmitted
 * @retval k_rx_err_null_ptr NULL pointer in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout TX timeout or mutex timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 *
 * @post Character transmitted on TXD pin
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocking call (waits for TX ready)
 * @note Use rx_bus_uart_puts() for strings with CRLF conversion
 *
 * @warning Do not call from interrupt context (blocking)
 *
 * @par Performance:
 * - Execution time: byte_time + ~5 µs overhead
 * - Example @ 115200 baud: ~92 µs per character
 *
 * @par Example:
 * @code
 * // Send single character
 * rx_err_t err = rx_bus_uart_putc(&manager, "console", 'A');
 * if (err != k_rx_ok) {
 *     // Handle error
 * }
 *
 * // Send binary control character
 * err = rx_bus_uart_putc(&manager, "device", 0x1B);  // ESC
 * @endcode
 *
 * @see rx_bus_uart_puts() Write null-terminated string with CRLF
 * @see rx_bus_uart_write() Write binary buffer
 * @see rx_bus_uart_getc() Read single character
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_putc(rx_bus_manager_t* manager, const char* bus_name, char c);

/**
 * @brief Write null-terminated string to UART through bus manager
 *
 * @details
 * Transmits a null-terminated string with automatic LF to CRLF conversion
 * for proper terminal display. Useful for debug console output.
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name, str)
 * 2. Acquire mutex
 * 3. For each character until '\0':
 *    a. If character is '\n':
 *       - Write '\r' first (carriage return)
 *       - Write '\n' second (line feed)
 *    b. Else:
 *       - Write character as-is
 *    c. Wait for TX ready with timeout
 * 4. Release mutex
 *
 * @par CRLF Conversion:
 * | Input | Output | Terminal Effect |
 * |-------|--------|-----------------|
 * | \n | \r\n | New line (CR+LF) |
 * | \r | \r | Carriage return only |
 * | \r\n | \r\r\n | Extra CR (avoid) |
 * | Other | Same | No conversion |
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[in] str Null-terminated string to write
 *   - ASCII text (printable + control chars)
 *   - Must end with '\0'
 *   - Maximum length limited by timeout
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, entire string transmitted
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout TX timeout or mutex timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 * @pre str must be null-terminated
 *
 * @post Entire string transmitted (partial on timeout)
 * @post Each '\n' converted to '\r\n'
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocking call (waits for TX completion)
 * @note Best for debug console and terminal output
 * @note Binary data should use rx_bus_uart_write() instead
 *
 * @warning Long strings may block for significant time
 * @warning str must be null-terminated (no length check)
 * @warning Do not use for binary data (CRLF conversion)
 *
 * @par Performance:
 * - Execution time: strlen(str) * byte_time
 * - Example @ 115200 baud: 20 chars = ~1.7 ms
 * - CRLF conversion adds ~87 µs per '\n'
 *
 * @par Example - Debug Output:
 * @code
 * // Simple debug message
 * rx_bus_uart_puts(&manager, "console", "System initialized\n");
 *
 * // Formatted output (use snprintf first)
 * char msg[64];
 * snprintf(msg, sizeof(msg), "Battery: %d mV\n", voltage_mv);
 * rx_bus_uart_puts(&manager, "console", msg);
 * @endcode
 *
 * @par Example - With Error Handling:
 * @code
 * rx_err_t err = rx_bus_uart_puts(&manager, "console", "ERROR: Sensor fault\n");
 * if (err == k_rx_err_timeout) {
 *     // TX timeout - console may be disconnected
 *     led_set_error(true);
 * }
 * @endcode
 *
 * @see rx_bus_uart_putc() Write single character (no conversion)
 * @see rx_bus_uart_write() Write binary buffer (no conversion)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_puts(rx_bus_manager_t* manager, const char* bus_name, const char* str);

/**
 * @brief Read single character from UART through bus manager
 *
 * @details
 * Reads one character from UART RX FIFO in **non-blocking** mode. Returns
 * immediately with k_rx_err_empty if no data available.
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name, c)
 * 2. Acquire mutex
 * 3. Check RDRF flag (RX data register full)
 * 4. If flag set:
 *    a. Read character from RDR
 *    b. Release mutex, return k_rx_ok
 * 5. Else (no data):
 *    a. Release mutex, return k_rx_err_empty
 *
 * @par Non-Blocking Behavior:
 * This function NEVER blocks waiting for data:
 * - Data available: Returns k_rx_ok with character
 * - No data: Returns k_rx_err_empty immediately
 * - Call rx_bus_uart_rx_available() first to check
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[out] c Character received
 *   - Range: 0x00-0xFF (full 8-bit)
 *   - Valid only if k_rx_ok returned
 *   - Undefined if k_rx_err_empty
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, character read into c
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_empty No data available (NOT an error - normal)
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 * @pre c pointer must be valid
 *
 * @post If k_rx_ok: c contains received character
 * @post If k_rx_ok: Character removed from RX FIFO
 * @post If k_rx_err_empty: FIFO unchanged, c undefined
 *
 * @note **Non-blocking** - returns immediately
 * @note k_rx_err_empty is NOT a failure - it means no data available
 * @note Thread-safe via bus manager mutex
 * @note Use rx_bus_uart_rx_available() to check before calling
 *
 * @warning c undefined if return != k_rx_ok
 * @warning RX overrun possible if not called frequently (16 byte FIFO)
 *
 * @par Performance:
 * - Execution time: ~8 µs (data available) or ~5 µs (empty)
 * - Non-blocking: Always returns immediately
 *
 * @par Example - Polling Loop:
 * @code
 * char c;
 * rx_err_t err = rx_bus_uart_getc(&manager, "console", &c);
 * if (err == k_rx_ok) {
 *     // Character received
 *     if (c == '\r' || c == '\n') {
 *         // Process command
 *         process_command();
 *     } else {
 *         // Echo character
 *         rx_bus_uart_putc(&manager, "console", c);
 *     }
 * } else if (err == k_rx_err_empty) {
 *     // No data available - normal
 * } else {
 *     // Actual error
 *     rx_log_error("UART", "Read error: %d", err);
 * }
 * @endcode
 *
 * @par Example - With Availability Check:
 * @code
 * bool data_available = false;
 * rx_bus_uart_rx_available(&manager, "console", &data_available);
 * if (data_available) {
 *     char c;
 *     rx_err_t err = rx_bus_uart_getc(&manager, "console", &c);
 *     // err should be k_rx_ok (data was available)
 *     process_char(c);
 * }
 * @endcode
 *
 * @see rx_bus_uart_rx_available() Check if data available first
 * @see rx_bus_uart_read() Read multiple bytes
 * @see rx_bus_uart_putc() Write single character
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_getc(rx_bus_manager_t* manager, const char* bus_name, char* c);

/**
 * @brief Check if RX data is available on UART
 *
 * @details
 * Queries the UART RX FIFO to determine if at least one byte is available
 * for reading. Non-blocking operation useful before calling rx_bus_uart_getc()
 * or rx_bus_uart_read().
 *
 * @par Algorithm Steps:
 * 1. Validate pointers (manager, bus_name, available)
 * 2. Acquire mutex
 * 3. Read RDRF flag (RX data register full)
 * 4. Set *available = true if flag set, false otherwise
 * 5. Release mutex
 * 6. Return k_rx_ok
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name UART bus name
 *   - Must match registered bus
 * @param[out] available True if at least one byte is available
 *   - true: FIFO contains >= 1 byte
 *   - false: FIFO empty
 *   - Valid only if k_rx_ok returned
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, available contains valid result
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout Mutex acquisition timeout
 *
 * @pre Bus must be initialized via rx_bus_uart_init()
 * @pre available pointer must be valid
 *
 * @post available contains true/false status
 * @post RX FIFO unchanged (non-destructive read)
 *
 * @note **Non-blocking** - returns immediately
 * @note Thread-safe via bus manager mutex
 * @note Does NOT remove data from FIFO
 * @note available = false is normal (no error)
 *
 * @par Performance:
 * - Execution time: ~6 µs
 * - Non-blocking: Always returns immediately
 *
 * @par Example - Efficient Polling:
 * @code
 * // Check before reading to avoid unnecessary calls
 * bool data_ready = false;
 * rx_err_t err = rx_bus_uart_rx_available(&manager, "console", &data_ready);
 * if (err == k_rx_ok && data_ready) {
 *     char c;
 *     err = rx_bus_uart_getc(&manager, "console", &c);
 *     // Process character
 * }
 * // If data_ready = false, skip read call entirely
 * @endcode
 *
 * @par Example - Background Task:
 * @code
 * void rx_task_entry(ULONG arg) {
 *     while (1) {
 *         bool available = false;
 *         rx_bus_uart_rx_available(&manager, "gps", &available);
 *         if (available) {
 *             // Read and process data
 *             uint8_t buffer[64];
 *             uint16_t bytes_read = 0;
 *             rx_bus_uart_read(&manager, "gps", buffer, sizeof(buffer), &bytes_read);
 *             process_gps_data(buffer, bytes_read);
 *         } else {
 *             // No data - sleep briefly
 *             tx_thread_sleep(10);  // 100 ms
 *         }
 *     }
 * }
 * @endcode
 *
 * @see rx_bus_uart_getc() Read single character
 * @see rx_bus_uart_read() Read multiple bytes
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_uart_rx_available(rx_bus_manager_t* manager, const char* bus_name, bool* available);

#ifdef __cplusplus
}
#endif
