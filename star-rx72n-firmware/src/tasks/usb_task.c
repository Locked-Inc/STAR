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
  k_usb_task_stack_size    = 1024, /**< ThreadX task stack in bytes. */
  k_usb_task_priority      = 4,    /**< One higher than comm_task (5) so we run first. */
  k_usb_task_input         = 0,    /**< Thread entry input (unused). */
  k_heartbeat_tick_period  = 100U, /**< 100 ticks @ 100 Hz tick = 1 s heartbeat cadence. */
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

  /* Drive PB3 LOW AT THE VERY FIRST INSTRUCTION of the task, BEFORE
   * anything that could block (rx_log_info, mutex acquire, etc.).
   * If AD2 DIO7 stays HIGH, the task entry was never reached.  If it
   * goes LOW, the task DID start but gets stuck later. */
  {
    volatile uint8_t* const pb_pdr  = (volatile uint8_t*)0x0008C00BU;
    volatile uint8_t* const pb_podr = (volatile uint8_t*)0x0008C02BU;
    volatile uint8_t* const pb_pmr  = (volatile uint8_t*)0x0008C06BU;
    *pb_pmr  &= (uint8_t)~(1U << 3);
    *pb_pdr  |= (uint8_t)(1U << 3);
    *pb_podr &= (uint8_t)~(1U << 3); /* LOW -- task was entered */
  }

  rx_log_info(s_tag, "USB polling loop entering");

  /* Diagnostic: drive BOTH pins of P4 (silkscreen "EN3D") as GPIO so
   * an AD2 logic probe on either P4 pad sees a heartbeat.
   *
   *   P4 pad 1 = net /ENC3_TCLKC = MCU pin 75 = PC0
   *   P4 pad 2 = net /ENC3_TCLKD = MCU pin 82 = PB3
   *
   * Force PMR=0 (GPIO mode) and PDR=1 (output) every loop iteration
   * in case another task (e.g. motor_control_task) reclaims these
   * pins for their encoder function -- we stomp it back to GPIO.
   * Both pins toggle every tick (~50 Hz square wave visible on AD2)
   * with a 4-edge burst when the heartbeat block enters rx_usb_write. */
  volatile uint8_t* const portb_pdr  = (volatile uint8_t*)0x0008C00BU;
  volatile uint8_t* const portb_podr = (volatile uint8_t*)0x0008C02BU;
  volatile uint8_t* const portb_pmr  = (volatile uint8_t*)0x0008C06BU;
  volatile uint8_t* const portc_pdr  = (volatile uint8_t*)0x0008C00CU;
  volatile uint8_t* const portc_podr = (volatile uint8_t*)0x0008C02CU;
  volatile uint8_t* const portc_pmr  = (volatile uint8_t*)0x0008C06CU;

  /* main.c drove PB3 HIGH before tx_kernel_enter as a "reached init"
   * beacon.  Drive PB3 LOW here to prove usb_task is actually
   * running: AD2 DIO7 on P4 pad 2 should show LOW if so.  PC0 held
   * HIGH as a secondary signal for the other P4 pad. */
  *portb_pmr &= (uint8_t)~(1U << 3);
  *portb_pdr |= (uint8_t)(1U << 3);
  *portb_podr &= (uint8_t)~(1U << 3); /* LOW */
  *portc_pmr &= (uint8_t)~(1U << 0);
  *portc_pdr |= (uint8_t)(1U << 0);
  *portc_podr |= (uint8_t)(1U << 0); /* HIGH */

  uint32_t tick = 0U;
  for (;;) {
    rx_usb_isr_handler();

    /* Re-assert PB3 as output.  Default state = HIGH every tick.
     * Then try rx_usb_write; on success flip PB3 LOW so AD2 sees:
     *   steady HIGH -> rx_usb_write keeps failing every tick
     *   toggling    -> rx_usb_write succeeds intermittently
     *   steady LOW  -> rx_usb_write succeeds every tick */
    *portb_pmr &= (uint8_t)~(1U << 3);
    *portb_pdr |= (uint8_t)(1U << 3);
    *portb_podr |= (uint8_t)(1U << 3);  /* default HIGH = write failed */
    *portc_pmr &= (uint8_t)~(1U << 0);
    *portc_pdr |= (uint8_t)(1U << 0);
    *portc_podr |= (uint8_t)(1U << 0);

    static const char beat[] = "X";
    if (rx_usb_write(k_usb_port_proto, (const uint8_t*)beat, 1U) == k_rx_ok) {
      *portb_podr &= (uint8_t)~(1U << 3); /* LOW = write succeeded */
    }
    (void)rx_usb_write(k_usb_port_decoded, (const uint8_t*)beat, 1U);
    (void)rx_usb_write(k_usb_port_log,     (const uint8_t*)beat, 1U);

    /* RAW register write test: bypass all abstractions and write
     * directly to pipe 1 (port 0 bulk IN, EP 0x81).  This proves
     * whether the hardware itself can transmit -- if the host starts
     * seeing 'Z' bytes on /dev/ttyACM1, fifo_write in rx_usb_hw.c is
     * broken; if not, the issue is even lower. */
    {
      volatile uint16_t* const cfifosel = (volatile uint16_t*)0x000A0020U;
      volatile uint16_t* const cfifoctr = (volatile uint16_t*)0x000A0022U;
      volatile uint8_t*  const cfifob   = (volatile uint8_t*) 0x000A0014U;
      volatile uint16_t* const pipe1ctr = (volatile uint16_t*)0x000A0070U;

      /* PID=NAK (clear PID bit 0) */
      *pipe1ctr &= (uint16_t)~0x0001U;
      /* Spin briefly for PBUSY */
      for (int w = 0; w < 100; ++w) {
        if ((*pipe1ctr & 0x20U) == 0U) break;
      }
      /* Clear BEMPSTS[1] + BRDYSTS[1] */
      *((volatile uint16_t*)0x000A004AU) = (uint16_t)(~(1U << 1) & 0x03FFU);
      *((volatile uint16_t*)0x000A0046U) = (uint16_t)(~(1U << 1) & 0x03FFU);
      /* CFIFOSEL: clear CURPIPE, ISEL; wait; set CURPIPE=1, ISEL=1, MBW=0 */
      uint16_t sel = *cfifosel;
      sel &= (uint16_t)~0x003FU; /* clear CURPIPE[3:0] and ISEL bit5 */
      *cfifosel = sel;
      for (int w = 0; w < 100; ++w) {
        if ((*cfifosel & 0x000FU) == 0U) break;
      }
      sel |= (uint16_t)0x0021U;  /* CURPIPE=1 | ISEL=1 */
      sel &= (uint16_t)~0x0400U; /* MBW=0 (8-bit) */
      *cfifosel = sel;
      /* Wait FRDY */
      for (int w = 0; w < 1000; ++w) {
        if ((*cfifoctr & 0x2000U) != 0U) break;
      }
      /* Write 'Z' and set BVAL */
      *cfifob = (uint8_t)'Z';
      *cfifoctr |= 0x8000U; /* BVAL */
      /* PID=BUF */
      *pipe1ctr = (uint16_t)((*pipe1ctr & (uint16_t)~0x0003U) | 0x0001U);
    }
    tick++;
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
