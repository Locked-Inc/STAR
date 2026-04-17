/**
 * @file usb_task.c
 * @brief Dedicated USB polling task (priority 4, one above comm_task)
 *
 * @see usb_task.h for rationale.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "usb_task.h"

#include "rx_log.h"
#include "rx_usb.h"
#include "tx_api.h"

typedef enum : uint16_t {
  k_usb_task_stack_size = 1024, /**< ThreadX task stack in bytes. */
  k_usb_task_priority   = 4,    /**< One higher than comm_task (5) so we run first. */
  k_usb_task_input      = 0,    /**< Thread entry input (unused). */
} usb_task_constants_t;

static TX_THREAD s_usb_thread;
static uint8_t   s_usb_stack[k_usb_task_stack_size];
static bool      s_usb_created = false;

static const char* s_tag = "USB_TASK";

/**
 * @brief USB task entry: bring up production stack then poll forever.
 */
static void internal_usb_task_entry(ULONG input)
{
  (void)input;

  /* Bring up the USB0 peripheral via the production rx_usb stack.
   * rx_usb_init() runs `internal_usb_enable_module_clock` /
   * `internal_usb_configure_clock` which both call `tx_thread_sleep`, so
   * this cannot run pre-kernel.  Running here -- at priority 4, before
   * comm_task (5) gets CPU -- guarantees DPRPU is asserted before any
   * other transport init touches SPI / I2C / UART. */
  const rx_err_t err = rx_usb_init(nullptr);
  if (err != k_rx_ok && err != k_rx_err_invalid_state) {
    rx_log_error_val(s_tag, "rx_usb_init failed", (uint32_t)err);
    /* Fall through: the inline USB attach in main() already got us to a
     * state where rx_usb_isr_handler can service SETUP on pipe 0, so
     * enumeration still works even without the full production init. */
  }

  rx_log_info(s_tag, "USB polling loop entering");

  /* The USBI vector does not fire on this chip.  Poll the production ISR
   * dispatcher every ThreadX tick (10 ms) -- fast enough that Linux's
   * SETUP retries (~50 ms) never time out, cheap enough that
   * lower-priority tasks keep running. */
  for (;;) {
    rx_usb_isr_handler();
    (void)tx_thread_sleep(1U);
  }
}

rx_err_t usb_task_create(void)
{
  if (s_usb_created) {
    return k_rx_err_invalid_state;
  }

  const UINT tx_status = tx_thread_create(&s_usb_thread,
                                          "USBTask",
                                          internal_usb_task_entry,
                                          k_usb_task_input,
                                          s_usb_stack,
                                          k_usb_task_stack_size,
                                          k_usb_task_priority,
                                          k_usb_task_priority,
                                          TX_NO_TIME_SLICE,
                                          TX_AUTO_START);
  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "tx_thread_create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_usb_created = true;
  rx_log_info(s_tag, "USB task created");
  return k_rx_ok;
}
