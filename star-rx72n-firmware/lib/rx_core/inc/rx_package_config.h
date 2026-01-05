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
 * Define ONE of these to specify your RX72N package variant.
 * If none is defined, defaults to 100-pin LFQFP (STAR project hardware).
 */
// #define RX72N_PACKAGE_100PIN
// #define RX72N_PACKAGE_144PIN
// #define RX72N_PACKAGE_145PIN
// #define RX72N_PACKAGE_176PIN
// #define RX72N_PACKAGE_224PIN

/* Default to 100-pin if not specified */
#if !defined(RX72N_PACKAGE_100PIN) && \
    !defined(RX72N_PACKAGE_144PIN) && \
    !defined(RX72N_PACKAGE_145PIN) && \
    !defined(RX72N_PACKAGE_176PIN) && \
    !defined(RX72N_PACKAGE_224PIN)
#define RX72N_PACKAGE_100PIN
#endif

/* Verify only one package is defined */
#if (defined(RX72N_PACKAGE_100PIN) + \
     defined(RX72N_PACKAGE_144PIN) + \
     defined(RX72N_PACKAGE_145PIN) + \
     defined(RX72N_PACKAGE_176PIN) + \
     defined(RX72N_PACKAGE_224PIN)) > 1
#error "Only one RX72N_PACKAGE_* macro should be defined!"
#endif

/* =============================================================================
 * Port Availability by Package
 * =============================================================================
 */

/*
 * Define which ports are available on each package variant.
 * Ports not listed are NOT available and will generate compile errors if used.
 */

#if defined(RX72N_PACKAGE_100PIN)
/* 100-pin LFQFP (R5F572NNHGFP#30) - STAR Project Hardware */
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
/* Port 6, 7, 8, 9, F, G NOT available */

#elif defined(RX72N_PACKAGE_144PIN) || defined(RX72N_PACKAGE_145PIN)
/* 144-pin LFQFP or 145-pin TFLGA */
#define RX72N_PORT0_AVAILABLE  /* Limited: P00-P03, P05, P07 */
#define RX72N_PORT1_AVAILABLE  /* Limited: P12-P17 only */
#define RX72N_PORT2_AVAILABLE  /* Full: P20-P27 */
#define RX72N_PORT3_AVAILABLE  /* Full: P30-P37 */
#define RX72N_PORT4_AVAILABLE  /* Full: P40-P47 */
#define RX72N_PORT5_AVAILABLE  /* Limited: P50-P56 only */
#define RX72N_PORT8_AVAILABLE  /* Limited: P80-P83, P86-P87 only */
#define RX72N_PORT9_AVAILABLE  /* Limited: P90-P93 only */
#define RX72N_PORTA_AVAILABLE  /* Full: PA0-PA7 */
#define RX72N_PORTB_AVAILABLE  /* Full: PB0-PB7 */
#define RX72N_PORTC_AVAILABLE  /* Full: PC0-PC7 */
#define RX72N_PORTD_AVAILABLE  /* Full: PD0-PD7 */
#define RX72N_PORTE_AVAILABLE  /* Full: PE0-PE7 */
#define RX72N_PORTF_AVAILABLE  /* Limited: PF5 only */
#define RX72N_PORTJ_AVAILABLE  /* Limited: PJ3, PJ5 only */
/* Port 6, 7, G NOT available */

#elif defined(RX72N_PACKAGE_176PIN)
/* 176-pin LFQFP or LFBGA */
#define RX72N_PORT0_AVAILABLE  /* Full: P00-P07 */
#define RX72N_PORT1_AVAILABLE  /* Full: P10-P17 */
#define RX72N_PORT2_AVAILABLE  /* Full: P20-P27 */
#define RX72N_PORT3_AVAILABLE  /* Full: P30-P37 */
#define RX72N_PORT4_AVAILABLE  /* Full: P40-P47 */
#define RX72N_PORT5_AVAILABLE  /* Full: P50-P57 */
#define RX72N_PORT6_AVAILABLE  /* Full: P60-P67 */
#define RX72N_PORT7_AVAILABLE  /* Full: P70-P77 */
#define RX72N_PORT8_AVAILABLE  /* Full: P80-P87 */
#define RX72N_PORT9_AVAILABLE  /* Full: P90-P97 */
#define RX72N_PORTA_AVAILABLE  /* Full: PA0-PA7 */
#define RX72N_PORTB_AVAILABLE  /* Full: PB0-PB7 */
#define RX72N_PORTC_AVAILABLE  /* Full: PC0-PC7 */
#define RX72N_PORTD_AVAILABLE  /* Full: PD0-PD7 */
#define RX72N_PORTE_AVAILABLE  /* Full: PE0-PE7 */
#define RX72N_PORTF_AVAILABLE  /* Full: PF0-PF7 */
#define RX72N_PORTG_AVAILABLE  /* Full: PG0-PG7 */
#define RX72N_PORTJ_AVAILABLE  /* Full: PJ0-PJ7 */
/* Port H, K, L, M, N, Q NOT available */

#elif defined(RX72N_PACKAGE_224PIN)
/* 224-pin LFBGA - All ports available */
#define RX72N_PORT0_AVAILABLE  /* Full: P00-P07 */
#define RX72N_PORT1_AVAILABLE  /* Full: P10-P17 */
#define RX72N_PORT2_AVAILABLE  /* Full: P20-P27 */
#define RX72N_PORT3_AVAILABLE  /* Full: P30-P37 */
#define RX72N_PORT4_AVAILABLE  /* Full: P40-P47 */
#define RX72N_PORT5_AVAILABLE  /* Full: P50-P57 */
#define RX72N_PORT6_AVAILABLE  /* Full: P60-P67 */
#define RX72N_PORT7_AVAILABLE  /* Full: P70-P77 */
#define RX72N_PORT8_AVAILABLE  /* Full: P80-P87 */
#define RX72N_PORT9_AVAILABLE  /* Full: P90-P97 */
#define RX72N_PORTA_AVAILABLE  /* Full: PA0-PA7 */
#define RX72N_PORTB_AVAILABLE  /* Full: PB0-PB7 */
#define RX72N_PORTC_AVAILABLE  /* Full: PC0-PC7 */
#define RX72N_PORTD_AVAILABLE  /* Full: PD0-PD7 */
#define RX72N_PORTE_AVAILABLE  /* Full: PE0-PE7 */
#define RX72N_PORTF_AVAILABLE  /* Full: PF0-PF7 */
#define RX72N_PORTG_AVAILABLE  /* Full: PG0-PG7 */
#define RX72N_PORTH_AVAILABLE  /* Full: PH0-PH7 */
#define RX72N_PORTJ_AVAILABLE  /* Full: PJ0-PJ7 */
#define RX72N_PORTK_AVAILABLE  /* Full: PK0-PK7 */
#define RX72N_PORTL_AVAILABLE  /* Full: PL0-PL7 */
#define RX72N_PORTM_AVAILABLE  /* Full: PM0-PM7 */
#define RX72N_PORTN_AVAILABLE  /* Full: PN0-PN7 */
#define RX72N_PORTQ_AVAILABLE  /* Full: PQ0-PQ7 */

#endif

/* =============================================================================
 * Compile-Time Package Information
 * =============================================================================
 */

/**
 * @brief Get package name as string (for debug/logging)
 */
#if defined(RX72N_PACKAGE_100PIN)
#define RX72N_PACKAGE_NAME "100-pin LFQFP"
#define RX72N_IO_PIN_COUNT 63  /* 63 usable I/O pins (not counting power/clock) */
#elif defined(RX72N_PACKAGE_144PIN)
#define RX72N_PACKAGE_NAME "144-pin LFQFP"
#define RX72N_IO_PIN_COUNT 111
#elif defined(RX72N_PACKAGE_145PIN)
#define RX72N_PACKAGE_NAME "145-pin TFLGA"
#define RX72N_IO_PIN_COUNT 111
#elif defined(RX72N_PACKAGE_176PIN)
#define RX72N_PACKAGE_NAME "176-pin LFQFP/LFBGA"
#define RX72N_IO_PIN_COUNT 136
#elif defined(RX72N_PACKAGE_224PIN)
#define RX72N_PACKAGE_NAME "224-pin LFBGA"
#define RX72N_IO_PIN_COUNT 182
#endif

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PACKAGE_CONFIG_H */
