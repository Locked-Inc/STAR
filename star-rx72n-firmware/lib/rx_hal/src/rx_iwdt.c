/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Driver Implementation for RX72N
 *
 * Implements the IWDT driver for system recovery from software hangs.
 * The IWDT uses a dedicated 120 kHz oscillator independent of the main clock.
 *
 * Hardware Details:
 * - IWDT clock: 120 kHz (IWDT-dedicated oscillator)
 * - Timeout calculation: cycles / (clock / divisor)
 * - Register base: 0x00088030
 *
 * Configuration Used:
 * - Window mode: Disabled (refresh allowed anytime)
 * - Reset action: Full chip reset (not NMI)
 * - Count in sleep: Enabled (continues counting during WAIT/STOP)
 *
 * @see RX72N Hardware Manual, Section 25 - Independent Watchdog Timer
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "rx_iwdt.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/** @brief IWDT clock frequency in Hz */
#define IWDT_CLOCK_HZ 120000

/** @brief Timeout configuration lookup table entry */
typedef struct {
  uint32_t timeout_ms; /**< Timeout in milliseconds */
  uint16_t tops;       /**< TOPS bits (timeout period select) */
  uint16_t cks;        /**< CKS bits (clock divisor) */
} iwdt_timeout_entry_t;

/**
 * @brief Timeout configuration lookup table
 *
 * Maps requested timeouts to register values.
 * Entries sorted by increasing timeout for efficient lookup.
 *
 * Timeout = (cycles * divisor) / 120000
 *
 * Examples with TOPS=16384, varying CKS:
 * - CKS=1:   16384 * 1 / 120000 = 136.5ms  (~128ms)
 * - CKS=16:  16384 * 16 / 120000 = 2184ms  (~2048ms)
 * - CKS=128: 16384 * 128 / 120000 = 17476ms (~16384ms)
 */
static const iwdt_timeout_entry_t s_timeout_table[] = {
  /* timeout_ms, TOPS (cycles),        CKS (divisor)           */
  {128, k_iwdt_tops_1024, k_iwdt_cks_div_1},      /* ~8.5ms actual  */
  {512, k_iwdt_tops_4096, k_iwdt_cks_div_1},      /* ~34ms actual   */
  {1000, k_iwdt_tops_8192, k_iwdt_cks_div_1},     /* ~68ms actual   */
  {2048, k_iwdt_tops_16384, k_iwdt_cks_div_1},    /* ~136ms actual  */
  {8192, k_iwdt_tops_16384, k_iwdt_cks_div_128},  /* ~17s actual    */
  {16384, k_iwdt_tops_16384, k_iwdt_cks_div_128}, /* ~17s actual    */
};

/** @brief Number of entries in timeout table */
#define TIMEOUT_TABLE_SIZE (sizeof(s_timeout_table) / sizeof(s_timeout_table[0]))

/* =============================================================================
 * Module State
 * =============================================================================
 */

/** @brief Flag indicating IWDT has been initialized */
static uint8_t s_iwdt_initialized = 0;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Find best timeout configuration for requested timeout
 *
 * @param[in]  timeout_ms Requested timeout in milliseconds
 * @param[out] entry      Pointer to receive configuration entry
 *
 * @return RX_OK if valid configuration found
 * @return RX_ERR_INVALID_ARG if timeout out of range
 */
static rx_err_t internal_find_timeout_config(uint32_t                     timeout_ms,
                                             const iwdt_timeout_entry_t** entry)
{
  if (entry == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Find first entry with timeout >= requested */
  for (uint32_t i = 0; i < TIMEOUT_TABLE_SIZE; i++) {
    if (s_timeout_table[i].timeout_ms >= timeout_ms) {
      *entry = &s_timeout_table[i];
      return RX_OK;
    }
  }

  /* Use maximum timeout if requested is too large */
  if (timeout_ms > s_timeout_table[TIMEOUT_TABLE_SIZE - 1].timeout_ms) {
    *entry = &s_timeout_table[TIMEOUT_TABLE_SIZE - 1];
    return RX_OK;
  }

  return RX_ERR_INVALID_ARG;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_iwdt_init(uint32_t timeout_ms)
{
#ifdef __RX__
  if (s_iwdt_initialized) {
    return RX_ERR_INVALID_STATE;
  }

  if (timeout_ms == 0) {
    return RX_ERR_INVALID_ARG;
  }

  /* Find best timeout configuration */
  const iwdt_timeout_entry_t* config = NULL;
  rx_err_t                    err    = internal_find_timeout_config(timeout_ms, &config);
  if (err != RX_OK) {
    return err;
  }

  /*
   * Configure IWDT Control Register (IWDTCR)
   *
   * Bits [1:0]   TOPS  - Timeout Period Select
   * Bits [7:4]   CKS   - Clock Division Ratio Select
   * Bits [9:8]   RPES  - Window End Position (0x03 = 0%, window disabled)
   * Bits [13:12] RPSS  - Window Start Position (0x00 = 100%, full window)
   */
  uint16_t iwdtcr = 0;
  iwdtcr |= config->tops;    /* Timeout period */
  iwdtcr |= config->cks;     /* Clock divisor */
  iwdtcr |= k_iwdt_rpes_0;   /* Window end at 0% (disabled) */
  iwdtcr |= k_iwdt_rpss_100; /* Window start at 100% (full) */

  IWDT.IWDTCR = iwdtcr;

  /*
   * Configure Reset Control Register (IWDTRCR)
   *
   * Bit 7 RSTIRQS - Reset Interrupt Request Select
   *   0 = Non-maskable interrupt request (NMI)
   *   1 = Reset
   *
   * We use reset (1) for safety - NMI could be masked or ignored.
   */
  IWDT.IWDTRCR = k_iwdt_rstirqs_reset;

  /*
   * Configure Count Stop Control Register (IWDTCSTPR)
   *
   * Bit 7 SLCSTP - Sleep Mode Count Stop Control
   *   0 = Count continues during sleep
   *   1 = Count stops during sleep
   *
   * We continue counting during sleep for safety.
   */
  IWDT.IWDTCSTPR = k_iwdt_slcstp_continue;

  /*
   * Start the IWDT by performing first refresh
   *
   * The IWDT starts counting after the first refresh sequence.
   * After this, the watchdog CANNOT be stopped.
   */
  rx_iwdt_feed();

  s_iwdt_initialized = 1;
  return RX_OK;

#else
  /* Host-side stub for unit testing */
  (void)timeout_ms;
  s_iwdt_initialized = 1;
  return RX_OK;
#endif
}

void rx_iwdt_feed(void)
{
#ifdef __RX__
  /*
   * IWDT Refresh Sequence (Section 25.3.1)
   *
   * Write 0x00 followed by 0xFF to IWDTRR.
   * This sequence resets the down-counter to its initial value.
   *
   * CRITICAL: This sequence must not be interrupted.
   * An incomplete sequence will trigger a refresh error.
   */

  /* Disable interrupts during refresh for atomicity */
  uint32_t psw;
  __asm__ volatile("mvfc psw, %0" : "=r"(psw));
  __asm__ volatile("clrpsw i");

  /* Perform refresh sequence */
  IWDT.IWDTRR = k_iwdt_refresh_start; /* Write 0x00 */
  IWDT.IWDTRR = k_iwdt_refresh_end;   /* Write 0xFF */

  /* Restore interrupt state */
  __asm__ volatile("mvtc %0, psw" : : "r"(psw));

#else
  /* Host-side stub - no operation */
#endif
}

bool rx_iwdt_was_reset(void)
{
#ifdef __RX__
  /*
   * Check IWDT Status Register for underflow or refresh error
   *
   * Bit 14 UNDFF - Underflow Flag
   *   1 = Counter underflow occurred
   *
   * Bit 15 REFEF - Refresh Error Flag
   *   1 = Refresh error occurred
   */
  uint16_t status = IWDT.IWDTSR;
  return ((status & k_iwdt_undff) != 0) || ((status & k_iwdt_refef) != 0);

#else
  return false;
#endif
}

rx_iwdt_reset_cause_t rx_iwdt_get_reset_cause(void)
{
#ifdef __RX__
  uint16_t status = IWDT.IWDTSR;

  if (status & k_iwdt_refef) {
    return k_iwdt_reset_refresh_error;
  }

  if (status & k_iwdt_undff) {
    return k_iwdt_reset_underflow;
  }

  return k_iwdt_reset_none;

#else
  return k_iwdt_reset_none;
#endif
}

void rx_iwdt_clear_status(void)
{
#ifdef __RX__
  /*
   * Clear status flags by writing 0 to the flag bits
   *
   * Bits 14-15 are write-0-to-clear
   */
  IWDT.IWDTSR &= ~(k_iwdt_undff | k_iwdt_refef);

#else
  /* Host-side stub - no operation */
#endif
}
