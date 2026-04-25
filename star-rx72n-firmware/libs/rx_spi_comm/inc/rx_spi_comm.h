/**
 * @file rx_spi_comm.h
 * @brief High-Level SPI Frame Protocol Communication Layer for RX72N Peripheral Mode
 *
 * @details
 * ## Overview
 * Production-quality SPI communication layer providing reliable frame-based protocol
 * between RX72N MCU (SPI peripheral) and Raspberry Pi 5 (SPI controller). Integrates
 * frame encoding/decoding with CRC-32 validation, automatic sequence number management,
 * optional Forward Error Correction (FEC), and HARQ (Hybrid Automatic Repeat reQuest)
 * retransmission support.
 *
 * **Purpose**: Enable reliable, high-throughput bidirectional communication over SPI
 * hardware interface with automatic error detection, correction, and recovery mechanisms.
 *
 * ## System Architecture
 *
 * @dot
 * digraph spi_comm_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_app {
 *     label="Application Layer (RX72N)";
 *     style=filled;
 *     fillcolor=lightblue;
 *     app [label="Application Code\n(Command Handlers)"];
 *   }
 *
 *   subgraph cluster_comm {
 *     label="Communication Manager";
 *     style=filled;
 *     fillcolor=lightgreen;
 *     comm_mgr [label="rx_comm_manager\n(Channel Coordinator)"];
 *   }
 *
 *   subgraph cluster_spi {
 *     label="SPI Communication Layer (This Module)";
 *     style=filled;
 *     fillcolor=lightyellow;
 *     spi_send [label="rx_spi_comm_send()"];
 *     spi_recv [label="rx_spi_comm_receive()"];
 *     seq_mgmt [label="Sequence Management"];
 *   }
 *
 *   subgraph cluster_frame {
 *     label="Frame Protocol Layer";
 *     style=filled;
 *     fillcolor=lightcyan;
 *     encoder [label="rx_frame_encoder\n(CRC-32, Header)"];
 *     decoder [label="rx_frame_decoder\n(CRC Verify, Parse)"];
 *   }
 *
 *   subgraph cluster_harq {
 *     label="Optional FEC/HARQ Layer";
 *     style=filled;
 *     fillcolor=lavender;
 *     fec [label="rx_harq\n(FEC Encode/Decode)"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="SPI Hardware Abstraction";
 *     style=filled;
 *     fillcolor=lightgray;
 *     rspi [label="RSPI HAL\n(RX72N Peripheral Mode)"];
 *   }
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5 (SPI Controller)";
 *     style=filled;
 *     fillcolor=lightcoral;
 *     rpi_spi [label="Linux spidev\n(/dev/spidev0.0)"];
 *   }
 *
 *   // Data flow: Send path
 *   app -> comm_mgr [label="send command"];
 *   comm_mgr -> spi_send [label="route to SPI"];
 *   spi_send -> seq_mgmt [label="assign sequence"];
 *   spi_send -> encoder [label="encode frame"];
 *   encoder -> fec [label="optional FEC", style=dashed];
 *   fec -> rspi [label="SPI transfer"];
 *   encoder -> rspi [label="no FEC"];
 *   rspi -> rpi_spi [label="COPI/CIPO"];
 *
 *   // Data flow: Receive path
 *   rpi_spi -> rspi [label="COPI/CIPO"];
 *   rspi -> decoder [label="raw bytes"];
 *   decoder -> fec [label="optional FEC", style=dashed];
 *   fec -> spi_recv [label="decoded frame"];
 *   decoder -> spi_recv [label="no FEC"];
 *   spi_recv -> seq_mgmt [label="validate sequence"];
 *   spi_recv -> comm_mgr [label="dispatch frame"];
 *   comm_mgr -> app [label="invoke callback"];
 * }
 * @enddot
 *
 * ## SPI Communication Protocol
 *
 * **Hardware Configuration:**
 * - **RX72N Role**: SPI Peripheral
 * - **RPi5 Role**: SPI Controller
 * - **RSPI Channel**: RSPI0 (configurable)
 * - **SPI Mode**: Mode 0 (CPOL=0, CPHA=0) default
 * - **Clock Speed**: Up to 10 MHz (limited by RPi5 spidev)
 * - **Data Width**: 8-bit transfers
 * - **Endianness**: Little-endian (all multi-byte fields)
 *
 * **Frame Format** (rx_frame protocol):
 * ```
 * +--------+----------+--------+-------+----------------+----------+
 * | Sync   | Sequence | Length | Type  | Flags | Payload (0-255) | CRC-32  |
 * | 2B     | 2B       | 2B     | 1B    | 1B    | variable        | 4B      |
 * +--------+----------+--------+-------+----------------+----------+
 * | 0xAA55 | uint16   | uint16 | uint8 | uint8 | uint8[]         | uint32  |
 * +--------+----------+--------+-------+----------------+----------+
 * ```
 *
 * **Sequence Number Management (shared session):**
 * - TX/RX sequences managed by shared rx_session_state_t
 * - Cross-transport continuity: USB and SPI share the same sequence counters
 * - Gap tolerance: Accepts gaps up to 10 frames (matching Go gateway)
 * - Sequence reset: Via rx_session_reset() on the shared session
 *
 * **Error Detection and Correction:**
 * - **CRC-32**: Mandatory for all frames (polynomial 0x04C11DB7)
 * - **FEC (optional)**: Reed-Solomon or convolutional coding
 * - **HARQ (optional)**: Soft-combining retransmissions
 *
 * ## Performance Characteristics
 *
 * | Operation | Avg Time | Worst Case | Notes |
 * |-----------|----------|------------|-------|
 * | **rx_spi_comm_init()** | ~10 us | ~20 us | Frame encoder/decoder setup |
 * | **rx_spi_comm_send()** | ~150-500 us | ~1 ms | Depends on payload size, FEC |
 * | **rx_spi_comm_receive()** | ~100-400 us | ~800 us | Depends on payload size, FEC |
 * | **rx_spi_comm_send_ack()** | ~50 us | ~100 us | Minimal frame (no payload) |
 * | **SPI transfer (HW)** | payload_len x 0.8 us | - | @ 10 MHz SPI clock |
 * | **CRC-32 calculation** | payload_len x 0.05 us | - | Software CRC (hardware CRC faster) |
 * | **FEC encoding** | payload_len x 2 us | - | Optional, adds ~50% overhead |
 * | **FEC decoding** | payload_len x 3 us | - | Optional, with error correction |
 *
 * **Throughput Analysis**:
 * - **Theoretical max**: 10 Mbps (SPI clock) x 8 bits = 80 Mbps
 * - **Practical (no FEC)**: ~6 Mbps (frame overhead + processing)
 * - **With FEC**: ~4 Mbps (encoding/decoding overhead)
 * - **Latency (256B frame)**: ~300 us (send) + ~250 us (receive) = ~550 us round-trip
 *
 * ## Memory Usage
 *
 * | Memory Type | Size | Usage |
 * |-------------|------|-------|
 * | **rx_spi_comm_handle_t** | ~4116 bytes | Main state structure |
 * | **RX buffer** | 2048 bytes | Receive staging buffer |
 * | **TX buffer** | 2048 bytes | Transmit staging buffer |
 * | **Frame encoder** | ~8 bytes | Frame encoding state |
 * | **Frame decoder** | ~8 bytes | Frame decoding state |
 * | **Stack (send)** | ~64 bytes | Function locals + encoder |
 * | **Stack (receive)** | ~80 bytes | Function locals + decoder + rx_frame_t |
 *
 * **Memory Layout** (rx_spi_comm_handle_t):
 * ```
 * Offset | Size | Field               | Purpose
 * -------|------|---------------------|---------------------------
 * 0x00   | 8    | encoder             | Frame encoder state
 * 0x08   | 8    | decoder             | Frame decoder state
 * 0x10   | 2048 | rx_buffer           | RX staging buffer
 * 0x810  | 2048 | tx_buffer           | TX staging buffer
 * 0x1010 | 4/8  | session             | Pointer to shared session state
 * 0x1014 | 1    | channel             | RSPI channel (0-2)
 * 0x1015 | 1    | fec_enabled         | FEC enable flag
 * 0x1016 | 1    | initialized         | Initialization flag
 * 0x1017 | 1    | (padding)           | Alignment
 * -------|------|---------------------|---------------------------
 * Total: 4120 bytes (0x1018)
 * ```
 *
 * ## Thread Safety Analysis
 *
 * **Conditional Thread Safety** - Safe with external synchronization:
 *
 * | Scenario | Thread Safety | Mitigation Required |
 * |----------|---------------|---------------------|
 * | **Single-threaded** | [PASS] Safe | None |
 * | **Multi-threaded (same handle)** | [FAIL] Unsafe | External mutex required |
 * | **Multi-threaded (different handles)** | [PASS] Safe | None (independent state) |
 * | **Send from multiple threads** | [FAIL] Unsafe | Mutex around send operations |
 * | **Receive from multiple threads** | [FAIL] Unsafe | Dedicate one thread to polling |
 * | **Send from ISR** | [WARN] Conditional | OK if SPI HAL is ISR-safe |
 * | **Receive from ISR** | [WARN] Conditional | OK if SPI HAL is ISR-safe |
 *
 * **Recommended Architecture**:
 * - Dedicate one thread to SPI communication (poll + send)
 * - Use communication manager (rx_comm_manager) for multi-channel coordination
 * - External mutex if multiple threads must send on same handle
 *
 * ## Hardware Requirements
 *
 * | Resource | Requirement | Usage |
 * |----------|-------------|-------|
 * | **CPU** | RX72N @ 240 MHz | SPI transfer + CRC calculation |
 * | **RAM** | ~4.2 KB per handle | State + buffers (see Memory Usage) |
 * | **RSPI** | RSPI0, RSPI1, or RSPI2 | Configurable channel (default: RSPI0) |
 * | **SPI Pins** | 4 pins (COPI/CIPO/CLK/CS) | See pinout in docs/sections/03_hardware_pinout.tex |
 * | **DMA (optional)** | DMAC channel | For high-throughput SPI transfers |
 * | **Interrupts** | RSPI RX/TX interrupts | For non-blocking transfers |
 *
 * **Pin Configuration** (RSPI0 example):
 * - **COPI (Controller Out Peripheral In)**: PD3 (RX72N input from RPi5)
 * - **CIPO (Controller In Peripheral Out)**: PD2 (RX72N output to RPi5)
 * - **SCLK (Serial Clock)**: PD1 (Clock from RPi5 controller)
 * - **CS (Chip Select)**: PD0 (Active low, from RPi5)
 *
 * ## Usage Examples
 *
 * @par Complete Setup and Integration:
 * @code{.c}
 * #include "rx_spi_comm.h"
 * #include "hardware.h"
 *
 * // Global SPI communication handle
 * static rx_spi_comm_handle_t g_spi_handle;
 *
 * // Initialize SPI communication
 * rx_err_t init_spi_communication(void)
 * {
 *   // Step 1: Initialize SPI hardware (RSPI peripheral mode)
 *   rspi_config_t rspi_cfg = {
 *     .channel = 0,              // RSPI0
 *     .mode = 0,                 // SPI mode 0 (CPOL=0, CPHA=0)
 *     .bit_rate = 10000000,      // 10 MHz
 *     .peripheral_mode = true,   // RX72N is SPI peripheral
 *   };
 *   rx_err_t err = rspi_init_peripheral(&rspi_cfg);
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   // Step 2: Initialize shared session state
 *   static rx_session_state_t g_session;
 *   err = rx_session_init(&g_session);
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   // Step 3: Initialize SPI communication layer
 *   rx_spi_comm_config_t comm_cfg = {
 *     .session = &g_session,
 *     .channel = 0,              // RSPI0
 *     .spi_mode = 0,             // SPI mode 0
 *     .fec_enabled = false,      // Disable FEC for now
 *   };
 *   err = rx_spi_comm_init(&g_spi_handle, &comm_cfg);
 *   if (err != k_rx_ok) {
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 *
 * // Send telemetry response
 * rx_err_t send_telemetry(void)
 * {
 *   uint8_t telemetry[16];
 *
 *   // Fill telemetry data
 *   telemetry[0] = get_temperature_celsius();
 *   telemetry[1] = get_motor_current();
 *   // ... fill remaining fields ...
 *
 *   // Send telemetry frame
 *   rx_err_t err = rx_spi_comm_send(&g_spi_handle,
 *                                   k_frame_type_response,
 *                                   k_frame_flag_none,
 *                                   telemetry,
 *                                   sizeof(telemetry));
 *   if (err != k_rx_ok) {
 *     // Handle send error
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 *
 * // Receive command from RPi5
 * void poll_spi_commands(void)
 * {
 *   rx_frame_t frame;
 *
 *   // Poll with 0ms timeout (non-blocking)
 *   rx_err_t err = rx_spi_comm_receive(&g_spi_handle, &frame, 0);
 *
 *   if (err == k_rx_ok) {
 *     // Frame received successfully
 *     handle_command(&frame);
 *
 *     // Send ACK
 *     rx_spi_comm_send_ack(&g_spi_handle, frame.header.sequence);
 *   } else if (err == k_rx_err_timeout) {
 *     // No data available - this is normal
 *   } else if (err == k_rx_err_crc_mismatch) {
 *     // CRC error - send NACK for retransmission
 *     rx_spi_comm_send_nack(&g_spi_handle, frame.header.sequence, k_frame_flag_none);
 *   } else {
 *     // Other error - log and continue
 *     log_error("SPI receive error: %d", err);
 *   }
 * }
 * @endcode
 *
 * @par Example - Sequence Number Management:
 * @code{.c}
 * // Reset sequence counters (e.g., after connection reset)
 * rx_spi_comm_reset_sequence(&g_spi_handle);
 *
 * // Query current TX sequence
 * uint16_t tx_seq;
 * rx_err_t err = rx_spi_comm_get_tx_sequence(&g_spi_handle, &tx_seq);
 * printf("Current TX sequence: %u\n", tx_seq);
 *
 * // Query expected RX sequence
 * uint16_t rx_seq;
 * err = rx_spi_comm_get_rx_sequence(&g_spi_handle, &rx_seq);
 * printf("Expected RX sequence: %u\n", rx_seq);
 * @endcode
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Compliance | Implementation Notes |
 * |------|------------|---------------------|
 * | **Rule 1: Control Flow** | [PASS] | No goto, setjmp, longjmp, or recursion |
 * | **Rule 2: Loop Bounds** | [PASS] | All loops have statically provable bounds |
 * | **Rule 3: No Dynamic Allocation** | [PASS] | Zero malloc/free - all static buffers |
 * | **Rule 4: Function Size** | [PASS] | All functions < 60 lines |
 * | **Rule 5: Assertions** | [PASS] | Minimum 2 checks per function (nullptr, initialized) |
 * | **Rule 6: Data Scope** | [PASS] | Variables declared at smallest scope |
 * | **Rule 7: Check Returns** | [PASS] | All return values validated |
 * | **Rule 8: Preprocessor** | [PASS] | Typed enums for constants, minimal macros |
 * | **Rule 9: Pointers** | [PASS] | Single-level dereferencing only |
 * | **Rule 10: Compiler Warnings** | [PASS] | -Wall -Wextra -Werror enabled |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Module responsible ONLY for SPI frame protocol; hardware abstraction delegated to RSPI HAL |
 * | **Open/Closed** | Extensible via configuration (FEC enable/disable); new frame types handled by frame layer |
 * | **Liskov Substitution** | Interchangeable with rx_usb_comm (same rx_frame_t interface) |
 * | **Interface Segregation** | Small focused API (9 functions); send/receive/utility separated |
 * | **Dependency Inversion** | Depends on abstract rx_frame_t and RSPI HAL, not concrete implementations |
 *
 * ## Module Dependencies
 *
 * **Includes:**
 * - rx_err.h - Error code definitions
 * - rx_frame.h - Frame protocol definitions and encoder/decoder
 * - stdbool.h - Boolean type
 * - stdint.h - Fixed-width integer types
 *
 * **Links Against:**
 * - rx_frame - Frame encoding/decoding with CRC-32
 * - rx_harq (optional) - FEC encoding/decoding with HARQ
 * - rspi HAL - SPI hardware abstraction (peripheral mode)
 * - rx_crc32 - CRC-32 calculation (via rx_frame)
 *
 * **Referenced Documentation:**
 * - docs/sections/01_nanopb_protocol.tex - Complete protocol specification
 * - docs/sections/03_hardware_pinout.tex - SPI pin assignments
 * - RX72N_Manual_Chapters/Ch25_SPI.txt - RSPI peripheral specification
 *
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 *
 * @see rx_frame.h Frame protocol layer
 * @see rx_harq.h HARQ layer (optional)
 * @see hardware.h SPI HAL interface
 * @see rx_comm_manager.h Multi-channel communication coordinator
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx_err.h"
#include "rx_frame.h"
#include "rx_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum rx_spi_comm_constants_t
 * @brief SPI communication configuration constants and buffer sizes
 *
 * @details
 * Defines compile-time constants for SPI channel selection, buffer sizes,
 * and default timeout values. All sizes chosen to accommodate maximum frame
 * size (k_frame_max_total_size = 263 bytes) with safety margin.
 *
 * **Design Rationale:**
 * - **2048-byte buffers**: Allows ~7 maximum-size frames to be buffered,
 *   providing tolerance for burst traffic and preventing frame loss.
 * - **10 MHz SPI clock**: Maximum reliable speed for RPi5 spidev with 1m cable.
 * - **1 second timeout**: Conservative default for reliable operation.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Default SPI mode (CPOL=0, CPHA=0)
   * @details
   * SPI Mode 0: Clock idles low, data sampled on rising edge.
   * **RPi5 spidev mode**: mode=0 (matches this setting)
   * @par Value: 0
   * @par Modes: 0 (0,0), 1 (0,1), 2 (1,0), 3 (1,1)
   */
  k_spi_comm_default_mode = 0,

  /**
   * @brief RX staging buffer size in bytes
   * @details
   * Buffer for incoming frames before decoding. Size allows buffering
   * multiple frames to handle burst traffic from RPi5.
   * **Capacity**: ~7 maximum-size frames (263 bytes each)
   * **Memory**: Static allocation in rx_spi_comm_handle_t
   * @par Value: 2048 bytes
   * @par Rationale: Power-of-2 for alignment, sufficient for burst tolerance
   */
  k_spi_comm_rx_buffer_size = 2048,

  /**
   * @brief TX staging buffer size in bytes
   * @details
   * Buffer for outgoing frames before SPI transmission. Same size as RX
   * buffer for symmetry and burst tolerance.
   * **Capacity**: ~7 maximum-size frames (263 bytes each)
   * **Memory**: Static allocation in rx_spi_comm_handle_t
   * @par Value: 2048 bytes
   * @par Rationale: Symmetric with RX buffer, burst tolerance
   */
  k_spi_comm_tx_buffer_size = 2048,

  /**
   * @brief Default timeout in milliseconds for blocking operations
   * @details
   * Used for rx_spi_comm_receive() when non-zero timeout specified.
   * **Conservative value**: Allows reliable operation even with CPU load.
   * **Note**: Most operations use 0ms timeout (non-blocking polling).
   * @par Value: 1000 milliseconds (1 second)
   * @par Rationale: Conservative for reliable operation
   */
  k_spi_comm_default_timeout = 1000,
} rx_spi_comm_constants_t;

/* =============================================================================
 * Retransmit Configuration
 * =============================================================================
 */

/**
 * @enum rx_retransmit_defaults_retries_t
 * @brief Default retry count for automatic retransmission
 *
 * @details
 * Maximum number of retransmission attempts before giving up.
 * Applied when auto_retransmit is enabled but max_retries is zero.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_retransmit_default_max_retries = 3, /**< Default max retransmit attempts */
} rx_retransmit_defaults_retries_t;

/**
 * @enum rx_retransmit_defaults_timing_t
 * @brief Default timing values for automatic retransmission
 *
 * @details
 * Timing parameters for ACK timeout and exponential backoff cap.
 * Applied when auto_retransmit is enabled but timing values are zero.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_retransmit_default_ack_timeout_ms = 50,  /**< Initial ACK wait in ms */
  k_retransmit_default_max_backoff_ms = 400, /**< Max exponential backoff cap in ms */
} rx_retransmit_defaults_timing_t;

/**
 * @struct rx_spi_comm_retransmit_config_t
 * @brief Configuration for automatic retransmission behavior
 *
 * @details
 * Controls retransmission parameters when auto_retransmit is enabled.
 * Zero values are replaced with defaults during initialization.
 *
 * @par Field Constraints:
 * | Field | Min | Max | Default | Notes |
 * |-------|-----|-----|---------|-------|
 * | max_retries | 1 | 10 | 3 | 0 = use default |
 * | ack_timeout_ms | 10 | 1000 | 50 | 0 = use default |
 * | max_backoff_ms | 50 | 5000 | 400 | 0 = use default |
 *
 * @since Version 1.0.0
 * @see rx_spi_comm_set_auto_retransmit() Runtime configuration
 */
typedef struct {
  uint8_t  max_retries;    /**< Max retransmit attempts (default 3, 0 = use default) */
  uint16_t ack_timeout_ms; /**< Initial ACK timeout in ms (default 50, 0 = use default) */
  uint16_t max_backoff_ms; /**< Max backoff cap in ms (default 400, 0 = use default) */
} rx_spi_comm_retransmit_config_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @struct rx_spi_comm_handle_t
 * @brief SPI communication handle containing all state for frame protocol
 *
 * @details
 * Complete state for SPI frame-based communication including frame encoder/decoder,
 * sequence number tracking, and staging buffers. This structure must be initialized
 * via rx_spi_comm_init() before use.
 *
 * **Lifecycle:**
 * 1. Allocate handle (typically static or global)
 * 2. Initialize via rx_spi_comm_init()
 * 3. Use for send/receive operations
 * 4. Deinitialize via rx_spi_comm_deinit() when done
 *
 * **Memory Footprint**: 4120 bytes (see file-level Memory Usage section)
 *
 * **Thread Safety**: Not thread-safe - use external mutex if accessed from
 * multiple threads.
 *
 * @par Memory Layout:
 * See file-level Memory Usage section for detailed offset and size breakdown.
 *
 * @par Usage Example:
 * @code{.c}
 * static rx_spi_comm_handle_t g_spi_handle;
 * static rx_session_state_t   g_session;
 *
 * void init(void) {
 *   rx_session_init(&g_session);
 *   rx_spi_comm_config_t cfg = {
 *     .session = &g_session,
 *     .channel = 0,
 *     .spi_mode = 0,
 *     .fec_enabled = false
 *   };
 *   rx_err_t err = rx_spi_comm_init(&g_spi_handle, &cfg);
 *   // ... check error ...
 * }
 * @endcode
 *
 * @since Version 1.0.0
 * @see rx_spi_comm_init() Initialization function
 * @see rx_spi_comm_deinit() Cleanup function
 */
typedef struct {
  /**
   * @brief Frame encoder for outgoing frames
   * @details
   * Encodes payload + header into wire format with CRC-32.
   * **State**: Maintains encoding context (minimal ~8 bytes).
   * @see rx_frame.h rx_frame_encoder_t definition
   */
  rx_frame_encoder_t encoder;

  /**
   * @brief Frame decoder for incoming frames
   * @details
   * Decodes wire format into payload + header, validates CRC-32.
   * **State**: Maintains decoding context (minimal ~8 bytes).
   * @see rx_frame.h rx_frame_decoder_t definition
   */
  rx_frame_decoder_t decoder;

  /**
   * @brief RX staging buffer for incoming SPI data
   * @details
   * Temporary buffer for receiving frames from RPi5 before decoding.
   * **Size**: 2048 bytes (k_spi_comm_rx_buffer_size)
   * **Capacity**: ~7 maximum-size frames
   * **Access**: Internal only (not exposed to application)
   */
  uint8_t rx_buffer[k_spi_comm_rx_buffer_size];

  /**
   * @brief TX staging buffer for outgoing SPI data
   * @details
   * Temporary buffer for encoding frames before SPI transmission to RPi5.
   * **Size**: 2048 bytes (k_spi_comm_tx_buffer_size)
   * **Capacity**: ~7 maximum-size frames
   * **Access**: Internal only (not exposed to application)
   */
  uint8_t tx_buffer[k_spi_comm_tx_buffer_size];

  /**
   * @brief Pointer to shared session state (cross-transport sequence continuity)
   * @details
   * Points to a single rx_session_state_t owned by rx_comm_manager_t, shared
   * with the USB CDC transport. When the active transport switches (e.g., USB
   * at seq=105 to SPI), SPI continues at seq=106 because both reference the
   * same session state.
   * **Lifetime**: Must remain valid for the lifetime of this handle.
   * **Thread Safety**: Session functions are thread-safe (internal mutex).
   * @see rx_session.h Shared session API
   * @see rx_comm_manager.h Owns the session instance
   */
  rx_session_state_t* session;

  /**
   * @brief RSPI channel (0-2)
   * @details
   * Hardware RSPI peripheral channel.
   * **Valid values**: k_rspi_channel_0, k_rspi_channel_1, k_rspi_channel_2
   * **Default**: k_rspi_channel_0 (RSPI0 on PD0-PD3)
   * @see rspi_channel_t for channel enumeration
   */
  rspi_channel_t channel;

  /**
   * @brief FEC (Forward Error Correction) enabled flag
   * @details
   * If true, FEC encoding/decoding applied to frames.
   * **Impact**: Adds ~50% overhead, improves reliability in noisy environments.
   * **Default**: false (FEC disabled for speed)
   * @warning FEC requires both sides (RX72N + RPi5) to agree on encoding
   */
  bool fec_enabled;

  /**
   * @brief Initialization flag
   * @details
   * Set to true by rx_spi_comm_init(), cleared by rx_spi_comm_deinit().
   * **Purpose**: Prevents operations on uninitialized handle.
   * **Validation**: Checked by all public functions.
   */
  bool initialized;

  /**
   * @brief Callback invoked when PING control frame received
   * @details Called with decoded frame and user context during rx_spi_comm_receive.
   */
  void (*on_ping_cb)(const rx_frame_t* frame, void* ctx);

  /**
   * @brief Callback invoked when RESET control frame received
   * @details Called with decoded frame and user context during rx_spi_comm_receive.
   */
  void (*on_reset_cb)(const rx_frame_t* frame, void* ctx);

  /**
   * @brief User context pointer passed to control frame callbacks (PING, RESET)
   */
  void* control_cb_ctx;

  /* ---- Retransmission state (only active when auto_retransmit enabled) ---- */

  /**
   * @brief Enable automatic retransmission of sent frames
   * @details When true, sent frames are buffered and retransmitted on NACK or timeout.
   */
  bool auto_retransmit;

  /**
   * @brief Active retransmit configuration (copied from config or set at runtime)
   */
  rx_spi_comm_retransmit_config_t retransmit_cfg;

  /**
   * @brief Retry buffer holding last sent encoded wire frame
   * @details Sized to k_frame_max_size (1036 bytes) for maximum frame.
   */
  uint8_t retry_buffer[k_frame_max_size];

  /**
   * @brief TX encode scratch buffer (handle-owned, not on caller stack)
   * @details Sized to k_frame_max_size (1036 bytes). Replaces a 1036-byte
   * stack allocation in rx_spi_comm_send/_send_pong that previously consumed
   * ~50% of a 2 KB ThreadX task stack. Single-producer per handle, so the
   * caller-side mutex serializing SPI transfers also serializes use of this
   * scratch buffer.
   */
  uint8_t tx_encode_buffer[k_frame_max_size];

  /**
   * @brief Actual encoded wire length of buffered frame in retry_buffer
   */
  uint32_t retry_wire_len;

  /**
   * @brief Sequence number of the frame in retry_buffer
   */
  uint16_t retry_sequence;

  /**
   * @brief Current retry attempt count (0 = first send, 1+ = retransmissions)
   */
  uint8_t retry_count;

  /**
   * @brief True when a frame is buffered and awaiting ACK
   */
  bool retry_pending;

  /**
   * @brief Timestamp (ms) of last send or retransmit of the buffered frame
   */
  uint32_t retry_send_time_ms;

  /**
   * @brief Optional callback invoked when ACK received for a pending frame
   */
  void (*on_ack_cb)(uint16_t sequence, void* ctx);

  /**
   * @brief Optional callback invoked when NACK received for a pending frame
   */
  void (*on_nack_cb)(uint16_t sequence, void* ctx);

  /**
   * @brief User context pointer passed to retransmit callbacks (ACK, NACK)
   */
  void* retransmit_cb_ctx;
} rx_spi_comm_handle_t;

/**
 * @struct rx_spi_comm_config_t
 * @brief Configuration structure for SPI communication initialization
 *
 * @details
 * Specifies SPI hardware parameters and optional FEC enable. Used during
 * rx_spi_comm_init() to configure the communication layer.
 *
 * **IMPORTANT**: config is REQUIRED (must not be NULL). The session pointer
 * in the config is mandatory for shared sequence state.
 *
 * @par Field Constraints:
 * | Field | Min | Max | Default | Notes |
 * |-------|-----|-----|---------|-------|
 * | session | non-NULL | - | - | Required: shared session state |
 * | channel | 0 | 2 | 0 | RX72N has 3 RSPI channels |
 * | spi_mode | 0 | 3 | 0 | Mode 0 recommended for RPi5 |
 * | fec_enabled | false | true | false | Enable for noisy environments |
 *
 * @par Usage Example:
 * @code{.c}
 * rx_spi_comm_handle_t handle;
 * rx_session_state_t session;
 * rx_session_init(&session);
 *
 * rx_spi_comm_config_t cfg = {
 *   .session = &session,
 *   .channel = k_rspi_channel_0,
 *   .spi_mode = k_spi_comm_default_mode,
 *   .fec_enabled = false
 * };
 * rx_err_t err = rx_spi_comm_init(&handle, &cfg);
 * @endcode
 *
 * @since Version 1.0.0
 * @see rx_spi_comm_init() Initialization function
 * @see rx_spi_comm_constants_t Default values
 */
typedef struct {
  /**
   * @brief Shared session state (REQUIRED, must not be NULL)
   * @details
   * Pointer to shared session state for cross-transport sequence continuity.
   * Owned by rx_comm_manager_t, shared with USB CDC transport.
   * **Lifetime**: Must remain valid for the lifetime of the SPI comm handle.
   * @see rx_session.h Shared session API
   */
  rx_session_state_t* session;

  /**
   * @brief RSPI channel (0-2)
   * @details
   * Hardware peripheral channel. RX72N has 3 RSPI channels.
   * **Default**: k_rspi_channel_0 (RSPI0)
   * @see rspi_channel_t for channel enumeration
   */
  rspi_channel_t channel;

  /**
   * @brief SPI mode (0-3)
   * @details
   * Clock polarity and phase configuration.
   * **Mode 0**: CPOL=0, CPHA=0 (clock idles low, sample on rising edge)
   * **Default**: 0
   * **Valid range**: [0, 3]
   */
  uint8_t spi_mode;

  /**
   * @brief Enable Forward Error Correction
   * @details
   * If true, applies FEC encoding to outgoing frames and FEC decoding
   * to incoming frames. Improves reliability at cost of throughput.
   * **Default**: false
   * **Overhead**: ~50% (e.g., 100 bytes -> 150 bytes with FEC)
   */
  bool fec_enabled;

  /**
   * @brief Enable automatic retransmission (default: false)
   * @details
   * When true, sent frames with k_frame_flag_requires_ack are buffered
   * and automatically retransmitted on NACK or ACK timeout.
   * **Default**: false (fire-and-forget behavior)
   */
  bool auto_retransmit;

  /**
   * @brief Retransmit configuration (only used if auto_retransmit true)
   * @details Zero fields use defaults. Ignored when auto_retransmit is false.
   */
  rx_spi_comm_retransmit_config_t retransmit_config;
} rx_spi_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize SPI communication handle with frame protocol support
 *
 * @details
 * Performs complete initialization of SPI communication handle including frame
 * encoder/decoder setup, sequence number initialization, and optional FEC configuration.
 * This function MUST be called before any send/receive operations.
 *
 * **IMPORTANT**: This function does NOT initialize SPI hardware. Caller must
 * initialize RSPI peripheral via rspi_init_peripheral() BEFORE calling this function.
 *
 * **Algorithm:**
 * 1. **Validate parameters**: Check handle pointer for nullptr (NASA Rule 5)
 * 2. **Clear handle**: Zero-fill entire structure (4120 bytes)
 * 3. **Apply configuration**: Copy config fields if non-NULL, else use defaults
 * 4. **Initialize frame encoder**: Setup CRC-32 and header encoding
 * 5. **Initialize frame decoder**: Setup CRC-32 validation and header parsing
 * 6. **Initialize sequences**: Set tx_sequence = 0, rx_sequence = 0
 * 7. **Mark initialized**: Set handle->initialized = true
 *
 * **Configuration Options** (config is REQUIRED, not optional):
 * - **session**: REQUIRED, pointer to initialized rx_session_state_t
 * - **channel**: 0-2 (RSPI0/1/2), default 0
 * - **spi_mode**: 0-3 (SPI modes), default 0
 * - **fec_enabled**: true/false, default false
 *
 * @param[out] handle Pointer to SPI communication handle to initialize
 *   - **Valid range**: Non-nullptr to allocated rx_spi_comm_handle_t (4120 bytes)
 *   - **Constraints**: Must be allocated by caller (typically static or global)
 *   - **Side effects**: Entire structure zero-filled, then populated from config
 *   - **Lifetime**: Must remain valid until rx_spi_comm_deinit() called
 *
 * @param[in] config Required configuration structure (must not be NULL)
 *   - **Valid range**: Non-nullptr to valid rx_spi_comm_config_t
 *   - **Constraints**: session must be non-NULL, channel must be 0-2, spi_mode must be 0-3
 *   - **Lifetime**: Not stored - values copied into handle (except session ptr)
 *
 * @return rx_err_t Error code indicating initialization success or failure
 * @retval k_rx_ok Success - handle fully initialized and ready for operation
 * @retval k_rx_err_invalid_arg handle is nullptr; config is nullptr; config->session is nullptr; config->channel > 2 (invalid RSPI channel); config->spi_mode > 3 (invalid SPI mode)
 *
 * @pre handle must point to allocated memory (uninitialized content OK)
 * @pre RSPI hardware must already be initialized via rspi_init_peripheral()
 * @post handle->initialized = true on success
 * @post handle->session points to config->session
 * @post All buffers zero-filled (rx_buffer, tx_buffer)
 *
 * @note This function does NOT initialize SPI hardware - caller responsible
 * @note On success, handle is ready for rx_spi_comm_send/receive operations
 * @note Thread-safe ONLY if each thread uses a different handle
 *
 * @warning Must call rspi_init_peripheral() BEFORE this function
 *
 * @par Performance:
 * - Execution time: ~10 us @ 240 MHz
 * - Dominated by memset (4120 bytes)
 *
 * @par Example:
 * @code{.c}
 * static rx_spi_comm_handle_t g_spi_handle;
 * static rx_session_state_t   g_session;
 *
 * // Step 1: Initialize shared session
 * rx_err_t err = rx_session_init(&g_session);
 * if (err != k_rx_ok) return err;
 *
 * // Step 2: Initialize SPI communication layer
 * rx_spi_comm_config_t comm_cfg = {
 *   .session = &g_session,
 *   .channel = 0,
 *   .spi_mode = 0,
 *   .fec_enabled = false
 * };
 * err = rx_spi_comm_init(&g_spi_handle, &comm_cfg);
 * if (err != k_rx_ok) {
 *   log_error("SPI comm init failed: %d", err);
 *   return err;
 * }
 * @endcode
 *
 * @see rx_spi_comm_deinit() Cleanup and resource release
 * @see rx_spi_comm_send() Send frame
 * @see rx_spi_comm_receive() Receive frame
 * @see rspi_init_peripheral() Initialize SPI hardware (must call first)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_init(rx_spi_comm_handle_t*       handle,
                                        const rx_spi_comm_config_t* config);

/**
 * @brief Deinitialize SPI communication handle and release resources
 *
 * @details
 * Marks SPI communication handle as uninitialized, preventing further operations.
 * This function does NOT deinitialize RSPI hardware - caller must separately call
 * rspi_deinit() if needed.
 *
 * **Algorithm:**
 * 1. Validate handle pointer (NULL check)
 * 2. Validate initialized flag (must be initialized to deinitialize)
 * 3. Clear initialized flag (marks handle as invalid)
 *
 * **Design Note**: Minimal deinitialization - no dynamic resources to free
 * (all memory is statically allocated in handle structure). Function primarily
 * serves as safety guard to prevent use-after-deinit.
 *
 * @param[in,out] handle Pointer to SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must have been initialized via rx_spi_comm_init()
 *   - **Side effects**: Clears handle->initialized flag
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success - handle deinitialized
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle was not initialized
 *
 * @pre handle must be non-NULL
 * @pre handle must be initialized (handle->initialized == true)
 * @post handle->initialized = false
 * @post Subsequent calls to rx_spi_comm_send/receive will return k_rx_err_invalid_state
 *
 * @note Does NOT free memory (handle is statically allocated)
 * @note Does NOT deinitialize RSPI hardware
 * @note Thread-safe if no concurrent operations on same handle
 *
 * @par Example:
 * @code{.c}
 * // Deinitialize SPI communication
 * rx_err_t err = rx_spi_comm_deinit(&g_spi_handle);
 * if (err == k_rx_ok) {
 *   // Handle no longer usable
 *
 *   // Separately deinitialize RSPI hardware if needed
 *   rspi_deinit(g_spi_handle.channel);
 * }
 * @endcode
 *
 * @see rx_spi_comm_init() Initialization function
 * @see rspi_deinit() Deinitialize RSPI hardware
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_deinit(rx_spi_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send frame on SPI with automatic sequencing and CRC
 *
 * @details
 * Encodes payload into frame format with automatic sequence number assignment,
 * CRC-32 calculation, and optional FEC encoding. Transmits frame to RPi5 via
 * SPI hardware. This is the primary send function for all frame types.
 *
 * **Algorithm:**
 * 1. **Validate parameters**: Check handle, payload, payload_len (NASA Rule 5)
 * 2. **Assign sequence**: Get next TX sequence from shared rx_session_state_t
 * 3. **Build frame**: Create frame header (type, flags, length, sequence)
 * 4. **Encode frame**: Call frame encoder to add CRC-32
 * 5. **Optional FEC**: If enabled, apply FEC encoding
 * 6. **SPI transfer**: Transmit encoded frame via RSPI peripheral
 * 7. **Update state**: TX sequence already incremented by rx_session_next_tx()
 *
 * **Performance**:
 * - Small payload (16 bytes): ~150 us
 * - Medium payload (128 bytes): ~300 us
 * - Large payload (255 bytes): ~500 us
 * - FEC overhead: +50% time
 *
 * @param[in,out] handle Initialized SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must be initialized via rx_spi_comm_init()
 *   - **Side effects**: Increments shared session TX sequence via rx_session_next_tx()
 *
 * @param[in] type Frame type (command, request, response, etc.)
 *   - **Valid range**: Any rx_frame_type_t value
 *   - **Common values**: k_frame_type_response (most common for RX72N)
 *   - **Units**: Enumeration value
 *
 * @param[in] flags Frame flags (ACK, NACK, etc.)
 *   - **Valid range**: Bitmask of rx_frame_flag_t values
 *   - **Common values**: k_frame_flag_none (0x00)
 *   - **Units**: Bitmask
 *
 * @param[in] payload Payload data to send
 *   - **Valid range**: Non-NULL if payload_len > 0, may be NULL if payload_len == 0
 *   - **Constraints**: Must contain valid data for payload_len bytes
 *   - **Lifetime**: Copied immediately, does not need to persist
 *   - **Max size**: k_frame_max_payload (255 bytes)
 *
 * @param[in] payload_len Payload length in bytes
 *   - **Valid range**: [0, k_frame_max_payload] (0-255 bytes)
 *   - **Units**: Bytes
 *   - **Constraints**: Must be <= k_frame_max_payload
 *
 * @return rx_err_t Error code indicating send success or failure
 * @retval k_rx_ok Frame sent successfully, tx_sequence incremented
 * @retval k_rx_err_invalid_arg handle or payload is nullptr; payload_len > k_frame_max_payload (255 bytes); payload is nullptr but payload_len > 0
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval k_rx_err_hw SPI hardware error (RSPI transfer failed)
 * @retval k_rx_err_timeout SPI transfer timeout
 *
 * @pre handle must be non-NULL and initialized
 * @pre If payload_len > 0, payload must point to valid data
 * @pre RSPI hardware must be initialized and ready
 * @post Shared session TX sequence incremented (wraps at 65535)
 * @post Frame transmitted to RPi5 if successful
 *
 * @note Sequence number managed automatically - caller does not specify
 * @note CRC-32 calculated automatically by frame encoder
 * @note FEC applied automatically if handle->fec_enabled = true
 * @warning Ensure RPi5 is ready to receive (chip select asserted)
 *
 * @par Example - Send Telemetry Response:
 * @code{.c}
 * uint8_t telemetry[16];
 * // ... fill telemetry data ...
 *
 * rx_err_t err = rx_spi_comm_send(&g_spi_handle,
 *                                 k_frame_type_response,
 *                                 k_frame_flag_none,
 *                                 telemetry,
 *                                 sizeof(telemetry));
 * if (err == k_rx_ok) {
 *   // Telemetry sent successfully
 * } else {
 *   log_error("Send failed: %d", err);
 * }
 * @endcode
 *
 * @par Example - Send Empty ACK (No Payload):
 * @code{.c}
 * // Send empty frame with ACK flag
 * rx_err_t err = rx_spi_comm_send(&g_spi_handle,
 *                                 k_frame_type_response,
 *                                 k_frame_flag_ack,
 *                                 NULL,    // No payload
 *                                 0);      // Zero length
 * @endcode
 *
 * @see rx_spi_comm_send_ack() Convenience wrapper for ACK frames
 * @see rx_spi_comm_send_nack() Convenience wrapper for NACK frames
 * @see rx_spi_comm_receive() Receive frames from RPi5
 * @see rx_frame.h Frame protocol definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_send(rx_spi_comm_handle_t* handle,
                                        rx_frame_type_t       type,
                                        uint8_t               flags,
                                        const uint8_t*        payload,
                                        uint32_t              payload_len);

/**
 * @brief Send ACK frame for received frame (convenience wrapper)
 *
 * @details
 * Convenience function for sending acknowledgment frames. Automatically sets
 * frame type to k_frame_type_response and flags to k_frame_flag_ack. Includes
 * the sequence number being acknowledged in the frame.
 *
 * **Use Case**: Acknowledge successful receipt of a frame from RPi5.
 *
 * **Algorithm:**
 * 1. Build ACK frame with type=response, flags=ack
 * 2. Include sequence number in payload (2 bytes, little-endian)
 * 3. Call rx_spi_comm_send() with constructed frame
 *
 * **Performance**: ~50 us (minimal frame with 2-byte payload)
 *
 * @param[in,out] handle Initialized SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must be initialized via rx_spi_comm_init()
 *
 * @param[in] sequence Sequence number to acknowledge
 *   - **Valid range**: [0, 65535]
 *   - **Units**: Sequence number (uint16_t)
 *   - **Purpose**: Tells RPi5 which frame is being acknowledged
 *
 * @return rx_err_t Error code (same as rx_spi_comm_send)
 * @retval k_rx_ok ACK sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval (other) Errors propagated from rx_spi_comm_send()
 *
 * @pre Same preconditions as rx_spi_comm_send()
 * @post ACK frame transmitted with specified sequence number
 *
 * @note Equivalent to calling rx_spi_comm_send() with type=response, flags=ack
 * @note This is a thin wrapper - no additional validation performed
 *
 * @par Example:
 * @code{.c}
 * // Receive frame and send ACK
 * rx_frame_t frame;
 * rx_err_t err = rx_spi_comm_receive(&g_spi_handle, &frame, 0);
 * if (err == k_rx_ok) {
 *   // Process frame
 *   handle_frame(&frame);
 *
 *   // Send ACK for this frame
 *   err = rx_spi_comm_send_ack(&g_spi_handle, frame.header.sequence);
 * }
 * @endcode
 *
 * @see rx_spi_comm_send() Full send function
 * @see rx_spi_comm_send_nack() Send NACK for errors
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_send_ack(rx_spi_comm_handle_t* handle, uint16_t sequence);

/**
 * @brief Send NACK frame for received frame with error indication
 *
 * @details
 * Convenience function for sending negative acknowledgment frames. Used to
 * request retransmission when frame reception fails (CRC error, corruption, etc.).
 *
 * **Use Cases**:
 * - CRC validation failure (hard NACK)
 * - FEC decoding failure (soft NACK if HARQ enabled)
 * - Out-of-order sequence detection
 * - Protocol violation
 *
 * **Algorithm:**
 * 1. Build NACK frame with type=response, flags=nack | additional_flags
 * 2. Include sequence number in payload (2 bytes, little-endian)
 * 3. Call rx_spi_comm_send() with constructed frame
 *
 * **Performance**: ~50 us (minimal frame with 2-byte payload)
 *
 * @param[in,out] handle Initialized SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must be initialized via rx_spi_comm_init()
 *
 * @param[in] sequence Sequence number being NACKed
 *   - **Valid range**: [0, 65535]
 *   - **Units**: Sequence number (uint16_t)
 *   - **Purpose**: Tells RPi5 which frame failed and needs retransmission
 *
 * @param[in] flags Additional flags (e.g., k_frame_flag_soft_nack)
 *   - **Valid range**: Bitmask of rx_frame_flag_t values
 *   - **Common values**: k_frame_flag_none (hard NACK), k_frame_flag_soft_nack (HARQ)
 *   - **Units**: Bitmask
 *
 * @return rx_err_t Error code (same as rx_spi_comm_send)
 * @retval k_rx_ok NACK sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval (other) Errors propagated from rx_spi_comm_send()
 *
 * @pre Same preconditions as rx_spi_comm_send()
 * @post NACK frame transmitted with specified sequence number and flags
 *
 * @note RPi5 should retransmit frame with specified sequence number
 * @note Use k_frame_flag_soft_nack for FEC soft-combining (HARQ)
 *
 * @par Example - Hard NACK on CRC Error:
 * @code{.c}
 * rx_frame_t frame;
 * rx_err_t err = rx_spi_comm_receive(&g_spi_handle, &frame, 0);
 * if (err == k_rx_err_crc_mismatch) {
 *   // CRC validation failed - send hard NACK
 *   rx_spi_comm_send_nack(&g_spi_handle,
 *                         frame.header.sequence,
 *                         k_frame_flag_none);  // Hard NACK
 * }
 * @endcode
 *
 * @par Example - Soft NACK for HARQ:
 * @code{.c}
 * // FEC decoding failed but HARQ soft-combining available
 * if (fec_decode_failed && harq_enabled) {
 *   rx_spi_comm_send_nack(&g_spi_handle,
 *                         frame.header.sequence,
 *                         k_frame_flag_soft_nack);  // Soft NACK for HARQ
 * }
 * @endcode
 *
 * @see rx_spi_comm_send() Full send function
 * @see rx_spi_comm_send_ack() Send ACK for success
 * @see rx_frame.h Frame flag definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_send_nack(rx_spi_comm_handle_t* handle, uint16_t sequence, uint8_t flags);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode frame from RPi5 with CRC validation
 *
 * @details
 * Reads raw bytes from SPI hardware, validates CRC-32, decodes frame header and
 * payload, and optionally applies FEC decoding with HARQ soft-combining. Returns
 * complete decoded frame to caller.
 *
 * **Algorithm:**
 * 1. **Validate parameters**: Check handle, frame pointer (NASA Rule 5)
 * 2. **Poll SPI hardware**: Read available bytes (non-blocking if timeout=0)
 * 3. **Optional FEC decode**: If enabled, apply FEC decoding with HARQ
 * 4. **Decode frame**: Parse header, extract payload
 * 5. **Validate CRC**: Compute CRC-32 and compare with frame CRC
 * 6. **Validate sequence**: Check if sequence matches expected rx_sequence
 * 7. **Update state**: Increment rx_sequence on success
 * 8. **Return frame**: Copy decoded frame to output parameter
 *
 * **Blocking Behavior**:
 * - **timeout_ms = 0**: Non-blocking poll (returns immediately)
 * - **timeout_ms > 0**: Blocks up to timeout_ms waiting for data
 * - **timeout_ms = k_spi_comm_default_timeout**: Use default (1 second)
 *
 * **Performance**:
 * - Small frame (16 bytes): ~100 us
 * - Medium frame (128 bytes): ~250 us
 * - Large frame (255 bytes): ~400 us
 * - FEC overhead: +100 us
 *
 * @param[in,out] handle Initialized SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must be initialized via rx_spi_comm_init()
 *   - **Side effects**: Updates shared session RX sequence via rx_session_validate_rx()
 *
 * @param[out] frame Pointer to frame structure to receive decoded frame
 *   - **Valid range**: Non-nullptr to allocated rx_frame_t
 *   - **Constraints**: Must have sufficient space (263 bytes)
 *   - **Lifetime**: Filled by this function, owned by caller afterward
 *
 * @param[in] timeout_ms Timeout in milliseconds
 *   - **Valid range**: [0, 60000] (0 = non-blocking, up to 1 minute)
 *   - **Units**: Milliseconds
 *   - **Common values**: 0 (poll), k_spi_comm_default_timeout (1 second)
 *
 * @return rx_err_t Error code indicating receive result
 * @retval k_rx_ok Frame received and decoded successfully, rx_sequence incremented
 * @retval k_rx_err_invalid_arg handle or frame is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval k_rx_err_timeout No data received within timeout (not an error if timeout=0)
 * @retval k_rx_err_crc_mismatch CRC validation failed (frame corrupted)
 * @retval k_rx_err_protocol_error Frame format invalid (bad sync, length, etc.)
 * @retval k_rx_err_sequence_error Unexpected sequence number (out-of-order)
 * @retval k_rx_err_hw SPI hardware error
 *
 * @pre handle must be non-NULL and initialized
 * @pre frame must be non-NULL and allocated
 * @pre RSPI hardware must be initialized and ready
 * @post On k_rx_ok: frame filled with decoded data, rx_sequence incremented
 * @post On timeout: frame unchanged, rx_sequence unchanged
 * @post On CRC error: frame may be partially filled, rx_sequence unchanged
 *
 * @note timeout=0 is recommended for polling loops (non-blocking)
 * @note CRC errors should trigger NACK and retransmission request
 * @note Sequence errors indicate lost frames or out-of-order delivery
 * @warning Must call rx_spi_comm_send_ack() or rx_spi_comm_send_nack() after receive
 *
 * @par Example - Non-Blocking Poll:
 * @code{.c}
 * rx_frame_t frame;
 *
 * // Poll with 0ms timeout (non-blocking)
 * rx_err_t err = rx_spi_comm_receive(&g_spi_handle, &frame, 0);
 *
 * if (err == k_rx_ok) {
 *   // Frame received successfully
 *   handle_frame(&frame);
 *   rx_spi_comm_send_ack(&g_spi_handle, frame.header.sequence);
 * } else if (err == k_rx_err_timeout) {
 *   // No data available - this is normal
 * } else if (err == k_rx_err_crc_mismatch) {
 *   // CRC error - request retransmission
 *   rx_spi_comm_send_nack(&g_spi_handle, frame.header.sequence, k_frame_flag_none);
 * } else {
 *   // Other error - log and continue
 *   log_error("Receive error: %d", err);
 * }
 * @endcode
 *
 * @par Example - Blocking Receive with Timeout:
 * @code{.c}
 * rx_frame_t frame;
 *
 * // Wait up to 1 second for frame
 * rx_err_t err = rx_spi_comm_receive(&g_spi_handle, &frame, 1000);
 *
 * if (err == k_rx_ok) {
 *   // Frame received within timeout
 *   process_frame(&frame);
 *   rx_spi_comm_send_ack(&g_spi_handle, frame.header.sequence);
 * } else if (err == k_rx_err_timeout) {
 *   // No frame received within 1 second - RPi5 may be idle
 *   log_warn("No frame received within timeout");
 * }
 * @endcode
 *
 * @see rx_spi_comm_send() Send frames to RPi5
 * @see rx_spi_comm_send_ack() Send ACK for received frame
 * @see rx_spi_comm_send_nack() Send NACK on error
 * @see rx_spi_comm_data_available() Check if data waiting
 * @see rx_frame.h Frame structure definition
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_receive(rx_spi_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

/**
 * @brief Check if data is available for reading (non-blocking query)
 *
 * @details
 * Queries SPI hardware to determine if data is available in receive buffer
 * without actually reading the data. Useful for polling loops to avoid
 * unnecessary receive attempts.
 *
 * **Use Case**: Optimize polling by checking availability before calling
 * rx_spi_comm_receive(), avoiding timeout overhead.
 *
 * **Algorithm:**
 * 1. Validate handle and available pointer
 * 2. Query RSPI hardware for RX buffer status
 * 3. Set *available = true if data waiting, false otherwise
 *
 * **Performance**: ~2 us (fast hardware register read)
 *
 * @param[in] handle Initialized SPI communication handle
 *   - **Valid range**: Non-nullptr to initialized handle
 *   - **Constraints**: Must be initialized via rx_spi_comm_init()
 *
 * @param[out] available Pointer to receive availability status
 *   - **Valid range**: Non-nullptr to bool
 *   - **Output values**: true (data available), false (no data)
 *   - **On error**: Set to false
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Query successful, available flag updated
 * @retval k_rx_err_invalid_arg handle or available is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be non-NULL and initialized
 * @pre available must be non-NULL
 * @post *available set to true or false based on hardware state
 * @post On error, *available set to false (safe default)
 *
 * @note This function does NOT read or modify receive buffer
 * @note Availability can change between check and actual receive
 * @note Fast operation - suitable for high-frequency polling
 *
 * @par Example - Optimized Polling Loop:
 * @code{.c}
 * void poll_spi(void)
 * {
 *   bool available = false;
 *   rx_err_t err = rx_spi_comm_data_available(&g_spi_handle, &available);
 *
 *   if (err == k_rx_ok && available) {
 *     // Data is available - proceed with receive
 *     rx_frame_t frame;
 *     err = rx_spi_comm_receive(&g_spi_handle, &frame, 0);
 *     if (err == k_rx_ok) {
 *       handle_frame(&frame);
 *     }
 *   }
 * }
 * @endcode
 *
 * @see rx_spi_comm_receive() Receive frame
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_data_available(const rx_spi_comm_handle_t* handle,
                                                  bool*                       available);

/* Sequence management is now handled by the shared rx_session_state_t.
 * Use rx_session_next_tx(), rx_session_validate_rx(), rx_session_reset()
 * via the session pointer in the handle or config.
 * @see rx_session.h
 */

/* =============================================================================
 * Control Frame API
 * =============================================================================
 */

/**
 * @brief Register callbacks for PING and RESET control frames
 *
 * @details
 * Sets callback functions that are invoked when rx_spi_comm_receive()
 * encounters PING or RESET control frames. Control frames are handled
 * internally (auto PONG / auto RESET_ACK) and callbacks provide
 * notification to the application layer.
 *
 * @param[in,out] handle Initialized SPI communication handle
 * @param[in] on_ping_cb Callback for PING frames (may be NULL to disable)
 * @param[in] on_reset_cb Callback for RESET frames (may be NULL to disable)
 * @param[in] cb_ctx User context pointer passed to callbacks
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Callbacks registered successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 *
 * @pre handle must be non-NULL
 * @post on_ping_cb, on_reset_cb, control_cb_ctx stored in handle
 *
 * @note Callbacks are invoked from within rx_spi_comm_receive() context
 * @note Not thread-safe, set callbacks before starting receive loop
 *
 * @see rx_spi_comm_receive() Invokes callbacks on control frames
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_set_control_callbacks(rx_spi_comm_handle_t* handle,
                                  void (*on_ping_cb)(const rx_frame_t* frame, void* ctx),
                                  void (*on_reset_cb)(const rx_frame_t* frame, void* ctx),
                                  void* cb_ctx);

/**
 * @brief Send PONG frame with payload (response to PING)
 *
 * @details
 * Constructs and transmits a PONG frame echoing the provided payload.
 * Used internally by rx_spi_comm_receive() for auto-PONG, but also
 * available for manual PONG sending.
 *
 * @param[in,out] handle Initialized SPI communication handle
 * @param[in] payload Payload data to echo (may be NULL if payload_len is 0)
 * @param[in] payload_len Length of payload in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok PONG frame sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval (other) Errors propagated from frame creation or SPI transfer
 *
 * @pre handle must be non-NULL and initialized
 * @post PONG frame transmitted with echoed payload
 *
 * @note Gets next TX sequence from shared session state for the PONG frame
 *
 * @see rx_frame_create_pong() Frame creation for PONG
 * @see rx_spi_comm_receive() Auto-sends PONG on PING reception
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_send_pong(rx_spi_comm_handle_t* handle, const uint8_t* payload, uint32_t payload_len);

/**
 * @brief Send RESET_ACK frame (response to RESET)
 *
 * @details
 * Constructs and transmits a RESET_ACK frame. Used internally by
 * rx_spi_comm_receive() for auto-RESET_ACK, but also available
 * for manual sending.
 *
 * @param[in,out] handle Initialized SPI communication handle
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok RESET_ACK frame sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 * @retval (other) Errors propagated from frame creation or SPI transfer
 *
 * @pre handle must be non-NULL and initialized
 * @post RESET_ACK frame transmitted
 *
 * @note Does NOT reset sequence numbers (caller must do that if desired)
 *
 * @see rx_frame_create_reset_ack() Frame creation for RESET_ACK
 * @see rx_spi_comm_receive() Auto-sends RESET_ACK on RESET reception
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_send_reset_ack(rx_spi_comm_handle_t* handle);

/**
 * @brief Check and process automatic retransmissions
 *
 * @details
 * Checks whether an ACK timeout has elapsed for the most recent sent frame.
 * If auto_retransmit is enabled and a retransmit is pending, resends the
 * buffered frame (up to max_retries with exponential backoff).
 *
 * @param[in,out] handle Initialized SPI communication handle
 * @param[in] current_time_ms Current system time in milliseconds
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok No action needed or retransmit sent
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be non-NULL and initialized
 * @post retry_count incremented on retransmit, pending cleared on limit
 *
 * @note Safe to call when auto_retransmit is disabled (no-op)
 *
 * @see rx_spi_comm_set_auto_retransmit() Enable/disable retransmission
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_spi_comm_process_retransmits(rx_spi_comm_handle_t* handle,
                                                       uint32_t              current_time_ms);

/**
 * @brief Enable or disable automatic retransmission at runtime
 *
 * @details
 * Allows the RPi5 to configure retransmission behavior via protobuf command.
 * When disabling, any pending retry state is cleared.
 *
 * @param[in,out] handle Initialized SPI communication handle
 * @param[in] enabled true to enable, false to disable
 * @param[in] config Retransmit configuration (may be NULL to keep current or use defaults)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Configuration applied successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be non-NULL and initialized
 * @post auto_retransmit flag and config updated
 * @post If disabling, pending retry state cleared
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_set_auto_retransmit(rx_spi_comm_handle_t*                  handle,
                                bool                                   enabled,
                                const rx_spi_comm_retransmit_config_t* config);

/**
 * @brief Register ACK/NACK notification callbacks for retransmission
 *
 * @details
 * Sets optional callbacks invoked when ACK or NACK control frames are
 * consumed during rx_spi_comm_receive(). Useful for telemetry and diagnostics.
 *
 * @param[in,out] handle Initialized SPI communication handle
 * @param[in] on_ack_cb Callback for ACK frames (may be NULL to disable)
 * @param[in] on_nack_cb Callback for NACK frames (may be NULL to disable)
 * @param[in] ctx User context pointer passed to callbacks
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Callbacks registered
 * @retval k_rx_err_invalid_arg handle is nullptr
 *
 * @pre handle must be non-NULL
 * @post Callback pointers stored in handle
 *
 * @note Callbacks invoked from within rx_spi_comm_receive() context
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_spi_comm_set_retransmit_callbacks(rx_spi_comm_handle_t* handle,
                                     void (*on_ack_cb)(uint16_t sequence, void* ctx),
                                     void (*on_nack_cb)(uint16_t sequence, void* ctx),
                                     void* ctx);

#ifdef __cplusplus
}
#endif
