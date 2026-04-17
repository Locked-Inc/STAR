/**
 * @file usb_task.h
 * @brief Dedicated USB polling task (priority 4, above comm_task)
 *
 * @details
 * On this RX72N board the USB0 USBI interrupt vector (36) does not fire --
 * verified with halt-on-entry ISRs on vectors 36, 62, 102, 185.  The
 * production rx_usb stack is otherwise correct; we just have to drive
 * `rx_usb_isr_handler()` from a ThreadX polling loop instead of relying
 * on interrupts.
 *
 * The poller lives in its own task (not inline in comm_task) so that:
 *
 *   1. `rx_usb_init()` can run in ThreadX context (its clock-stabilisation
 *      waits use `tx_thread_sleep` and would fault pre-kernel).
 *   2. USB comes up BEFORE comm_task's `internal_init_transports()` which
 *      takes ~50 ms configuring SPI / I2C / UART and would otherwise push
 *      the host's enumeration retries off their deadline.
 *   3. SETUP / BRDY / BEMP latency stays near the 10 ms ThreadX tick
 *      irrespective of lower-priority work.
 *
 * Priority 4 is one higher than comm_task (5), so tx_kernel_enter schedules
 * this task first.  After its init phase the task yields via
 * `tx_thread_sleep(1)` between polls so the scheduler gives lower-priority
 * tasks CPU time.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the USB polling task.
 *
 * @return k_rx_ok on success, k_rx_err_rtos_thread_create otherwise.
 *
 * @pre Called from tx_application_define() before any task that uses
 *      rx_usb_read() / rx_usb_write() (notably comm_task).
 *
 * @post USB polling task scheduled at priority 4; rx_usb_init() will run
 *       in its entry function before it enters the poll loop.
 */
rx_err_t usb_task_create(void);
