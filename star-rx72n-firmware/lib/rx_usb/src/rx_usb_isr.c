/* lib/rx_usb/src/rx_usb_isr.c */

/**
 * @file rx_usb_isr.c
 * @brief USB0 Interrupt Service Routine for RX72N
 * @details
 * This file handles all USB0 interrupts:
 * - VBUS detection (cable connect/disconnect)
 * - Device state transitions (reset, suspend, resume)
 * - Control transfer stage transitions (SETUP, DATA, STATUS)
 * - Buffer ready/empty for data pipes
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx72n_regs.h"
#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

static const char* s_tag = "USB_ISR";

/** @brief USB pipe number constants */
typedef enum : uint8_t {
  k_usb_pipe_dcp = 0, /**< Default Control Pipe (DCP) number */
  k_usb_pipe_max = 9, /**< Maximum pipe number (pipes 0-9) */
} usb_pipe_numbers_t;

/* =============================================================================
 * Forward Declarations (internal functions from other modules)
 * =============================================================================
 */

/* From rx_usb.c */
extern void rx_usb_set_state(rx_usb_state_t state);
extern void rx_usb_count_bus_reset(void);
extern void rx_usb_count_suspend(void);

/* From rx_usb_cdc.c */
extern void rx_usb_cdc_handle_setup(void);
extern void rx_usb_cdc_handle_bulk_in(void);
extern void rx_usb_cdc_handle_bulk_out(void);

/* From rx_usb_hw.c */
extern rx_usb_state_t rx_usb_hw_get_bus_state(void);

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Handle VBUS interrupt (cable connect/disconnect)
 */
static void internal_handle_vbus_interrupt(void)
{
  const uint16_t syssts = usb0()->syssts0;

  /* Check line state to determine if cable is connected */
  uint16_t lnst = syssts & k_usb_syssts0_lnst_mask;

  if (lnst == k_usb_syssts0_lnst_se0) {
    /* SE0 = disconnected or reset in progress */
    rx_log_debug(s_tag, "VBUS: SE0 detected");
  } else if (lnst == k_usb_syssts0_lnst_fs_j) {
    /* Full-Speed J-state = idle, cable connected */
    rx_log_debug(s_tag, "VBUS: FS J-state (connected)");
    rx_usb_set_state(k_usb_state_attached);
  }

  /* Clear VBUS interrupt flag */
  usb0()->intsts0 = (uint16_t)~k_usb_intsts0_vbint;
}

/**
 * @brief Handle device state transition interrupt
 */
static void internal_handle_dvst_interrupt(void)
{
  const uint16_t intsts0 = usb0()->intsts0;
  const uint16_t dvsq    = intsts0 & k_usb_intsts0_dvsq_mask;

  switch (dvsq) {
    case k_usb_intsts0_dvsq_powered:
      rx_log_debug(s_tag, "DVST: Powered state");
      rx_usb_set_state(k_usb_state_powered);
      break;

    case k_usb_intsts0_dvsq_default:
      rx_log_debug(s_tag, "DVST: Default state (bus reset complete)");
      rx_usb_set_state(k_usb_state_default);
      rx_usb_count_bus_reset();
      break;

    case k_usb_intsts0_dvsq_address:
      rx_log_debug(s_tag, "DVST: Address state");
      rx_usb_set_state(k_usb_state_addressed);
      break;

    case k_usb_intsts0_dvsq_configured:
      rx_log_debug(s_tag, "DVST: Configured state");
      rx_usb_set_state(k_usb_state_configured);
      break;

    case k_usb_intsts0_dvsq_suspend:
      rx_log_debug(s_tag, "DVST: Suspended state");
      rx_usb_set_state(k_usb_state_suspended);
      rx_usb_count_suspend();
      break;

    default:
      rx_log_warn(s_tag, "DVST: Unknown state");
      break;
  }

  /* Clear DVST interrupt flag */
  usb0()->intsts0 = (uint16_t)~k_usb_intsts0_dvst;
}

/**
 * @brief Handle control transfer stage transition interrupt
 */
static void internal_handle_ctrt_interrupt(void)
{
  const uint16_t intsts0 = usb0()->intsts0;
  const uint16_t ctsq    = intsts0 & k_usb_intsts0_ctsq_mask;

  switch (ctsq) {
    case k_usb_intsts0_ctsq_idle:
      /* Idle or setup stage - check for valid SETUP packet */
      if (intsts0 & k_usb_intsts0_valid) {
        rx_usb_cdc_handle_setup();
        /* Clear VALID flag */
        usb0()->intsts0 = (uint16_t)~k_usb_intsts0_valid;
      }
      break;

    case k_usb_intsts0_ctsq_rd_data:
      /* Control read data stage - send data to host */
      rx_usb_cdc_handle_bulk_in();
      break;

    case k_usb_intsts0_ctsq_rd_status:
      /* Control read status stage - host sends ZLP ACK */
      /* Complete the control transfer */
      usb0()->dcpctr |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_wr_data:
      /* Control write data stage - receive data from host */
      rx_usb_cdc_handle_bulk_out();
      break;

    case k_usb_intsts0_ctsq_wr_status:
      /* Control write status stage - send ZLP ACK */
      /* Complete the control transfer */
      usb0()->dcpctr |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_wr_nd:
      /* Control write with no data - status stage */
      usb0()->dcpctr |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_seq_err:
      /* Sequence error - stall the pipe */
      rx_log_warn(s_tag, "CTRT: Sequence error");
      usb0()->dcpctr = (usb0()->dcpctr & ~k_usb_dcpctr_pid_mask) | k_usb_dcpctr_pid_stall;
      break;

    default:
      break;
  }

  /* Clear CTRT interrupt flag */
  usb0()->intsts0 = (uint16_t)~k_usb_intsts0_ctrt;
}

/**
 * @brief Handle buffer ready interrupt (data received)
 */
static void internal_handle_brdy_interrupt(void)
{
  uint16_t brdysts = usb0()->brdysts;

  /* Check each pipe for buffer ready */
  for (uint8_t pipe = k_usb_pipe_dcp; pipe <= k_usb_pipe_max; pipe++) {
    if (brdysts & (1U << pipe)) {
      if (pipe == k_usb_pipe_dcp) {
        /* DCP buffer ready - control transfer data */
        /* Handled in CTRT interrupt */
      } else if (pipe == k_usb_cdc_ep_bulk_out) {
        /* Bulk OUT data received from host */
        rx_usb_cdc_handle_bulk_out();
      }

      /* Clear pipe buffer ready flag */
      usb0()->brdysts = (uint16_t)~(1U << pipe);
    }
  }
}

/**
 * @brief Handle buffer empty interrupt (transmission complete)
 */
static void internal_handle_bemp_interrupt(void)
{
  uint16_t bempsts = usb0()->bempsts;

  /* Check each pipe for buffer empty */
  for (uint8_t pipe = k_usb_pipe_dcp; pipe <= k_usb_pipe_max; pipe++) {
    if (bempsts & (1U << pipe)) {
      if (pipe == k_usb_pipe_dcp) {
        /* DCP buffer empty - control transfer complete */
        /* Handled in CTRT interrupt */
      } else if (pipe == k_usb_cdc_ep_bulk_in) {
        /* Bulk IN transmission complete - can send more data */
        rx_usb_cdc_handle_bulk_in();
      }

      /* Clear pipe buffer empty flag */
      usb0()->bempsts = (uint16_t)~(1U << pipe);
    }
  }
}

/**
 * @brief Handle resume interrupt
 */
static void internal_handle_resume_interrupt(void)
{
  rx_log_debug(s_tag, "Resume detected");

  /* Update state based on hardware */
  rx_usb_state_t state = rx_usb_hw_get_bus_state();
  rx_usb_set_state(state);

  /* Clear resume interrupt flag */
  usb0()->intsts0 = (uint16_t)~k_usb_intsts0_resm;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief USB0 Interrupt Service Routine
 *
 * This function is called from the vector table when a USB interrupt occurs.
 * It reads the interrupt status register and dispatches to the appropriate
 * handler.
 */
void rx_usb_isr_handler(void)
{
  const uint16_t intsts0 = usb0()->intsts0;
  const uint16_t intenb0 = usb0()->intenb0;

  /* Only process enabled interrupts */
  uint16_t active = intsts0 & intenb0;

  /* VBUS interrupt (cable connect/disconnect) - highest priority */
  if (active & k_usb_intsts0_vbint) {
    internal_handle_vbus_interrupt();
  }

  /* Device state transition interrupt */
  if (active & k_usb_intsts0_dvst) {
    internal_handle_dvst_interrupt();
  }

  /* Resume interrupt */
  if (active & k_usb_intsts0_resm) {
    internal_handle_resume_interrupt();
  }

  /* Control transfer stage transition */
  if (active & k_usb_intsts0_ctrt) {
    internal_handle_ctrt_interrupt();
  }

  /* Buffer ready interrupt (data received) */
  if (active & k_usb_intsts0_brdy) {
    internal_handle_brdy_interrupt();
  }

  /* Buffer empty interrupt (transmission complete) */
  if (active & k_usb_intsts0_bemp) {
    internal_handle_bemp_interrupt();
  }
}

/**
 * @brief USB0 USBI Interrupt Handler
 *
 * This is the actual interrupt handler that is registered in the vector table.
 * It calls the main ISR handler.
 */
void usb0_usbi_isr(void)
{
  /* Clear interrupt request flag in ICU */
  icu()->ir[k_vect_usb0_usbi] = 0;

  /* Call main USB handler */
  rx_usb_isr_handler();
}

/**
 * @brief USB0 D0FIFO Interrupt Handler
 *
 * DMA-related interrupt for D0FIFO (not used in this implementation).
 */
void usb0_d0fifo_isr(void)
{
  /* Clear interrupt request flag */
  icu()->ir[k_vect_usb0_d0fifo] = 0;

  /* D0FIFO DMA not implemented - clear and ignore */
}

/**
 * @brief USB0 D1FIFO Interrupt Handler
 *
 * DMA-related interrupt for D1FIFO (not used in this implementation).
 */
void usb0_d1fifo_isr(void)
{
  /* Clear interrupt request flag */
  icu()->ir[k_vect_usb0_d1fifo] = 0;

  /* D1FIFO DMA not implemented - clear and ignore */
}

/**
 * @brief USB0 Resume Interrupt Handler
 *
 * Separate resume interrupt for wakeup from low-power modes.
 */
void usb0_usbr_isr(void)
{
  /* Clear interrupt request flag */
  icu()->ir[k_vect_usb0_usbr] = 0;

  /* Handle resume */
  internal_handle_resume_interrupt();
}
