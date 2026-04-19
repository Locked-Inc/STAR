/**
 * @file rx_usb_comm.h
 * @brief High-Level USB CDC Communication Layer for RX72N
 *
 * @details
 * Integrates Frame Layer and USB CDC for reliable communication with the
 * RPi5 controller. Provides the same frame-based protocol as SPI but over
 * USB CDC-ACM virtual serial port. This module enables debug and control
 * communication without requiring the SPI interface.
 *
 * @par System Architecture
 * @dot
 * digraph usb_comm_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5";
 *     style=filled;
 *     color=lightblue;
 *     ttyacm0 [label="/dev/ttyACM0\n(Protocol Port)"];
 *     ttyacm1 [label="/dev/ttyACM1\n(Debug Port)"];
 *     ros2 [label="ROS2 Node\nProtobuf Messages"];
 *     terminal [label="Terminal\nscreen/minicom"];
 *   }
 *
 *   subgraph cluster_usb {
 *     label="USB CDC-ACM";
 *     style=filled;
 *     color=lightyellow;
 *     usb_bus [label="USB 2.0 Full Speed\n12 Mbps"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N Firmware";
 *     style=filled;
 *     color=lightgreen;
 *     usb_comm [label="rx_usb_comm\n(This Module)"];
 *     frame [label="rx_frame\nEncoder/Decoder"];
 *     usb_drv [label="rx_usb\nCDC Driver"];
 *     nanopb [label="nanopb\nProtobuf"];
 *   }
 *
 *   ros2 -> ttyacm0 [label="Protobuf"];
 *   terminal -> ttyacm1 [label="ASCII"];
 *   ttyacm0 -> usb_bus;
 *   ttyacm1 -> usb_bus;
 *   usb_bus -> usb_drv;
 *   usb_drv -> usb_comm [label="Port 0"];
 *   usb_comm -> frame;
 *   frame -> nanopb;
 * }
 * @enddot
 *
 * @par Features
 * - Frame encoding/decoding with CRC-32 error detection
 * - Automatic sequence number tracking for reliable delivery
 * - Non-blocking send/receive operations
 * - Same wire protocol as SPI (interchangeable transport)
 * - Binary protocol mode and debug text mode support
 * - Optional time interface for timeout handling
 *
 * @par Frame Protocol
 * Uses the same frame format as SPI communication:
 * ```
 * +------+------+------+------+--------+-------+
 * | SYNC | TYPE | FLAG | SEQ  | LENGTH | PAYLOAD... | CRC32 |
 * | 0xAA | 1B   | 1B   | 2B   | 2B     | N bytes    | 4B    |
 * +------+------+------+------+--------+-------+
 * ```
 *
 * @par Port Assignment
 * | Port | USB Endpoint | Linux Device | Purpose                    |
 * |------|--------------|--------------|----------------------------|
 * | 0    | EP1/EP2      | /dev/ttyACM0 | Binary protocol (nanopb)   |
 * | 1    | EP3/EP4      | /dev/ttyACM1 | Debug text output          |
 *
 * @par Performance Characteristics
 * | Metric                | Value          | Notes                      |
 * |-----------------------|----------------|----------------------------|
 * | Max throughput        | ~1 MB/s        | USB Full Speed theoretical |
 * | Typical throughput    | 500-800 KB/s   | With protocol overhead     |
 * | Frame latency         | ~1-5 ms        | Depends on USB polling     |
 * | Max frame size        | 2048 bytes     | Buffer size limit          |
 *
 * @par Memory Usage
 * | Component             | Size     | Description                    |
 * |-----------------------|----------|--------------------------------|
 * | rx_usb_comm_handle_t  | ~4.2 KB  | Per-instance handle            |
 * | RX buffer             | 2048 B   | Staging buffer for receives    |
 * | TX buffer             | 2048 B   | Staging buffer for sends       |
 * | Frame encoder/decoder | ~100 B   | Internal state machines        |
 *
 * @par Thread Safety
 * The handle is **NOT thread-safe**. External synchronization (ThreadX mutex)
 * is required for concurrent access from multiple threads. See usage example
 * in rx_usb_comm_handle_t documentation.
 *
 * @par Module Dependencies
 * - rx_frame.h: Frame encoding/decoding layer
 * - rx_usb.h: USB CDC driver and multi-port API
 * - rx_err.h: Error code definitions
 * - rx_time_interface.h: Optional time interface for timeouts
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] Loop bounds via k_usb_comm_max_receive_iterations
 * - Rule 3: [OK] No dynamic memory (static buffers in handle)
 * - Rule 4: [OK] Functions are concise
 * - Rule 5: [OK] Input validation on all public functions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Constants use C23 typed enums
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** USB communication only, frame details in rx_frame
 * - **O (OCP):** Extensible via mode switching, configurable timeouts
 * - **L (LSP):** Same protocol as rx_spi_comm (interchangeable)
 * - **I (ISP):** Clean send/receive interface
 * - **D (DIP):** Depends on rx_time_interface_t abstraction
 *
 * @see rx_frame.h Frame layer implementation
 * @see rx_usb.h USB CDC driver and multi-port API
 * @see rx_spi_comm.h SPI communication (same protocol)
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "rx_frame.h"
#include "rx_session.h"
#include "rx_time_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum rx_usb_comm_constants_t
 * @brief USB communication configuration constants
 *
 * @details
 * Compile-time constants for USB communication buffer sizes, timeouts,
 * and loop bounds. These values are tuned for the RX72N USB Full Speed
 * implementation with typical control message sizes.
 *
 * @par Buffer Sizing Rationale
 * - 2048 bytes accommodates largest expected protobuf message + frame overhead
 * - Matches USB bulk transfer maximum efficiency (multiple of 64 bytes)
 * - Provides headroom for frame header (8 bytes) and CRC (4 bytes)
 *
 * @par Memory Impact
 * Total per-handle: ~4.2 KB (2 x 2048 buffers + encoder/decoder state)
 *
 * @invariant Buffer sizes must be >= maximum frame size
 * @invariant k_usb_comm_max_receive_iterations bounds all receive loops
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief RX staging buffer size in bytes
   * @details
   * Buffer for accumulating incoming USB data before frame decoding.
   * Must be large enough for largest expected frame + partial data.
   * @par Value: 2048 bytes
   */
  k_usb_comm_rx_buffer_size = 2048,

  /**
   * @brief TX staging buffer size in bytes
   * @details
   * Buffer for assembled frames before USB transmission.
   * Must accommodate frame header + max payload + CRC.
   * @par Value: 2048 bytes
   */
  k_usb_comm_tx_buffer_size = 2048,

  /**
   * @brief Default timeout for blocking operations (milliseconds)
   * @details
   * Used when timeout_ms parameter is 0 or for internal waits.
   * @par Value: 1000 ms (1 second)
   */
  k_usb_comm_default_timeout = 1000,

  /**
   * @brief Maximum iterations for receive loop (NASA Rule 2)
   * @details
   * Provides static upper bound for receive polling loops to ensure
   * bounded execution time even under pathological conditions.
   * @par Value: 1000 iterations
   * @warning Reaching this limit indicates USB stall or protocol error
   */
  k_usb_comm_max_receive_iterations = 1000,
} rx_usb_comm_constants_t;

/**
 * @enum rx_usb_comm_mode_t
 * @brief USB communication operating mode
 *
 * @details
 * Controls whether the USB CDC communication layer operates in binary protocol
 * mode (for robot control via protobuf) or debug text mode (for terminal access
 * via screen/minicom). Mode can be changed at runtime via rx_usb_comm_set_mode().
 *
 * @par Mode Comparison
 * | Aspect          | Binary Mode              | Debug Mode              |
 * |-----------------|--------------------------|-------------------------|
 * | Data format     | Framed + CRC-32          | Plain ASCII text        |
 * | Use case        | Robot control (protobuf) | Debug output (terminal) |
 * | rx_usb_comm_send| Enabled                  | Returns error           |
 * | Typical client  | ROS2 node, Python script | screen, minicom         |
 *
 * @par State Machine
 * @startuml
 * [*] --> Invalid : Power on
 * Invalid --> Binary : rx_usb_comm_init()
 * Binary --> Debug : rx_usb_comm_set_mode(DEBUG)
 * Debug --> Binary : rx_usb_comm_set_mode(BINARY)
 * Binary --> Invalid : rx_usb_comm_deinit()
 * Debug --> Invalid : rx_usb_comm_deinit()
 * @enduml
 *
 * @note Mode only affects this communication layer, not the underlying USB driver
 *
 * @see rx_usb_comm_set_mode() Change mode at runtime
 * @see rx_usb_comm_get_mode() Query current mode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Invalid/uninitialized mode sentinel
   * @details
   * Indicates handle has not been initialized. Used internally to detect
   * use-before-init errors. Never set this mode explicitly.
   * @par Value: 0xFF
   * @warning Attempting operations in this mode returns k_rx_err_invalid_state
   */
  k_usb_comm_mode_invalid = 0xFF,

  /**
   * @brief Binary protocol mode (default after init)
   * @details
   * Frame-based communication with CRC-32 validation. Used for robot control
   * messages encoded with protobuf. rx_usb_comm_send() and rx_usb_comm_receive()
   * operate normally in this mode.
   * @par Value: 0
   * @par Use case: ROS2 integration, automated testing
   */
  k_usb_comm_mode_binary = 0,

  /**
   * @brief Debug text mode for terminal access
   * @details
   * Plain ASCII text output without framing. Used for debugging via terminal
   * emulators (screen, minicom). In this mode:
   * - rx_usb_comm_send() returns k_rx_err_invalid_state
   * - Use rx_usb_puts(k_usb_port_debug, ...) for output instead
   * - rx_usb_comm_receive() can still receive data if needed
   * @par Value: 1
   * @par Use case: Interactive debugging, log viewing
   */
  k_usb_comm_mode_debug = 1,
} rx_usb_comm_mode_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @struct rx_usb_comm_handle_t
 * @brief USB communication handle containing all operational state
 *
 * @details
 * Contains all state for USB communication including frame encoder/decoder,
 * sequence tracking, and staging buffers. One handle corresponds to one
 * logical USB communication channel.
 *
 * @par Memory Layout
 * | Offset | Size     | Field          | Description                    |
 * |--------|----------|----------------|--------------------------------|
 * | 0      | ~50 B    | encoder        | Frame encoder state machine    |
 * | 50     | ~50 B    | decoder        | Frame decoder state machine    |
 * | 100    | 2048 B   | rx_buffer      | RX staging buffer              |
 * | 2148   | 2048 B   | tx_buffer      | TX staging buffer              |
 * | 4196   | 4 B      | rx_buffer_len  | Valid bytes in RX buffer       |
 * | 4200   | 4 B      | rx_buffer_pos  | Current read position          |
 * | 4204   | 4-8 B    | session        | Shared session state pointer   |
 * | 4212   | 1 B      | initialized    | Init flag                      |
 * | 4209   | 1 B      | mode           | Operating mode                 |
 * | 4211   | 4-8 B    | time_iface     | Time interface pointer         |
 * | **Total** | **~4.2 KB** |         |                                |
 *
 * @par Port Assignment
 * This handle always operates on **Port 0** (k_usb_port_proto) for protocol.
 * For debug text output, use rx_usb_puts(k_usb_port_debug, ...) directly.
 *
 * @par Thread Safety
 * This handle is **NOT thread-safe**. External synchronization via ThreadX
 * mutex is required for concurrent access from multiple threads.
 *
 * @par Usage Example (Thread-Safe)
 * @code
 * static rx_usb_comm_handle_t g_usb_comm;
 * static TX_MUTEX g_usb_comm_mutex;
 *
 * void system_init(void) {
 *     // Initialize USB communication
 *     rx_usb_comm_init(&g_usb_comm, NULL);
 *
 *     // Create mutex for thread safety
 *     tx_mutex_create(&g_usb_comm_mutex, "USB_COMM", TX_NO_INHERIT);
 * }
 *
 * rx_err_t send_message(const uint8_t* data, uint32_t len) {
 *     rx_err_t err;
 *
 *     // Acquire mutex before accessing handle
 *     UINT status = tx_mutex_get(&g_usb_comm_mutex, TX_WAIT_FOREVER);
 *     if (status != TX_SUCCESS) {
 *         return k_rx_err_rtos_mutex;
 *     }
 *
 *     // Send data (mutex held)
 *     err = rx_usb_comm_send(&g_usb_comm, k_frame_type_response, 0, data, len);
 *
 *     // Release mutex
 *     (void)tx_mutex_put(&g_usb_comm_mutex);
 *
 *     return err;
 * }
 * @endcode
 *
 * @par Complete Initialization Example
 * @code
 * static rx_usb_comm_handle_t g_usb_comm;
 * static rx_session_state_t g_session;
 * static rx_time_interface_t g_time_iface;
 *
 * void usb_comm_setup(void) {
 *     rx_session_init(&g_session);
 *     rx_usb_comm_config_t config = {
 *         .session    = &g_session,
 *         .time_iface = &g_time_iface // Or NULL for default
 *     };
 *
 *     rx_err_t err = rx_usb_comm_init(&g_usb_comm, &config);
 *     if (err != k_rx_ok) {
 *         rx_log_error("USB_COMM", "Init failed");
 *         return;
 *     }
 *
 *     // Ready to send/receive frames
 * }
 * @endcode
 *
 * @invariant initialized == 0 or initialized == 1
 * @invariant rx_buffer_pos <= rx_buffer_len
 * @invariant rx_buffer_len <= k_usb_comm_rx_buffer_size
 *
 * @warning Do not access fields directly; use API functions
 * @attention Handle must be initialized via rx_usb_comm_init() before use
 *
 * @see rx_usb_comm_init() Initialize handle
 * @see rx_usb_comm_deinit() Cleanup handle
 * @see rx_usb_comm_config_t Configuration options
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Frame encoder state machine
   * @details Encodes payloads into framed format with sync, header, and CRC.
   * @see rx_frame_encoder_t
   */
  rx_frame_encoder_t encoder;

  /**
   * @brief Frame decoder state machine
   * @details Decodes incoming bytes, validates CRC, extracts payload.
   * @see rx_frame_decoder_t
   */
  rx_frame_decoder_t decoder;

  /**
   * @brief RX staging buffer
   * @details
   * Accumulates incoming USB data before frame decoding.
   * @par Size: k_usb_comm_rx_buffer_size (2048 bytes)
   * @invariant Valid data in [0, rx_buffer_len)
   */
  uint8_t rx_buffer[k_usb_comm_rx_buffer_size];

  /**
   * @brief TX staging buffer
   * @details
   * Holds assembled frame for USB transmission.
   * @par Size: k_usb_comm_tx_buffer_size (2048 bytes)
   */
  uint8_t tx_buffer[k_usb_comm_tx_buffer_size];

  /**
   * @brief Number of valid bytes in RX buffer
   * @details
   * Bytes [0, rx_buffer_len) contain unprocessed data.
   * @invariant rx_buffer_len <= k_usb_comm_rx_buffer_size
   */
  uint32_t rx_buffer_len;

  /**
   * @brief Current read position in RX buffer
   * @details
   * Next byte to process is rx_buffer[rx_buffer_pos].
   * @invariant rx_buffer_pos <= rx_buffer_len
   */
  uint32_t rx_buffer_pos;

  /**
   * @brief Shared session state for cross-transport sequence continuity
   * @details
   * Points to a single rx_session_state_t instance owned by the comm_manager.
   * Both USB and SPI transports share this state so that sequence numbers
   * remain continuous across transport switches.
   * @see rx_session_state_t
   * @see rx_session_next_tx() Get next TX sequence
   * @see rx_session_validate_rx() Validate incoming RX sequence
   */
  rx_session_state_t* session;

  /**
   * @brief Initialization status flag
   * @details
   * Set to 1 after successful rx_usb_comm_init(), 0 otherwise.
   * @par Values: 0 = not initialized, 1 = initialized
   */
  uint8_t initialized;

  /**
   * @brief Current communication mode
   * @details
   * Controls binary vs debug mode operation.
   * @see rx_usb_comm_mode_t
   */
  rx_usb_comm_mode_t mode;

  /**
   * @brief Optional time interface for timeout handling
   * @details
   * When non-NULL, provides timing functions for accurate timeouts.
   * When NULL, uses internal default timing.
   * @see rx_time_interface_t
   */
  const rx_time_interface_t* time_iface;

  /**
   * @brief PING control frame callback (optional)
   *
   * @details
   * Invoked after auto-PONG is sent in response to a received PING frame.
   * Allows application to track ping activity or update heartbeat state.
   * Set via rx_usb_comm_set_control_callbacks(). NULL disables callback.
   *
   * @param[in] frame The received PING frame (valid for callback duration only)
   * @param[in] ctx Opaque user context from cb_ctx field
   * @return void
   *
   * @pre PONG has already been sent before this callback fires
   * @post No state modification requirements
   *
   * @see rx_usb_comm_set_control_callbacks()
   * @since Version 1.0.0
   */
  void (*on_ping_cb)(const rx_frame_t* frame, void* ctx);

  /**
   * @brief RESET control frame callback (optional)
   *
   * @details
   * Invoked after RESET_ACK is sent and session is reset in response to
   * a received RESET frame. Allows application to perform additional cleanup.
   * Set via rx_usb_comm_set_control_callbacks(). NULL disables callback.
   *
   * @param[in] frame The received RESET frame (valid for callback duration only)
   * @param[in] ctx Opaque user context from cb_ctx field
   * @return void
   *
   * @pre RESET_ACK sent and session reset before this callback fires
   * @post No state modification requirements
   *
   * @see rx_usb_comm_set_control_callbacks()
   * @since Version 1.0.0
   */
  void (*on_reset_cb)(const rx_frame_t* frame, void* ctx);

  /**
   * @brief User context pointer passed to control frame callbacks
   *
   * @details
   * Opaque pointer forwarded to on_ping_cb and on_reset_cb as the ctx
   * parameter. Allows callbacks to access application state without globals.
   * Ownership remains with the caller; this module does not free cb_ctx.
   *
   * @see rx_usb_comm_set_control_callbacks()
   * @since Version 1.0.0
   */
  void* cb_ctx;
} rx_usb_comm_handle_t;

/**
 * @struct rx_usb_comm_config_t
 * @brief USB communication configuration options
 *
 * @details
 * Configuration passed to rx_usb_comm_init(). The session pointer is
 * required for sequence tracking; the time interface is optional.
 *
 * @par Default Values
 * | Field       | Default | Description                          |
 * |-------------|---------|--------------------------------------|
 * | session     | (req'd) | Shared session state pointer         |
 * | time_iface  | NULL    | Use internal timing                  |
 *
 * @par Usage Example
 * @code
 * static rx_session_state_t g_session;
 * rx_session_init(&g_session);
 *
 * rx_usb_comm_config_t config = {
 *     .session    = &g_session,
 *     .time_iface = &my_time_interface
 * };
 * rx_usb_comm_init(&handle, &config);
 * @endcode
 *
 * @see rx_usb_comm_init() Uses this configuration
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Shared session state for cross-transport sequence tracking
   * @details
   * Pointer to the session state owned by rx_comm_manager. Must be
   * initialized via rx_session_init() before passing to rx_usb_comm_init().
   * Required - must not be NULL.
   * @see rx_session_state_t
   */
  rx_session_state_t* session;

  /**
   * @brief Time interface for timeout handling
   * @details
   * Optional interface providing timing functions. When NULL, the
   * module uses internal defaults (may be less accurate).
   * @par Recommended: Provide for accurate timeout handling
   * @see rx_time_interface_t
   */
  const rx_time_interface_t* time_iface;
} rx_usb_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize USB communication handle
 *
 * @details
 * Sets up frame encoder/decoder, clears internal buffers, and prepares the
 * handle for communication. The USB driver must be initialized separately
 * via rx_usb_init() before this handle can successfully send/receive data.
 *
 * @par Algorithm
 * 1. Validate handle pointer
 * 2. Clear handle memory (memset to 0)
 * 3. Initialize frame encoder and decoder
 * 4. Copy configuration (or use defaults if nullptr)
 * 5. Set mode to k_usb_comm_mode_binary
 * 6. Set initialized flag
 *
 * @param[out] handle Pointer to handle to initialize
 *   - Must be valid non-nullptr
 *   - All fields will be overwritten
 *   - Caller maintains ownership
 * @param[in] config Configuration options (must not be NULL)
 *   - session: Required shared session state pointer
 *   - time_iface: Optional time interface (NULL for defaults)
 *   - Structure is copied, need not persist
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Handle initialized successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 *
 * @pre handle points to allocated rx_usb_comm_handle_t
 * @pre USB driver initialized via rx_usb_init() (for actual communication)
 *
 * @post handle->initialized == 1
 * @post handle->mode == k_usb_comm_mode_binary
 * @post Sequence counters reset to 0
 * @post Buffers cleared
 *
 * @note This handle operates on Port 0 (k_usb_port_proto) for protocol
 * @note Safe to call multiple times (reinitializes)
 *
 * @par Example
 * @code
 * static rx_usb_comm_handle_t usb_comm;
 *
 * void init(void) {
 *     // Initialize USB driver first
 *     rx_usb_init();
 *
 *     // Then initialize communication layer
 *     rx_err_t err = rx_usb_comm_init(&usb_comm, NULL);
 *     if (err != k_rx_ok) {
 *         // Handle error
 *     }
 * }
 * @endcode
 *
 * @par Performance
 * ~50-100 us (buffer clearing + encoder/decoder init)
 *
 * @see rx_usb_comm_deinit() Cleanup resources
 * @see rx_usb_init() USB driver initialization (prerequisite)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_usb_comm_init(rx_usb_comm_handle_t*       handle,
                                        const rx_usb_comm_config_t* config);

/**
 * @brief Deinitialize USB communication handle
 *
 * @param[in,out] handle Pointer to handle
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_usb_comm_deinit(rx_usb_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send a response frame over USB
 *
 * @details
 * Encodes the payload into a frame with automatic sequence number assignment
 * and CRC-32 calculation. The frame is queued to the USB TX buffer for
 * transmission. This operation is non-blocking - it returns as soon as the
 * data is queued (not when transmission completes).
 *
 * @par Frame Structure (Generated)
 * ```
 * +------+------+------+------+--------+------------+-------+
 * | SYNC | TYPE | FLAG | SEQ  | LENGTH | PAYLOAD    | CRC32 |
 * | 0xAA | type | flags| auto | len    | data       | auto  |
 * +------+------+------+------+--------+------------+-------+
 * ```
 *
 * @par Algorithm
 * 1. Validate handle, payload pointers
 * 2. Check initialization and mode (must be binary mode)
 * 3. Verify USB is configured and ready
 * 4. Check payload fits in buffer
 * 5. Build frame header with current sequence number
 * 6. Copy payload to frame
 * 7. Calculate and append CRC-32
 * 8. Queue frame to USB TX buffer
 * 9. Increment sequence counter
 *
 * @param[in,out] handle Initialized handle
 *   - Must be initialized via rx_usb_comm_init()
 *   - Session TX sequence incremented on success
 * @param[in] type Frame type (typically k_frame_type_response)
 *   - See rx_frame_type_t for valid types
 * @param[in] flags Frame flags (ORed together)
 *   - See rx_frame_flags_t for valid flags
 *   - 0 for no flags
 * @param[in] payload Pointer to payload data
 *   - Must not be nullptr (even for zero-length payload, use dummy)
 *   - Copied to internal buffer (no ownership transfer)
 * @param[in] payload_len Payload length in bytes
 *   - Max: k_usb_comm_tx_buffer_size - frame overhead (~2036 bytes)
 *   - 0 is valid (empty payload)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame queued successfully
 * @retval k_rx_err_invalid_arg handle or payload is nullptr
 * @retval k_rx_err_invalid_state Not initialized, wrong mode, or USB not configured
 * @retval k_rx_err_invalid_size Payload exceeds maximum frame size
 * @retval k_rx_err_busy USB TX buffer full, try again later
 *
 * @pre handle initialized via rx_usb_comm_init()
 * @pre handle->mode == k_usb_comm_mode_binary
 * @pre USB driver initialized and configured
 * @pre payload !=nullptr
 *
 * @post On success: session TX sequence incremented
 * @post On success: frame queued for transmission
 * @post On failure: no state changes
 *
 * @note Non-blocking: returns immediately after queueing
 * @note Sends on Port 0 (k_usb_port_proto)
 * @note Thread safety: caller must provide external synchronization
 *
 * @warning In debug mode, returns k_rx_err_invalid_state
 *
 * @par Example
 * @code
 * // Send protobuf-encoded response
 * uint8_t pb_buffer[256];
 * size_t pb_len = encode_protobuf_response(pb_buffer, sizeof(pb_buffer));
 *
 * rx_err_t err = rx_usb_comm_send(
 *     &handle,
 *     k_frame_type_response,
 *     0,  // no flags
 *     pb_buffer,
 *     (uint32_t)pb_len
 * );
 *
 * if (err == k_rx_err_busy) {
 *     // TX buffer full, retry after short delay
 *     tx_thread_sleep(1);
 *     err = rx_usb_comm_send(...);
 * }
 * @endcode
 *
 * @par Performance
 * ~10-50 us (encoding + USB queue)
 *
 * @see rx_usb_comm_receive() Receive frames from peer
 * @see rx_frame_type_t Frame type definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_usb_comm_send(rx_usb_comm_handle_t* handle,
                                        rx_frame_type_t       type,
                                        uint8_t               flags,
                                        const uint8_t*        payload,
                                        uint32_t              payload_len);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode a frame from USB
 *
 * @details
 * Reads data from the USB CDC buffer, feeds it through the frame decoder,
 * validates CRC-32, and extracts the payload. The function can operate in
 * blocking mode (with timeout) or polling mode (timeout_ms = 0).
 *
 * @par Algorithm
 * 1. Validate handle and frame pointers
 * 2. Check initialization and USB ready state
 * 3. Loop until frame complete or timeout:
 *    a. Read available bytes from USB buffer
 *    b. Feed bytes to frame decoder
 *    c. Check if complete frame received
 *    d. If not complete and timeout_ms > 0, wait briefly
 * 4. On complete frame: validate CRC, check sequence
 * 5. Copy decoded frame to output
 * 6. Update expected sequence number
 *
 * @dot
 * digraph receive_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   validate [label="Validate params"];
 *   check_usb [label="USB ready?"];
 *   read_usb [label="Read USB buffer"];
 *   decode [label="Feed to decoder"];
 *   complete [label="Frame complete?" shape=diamond];
 *   timeout_check [label="Timeout?" shape=diamond];
 *   wait [label="Brief sleep"];
 *   validate_crc [label="Validate CRC"];
 *   success [label="Return frame" style=filled, fillcolor=lightgreen];
 *   timeout_err [label="Return timeout" style=filled, fillcolor=lightyellow];
 *   crc_err [label="Return CRC error" style=filled, fillcolor=lightyellow];
 *   start -> validate;
 *   validate -> check_usb;
 *   check_usb -> read_usb;
 *   read_usb -> decode;
 *   decode -> complete;
 *   complete -> validate_crc [label="yes"];
 *   complete -> timeout_check [label="no"];
 *   timeout_check -> timeout_err [label="yes"];
 *   timeout_check -> wait [label="no"];
 *   wait -> read_usb;
 *   validate_crc -> success [label="ok"];
 *   validate_crc -> crc_err [label="fail"];
 * }
 * @enddot
 *
 * @param[in,out] handle Initialized handle
 *   - Must be initialized via rx_usb_comm_init()
 *   - Internal buffers and decoder state modified
 * @param[out] frame Pointer to frame structure for output
 *   - Must be valid non-nullptr
 *   - All fields populated on success
 * @param[in] timeout_ms Timeout in milliseconds
 *   - 0: Poll once, return immediately if no frame
 *   - >0: Block up to timeout_ms waiting for complete frame
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Complete frame received and decoded
 * @retval k_rx_err_invalid_arg handle or frame is nullptr
 * @retval k_rx_err_invalid_state Not initialized or USB not configured
 * @retval k_rx_err_timeout No complete frame within timeout (timeout_ms > 0)
 * @retval k_rx_err_no_data No data available (timeout_ms == 0)
 * @retval k_rx_err_crc_mismatch CRC-32 validation failed
 * @retval k_rx_err_protocol_error Invalid frame format (bad sync, length)
 *
 * @pre handle initialized via rx_usb_comm_init()
 * @pre USB driver initialized and configured
 * @pre frame points to valid rx_frame_t
 *
 * @post On success: frame contains decoded data
 * @post On success: session RX sequence updated
 * @post On CRC error: frame may be partially populated
 *
 * @note Receives from Port 0 (k_usb_port_proto)
 * @note Loop iterations bounded by k_usb_comm_max_receive_iterations
 * @note Thread safety: caller must provide external synchronization
 *
 * @par Example
 * @code
 * rx_frame_t frame;
 *
 * // Blocking receive with 100ms timeout
 * rx_err_t err = rx_usb_comm_receive(&handle, &frame, 100);
 *
 * switch (err) {
 *     case k_rx_ok:
 *         // Process frame (USB CDC: no ACK needed)
 *         process_frame(&frame);
 *         break;
 *     case k_rx_err_timeout:
 *         // No frame received, normal in polling scenarios
 *         break;
 *     case k_rx_err_crc_mismatch:
 *         // Corrupted frame, discard (USB hardware handles retries)
 *         rx_log_warn("USB", "CRC mismatch");
 *         break;
 *     default:
 *         // Other error
 *         rx_log_error("USB", "Receive error");
 *         break;
 * }
 * @endcode
 *
 * @par Performance
 * - Polling (timeout_ms=0): ~5-20 us
 * - With timeout: depends on data availability
 *
 * @see rx_usb_comm_send() Send frames to peer
 * @see rx_usb_comm_data_available() Check if data waiting
 * @see rx_frame_t Frame structure definition
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_usb_comm_receive(rx_usb_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

/**
 * @brief Check if data is available for reading
 *
 * @param[in]  handle Initialized handle
 * @param[out] available True if at least one byte is waiting
 *
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_usb_comm_data_available(const rx_usb_comm_handle_t* handle,
                                                  bool*                       available);

/**
 * @brief Check if USB is connected and ready
 *
 * @note Checks Port 0 (k_usb_port_proto) status.
 *
 * @param[in]  handle Initialized handle
 * @param[out] ready True if USB is configured and ready for communication
 *
 * @return k_rx_ok on success
 */
[[nodiscard]] rx_err_t rx_usb_comm_is_ready(const rx_usb_comm_handle_t* handle, bool* ready);

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Flush internal receive buffer
 *
 * Discards any partially received data.
 *
 * @param[in,out] handle Pointer to handle
 */
void rx_usb_comm_flush_rx(rx_usb_comm_handle_t* handle);

/* =============================================================================
 * Mode Switching API
 * =============================================================================
 */

/**
 * @brief Set USB communication mode
 *
 * @details
 * Switches between binary protocol mode and debug text mode at runtime.
 * This allows the same USB connection to be used for robot control (binary)
 * or interactive debugging (text) depending on the development phase.
 *
 * @par Mode Behaviors
 * - **Binary mode (default):** Frame-based protocol for robot control.
 *   Use rx_usb_comm_send/receive for framed data transfer.
 * - **Debug mode:** Plain ASCII text output for debugging.
 *   Use rx_usb_puts(k_usb_port_debug, ...) for text output.
 *   rx_usb_comm_send() returns k_rx_err_invalid_state in this mode.
 *
 * @param[in,out] handle Initialized handle
 *   - Mode field will be updated
 * @param[in] mode New mode to set
 *   - k_usb_comm_mode_binary or k_usb_comm_mode_debug
 *   - k_usb_comm_mode_invalid is rejected
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Mode changed successfully
 * @retval k_rx_err_invalid_arg handle is nullptr or mode is invalid
 * @retval k_rx_err_invalid_state Handle not initialized
 *
 * @pre handle initialized via rx_usb_comm_init()
 *
 * @post handle->mode == mode (on success)
 *
 * @note Does not flush buffers - pending data remains
 * @note Thread safety: caller must provide external synchronization
 *
 * @par Example
 * @code
 * // Switch to debug mode for interactive testing
 * rx_usb_comm_set_mode(&handle, k_usb_comm_mode_debug);
 * rx_usb_puts(k_usb_port_debug, "Debug mode active\\r\\n");
 *
 * // Switch back to binary mode for robot control
 * rx_usb_comm_set_mode(&handle, k_usb_comm_mode_binary);
 * @endcode
 *
 * @see rx_usb_comm_get_mode() Query current mode
 * @see rx_usb_comm_mode_t Mode definitions
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_usb_comm_set_mode(rx_usb_comm_handle_t* handle, rx_usb_comm_mode_t mode);

/**
 * @brief Get current USB communication mode
 *
 * @param[in] handle Initialized handle
 * @return Current mode, or k_usb_comm_mode_invalid if handle is nullptr/uninitialized
 */
rx_usb_comm_mode_t rx_usb_comm_get_mode(const rx_usb_comm_handle_t* handle);

/* =============================================================================
 * Control Frame API
 * =============================================================================
 */

/**
 * @brief Register callbacks for control frame notifications
 *
 * @details
 * Sets optional callbacks invoked when PING or RESET control frames are
 * received. The auto-response (PONG or RESET_ACK) is always sent regardless
 * of whether callbacks are registered.
 *
 * @param[in,out] handle Initialized USB communication handle
 * @param[in] on_ping_cb Callback for PING frames (may be NULL to disable)
 * @param[in] on_reset_cb Callback for RESET frames (may be NULL to disable)
 * @param[in] cb_ctx User context pointer passed to callbacks
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, callbacks registered
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be initialized via rx_usb_comm_init()
 * @post on_ping_cb, on_reset_cb, cb_ctx stored in handle
 *
 * @see rx_usb_comm_receive() Control frames handled automatically during receive
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_usb_comm_set_control_callbacks(rx_usb_comm_handle_t* handle,
                                  void (*on_ping_cb)(const rx_frame_t* frame, void* ctx),
                                  void (*on_reset_cb)(const rx_frame_t* frame, void* ctx),
                                  void* cb_ctx);

/**
 * @brief Send a PONG response frame
 *
 * @details
 * Constructs and sends a PONG frame echoing the provided payload.
 * Called automatically by the receive path when a PING frame is received.
 * Can also be called manually if needed.
 *
 * @param[in,out] handle Initialized USB communication handle
 * @param[in] payload Payload to echo (from received PING, may be NULL)
 * @param[in] payload_len Length of payload in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok PONG sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized or USB not ready
 *
 * @pre handle must be initialized via rx_usb_comm_init()
 * @post TX sequence incremented via shared session
 *
 * @see rx_usb_comm_receive() Calls this automatically for PING frames
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_usb_comm_send_pong(rx_usb_comm_handle_t* handle, const uint8_t* payload, uint16_t payload_len);

/**
 * @brief Send a RESET_ACK response frame
 *
 * @details
 * Constructs and sends a RESET_ACK frame (no payload). Called automatically
 * by the receive path when a RESET frame is received.
 *
 * @param[in,out] handle Initialized USB communication handle
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok RESET_ACK sent successfully
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized or USB not ready
 *
 * @pre handle must be initialized via rx_usb_comm_init()
 * @post TX sequence incremented via shared session
 *
 * @see rx_usb_comm_receive() Calls this automatically for RESET frames
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_usb_comm_send_reset_ack(rx_usb_comm_handle_t* handle);

#ifdef __cplusplus
}
#endif
