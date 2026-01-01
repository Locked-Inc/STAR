/**
 * @file rx_register_guard.c
 * @brief Register Guard Implementation - ESD/EMI Protection
 *
 * Implements periodic refresh of critical registers to recover from
 * transient corruption caused by electrical noise.
 *
 * Guarded Registers:
 * - PORT0-PORT9, PORTA-PORTG, PORTJ PDR (port direction)
 * - SYSTEM.MSTPCRA/B/C/D (module stop control)
 *
 * @see RX72N Hardware Manual for register details
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_register_guard.h"

#include "rx_gpio_constants.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Types
 * =============================================================================
 */

/**
 * @brief Captured golden values for PORT PDR registers
 */
typedef struct {
  uint8_t port0_pdr;
  uint8_t port1_pdr;
  uint8_t port2_pdr;
  uint8_t port3_pdr;
  uint8_t port4_pdr;
  uint8_t port5_pdr;
  uint8_t port6_pdr;
  uint8_t port7_pdr;
  uint8_t port8_pdr;
  uint8_t port9_pdr;
  uint8_t porta_pdr;
  uint8_t portb_pdr;
  uint8_t portc_pdr;
  uint8_t portd_pdr;
  uint8_t porte_pdr;
  uint8_t portf_pdr;
  uint8_t portg_pdr;
  uint8_t portj_pdr;
} pdr_golden_t;

/**
 * @brief Captured golden values for module stop registers
 */
typedef struct {
  uint32_t mstpcra;
  uint32_t mstpcrb;
  uint32_t mstpcrc;
  uint32_t mstpcrd;
} mstpcr_golden_t;

/**
 * @brief Module state
 */
typedef struct {
  pdr_golden_t    pdr;         /**< Golden PDR values */
  mstpcr_golden_t mstpcr;      /**< Golden MSTPCR values */
  uint32_t        corrections; /**< Number of corrections made */
  uint8_t         initialized; /**< Module initialized flag */
} register_guard_state_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

static register_guard_state_t s_state = {0};

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

#ifdef __RX__
/**
 * @brief Capture current PORT PDR values as golden reference
 */
static void internal_capture_pdr(void)
{
  s_state.pdr.port0_pdr = PORT0.PDR;
  s_state.pdr.port1_pdr = PORT1.PDR;
  s_state.pdr.port2_pdr = PORT2.PDR;
  s_state.pdr.port3_pdr = PORT3.PDR;
  s_state.pdr.port4_pdr = PORT4.PDR;
  s_state.pdr.port5_pdr = PORT5.PDR;
  s_state.pdr.port6_pdr = PORT6.PDR;
  s_state.pdr.port7_pdr = PORT7.PDR;
  s_state.pdr.port8_pdr = PORT8.PDR;
  s_state.pdr.port9_pdr = PORT9.PDR;
  s_state.pdr.porta_pdr = PORTA.PDR;
  s_state.pdr.portb_pdr = PORTB.PDR;
  s_state.pdr.portc_pdr = PORTC.PDR;
  s_state.pdr.portd_pdr = PORTD.PDR;
  s_state.pdr.porte_pdr = PORTE.PDR;
  s_state.pdr.portf_pdr = PORTF.PDR;
  s_state.pdr.portg_pdr = PORTG.PDR;
  s_state.pdr.portj_pdr = PORTJ.PDR;
}

/**
 * @brief Capture current MSTPCR values as golden reference
 */
static void internal_capture_mstpcr(void)
{
  s_state.mstpcr.mstpcra = SYSTEM.MSTPCRA;
  s_state.mstpcr.mstpcrb = SYSTEM.MSTPCRB;
  s_state.mstpcr.mstpcrc = SYSTEM.MSTPCRC;
  s_state.mstpcr.mstpcrd = SYSTEM.MSTPCRD;
}

/**
 * @brief Refresh PORT PDR values, counting corrections
 */
static void internal_refresh_pdr(void)
{
  /* Check and restore each PORT PDR */
  if (PORT0.PDR != s_state.pdr.port0_pdr) {
    PORT0.PDR = s_state.pdr.port0_pdr;
    s_state.corrections++;
  }
  if (PORT1.PDR != s_state.pdr.port1_pdr) {
    PORT1.PDR = s_state.pdr.port1_pdr;
    s_state.corrections++;
  }
  if (PORT2.PDR != s_state.pdr.port2_pdr) {
    PORT2.PDR = s_state.pdr.port2_pdr;
    s_state.corrections++;
  }
  if (PORT3.PDR != s_state.pdr.port3_pdr) {
    PORT3.PDR = s_state.pdr.port3_pdr;
    s_state.corrections++;
  }
  if (PORT4.PDR != s_state.pdr.port4_pdr) {
    PORT4.PDR = s_state.pdr.port4_pdr;
    s_state.corrections++;
  }
  if (PORT5.PDR != s_state.pdr.port5_pdr) {
    PORT5.PDR = s_state.pdr.port5_pdr;
    s_state.corrections++;
  }
  if (PORT6.PDR != s_state.pdr.port6_pdr) {
    PORT6.PDR = s_state.pdr.port6_pdr;
    s_state.corrections++;
  }
  if (PORT7.PDR != s_state.pdr.port7_pdr) {
    PORT7.PDR = s_state.pdr.port7_pdr;
    s_state.corrections++;
  }
  if (PORT8.PDR != s_state.pdr.port8_pdr) {
    PORT8.PDR = s_state.pdr.port8_pdr;
    s_state.corrections++;
  }
  if (PORT9.PDR != s_state.pdr.port9_pdr) {
    PORT9.PDR = s_state.pdr.port9_pdr;
    s_state.corrections++;
  }
  if (PORTA.PDR != s_state.pdr.porta_pdr) {
    PORTA.PDR = s_state.pdr.porta_pdr;
    s_state.corrections++;
  }
  if (PORTB.PDR != s_state.pdr.portb_pdr) {
    PORTB.PDR = s_state.pdr.portb_pdr;
    s_state.corrections++;
  }
  if (PORTC.PDR != s_state.pdr.portc_pdr) {
    PORTC.PDR = s_state.pdr.portc_pdr;
    s_state.corrections++;
  }
  if (PORTD.PDR != s_state.pdr.portd_pdr) {
    PORTD.PDR = s_state.pdr.portd_pdr;
    s_state.corrections++;
  }
  if (PORTE.PDR != s_state.pdr.porte_pdr) {
    PORTE.PDR = s_state.pdr.porte_pdr;
    s_state.corrections++;
  }
  if (PORTF.PDR != s_state.pdr.portf_pdr) {
    PORTF.PDR = s_state.pdr.portf_pdr;
    s_state.corrections++;
  }
  if (PORTG.PDR != s_state.pdr.portg_pdr) {
    PORTG.PDR = s_state.pdr.portg_pdr;
    s_state.corrections++;
  }
  if (PORTJ.PDR != s_state.pdr.portj_pdr) {
    PORTJ.PDR = s_state.pdr.portj_pdr;
    s_state.corrections++;
  }
}

/**
 * @brief Refresh MSTPCR values, counting corrections
 *
 * Requires PRCR unlock for write access
 */
static void internal_refresh_mstpcr(void)
{
  bool needs_update = false;

  /* Check if any MSTPCR needs update */
  if (SYSTEM.MSTPCRA != s_state.mstpcr.mstpcra) {
    needs_update = true;
  }
  if (SYSTEM.MSTPCRB != s_state.mstpcr.mstpcrb) {
    needs_update = true;
  }
  if (SYSTEM.MSTPCRC != s_state.mstpcr.mstpcrc) {
    needs_update = true;
  }
  if (SYSTEM.MSTPCRD != s_state.mstpcr.mstpcrd) {
    needs_update = true;
  }

  if (!needs_update) {
    return;
  }

  /*
   * MSTPCR requires PRCR unlock to modify
   *
   * PRCR bit 1 (PRC1) controls module stop registers
   * Key is in upper byte, PRC1 bit enables module stop register writes
   */
  uint32_t psw;
  __asm__ volatile("mvfc psw, %0" : "=r"(psw));
  __asm__ volatile("clrpsw i"); /* Disable interrupts during unlock */

  SYSTEM.PRCR = (k_prcr_key << 8) | k_prcr_lock_prc1; /* Unlock MSTPCR writes */

  if (SYSTEM.MSTPCRA != s_state.mstpcr.mstpcra) {
    SYSTEM.MSTPCRA = s_state.mstpcr.mstpcra;
    s_state.corrections++;
  }
  if (SYSTEM.MSTPCRB != s_state.mstpcr.mstpcrb) {
    SYSTEM.MSTPCRB = s_state.mstpcr.mstpcrb;
    s_state.corrections++;
  }
  if (SYSTEM.MSTPCRC != s_state.mstpcr.mstpcrc) {
    SYSTEM.MSTPCRC = s_state.mstpcr.mstpcrc;
    s_state.corrections++;
  }
  if (SYSTEM.MSTPCRD != s_state.mstpcr.mstpcrd) {
    SYSTEM.MSTPCRD = s_state.mstpcr.mstpcrd;
    s_state.corrections++;
  }

  SYSTEM.PRCR = (k_prcr_key << 8) | k_prcr_lock_all; /* Lock MSTPCR writes */

  __asm__ volatile("mvtc %0, psw" : : "r"(psw)); /* Restore interrupt state */
}
#endif /* __RX__ */

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_register_guard_init(void)
{
#ifdef __RX__
  /* Capture current register values as golden reference */
  internal_capture_pdr();
  internal_capture_mstpcr();
#endif

  s_state.corrections = 0;
  s_state.initialized = 1;

  return k_rx_ok;
}

void rx_register_guard_refresh(void)
{
  if (!s_state.initialized) {
    return;
  }

#ifdef __RX__
  internal_refresh_pdr();
  internal_refresh_mstpcr();
#endif
}

uint32_t rx_register_guard_get_correction_count(void)
{
  return s_state.corrections;
}

void rx_register_guard_reset_count(void)
{
  s_state.corrections = 0;
}

bool rx_register_guard_is_initialized(void)
{
  return s_state.initialized != 0;
}
