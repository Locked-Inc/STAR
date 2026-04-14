/**
 * @file rx72n_elc_regs.h
 * @brief RX72N Event Link Controller (ELC) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N Event Link Controller.
 * The ELC uses interrupt requests from peripheral modules as event signals
 * to interconnect peripherals without CPU intervention.
 *
 * @par ELC Overview
 * - 135 types of event signals can be directly interconnected
 * - Timer event input control for MTU, CMT, TMR, TPU, CMTW, GPTW
 * - Port event I/O control for Port B and Port E
 * - Software event generation capability
 *
 * @par Register Organization
 * | Address       | Size | Description                           |
 * |---------------|------|---------------------------------------|
 * | 0x0008B100    | 8    | ELCR - Event Link Control             |
 * | 0x0008B101    | 8    | ELSR0 - MTU0 event link               |
 * | 0x0008B104    | 8    | ELSR3 - MTU3 event link               |
 * | ...           | ...  | (various ELSRn registers)             |
 * | 0x0008B11F    | 8    | ELOPA - MTU0/3 option setting         |
 * | 0x0008B120    | 8    | ELOPB - MTU4 option setting           |
 * | 0x0008B121    | 8    | ELOPC - CMT1 option setting           |
 * | 0x0008B122    | 8    | ELOPD - TMR0-3 option setting         |
 * | 0x0008B123-4  | 8    | PGR1/2 - Port group registers         |
 * | 0x0008B125-6  | 8    | PGC1/2 - Port group control           |
 * | 0x0008B127-8  | 8    | PDBF1/2 - Port data buffers           |
 * | 0x0008B129-C  | 8    | PEL0-3 - Port event link              |
 * | 0x0008B12D    | 8    | ELSEGR - Software event generation    |
 * | 0x0008B13F    | 8    | ELOPF - TPU0-3 option setting         |
 * | 0x0008B141    | 8    | ELOPH - CMTW0 option setting          |
 * | 0x0008B146-4F | 8    | ELSR48-57 - GPTW event links          |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: N/A (no loops)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: Short accessor functions
 * - Rule 5: Static assertions verify addresses
 * - Rule 6: Minimal scope
 * - Rule 7: N/A
 * - Rule 8: All constants use C23 typed enums
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 21: Event Link Controller (ELC), pages 835-860
 * - Section 21.2: Register Descriptions
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ELC Base Address and Register Addresses
 * =============================================================================
 */

/**
 * @enum elc_addresses_t
 * @brief ELC register addresses
 *
 * @details
 * All addresses verified against RX72N Hardware Manual Ch21.
 *
 * @note Addresses verified 2026-01-29 against manual section 21.2
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief ELC module base address */
  k_elc_base_addr = 0x0008B100U,

  /** @brief ELCR - Event Link Control Register */
  k_elc_elcr_addr = 0x0008B100U,

  /* ELSRn - Event Link Setting Registers (n = peripheral specific) */
  k_elc_elsr0_addr  = 0x0008B101U, /**< MTU0 */
  k_elc_elsr3_addr  = 0x0008B104U, /**< MTU3 */
  k_elc_elsr4_addr  = 0x0008B105U, /**< MTU4 */
  k_elc_elsr7_addr  = 0x0008B108U, /**< CMT1 */
  k_elc_elsr10_addr = 0x0008B10BU, /**< TMR0 */
  k_elc_elsr11_addr = 0x0008B10CU, /**< TMR1 */
  k_elc_elsr12_addr = 0x0008B10DU, /**< TMR2 */
  k_elc_elsr13_addr = 0x0008B10EU, /**< TMR3 */
  k_elc_elsr15_addr = 0x0008B110U, /**< S12AD (ELCTRG00N) */
  k_elc_elsr16_addr = 0x0008B111U, /**< DA0 */
  k_elc_elsr18_addr = 0x0008B113U, /**< ICU Interrupt 1 */
  k_elc_elsr19_addr = 0x0008B114U, /**< ICU Interrupt 2 */
  k_elc_elsr20_addr = 0x0008B115U, /**< Output port group 1 */
  k_elc_elsr21_addr = 0x0008B116U, /**< Output port group 2 */
  k_elc_elsr22_addr = 0x0008B117U, /**< Input port group 1 */
  k_elc_elsr23_addr = 0x0008B118U, /**< Input port group 2 */
  k_elc_elsr24_addr = 0x0008B119U, /**< Single port 0 */
  k_elc_elsr25_addr = 0x0008B11AU, /**< Single port 1 */
  k_elc_elsr26_addr = 0x0008B11BU, /**< Single port 2 */
  k_elc_elsr27_addr = 0x0008B11CU, /**< Single port 3 */
  k_elc_elsr28_addr = 0x0008B11DU, /**< Clock source switch to LOCO */
  k_elc_elsr33_addr = 0x0008B131U, /**< CMTW0 */
  k_elc_elsr35_addr = 0x0008B133U, /**< TPU0 */
  k_elc_elsr36_addr = 0x0008B134U, /**< TPU1 */
  k_elc_elsr37_addr = 0x0008B135U, /**< TPU2 */
  k_elc_elsr38_addr = 0x0008B136U, /**< TPU3 */
  k_elc_elsr45_addr = 0x0008B13DU, /**< S12AD1 (ELCTRG10N) */
  k_elc_elsr48_addr = 0x0008B146U, /**< GPTW event source A */
  k_elc_elsr49_addr = 0x0008B147U, /**< GPTW event source B */
  k_elc_elsr50_addr = 0x0008B148U, /**< GPTW event source C */
  k_elc_elsr51_addr = 0x0008B149U, /**< GPTW event source D */
  k_elc_elsr52_addr = 0x0008B14AU, /**< GPTW event source E */
  k_elc_elsr53_addr = 0x0008B14BU, /**< GPTW event source F */
  k_elc_elsr54_addr = 0x0008B14CU, /**< GPTW event source G */
  k_elc_elsr55_addr = 0x0008B14DU, /**< GPTW event source H */
  k_elc_elsr56_addr = 0x0008B14EU, /**< S12AD (ELCTRG01N) */
  k_elc_elsr57_addr = 0x0008B14FU, /**< S12AD1 (ELCTRG11N) */

  /* Option Setting Registers */
  k_elc_elopa_addr = 0x0008B11FU, /**< MTU0/MTU3 option setting */
  k_elc_elopb_addr = 0x0008B120U, /**< MTU4 option setting */
  k_elc_elopc_addr = 0x0008B121U, /**< CMT1 option setting */
  k_elc_elopd_addr = 0x0008B122U, /**< TMR0-3 option setting */
  k_elc_elopf_addr = 0x0008B13FU, /**< TPU0-3 option setting */
  k_elc_eloph_addr = 0x0008B141U, /**< CMTW0 option setting */

  /* Port Event Registers */
  k_elc_pgr1_addr  = 0x0008B123U, /**< Port Group Register 1 */
  k_elc_pgr2_addr  = 0x0008B124U, /**< Port Group Register 2 */
  k_elc_pgc1_addr  = 0x0008B125U, /**< Port Group Control 1 */
  k_elc_pgc2_addr  = 0x0008B126U, /**< Port Group Control 2 */
  k_elc_pdbf1_addr = 0x0008B127U, /**< Port Data Buffer 1 */
  k_elc_pdbf2_addr = 0x0008B128U, /**< Port Data Buffer 2 */
  k_elc_pel0_addr  = 0x0008B129U, /**< Port Event Link 0 */
  k_elc_pel1_addr  = 0x0008B12AU, /**< Port Event Link 1 */
  k_elc_pel2_addr  = 0x0008B12BU, /**< Port Event Link 2 */
  k_elc_pel3_addr  = 0x0008B12CU, /**< Port Event Link 3 */

  /* Software Event Generation */
  k_elc_elsegr_addr = 0x0008B12DU, /**< Software Event Generation */
} elc_addresses_t;

/* =============================================================================
 * ELCR Register Bits (Event Link Control)
 * =============================================================================
 */

/**
 * @enum elcr_bits_t
 * @brief Event Link Control Register (ELCR) bit definitions
 *
 * @par Register Layout (8-bit @ 0x0008B100)
 * | Bit | Field | Description                 |
 * |-----|-------|-----------------------------|
 * | 6:0 | -     | Reserved (write 1)          |
 * | 7   | ELCON | All Event Link Enable       |
 *
 * @note Manual ref: Ch21 section 21.2.1
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief Reserved bits mask (write 1) */
  k_elcr_reserved = 0x7FU,

  /** @brief All Event Link Enable (0=disabled, 1=enabled) */
  k_elcr_elcon = 0x80U,
} elcr_bits_t;

/* =============================================================================
 * ELSRn Event Signal Values
 * =============================================================================
 */

/**
 * @enum elsr_event_t
 * @brief Event signal values for ELSRn registers
 *
 * @details
 * Values to set in ELSRn.ELS[7:0] to select the event signal source.
 * Only a subset of commonly-used events are defined here.
 *
 * @note Manual ref: Ch21 Table 21.3
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elsr_event_disabled = 0x00U, /**< Event output disabled */

  /* MTU0 events (0x01-0x07) */
  k_elsr_mtu0_cmpa = 0x01U, /**< MTU0 compare match A */
  k_elsr_mtu0_cmpb = 0x02U, /**< MTU0 compare match B */
  k_elsr_mtu0_cmpc = 0x03U, /**< MTU0 compare match C */
  k_elsr_mtu0_cmpd = 0x04U, /**< MTU0 compare match D */
  k_elsr_mtu0_cmpe = 0x05U, /**< MTU0 compare match E */
  k_elsr_mtu0_cmpf = 0x06U, /**< MTU0 compare match F */
  k_elsr_mtu0_ovf  = 0x07U, /**< MTU0 overflow */

  /* MTU3 events (0x10-0x14) */
  k_elsr_mtu3_cmpa = 0x10U, /**< MTU3 compare match A */
  k_elsr_mtu3_cmpb = 0x11U, /**< MTU3 compare match B */
  k_elsr_mtu3_cmpc = 0x12U, /**< MTU3 compare match C */
  k_elsr_mtu3_cmpd = 0x13U, /**< MTU3 compare match D */
  k_elsr_mtu3_ovf  = 0x14U, /**< MTU3 overflow */

  /* MTU4 events (0x15-0x1A) */
  k_elsr_mtu4_cmpa = 0x15U, /**< MTU4 compare match A */
  k_elsr_mtu4_cmpb = 0x16U, /**< MTU4 compare match B */
  k_elsr_mtu4_cmpc = 0x17U, /**< MTU4 compare match C */
  k_elsr_mtu4_cmpd = 0x18U, /**< MTU4 compare match D */
  k_elsr_mtu4_ovf  = 0x19U, /**< MTU4 overflow */
  k_elsr_mtu4_udf  = 0x1AU, /**< MTU4 underflow */

  /* CMT1 event (0x1F) */
  k_elsr_cmt1_cmp = 0x1FU, /**< CMT1 compare match */

  /* TMR events (0x22-0x2D) */
  k_elsr_tmr0_cmpa = 0x22U, /**< TMR0 compare match A */
  k_elsr_tmr0_cmpb = 0x23U, /**< TMR0 compare match B */
  k_elsr_tmr0_ovf  = 0x24U, /**< TMR0 overflow */

  /* S12AD events (0x58, 0x6C) */
  k_elsr_s12ad0_end = 0x58U, /**< S12AD A/D conversion end */
  k_elsr_s12ad1_end = 0x6CU, /**< S12AD1 A/D conversion end */

  /* DMAC transfer end events (0x5D-0x60) */
  k_elsr_dmac0_end = 0x5DU, /**< DMAC0 transfer end */
  k_elsr_dmac1_end = 0x5EU, /**< DMAC1 transfer end */
  k_elsr_dmac2_end = 0x5FU, /**< DMAC2 transfer end */
  k_elsr_dmac3_end = 0x60U, /**< DMAC3 transfer end */

  /* DTC transfer end (0x61) */
  k_elsr_dtc_end = 0x61U, /**< DTC transfer end */

  /* Port events (0x63-0x68) */
  k_elsr_port_grp1    = 0x63U, /**< Input port group 1 edge */
  k_elsr_port_grp2    = 0x64U, /**< Input port group 2 edge */
  k_elsr_port_single0 = 0x65U, /**< Single port 0 edge */
  k_elsr_port_single1 = 0x66U, /**< Single port 1 edge */
  k_elsr_port_single2 = 0x67U, /**< Single port 2 edge */
  k_elsr_port_single3 = 0x68U, /**< Single port 3 edge */

  /* Software event (0x69) */
  k_elsr_software = 0x69U, /**< Software event */

  /* GPTW0 events (0x80-0x87) */
  k_elsr_gptw0_cmpa = 0x80U, /**< GPTW0 compare match A */
  k_elsr_gptw0_cmpb = 0x81U, /**< GPTW0 compare match B */
  k_elsr_gptw0_cmpc = 0x82U, /**< GPTW0 compare match C */
  k_elsr_gptw0_cmpd = 0x83U, /**< GPTW0 compare match D */
  k_elsr_gptw0_cmpe = 0x84U, /**< GPTW0 compare match E */
  k_elsr_gptw0_cmpf = 0x85U, /**< GPTW0 compare match F */
  k_elsr_gptw0_ovf  = 0x86U, /**< GPTW0 overflow */
  k_elsr_gptw0_udf  = 0x87U, /**< GPTW0 underflow */

  /* GPTW1 events (0x88-0x8F) */
  k_elsr_gptw1_cmpa = 0x88U, /**< GPTW1 compare match A */
  k_elsr_gptw1_cmpb = 0x89U, /**< GPTW1 compare match B */
  k_elsr_gptw1_cmpc = 0x8AU, /**< GPTW1 compare match C */
  k_elsr_gptw1_cmpd = 0x8BU, /**< GPTW1 compare match D */
  k_elsr_gptw1_cmpe = 0x8CU, /**< GPTW1 compare match E */
  k_elsr_gptw1_cmpf = 0x8DU, /**< GPTW1 compare match F */
  k_elsr_gptw1_ovf  = 0x8EU, /**< GPTW1 overflow */
  k_elsr_gptw1_udf  = 0x8FU, /**< GPTW1 underflow */

  /* GPTW2 events (0x90-0x97) */
  k_elsr_gptw2_cmpa = 0x90U, /**< GPTW2 compare match A */
  k_elsr_gptw2_cmpb = 0x91U, /**< GPTW2 compare match B */
  k_elsr_gptw2_cmpc = 0x92U, /**< GPTW2 compare match C */
  k_elsr_gptw2_cmpd = 0x93U, /**< GPTW2 compare match D */
  k_elsr_gptw2_cmpe = 0x94U, /**< GPTW2 compare match E */
  k_elsr_gptw2_cmpf = 0x95U, /**< GPTW2 compare match F */
  k_elsr_gptw2_ovf  = 0x96U, /**< GPTW2 overflow */
  k_elsr_gptw2_udf  = 0x97U, /**< GPTW2 underflow */

  /* GPTW3 events (0x98-0x9F) */
  k_elsr_gptw3_cmpa = 0x98U, /**< GPTW3 compare match A */
  k_elsr_gptw3_cmpb = 0x99U, /**< GPTW3 compare match B */
  k_elsr_gptw3_cmpc = 0x9AU, /**< GPTW3 compare match C */
  k_elsr_gptw3_cmpd = 0x9BU, /**< GPTW3 compare match D */
  k_elsr_gptw3_cmpe = 0x9CU, /**< GPTW3 compare match E */
  k_elsr_gptw3_cmpf = 0x9DU, /**< GPTW3 compare match F */
  k_elsr_gptw3_ovf  = 0x9EU, /**< GPTW3 overflow */
  k_elsr_gptw3_udf  = 0x9FU, /**< GPTW3 underflow */
} elsr_event_t;

/* =============================================================================
 * Timer Option Setting Bits
 * =============================================================================
 */

/**
 * @enum elop_timer_op_t
 * @brief Timer operation selection bits for ELOPA/B/C/D/F/H registers
 *
 * @details
 * Each timer can be configured to perform different operations
 * when an event signal is received. These 2-bit fields are used
 * in the ELOPx registers.
 *
 * For MTU/TPU timers (ELOPA, ELOPF): 0x02 = Input capture (TCNT -> TGRA).
 * For CMT/TMR/CMTW timers (ELOPC, ELOPD, ELOPH): 0x02 = Event counter.
 *
 * @note Manual ref: Ch21 sections 21.2.3-21.2.8, Table 21.5
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elop_timer_count_start   = 0x00U, /**< Counting is started */
  k_elop_timer_count_restart = 0x01U, /**< Counting is restarted */
  k_elop_timer_input_capture = 0x02U, /**< Input capture (MTU/TPU) or event counter (CMT/TMR/CMTW) */
  k_elop_timer_disabled      = 0x03U, /**< Event output is disabled */
} elop_timer_op_t;

/**
 * @enum elopa_bits_t
 * @brief ELOPA register bit positions (MTU0/MTU3)
 *
 * @par Register Layout (8-bit @ 0x0008B11F)
 * | Bits | Field     | Description                 |
 * |------|-----------|-----------------------------|
 * | 1:0  | MTU0MD    | MTU0 operation setting      |
 * | 5:2  | -         | Reserved                    |
 * | 7:6  | MTU3MD    | MTU3 operation setting      |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elopa_mtu0op_shift = 0U,    /**< MTU0 operation field shift (bits 1:0) */
  k_elopa_mtu0op_mask  = 0x03U, /**< MTU0 operation field mask */
  k_elopa_mtu3op_shift = 6U,    /**< MTU3 operation field shift (bits 7:6) */
  k_elopa_mtu3op_mask  = 0xC0U, /**< MTU3 operation field mask */
} elopa_bits_t;

/**
 * @enum elopb_bits_t
 * @brief ELOPB register bit positions (MTU4)
 *
 * @par Register Layout (8-bit @ 0x0008B120)
 * | Bits | Field    | Description                 |
 * |------|----------|-----------------------------|
 * | 1:0  | MTU4OP   | MTU4 operation setting      |
 * | 7:2  | -        | Reserved                    |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elopb_mtu4op_shift = 0U,    /**< MTU4 operation field shift */
  k_elopb_mtu4op_mask  = 0x03U, /**< MTU4 operation field mask */
} elopb_bits_t;

/**
 * @enum elopc_bits_t
 * @brief ELOPC register bit positions (CMT1)
 *
 * @par Register Layout (8-bit @ 0x0008B121)
 * | Bits | Field    | Description                 |
 * |------|----------|-----------------------------|
 * | 1:0  | -        | Reserved                    |
 * | 3:2  | CMT1MD   | CMT1 operation setting      |
 * | 7:4  | -        | Reserved                    |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elopc_cmt1op_shift = 2U,    /**< CMT1 operation field shift (bits 3:2) */
  k_elopc_cmt1op_mask  = 0x0CU, /**< CMT1 operation field mask */
} elopc_bits_t;

/**
 * @enum elopd_bits_t
 * @brief ELOPD register bit positions (TMR0-3)
 *
 * @par Register Layout (8-bit @ 0x0008B122)
 * | Bits | Field    | Description                 |
 * |------|----------|-----------------------------|
 * | 1:0  | TMR0OP   | TMR0 operation setting      |
 * | 3:2  | TMR1OP   | TMR1 operation setting      |
 * | 5:4  | TMR2OP   | TMR2 operation setting      |
 * | 7:6  | TMR3OP   | TMR3 operation setting      |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elopd_tmr0op_shift = 0U,    /**< TMR0 operation field shift */
  k_elopd_tmr0op_mask  = 0x03U, /**< TMR0 operation field mask */
  k_elopd_tmr1op_shift = 2U,    /**< TMR1 operation field shift */
  k_elopd_tmr1op_mask  = 0x0CU, /**< TMR1 operation field mask */
  k_elopd_tmr2op_shift = 4U,    /**< TMR2 operation field shift */
  k_elopd_tmr2op_mask  = 0x30U, /**< TMR2 operation field mask */
  k_elopd_tmr3op_shift = 6U,    /**< TMR3 operation field shift */
  k_elopd_tmr3op_mask  = 0xC0U, /**< TMR3 operation field mask */
} elopd_bits_t;

/* =============================================================================
 * Port Event Registers
 * =============================================================================
 */

/**
 * @enum pgc_bits_t
 * @brief Port Group Control (PGC1/PGC2) register bit definitions
 *
 * @par Register Layout (8-bit @ 0x0008B125/0x0008B126)
 * | Bits | Field       | Description                             |
 * |------|-------------|-----------------------------------------|
 * | 1:0  | PGCI[1:0]   | Event output edge select (input ports)  |
 * | 2    | PGCOVE      | PDBF overwrite enable                   |
 * | 3    | -           | Reserved                                |
 * | 6:4  | PGCO[2:0]   | Port group operation select             |
 * | 7    | -           | Reserved                                |
 *
 * @note Manual ref: Ch21 section 21.2.10
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* PGCI[1:0] - Event Output Edge Select for input port groups (bits 1:0) */
  k_pgc_pgci_shift   = 0U,    /**< PGCI field shift */
  k_pgc_pgci_mask    = 0x03U, /**< PGCI field mask */
  k_pgc_pgci_rising  = 0x00U, /**< Event on rising edge of input signal */
  k_pgc_pgci_falling = 0x01U, /**< Event on falling edge of input signal */
  k_pgc_pgci_both    = 0x02U, /**< Event on both edges of input signal (1x) */
  /* PGCOVE - PDBF Overwrite enable (bit 2) */
  k_pgc_pgcove       = 0x04U, /**< PDBF overwrite enable (0=disabled, 1=enabled) */
  /* PGCO[2:0] - Port Group Operation Select for output port groups (bits 6:4) */
  k_pgc_pgco_shift   = 4U,    /**< PGCO field shift */
  k_pgc_pgco_mask    = 0x70U, /**< PGCO field mask */
  k_pgc_pgco_low     = 0x00U, /**< Output low when event signal is input */
  k_pgc_pgco_high    = 0x10U, /**< Output high when event signal is input */
  k_pgc_pgco_toggle  = 0x20U, /**< Toggle output when event signal is input */
  k_pgc_pgco_buffer  = 0x30U, /**< Output buffer (PDBF) value when event signal is input */
  k_pgc_pgco_rotate  = 0x40U, /**< Rotate output MSB-to-LSB when event signal is input (1xx) */
} pgc_bits_t;

/**
 * @enum pel_bits_t
 * @brief Port Event Link (PEL0-3) register bit definitions
 *
 * @par Register Layout (8-bit @ 0x0008B129-0x0008B12C)
 * | Bits | Field      | Description                                      |
 * |------|------------|--------------------------------------------------|
 * | 2:0  | PSB[2:0]   | Bit number for single port (pin 0-7)             |
 * | 4:3  | PSP[1:0]   | Port select (00=disabled, 01=Port B, 10=Port E)  |
 * | 6:5  | PSM[1:0]   | Event link spec (edge or output mode)            |
 * | 7    | -          | Reserved                                         |
 *
 * @note Manual ref: Ch21 section 21.2.12
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* PSB[2:0] - Bit Number Specification (bits 2:0) */
  k_pel_psb_mask     = 0x07U, /**< 3-bit pin select mask (pins 0-7) */
  /* PSP[1:0] - Port Number Specification (bits 4:3) */
  k_pel_psp_shift    = 3U,    /**< PSP field shift */
  k_pel_psp_mask     = 0x18U, /**< PSP field mask */
  k_pel_psp_disabled = 0x00U, /**< Setting disabled */
  k_pel_psp_portb    = 0x08U, /**< Port B (corresponding to PGR1) */
  k_pel_psp_porte    = 0x10U, /**< Port E (corresponding to PGR2) */
  /* PSM[1:0] - Event Link Specification (bits 6:5) */
  k_pel_psm_shift    = 5U,    /**< PSM field shift */
  k_pel_psm_mask     = 0x60U, /**< PSM field mask */
  k_pel_psm_rising   = 0x00U, /**< Rising edge (input port) or output low (output port) */
  k_pel_psm_falling  = 0x20U, /**< Falling edge (input port) or output high (output port) */
  k_pel_psm_both     = 0x40U, /**< Both edges (input port) or toggle output (output port) (1x) */
} pel_bits_t;

/**
 * @enum elsegr_bits_t
 * @brief Software Event Generation Register (ELSEGR) bit definitions
 *
 * @par Register Layout (8-bit @ 0x0008B12D)
 * | Bit | Field | Description                    |
 * |-----|-------|--------------------------------|
 * | 0   | SEG   | Software Event Generation      |
 * | 5:1 | -     | Reserved                       |
 * | 6   | WE    | Write Enable                   |
 * | 7   | WI    | Write Disable                  |
 *
 * @note To write: Set WE=1, WI=0, then set SEG
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elsegr_seg = 0x01U, /**< Software event generation */
  k_elsegr_we  = 0x40U, /**< Write enable */
  k_elsegr_wi  = 0x80U, /**< Write disable (set 1 to disable) */
} elsegr_bits_t;

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @enum elc_mstpcr_t
 * @brief ELC module stop control bits
 *
 * @details
 * The ELC is controlled by MSTPCRB.MSTPB9.
 * Must be cleared (0) before accessing ELC registers.
 *
 * @note Manual ref: Ch11 (Low Power Consumption) module stop tables
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief MSTPCRB bit for ELC (bit 9)
   * @details
   * - 0: ELC module is running
   * - 1: ELC module is stopped (default)
   */
  k_elc_mstpcrb_bit = (1U << 9U),
} elc_mstpcr_t;

/* =============================================================================
 * ELSRn Address Range Adjustments
 * =============================================================================
 */

/**
 * @enum elsr_range_adj_t
 * @brief Address computation adjustments for the three ELSRn register ranges
 *
 * @details
 * ELSRn registers are NOT contiguous -- option and port registers occupy the
 * gaps. There are three separate address ranges:
 *
 * Range A  n=0..28  : address = base + k_elsr_range_a_adj + n  (0x0008B101..0x0008B11D)
 * Range B  n=33..45 : address = base + k_elsr_range_b_adj + n  (0x0008B131..0x0008B13D)
 * Range C  n=48..57 : address = base + k_elsr_range_c_adj + n  (0x0008B146..0x0008B14F)
 *
 * Derivation:
 *   ELSR0  = 0x0008B101 = base + 1 + 0   -> adj_a = 1
 *   ELSR33 = 0x0008B131 = base + 0x10 + 33 -> adj_b = 0x10
 *   ELSR48 = 0x0008B146 = base + 0x16 + 48 -> adj_c = 0x16
 *
 * @note Manual ref: Ch21 section 21.2.2, address list
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_elsr_range_a_adj = 0x01U, /**< Adjustment for n=0..28 */
  k_elsr_range_b_adj = 0x10U, /**< Adjustment for n=33..45 */
  k_elsr_range_c_adj = 0x16U, /**< Adjustment for n=48..57 */
  k_elsr_range_a_max = 28U,   /**< Last valid n in range A */
  k_elsr_range_b_max = 45U,   /**< Last valid n in range B */
  k_elsr_range_c_min = 48U,   /**< First valid n in range C */
  k_elsr_range_c_max = 57U,   /**< Last valid n in range C */
} elsr_range_adj_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to ELCR register (Event Link Control)
 * @return Volatile pointer to ELCR register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elcr_reg(void)
{
  return (volatile uint8_t*)k_elc_elcr_addr;
}

/**
 * @brief Get pointer to ELSRn register
 * @param n Register index (0, 3, 4, 7, 10-13, 15, 16, 18-28, 33, 35-38, 45, 48-57)
 * @return Volatile pointer to ELSRn register (8-bit)
 * @warning Not all indices in [0..57] are valid -- consult Table 21.2 for the
 *          valid set. Passing an invalid index returns a pointer to a reserved
 *          or wrong address.
 * @note ELSRn registers occupy three non-contiguous ranges; see elsr_range_adj_t.
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsr_reg(uint8_t n)
{
  if (n <= k_elsr_range_a_max) {
    return (volatile uint8_t*)(uintptr_t)(k_elc_base_addr + k_elsr_range_a_adj + n);
  }
  if ((n >= k_elsr_range_a_max + 1U) && (n <= k_elsr_range_b_max)) {
    return (volatile uint8_t*)(uintptr_t)(k_elc_base_addr + k_elsr_range_b_adj + n);
  }
  if ((n >= k_elsr_range_c_min) && (n <= k_elsr_range_c_max)) {
    return (volatile uint8_t*)(uintptr_t)(k_elc_base_addr + k_elsr_range_c_adj + n);
  }
  return nullptr;
}

/**
 * @brief Get pointer to ELSR0 register (MTU0)
 * @return Volatile pointer to ELSR0 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsr0_reg(void)
{
  return (volatile uint8_t*)k_elc_elsr0_addr;
}

/**
 * @brief Get pointer to ELSR15 register (S12AD ELCTRG00N)
 * @return Volatile pointer to ELSR15 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsr15_reg(void)
{
  return (volatile uint8_t*)k_elc_elsr15_addr;
}

/**
 * @brief Get pointer to ELSR45 register (S12AD1 ELCTRG10N)
 * @return Volatile pointer to ELSR45 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsr45_reg(void)
{
  return (volatile uint8_t*)k_elc_elsr45_addr;
}

/**
 * @brief Get pointer to ELSR48 register (GPTW event source A)
 * @return Volatile pointer to ELSR48 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsr48_reg(void)
{
  return (volatile uint8_t*)k_elc_elsr48_addr;
}

/**
 * @brief Get pointer to ELOPA register (MTU0/MTU3 option)
 * @return Volatile pointer to ELOPA register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elopa_reg(void)
{
  return (volatile uint8_t*)k_elc_elopa_addr;
}

/**
 * @brief Get pointer to ELOPB register (MTU4 option)
 * @return Volatile pointer to ELOPB register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elopb_reg(void)
{
  return (volatile uint8_t*)k_elc_elopb_addr;
}

/**
 * @brief Get pointer to PGR1 register (Port Group 1)
 * @return Volatile pointer to PGR1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pgr1_reg(void)
{
  return (volatile uint8_t*)k_elc_pgr1_addr;
}

/**
 * @brief Get pointer to PGR2 register (Port Group 2)
 * @return Volatile pointer to PGR2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pgr2_reg(void)
{
  return (volatile uint8_t*)k_elc_pgr2_addr;
}

/**
 * @brief Get pointer to PGC1 register (Port Group Control 1)
 * @return Volatile pointer to PGC1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pgc1_reg(void)
{
  return (volatile uint8_t*)k_elc_pgc1_addr;
}

/**
 * @brief Get pointer to PGC2 register (Port Group Control 2)
 * @return Volatile pointer to PGC2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pgc2_reg(void)
{
  return (volatile uint8_t*)k_elc_pgc2_addr;
}

/**
 * @brief Get pointer to PDBF1 register (Port Data Buffer 1)
 * @return Volatile pointer to PDBF1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pdbf1_reg(void)
{
  return (volatile uint8_t*)k_elc_pdbf1_addr;
}

/**
 * @brief Get pointer to PDBF2 register (Port Data Buffer 2)
 * @return Volatile pointer to PDBF2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pdbf2_reg(void)
{
  return (volatile uint8_t*)k_elc_pdbf2_addr;
}

/**
 * @brief Get pointer to PELn register (Port Event Link)
 * @param n Register index (0-3)
 * @return Volatile pointer to PELn register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_pel_reg(uint8_t n)
{
  return (volatile uint8_t*)(uintptr_t)(k_elc_pel0_addr + n);
}

/**
 * @brief Get pointer to ELSEGR register (Software Event Generation)
 * @return Volatile pointer to ELSEGR register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* elc_elsegr_reg(void)
{
  return (volatile uint8_t*)k_elc_elsegr_addr;
}

/* =============================================================================
 * Static Assertions - Address Verification
 * =============================================================================
 */

/**
 * @name Address Verification
 * @brief Compile-time verification of register addresses
 * @{
 */

/* Verify base address */
static_assert(k_elc_base_addr == 0x0008B100U,
              "ELC base address must be 0x0008B100 per Ch21 manual");

/* Verify ELCR address */
static_assert(k_elc_elcr_addr == 0x0008B100U, "ELCR address must be 0x0008B100 per Ch21 manual");

/* Verify ELSRn addresses */
static_assert(k_elc_elsr0_addr == 0x0008B101U, "ELSR0 address must be 0x0008B101 per Ch21 manual");

static_assert(k_elc_elsr3_addr == 0x0008B104U, "ELSR3 address must be 0x0008B104 per Ch21 manual");

static_assert(k_elc_elsr4_addr == 0x0008B105U, "ELSR4 address must be 0x0008B105 per Ch21 manual");

static_assert(k_elc_elsr15_addr == 0x0008B110U,
              "ELSR15 address must be 0x0008B110 per Ch21 manual");

static_assert(k_elc_elsr45_addr == 0x0008B13DU,
              "ELSR45 address must be 0x0008B13D per Ch21 manual");

static_assert(k_elc_elsr48_addr == 0x0008B146U,
              "ELSR48 address must be 0x0008B146 per Ch21 manual");

static_assert(k_elc_elsr57_addr == 0x0008B14FU,
              "ELSR57 address must be 0x0008B14F per Ch21 manual");

/* Verify option setting register addresses */
static_assert(k_elc_elopa_addr == 0x0008B11FU, "ELOPA address must be 0x0008B11F per Ch21 manual");

static_assert(k_elc_elopb_addr == 0x0008B120U, "ELOPB address must be 0x0008B120 per Ch21 manual");

static_assert(k_elc_elopc_addr == 0x0008B121U, "ELOPC address must be 0x0008B121 per Ch21 manual");

static_assert(k_elc_elopd_addr == 0x0008B122U, "ELOPD address must be 0x0008B122 per Ch21 manual");

static_assert(k_elc_elopf_addr == 0x0008B13FU, "ELOPF address must be 0x0008B13F per Ch21 manual");

static_assert(k_elc_eloph_addr == 0x0008B141U, "ELOPH address must be 0x0008B141 per Ch21 manual");

/* Verify port event register addresses */
static_assert(k_elc_pgr1_addr == 0x0008B123U, "PGR1 address must be 0x0008B123 per Ch21 manual");

static_assert(k_elc_pgr2_addr == 0x0008B124U, "PGR2 address must be 0x0008B124 per Ch21 manual");

static_assert(k_elc_pgc1_addr == 0x0008B125U, "PGC1 address must be 0x0008B125 per Ch21 manual");

static_assert(k_elc_pgc2_addr == 0x0008B126U, "PGC2 address must be 0x0008B126 per Ch21 manual");

static_assert(k_elc_pdbf1_addr == 0x0008B127U, "PDBF1 address must be 0x0008B127 per Ch21 manual");

static_assert(k_elc_pdbf2_addr == 0x0008B128U, "PDBF2 address must be 0x0008B128 per Ch21 manual");

static_assert(k_elc_pel0_addr == 0x0008B129U, "PEL0 address must be 0x0008B129 per Ch21 manual");

static_assert(k_elc_pel1_addr == 0x0008B12AU, "PEL1 address must be 0x0008B12A per Ch21 manual");

static_assert(k_elc_pel2_addr == 0x0008B12BU, "PEL2 address must be 0x0008B12B per Ch21 manual");

static_assert(k_elc_pel3_addr == 0x0008B12CU, "PEL3 address must be 0x0008B12C per Ch21 manual");

static_assert(k_elc_elsegr_addr == 0x0008B12DU,
              "ELSEGR address must be 0x0008B12D per Ch21 manual");

/* Verify bit definitions */
static_assert(k_elcr_elcon == 0x80U, "ELCR.ELCON must be bit 7");

static_assert(k_elc_mstpcrb_bit == (1U << 9U), "ELC module stop is MSTPCRB bit 9");

/** @} */

#ifdef __cplusplus
}
#endif
