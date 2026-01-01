/* lib/rx_core/inc/rx_gpio_constants.h */

/**
 * @file rx_gpio_constants.h
 * @brief GPIO and System Constants for RX72N
 *
 * Defines named constants for GPIO pins, error handler defaults, and
 * PRCR (Protect Register) values. Used throughout rx_core to replace
 * magic numbers and improve code readability.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_GPIO_CONSTANTS_H
#define STAR_RX72N_GPIO_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Port and Pin Constants
 * =============================================================================
 */

/**
 * @brief GPIO port and pin constants for RX72N
 *
 * The RX72N has decimal ports (0-9) and hexadecimal ports (A-G, mapped as
 * 0xA-0x10). Each port has up to 8 pins (0-7).
 */
typedef enum {
  k_pins_per_port         = 8,    /**< Pins per GPIO port (0-7) */
  k_max_decimal_port      = 9,    /**< Maximum decimal port number */
  k_hex_port_start        = 0xA,  /**< First hex port (A) */
  k_hex_port_end          = 0x10, /**< Last hex port (G) */
  k_hex_port_offset       = 10,   /**< Offset for hex port mapping */
  k_invalid_pin_index     = 0xFFFF, /**< Invalid pin array index (16-bit) */
  k_invalid_port          = 0xFF,   /**< Invalid port number (8-bit) */
} rx_gpio_constants_t;

/* =============================================================================
 * Error Handler Defaults
 * =============================================================================
 */

/**
 * @brief Error handler default configuration values
 *
 * Default retry and backoff parameters for the error handler.
 */
typedef enum {
  k_error_handler_default_max_retries        = 3,    /**< Default max retry count */
  k_error_handler_default_initial_backoff_ms = 100,  /**< Default initial backoff (ms) */
  k_error_handler_default_max_backoff_ms     = 5000, /**< Default max backoff (ms) */
} rx_error_handler_defaults_t;

/* =============================================================================
 * PRCR (Protect Register) Constants
 * =============================================================================
 */

/**
 * @brief PRCR (Protect Register) constants for RX72N
 *
 * The PRCR register controls write protection for critical system registers.
 * Writing requires a key (0xA5) in the upper byte.
 *
 * PRC1 bit controls:
 * - Module stop control registers (MSTPCRA/B/C/D)
 *
 * Reference: RX72N Hardware Manual, PRCR register section
 */
typedef enum {
  k_prcr_key       = 0xA5, /**< PRCR unlock key (upper byte) */
  k_prcr_lock_all  = 0x00, /**< Lock all protected registers */
  k_prcr_lock_prc1 = 0x02, /**< Unlock PRC1 (module stop control) */
} rx_prcr_constants_t;

/* =============================================================================
 * ThreadX Time Constants
 * =============================================================================
 */

/**
 * @brief ThreadX time conversion constants
 *
 * ThreadX is configured at 100 Hz tick rate (TX_TIMER_TICKS_PER_SECOND = 100),
 * which means each tick is 10 milliseconds.
 */
typedef enum {
  k_threadx_ms_per_tick = 10, /**< Milliseconds per ThreadX tick (100 Hz = 10ms) */
} rx_threadx_time_constants_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_GPIO_CONSTANTS_H */
