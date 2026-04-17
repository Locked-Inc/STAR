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
 * @brief USB task entry: poll the production ISR dispatcher forever.
 *
 * @details
 * The USB0 peripheral is attached pre-kernel by main()'s inline attach
 * block (SYSCFG / DCPCFG / DCPCTR / INTENB0 / DPRPU), which also services
 * the initial enumeration via an inline SETUP handler so Linux completes
 * GET_DESCRIPTOR + SET_ADDRESS inside its retry window.  Once
 * tx_kernel_enter() runs, main's loop exits and this task takes over:
 * it calls the production rx_usb_isr_handler every 10 ms so any later
 * SETUPs (e.g. string descriptor requests) get routed through the full
 * rx_usb_cdc stack rather than the inline stub handler.
 *
 * Priority 4 is the highest app-task priority so comm_task (5) and
 * everything below can't starve USB servicing.
 */
static void internal_usb_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "USB polling loop entering");

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
