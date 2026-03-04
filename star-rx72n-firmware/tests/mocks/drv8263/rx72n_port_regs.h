/**
 * @file rx72n_port_regs.h
 * @brief Mock PORT register definitions for DRV8263 unit testing
 *
 * @details
 * Provides RAM-based mock of RX72N PORT register structures. This mock
 * shadows the real rx72n_port_regs.h header so that rx_drv8263.c can be
 * compiled and tested on the host without hardware.
 *
 * The rx_port_regs_t struct layout matches the real hardware struct exactly
 * (same field names, same packed layout with padding) so that code accessing
 * base->podr, base->pidr, etc. works identically.
 *
 * Port accessor functions (port0() through portj()) return pointers into a
 * global RAM array instead of hardware memory-mapped addresses.
 *
 * @author STAR Team
 * @date 2026-03-03
 * @version 1.0.0
 * @copyright STAR Project
 *
 * @see mock_drv8263_port.h Test helper API for manipulating mock ports
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: Static array allocation for g_mock_port_regs
 * - Rule 4: All port accessor functions are single-line inlines
 * - Rule 8: C23 typed enums for all constants (padding sizes, indices)
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Each accessor returns one port's registers
 * - **L (Liskov Substitution):** Mock port accessors substitute for real
 *   hardware memory-mapped register accessors
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Padding Size Constants
 * ============================================================================= */

/**
 * @enum port_pad_size_t
 * @brief Padding byte counts between PORT register groups in the hardware layout
 *
 * @details
 * The RX72N PORT registers are not contiguous in memory; each 8-bit register
 * is separated by padding bytes to match the hardware memory map. These
 * constants define the padding sizes used in the rx_port_regs_t packed struct
 * so that the mock layout is identical to the real hardware.
 *
 * @see rx_port_regs_t Struct that uses these padding sizes
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_port_pad_1f = 0x1F, /**< Padding between consecutive register groups
                              (31 bytes, used for PDR-PODR, PODR-PIDR,
                              PIDR-PMR, and PCR-DSCR gaps) */
  k_port_pad_5f = 0x5F, /**< Padding between PMR and PCR groups
                              (95 bytes) */
  k_port_pad_47 = 0x47, /**< Padding between DSCR and DSCR2 groups
                              (71 bytes) */
} port_pad_size_t;

/* =============================================================================
 * Port Register Structure (identical layout to real hardware struct)
 * ============================================================================= */

/**
 * @struct rx_port_regs_t
 * @brief Mock PORT register structure matching real RX72N hardware layout
 *
 * @details
 * Packed structure replicating the memory layout of a single RX72N GPIO port
 * register block. The padding arrays ensure that each register field sits at
 * the same offset as in the real memory-mapped I/O, allowing production code
 * to access fields by name (e.g., base->podr) without modification.
 *
 * Register offsets within a port block:
 * - PDR:   +0x00  (Port Direction Register)
 * - PODR:  +0x20  (Port Output Data Register)
 * - PIDR:  +0x40  (Port Input Data Register)
 * - PMR:   +0x60  (Port Mode Register)
 * - PCR:   +0xC0  (Pull-up Control Register)
 * - DSCR:  +0xE0  (Drive Strength Control Register)
 * - DSCR2: +0x128 (Drive Strength Control Register 2)
 *
 * @invariant Total struct size must match hardware register block stride
 * @invariant Field offsets must match real hardware memory map exactly
 *
 * @code
 * volatile rx_port_regs_t* p = port0();
 * p->pdr  = 0xFF;  // Set all pins as output
 * p->podr = 0x01;  // Drive pin 0 HIGH
 * uint8_t input = p->pidr;  // Read input pins
 * @endcode
 *
 * @see port_pad_size_t Padding constants used in this struct
 * @see g_mock_port_regs Array of these structs for all mock ports
 *
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  volatile uint8_t pdr;                  /**< Port Direction Register: 1 = output, 0 = input
                                              per pin (8 bits per port) */
  volatile uint8_t _pad1[k_port_pad_1f]; /**< Padding between PDR and PODR (31 bytes) */
  volatile uint8_t podr;                 /**< Port Output Data Register: output level per pin,
                                              1 = HIGH, 0 = LOW */
  volatile uint8_t _pad2[k_port_pad_1f]; /**< Padding between PODR and PIDR (31 bytes) */
  volatile uint8_t pidr;                 /**< Port Input Data Register: current input level
                                              per pin, read-only on real hardware */
  volatile uint8_t _pad3[k_port_pad_1f]; /**< Padding between PIDR and PMR (31 bytes) */
  volatile uint8_t pmr;                  /**< Port Mode Register: 0 = GPIO, 1 = peripheral
                                              function per pin */
  volatile uint8_t _pad4[k_port_pad_5f]; /**< Padding between PMR and PCR (95 bytes) */
  volatile uint8_t pcr;                  /**< Pull-up Control Register: 1 = pull-up enabled
                                              per pin */
  volatile uint8_t _pad5[k_port_pad_1f]; /**< Padding between PCR and DSCR (31 bytes) */
  volatile uint8_t dscr;                 /**< Drive Strength Control Register: drive current
                                              selection per pin */
  volatile uint8_t _pad6[k_port_pad_47]; /**< Padding between DSCR and DSCR2 (71 bytes) */
  volatile uint8_t dscr2;               /**< Drive Strength Control Register 2: extended
                                              drive strength configuration */
} rx_port_regs_t;

/**
 * @enum port_regs_size_t
 * @brief Expected size of the mock port register structure
 *
 * @details
 * Used in the static assertion to verify the packed struct layout matches
 * the hardware register block stride. Derived from the register offsets
 * and padding sizes in rx_port_regs_t.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_port_regs_expected_size = 297, /**< Total bytes: 7 registers + padding */
} port_regs_size_t;

_Static_assert(sizeof(rx_port_regs_t) == k_port_regs_expected_size,
               "rx_port_regs_t size mismatch");

/* =============================================================================
 * Mock Constants (needed by rx_port_constants.h consumers but not used here)
 * ============================================================================= */

/**
 * @enum mock_port_count_t
 * @brief Total number of entries in the mock port register array
 *
 * @details
 * Defines the size of g_mock_port_regs[]. The array must be large enough to
 * hold all port indices used by the RX72N: ports 0-9 (indices 0-9),
 * ports A-F (indices 10-15), port G (index 16), a reserved slot (index 17),
 * port J (index 18), and one spare (index 19).
 *
 * @code
 * rx_port_regs_t regs[k_mock_port_max];
 * memset(regs, 0, sizeof(regs));
 * @endcode
 *
 * @see g_mock_port_regs Global array sized by this constant
 * @see mock_port_idx_t Individual port index values
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_port_max = 20, /**< Covers ports 0-9 (0-9), A-F (10-15), G (16),
                             reserved (17), J (18), spare (19) */
} mock_port_count_t;

/* =============================================================================
 * Port Index Constants
 * ============================================================================= */

/**
 * @enum mock_port_idx_t
 * @brief Array indices for each port in the g_mock_port_regs array
 *
 * @details
 * Maps RX72N port letters to their corresponding index in the mock port
 * register array. Used by the port accessor functions (port0() through
 * portj()) to avoid magic number literals in array subscripts.
 *
 * @see g_mock_port_regs Global array indexed by these values
 * @see mock_port_count_t Array size constant
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_port_idx_0 = 0,  /**< Index for Port 0 */
  k_port_idx_1 = 1,  /**< Index for Port 1 */
  k_port_idx_2 = 2,  /**< Index for Port 2 */
  k_port_idx_3 = 3,  /**< Index for Port 3 */
  k_port_idx_4 = 4,  /**< Index for Port 4 */
  k_port_idx_5 = 5,  /**< Index for Port 5 */
  k_port_idx_6 = 6,  /**< Index for Port 6 */
  k_port_idx_7 = 7,  /**< Index for Port 7 */
  k_port_idx_8 = 8,  /**< Index for Port 8 */
  k_port_idx_9 = 9,  /**< Index for Port 9 */
  k_port_idx_a = 10,  /**< Index for Port A */
  k_port_idx_b = 11,  /**< Index for Port B */
  k_port_idx_c = 12,  /**< Index for Port C */
  k_port_idx_d = 13,  /**< Index for Port D */
  k_port_idx_e = 14,  /**< Index for Port E */
  k_port_idx_f = 15,  /**< Index for Port F */
  k_port_idx_g = 16,          /**< Index for Port G */
  k_port_idx_reserved = 17,  /**< Reserved index (unused, gap between G and J) */
  k_port_idx_j = 18,          /**< Index for Port J (skips reserved=17) */
} mock_port_idx_t;

/* =============================================================================
 * Mock Port Register Array (defined in mock_drv8263_port.c)
 * ============================================================================= */

/**
 * @var g_mock_port_regs
 * @brief Global RAM array of mock port register structs
 *
 * @details
 * Contains k_mock_port_max entries of rx_port_regs_t, providing RAM-backed
 * storage that replaces hardware memory-mapped port registers during testing.
 * Port accessor functions (port0() through portj()) return pointers into
 * this array. Test helpers in mock_drv8263_port.h manipulate these registers
 * to simulate hardware behavior.
 *
 * @note Defined in mock_drv8263_port.c; declared extern here for accessor use
 *
 * @warning Do not access directly in tests; use mock_drv8263_port_set_pidr()
 *          and mock_drv8263_port_get_podr() for clean test interfaces
 *
 * @since Version 1.0.0
 */
extern rx_port_regs_t g_mock_port_regs[k_mock_port_max];

/* =============================================================================
 * Port Accessor Functions (mock versions returning RAM pointers)
 * ============================================================================= */

/**
 * @brief Mock port accessor functions
 *
 * @details
 * Each function returns a volatile pointer to the corresponding mock port
 * register struct in g_mock_port_regs. These shadow the real hardware
 * accessor functions that return memory-mapped addresses.
 *
 * @return Pointer to mock port register struct for the given port
 *
 * @pre g_mock_port_regs array must be defined (in mock_drv8263_port.c)
 * @pre mock_drv8263_port_reset() should be called before first use in tests
 *
 * @post Returned pointer is valid for the lifetime of the test
 * @post No side effects on port register contents
 *
 * @note Thread-safe (read-only pointer computation)
 *
 * @see rx_port_regs_t Port register structure
 * @see g_mock_port_regs Global mock register array
 *
 * @since Version 1.0.0
 * @{
 */

static inline volatile rx_port_regs_t* port0(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_0];
}

static inline volatile rx_port_regs_t* port1(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_1];
}

static inline volatile rx_port_regs_t* port2(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_2];
}

static inline volatile rx_port_regs_t* port3(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_3];
}

static inline volatile rx_port_regs_t* port4(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_4];
}

static inline volatile rx_port_regs_t* port5(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_5];
}

static inline volatile rx_port_regs_t* port6(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_6];
}

static inline volatile rx_port_regs_t* port7(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_7];
}

static inline volatile rx_port_regs_t* port8(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_8];
}

static inline volatile rx_port_regs_t* port9(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_9];
}

static inline volatile rx_port_regs_t* porta(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_a];
}

static inline volatile rx_port_regs_t* portb(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_b];
}

static inline volatile rx_port_regs_t* portc(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_c];
}

static inline volatile rx_port_regs_t* portd(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_d];
}

static inline volatile rx_port_regs_t* porte(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_e];
}

static inline volatile rx_port_regs_t* portf(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_f];
}

static inline volatile rx_port_regs_t* portg(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_g];
}

static inline volatile rx_port_regs_t* portj(void)
{
  return (volatile rx_port_regs_t*)&g_mock_port_regs[k_port_idx_j];
}

/** @} */

#ifdef __cplusplus
}
#endif
