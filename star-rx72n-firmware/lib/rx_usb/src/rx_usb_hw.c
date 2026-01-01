/**
 * @file rx_usb_hw.c
 * @brief USB0 Hardware Layer for RX72N
 *
 * This file provides low-level hardware access to the USB0 peripheral.
 * It handles module initialization, clock configuration, and register access.
 */

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

static const char* s_tag = "USB_HW";

/* USB clock stabilization wait time (in loop iterations) */
#define USB_PLL_WAIT_COUNT 1000

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static bool s_hw_initialized = false;

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Initialize USB0 hardware
 *
 * This function:
 * 1. Enables the USB0 module clock (clears module stop bit)
 * 2. Configures USB0 for function (peripheral) mode
 * 3. Sets up the USB clock (48 MHz from PLL)
 * 4. Configures interrupts
 */
rx_err_t rx_usb_hw_init(void)
{
  if (s_hw_initialized) {
    return k_rx_ok;
  }

  rx_log_debug(s_tag, "Initializing USB0 hardware");

  /* 1. Enable USB0 module clock */
  /* Unlock protection */
  SYSTEM.PRCR = 0xA50B;

  /* Clear module stop bit for USB0 (bit 19 in MSTPCRB) */
  SYSTEM.MSTPCRB &= ~(1UL << k_mstpb_usb0);

  /* Lock protection */
  SYSTEM.PRCR = 0xA500;

  /* 2. Disable USB module before configuration */
  USB0.SYSCFG = 0x0000;

  /* 3. Wait for USB clock to stabilize (if using PLL) */
  /* Note: USB requires 48 MHz clock from main PLL */
  volatile uint32_t wait = USB_PLL_WAIT_COUNT;
  while (wait--) {
    __asm__ volatile("nop");
  }

  /* 4. Configure USB0 for Function (peripheral) mode */
  /* DCFM = 0: Function mode (not host) */
  /* DRPD = 0: Disable D+/D- pull-down (function mode) */
  /* DPRPU = 0: D+ pull-up disabled initially (enabled on attach) */
  /* USBE = 0: USB module disabled initially */
  USB0.SYSCFG = 0x0000;

  /* 5. Configure USB clock */
  /* SCKE = 1: Enable USB clock */
  USB0.SYSCFG |= k_usb_syscfg_scke;

  /* Wait for clock to stabilize */
  wait = USB_PLL_WAIT_COUNT;
  while (wait--) {
    __asm__ volatile("nop");
  }

  /* 6. Enable USB module */
  USB0.SYSCFG |= k_usb_syscfg_usbe;

  /* 7. Configure interrupts */
  /* Enable: VBUS, device state, control transfer, buffer ready/empty */
  USB0.INTENB0 = k_usb_intenb0_vbse | k_usb_intenb0_dvse | k_usb_intenb0_ctre |
                 k_usb_intenb0_brdye | k_usb_intenb0_bempe;

  /* 8. Configure Interrupt Controller (ICU) */
  /* Clear pending interrupt */
  ICU.IR[k_vect_usb0_usbi] = 0;

  /* Set interrupt priority (6 = moderate, below motor control) */
  ICU.IPR[k_vect_usb0_usbi] = 6;

  /* Enable interrupt in IER */
  ICU.IER[k_vect_usb0_usbi / 8] |= (1 << (k_vect_usb0_usbi % 8));

  /* 9. Set default control pipe max packet size (64 bytes for FS) */
  USB0.DCPMAXP = k_usb_cdc_max_packet_fs;

  s_hw_initialized = true;

  rx_log_info(s_tag, "USB0 hardware initialized");

  return k_rx_ok;
}

/**
 * @brief Deinitialize USB0 hardware
 */
rx_err_t rx_usb_hw_deinit(void)
{
  if (!s_hw_initialized) {
    return k_rx_ok;
  }

  rx_log_debug(s_tag, "Deinitializing USB0 hardware");

  /* Disable USB module */
  USB0.SYSCFG = 0x0000;

  /* Disable interrupt in ICU */
  ICU.IER[k_vect_usb0_usbi / 8] &= ~(1 << (k_vect_usb0_usbi % 8));
  ICU.IR[k_vect_usb0_usbi] = 0;

  /* Disable USB0 module clock */
  SYSTEM.PRCR = 0xA50B;
  SYSTEM.MSTPCRB |= (1UL << k_mstpb_usb0);
  SYSTEM.PRCR = 0xA500;

  s_hw_initialized = false;

  return k_rx_ok;
}

/**
 * @brief Attach to USB bus (enable D+ pull-up)
 *
 * This signals to the host that a device is connected.
 */
rx_err_t rx_usb_hw_attach(void)
{
  if (!s_hw_initialized) {
    return k_rx_err_invalid_state;
  }

  rx_log_debug(s_tag, "Attaching to USB bus");

  /* Enable D+ pull-up resistor to signal device presence */
  USB0.SYSCFG |= k_usb_syscfg_dprpu;

  return k_rx_ok;
}

/**
 * @brief Detach from USB bus (disable D+ pull-up)
 */
rx_err_t rx_usb_hw_detach(void)
{
  if (!s_hw_initialized) {
    return k_rx_ok;
  }

  rx_log_debug(s_tag, "Detaching from USB bus");

  /* Disable D+ pull-up resistor */
  USB0.SYSCFG &= ~k_usb_syscfg_dprpu;

  return k_rx_ok;
}

/**
 * @brief Read data from USB FIFO
 *
 * @param pipe Pipe number (0 = DCP, 1-9 = data pipes)
 * @param data Output buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes read
 */
uint32_t rx_usb_hw_fifo_read(uint8_t pipe, uint8_t* data, uint32_t max_len)
{
  if (data == NULL || max_len == 0) {
    return 0;
  }

  /* Select pipe for CFIFO access */
  USB0.CFIFOSEL = (pipe & k_usb_fifosel_curpipe_mask);

  /* Wait for FIFO ready */
  volatile uint32_t timeout = 1000;
  while (!(USB0.CFIFOCTR & k_usb_fifoctr_frdy) && timeout--) {
    __asm__ volatile("nop");
  }

  if (timeout == 0) {
    return 0;
  }

  /* Get received data length */
  uint32_t len = USB0.CFIFOCTR & k_usb_fifoctr_dtln_mask;
  if (len > max_len) {
    len = max_len;
  }

  /* Read data from FIFO */
  for (uint32_t i = 0; i < len; i++) {
    data[i] = (uint8_t)(USB0.CFIFO & 0xFF);
  }

  /* Clear buffer */
  USB0.CFIFOCTR |= k_usb_fifoctr_bclr;

  return len;
}

/**
 * @brief Write data to USB FIFO
 *
 * @param pipe Pipe number (0 = DCP, 1-9 = data pipes)
 * @param data Input buffer
 * @param len Number of bytes to write
 * @return Number of bytes written
 */
uint32_t rx_usb_hw_fifo_write(uint8_t pipe, const uint8_t* data, uint32_t len)
{
  if (data == NULL || len == 0) {
    return 0;
  }

  /* Select pipe for CFIFO access with write direction */
  USB0.CFIFOSEL = (pipe & k_usb_fifosel_curpipe_mask) | k_usb_fifosel_isel;

  /* Wait for FIFO ready */
  volatile uint32_t timeout = 1000;
  while (!(USB0.CFIFOCTR & k_usb_fifoctr_frdy) && timeout--) {
    __asm__ volatile("nop");
  }

  if (timeout == 0) {
    return 0;
  }

  /* Write data to FIFO */
  for (uint32_t i = 0; i < len; i++) {
    USB0.CFIFO = data[i];
  }

  /* Set buffer valid to signal data ready for transmission */
  USB0.CFIFOCTR |= k_usb_fifoctr_bval;

  return len;
}

/**
 * @brief Get current USB bus state from hardware
 */
rx_usb_state_t rx_usb_hw_get_bus_state(void)
{
  uint16_t intsts0 = USB0.INTSTS0;
  uint16_t dvsq    = (intsts0 & k_usb_intsts0_dvsq_mask);

  switch (dvsq) {
    case k_usb_intsts0_dvsq_powered:
      return k_usb_state_powered;
    case k_usb_intsts0_dvsq_default:
      return k_usb_state_default;
    case k_usb_intsts0_dvsq_address:
      return k_usb_state_addressed;
    case k_usb_intsts0_dvsq_configured:
      return k_usb_state_configured;
    case k_usb_intsts0_dvsq_suspend:
      return k_usb_state_suspended;
    default:
      return k_usb_state_detached;
  }
}

/**
 * @brief Set USB address (called during enumeration)
 */
void rx_usb_hw_set_address(uint8_t address)
{
  USB0.USBADDR = address & 0x7F;
  rx_log_debug(s_tag, "USB address set");
}

/**
 * @brief Configure a pipe for bulk/interrupt transfer
 */
rx_err_t rx_usb_hw_configure_pipe(uint8_t  pipe,
                                  uint8_t  endpoint,
                                  bool     is_in,
                                  uint16_t type,
                                  uint16_t max_packet)
{
  if (pipe == 0 || pipe > 9) {
    return k_rx_err_invalid_arg;
  }

  /* Select pipe for configuration */
  USB0.PIPESEL = pipe;

  /* Configure pipe */
  uint16_t cfg = (endpoint & k_usb_pipecfg_epnum_mask) | type;

  if (is_in) {
    cfg |= k_usb_pipecfg_dir; /* DIR=1 for IN (device to host) */
  }

  USB0.PIPECFG  = cfg;
  USB0.PIPEMAXP = max_packet;

  /* Clear pipe */
  volatile uint16_t* pipe_ctr = &USB0.PIPE1CTR + (pipe - 1);
  *pipe_ctr |= k_usb_pipectr_aclrm;
  *pipe_ctr &= ~k_usb_pipectr_aclrm;

  /* Enable pipe */
  *pipe_ctr = (*pipe_ctr & ~k_usb_pipectr_pid_mask) | k_usb_pipectr_pid_buf;

  return k_rx_ok;
}
