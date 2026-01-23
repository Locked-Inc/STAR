/**
 * @file mock_usb0_regs.c
 * @brief Mock USB0/ICU/SYSTEM Register Implementation
 *
 * Provides mock register instances and helper functions for testing
 * USB driver code on the host without actual hardware.
 *
 * @date 2025-12-01
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "mock_usb0_regs.h"

#include <assert.h>
#include <string.h>

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

mock_usb0_regs_t   g_mock_usb0;
mock_icu_regs_t    g_mock_icu;
mock_system_regs_t g_mock_system;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

void mock_regs_init(void)
{
  mock_regs_clear();

  /* Set reasonable defaults for USB testing */
  g_mock_usb0.cfifoctr = k_usb_cfifoctr_frdy;   /* FIFO ready by default */
  g_mock_usb0.dcpmaxp  = k_usb_dcpmaxp_default; /* Max packet size for control endpoint */
}

void mock_regs_clear(void)
{
  memset(&g_mock_usb0, 0, sizeof(g_mock_usb0));
  memset(&g_mock_icu, 0, sizeof(g_mock_icu));
  memset(&g_mock_system, 0, sizeof(g_mock_system));
}

void mock_usb0_set_intsts0(uint16_t value)
{
  g_mock_usb0.intsts0 = value;
}

void mock_usb0_set_dvsq(uint16_t dvsq)
{
  /* Validate DVSQ value is within valid 3-bit range (0-7) */
  assert(dvsq <= k_usb_dvsq_max_value);
  /* Clear existing DVSQ bits and set new value */
  g_mock_usb0.intsts0 = (g_mock_usb0.intsts0 & ~k_usb_intsts0_dvsq_mask) |
                        ((dvsq << k_usb_intsts0_dvsq_shift) & k_usb_intsts0_dvsq_mask);
}

void mock_usb0_set_ctsq(uint16_t ctsq)
{
  /* Validate CTSQ value is within valid 3-bit range (0-7) */
  assert(ctsq <= k_usb_ctsq_max_value);
  /* Clear existing CTSQ bits and set new value */
  g_mock_usb0.intsts0 =
    (g_mock_usb0.intsts0 & ~k_usb_intsts0_ctsq_mask) | (ctsq & k_usb_intsts0_ctsq_mask);
}

void mock_usb0_set_fifo_ready(uint8_t ready)
{
  if (ready) {
    g_mock_usb0.cfifoctr |= k_usb_cfifoctr_frdy;
  } else {
    g_mock_usb0.cfifoctr &= ~k_usb_cfifoctr_frdy;
  }
}

void mock_usb0_set_fifo_dtln(uint16_t len)
{
  /* Validate DTLN value is within valid 9-bit range (0-511) */
  assert(len <= k_usb_dtln_max_value);
  g_mock_usb0.cfifoctr =
    (g_mock_usb0.cfifoctr & ~k_usb_cfifoctr_dtln_mask) | (len & k_usb_cfifoctr_dtln_mask);
}

void mock_usb0_set_pipe1_busy(uint8_t busy)
{
  if (busy) {
    g_mock_usb0.pipe1ctr |= k_usb_pipectr_pbusy;
  } else {
    g_mock_usb0.pipe1ctr &= ~k_usb_pipectr_pbusy;
  }
}
