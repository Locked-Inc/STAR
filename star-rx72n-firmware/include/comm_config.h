/* include/comm_config.h */

/**
 * @file comm_config.h
 * @brief Communication configuration constants
 * @details
 * Defines RPi5 communication parameters (USB CDC / SPI).
 * Used by Comm_Manager task.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_COMM_CONFIG_H
#define STAR_RX72N_COMM_CONFIG_H

/* =============================================================================
 * Communication Configuration
 * =============================================================================
 */

/**
 * @brief Communication timing
 * @details RPi5 communication via USB CDC / SPI
 */
typedef enum {
  k_comm_rate_hz         = 100, /**< Communication rate (Hz) */
  k_comm_period_ms       = 10,  /**< Communication period (ms) = 1000/100 */
  k_comm_timeout_ms      = 500, /**< Timeout before E-STOP (ms) */
  k_comm_max_frame_bytes = 256, /**< Maximum frame size (bytes) */
} comm_config_t;

#endif /* STAR_RX72N_COMM_CONFIG_H */
