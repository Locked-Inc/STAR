/* lib/rx_core/inc/rx_package_config.h */

/**
 * @file rx_package_config.h
 * @brief RX72N Package Configuration
 *
 * Defines which RX72N package variant is being used and which ports are available.
 * This prevents accidentally using ports that don't exist on your hardware.
 *
 * Package Options:
 * - RX72N_PACKAGE_100PIN  - 100-pin LFQFP (R5F572NNHGFP#30) - STAR project default
 * - RX72N_PACKAGE_144PIN  - 144-pin LFQFP
 * - RX72N_PACKAGE_145PIN  - 145-pin TFLGA
 * - RX72N_PACKAGE_176PIN  - 176-pin LFQFP or LFBGA
 * - RX72N_PACKAGE_224PIN  - 224-pin LFBGA (all ports available)
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_PACKAGE_CONFIG_H
#define STAR_RX_PACKAGE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Package Selection
 * =============================================================================
 */

/**
 * STAR project uses 100-pin LFQFP package (R5F572NNHGFP#30).
 * This is the only supported package variant.
 */
#define RX72N_PACKAGE_100PIN

/* =============================================================================
 * Port Availability (100-pin LFQFP Only)
 * =============================================================================
 */

/**
 * Available ports on 100-pin LFQFP package (R5F572NNHGFP#30).
 * Ports 6, 7, 8, 9, F, G, H, K, L, M, N, Q are NOT available on this package.
 */
#define RX72N_PORT0_AVAILABLE  /* Limited: P05, P07 only */
#define RX72N_PORT1_AVAILABLE  /* Limited: P12-P17 only */
#define RX72N_PORT2_AVAILABLE  /* Full: P20-P27 */
#define RX72N_PORT3_AVAILABLE  /* Full: P30-P37 */
#define RX72N_PORT4_AVAILABLE  /* Full: P40-P47 */
#define RX72N_PORT5_AVAILABLE  /* Limited: P50-P55 only */
#define RX72N_PORTA_AVAILABLE  /* Full: PA0-PA7 */
#define RX72N_PORTB_AVAILABLE  /* Full: PB0-PB7 */
#define RX72N_PORTC_AVAILABLE  /* Full: PC0-PC7 */
#define RX72N_PORTD_AVAILABLE  /* Full: PD0-PD7 */
#define RX72N_PORTE_AVAILABLE  /* Full: PE0-PE7 */
#define RX72N_PORTJ_AVAILABLE  /* Limited: PJ3, PJ5 only */

/* =============================================================================
 * Package Information
 * =============================================================================
 */

/** @brief Package name (for debug/logging) */
#define RX72N_PACKAGE_NAME "100-pin LFQFP"

/** @brief Number of usable I/O pins (not counting power/clock) */
#define RX72N_IO_PIN_COUNT (63)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PACKAGE_CONFIG_H */
