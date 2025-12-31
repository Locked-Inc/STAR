/**
 * @file rx_usb_isr.c
 * @brief USB0 Interrupt Service Routine for RX72N
 *
 * This file handles all USB0 interrupts:
 * - VBUS detection (cable connect/disconnect)
 * - Device state transitions (reset, suspend, resume)
 * - Control transfer stage transitions (SETUP, DATA, STATUS)
 * - Buffer ready/empty for data pipes
 */

#include "rx72n_regs.h"
#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

static const char* s_tag = "USB_ISR";

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
  uint16_t syssts = USB0.SYSSTS0;

  /* Check line state to determine if cable is connected */
  uint16_t lnst = syssts & k_usb_syssts0_lnst_mask;

  if (lnst == k_usb_syssts0_lnst_se0) {
    /* SE0 = disconnected or reset in progress */
    RX_LOG_DEBUG(s_tag, "VBUS: SE0 detected");
  } else if (lnst == k_usb_syssts0_lnst_fs_j) {
    /* Full-Speed J-state = idle, cable connected */
    RX_LOG_DEBUG(s_tag, "VBUS: FS J-state (connected)");
    rx_usb_set_state(k_usb_state_attached);
  }

  /* Clear VBUS interrupt flag */
  USB0.INTSTS0 = (uint16_t)~k_usb_intsts0_vbint;
}

/**
 * @brief Handle device state transition interrupt
 */
static void internal_handle_dvst_interrupt(void)
{
  uint16_t intsts0 = USB0.INTSTS0;
  uint16_t dvsq    = intsts0 & k_usb_intsts0_dvsq_mask;

  switch (dvsq) {
    case k_usb_intsts0_dvsq_powered:
      RX_LOG_DEBUG(s_tag, "DVST: Powered state");
      rx_usb_set_state(k_usb_state_powered);
      break;

    case k_usb_intsts0_dvsq_default:
      RX_LOG_DEBUG(s_tag, "DVST: Default state (bus reset complete)");
      rx_usb_set_state(k_usb_state_default);
      rx_usb_count_bus_reset();
      break;

    case k_usb_intsts0_dvsq_address:
      RX_LOG_DEBUG(s_tag, "DVST: Address state");
      rx_usb_set_state(k_usb_state_addressed);
      break;

    case k_usb_intsts0_dvsq_configured:
      RX_LOG_DEBUG(s_tag, "DVST: Configured state");
      rx_usb_set_state(k_usb_state_configured);
      break;

    case k_usb_intsts0_dvsq_suspend:
      RX_LOG_DEBUG(s_tag, "DVST: Suspended state");
      rx_usb_set_state(k_usb_state_suspended);
      rx_usb_count_suspend();
      break;

    default:
      RX_LOG_WARN(s_tag, "DVST: Unknown state");
      break;
  }

  /* Clear DVST interrupt flag */
  USB0.INTSTS0 = (uint16_t)~k_usb_intsts0_dvst;
}

/**
 * @brief Handle control transfer stage transition interrupt
 */
static void internal_handle_ctrt_interrupt(void)
{
  uint16_t intsts0 = USB0.INTSTS0;
  uint16_t ctsq    = intsts0 & k_usb_intsts0_ctsq_mask;

  switch (ctsq) {
    case k_usb_intsts0_ctsq_idle:
      /* Idle or setup stage - check for valid SETUP packet */
      if (intsts0 & k_usb_intsts0_valid) {
        rx_usb_cdc_handle_setup();
        /* Clear VALID flag */
        USB0.INTSTS0 = (uint16_t)~k_usb_intsts0_valid;
      }
      break;

    case k_usb_intsts0_ctsq_rd_data:
      /* Control read data stage - send data to host */
      rx_usb_cdc_handle_bulk_in();
      break;

    case k_usb_intsts0_ctsq_rd_status:
      /* Control read status stage - host sends ZLP ACK */
      /* Complete the control transfer */
      USB0.DCPCTR |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_wr_data:
      /* Control write data stage - receive data from host */
      rx_usb_cdc_handle_bulk_out();
      break;

    case k_usb_intsts0_ctsq_wr_status:
      /* Control write status stage - send ZLP ACK */
      /* Complete the control transfer */
      USB0.DCPCTR |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_wr_nd:
      /* Control write with no data - status stage */
      USB0.DCPCTR |= k_usb_dcpctr_ccpl;
      break;

    case k_usb_intsts0_ctsq_seq_err:
      /* Sequence error - stall the pipe */
      RX_LOG_WARN(s_tag, "CTRT: Sequence error");
      USB0.DCPCTR = (USB0.DCPCTR & ~k_usb_dcpctr_pid_mask) | k_usb_dcpctr_pid_stall;
      break;

    default:
      break;
  }

  /* Clear CTRT interrupt flag */
  USB0.INTSTS0 = (uint16_t)~k_usb_intsts0_ctrt;
}

/**
 * @brief Handle buffer ready interrupt (data received)
 */
static void internal_handle_brdy_interrupt(void)
{
  uint16_t brdysts = USB0.BRDYSTS;

  /* Check each pipe for buffer ready */
  for (uint8_t pipe = 0; pipe <= 9; pipe++) {
    if (brdysts & (1U << pipe)) {
      if (pipe == 0) {
        /* DCP buffer ready - control transfer data */
        /* Handled in CTRT interrupt */
      } else if (pipe == k_usb_cdc_ep_bulk_out) {
        /* Bulk OUT data received from host */
        rx_usb_cdc_handle_bulk_out();
      }

      /* Clear pipe buffer ready flag */
      USB0.BRDYSTS = (uint16_t)~(1U << pipe);
    }
  }
}

/**
 * @brief Handle buffer empty interrupt (transmission complete)
 */
static void internal_handle_bemp_interrupt(void)
{
  uint16_t bempsts = USB0.BEMPSTS;

  /* Check each pipe for buffer empty */
  for (uint8_t pipe = 0; pipe <= 9; pipe++) {
    if (bempsts & (1U << pipe)) {
      if (pipe == 0) {
        /* DCP buffer empty - control transfer complete */
        /* Handled in CTRT interrupt */
      } else if (pipe == k_usb_cdc_ep_bulk_in) {
        /* Bulk IN transmission complete - can send more data */
        rx_usb_cdc_handle_bulk_in();
      }

      /* Clear pipe buffer empty flag */
      USB0.BEMPSTS = (uint16_t)~(1U << pipe);
    }
  }
}

/**
 * @brief Handle resume interrupt
 */
static void internal_handle_resume_interrupt(void)
{
  RX_LOG_DEBUG(s_tag, "Resume detected");

  /* Update state based on hardware */
  rx_usb_state_t state = rx_usb_hw_get_bus_state();
  rx_usb_set_state(state);

  /* Clear resume interrupt flag */
  USB0.INTSTS0 = (uint16_t)~k_usb_intsts0_resm;
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
  uint16_t intsts0 = USB0.INTSTS0;
  uint16_t intenb0 = USB0.INTENB0;

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
  ICU.IR[VECT_USB0_USBI] = 0;

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
  ICU.IR[VECT_USB0_D0FIFO] = 0;

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
  ICU.IR[VECT_USB0_D1FIFO] = 0;

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
  ICU.IR[VECT_USB0_USBR] = 0;

  /* Handle resume */
  internal_handle_resume_interrupt();
}
