/**
 * @file bb_usb_cdc.c
 * @brief USB CDC ACM serial transport layer implementation (POSIX termios).
 *
 * @details
 * Implements byte-stream send/receive over a POSIX tty device (e.g.
 * /dev/ttyACM0). Uses termios to configure raw binary mode and O_NONBLOCK
 * for non-blocking receive.
 *
 * Architecture:
 *   - bb_usb_cdc_init() opens the device, calls termios cfmakeraw(), and
 *     initializes two pthread_mutex_t (one for send, one for receive).
 *   - bb_usb_cdc_send() holds s_send_mutex, calls write() in a loop until
 *     all bytes are sent or timeout_ms elapses.
 *   - bb_usb_cdc_receive() holds s_recv_mutex, calls read() with O_NONBLOCK;
 *     returns 0 bytes if no data is available (EAGAIN / EWOULDBLOCK).
 *   - bb_usb_cdc_is_connected() uses TIOCMGET to check carrier detect.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "bb_usb_cdc.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum usb_cdc_timing_t
 * @brief Timing constants for CDC TX polling.
 *
 * @details
 * k_poll_sleep_ns is the nanosecond sleep between write() retry loops when
 * the kernel TX buffer is temporarily full.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_poll_sleep_ns = 500000U, /**< 0.5 ms sleep between TX retries */
} usb_cdc_timing_t;

/* ---------------------------------------------------------------------------
 * Private state
 * ---------------------------------------------------------------------------*/

/**
 * @var s_fd
 * @brief File descriptor for the tty device. -1 when not initialized.
 *
 * @warning Do not access directly outside this file.
 * @since Version 1.0.0
 */
static int s_fd = -1;

/**
 * @var s_send_mutex
 * @brief Mutex protecting bb_usb_cdc_send() from concurrent callers.
 *
 * @since Version 1.0.0
 */
static pthread_mutex_t s_send_mutex;

/**
 * @var s_recv_mutex
 * @brief Mutex protecting bb_usb_cdc_receive() from concurrent callers.
 *
 * @since Version 1.0.0
 */
static pthread_mutex_t s_recv_mutex;

/* ---------------------------------------------------------------------------
 * Private helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Configure termios for raw binary 8N1 mode.
 *
 * @details
 * Calls cfmakeraw() to disable all line discipline processing, then applies
 * the settings via tcsetattr(). No baud rate is set because USB CDC ACM
 * operates at USB speed regardless of the termios baud setting; however,
 * cfmakeraw() is still required to disable line discipline transformations
 * that would corrupt binary data.
 *
 * @param[in] fd File descriptor of the open tty device
 *
 * @return bb_err_t
 * @retval k_bb_err_ok  termios configured successfully
 * @retval k_bb_err_io  tcgetattr() or tcsetattr() failed
 *
 * @pre fd must be a valid open file descriptor
 * @post tty configured for raw binary 8N1 mode
 *
 * @since Version 1.0.0
 */
static bb_err_t internal_configure_tty(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        return k_bb_err_io;
    }

    cfmakeraw(&tty);
    tty.c_cc[VMIN]  = 0U; /* Non-blocking read */
    tty.c_cc[VTIME] = 0U; /* No read timeout */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return k_bb_err_io;
    }

    return k_bb_err_ok;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize the USB CDC serial transport.
 *
 * @details
 * Opens device_path with O_RDWR | O_NOCTTY | O_NONBLOCK, calls
 * internal_configure_tty() for raw mode, then initializes the send and
 * receive mutexes.
 *
 * @param[in] device_path Null-terminated path to the tty device
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Initialization successful
 * @retval k_bb_err_null_ptr        device_path is NULL
 * @retval k_bb_err_already_initialized Module already initialized
 * @retval k_bb_err_io              open() or tcsetattr() failed
 * @retval k_bb_err_hardware        pthread_mutex_init() failed
 *
 * @pre device_path must point to a valid null-terminated string
 * @pre s_fd must be -1 (not already initialized)
 * @post s_fd is a valid open file descriptor
 * @post s_send_mutex and s_recv_mutex initialized
 *
 * @note Call once from main() before spawning comm_task thread
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_init(const char* device_path)
{
    if (device_path == NULL) {
        return k_bb_err_null_ptr;
    }
    if (s_fd >= 0) {
        return k_bb_err_already_initialized;
    }

    int fd = open(device_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return k_bb_err_io;
    }

    bb_err_t err = internal_configure_tty(fd);
    if (err != k_bb_err_ok) {
        (void)close(fd);
        return err;
    }

    if (pthread_mutex_init(&s_send_mutex, NULL) != 0) {
        (void)close(fd);
        return k_bb_err_hardware;
    }

    if (pthread_mutex_init(&s_recv_mutex, NULL) != 0) {
        (void)pthread_mutex_destroy(&s_send_mutex);
        (void)close(fd);
        return k_bb_err_hardware;
    }

    s_fd = fd;
    return k_bb_err_ok;
}

/**
 * @brief De-initialize the USB CDC serial transport.
 *
 * @details
 * Closes the file descriptor, destroys both mutexes, and resets s_fd to -1.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok             De-initialization successful
 * @retval k_bb_err_not_initialized Module was not initialized
 *
 * @pre bb_usb_cdc_init() must have been called
 * @post s_fd = -1; mutexes destroyed
 *
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_deinit(void)
{
    if (s_fd < 0) {
        return k_bb_err_not_initialized;
    }

    (void)close(s_fd);
    s_fd = -1;

    (void)pthread_mutex_destroy(&s_send_mutex);
    (void)pthread_mutex_destroy(&s_recv_mutex);

    return k_bb_err_ok;
}

/**
 * @brief Send bytes over USB CDC.
 *
 * @details
 * Acquires s_send_mutex, then writes all len bytes via write() in a loop.
 * If write() returns EAGAIN (kernel TX buffer full), sleeps k_poll_sleep_ns
 * and retries until timeout_ms elapses. Releases the mutex before returning.
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
 * @retval k_bb_err_io       write() failed with a fatal error
 *
 * @pre bb_usb_cdc_init() must have been called
 * @pre data must point to a valid buffer of at least len bytes
 * @post len bytes written to the tty file descriptor
 *
 * @note Not re-entrant; one sender thread only
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_send(const uint8_t* data, uint32_t len, uint32_t timeout_ms)
{
    if (data == NULL) {
        return k_bb_err_null_ptr;
    }
    if (s_fd < 0) {
        return k_bb_err_not_initialized;
    }

    struct timespec deadline = {0};
    if (timeout_ms > 0U) {
        (void)clock_gettime(CLOCK_MONOTONIC, &deadline);
        deadline.tv_sec  += (time_t)(timeout_ms / 1000U);
        deadline.tv_nsec += (long)((timeout_ms % 1000U) * 1000000L);
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    (void)pthread_mutex_lock(&s_send_mutex);

    uint32_t written = 0U;
    bb_err_t result = k_bb_err_ok;

    while (written < len) {
        ssize_t n = write(s_fd, &data[written], (size_t)(len - written));

        if (n > 0) {
            written += (uint32_t)n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms > 0U) {
                struct timespec now = {0};
                (void)clock_gettime(CLOCK_MONOTONIC, &now);
                if (now.tv_sec > deadline.tv_sec ||
                    (now.tv_sec == deadline.tv_sec &&
                     now.tv_nsec >= deadline.tv_nsec)) {
                    result = k_bb_err_timeout;
                    break;
                }
            }
            struct timespec sleep_time = {
                .tv_sec  = 0,
                .tv_nsec = (long)k_poll_sleep_ns,
            };
            (void)nanosleep(&sleep_time, NULL);
            continue;
        }

        result = k_bb_err_io;
        break;
    }

    (void)pthread_mutex_unlock(&s_send_mutex);
    return result;
}

/**
 * @brief Receive bytes from USB CDC (non-blocking).
 *
 * @details
 * Acquires s_recv_mutex and calls read() with the O_NONBLOCK file descriptor.
 * If EAGAIN or EWOULDBLOCK, sets *out_len = 0 and returns k_bb_err_ok.
 *
 * @param[out] buf      Destination buffer
 * @param[in]  max_len  Maximum bytes to read
 * @param[out] out_len  Actual bytes read (0 if no data available)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read succeeded (out_len may be 0)
 * @retval k_bb_err_null_ptr buf or out_len is NULL
 * @retval k_bb_err_not_initialized Module not initialized
 * @retval k_bb_err_io       read() failed with a fatal error
 *
 * @pre bb_usb_cdc_init() must have been called
 * @pre buf must point to a valid buffer of at least max_len bytes
 * @post *out_len set to number of bytes written to buf
 *
 * @note Non-blocking; comm_task polls at 100 Hz
 * @since Version 1.0.0
 */
bb_err_t bb_usb_cdc_receive(uint8_t* buf, uint32_t max_len, uint32_t* out_len)
{
    if (buf == NULL || out_len == NULL) {
        return k_bb_err_null_ptr;
    }
    if (s_fd < 0) {
        return k_bb_err_not_initialized;
    }

    (void)pthread_mutex_lock(&s_recv_mutex);

    ssize_t n = read(s_fd, buf, (size_t)max_len);
    bb_err_t result = k_bb_err_ok;

    if (n >= 0) {
        *out_len = (uint32_t)n;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *out_len = 0U;
    } else {
        *out_len = 0U;
        result = k_bb_err_io;
    }

    (void)pthread_mutex_unlock(&s_recv_mutex);
    return result;
}

/**
 * @brief Check whether the CDC host is connected.
 *
 * @details
 * Uses TIOCMGET to check modem control lines. Returns true if TIOCM_CAR
 * (Data Carrier Detect) is asserted, meaning the host port is open.
 *
 * @return bool true if host CDC port is open, false otherwise
 *
 * @pre bb_usb_cdc_init() must have been called
 * @post No side effects
 *
 * @since Version 1.0.0
 */
bool bb_usb_cdc_is_connected(void)
{
    if (s_fd < 0) {
        return false;
    }

    int modem_bits = 0;
    if (ioctl(s_fd, TIOCMGET, &modem_bits) < 0) {
        return false;
    }

    return (modem_bits & TIOCM_CAR) != 0;
}
