/**
 * @file usb_task.c
 * @brief Dedicated USB polling task -- 3-port CDC throughput + echo harness
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
  k_usb_task_stack_size = 2048, /**< ThreadX task stack in bytes. */
  k_usb_task_priority   = 4,    /**< One higher than comm_task (5) so we run first. */
  k_usb_task_input      = 0,    /**< Thread entry input (unused). */
  k_tx_packet_bytes     = 512U, /**< Per-port TX burst -- 8 USB FS bulk MPS packets. */
  k_rx_drain_bytes      = 128U, /**< Per-tick H->D drain buffer. */
} usb_task_constants_t;

static TX_THREAD s_usb_thread;
static uint8_t   s_usb_stack[k_usb_task_stack_size];
static bool      s_usb_created = false;

static const char* s_tag = "USB_TASK";

/* 512 B canned payload per port (8 back-to-back 64 B lines).  Each
 * line is self-identifying ("p0:ABCD..." / "p1:..." / "p2:...") so a
 * host `cat /dev/ttyACMn` shows which port it is tapping. */
#define STAR_USB_STRESS_LINE_PROTO   "p0:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVW\n"
#define STAR_USB_STRESS_LINE_DECODED "p1:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVW\n"
#define STAR_USB_STRESS_LINE_LOG     "p2:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVW\n"
__attribute__((unused)) static const uint8_t s_tx_proto[k_tx_packet_bytes] = STAR_USB_STRESS_LINE_PROTO
    STAR_USB_STRESS_LINE_PROTO STAR_USB_STRESS_LINE_PROTO STAR_USB_STRESS_LINE_PROTO
    STAR_USB_STRESS_LINE_PROTO STAR_USB_STRESS_LINE_PROTO STAR_USB_STRESS_LINE_PROTO
    STAR_USB_STRESS_LINE_PROTO;
__attribute__((unused)) static const uint8_t s_tx_decoded[k_tx_packet_bytes] = STAR_USB_STRESS_LINE_DECODED
    STAR_USB_STRESS_LINE_DECODED STAR_USB_STRESS_LINE_DECODED STAR_USB_STRESS_LINE_DECODED
    STAR_USB_STRESS_LINE_DECODED STAR_USB_STRESS_LINE_DECODED STAR_USB_STRESS_LINE_DECODED
    STAR_USB_STRESS_LINE_DECODED;
__attribute__((unused)) static const uint8_t s_tx_log[k_tx_packet_bytes] = STAR_USB_STRESS_LINE_LOG
    STAR_USB_STRESS_LINE_LOG STAR_USB_STRESS_LINE_LOG STAR_USB_STRESS_LINE_LOG
    STAR_USB_STRESS_LINE_LOG STAR_USB_STRESS_LINE_LOG STAR_USB_STRESS_LINE_LOG
    STAR_USB_STRESS_LINE_LOG;

/* Echo scratch, static so we don't bloat the task stack. */
static uint8_t s_echo_buf[k_rx_drain_bytes];

/**
 * @brief Drain up to k_rx_drain_bytes from `port` and echo them back.
 *
 * Skips cleanly if nothing is queued so stress-blast pace isn't
 * disrupted.  Any error from rx_usb_rx_available / rx_usb_read causes
 * an early return without writing.
 */
static void internal_echo_port(rx_usb_port_id_t port)
{
  uint32_t avail = 0U;
  if (rx_usb_rx_available(port, &avail) != k_rx_ok || avail == 0U) {
    return;
  }

  const uint32_t max = (avail < k_rx_drain_bytes) ? avail : k_rx_drain_bytes;
  uint32_t       got = 0U;
  if (rx_usb_read(port, s_echo_buf, max, &got) != k_rx_ok || got == 0U) {
    return;
  }

  (void)rx_usb_write(port, s_echo_buf, got);
}

/**
 * @brief USB task entry.
 *
 * Every ThreadX tick (~10 ms at 100 Hz tick):
 *   1. rx_usb_isr_handler() backstop call so any USBI the ICU missed
 *      still gets serviced from task context.
 *   2. 64 B D->H blast on all three ports (rx_usb_write returns
 *      immediately with k_rx_err_busy if the ring is full).
 *   3. internal_echo_port() on all three ports -- drains any pending
 *      bulk-OUT data and writes it straight back on the same port so
 *      host->device->host round-trip testing works end-to-end.  Even
 *      LOG (port 2, nominally RO in the user's narrative) has bulk OUT
 *      per descriptors, and draining it prevents the kernel's write()
 *      from blocking indefinitely if anyone ever writes to
 *      /dev/ttyACMn.
 */
static void internal_usb_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "USB stress task entering");

  /* STAR_STRESS_PORTS bitfield: bit 0 = PROTO, 1 = DECODED, 2 = LOG.
   * Default is 0b111 (all three active).  Override at build time with
   * e.g. -DSTAR_STRESS_PORTS=1 for single-pipe throughput ceiling
   * measurement (no bus-contention from the other two pipes). */
#ifndef STAR_STRESS_PORTS
#define STAR_STRESS_PORTS 0x07U
#endif

  for (;;) {
    rx_usb_isr_handler();

#if (STAR_STRESS_PORTS) & 0x01U
    (void)rx_usb_write(k_usb_port_proto, s_tx_proto, k_tx_packet_bytes);
    internal_echo_port(k_usb_port_proto);
#endif
#if (STAR_STRESS_PORTS) & 0x02U
    (void)rx_usb_write(k_usb_port_decoded, s_tx_decoded, k_tx_packet_bytes);
    internal_echo_port(k_usb_port_decoded);
#endif
#if (STAR_STRESS_PORTS) & 0x04U
    (void)rx_usb_write(k_usb_port_log, s_tx_log, k_tx_packet_bytes);
    internal_echo_port(k_usb_port_log);
#endif

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
