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
  s_state.pdr.port0_pdr = port0()->pdr;
  s_state.pdr.port1_pdr = port1()->pdr;
  s_state.pdr.port2_pdr = port2()->pdr;
  s_state.pdr.port3_pdr = port3()->pdr;
  s_state.pdr.port4_pdr = port4()->pdr;
  s_state.pdr.port5_pdr = port5()->pdr;
  s_state.pdr.port6_pdr = port6()->pdr;
  s_state.pdr.port7_pdr = port7()->pdr;
  s_state.pdr.port8_pdr = port8()->pdr;
  s_state.pdr.port9_pdr = port9()->pdr;
  s_state.pdr.porta_pdr = porta()->pdr;
  s_state.pdr.portb_pdr = portb()->pdr;
  s_state.pdr.portc_pdr = portc()->pdr;
  s_state.pdr.portd_pdr = portd()->pdr;
  s_state.pdr.porte_pdr = porte()->pdr;
  s_state.pdr.portf_pdr = portf()->pdr;
  s_state.pdr.portg_pdr = portg()->pdr;
  s_state.pdr.portj_pdr = portj()->pdr;
}

/**
 * @brief Capture current MSTPCR values as golden reference
 */
static void internal_capture_mstpcr(void)
{
  s_state.mstpcr.mstpcra = system_regs()->mstpcra;
  s_state.mstpcr.mstpcrb = system_regs()->mstpcrb;
  s_state.mstpcr.mstpcrc = system_regs()->mstpcrc;
  s_state.mstpcr.mstpcrd = system_regs()->mstpcrd;
}

/**
 * @brief Refresh PORT PDR values, counting corrections
 */
static void internal_refresh_pdr(void)
{
  /* Check and restore each PORT PDR */
  if (port0()->pdr != s_state.pdr.port0_pdr) {
    port0()->pdr = s_state.pdr.port0_pdr;
    s_state.corrections++;
  }
  if (port1()->pdr != s_state.pdr.port1_pdr) {
    port1()->pdr = s_state.pdr.port1_pdr;
    s_state.corrections++;
  }
  if (port2()->pdr != s_state.pdr.port2_pdr) {
    port2()->pdr = s_state.pdr.port2_pdr;
    s_state.corrections++;
  }
  if (port3()->pdr != s_state.pdr.port3_pdr) {
    port3()->pdr = s_state.pdr.port3_pdr;
    s_state.corrections++;
  }
  if (port4()->pdr != s_state.pdr.port4_pdr) {
    port4()->pdr = s_state.pdr.port4_pdr;
    s_state.corrections++;
  }
  if (port5()->pdr != s_state.pdr.port5_pdr) {
    port5()->pdr = s_state.pdr.port5_pdr;
    s_state.corrections++;
  }
  if (port6()->pdr != s_state.pdr.port6_pdr) {
    port6()->pdr = s_state.pdr.port6_pdr;
    s_state.corrections++;
  }
  if (port7()->pdr != s_state.pdr.port7_pdr) {
    port7()->pdr = s_state.pdr.port7_pdr;
    s_state.corrections++;
  }
  if (port8()->pdr != s_state.pdr.port8_pdr) {
    port8()->pdr = s_state.pdr.port8_pdr;
    s_state.corrections++;
  }
  if (port9()->pdr != s_state.pdr.port9_pdr) {
    port9()->pdr = s_state.pdr.port9_pdr;
    s_state.corrections++;
  }
  if (porta()->pdr != s_state.pdr.porta_pdr) {
    porta()->pdr = s_state.pdr.porta_pdr;
    s_state.corrections++;
  }
  if (portb()->pdr != s_state.pdr.portb_pdr) {
    portb()->pdr = s_state.pdr.portb_pdr;
    s_state.corrections++;
  }
  if (portc()->pdr != s_state.pdr.portc_pdr) {
    portc()->pdr = s_state.pdr.portc_pdr;
    s_state.corrections++;
  }
  if (portd()->pdr != s_state.pdr.portd_pdr) {
    portd()->pdr = s_state.pdr.portd_pdr;
    s_state.corrections++;
  }
  if (porte()->pdr != s_state.pdr.porte_pdr) {
    porte()->pdr = s_state.pdr.porte_pdr;
    s_state.corrections++;
  }
  if (portf()->pdr != s_state.pdr.portf_pdr) {
    portf()->pdr = s_state.pdr.portf_pdr;
    s_state.corrections++;
  }
  if (portg()->pdr != s_state.pdr.portg_pdr) {
    portg()->pdr = s_state.pdr.portg_pdr;
    s_state.corrections++;
  }
  if (portj()->pdr != s_state.pdr.portj_pdr) {
    portj()->pdr = s_state.pdr.portj_pdr;
    s_state.corrections++;
  }
}

/**
 * @brief Refresh MSTPCR values, counting corrections
 *
 * Requires PRCR unlock for write access. Uses inline assembly to atomically
 * save/restore interrupt state during the critical section where PRCR is
 * unlocked. This is necessary because there's no C library function to
 * manipulate the PSW (Program Status Word) register on RX architecture.
 */
static void internal_refresh_mstpcr(void)
{
  bool needs_update = false;

  /* Check if any MSTPCR needs update */
  if (system_regs()->mstpcra != s_state.mstpcr.mstpcra) {
    needs_update = true;
  }
  if (system_regs()->mstpcrb != s_state.mstpcr.mstpcrb) {
    needs_update = true;
  }
  if (system_regs()->mstpcrc != s_state.mstpcr.mstpcrc) {
    needs_update = true;
  }
  if (system_regs()->mstpcrd != s_state.mstpcr.mstpcrd) {
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

  system_regs()->prcr =
    (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_prc1; /* Unlock MSTPCR writes */

  if (system_regs()->mstpcra != s_state.mstpcr.mstpcra) {
    system_regs()->mstpcra = s_state.mstpcr.mstpcra;
    s_state.corrections++;
  }
  if (system_regs()->mstpcrb != s_state.mstpcr.mstpcrb) {
    system_regs()->mstpcrb = s_state.mstpcr.mstpcrb;
    s_state.corrections++;
  }
  if (system_regs()->mstpcrc != s_state.mstpcr.mstpcrc) {
    system_regs()->mstpcrc = s_state.mstpcr.mstpcrc;
    s_state.corrections++;
  }
  if (system_regs()->mstpcrd != s_state.mstpcr.mstpcrd) {
    system_regs()->mstpcrd = s_state.mstpcr.mstpcrd;
    s_state.corrections++;
  }

  system_regs()->prcr = (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_all; /* Lock MSTPCR writes */

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
