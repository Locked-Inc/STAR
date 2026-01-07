/* lib/rx_hal/inc/rx72n_mpc_regs.h */

/**
 * @file rx72n_mpc_regs.h
 * @brief RX72N MPC Pin Controller Register Definitions
 *
 * Register definitions for the Multi-Function Pin Controller (MPC) used for
 * pin function selection and configuration.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_MPC_REGS_H
#define STAR_RX72N_MPC_REGS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Multi-Function Pin Controller (MPC)
 * =============================================================================
 */

/** @brief MPC base address */
typedef enum {
  k_mpc_base_addr = 0x0008C100, /**< MPC register base address */
} rx_mpc_addresses_t;

/** @brief MPC register reserved field sizes */
typedef enum {
  k_mpc_reserved1_bytes = 1,  /**< Reserved after PFCSE (0x0008C101) */
  k_mpc_reserved2_bytes = 4,  /**< Reserved after PFBCR3 (0x0008C10A-0x0008C10D) */
  k_mpc_reserved3_bytes = 16, /**< Reserved after PFENET (0x0008C10F-0x0008C11E) */
  k_mpc_reserved4_bytes = 8,  /**< Reserved after PWPR (0x0008C120-0x0008C127) */
  k_mpc_dscr2_count     = 24, /**< DSCR2 registers count (0x0008C128-0x0008C13F) */

  /* Port reserved byte counts (for ports with <8 pins) */
  k_port3_reserved_bytes  = 3, /**< Port 3: P35-P37 (P35 is NMI-only) */
  k_port5_reserved_bytes  = 1, /**< Port 5: P53 (dedicated BCLK) */
  k_port6_reserved_bytes  = 1, /**< Port 6: P65 (dedicated CKE) */
  k_port7_reserved_bytes  = 1, /**< Port 7: P70 (does not exist) */
  k_portf_reserved1_bytes = 2, /**< Port F: PF3-PF4 */
  k_portf_reserved2_bytes = 2, /**< Port F: PF6-PF7 */
  k_portj_reserved1_bytes = 1, /**< Port J: PJ4 */
  k_portj_reserved2_bytes = 2, /**< Port J: PJ6-PJ7 */
} mpc_reserved_sizes_t;

/**
 * @brief Pin Function Select Register (C bitfield)
 * @details
 * Each pin has a PFS register controlling peripheral function selection.
 *
 * This struct uses C bitfields (the `: N` syntax) to pack multiple fields
 * into a single byte, matching the hardware register layout exactly:
 *
 * Bit layout in hardware register (1 byte):
 * ```
 * 7       6       5   4   3   2   1   0
 * +-----------------------------------------+
 * | ASEL | ISEL | Reserved | PSEL[4:0]    |
 * +-----------------------------------------+
 * ```
 *
 * The syntax `volatile uint8_t name : bits` defines a bitfield where:
 * - `name` is the field name (or omitted for padding/reserved bits)
 * - `bits` is the number of bits the field occupies
 *
 * Example: `psel : 5` means psel occupies 5 bits (bits 0-4)
 *
 * This is standard C (not C++) and commonly used in embedded systems
 * for hardware register definitions where bits are packed together.
 */
typedef struct {
  volatile uint8_t psel : 5; /**< Peripheral Select (bits 0-4) */
  volatile uint8_t : 1;      /**< Reserved bit (bit 5) - unnamed bitfield for padding */
  volatile uint8_t isel : 1; /**< Interrupt Input Select (bit 6) */
  volatile uint8_t asel : 1; /**< Analog Input Select (bit 7) */
} rx_pfs_regs_t;

/**
 * @brief MPC Register Map
 * @details
 * Multi-Function Pin Controller (MPC) registers for pin function selection.
 * Controls which peripheral function each GPIO pin is assigned to.
 * Base address: 0x0008C100
 *
 * CRITICAL: This struct matches the exact hardware memory layout!
 * - 0x0008C100: Bus control registers (PFCSE, PFCSS, PFAOE, PFBCR, PFENET)
 * - 0x0008C11F: PWPR (Write Protect Register)
 * - 0x0008C140: PFS registers start (P00PFS)
 *
 * Package Support (100-pin LFQFP R5F572NNHGFP#30):
 * - Available ports: 0-5 (partial), A-E (full), J (partial: PJ3, PJ5)
 * - Not available on 100-pin: 6-9, F, G, H
 * - Note: All ports are defined for compatibility with larger packages
 *
 * Complete Memory Layout:
 * - 0x0008C100: PFCSE (CS Output Enable Register)
 * - 0x0008C101: Reserved (1 byte)
 * - 0x0008C102-0x0008C103: PFCSS0, PFCSS1 (CS Output Pin Select)
 * - 0x0008C104-0x0008C105: PFAOE0, PFAOE1 (Address Output Enable)
 * - 0x0008C106-0x0008C109: PFBCR0-3 (External Bus Control)
 * - 0x0008C10A-0x0008C10D: Reserved (4 bytes)
 * - 0x0008C10E: PFENET (Ethernet Control)
 * - 0x0008C10F-0x0008C11E: Reserved (16 bytes)
 * - 0x0008C11F: PWPR (Write Protect Register)
 * - 0x0008C120-0x0008C127: Reserved (8 bytes)
 * - 0x0008C128-0x0008C13F: DSCR2 (Drive Capacity Control, 24 bytes)
 * - 0x0008C140+: PFS registers (P00PFS through PJPFS, 152 bytes)
 */
typedef struct {
  /* Bus Control Registers (0x0008C100 - 0x0008C10E) */
  volatile uint8_t pfcse;                            /**< +0x00: CS Output Enable Register */
  uint8_t          reserved1[k_mpc_reserved1_bytes]; /**< +0x01: Reserved */
  volatile uint8_t pfcss0;                           /**< +0x02: CS Output Pin Select 0 */
  volatile uint8_t pfcss1;                           /**< +0x03: CS Output Pin Select 1 */
  volatile uint8_t pfaoe0;                           /**< +0x04: Address Output Enable 0 */
  volatile uint8_t pfaoe1;                           /**< +0x05: Address Output Enable 1 */
  volatile uint8_t pfbcr0;                           /**< +0x06: External Bus Control 0 */
  volatile uint8_t pfbcr1;                           /**< +0x07: External Bus Control 1 */
  volatile uint8_t pfbcr2;                           /**< +0x08: External Bus Control 2 */
  volatile uint8_t pfbcr3;                           /**< +0x09: External Bus Control 3 */
  uint8_t          reserved2[k_mpc_reserved2_bytes]; /**< +0x0A-0x0D: Reserved */
  volatile uint8_t pfenet;                           /**< +0x0E: Ethernet Control Register */
  uint8_t          reserved3[k_mpc_reserved3_bytes]; /**< +0x0F-0x1E: Reserved */

  /* Write Protection (0x0008C11F) */
  volatile uint8_t pwpr; /**< +0x1F: Write Protect Register (enable PFS writes) */

  /* Reserved and Drive Capacity (0x0008C120 - 0x0008C13F) */
  uint8_t reserved4[k_mpc_reserved4_bytes]; /**< +0x20-0x27: Reserved */
  uint8_t dscr2[k_mpc_dscr2_count];         /**< +0x28-0x3F: Drive Capacity Control 2 */

  /* Pin Function Select Registers start at 0x0008C140 (+0x40 from base) */
  volatile uint8_t p00pfs;                        /**< Port 0 Pin 0 Function Select */
  volatile uint8_t p01pfs;                        /**< Port 0 Pin 1 Function Select */
  volatile uint8_t p02pfs;                        /**< Port 0 Pin 2 Function Select */
  volatile uint8_t p03pfs;                        /**< Port 0 Pin 3 Function Select */
  volatile uint8_t p04pfs;                        /**< Port 0 Pin 4 Function Select */
  volatile uint8_t p05pfs;                        /**< Port 0 Pin 5 Function Select */
  volatile uint8_t p06pfs;                        /**< Port 0 Pin 6 Function Select */
  volatile uint8_t p07pfs;                        /**< Port 0 Pin 7 Function Select */
  volatile uint8_t p10pfs;                        /**< Port 1 Pin 0 Function Select */
  volatile uint8_t p11pfs;                        /**< Port 1 Pin 1 Function Select */
  volatile uint8_t p12pfs;                        /**< Port 1 Pin 2 Function Select */
  volatile uint8_t p13pfs;                        /**< Port 1 Pin 3 Function Select */
  volatile uint8_t p14pfs;                        /**< Port 1 Pin 4 Function Select */
  volatile uint8_t p15pfs;                        /**< Port 1 Pin 5 Function Select */
  volatile uint8_t p16pfs;                        /**< Port 1 Pin 6 Function Select */
  volatile uint8_t p17pfs;                        /**< Port 1 Pin 7 Function Select */
  volatile uint8_t p20pfs;                        /**< Port 2 Pin 0 Function Select */
  volatile uint8_t p21pfs;                        /**< Port 2 Pin 1 Function Select */
  volatile uint8_t p22pfs;                        /**< Port 2 Pin 2 Function Select */
  volatile uint8_t p23pfs;                        /**< Port 2 Pin 3 Function Select */
  volatile uint8_t p24pfs;                        /**< Port 2 Pin 4 Function Select */
  volatile uint8_t p25pfs;                        /**< Port 2 Pin 5 Function Select */
  volatile uint8_t p26pfs;                        /**< Port 2 Pin 6 Function Select */
  volatile uint8_t p27pfs;                        /**< Port 2 Pin 7 Function Select */
  volatile uint8_t p30pfs;                        /**< Port 3 Pin 0 Function Select */
  volatile uint8_t p31pfs;                        /**< Port 3 Pin 1 Function Select */
  volatile uint8_t p32pfs;                        /**< Port 3 Pin 2 Function Select */
  volatile uint8_t p33pfs;                        /**< Port 3 Pin 3 Function Select */
  volatile uint8_t p34pfs;                        /**< Port 3 Pin 4 Function Select */
  uint8_t port3_reserved[k_port3_reserved_bytes]; /**< Port 3 reserved (P35=NMI, P36-P37 N/A) */
  volatile uint8_t p40pfs;                        /**< Port 4 Pin 0 Function Select */
  volatile uint8_t p41pfs;                        /**< Port 4 Pin 1 Function Select */
  volatile uint8_t p42pfs;                        /**< Port 4 Pin 2 Function Select */
  volatile uint8_t p43pfs;                        /**< Port 4 Pin 3 Function Select */
  volatile uint8_t p44pfs;                        /**< Port 4 Pin 4 Function Select */
  volatile uint8_t p45pfs;                        /**< Port 4 Pin 5 Function Select */
  volatile uint8_t p46pfs;                        /**< Port 4 Pin 6 Function Select */
  volatile uint8_t p47pfs;                        /**< Port 4 Pin 7 Function Select */
  volatile uint8_t p50pfs;                        /**< Port 5 Pin 0 Function Select */
  volatile uint8_t p51pfs;                        /**< Port 5 Pin 1 Function Select */
  volatile uint8_t p52pfs;                        /**< Port 5 Pin 2 Function Select */
  uint8_t port5_reserved[k_port5_reserved_bytes]; /**< Port 5 reserved (P53=BCLK dedicated) */
  volatile uint8_t p54pfs;                        /**< Port 5 Pin 4 Function Select */
  volatile uint8_t p55pfs;                        /**< Port 5 Pin 5 Function Select */
  volatile uint8_t p56pfs;                        /**< Port 5 Pin 6 Function Select */
  volatile uint8_t p57pfs;                        /**< Port 5 Pin 7 Function Select */
  volatile uint8_t p60pfs;                        /**< Port 6 Pin 0 Function Select */
  volatile uint8_t p61pfs;                        /**< Port 6 Pin 1 Function Select */
  volatile uint8_t p62pfs;                        /**< Port 6 Pin 2 Function Select */
  volatile uint8_t p63pfs;                        /**< Port 6 Pin 3 Function Select */
  volatile uint8_t p64pfs;                        /**< Port 6 Pin 4 Function Select */
  uint8_t port6_reserved[k_port6_reserved_bytes]; /**< Port 6 reserved (P65=CKE dedicated) */
  volatile uint8_t p66pfs;                        /**< Port 6 Pin 6 Function Select */
  volatile uint8_t p67pfs;                        /**< Port 6 Pin 7 Function Select */
  uint8_t          port7_reserved[k_port7_reserved_bytes];   /**< Port 7 reserved (P70 N/A) */
  volatile uint8_t p71pfs;                                   /**< Port 7 Pin 1 Function Select */
  volatile uint8_t p72pfs;                                   /**< Port 7 Pin 2 Function Select */
  volatile uint8_t p73pfs;                                   /**< Port 7 Pin 3 Function Select */
  volatile uint8_t p74pfs;                                   /**< Port 7 Pin 4 Function Select */
  volatile uint8_t p75pfs;                                   /**< Port 7 Pin 5 Function Select */
  volatile uint8_t p76pfs;                                   /**< Port 7 Pin 6 Function Select */
  volatile uint8_t p77pfs;                                   /**< Port 7 Pin 7 Function Select */
  volatile uint8_t p80pfs;                                   /**< Port 8 Pin 0 Function Select */
  volatile uint8_t p81pfs;                                   /**< Port 8 Pin 1 Function Select */
  volatile uint8_t p82pfs;                                   /**< Port 8 Pin 2 Function Select */
  volatile uint8_t p83pfs;                                   /**< Port 8 Pin 3 Function Select */
  volatile uint8_t p84pfs;                                   /**< Port 8 Pin 4 Function Select */
  volatile uint8_t p85pfs;                                   /**< Port 8 Pin 5 Function Select */
  volatile uint8_t p86pfs;                                   /**< Port 8 Pin 6 Function Select */
  volatile uint8_t p87pfs;                                   /**< Port 8 Pin 7 Function Select */
  volatile uint8_t p90pfs;                                   /**< Port 9 Pin 0 Function Select */
  volatile uint8_t p91pfs;                                   /**< Port 9 Pin 1 Function Select */
  volatile uint8_t p92pfs;                                   /**< Port 9 Pin 2 Function Select */
  volatile uint8_t p93pfs;                                   /**< Port 9 Pin 3 Function Select */
  volatile uint8_t p94pfs;                                   /**< Port 9 Pin 4 Function Select */
  volatile uint8_t p95pfs;                                   /**< Port 9 Pin 5 Function Select */
  volatile uint8_t p96pfs;                                   /**< Port 9 Pin 6 Function Select */
  volatile uint8_t p97pfs;                                   /**< Port 9 Pin 7 Function Select */
  volatile uint8_t pa0pfs;                                   /**< Port A Pin 0 Function Select */
  volatile uint8_t pa1pfs;                                   /**< Port A Pin 1 Function Select */
  volatile uint8_t pa2pfs;                                   /**< Port A Pin 2 Function Select */
  volatile uint8_t pa3pfs;                                   /**< Port A Pin 3 Function Select */
  volatile uint8_t pa4pfs;                                   /**< Port A Pin 4 Function Select */
  volatile uint8_t pa5pfs;                                   /**< Port A Pin 5 Function Select */
  volatile uint8_t pa6pfs;                                   /**< Port A Pin 6 Function Select */
  volatile uint8_t pa7pfs;                                   /**< Port A Pin 7 Function Select */
  volatile uint8_t pb0pfs;                                   /**< Port B Pin 0 Function Select */
  volatile uint8_t pb1pfs;                                   /**< Port B Pin 1 Function Select */
  volatile uint8_t pb2pfs;                                   /**< Port B Pin 2 Function Select */
  volatile uint8_t pb3pfs;                                   /**< Port B Pin 3 Function Select */
  volatile uint8_t pb4pfs;                                   /**< Port B Pin 4 Function Select */
  volatile uint8_t pb5pfs;                                   /**< Port B Pin 5 Function Select */
  volatile uint8_t pb6pfs;                                   /**< Port B Pin 6 Function Select */
  volatile uint8_t pb7pfs;                                   /**< Port B Pin 7 Function Select */
  volatile uint8_t pc0pfs;                                   /**< Port C Pin 0 Function Select */
  volatile uint8_t pc1pfs;                                   /**< Port C Pin 1 Function Select */
  volatile uint8_t pc2pfs;                                   /**< Port C Pin 2 Function Select */
  volatile uint8_t pc3pfs;                                   /**< Port C Pin 3 Function Select */
  volatile uint8_t pc4pfs;                                   /**< Port C Pin 4 Function Select */
  volatile uint8_t pc5pfs;                                   /**< Port C Pin 5 Function Select */
  volatile uint8_t pc6pfs;                                   /**< Port C Pin 6 Function Select */
  volatile uint8_t pc7pfs;                                   /**< Port C Pin 7 Function Select */
  volatile uint8_t pd0pfs;                                   /**< Port D Pin 0 Function Select */
  volatile uint8_t pd1pfs;                                   /**< Port D Pin 1 Function Select */
  volatile uint8_t pd2pfs;                                   /**< Port D Pin 2 Function Select */
  volatile uint8_t pd3pfs;                                   /**< Port D Pin 3 Function Select */
  volatile uint8_t pd4pfs;                                   /**< Port D Pin 4 Function Select */
  volatile uint8_t pd5pfs;                                   /**< Port D Pin 5 Function Select */
  volatile uint8_t pd6pfs;                                   /**< Port D Pin 6 Function Select */
  volatile uint8_t pd7pfs;                                   /**< Port D Pin 7 Function Select */
  volatile uint8_t pe0pfs;                                   /**< Port E Pin 0 Function Select */
  volatile uint8_t pe1pfs;                                   /**< Port E Pin 1 Function Select */
  volatile uint8_t pe2pfs;                                   /**< Port E Pin 2 Function Select */
  volatile uint8_t pe3pfs;                                   /**< Port E Pin 3 Function Select */
  volatile uint8_t pe4pfs;                                   /**< Port E Pin 4 Function Select */
  volatile uint8_t pe5pfs;                                   /**< Port E Pin 5 Function Select */
  volatile uint8_t pe6pfs;                                   /**< Port E Pin 6 Function Select */
  volatile uint8_t pe7pfs;                                   /**< Port E Pin 7 Function Select */
  volatile uint8_t pf0pfs;                                   /**< Port F Pin 0 Function Select */
  volatile uint8_t pf1pfs;                                   /**< Port F Pin 1 Function Select */
  volatile uint8_t pf2pfs;                                   /**< Port F Pin 2 Function Select */
  uint8_t          portf_reserved1[k_portf_reserved1_bytes]; /**< Port F reserved (PF3-PF4 N/A) */
  volatile uint8_t pf5pfs;                                   /**< Port F Pin 5 Function Select */
  uint8_t          portf_reserved2[k_portf_reserved2_bytes]; /**< Port F reserved (PF6-PF7 N/A) */
  volatile uint8_t pg0pfs;                                   /**< Port G Pin 0 Function Select */
  volatile uint8_t pg1pfs;                                   /**< Port G Pin 1 Function Select */
  volatile uint8_t pg2pfs;                                   /**< Port G Pin 2 Function Select */
  volatile uint8_t pg3pfs;                                   /**< Port G Pin 3 Function Select */
  volatile uint8_t pg4pfs;                                   /**< Port G Pin 4 Function Select */
  volatile uint8_t pg5pfs;                                   /**< Port G Pin 5 Function Select */
  volatile uint8_t pg6pfs;                                   /**< Port G Pin 6 Function Select */
  volatile uint8_t pg7pfs;                                   /**< Port G Pin 7 Function Select */
  volatile uint8_t ph0pfs;                                   /**< Port H Pin 0 Function Select */
  volatile uint8_t ph1pfs;                                   /**< Port H Pin 1 Function Select */
  volatile uint8_t ph2pfs;                                   /**< Port H Pin 2 Function Select */
  volatile uint8_t ph3pfs;                                   /**< Port H Pin 3 Function Select */
  volatile uint8_t ph4pfs;                                   /**< Port H Pin 4 Function Select */
  volatile uint8_t ph5pfs;                                   /**< Port H Pin 5 Function Select */
  volatile uint8_t ph6pfs;                                   /**< Port H Pin 6 Function Select */
  volatile uint8_t ph7pfs;                                   /**< Port H Pin 7 Function Select */
  volatile uint8_t pj0pfs;                                   /**< Port J Pin 0 Function Select */
  volatile uint8_t pj1pfs;                                   /**< Port J Pin 1 Function Select */
  volatile uint8_t pj2pfs;                                   /**< Port J Pin 2 Function Select */
  volatile uint8_t pj3pfs;                                   /**< Port J Pin 3 Function Select */
  uint8_t          portj_reserved1[k_portj_reserved1_bytes]; /**< Port J reserved (PJ4 N/A) */
  volatile uint8_t pj5pfs;                                   /**< Port J Pin 5 Function Select */
  uint8_t          portj_reserved2[k_portj_reserved2_bytes]; /**< Port J reserved (PJ6-PJ7 N/A) */
} rx_mpc_regs_t;

/**
 * @brief Get pointer to MPC registers
 * @return Volatile pointer to MPC register structure
 */
static inline volatile rx_mpc_regs_t* mpc(void)
{
  return (volatile rx_mpc_regs_t*)k_mpc_base_addr;
}

/* MPC Write Protect Register (PWPR) bits */
typedef enum {
  k_mpc_pwpr_pfswe = (1 << 6), /**< PFS Write Enable */
  k_mpc_pwpr_b0wi  = (1 << 7), /**< PFSWE Bit Write Disable */
} mpc_pwpr_bits_t;

/* PFS Register bits */
typedef enum {
  k_pfs_psel_mask = 0x1F,     /**< Peripheral Select mask (bits 0-4) */
  k_pfs_isel      = (1 << 6), /**< Interrupt Input Select */
  k_pfs_asel      = (1 << 7), /**< Analog Input Select */
} pfs_bits_t;

/* =============================================================================
 * Compile-Time Register Layout Verification
 * =============================================================================
 * These static assertions verify the struct layout matches hardware exactly.
 * If any assertion fails, the struct definition is WRONG and must be fixed!
 */

/* Verify critical register offsets from base (0x0008C100) */
_Static_assert(offsetof(rx_mpc_regs_t, pfcse) == 0x00,
               "PFCSE must be at offset 0x00 (address 0x0008C100)");
_Static_assert(offsetof(rx_mpc_regs_t, pwpr) == 0x1F,
               "PWPR must be at offset 0x1F (address 0x0008C11F)");
_Static_assert(offsetof(rx_mpc_regs_t, p00pfs) == 0x40,
               "P00PFS must be at offset 0x40 (address 0x0008C140)");

/* Verify specific port offsets from P00PFS base */
_Static_assert(offsetof(rx_mpc_regs_t, p50pfs) == 0x40 + 0x28,
               "P50PFS must be at offset 0x68 (Port 5 base)");
_Static_assert(offsetof(rx_mpc_regs_t, pa0pfs) == 0x40 + 0x50,
               "PA0PFS must be at offset 0x90 (Port A base)");
_Static_assert(offsetof(rx_mpc_regs_t, pe0pfs) == 0x40 + 0x70,
               "PE0PFS must be at offset 0xB0 (Port E base)");
_Static_assert(offsetof(rx_mpc_regs_t, pj0pfs) == 0x40 + 0x90,
               "PJ0PFS must be at offset 0xD0 (Port J base)");

/* Verify struct total size */
_Static_assert(sizeof(rx_mpc_regs_t) == 0x40 + 152,
               "MPC struct size must be 64 + 152 = 216 bytes total");

/* =============================================================================
 * Pin Mapping Examples
 * =============================================================================
 * Example MPC configuration for common STAR peripherals.
 * See rx_mpc.h for PSEL constant definitions and helper functions.
 *
 * 1. Configure PB7 as SCI9 TXD (Debug UART):
 *    mpc()->pwpr = 0x00;              // Clear B0WI
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;  // Enable PFS write
 *    mpc()->pb7pfs = 0x0A;            // PSEL = 0x0A for SCI
 *    mpc()->pwpr = 0x00;              // Disable PFS write
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;   // Protect PFSWE
 *
 * 2. Configure PE5 as GTIOC0A (Motor 0 PWM Phase):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->pe5pfs = 0x07;            // PSEL = 0x07 for GPTW
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * 3. Configure P24 as MTCLKA (Encoder 0 Phase A):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->p24pfs = 0x02;            // PSEL = 0x02 for MTU CLK
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * 4. Configure PA4 as SSLA0-B (RPi5 SPI Chip Select):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->pa4pfs = 0x0D;            // PSEL = 0x0D for RSPI
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * Note: After MPC configuration, also set PMR (Port Mode Register) bit to '1'
 * to enable peripheral mode. See rx_mpc.h for helper functions and named constants.
 */

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MPC_REGS_H */
