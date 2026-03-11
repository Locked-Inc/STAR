/**
 * @file rx_i2c_comm.h
 * @brief High-Level I2C Peripheral Communication Layer for RX72N
 *
 * @details
 * Integrates the Frame Layer and RIIC0 (I2C) peripheral driver for reliable
 * communication with the RPi5 controller acting as I2C bus controller.
 * The RX72N acts as an I2C peripheral (device), responding to a fixed
 * 7-bit address set during initialization. This provides an alternative
 * communication channel when SPI or USB CDC are unavailable.
 *
 * @par System Architecture
 * @dot
 * digraph i2c_comm_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rpi5 {
 *     label="Raspberry Pi 5 (I2C Controller)";
 *     style=filled;
 *     color=lightblue;
 *     i2c_dev [label="/dev/i2c-1\n(I2C bus controller)"];
 *     ros2 [label="ROS2 Node\nProtobuf Messages"];
 *   }
 *
 *   subgraph cluster_i2c {
 *     label="I2C Bus (SCL0/SDA0 - RIIC0)";
 *     style=filled;
 *     color=lightyellow;
 *     i2c_hw [label="RIIC0 Hardware\n100kHz/400kHz"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N Firmware (I2C Peripheral)";
 *     style=filled;
 *     color=lightgreen;
 *     i2c_comm [label="rx_i2c_comm\n(This Module)"];
 *     frame [label="rx_frame\nEncoder/Decoder"];
 *     nanopb [label="nanopb\nProtobuf"];
 *   }
 *
 *   ros2 -> i2c_dev [label="Protobuf"];
 *   i2c_dev -> i2c_hw [label="I2C write/read"];
 *   i2c_hw -> i2c_comm [label="RIIC0 peripheral"];
 *   i2c_comm -> frame;
 *   frame -> nanopb;
 * }
 * @enddot
 *
 * @par Peripheral Mode Operation
 * The RX72N acts as an I2C peripheral device at a configurable 7-bit address.
 * The RPi5 initiates all transactions as I2C controller. The RX72N uses
 * polling (ICSR2.RDRF for receive, ICSR2.TDRE for transmit) to handle
 * incoming writes and prepare outgoing read responses.
 *
 * @par Features
 * - Frame encoding/decoding with CRC-32 error detection
 * - Automatic sequence number tracking for reliable delivery
 * - Non-blocking receive operations
 * - Same wire protocol as USB/SPI/UART (interchangeable transport)
 * - Configurable I2C channel and device address
 *
 * @par Frame Protocol
 * Uses the same frame format as USB, SPI, and UART communication:
 * ```
 * +------+------+------+------+--------+-----------+-------+
 * | SYNC | TYPE | FLAG | SEQ  | LENGTH | PAYLOAD   | CRC32 |
 * | 0xAA | 1B   | 1B   | 2B   | 2B     | N bytes   | 4B    |
 * +------+------+------+------+--------+-----------+-------+
 * ```
 *
 * @par I2C Channel Assignment
 * | Channel | Peripheral | Addr  | Purpose                      |
 * |---------|------------|-------|------------------------------|
 * | RIIC0   | k_i2c_comm_default_channel | 0x42 (default) | Binary protocol (nanopb) |
 *
 * @par Performance Characteristics
 * | Metric                | Value          | Notes                           |
 * |-----------------------|----------------|---------------------------------|
 * | I2C speed             | 400kHz max     | Fast mode supported             |
 * | Frame latency         | ~1-5 ms        | Depends on I2C clock speed      |
 * | Max frame size        | 2048 bytes     | Buffer size limit               |
 *
 * @par Memory Usage
 * | Component              | Size     | Description                    |
 * |------------------------|----------|--------------------------------|
 * | rx_i2c_comm_handle_t   | ~4.2 KB  | Per-instance handle            |
 * | RX buffer              | 2048 B   | Staging buffer for receives    |
 * | TX buffer              | 2048 B   | Staging buffer for sends       |
 *
 * @par Thread Safety
 * The handle is **NOT thread-safe**. External synchronization (ThreadX mutex)
 * is required for concurrent access from multiple threads.
 *
 * @par Module Dependencies
 * - rx_frame.h: Frame encoding/decoding layer
 * - hardware.h: I2C HAL (riic_init_peripheral, riic_peripheral_read, riic_peripheral_write)
 * - rx_err.h: Error code definitions
 * - rx_session.h: Cross-transport sequence continuity
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] Loop bounds via k_i2c_comm_max_receive_iterations
 * - Rule 3: [OK] No dynamic memory (static buffers in handle)
 * - Rule 4: [OK] Functions are concise
 * - Rule 5: [OK] Input validation on all public functions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Constants use C23 typed enums
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** I2C communication only, frame details in rx_frame
 * - **O (OCP):** Configurable channel/address via rx_i2c_comm_config_t
 * - **L (LSP):** Same protocol as rx_usb_comm / rx_uart_comm (interchangeable)
 * - **I (ISP):** Clean send/receive interface
 * - **D (DIP):** Depends on hardware.h HAL abstraction
 *
 * @see rx_frame.h Frame layer implementation
 * @see hardware.h I2C HAL
 * @see rx_usb_comm.h USB CDC communication (same protocol)
 * @see rx_uart_comm.h UART communication (same protocol)
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
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
 * @enum rx_i2c_comm_constants_t
 * @brief I2C communication configuration constants
 *
 * @details
 * Compile-time constants for I2C communication buffer sizes and loop bounds.
 * These values are tuned for the RX72N RIIC0 I2C implementation with typical
 * control message sizes.
 *
 * @par Buffer Sizing Rationale
 * - 2048 bytes accommodates largest expected protobuf message + frame overhead
 * - Provides headroom for frame header (8 bytes) and CRC (4 bytes)
 *
 * @invariant Buffer sizes must be >= maximum frame size
 * @invariant k_i2c_comm_max_receive_iterations bounds all receive loops
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief RX staging buffer size in bytes
   * @details
   * Buffer for accumulating incoming I2C data before frame decoding.
   * @par Value: 2048 bytes
   */
  k_i2c_comm_rx_buffer_size = 2048,

  /**
   * @brief TX staging buffer size in bytes
   * @details
   * Buffer for assembled frames before I2C transmission.
   * @par Value: 2048 bytes
   */
  k_i2c_comm_tx_buffer_size = 2048,

  /**
   * @brief Maximum iterations for receive loop (NASA Rule 2)
   * @details
   * Provides static upper bound for receive polling loops to ensure
   * bounded execution time even under pathological conditions.
   * @par Value: 1000 iterations
   */
  k_i2c_comm_max_receive_iterations = 1000,
} rx_i2c_comm_constants_t;

/**
 * @enum rx_i2c_comm_defaults_t
 * @brief Default I2C channel and address for RPi5 communication
 *
 * @details
 * Default hardware channel (RIIC0) and 7-bit I2C device address (0x42) used
 * when the caller does not override via rx_i2c_comm_config_t.
 *
 * @par Address Selection
 * 0x42 ('B' in ASCII) is chosen to avoid conflicts with common I2C sensor
 * addresses (0x20-0x27 GPIO expanders, 0x3C/0x3D OLED, 0x48-0x4F ADC/temp,
 * 0x68/0x69 IMU, 0x76/0x77 barometer).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_i2c_comm_default_channel = 0,    /**< RIIC0 channel */
  k_i2c_comm_default_addr    = 0x42, /**< Default 7-bit device address */
} rx_i2c_comm_defaults_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @struct rx_i2c_comm_handle_t
 * @brief I2C communication handle containing all operational state
 *
 * @details
 * Contains all state for I2C peripheral communication including frame
 * encoder/decoder, sequence tracking, and staging buffers. One handle
 * corresponds to one logical I2C communication channel (RIIC0).
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
 * | 4213   | 1 B      | channel_value  | RIIC channel number            |
 * | 4214   | 1 B      | device_addr    | 7-bit I2C device address       |
 * | **Total** | **~4.2 KB** |          |                                |
 *
 * @invariant initialized == 0 or initialized == 1
 * @invariant rx_buffer_pos <= rx_buffer_len
 * @invariant rx_buffer_len <= k_i2c_comm_rx_buffer_size
 *
 * @warning Do not access fields directly; use API functions
 * @attention Handle must be initialized via rx_i2c_comm_init() before use
 *
 * @see rx_i2c_comm_init() Initialize handle
 * @see rx_i2c_comm_deinit() Cleanup handle
 * @see rx_i2c_comm_config_t Configuration options
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Frame encoder state machine
   * @details Encodes payloads into framed format with sync, header, and CRC.
   */
  rx_frame_encoder_t encoder;

  /**
   * @brief Frame decoder state machine
   * @details Decodes incoming bytes, validates CRC, extracts payload.
   */
  rx_frame_decoder_t decoder;

  /**
   * @brief RX staging buffer
   * @details Accumulates incoming I2C data before frame decoding.
   * @invariant Valid data in [0, rx_buffer_len)
   */
  uint8_t rx_buffer[k_i2c_comm_rx_buffer_size];

  /**
   * @brief TX staging buffer
   * @details Holds assembled frame for I2C transmission.
   */
  uint8_t tx_buffer[k_i2c_comm_tx_buffer_size];

  /**
   * @brief Number of valid bytes in RX buffer
   * @invariant rx_buffer_len <= k_i2c_comm_rx_buffer_size
   */
  uint32_t rx_buffer_len;

  /**
   * @brief Current read position in RX buffer
   * @invariant rx_buffer_pos <= rx_buffer_len
   */
  uint32_t rx_buffer_pos;

  /**
   * @brief Shared session state for cross-transport sequence continuity
   * @details Points to session state owned by rx_comm_manager.
   * @see rx_session_state_t
   */
  rx_session_state_t* session;

  /**
   * @brief Initialization status flag
   * @par Values: 0 = not initialized, 1 = initialized
   */
  uint8_t initialized;

  /**
   * @brief RIIC channel number
   * @details Stores the riic_channel_t.value configured during init.
   */
  uint8_t channel_value;

  /**
   * @brief 7-bit I2C device address for this peripheral
   * @details Stores the i2c_device_addr_t.value configured during init.
   */
  uint8_t device_addr;
} rx_i2c_comm_handle_t;

/**
 * @struct rx_i2c_comm_config_t
 * @brief I2C communication configuration options
 *
 * @details
 * Configuration passed to rx_i2c_comm_init(). The session pointer is
 * required for sequence tracking.
 *
 * @par Default Values
 * | Field       | Default                   | Description                          |
 * |-------------|---------------------------|--------------------------------------|
 * | session     | (required)                | Shared session state pointer         |
 * | channel     | k_i2c_comm_default_channel| RIIC0 is the default I2C channel     |
 * | device_addr | k_i2c_comm_default_addr   | 0x42 default RPi5 peripheral address |
 *
 * @see rx_i2c_comm_init() Uses this configuration
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Shared session state for cross-transport sequence tracking
   * @details Must be initialized via rx_session_init() before passing here.
   * Required - must not be NULL.
   */
  rx_session_state_t* session;

  /**
   * @brief RIIC channel to use
   * @details Selects which RIIC peripheral to use for I2C communication.
   * @par Default: {.value = k_i2c_comm_default_channel} (RIIC0)
   */
  riic_channel_t channel;

  /**
   * @brief 7-bit I2C device address for this RX72N peripheral
   * @details RPi5 will address this device on the I2C bus.
   * @par Default: {.value = k_i2c_comm_default_addr} (0x42)
   */
  i2c_device_addr_t device_addr;
} rx_i2c_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize I2C communication handle in peripheral mode
 *
 * @details
 * Sets up frame encoder/decoder, clears internal buffers, configures RIIC
 * peripheral at the specified I2C device address, and prepares the handle
 * for communication.
 *
 * @par Algorithm
 * 1. Validate handle pointer (NULL check)
 * 2. Clear handle memory
 * 3. Initialize frame encoder and decoder
 * 4. Copy configuration (channel, device_addr, session)
 * 5. Call riic_init_peripheral() to configure hardware
 * 6. Set initialized flag
 *
 * @param[out] handle Pointer to handle to initialize (must not be nullptr)
 * @param[in] config Configuration options (must not be NULL)
 *   - session: Required shared session state pointer
 *   - channel: RIIC channel (default RIIC0)
 *   - device_addr: 7-bit I2C device address
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Handle initialized successfully
 * @retval k_rx_err_invalid_arg handle or config is nullptr, or session is nullptr
 * @retval Other errors from riic_init_peripheral() or frame encoder/decoder init
 *
 * @pre handle points to allocated rx_i2c_comm_handle_t
 * @pre config->session points to initialized rx_session_state_t
 * @post handle->initialized == 1
 * @post RIIC peripheral will respond to config->device_addr on I2C bus
 *
 * @note Not thread-safe; call from single initialization context
 * @note Only initializes software handle; RIIC hardware configured via riic_init_peripheral()
 *
 * @see rx_i2c_comm_deinit() Cleanup resources
 * @see riic_channel_t I2C channel wrapper
 * @see i2c_device_addr_t I2C device address wrapper
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_i2c_comm_init(rx_i2c_comm_handle_t*       handle,
                                         const rx_i2c_comm_config_t* config);

/**
 * @brief Deinitialize I2C communication handle
 *
 * @details
 * Clears the initialized flag to prevent further use of the handle.
 * Does not deinitialize the underlying RIIC hardware.
 *
 * @param[in,out] handle Pointer to initialized handle
 * @return k_rx_ok on success, k_rx_err_invalid_arg if handle is nullptr
 *
 * @pre handle must not be nullptr
 * @post handle->initialized == 0
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_i2c_comm_deinit(rx_i2c_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send a frame over I2C (RIIC peripheral -> controller)
 *
 * @details
 * Encodes the payload into a frame with automatic sequence number assignment
 * and CRC-32 calculation, then transmits it via riic_peripheral_write().
 * The RPi5 (I2C controller) must initiate a read transaction to receive data.
 *
 * @param[in,out] handle Initialized handle
 * @param[in] type Frame type (typically k_frame_type_response)
 * @param[in] flags Frame flags (ORed together, 0 for none)
 * @param[in] payload Pointer to payload data (may be nullptr if payload_len == 0)
 * @param[in] payload_len Payload length in bytes (0 to k_frame_max_payload)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame queued for I2C controller read
 * @retval k_rx_err_invalid_arg handle is nullptr
 * @retval k_rx_err_invalid_state Not initialized
 * @retval k_rx_err_invalid_size Payload exceeds maximum frame size
 * @retval k_rx_err_timeout I2C transmit register not ready within timeout
 *
 * @pre handle initialized via rx_i2c_comm_init()
 * @post On success: session TX sequence incremented
 *
 * @note I2C send requires RPi5 to initiate a read transaction
 * @note Thread safety: caller must provide external synchronization
 *
 * @see rx_i2c_comm_receive() Receive frames from RPi5
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_i2c_comm_send(rx_i2c_comm_handle_t* handle,
                                         rx_frame_type_t        type,
                                         uint8_t                flags,
                                         const uint8_t*         payload,
                                         uint32_t               payload_len);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode a frame from I2C controller (RPi5 write)
 *
 * @details
 * Polls ICSR2.RDRF for incoming bytes written by the I2C controller (RPi5),
 * feeds them through the frame decoder, validates CRC-32, and extracts the
 * payload. Non-blocking: returns k_rx_err_no_data if no complete frame is
 * available.
 *
 * @param[in,out] handle Initialized handle
 * @param[out] frame Pointer to frame structure for output (must not be nullptr)
 * @param[in] timeout_ms Timeout in milliseconds (reserved for future use; poll is non-blocking)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Complete frame received and decoded
 * @retval k_rx_err_invalid_arg handle or frame is nullptr
 * @retval k_rx_err_invalid_state Not initialized
 * @retval k_rx_err_no_data No complete frame available yet
 * @retval k_rx_err_crc_mismatch CRC-32 validation failed
 * @retval k_rx_err_protocol_error Invalid frame format
 *
 * @pre handle initialized via rx_i2c_comm_init()
 * @pre frame points to valid rx_frame_t
 * @post On success: frame contains decoded data, session RX sequence updated
 *
 * @note Loop iterations bounded by k_i2c_comm_max_receive_iterations
 * @note Thread safety: caller must provide external synchronization
 *
 * @see rx_i2c_comm_send() Send frames to RPi5
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_i2c_comm_receive(rx_i2c_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
