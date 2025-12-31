/**
 * @file usb_comm_task.h
 * @brief Public API for the USB CDC Communication ThreadX task
 *
 * This task handles Protocol Buffer communication with the RPi5 controller
 * over USB CDC-ACM (virtual serial port). It receives commands and sends
 * telemetry/status responses.
 *
 * Key Features:
 * - USB CDC-ACM virtual serial port (/dev/ttyACM0 on RPi5)
 * - Protocol Buffer encoding/decoding via nanopb
 * - Frame layer with CRC-32 validation
 * - Lower priority than SPI (USB for telemetry, SPI for real-time)
 * - Integration with motor control via shared state
 *
 * Communication Flow:
 * 1. RPi5 sends command frame via USB → RX72N decodes → dispatches to handler
 * 2. Handler processes command or queries telemetry
 * 3. RX72N encodes response → sends back via USB
 *
 * @see rx_usb_comm.h for USB communication layer
 * @see rx_usb.h for USB CDC driver
 * @see rx_nanopb.h for Protocol Buffer encoding/decoding
 *
 * @date 2025-12-30
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef USB_COMM_TASK_H
#define USB_COMM_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motor_shared_state.h"
#include "rx_err.h"
#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief USB communication task configuration
 */
typedef enum {
  k_usb_comm_task_poll_hz    = 50,   /**< USB polling frequency (Hz) */
  k_usb_comm_task_period_ms  = 20,   /**< Polling period (ms) - 1000/50 */
  k_usb_comm_task_stack_size = 4096, /**< Stack size (bytes) */
  k_usb_comm_task_priority   = 10,   /**< Task priority (lower than SPI) */
} usb_comm_task_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start the USB communication task
 *
 * Initializes USB CDC driver, frame encoder/decoder, and nanopb wrapper,
 * then starts the task that handles RPi5 communication via USB.
 *
 * This task handles:
 * - Receiving command frames from RPi5 via USB CDC
 * - Decoding Protocol Buffer messages
 * - Processing telemetry requests
 * - Processing configuration commands
 * - Sending telemetry and status responses
 *
 * @param[in] shared_state Pointer to initialized motor shared state
 *
 * @return RX_OK on success
 * @return RX_ERR_INVALID_ARG if shared_state is NULL
 * @return RX_ERR_INVALID_STATE if task creation fails
 *
 * @note The shared state must be initialized before calling this function
 * @note USB driver is initialized by this function
 */
rx_err_t usb_comm_task_create(motor_shared_state_t* shared_state);

/**
 * @brief Get USB communication statistics
 *
 * Returns statistics about frames sent/received, errors, etc.
 *
 * @param[out] frames_rx Number of frames received
 * @param[out] frames_tx Number of frames transmitted
 * @param[out] crc_errors Number of CRC validation failures
 * @param[out] decode_errors Number of Protocol Buffer decode failures
 *
 * @return RX_OK on success
 */
rx_err_t usb_comm_task_get_stats(uint32_t* frames_rx,
                                 uint32_t* frames_tx,
                                 uint32_t* crc_errors,
                                 uint32_t* decode_errors);

/**
 * @brief Reset USB communication statistics
 *
 * Clears all counters to zero.
 */
void usb_comm_task_reset_stats(void);

/**
 * @brief Check if USB is connected and enumerated
 *
 * @return true if USB is configured and ready for communication
 * @return false if USB is not connected or not enumerated
 */
bool usb_comm_task_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_COMM_TASK_H */
