/* lib/rx_core/src/rx_wdt.c */

/**
 * @file rx_wdt.c
 * @brief Watchdog Timer (WDT) Implementation
 * @details
 * Basic WDT implementation for RX72N providing software-controllable
 * watchdog functionality.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_wdt.h"

#include <string.h>

#include "rx72n_system_regs.h"
#include "rx72n_wdt_regs.h"

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/** @brief WDT driver state */
typedef struct {
  rx_wdt_config_t config;      /**< Configuration */
  bool            initialized; /**< Initialization flag */
  bool            running;     /**< Running state */
} rx_wdt_state_t;

/** @brief Global WDT state */
static rx_wdt_state_t s_wdt_state = {0};

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_wdt_init(const rx_wdt_config_t* config)
{
  rx_wdt_config_t default_config;

  if (s_wdt_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* Use default config if none provided */
  if (config == NULL) {
    memset(&default_config, 0, sizeof(rx_wdt_config_t));
    default_config.timeout_period   = k_wdt_timeout_4096_cycles;
    default_config.enable_on_init   = false;
    default_config.reset_on_timeout = true;
    config                          = &default_config;
  }

  /* Store configuration */
  memcpy(&s_wdt_state.config, config, sizeof(rx_wdt_config_t));
  s_wdt_state.initialized = true;
  s_wdt_state.running     = false;

  /* Start if requested */
  if (config->enable_on_init) {
    return rx_wdt_start();
  }

  return k_rx_ok;
}

rx_err_t rx_wdt_start(void)
{
  volatile rx_wdt_regs_t* regs;

  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* NOTE: WDT is configured via OFS registers at compile/flash time.
   * The start sequence (write 0x00 then 0xFF to WDTRR) only works if
   * WDT was configured to start in register-start mode in OFS.
   * If configured to auto-start, this function has no effect. */
  regs = wdt();

  /* Start WDT (only works in register-start mode) */
  regs->wdtrr = k_wdt_refresh_start;
  regs->wdtrr = k_wdt_refresh_end;

  s_wdt_state.running = true;

  return k_rx_ok;
}

rx_err_t rx_wdt_stop(void)
{
  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* NOTE: WDT cannot be stopped once started if configured to auto-start in OFS.
   * This function only updates software state. The hardware WDT will continue
   * running and must be fed to prevent reset. */
  s_wdt_state.running = false;

  return k_rx_ok;
}

rx_err_t rx_wdt_feed(void)
{
  volatile rx_wdt_regs_t* regs;

  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_wdt_state.running) {
    return k_rx_err_invalid_state;
  }

  /* Refresh watchdog - write 0x00 then 0xFF to WDTRR */
  regs        = wdt();
  regs->wdtrr = k_wdt_refresh_start;
  regs->wdtrr = k_wdt_refresh_end;

  return k_rx_ok;
}
