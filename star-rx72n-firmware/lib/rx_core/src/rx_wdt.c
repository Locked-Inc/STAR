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

/* =============================================================================
 * Hardware Register Definitions
 * =============================================================================
 */

/** @brief WDT register addresses */
typedef enum {
  k_wdt_base_addr = 0x00088020, /**< WDT base address */
} wdt_hw_addr_t;

/** @brief WDT register structure */
typedef struct {
  volatile uint8_t  wdtcr; /**< Control Register */
  volatile uint8_t  _pad0[1];
  volatile uint16_t wdtsr;  /**< Status Register */
  volatile uint16_t wdtrcr; /**< Refresh Register */
} rx_wdt_regs_t;

/** @brief WDT hardware constants */
typedef enum {
  k_wdt_refresh_key  = 0x00, /**< Refresh key */
  k_wdt_enable_bit   = 0x80, /**< Enable bit */
  k_wdt_divider_mask = 0x03, /**< Clock divider mask */
} wdt_hw_constants_t;

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
 * Hardware Access Functions
 * =============================================================================
 */

/**
 * @brief Get WDT register base
 * @return Pointer to WDT registers
 */
static inline volatile rx_wdt_regs_t* wdt_regs(void)
{
  return (volatile rx_wdt_regs_t*)k_wdt_base_addr;
}

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

  regs = wdt_regs();

  /* Configure and enable WDT */
  regs->wdtcr = (uint8_t)(k_wdt_enable_bit | s_wdt_state.config.timeout_period);

  s_wdt_state.running = true;

  return k_rx_ok;
}

rx_err_t rx_wdt_stop(void)
{
  volatile rx_wdt_regs_t* regs;

  if (!s_wdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  regs = wdt_regs();

  /* Disable WDT */
  regs->wdtcr = 0;

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

  /* Refresh watchdog */
  regs         = wdt_regs();
  regs->wdtrcr = k_wdt_refresh_key;

  return k_rx_ok;
}
