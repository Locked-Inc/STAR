/**
 * @file rx_usb.h
 * @brief Multi-Port USB CDC-ACM Composite Device Driver API for RX72N
 *
 * @details
 * ## Overview
 * Production-quality USB CDC-ACM (Communications Device Class - Abstract Control
 * Model) composite device driver for RX72N USB0 peripheral. Provides 3 independent
 * virtual serial ports over a single USB connection, enabling simultaneous binary
 * protocol communication, ASCII debugging output, and log streaming.
 *
 * When connected to USB host (Raspberry Pi 5), the RX72N appears as multiple
 * `/dev/ttyACM*` devices on Linux, each with independent TX/RX ring buffers and
 * event callbacks.
 *
 * ## System Architecture
 *
 * @dot
 * digraph usb_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_host {
 *     label="Raspberry Pi 5 (USB Host)";
 *     style=filled;
 *     color=lightblue;
 *     app_proto [label="Application\n(nanopb)"];
 *     app_debug [label="Debug Tool\n(minicom)"];
 *     app_log [label="Log Viewer\n(tail -f)"];
 *     ttyACM0 [label="/dev/ttyACM0\nProtocol Port"];
 *     ttyACM1 [label="/dev/ttyACM1\nDecoded Port"];
 *     ttyACM2 [label="/dev/ttyACM2\nLog Port"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N (USB Function/Peripheral)";
 *     style=filled;
 *     color=lightgreen;
 *     usb_api [label="rx_usb API\n(This Module)"];
 *     cdc_proto [label="CDC-ACM Port 0\nRX: 1024B\nTX: 1024B"];
 *     cdc_decoded [label="CDC-ACM Port 1\nRX: 256B\nTX: 512B"];
 *     cdc_log [label="CDC-ACM Port 2\nRX: 256B\nTX: 1024B"];
 *     usb_hw [label="USB0 Peripheral\nHardware"];
 *   }
 *
 *   app_proto -> ttyACM0 [label="read/write"];
 *   app_debug -> ttyACM1 [label="read/write"];
 *   app_log -> ttyACM2 [label="read"];
 *
 *   ttyACM0 -> cdc_proto [label="USB Bulk\nTransfers"];
 *   ttyACM1 -> cdc_decoded [label="USB Bulk\nTransfers"];
 *   ttyACM2 -> cdc_log [label="USB Bulk\nTransfers"];
 *
 *   usb_api -> cdc_proto;
 *   usb_api -> cdc_decoded;
 *   usb_api -> cdc_log;
 *
 *   cdc_proto -> usb_hw;
 *   cdc_decoded -> usb_hw;
 *   cdc_log -> usb_hw;
 * }
 * @enddot
 *
 * ## USB Device State Machine
 *
 * @startuml
 * [*] --> Detached : Power On
 *
 * Detached : VBUS = Low
 * Detached : No cable connected
 * Detached --> Attached : VBUS Detected
 *
 * Attached : VBUS = High
 * Attached : Waiting for bus reset
 * Attached --> Detached : VBUS Lost
 * Attached --> Powered : Internal transition
 *
 * Powered : Pull-up resistor enabled
 * Powered : Host detects device
 * Powered --> Default : USB Bus Reset
 * Powered --> Detached : VBUS Lost
 *
 * Default : Address = 0
 * Default : Enumeration starts
 * Default --> Addressed : SET_ADDRESS
 * Default --> Powered : USB Bus Reset
 * Default --> Detached : VBUS Lost
 *
 * Addressed : Unique address assigned
 * Addressed : Host reads descriptors
 * Addressed --> Configured : SET_CONFIGURATION
 * Addressed --> Default : USB Bus Reset
 * Addressed --> Detached : VBUS Lost
 *
 * Configured : All 3 ports ready
 * Configured : Data transfer enabled
 * Configured --> Suspended : Host Suspend
 * Configured --> Addressed : SET_CONFIGURATION(0)
 * Configured --> Default : USB Bus Reset
 * Configured --> Detached : VBUS Lost
 *
 * Suspended : Low power mode
 * Suspended : All transfers halted
 * Suspended --> Configured : Host Resume
 * Suspended --> Detached : VBUS Lost
 *
 * note right of Configured
 *   Only in this state can
 *   rx_usb_write/read transfer data
 * end note
 * @enduml
 *
 * ## Port Assignment and Usage
 *
 * | Port | ID | Linux Device | Purpose | RX Buf | TX Buf | Typical Usage |
 * |------|----|--------------|---------||--------|--------|---------------|
 * | **Protocol** | 0 | /dev/ttyACM0 | Binary protocol (nanopb) | 1024 | 1024 | Motor commands, telemetry, frame protocol |
 * | **Decoded** | 1 | /dev/ttyACM1 | ASCII frame dumps | 256 | 512 | Debugging, frame inspection, development |
 * | **Log** | 2 | /dev/ttyACM2 | Log output (mirrored UART) | 256 | 1024 | Runtime logging, diagnostics, errors |
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **USB Speed** | 12 Mbps | Full-Speed (USB 2.0) |
 * | **Effective Throughput** | ~1.2 MB/s | Bulk transfers, 64-byte packets |
 * | **Latency (write)** | 1-2 ms | Time from rx_usb_write() to host |
 * | **Latency (read)** | 1-5 ms | Depends on host poll rate |
 * | **Enumeration Time** | ~200 ms | Cable plug to Configured state |
 * | **Max Packet Size** | 64 bytes | Full-Speed bulk endpoint limit |
 * | **Ports Supported** | 3 | Limited by pipe count (9 pipes / 3 per CDC) |
 * | **Total Buffer RAM** | 4352 bytes | Sum of all port RX+TX buffers |
 *
 * **Throughput Analysis:**
 * - **Theoretical max**: 12 Mbps = 1.5 MB/s
 * - **Practical (bulk)**: ~1.2 MB/s (80% efficiency due to protocol overhead)
 * - **Per-port max**: ~400 KB/s (limited by ring buffer fill rate)
 *
 * ## Memory Usage
 *
 * **Static Memory (Global):**
 * - **Port buffers**: 4352 bytes (1024+1024 + 256+512 + 256+1024)
 * - **USB descriptors**: ~512 bytes (device, config, interface, endpoint)
 * - **State structures**: ~128 bytes (3 ports x ~42 bytes per port)
 * - **Total static**: ~5000 bytes (~5 KB)
 *
 * **Stack Usage per Call:**
 * - `rx_usb_write()`: ~32 bytes (locals + ring buffer manipulation)
 * - `rx_usb_read()`: ~32 bytes (locals + ring buffer manipulation)
 * - `rx_usb_isr_handler()`: ~128 bytes (ISR context + USB register access)
 *
 * ## Hardware Requirements
 *
 * **RX72N USB0 Peripheral:**
 * - **Pins**: USB_DP (PA4), USB_DM (PA5), USB_VBUS (PA6)
 * - **Clock**: 48 MHz USB clock (from PLL)
 * - **Interrupt**: USB0 interrupt (vector 90, priority 5 recommended)
 * - **DMA**: Optional for high-throughput transfers (not currently used)
 *
 * **External Hardware:**
 * - **VBUS detection**: PA6 (USB_VBUS) with internal comparator
 * - **Pull-up resistor**: Internal 1.5kOhm on USB_DP (D+ line)
 * - **No external PHY required** (internal transceiver)
 *
 * ## Thread Safety
 *
 * | Function | Thread Safe? | Notes |
 * |----------|--------------|-------|
 * | `rx_usb_init()` | [FAIL] No | Call once during init, single-threaded |
 * | `rx_usb_write()` | [FAIL] No | Use external mutex if multiple threads write to same port |
 * | `rx_usb_read()` | [FAIL] No | Use external mutex if multiple threads read from same port |
 * | `rx_usb_is_configured()` | [PASS] Yes | Read-only hardware state check |
 * | `rx_usb_get_state()` | [PASS] Yes | Read-only hardware state check |
 * | `rx_usb_isr_handler()` | ISR | Called from interrupt context, do not call directly |
 *
 * **Recommended Pattern**: Dedicate one ThreadX task per port for read/write operations.
 *
 * ## USB CDC-ACM Protocol Details
 *
 * **USB Descriptors:**
 * - **Device Descriptor**: VID/PID, class=0xEF (Miscellaneous), subclass=0x02 (IAD)
 * - **Configuration Descriptor**: Contains 3 CDC-ACM functions
 * - **Interface Association Descriptors (IAD)**: Groups each CDC function (2 interfaces each)
 * - **CDC Functional Descriptors**: Header, ACM, Union, Call Management
 * - **Endpoint Descriptors**: Interrupt (control) + 2x Bulk (data) per port
 *
 * **CDC-ACM Class Requests (Handled by Driver):**
 * - `SET_LINE_CODING` (0x20): Host configures baud rate, parity, stop bits
 * - `GET_LINE_CODING` (0x21): Host queries current line coding
 * - `SET_CONTROL_LINE_STATE` (0x22): DTR/RTS control signals (ignored)
 *
 * **Note**: Line coding (baud rate, etc.) is informational only. USB CDC operates
 * at USB speed (12 Mbps), not the configured baud rate. These settings are stored
 * but have no effect on actual transfer rate.
 *
 * ## Error Handling Strategy
 *
 * **Recoverable Errors** (return error code, retry possible):
 * - `k_rx_err_busy`: TX buffer full, wait or poll `rx_usb_tx_available()`
 * - `k_rx_err_timeout`: Flush timeout, expected if host not reading
 * - `k_rx_err_invalid_state`: Port not configured, wait for enumeration
 *
 * **Fatal Errors** (require re-initialization):
 * - `k_rx_err_null_ptr`: Programming error, fix code
 * - `k_rx_err_invalid_arg`: Invalid port ID, fix code
 *
 * **USB Events (Handled via Callback):**
 * - `k_usb_event_detached`: Cable unplugged, stop transfers
 * - `k_usb_event_reset`: Bus reset, state machine returns to Default
 * - `k_usb_event_configured`: Port ready, can start transfers
 *
 * ## Integration Example - Complete ThreadX Application
 *
 * @code{.c}
 * #include "rx_usb.h"
 * #include "tx_api.h"
 *
 * // USB event callback (ISR context)
 * void usb_event_callback(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx)
 * {
 *   (void)ctx;
 *
 *   switch (event) {
 *     case k_usb_event_configured:
 *       rx_log_info("USB", "Port %d configured", port);
 *       break;
 *     case k_usb_event_detached:
 *       rx_log_warn("USB", "USB cable detached");
 *       break;
 *     case k_usb_event_data_rx:
 *       // Signal task to read data
 *       tx_event_flags_set(&g_usb_events, (1 << port), TX_OR);
 *       break;
 *     default:
 *       break;
 *   }
 * }
 *
 * // Initialize USB during system startup
 * void system_init(void)
 * {
 *   // Configure USB event callback
 *   rx_usb_config_t config = {
 *     .callback = usb_event_callback,
 *     .ctx =nullptr
 *   };
 *
 *   rx_err_t err = rx_usb_init(&config);
 *   if (err != k_rx_ok) {
 *     rx_log_error("INIT", "USB init failed: %d", err);
 *     return;
 *   }
 *
 *   rx_log_info("INIT", "USB initialized, waiting for host...");
 * }
 *
 * // ThreadX task for protocol port communication
 * void usb_protocol_task_entry(ULONG thread_input)
 * {
 *   (void)thread_input;
 *
 *   // Wait for USB enumeration
 *   while (!rx_usb_is_configured(k_usb_port_proto)) {
 *     tx_thread_sleep(10);  // 100ms polling
 *   }
 *
 *   rx_log_info("USB", "Protocol port ready");
 *
 *   // Main communication loop
 *   while (1) {
 *     // Check for incoming data
 *     uint32_t available;
 *     rx_usb_rx_available(k_usb_port_proto, &available);
 *
 *     if (available > 0) {
 *       // Read frame data
 *       uint8_t rx_buf[256];
 *       uint32_t rx_len;
 *       rx_err_t err = rx_usb_read(k_usb_port_proto, rx_buf, sizeof(rx_buf), &rx_len);
 *
 *       if (err == k_rx_ok && rx_len > 0) {
 *         // Process received frame (nanopb decode, etc.)
 *         process_protocol_frame(rx_buf, rx_len);
 *
 *         // Send response
 *         uint8_t tx_buf[128];
 *         uint32_t tx_len = build_response(tx_buf, sizeof(tx_buf));
 *         rx_usb_write(k_usb_port_proto, tx_buf, tx_len);
 *       }
 *     }
 *
 *     // Send periodic telemetry
 *     if (telemetry_ready) {
 *       uint8_t telemetry[64];
 *       uint32_t len = build_telemetry(telemetry, sizeof(telemetry));
 *       rx_usb_write(k_usb_port_proto, telemetry, len);
 *     }
 *
 *     tx_thread_sleep(1);  // 10ms loop rate
 *   }
 * }
 *
 * // ThreadX task for log port output
 * void usb_log_task_entry(ULONG thread_input)
 * {
 *   (void)thread_input;
 *
 *   while (!rx_usb_is_configured(k_usb_port_log)) {
 *     tx_thread_sleep(10);
 *   }
 *
 *   rx_usb_puts(k_usb_port_log, "=== STAR System Log ===\r\n");
 *
 *   while (1) {
 *     // Send log messages (also mirrored to UART)
 *     if (log_message_pending) {
 *       char log_buf[128];
 *       format_log_message(log_buf, sizeof(log_buf));
 *       rx_usb_puts(k_usb_port_log, log_buf);
 *     }
 *
 *     tx_thread_sleep(5);  // 50ms log polling
 *   }
 * }
 * @endcode
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|---------------------|
 * | 1. Simple control flow | [PASS] Pass | No goto/setjmp/recursion, structured control flow only |
 * | 2. Fixed loop bounds | [PASS] Pass | All loops bounded by constants (k_usb_max_buffer_size) |
 * | 3. No dynamic memory | [PASS] Pass | Zero malloc/free, all buffers statically allocated |
 * | 4. Short functions | [PASS] Pass | All <60 lines, single responsibility |
 * | 5. Assertions | [PASS] Pass | Minimum 2 checks per function (NULL + state validation) |
 * | 6. Small scope | [PASS] Pass | Variables declared near use, static for module data |
 * | 7. Check returns | [PASS] Pass | All error codes checked or explicitly cast to (void) |
 * | 8. Limited preprocessor | [PASS] Pass | C23 typed enums for constants, minimal macros |
 * | 9. Restrict pointers | [WARN] Deviation | Function pointers for callbacks (DIP, event-driven) |
 * | 10. Compiler warnings | [PASS] Pass | -Wall -Wextra -Werror, zero warnings |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Module handles ONLY USB CDC transport, delegates to ring buffers, USB hardware layer |
 * | **Open/Closed** | Configurable via rx_usb_config_t (callbacks, context), extendable via events |
 * | **Liskov Substitution** | Not applicable (no inheritance in C), but consistent API across all ports |
 * | **Interface Segregation** | Focused API: init/deinit, read/write, status queries, text helpers (4 groups) |
 * | **Dependency Inversion** | Depends on abstractions: rx_err_t (error handling), callbacks (event notification) |
 *
 * ## References
 *
 * - **RX72N Manual**: Section 32 (USB 2.0 Full-Speed Module)
 * - **USB CDC-ACM Spec**: USB Class Definitions for Communications Devices v1.2
 * - **USB 2.0 Spec**: Chapter 9 (Device Framework), Chapter 5 (Electrical)
 * - **IAD ECN**: Interface Association Descriptor Engineering Change Notice
 * - **Renesas App Note**: R01AN2025 - RX Family USB Basic Driver
 *
 * @see rx_usb.c Implementation file
 * @see rx_usb_cdc.c CDC-ACM class implementation
 * @see rx_usb_hw.c USB peripheral hardware abstraction
 * @see docs/sections/03_hardware_pinout.tex USB pinout and connections
 *
 * @since Version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Port Identification
 * =============================================================================
 */

/**
 * @enum rx_usb_port_id_t
 * @brief USB CDC-ACM virtual serial port identifiers
 *
 * @details
 * Defines the 3 independent CDC-ACM virtual serial ports provided by the
 * USB composite device. Each port has dedicated TX/RX ring buffers, endpoints,
 * and appears as a separate `/dev/ttyACM*` device on Linux hosts.
 *
 * **Design Rationale:**
 * - **3 ports**: Separates protocol, debug, and logging traffic for clarity
 * - **Port 0 (Protocol)**: High bandwidth, binary data, critical path
 * - **Port 1 (Decoded)**: Medium bandwidth, ASCII text, development/debugging
 * - **Port 2 (Log)**: High TX, low RX, one-way logging output
 * - **Hardware limit**: RX72N USB0 has 9 pipes; each CDC needs 3 (1 interrupt + 2 bulk)
 *
 * **Host Device Mapping** (Linux):
 * ```
 * RX72N Port    ->  Host Device      Typical Usage
 * ===============================================================
 * k_usb_port_proto   ->  /dev/ttyACM0   nanopb protocol, motor commands
 * k_usb_port_decoded ->  /dev/ttyACM1   ASCII frame dumps, debugging
 * k_usb_port_log     ->  /dev/ttyACM2   Log output, diagnostics
 * ```
 *
 * **Port Assignment Example:**
 * @code{.c}
 * // Send motor command on protocol port
 * uint8_t cmd_frame[64];
 * encode_motor_command(cmd_frame, ...);
 * rx_usb_write(k_usb_port_proto, cmd_frame, 64);
 *
 * // Send ASCII debug info on decoded port
 * rx_usb_puts(k_usb_port_decoded, "Frame: type=0x01 seq=123\r\n");
 *
 * // Send log message on log port
 * rx_usb_puts(k_usb_port_log, "[INFO] Motor initialized\r\n");
 * @endcode
 *
 * @see rx_usb_write() Send data to a port
 * @see rx_usb_read() Receive data from a port
 * @see rx_usb_is_configured() Check if port is ready
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Protocol port for binary communication (nanopb frames)
   * @details
   * **Purpose**: High-speed binary protocol communication using Protocol Buffers.
   * **Host device**: /dev/ttyACM0 (Linux), COM3 (Windows - example)
   * **RX buffer**: 1024 bytes (largest, for incoming commands)
   * **TX buffer**: 1024 bytes (largest, for telemetry bursts)
   * **Typical data**: Motor commands, sensor telemetry, configuration
   * **Frame format**: Custom protocol with sync, length, CRC-32
   * @par Usage Example:
   * @code{.c}
   * uint8_t motor_cmd[32];
   * build_motor_command(motor_cmd, 1.5f);  // 1.5 m/s velocity
   * rx_usb_write(k_usb_port_proto, motor_cmd, 32);
   * @endcode
   * @see k_usb_port_proto_rx_size, k_usb_port_proto_tx_size
   */
  k_usb_port_proto = 0,

  /**
   * @brief Decoded port for ASCII frame dumps and debugging
   * @details
   * **Purpose**: Human-readable ASCII representation of protocol frames.
   * **Host device**: /dev/ttyACM1 (Linux)
   * **RX buffer**: 256 bytes (minimal, rarely used for input)
   * **TX buffer**: 512 bytes (medium, for ASCII text output)
   * **Typical data**: Frame dumps, hex dumps, debug strings
   * **Format**: Plain ASCII text with \r\n line endings
   * @par Usage Example:
   * @code{.c}
   * char debug_msg[128];
   * snprintf(debug_msg, sizeof(debug_msg),
   *          "RX Frame: seq=%d len=%d type=0x%02X\r\n",
   *          frame.seq, frame.len, frame.type);
   * rx_usb_puts(k_usb_port_decoded, debug_msg);
   * @endcode
   * @see k_usb_port_decoded_rx_size, k_usb_port_decoded_tx_size
   */
  k_usb_port_decoded = 1,

  /**
   * @brief Log port for system logging (mirrored to UART)
   * @details
   * **Purpose**: Runtime logging, diagnostics, error messages.
   * **Host device**: /dev/ttyACM2 (Linux)
   * **RX buffer**: 256 bytes (minimal, not typically used for input)
   * **TX buffer**: 1024 bytes (largest, buffers log bursts)
   * **Typical data**: [INFO], [WARN], [ERROR] log messages
   * **Format**: Timestamped ASCII log lines with \r\n
   * **Mirroring**: All data also sent to UART (SCI1) for debugging without USB
   * @par Usage Example:
   * @code{.c}
   * rx_usb_puts(k_usb_port_log, "[INFO] System initialized\r\n");
   * rx_usb_puts(k_usb_port_log, "[WARN] Motor current high: ");
   * rx_usb_putint(k_usb_port_log, current_ma);
   * rx_usb_puts(k_usb_port_log, " mA\r\n");
   * @endcode
   * @see k_usb_port_log_rx_size, k_usb_port_log_tx_size
   */
  k_usb_port_log = 2,

  /**
   * @brief Number of active USB CDC-ACM ports
   * @details
   * **Value**: 3 (protocol + decoded + log)
   * **Usage**: Loop bounds, array sizing, validation
   * @par Value: 3
   * @par Usage in Code:
   * @code{.c}
   * for (uint8_t port = 0; port < k_usb_port_count; port++) {
   *   rx_usb_reset_stats((rx_usb_port_id_t)port);
   * }
   * @endcode
   */
  k_usb_port_count = 3,

  /**
   * @brief Maximum number of ports supported by hardware
   * @details
   * **Value**: 3 (limited by RX72N USB0 pipe count)
   * **Hardware constraint**: USB0 has 9 pipes total (pipe 0 = control)
   * **CDC requirement**: 3 pipes per CDC function (1 interrupt + 2 bulk)
   * **Calculation**: (9 pipes - 1 control) / 3 pipes per CDC = 2.67 -> 3 max
   * **Future**: Cannot add more ports without major architectural change
   * @par Value: 3
   * @par Rationale: USB0 pipe limit (see RX72N manual section 32.2.4)
   */
  k_usb_port_max = 3,

} rx_usb_port_id_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum rx_usb_event_t
 * @brief USB event types delivered to user callback function
 *
 * @details
 * Defines all asynchronous USB events that can trigger the user-provided
 * callback function (rx_usb_callback_t). Events are generated by the USB
 * interrupt handler and delivered from ISR context.
 *
 * **Event Categories:**
 * - **Connection events**: Attached, Detached (cable physical state)
 * - **Enumeration events**: Reset, Configured (USB state machine transitions)
 * - **Power events**: Suspended, Resumed (host power management)
 * - **Data events**: Data RX, TX Complete (transfer completion)
 *
 * **Event Timing:**
 * ```
 * Cable Plug -> Attached (immediate)
 *           -> Reset (20-100ms)
 *           -> Configured (100-200ms)
 *           -> Data RX/TX (ongoing)
 *           -> Suspended (host idle >3ms)
 *           -> Resumed (host activity)
 * Cable Unplug -> Detached (immediate)
 * ```
 *
 * **Callback Constraints:**
 * - **ISR context**: Callback runs in USB interrupt, keep fast (<10 us)
 * - **No blocking**: Do not call tx_thread_sleep(), tx_mutex_get(), etc.
 * - **Safe operations**: Set flags, post semaphores, queue messages
 *
 * @par Example Callback Implementation:
 * @code{.c}
 * TX_EVENT_FLAGS_GROUP g_usb_events;
 *
 * void usb_callback(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx)
 * {
 *   switch (event) {
 *     case k_usb_event_configured:
 *       // Signal task that port is ready
 *       tx_event_flags_set(&g_usb_events, (1 << port), TX_OR);
 *       break;
 *
 *     case k_usb_event_data_rx:
 *       // Signal task to read data (don't read here - ISR context!)
 *       tx_event_flags_set(&g_usb_events, (1 << (port + 8)), TX_OR);
 *       break;
 *
 *     case k_usb_event_detached:
 *       // Mark all ports as disconnected
 *       g_usb_connected = false;
 *       break;
 *
 *     default:
 *       break;
 *   }
 * }
 * @endcode
 *
 * @see rx_usb_callback_t Callback function type
 * @see rx_usb_set_callback() Register callback
 * @see rx_usb_config_t Configuration with callback
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief USB cable attached (VBUS detected)
   * @details
   * **Trigger**: VBUS voltage rises above threshold (~4.4V)
   * **Hardware**: PA6 (USB_VBUS) pin detects voltage
   * **State change**: Detached -> Attached
   * **Timing**: Immediate (<1ms after cable plug)
   * **Typical action**: Log event, prepare for enumeration
   * **Note**: Enumeration starts automatically after this event
   */
  k_usb_event_attached = 0,

  /**
   * @brief USB cable detached (VBUS lost)
   * @details
   * **Trigger**: VBUS voltage drops below threshold (~4.0V)
   * **State change**: Any state -> Detached
   * **Timing**: Immediate (<1ms after cable unplug)
   * **Typical action**: Abort transfers, reset state, log event
   * **Critical**: All ports become unusable, rx_usb_write() will fail
   */
  k_usb_event_detached = 1,

  /**
   * @brief USB bus reset received from host
   * @details
   * **Trigger**: Host drives D+/D- to SE0 (both low) for >10ms
   * **State change**: Any state -> Default (address 0)
   * **Timing**: 10-50ms after cable attach, or at any time
   * **Typical action**: Log event, wait for re-enumeration
   * **Note**: Normal during initial enumeration and host driver reload
   */
  k_usb_event_reset = 2,

  /**
   * @brief Port configured by host (ready for data transfer)
   * @details
   * **Trigger**: Host sends SET_CONFIGURATION request (successful)
   * **State change**: Addressed -> Configured
   * **Timing**: 100-200ms after cable attach (end of enumeration)
   * **Typical action**: Signal tasks, start data transfers
   * **Critical**: Only after this event can rx_usb_write/read succeed
   * **Per-port**: Event delivered once per port (3 events total)
   * @par Example:
   * @code{.c}
   * if (event == k_usb_event_configured && port == k_usb_port_proto) {
   *   // Protocol port ready, start communication
   *   g_protocol_port_ready = true;
   * }
   * @endcode
   */
  k_usb_event_configured = 3,

  /**
   * @brief Host suspended the device (USB idle >3ms)
   * @details
   * **Trigger**: No USB activity (SOF packets) for 3ms
   * **State change**: Configured -> Suspended
   * **Timing**: 3-10ms after host goes idle
   * **Typical action**: Enter low-power mode, stop transfers
   * **USB spec requirement**: Device must reduce current to <2.5mA
   * **Note**: Rare in normal operation, typically only when host sleeps
   */
  k_usb_event_suspended = 4,

  /**
   * @brief Host resumed the device (USB activity detected)
   * @details
   * **Trigger**: USB activity detected (SOF packets resume)
   * **State change**: Suspended -> Configured
   * **Timing**: <10ms after host wakes
   * **Typical action**: Exit low-power mode, resume transfers
   */
  k_usb_event_resumed = 5,

  /**
   * @brief Data received from host (RX buffer has new data)
   * @details
   * **Trigger**: Bulk OUT transfer completed, data in RX ring buffer
   * **Timing**: 1-5ms after host writes to /dev/ttyACM*
   * **Typical action**: Signal task to call rx_usb_read()
   * **Critical**: Do NOT call rx_usb_read() from callback (ISR context)
   * **Per-port**: Event specific to the port that received data
   * @par Example:
   * @code{.c}
   * if (event == k_usb_event_data_rx) {
   *   // Signal task to read data (don't read in ISR!)
   *   tx_event_flags_set(&g_rx_flags, (1 << port), TX_OR);
   * }
   * @endcode
   */
  k_usb_event_data_rx = 6,

  /**
   * @brief TX buffer drained (all queued data sent to host)
   * @details
   * **Trigger**: Last byte from TX ring buffer transferred to host
   * **Timing**: Variable (depends on host read rate, buffer size)
   * **Typical action**: Signal tx_usb_flush() completion, log milestone
   * **Usage**: Synchronization point for critical message delivery
   * **Per-port**: Event specific to the port whose TX completed
   * @par Example:
   * @code{.c}
   * if (event == k_usb_event_tx_complete && port == k_usb_port_log) {
   *   // Log TX buffer empty, safe to enter low power
   *   g_log_tx_idle = true;
   * }
   * @endcode
   */
  k_usb_event_tx_complete = 7,

} rx_usb_event_t;

/**
 * @enum rx_usb_state_t
 * @brief USB device state machine states (USB 2.0 standard states)
 *
 * @details
 * Represents the USB device state as defined by USB 2.0 specification Chapter 9.
 * State transitions are managed automatically by the USB interrupt handler based
 * on host actions and VBUS status.
 *
 * **State Machine** (see file header @startuml diagram for visual):
 * ```
 * Detached -> Attached -> Powered -> Default -> Addressed -> Configured
 *                                     ^          v            v
 *                                     <--- Reset --           v
 *                                                             v
 *                                                        Suspended
 *                                                             v
 *                                                        (Resume) -> Configured
 * ```
 *
 * **State Transition Table:**
 *
 * | Current State | Event/Condition | Next State | Action |
 * |---------------|-----------------|------------|--------|
 * | Detached | VBUS detected | Attached | Enable pull-up resistor |
 * | Attached | Auto transition | Powered | Wait for bus reset |
 * | Powered | USB bus reset | Default | Set address to 0 |
 * | Default | SET_ADDRESS | Addressed | Store assigned address |
 * | Addressed | SET_CONFIGURATION | Configured | Enable endpoints |
 * | Configured | USB suspend (3ms idle) | Suspended | Halt transfers |
 * | Suspended | USB resume signal | Configured | Resume transfers |
 * | Any state | VBUS lost | Detached | Disable all endpoints |
 *
 * **Critical States for Application:**
 * - **Configured**: ONLY state where rx_usb_write/read work
 * - **Detached**: All USB operations fail
 * - **Other states**: Enumeration in progress, wait for Configured
 *
 * @par Example - Wait for Enumeration:
 * @code{.c}
 * // Wait for device to reach Configured state
 * while (rx_usb_get_state() != k_usb_state_configured) {
 *   tx_thread_sleep(10);  // Poll every 100ms
 * }
 *
 * // Alternative: Wait for specific port
 * while (!rx_usb_is_configured(k_usb_port_proto)) {
 *   tx_thread_sleep(10);
 * }
 * @endcode
 *
 * @see rx_usb_get_state() Query current state
 * @see rx_usb_is_configured() Check if port ready (preferred)
 * @see rx_usb_event_t Events that trigger state changes
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief No USB cable connected (VBUS = 0V)
   * @details
   * **Entry condition**: VBUS voltage drops below 4.0V (cable unplugged)
   * **Characteristics**:
   * - All USB operations disabled
   * - Pull-up resistor disabled (host cannot detect device)
   * - Minimal power consumption
   * **Valid operations**: rx_usb_init(), rx_usb_deinit()
   * **Invalid operations**: rx_usb_write(), rx_usb_read() (return k_rx_err_invalid_state)
   * **Exit**: VBUS detected -> Attached
   */
  k_usb_state_detached = 0,

  /**
   * @brief Cable connected, VBUS detected (VBUS > 4.4V)
   * @details
   * **Entry condition**: VBUS voltage rises above 4.4V
   * **Characteristics**:
   * - Pull-up resistor enabled on D+ (1.5kOhm, signals Full-Speed)
   * - Host detects device connection
   * - Waiting for host to initiate bus reset
   * **Duration**: Typically 10-50ms
   * **Exit**: Auto-transition -> Powered, or VBUS lost -> Detached
   */
  k_usb_state_attached = 1,

  /**
   * @brief Powered by USB, waiting for enumeration
   * @details
   * **Entry condition**: Internal transition from Attached
   * **Characteristics**:
   * - Device draws power from VBUS
   * - Host preparing to enumerate device
   * - Waiting for USB bus reset
   * **Duration**: Typically <10ms
   * **Exit**: Bus reset -> Default, or VBUS lost -> Detached
   */
  k_usb_state_powered = 2,

  /**
   * @brief Bus reset received, device at address 0
   * @details
   * **Entry condition**: Host drives D+/D- to SE0 for >10ms (bus reset)
   * **Characteristics**:
   * - Device address = 0 (default address)
   * - Only control endpoint (EP0) enabled
   * - Host reads device/configuration descriptors
   * - Host assigns unique address via SET_ADDRESS
   * **Duration**: 50-100ms (descriptor enumeration)
   * **Exit**: SET_ADDRESS -> Addressed, or bus reset -> Default (restart)
   */
  k_usb_state_default = 3,

  /**
   * @brief Unique address assigned by host
   * @details
   * **Entry condition**: Host sends SET_ADDRESS request (address 1-127)
   * **Characteristics**:
   * - Device responds to assigned address (not address 0)
   * - Control endpoint active
   * - Host continues reading descriptors
   * - Host selects configuration via SET_CONFIGURATION
   * **Duration**: 20-50ms
   * **Exit**: SET_CONFIGURATION -> Configured, or bus reset -> Default
   */
  k_usb_state_addressed = 4,

  /**
   * @brief Configuration set, all ports ready for data transfer
   * @details
   * **Entry condition**: Host sends SET_CONFIGURATION request
   * **Characteristics**:
   * - All 3 CDC-ACM ports fully configured
   * - All bulk endpoints enabled (6 bulk + 3 interrupt)
   * - rx_usb_write/read operations ALLOWED
   * - Normal operation state
   * **Duration**: Indefinite (until suspend, reset, or detach)
   * **Exit**: Suspend -> Suspended, bus reset -> Default, VBUS lost -> Detached
   * **CRITICAL**: This is the ONLY state where data transfer works
   * @par Example:
   * @code{.c}
   * if (rx_usb_get_state() == k_usb_state_configured) {
   *   // Safe to send/receive data
   *   rx_usb_write(k_usb_port_proto, data, len);
   * }
   * @endcode
   */
  k_usb_state_configured = 5,

  /**
   * @brief Host suspended device (USB idle >3ms)
   * @details
   * **Entry condition**: No USB activity (SOF packets) for 3 consecutive ms
   * **Characteristics**:
   * - All data transfers halted
   * - Device must reduce current to <2.5mA (USB spec requirement)
   * - rx_usb_write/read operations BLOCKED
   * - Waiting for resume signal from host
   * **Duration**: Variable (until host resumes or cable unplug)
   * **Exit**: Resume signal -> Configured, VBUS lost -> Detached
   * **Note**: Rare in normal operation, typically only when host computer sleeps
   */
  k_usb_state_suspended = 6,

} rx_usb_state_t;

/**
 * @brief USB callback function type (per-port)
 *
 * Callbacks are invoked from ISR context. Keep callback code fast and
 * non-blocking. Do not call blocking functions from the callback.
 *
 * @param port The port that generated the event
 * @param event The USB event that occurred
 * @param ctx User-provided context pointer
 */
typedef void (*rx_usb_callback_t)(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx);

/**
 * @brief USB configuration structure
 */
typedef struct {
  rx_usb_callback_t callback; /**< Event callback function (optional) */
  void*             ctx;      /**< User context for callback */
} rx_usb_config_t;

/**
 * @brief USB CDC line coding structure (baud rate, stop bits, etc.)
 *
 * This structure matches the USB CDC SetLineCoding/GetLineCoding format.
 */
typedef struct {
  uint32_t baud_rate; /**< Data terminal rate in bits per second */
  uint8_t  stop_bits; /**< 0=1 stop bit, 1=1.5 stop bits, 2=2 stop bits */
  uint8_t  parity;    /**< 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space */
  uint8_t  data_bits; /**< Data bits (5, 6, 7, 8, or 16) */
} rx_usb_line_coding_t;

/**
 * @brief USB statistics (per-port)
 */
typedef struct {
  uint32_t bytes_rx;     /**< Total bytes received from host */
  uint32_t bytes_tx;     /**< Total bytes transmitted to host */
  uint32_t rx_overruns;  /**< RX buffer overrun count */
  uint32_t tx_underruns; /**< TX underrun count (no data to send) */
  uint32_t bus_resets;   /**< USB bus reset count */
  uint32_t suspends;     /**< Suspend count */
} rx_usb_stats_t;

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief USB endpoint and packet configuration
 */
typedef enum : uint16_t {
  k_usb_max_packet_size = 64, /**< Full-speed bulk max packet size */
} rx_usb_packet_config_t;

/**
 * @brief Per-port buffer sizes
 *
 * Different ports may have different buffer requirements.
 * - Protocol port: larger buffers for binary data
 * - Decoded port: moderate buffers for ASCII frame dumps
 * - Log port: smaller RX (minimal), larger TX (buffering log output)
 */
typedef enum : uint16_t {
  k_usb_port_proto_rx_size   = 1024, /**< Protocol port RX buffer (bytes) */
  k_usb_port_proto_tx_size   = 1024, /**< Protocol port TX buffer (bytes) */
  k_usb_port_decoded_rx_size = 256,  /**< Decoded port RX buffer (bytes) */
  k_usb_port_decoded_tx_size = 512,  /**< Decoded port TX buffer (bytes) */
  k_usb_port_log_rx_size     = 256,  /**< Log port RX buffer (minimal) */
  k_usb_port_log_tx_size     = 1024, /**< Log port TX buffer (larger for buffering) */
  k_usb_max_buffer_size      = 1024, /**< Maximum buffer size (for loop bounds - NASA Rule 2) */
} rx_usb_port_buffer_sizes_t;

/**
 * @brief Default CDC line coding values
 *
 * These defaults are used during USB initialization before the host
 * sends SET_LINE_CODING. They represent a standard 115200 8N1 configuration.
 */
typedef enum : uint32_t {
  k_usb_default_baud_rate = 115200, /**< Default baud rate: 115200 bps */
  k_usb_default_stop_bits = 0,      /**< Default stop bits: 1 stop bit */
  k_usb_default_parity    = 0,      /**< Default parity: None */
  k_usb_default_data_bits = 8,      /**< Default data bits: 8 bits */
} rx_usb_line_coding_defaults_t;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize USB CDC composite device
 *
 * This function initializes the USB0 peripheral in function (device) mode
 * with multiple CDC-ACM interfaces. After initialization, the device is ready
 * to be connected to a USB host.
 *
 * All configured ports are initialized with their respective buffers.
 *
 * @param[in] config Configuration structure (NULL for defaults)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if already initialized
 */
[[nodiscard]] rx_err_t rx_usb_init(const rx_usb_config_t* config);

/**
 * @brief Deinitialize USB peripheral
 *
 * Stops USB operations and releases resources. The USB cable should be
 * disconnected before calling this function.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_usb_deinit(void);

/* =============================================================================
 * Port Status Functions
 * =============================================================================
 */

/**
 * @brief Check if a port is configured and ready
 *
 * This function returns true when the USB host has completed enumeration
 * and configuration for the specified port.
 *
 * @param[in] port Port to check
 * @return true if port is configured and ready for data transfer
 * @return false if not connected, not configured, or invalid port
 */
[[nodiscard]] bool rx_usb_is_configured(rx_usb_port_id_t port);

/**
 * @brief Get current USB device state
 *
 * Note: Device state is shared across all ports (USB is one physical device).
 *
 * @return Current USB device state
 */
rx_usb_state_t rx_usb_get_state(void);

/* =============================================================================
 * Data Transfer Functions (Non-Blocking, Async)
 *
 * All data transfer operations are non-blocking. Data is queued to ring
 * buffers and transferred asynchronously by the USB hardware.
 * =============================================================================
 */

/**
 * @brief Write data to a port's TX buffer (non-blocking)
 *
 * Data is copied to an internal ring buffer and transmitted to the host
 * asynchronously. This function returns immediately after queuing data.
 *
 * @param[in] port Target port
 * @param[in] data Data buffer to transmit
 * @param[in] len Number of bytes to transmit
 * @return k_rx_ok on success (all data queued)
 * @return k_rx_err_busy if TX buffer is full (partial or no data queued)
 * @return k_rx_err_invalid_state if port not configured
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if data is nullptr
 */
[[nodiscard]] rx_err_t rx_usb_write(rx_usb_port_id_t port, const uint8_t* data, uint32_t len);

/**
 * @brief Read data from a port's RX buffer (non-blocking)
 *
 * Reads available data from the internal ring buffer. This function returns
 * immediately with whatever data is available (may be 0 bytes).
 *
 * @param[in] port Source port
 * @param[out] data Output buffer for received data
 * @param[in] max_len Maximum number of bytes to read
 * @param[out] actual_len Actual number of bytes read (can be 0)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if data or actual_len is nullptr
 */
[[nodiscard]] rx_err_t
rx_usb_read(rx_usb_port_id_t port, uint8_t* data, uint32_t max_len, uint32_t* actual_len);

/**
 * @brief Check if RX data is available on a port
 *
 * @param[in] port Port to check
 * @param[out] available Number of bytes available in RX buffer
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if available is nullptr
 */
[[nodiscard]] rx_err_t rx_usb_rx_available(rx_usb_port_id_t port, uint32_t* available);

/**
 * @brief Check TX buffer space available on a port
 *
 * @param[in] port Port to check
 * @param[out] available Number of bytes free in TX buffer
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if available is nullptr
 */
[[nodiscard]] rx_err_t rx_usb_tx_available(rx_usb_port_id_t port, uint32_t* available);

/**
 * @brief Flush TX buffer (blocking with timeout)
 *
 * Blocks until all data in the TX buffer has been transmitted to the host,
 * or until the timeout expires.
 *
 * @param[in] port Port to flush
 * @param[in] timeout_ms Maximum time to wait in milliseconds (0 = no wait)
 * @return k_rx_ok on success (buffer empty)
 * @return k_rx_err_timeout if timeout expired before buffer was flushed
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_usb_flush(rx_usb_port_id_t port, uint32_t timeout_ms);

/* =============================================================================
 * Async Event Callback
 * =============================================================================
 */

/**
 * @brief Set event callback for a port
 *
 * The callback is invoked from ISR context when events occur (data received,
 * TX complete, configuration changes). Keep callback code fast and non-blocking.
 *
 * @param[in] port Port to set callback for
 * @param[in] callback Callback function (NULL to disable)
 * @param[in] ctx User context passed to callback
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 */
[[nodiscard]] rx_err_t
rx_usb_set_callback(rx_usb_port_id_t port, rx_usb_callback_t callback, void* ctx);

/* =============================================================================
 * Line Coding and Statistics
 * =============================================================================
 */

/**
 * @brief Get current CDC line coding settings for a port
 *
 * Returns the line coding (baud rate, stop bits, parity, data bits)
 * set by the host via SET_LINE_CODING request.
 *
 * @param[in] port Port to query
 * @param[out] line_coding Line coding structure to fill
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if line_coding is nullptr
 */
[[nodiscard]] rx_err_t rx_usb_get_line_coding(rx_usb_port_id_t      port,
                                              rx_usb_line_coding_t* line_coding);

/**
 * @brief Get USB statistics for a port
 *
 * @param[in] port Port to query
 * @param[out] stats Statistics structure to fill
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_null_ptr if stats is nullptr
 */
[[nodiscard]] rx_err_t rx_usb_get_stats(rx_usb_port_id_t port, rx_usb_stats_t* stats);

/**
 * @brief Reset USB statistics for a port
 *
 * @param[in] port Port to reset statistics for
 */
void rx_usb_reset_stats(rx_usb_port_id_t port);

/* =============================================================================
 * Debug Text Output Functions
 *
 * These functions provide UART-like text output over USB CDC for debugging.
 * They write ASCII text directly to the specified port.
 * Mirrors uart_debug_putc/uart_debug_puts/uart_debug_putint/uart_debug_puthex for consistency.
 * =============================================================================
 */

/**
 * @brief Write a single character to a port
 *
 * @param[in] port Target port
 * @param[in] c Character to write
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if port not configured
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_busy if TX buffer is full
 */
[[nodiscard]] rx_err_t rx_usb_putc(rx_usb_port_id_t port, char c);

/**
 * @brief Write a null-terminated string to a port
 *
 * @param[in] port Target port
 * @param[in] str String to write (must be null-terminated)
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if str is nullptr
 * @return k_rx_err_invalid_state if port not configured
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_busy if TX buffer is full (partial write)
 */
[[nodiscard]] rx_err_t rx_usb_puts(rx_usb_port_id_t port, const char* str);

/**
 * @brief Write a signed integer as decimal text to a port
 *
 * @param[in] port Target port
 * @param[in] value Integer value to write
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if port not configured
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_busy if TX buffer is full
 */
[[nodiscard]] rx_err_t rx_usb_putint(rx_usb_port_id_t port, int32_t value);

/**
 * @brief Write an unsigned integer as hexadecimal text to a port
 *
 * @param[in] port Target port
 * @param[in] value Value to write as hex
 * @param[in] digits Number of hex digits (1-8, zero-padded)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if port not configured
 * @return k_rx_err_invalid_arg if port is invalid
 * @return k_rx_err_busy if TX buffer is full
 */
[[nodiscard]] rx_err_t rx_usb_puthex(rx_usb_port_id_t port, uint32_t value, uint8_t digits);

/* =============================================================================
 * Internal Functions (for ISR and other USB modules)
 * =============================================================================
 */

/**
 * @brief USB interrupt handler
 *
 * This function should be called from the USB interrupt service routine.
 * It handles all USB events (enumeration, data transfer, etc.).
 *
 * @note This is called internally by the ISR; do not call directly.
 */
void rx_usb_isr_handler(void);

/**
 * @brief Mark USB hardware as already initialised externally.
 *
 * @details
 * Flip the internal `s_hw_initialized` flag to true so the next call to
 * rx_usb_hw_init() (via rx_usb_init) returns immediately without
 * re-running the clock / SYSCFG / DCP / INTENB0 / DPRPU sequence.
 * Intended for call sites that brought the peripheral up with an
 * equivalent inline register sequence (e.g. main()'s pre-kernel attach)
 * and now want the production driver state (ring buffers, CDC state,
 * s_usb.initialized) without disturbing live hardware.
 */
void rx_usb_hw_mark_initialized(void);

#ifdef __cplusplus
}
#endif
