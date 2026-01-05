/* lib/rx_core/inc/rx_port_constants.h */

/**
 * @file rx_port_constants.h
 * @brief Centralized RX72N Port Number Definitions
 *
 * LIBRARY-LEVEL SINGLE SOURCE OF TRUTH for all port and pin numbers.
 * This is the ONLY file in the library layer that should contain hex values
 * for port/pin numbers.
 *
 * All library code must use these constants - never hardcode port numbers!
 *
 * Architecture:
 * - This file defines the port/pin constants (with hex values)
 * - lib/rx_hal/ uses these constants (NO hex in lib code)
 * - include/hardware_pinout.h uses these constants (NO hex in app code)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_PORT_CONSTANTS_H
#define STAR_RX_PORT_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================================================================
 * Port Number Constants
 * =============================================================================
 */

/**
 * @brief RX72N Port Numbers
 *
 * Used by HAL layer for hardware access.
 * Used by application layer for pin definitions.
 *
 * These match the Renesas RX72N port naming convention and the encoding
 * used in hardware_pinout.h gpio_pin_t enum where:
 *   gpio_pin_t = (port << 8) | pin
 *
 * Example library use:
 *   case k_rx_port_b: return portb();
 *
 * Example application use:
 *   k_gpio_pb2 = (k_rx_port_b << 8) | k_rx_pin_2
 */
typedef enum {
  /* Decimal Ports (0-9) */
  k_rx_port_0 = 0x00, /**< Port 0 */
  k_rx_port_1 = 0x01, /**< Port 1 */
  k_rx_port_2 = 0x02, /**< Port 2 */
  k_rx_port_3 = 0x03, /**< Port 3 */
  k_rx_port_4 = 0x04, /**< Port 4 */
  k_rx_port_5 = 0x05, /**< Port 5 */
  k_rx_port_6 = 0x06, /**< Port 6 */
  k_rx_port_7 = 0x07, /**< Port 7 */
  k_rx_port_8 = 0x08, /**< Port 8 */
  k_rx_port_9 = 0x09, /**< Port 9 */

  /* Hexadecimal Ports (A-G, J) */
  k_rx_port_a = 0x0A, /**< Port A */
  k_rx_port_b = 0x0B, /**< Port B */
  k_rx_port_c = 0x0C, /**< Port C */
  k_rx_port_d = 0x0D, /**< Port D */
  k_rx_port_e = 0x0E, /**< Port E */
  k_rx_port_f = 0x0F, /**< Port F */
  k_rx_port_g = 0x10, /**< Port G */
  k_rx_port_j = 0x13, /**< Port J (not contiguous - note gap after Port G!) */
} rx_port_number_t;

/* =============================================================================
 * Pin Number Constants
 * =============================================================================
 */

/**
 * @brief Pin number constants (0-7)
 *
 * All RX72N ports have pins numbered 0-7. Use these constants instead of
 * raw numbers for self-documenting code.
 *
 * Example:
 *   gpio_pin_t my_pin = (k_rx_port_b << 8) | k_rx_pin_2;  // PB2
 */
typedef enum {
  k_rx_pin_0 = 0, /**< Pin 0 */
  k_rx_pin_1 = 1, /**< Pin 1 */
  k_rx_pin_2 = 2, /**< Pin 2 */
  k_rx_pin_3 = 3, /**< Pin 3 */
  k_rx_pin_4 = 4, /**< Pin 4 */
  k_rx_pin_5 = 5, /**< Pin 5 */
  k_rx_pin_6 = 6, /**< Pin 6 */
  k_rx_pin_7 = 7, /**< Pin 7 */

  k_rx_pin_max = 7, /**< Maximum pin number (for validation) */
} rx_pin_number_t;

/* =============================================================================
 * Bit Shift Constants
 * =============================================================================
 */

/**
 * @brief Bit shift constants for port/pin encoding
 *
 * Used when constructing gpio_pin_t values from port and pin numbers.
 *
 * Example:
 *   gpio_pin_t pin = (port << k_port_shift) | pin_num;
 */
typedef enum {
  k_port_shift = 8, /**< Left shift amount to move port to upper byte */
} port_encoding_t;

/* =============================================================================
 * Pre-Computed Port/Pin Combinations
 * =============================================================================
 */

/**
 * @brief Pre-computed port/pin combinations for all RX72N pins
 *
 * These constants provide ready-to-use gpio_pin_t values for all possible
 * port/pin combinations on the RX72N (18 ports × 8 pins = 144 combinations).
 *
 * Benefits:
 * - Cleaner syntax: k_rx_pa_2 instead of (k_rx_port_a << k_port_shift) | k_rx_pin_2
 * - No repeated bit-shift operations
 * - Comprehensive coverage of all theoretical pins
 *
 * Note: Not all combinations are physically available on the 100-pin LFQFP package,
 * but all are defined for completeness and compatibility with other packages.
 *
 * Example:
 *   gpio_pin_t my_pin = k_rx_pa_2;  // Port A, Pin 2
 */
typedef enum {
  /* Port 0 (0x00) - Pins 0-7 */
  k_rx_p0_0 = (k_rx_port_0 << k_port_shift) | k_rx_pin_0, /**< P00 */
  k_rx_p0_1 = (k_rx_port_0 << k_port_shift) | k_rx_pin_1, /**< P01 */
  k_rx_p0_2 = (k_rx_port_0 << k_port_shift) | k_rx_pin_2, /**< P02 */
  k_rx_p0_3 = (k_rx_port_0 << k_port_shift) | k_rx_pin_3, /**< P03 */
  k_rx_p0_4 = (k_rx_port_0 << k_port_shift) | k_rx_pin_4, /**< P04 */
  k_rx_p0_5 = (k_rx_port_0 << k_port_shift) | k_rx_pin_5, /**< P05 */
  k_rx_p0_6 = (k_rx_port_0 << k_port_shift) | k_rx_pin_6, /**< P06 */
  k_rx_p0_7 = (k_rx_port_0 << k_port_shift) | k_rx_pin_7, /**< P07 */

  /* Port 1 (0x01) - Pins 0-7 */
  k_rx_p1_0 = (k_rx_port_1 << k_port_shift) | k_rx_pin_0, /**< P10 */
  k_rx_p1_1 = (k_rx_port_1 << k_port_shift) | k_rx_pin_1, /**< P11 */
  k_rx_p1_2 = (k_rx_port_1 << k_port_shift) | k_rx_pin_2, /**< P12 */
  k_rx_p1_3 = (k_rx_port_1 << k_port_shift) | k_rx_pin_3, /**< P13 */
  k_rx_p1_4 = (k_rx_port_1 << k_port_shift) | k_rx_pin_4, /**< P14 */
  k_rx_p1_5 = (k_rx_port_1 << k_port_shift) | k_rx_pin_5, /**< P15 */
  k_rx_p1_6 = (k_rx_port_1 << k_port_shift) | k_rx_pin_6, /**< P16 */
  k_rx_p1_7 = (k_rx_port_1 << k_port_shift) | k_rx_pin_7, /**< P17 */

  /* Port 2 (0x02) - Pins 0-7 */
  k_rx_p2_0 = (k_rx_port_2 << k_port_shift) | k_rx_pin_0, /**< P20 */
  k_rx_p2_1 = (k_rx_port_2 << k_port_shift) | k_rx_pin_1, /**< P21 */
  k_rx_p2_2 = (k_rx_port_2 << k_port_shift) | k_rx_pin_2, /**< P22 */
  k_rx_p2_3 = (k_rx_port_2 << k_port_shift) | k_rx_pin_3, /**< P23 */
  k_rx_p2_4 = (k_rx_port_2 << k_port_shift) | k_rx_pin_4, /**< P24 */
  k_rx_p2_5 = (k_rx_port_2 << k_port_shift) | k_rx_pin_5, /**< P25 */
  k_rx_p2_6 = (k_rx_port_2 << k_port_shift) | k_rx_pin_6, /**< P26 */
  k_rx_p2_7 = (k_rx_port_2 << k_port_shift) | k_rx_pin_7, /**< P27 */

  /* Port 3 (0x03) - Pins 0-7 */
  k_rx_p3_0 = (k_rx_port_3 << k_port_shift) | k_rx_pin_0, /**< P30 */
  k_rx_p3_1 = (k_rx_port_3 << k_port_shift) | k_rx_pin_1, /**< P31 */
  k_rx_p3_2 = (k_rx_port_3 << k_port_shift) | k_rx_pin_2, /**< P32 */
  k_rx_p3_3 = (k_rx_port_3 << k_port_shift) | k_rx_pin_3, /**< P33 */
  k_rx_p3_4 = (k_rx_port_3 << k_port_shift) | k_rx_pin_4, /**< P34 */
  k_rx_p3_5 = (k_rx_port_3 << k_port_shift) | k_rx_pin_5, /**< P35 */
  k_rx_p3_6 = (k_rx_port_3 << k_port_shift) | k_rx_pin_6, /**< P36 */
  k_rx_p3_7 = (k_rx_port_3 << k_port_shift) | k_rx_pin_7, /**< P37 */

  /* Port 4 (0x04) - Pins 0-7 */
  k_rx_p4_0 = (k_rx_port_4 << k_port_shift) | k_rx_pin_0, /**< P40 */
  k_rx_p4_1 = (k_rx_port_4 << k_port_shift) | k_rx_pin_1, /**< P41 */
  k_rx_p4_2 = (k_rx_port_4 << k_port_shift) | k_rx_pin_2, /**< P42 */
  k_rx_p4_3 = (k_rx_port_4 << k_port_shift) | k_rx_pin_3, /**< P43 */
  k_rx_p4_4 = (k_rx_port_4 << k_port_shift) | k_rx_pin_4, /**< P44 */
  k_rx_p4_5 = (k_rx_port_4 << k_port_shift) | k_rx_pin_5, /**< P45 */
  k_rx_p4_6 = (k_rx_port_4 << k_port_shift) | k_rx_pin_6, /**< P46 */
  k_rx_p4_7 = (k_rx_port_4 << k_port_shift) | k_rx_pin_7, /**< P47 */

  /* Port 5 (0x05) - Pins 0-7 */
  k_rx_p5_0 = (k_rx_port_5 << k_port_shift) | k_rx_pin_0, /**< P50 */
  k_rx_p5_1 = (k_rx_port_5 << k_port_shift) | k_rx_pin_1, /**< P51 */
  k_rx_p5_2 = (k_rx_port_5 << k_port_shift) | k_rx_pin_2, /**< P52 */
  k_rx_p5_3 = (k_rx_port_5 << k_port_shift) | k_rx_pin_3, /**< P53 */
  k_rx_p5_4 = (k_rx_port_5 << k_port_shift) | k_rx_pin_4, /**< P54 */
  k_rx_p5_5 = (k_rx_port_5 << k_port_shift) | k_rx_pin_5, /**< P55 */
  k_rx_p5_6 = (k_rx_port_5 << k_port_shift) | k_rx_pin_6, /**< P56 */
  k_rx_p5_7 = (k_rx_port_5 << k_port_shift) | k_rx_pin_7, /**< P57 */

  /* Port 6 (0x06) - Pins 0-7 */
  k_rx_p6_0 = (k_rx_port_6 << k_port_shift) | k_rx_pin_0, /**< P60 */
  k_rx_p6_1 = (k_rx_port_6 << k_port_shift) | k_rx_pin_1, /**< P61 */
  k_rx_p6_2 = (k_rx_port_6 << k_port_shift) | k_rx_pin_2, /**< P62 */
  k_rx_p6_3 = (k_rx_port_6 << k_port_shift) | k_rx_pin_3, /**< P63 */
  k_rx_p6_4 = (k_rx_port_6 << k_port_shift) | k_rx_pin_4, /**< P64 */
  k_rx_p6_5 = (k_rx_port_6 << k_port_shift) | k_rx_pin_5, /**< P65 */
  k_rx_p6_6 = (k_rx_port_6 << k_port_shift) | k_rx_pin_6, /**< P66 */
  k_rx_p6_7 = (k_rx_port_6 << k_port_shift) | k_rx_pin_7, /**< P67 */

  /* Port 7 (0x07) - Pins 0-7 */
  k_rx_p7_0 = (k_rx_port_7 << k_port_shift) | k_rx_pin_0, /**< P70 */
  k_rx_p7_1 = (k_rx_port_7 << k_port_shift) | k_rx_pin_1, /**< P71 */
  k_rx_p7_2 = (k_rx_port_7 << k_port_shift) | k_rx_pin_2, /**< P72 */
  k_rx_p7_3 = (k_rx_port_7 << k_port_shift) | k_rx_pin_3, /**< P73 */
  k_rx_p7_4 = (k_rx_port_7 << k_port_shift) | k_rx_pin_4, /**< P74 */
  k_rx_p7_5 = (k_rx_port_7 << k_port_shift) | k_rx_pin_5, /**< P75 */
  k_rx_p7_6 = (k_rx_port_7 << k_port_shift) | k_rx_pin_6, /**< P76 */
  k_rx_p7_7 = (k_rx_port_7 << k_port_shift) | k_rx_pin_7, /**< P77 */

  /* Port 8 (0x08) - Pins 0-7 */
  k_rx_p8_0 = (k_rx_port_8 << k_port_shift) | k_rx_pin_0, /**< P80 */
  k_rx_p8_1 = (k_rx_port_8 << k_port_shift) | k_rx_pin_1, /**< P81 */
  k_rx_p8_2 = (k_rx_port_8 << k_port_shift) | k_rx_pin_2, /**< P82 */
  k_rx_p8_3 = (k_rx_port_8 << k_port_shift) | k_rx_pin_3, /**< P83 */
  k_rx_p8_4 = (k_rx_port_8 << k_port_shift) | k_rx_pin_4, /**< P84 */
  k_rx_p8_5 = (k_rx_port_8 << k_port_shift) | k_rx_pin_5, /**< P85 */
  k_rx_p8_6 = (k_rx_port_8 << k_port_shift) | k_rx_pin_6, /**< P86 */
  k_rx_p8_7 = (k_rx_port_8 << k_port_shift) | k_rx_pin_7, /**< P87 */

  /* Port 9 (0x09) - Pins 0-7 */
  k_rx_p9_0 = (k_rx_port_9 << k_port_shift) | k_rx_pin_0, /**< P90 */
  k_rx_p9_1 = (k_rx_port_9 << k_port_shift) | k_rx_pin_1, /**< P91 */
  k_rx_p9_2 = (k_rx_port_9 << k_port_shift) | k_rx_pin_2, /**< P92 */
  k_rx_p9_3 = (k_rx_port_9 << k_port_shift) | k_rx_pin_3, /**< P93 */
  k_rx_p9_4 = (k_rx_port_9 << k_port_shift) | k_rx_pin_4, /**< P94 */
  k_rx_p9_5 = (k_rx_port_9 << k_port_shift) | k_rx_pin_5, /**< P95 */
  k_rx_p9_6 = (k_rx_port_9 << k_port_shift) | k_rx_pin_6, /**< P96 */
  k_rx_p9_7 = (k_rx_port_9 << k_port_shift) | k_rx_pin_7, /**< P97 */

  /* Port A (0x0A) - Pins 0-7 */
  k_rx_pa_0 = (k_rx_port_a << k_port_shift) | k_rx_pin_0, /**< PA0 */
  k_rx_pa_1 = (k_rx_port_a << k_port_shift) | k_rx_pin_1, /**< PA1 */
  k_rx_pa_2 = (k_rx_port_a << k_port_shift) | k_rx_pin_2, /**< PA2 */
  k_rx_pa_3 = (k_rx_port_a << k_port_shift) | k_rx_pin_3, /**< PA3 */
  k_rx_pa_4 = (k_rx_port_a << k_port_shift) | k_rx_pin_4, /**< PA4 */
  k_rx_pa_5 = (k_rx_port_a << k_port_shift) | k_rx_pin_5, /**< PA5 */
  k_rx_pa_6 = (k_rx_port_a << k_port_shift) | k_rx_pin_6, /**< PA6 */
  k_rx_pa_7 = (k_rx_port_a << k_port_shift) | k_rx_pin_7, /**< PA7 */

  /* Port B (0x0B) - Pins 0-7 */
  k_rx_pb_0 = (k_rx_port_b << k_port_shift) | k_rx_pin_0, /**< PB0 */
  k_rx_pb_1 = (k_rx_port_b << k_port_shift) | k_rx_pin_1, /**< PB1 */
  k_rx_pb_2 = (k_rx_port_b << k_port_shift) | k_rx_pin_2, /**< PB2 */
  k_rx_pb_3 = (k_rx_port_b << k_port_shift) | k_rx_pin_3, /**< PB3 */
  k_rx_pb_4 = (k_rx_port_b << k_port_shift) | k_rx_pin_4, /**< PB4 */
  k_rx_pb_5 = (k_rx_port_b << k_port_shift) | k_rx_pin_5, /**< PB5 */
  k_rx_pb_6 = (k_rx_port_b << k_port_shift) | k_rx_pin_6, /**< PB6 */
  k_rx_pb_7 = (k_rx_port_b << k_port_shift) | k_rx_pin_7, /**< PB7 */

  /* Port C (0x0C) - Pins 0-7 */
  k_rx_pc_0 = (k_rx_port_c << k_port_shift) | k_rx_pin_0, /**< PC0 */
  k_rx_pc_1 = (k_rx_port_c << k_port_shift) | k_rx_pin_1, /**< PC1 */
  k_rx_pc_2 = (k_rx_port_c << k_port_shift) | k_rx_pin_2, /**< PC2 */
  k_rx_pc_3 = (k_rx_port_c << k_port_shift) | k_rx_pin_3, /**< PC3 */
  k_rx_pc_4 = (k_rx_port_c << k_port_shift) | k_rx_pin_4, /**< PC4 */
  k_rx_pc_5 = (k_rx_port_c << k_port_shift) | k_rx_pin_5, /**< PC5 */
  k_rx_pc_6 = (k_rx_port_c << k_port_shift) | k_rx_pin_6, /**< PC6 */
  k_rx_pc_7 = (k_rx_port_c << k_port_shift) | k_rx_pin_7, /**< PC7 */

  /* Port D (0x0D) - Pins 0-7 */
  k_rx_pd_0 = (k_rx_port_d << k_port_shift) | k_rx_pin_0, /**< PD0 */
  k_rx_pd_1 = (k_rx_port_d << k_port_shift) | k_rx_pin_1, /**< PD1 */
  k_rx_pd_2 = (k_rx_port_d << k_port_shift) | k_rx_pin_2, /**< PD2 */
  k_rx_pd_3 = (k_rx_port_d << k_port_shift) | k_rx_pin_3, /**< PD3 */
  k_rx_pd_4 = (k_rx_port_d << k_port_shift) | k_rx_pin_4, /**< PD4 */
  k_rx_pd_5 = (k_rx_port_d << k_port_shift) | k_rx_pin_5, /**< PD5 */
  k_rx_pd_6 = (k_rx_port_d << k_port_shift) | k_rx_pin_6, /**< PD6 */
  k_rx_pd_7 = (k_rx_port_d << k_port_shift) | k_rx_pin_7, /**< PD7 */

  /* Port E (0x0E) - Pins 0-7 */
  k_rx_pe_0 = (k_rx_port_e << k_port_shift) | k_rx_pin_0, /**< PE0 */
  k_rx_pe_1 = (k_rx_port_e << k_port_shift) | k_rx_pin_1, /**< PE1 */
  k_rx_pe_2 = (k_rx_port_e << k_port_shift) | k_rx_pin_2, /**< PE2 */
  k_rx_pe_3 = (k_rx_port_e << k_port_shift) | k_rx_pin_3, /**< PE3 */
  k_rx_pe_4 = (k_rx_port_e << k_port_shift) | k_rx_pin_4, /**< PE4 */
  k_rx_pe_5 = (k_rx_port_e << k_port_shift) | k_rx_pin_5, /**< PE5 */
  k_rx_pe_6 = (k_rx_port_e << k_port_shift) | k_rx_pin_6, /**< PE6 */
  k_rx_pe_7 = (k_rx_port_e << k_port_shift) | k_rx_pin_7, /**< PE7 */

  /* Port F (0x0F) - Pins 0-7 */
  k_rx_pf_0 = (k_rx_port_f << k_port_shift) | k_rx_pin_0, /**< PF0 */
  k_rx_pf_1 = (k_rx_port_f << k_port_shift) | k_rx_pin_1, /**< PF1 */
  k_rx_pf_2 = (k_rx_port_f << k_port_shift) | k_rx_pin_2, /**< PF2 */
  k_rx_pf_3 = (k_rx_port_f << k_port_shift) | k_rx_pin_3, /**< PF3 */
  k_rx_pf_4 = (k_rx_port_f << k_port_shift) | k_rx_pin_4, /**< PF4 */
  k_rx_pf_5 = (k_rx_port_f << k_port_shift) | k_rx_pin_5, /**< PF5 */
  k_rx_pf_6 = (k_rx_port_f << k_port_shift) | k_rx_pin_6, /**< PF6 */
  k_rx_pf_7 = (k_rx_port_f << k_port_shift) | k_rx_pin_7, /**< PF7 */

  /* Port G (0x10) - Pins 0-7 */
  k_rx_pg_0 = (k_rx_port_g << k_port_shift) | k_rx_pin_0, /**< PG0 */
  k_rx_pg_1 = (k_rx_port_g << k_port_shift) | k_rx_pin_1, /**< PG1 */
  k_rx_pg_2 = (k_rx_port_g << k_port_shift) | k_rx_pin_2, /**< PG2 */
  k_rx_pg_3 = (k_rx_port_g << k_port_shift) | k_rx_pin_3, /**< PG3 */
  k_rx_pg_4 = (k_rx_port_g << k_port_shift) | k_rx_pin_4, /**< PG4 */
  k_rx_pg_5 = (k_rx_port_g << k_port_shift) | k_rx_pin_5, /**< PG5 */
  k_rx_pg_6 = (k_rx_port_g << k_port_shift) | k_rx_pin_6, /**< PG6 */
  k_rx_pg_7 = (k_rx_port_g << k_port_shift) | k_rx_pin_7, /**< PG7 */

  /* Port J (0x13) - Pins 0-7 */
  k_rx_pj_0 = (k_rx_port_j << k_port_shift) | k_rx_pin_0, /**< PJ0 */
  k_rx_pj_1 = (k_rx_port_j << k_port_shift) | k_rx_pin_1, /**< PJ1 */
  k_rx_pj_2 = (k_rx_port_j << k_port_shift) | k_rx_pin_2, /**< PJ2 */
  k_rx_pj_3 = (k_rx_port_j << k_port_shift) | k_rx_pin_3, /**< PJ3 */
  k_rx_pj_4 = (k_rx_port_j << k_port_shift) | k_rx_pin_4, /**< PJ4 */
  k_rx_pj_5 = (k_rx_port_j << k_port_shift) | k_rx_pin_5, /**< PJ5 */
  k_rx_pj_6 = (k_rx_port_j << k_port_shift) | k_rx_pin_6, /**< PJ6 */
  k_rx_pj_7 = (k_rx_port_j << k_port_shift) | k_rx_pin_7, /**< PJ7 */
} rx_port_pin_t;

/* =============================================================================
 * Compile-Time Verification (Static Assertions)
 * =============================================================================
 */

/*
 * Verify port number constants match expected hex values.
 * These assertions ensure that the port numbering matches Renesas RX72N
 * documentation and the encoding used in hardware_pinout.h.
 */
_Static_assert(k_rx_port_0 == 0x00, "Port 0 must be 0x00");
_Static_assert(k_rx_port_1 == 0x01, "Port 1 must be 0x01");
_Static_assert(k_rx_port_5 == 0x05, "Port 5 must be 0x05");
_Static_assert(k_rx_port_a == 0x0A, "Port A must be 0x0A");
_Static_assert(k_rx_port_b == 0x0B, "Port B must be 0x0B");
_Static_assert(k_rx_port_c == 0x0C, "Port C must be 0x0C");
_Static_assert(k_rx_port_e == 0x0E, "Port E must be 0x0E");
_Static_assert(k_rx_port_j == 0x13, "Port J must be 0x13 (not contiguous)");

/*
 * Verify pin number constants match expected values.
 */
_Static_assert(k_rx_pin_0 == 0, "Pin 0 must be 0");
_Static_assert(k_rx_pin_2 == 2, "Pin 2 must be 2");
_Static_assert(k_rx_pin_7 == 7, "Pin 7 must be 7");
_Static_assert(k_rx_pin_max == 7, "Maximum pin number must be 7");

/*
 * Verify shift constant is correct.
 */
_Static_assert(k_port_shift == 8, "Port shift must be 8 bits");

/*
 * Verify encoding matches expected pattern: (port << 8) | pin
 * Example: Port B (0x0B), Pin 2 (0x02) = 0x0B02
 */
_Static_assert(((k_rx_port_b << k_port_shift) | k_rx_pin_2) == 0x0B02,
               "Port B Pin 2 encoding must be 0x0B02");
_Static_assert(((k_rx_port_e << k_port_shift) | k_rx_pin_5) == 0x0E05,
               "Port E Pin 5 encoding must be 0x0E05");
_Static_assert(((k_rx_port_0 << k_port_shift) | k_rx_pin_7) == 0x0007,
               "Port 0 Pin 7 encoding must be 0x0007");

/*
 * Verify pre-computed port/pin combinations are correct.
 */
_Static_assert(k_rx_p0_4 == 0x0004, "P04 must be 0x0004");
_Static_assert(k_rx_p2_5 == 0x0205, "P25 must be 0x0205");
_Static_assert(k_rx_pa_2 == 0x0A02, "PA2 must be 0x0A02");
_Static_assert(k_rx_pb_2 == 0x0B02, "PB2 must be 0x0B02");
_Static_assert(k_rx_pe_5 == 0x0E05, "PE5 must be 0x0E05");
_Static_assert(k_rx_pj_3 == 0x1303, "PJ3 must be 0x1303");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PORT_CONSTANTS_H */
