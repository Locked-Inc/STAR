/**
 * @file bb_usb_cdc.h
 * @brief USB CDC ACM serial transport layer for BeagleBone Blue.
 *
 * @details
 * Provides byte-stream send/receive over a USB CDC ACM serial port
 * (/dev/ttyACMx). The BeagleBone Blue connects to the RPi5 gateway via USB;
 * the gateway uses its existing CDC transport with no changes required.
 *
 * Architecture:
 * - bb_usb_cdc_init() opens the tty device and configures it for raw binary
 *   I/O via termios (no echo, no line discipline, 8N1).
 * - bb_usb_cdc_send() writes to the file descriptor with a blocking write.
 * - bb_usb_cdc_receive() performs a non-blocking read (O_NONBLOCK).
 * - bb_usb_cdc_is_connected() checks if the file descriptor is valid and the
 *   carrier detect (CD) line is asserted.
 *
 * Thread safety:
 * - bb_usb_cdc_send() is guarded by an internal pthread_mutex_t.
 * - bb_usb_cdc_receive() is guarded by a separate read mutex.
 * - Only one thread should call send; only one thread should call receive.
 *
 * @see tasks/inc/comm_task.h Consumer of this transport
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bb_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the USB CDC serial transport.
 *
 * @details
 * Opens the tty device at device_path (e.g. "/dev/ttyACM0"), configures
 * termios for raw binary mode (8N1, no echo, no line discipline), and
 * initializes the internal send/receive mutexes.
 *
 * @param[in] device_path Null-terminated path to the tty device
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Initialization successful
 * @retval k_bb_err_null_ptr        device_path is NULL
 * @retval k_bb_err_already_initialized Module already initialized
 * @retval k_bb_err_io              open() or tcsetattr() failed (check errno)
 * @retval k_bb_err_hardware        pthread_mutex_init() failed
 *
 * @pre device_path must point to a valid null-terminated string
 * @pre Module must not already be initialized
 * @post Internal file descriptor valid and configured for raw I/O
 * @post Send and receive mutexes initialized
 *
 * @note Call once from main() before spawning comm_task thread
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_init(const char* device_path);

/**
 * @brief De-initialize the USB CDC serial transport.
 *
 * @details
 * Closes the file descriptor and destroys the internal mutexes.
 * Must be called from normal thread context (NOT from a signal handler)
 * because pthread_mutex_destroy is not async-signal-safe.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok             De-initialization successful
 * @retval k_bb_err_not_initialized Module was not initialized
 *
 * @pre bb_usb_cdc_init() must have been called
 * @post File descriptor closed; module returns to uninitialized state
 *
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_deinit(void);

/**
 * @brief Send bytes over USB CDC.
 *
 * @details
 * Acquires the send mutex, writes all len bytes to the file descriptor
 * using write(), and releases the mutex. Blocks until all bytes are written
 * or timeout_ms elapses.
 *
 * @param[in] data       Buffer to transmit
 * @param[in] len        Number of bytes to send
 * @param[in] timeout_ms Maximum milliseconds to block (0 = no timeout)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       All bytes sent
 * @retval k_bb_err_null_ptr data is NULL
 * @retval k_bb_err_not_initialized Module not initialized
 * @retval k_bb_err_timeout  Bytes not fully sent within timeout_ms
 * @retval k_bb_err_io       write() failed (check errno)
 *
 * @pre bb_usb_cdc_init() must have been called
 * @pre data must point to a valid buffer of at least len bytes
 * @post len bytes written to the tty file descriptor
 *
 * @note Not re-entrant; one sender thread only
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_send(const uint8_t* data, uint32_t len, uint32_t timeout_ms);

/**
 * @brief Receive bytes from USB CDC (non-blocking).
 *
 * @details
 * Acquires the receive mutex and performs a non-blocking read() from the
 * file descriptor. Returns immediately with however many bytes are available.
 * Sets *out_len = 0 if no data is available.
 *
 * @param[out] buf      Destination buffer
 * @param[in]  max_len  Maximum bytes to read
 * @param[out] out_len  Actual bytes read (0 if no data available)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read succeeded (out_len may be 0)
 * @retval k_bb_err_null_ptr buf or out_len is NULL
 * @retval k_bb_err_not_initialized Module not initialized
 * @retval k_bb_err_io       read() failed (check errno)
 *
 * @pre bb_usb_cdc_init() must have been called
 * @pre buf must point to a valid buffer of at least max_len bytes
 * @post *out_len set to number of bytes written to buf
 *
 * @note Non-blocking; comm_task polls this at 100 Hz with nanosleep between
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_receive(uint8_t* buf, uint32_t max_len, uint32_t* out_len);

/**
 * @brief Check whether the CDC host is connected.
 *
 * @details
 * Returns true if the file descriptor is open and the TIOCMGET ioctl reports
 * TIOCM_CAR (carrier detect) asserted, indicating the host port is open.
 *
 * @return bool true if host CDC port is open, false otherwise
 *
 * @pre bb_usb_cdc_init() must have been called
 * @post No side effects
 *
 * @since Version 1.0.0
 */
bool bb_usb_cdc_is_connected(void);

#ifdef __cplusplus
}
#endif
