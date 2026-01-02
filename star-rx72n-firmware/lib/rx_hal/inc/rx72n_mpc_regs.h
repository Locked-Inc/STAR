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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Multi-Function Pin Controller (MPC)
 * =============================================================================
 */

/** @brief MPC register reserved field sizes */
typedef enum {
  k_mpc_reserved_after_pwpr_bytes = 32, /**< Reserved bytes after PWPR */
} mpc_reserved_sizes_t;

/**
 * @brief Pin Function Select Register (bitfield)
 * @details
 * Each pin has a PFS register controlling peripheral function selection.
 */
typedef struct {
  volatile uint8_t psel : 5; /**< Peripheral Select (bits 0-4) */
  volatile uint8_t : 1;      /**< Reserved */
  volatile uint8_t isel : 1; /**< Interrupt Input Select (bit 6) */
  volatile uint8_t asel : 1; /**< Analog Input Select (bit 7) */
} rx_pfs_regs_t;

/**
 * @brief MPC Register Map
 * @details
 * Multi-Function Pin Controller (MPC) registers for pin function selection.
 * Controls which peripheral function each GPIO pin is assigned to.
 * Base address: 0x0008C100
 */
typedef struct {
  volatile uint8_t pwpr; /**< Write Protect Register (enable PFS writes) */
  uint8_t          reserved0[k_mpc_reserved_after_pwpr_bytes]; /**< Reserved */
  volatile uint8_t p00pfs;                                     /**< Port 0 Pin 0 Function Select */
  volatile uint8_t p01pfs;                                     /**< Port 0 Pin 1 Function Select */
  volatile uint8_t p02pfs;                                     /**< Port 0 Pin 2 Function Select */
  volatile uint8_t p03pfs;                                     /**< Port 0 Pin 3 Function Select */
  volatile uint8_t p04pfs;                                     /**< Port 0 Pin 4 Function Select */
  volatile uint8_t p05pfs;                                     /**< Port 0 Pin 5 Function Select */
  volatile uint8_t p06pfs;                                     /**< Port 0 Pin 6 Function Select */
  volatile uint8_t p07pfs;                                     /**< Port 0 Pin 7 Function Select */
  volatile uint8_t p10pfs;                                     /**< Port 1 Pin 0 Function Select */
  volatile uint8_t p11pfs;                                     /**< Port 1 Pin 1 Function Select */
  volatile uint8_t p12pfs;                                     /**< Port 1 Pin 2 Function Select */
  volatile uint8_t p13pfs;                                     /**< Port 1 Pin 3 Function Select */
  volatile uint8_t p14pfs;                                     /**< Port 1 Pin 4 Function Select */
  volatile uint8_t p15pfs;                                     /**< Port 1 Pin 5 Function Select */
  volatile uint8_t p16pfs;                                     /**< Port 1 Pin 6 Function Select */
  volatile uint8_t p17pfs;                                     /**< Port 1 Pin 7 Function Select */
  volatile uint8_t p20pfs;                                     /**< Port 2 Pin 0 Function Select */
  volatile uint8_t p21pfs;                                     /**< Port 2 Pin 1 Function Select */
  volatile uint8_t p22pfs;                                     /**< Port 2 Pin 2 Function Select */
  volatile uint8_t p23pfs;                                     /**< Port 2 Pin 3 Function Select */
  volatile uint8_t p24pfs;                                     /**< Port 2 Pin 4 Function Select */
  volatile uint8_t p25pfs;                                     /**< Port 2 Pin 5 Function Select */
  volatile uint8_t p26pfs;                                     /**< Port 2 Pin 6 Function Select */
  volatile uint8_t p27pfs;                                     /**< Port 2 Pin 7 Function Select */
  volatile uint8_t p30pfs;                                     /**< Port 3 Pin 0 Function Select */
  volatile uint8_t p31pfs;                                     /**< Port 3 Pin 1 Function Select */
  volatile uint8_t p32pfs;                                     /**< Port 3 Pin 2 Function Select */
  volatile uint8_t p33pfs;                                     /**< Port 3 Pin 3 Function Select */
  volatile uint8_t p34pfs;                                     /**< Port 3 Pin 4 Function Select */
  volatile uint8_t p40pfs;                                     /**< Port 4 Pin 0 Function Select */
  volatile uint8_t p41pfs;                                     /**< Port 4 Pin 1 Function Select */
  volatile uint8_t p42pfs;                                     /**< Port 4 Pin 2 Function Select */
  volatile uint8_t p43pfs;                                     /**< Port 4 Pin 3 Function Select */
  volatile uint8_t p44pfs;                                     /**< Port 4 Pin 4 Function Select */
  volatile uint8_t p45pfs;                                     /**< Port 4 Pin 5 Function Select */
  volatile uint8_t p46pfs;                                     /**< Port 4 Pin 6 Function Select */
  volatile uint8_t p47pfs;                                     /**< Port 4 Pin 7 Function Select */
  volatile uint8_t p50pfs;                                     /**< Port 5 Pin 0 Function Select */
  volatile uint8_t p51pfs;                                     /**< Port 5 Pin 1 Function Select */
  volatile uint8_t p52pfs;                                     /**< Port 5 Pin 2 Function Select */
  volatile uint8_t p53pfs;                                     /**< Port 5 Pin 3 Function Select */
  volatile uint8_t p54pfs;                                     /**< Port 5 Pin 4 Function Select */
  volatile uint8_t p55pfs;                                     /**< Port 5 Pin 5 Function Select */
  volatile uint8_t p56pfs;                                     /**< Port 5 Pin 6 Function Select */
  volatile uint8_t p57pfs;                                     /**< Port 5 Pin 7 Function Select */
  /* Note: Additional port PFS registers continue for all ports */
  /* Simplified for common motor control pins */
} rx_mpc_regs_t;

#define MPC_BASE ((rx_mpc_regs_t*)0x0008C100)
#define MPC      (*MPC_BASE)

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

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MPC_REGS_H */
