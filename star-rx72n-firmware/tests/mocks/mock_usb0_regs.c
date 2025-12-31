/**
 * @file mock_usb0_regs.c
 * @brief Mock USB0/ICU/SYSTEM Register Implementation
 *
 * Provides mock register instances and helper functions for testing
 * USB driver code on the host without actual hardware.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "mock_usb0_regs.h"

#include <string.h>

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

mock_usb0_regs_t   g_mock_usb0;
mock_icu_regs_t    g_mock_icu;
mock_system_regs_t g_mock_system;

/* =============================================================================
 * USB0 Register Bit Definitions (copied from rx72n_regs.h for tests)
 * =============================================================================
 */

/* INTSTS0 bit positions */
#define INTSTS0_CTSQ_MASK (0x0007)
#define INTSTS0_DVSQ_MASK (0x0070)
#define INTSTS0_DVSQ_SHIFT (4)

/* CFIFOCTR bit definitions */
#define CFIFOCTR_DTLN_MASK (0x01FF)
#define CFIFOCTR_FRDY      (1 << 13)

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

void mock_regs_init(void)
{
  mock_regs_clear();

  /* Set reasonable defaults for USB testing */
  g_mock_usb0.CFIFOCTR = CFIFOCTR_FRDY; /* FIFO ready by default */
  g_mock_usb0.DCPMAXP  = 64;            /* Max packet size for control endpoint */
}

void mock_regs_clear(void)
{
  memset(&g_mock_usb0, 0, sizeof(g_mock_usb0));
  memset(&g_mock_icu, 0, sizeof(g_mock_icu));
  memset(&g_mock_system, 0, sizeof(g_mock_system));
}

void mock_usb0_set_intsts0(uint16_t value)
{
  g_mock_usb0.INTSTS0 = value;
}

void mock_usb0_set_dvsq(uint16_t dvsq)
{
  /* Clear existing DVSQ bits and set new value */
  g_mock_usb0.INTSTS0 =
    (g_mock_usb0.INTSTS0 & ~INTSTS0_DVSQ_MASK) | ((dvsq << INTSTS0_DVSQ_SHIFT) & INTSTS0_DVSQ_MASK);
}

void mock_usb0_set_ctsq(uint16_t ctsq)
{
  /* Clear existing CTSQ bits and set new value */
  g_mock_usb0.INTSTS0 = (g_mock_usb0.INTSTS0 & ~INTSTS0_CTSQ_MASK) | (ctsq & INTSTS0_CTSQ_MASK);
}

void mock_usb0_set_fifo_ready(uint8_t ready)
{
  if (ready) {
    g_mock_usb0.CFIFOCTR |= CFIFOCTR_FRDY;
  } else {
    g_mock_usb0.CFIFOCTR &= ~CFIFOCTR_FRDY;
  }
}

void mock_usb0_set_fifo_dtln(uint16_t len)
{
  g_mock_usb0.CFIFOCTR =
    (g_mock_usb0.CFIFOCTR & ~CFIFOCTR_DTLN_MASK) | (len & CFIFOCTR_DTLN_MASK);
}

void mock_usb0_set_pipe1_busy(uint8_t busy)
{
  if (busy) {
    g_mock_usb0.PIPE1CTR |= (1 << 5); /* PBUSY bit */
  } else {
    g_mock_usb0.PIPE1CTR &= ~(1 << 5);
  }
}
