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

/* Pin Function Select Register */
typedef struct {
  volatile uint8_t PSEL : 5; /* Peripheral Select (bits 0-4) */
  volatile uint8_t : 1;      /* Reserved */
  volatile uint8_t ISEL : 1; /* Interrupt Input Select (bit 6) */
  volatile uint8_t ASEL : 1; /* Analog Input Select (bit 7) */
} rx_pfs_regs_t;

/* MPC Register Block */
typedef struct {
  volatile uint8_t PWPR; /* 0x00: Write Protect Register */
  uint8_t          RESERVED0[32];
  volatile uint8_t P00PFS; /* 0x21: Port 0 Pin 0 Function Select */
  volatile uint8_t P01PFS; /* 0x22: Port 0 Pin 1 Function Select */
  volatile uint8_t P02PFS; /* 0x23: Port 0 Pin 2 Function Select */
  volatile uint8_t P03PFS; /* 0x24: Port 0 Pin 3 Function Select */
  volatile uint8_t P04PFS; /* 0x25: Port 0 Pin 4 Function Select */
  volatile uint8_t P05PFS; /* 0x26: Port 0 Pin 5 Function Select */
  volatile uint8_t P06PFS; /* 0x27: Port 0 Pin 6 Function Select */
  volatile uint8_t P07PFS; /* 0x28: Port 0 Pin 7 Function Select */
  volatile uint8_t P10PFS; /* 0x29: Port 1 Pin 0 Function Select */
  volatile uint8_t P11PFS; /* 0x2A: Port 1 Pin 1 Function Select */
  volatile uint8_t P12PFS; /* 0x2B: Port 1 Pin 2 Function Select */
  volatile uint8_t P13PFS; /* 0x2C: Port 1 Pin 3 Function Select */
  volatile uint8_t P14PFS; /* 0x2D: Port 1 Pin 4 Function Select */
  volatile uint8_t P15PFS; /* 0x2E: Port 1 Pin 5 Function Select */
  volatile uint8_t P16PFS; /* 0x2F: Port 1 Pin 6 Function Select */
  volatile uint8_t P17PFS; /* 0x30: Port 1 Pin 7 Function Select */
  volatile uint8_t P20PFS; /* 0x31: Port 2 Pin 0 Function Select */
  volatile uint8_t P21PFS; /* 0x32: Port 2 Pin 1 Function Select */
  volatile uint8_t P22PFS; /* 0x33: Port 2 Pin 2 Function Select */
  volatile uint8_t P23PFS; /* 0x34: Port 2 Pin 3 Function Select */
  volatile uint8_t P24PFS; /* 0x35: Port 2 Pin 4 Function Select */
  volatile uint8_t P25PFS; /* 0x36: Port 2 Pin 5 Function Select */
  volatile uint8_t P26PFS; /* 0x37: Port 2 Pin 6 Function Select */
  volatile uint8_t P27PFS; /* 0x38: Port 2 Pin 7 Function Select */
  volatile uint8_t P30PFS; /* 0x39: Port 3 Pin 0 Function Select */
  volatile uint8_t P31PFS; /* 0x3A: Port 3 Pin 1 Function Select */
  volatile uint8_t P32PFS; /* 0x3B: Port 3 Pin 2 Function Select */
  volatile uint8_t P33PFS; /* 0x3C: Port 3 Pin 3 Function Select */
  volatile uint8_t P34PFS; /* 0x3D: Port 3 Pin 4 Function Select */
  volatile uint8_t P40PFS; /* 0x3E: Port 4 Pin 0 Function Select */
  volatile uint8_t P41PFS; /* 0x3F: Port 4 Pin 1 Function Select */
  volatile uint8_t P42PFS; /* 0x40: Port 4 Pin 2 Function Select */
  volatile uint8_t P43PFS; /* 0x41: Port 4 Pin 3 Function Select */
  volatile uint8_t P44PFS; /* 0x42: Port 4 Pin 4 Function Select */
  volatile uint8_t P45PFS; /* 0x43: Port 4 Pin 5 Function Select */
  volatile uint8_t P46PFS; /* 0x44: Port 4 Pin 6 Function Select */
  volatile uint8_t P47PFS; /* 0x45: Port 4 Pin 7 Function Select */
  volatile uint8_t P50PFS; /* 0x46: Port 5 Pin 0 Function Select */
  volatile uint8_t P51PFS; /* 0x47: Port 5 Pin 1 Function Select */
  volatile uint8_t P52PFS; /* 0x48: Port 5 Pin 2 Function Select */
  volatile uint8_t P53PFS; /* 0x49: Port 5 Pin 3 Function Select */
  volatile uint8_t P54PFS; /* 0x4A: Port 5 Pin 4 Function Select */
  volatile uint8_t P55PFS; /* 0x4B: Port 5 Pin 5 Function Select */
  volatile uint8_t P56PFS; /* 0x4C: Port 5 Pin 6 Function Select */
  volatile uint8_t P57PFS; /* 0x4D: Port 5 Pin 7 Function Select */
  /* Note: Additional port PFS registers continue for all ports */
  /* Simplified for common motor control pins */
} rx_mpc_regs_t;

#define MPC_BASE ((rx_mpc_regs_t*)0x0008C100)
#define MPC      (*MPC_BASE)

/* MPC Write Protect Register (PWPR) bits */
typedef enum {
  k_mpc_pwpr_pfswe = (1 << 6), /* PFS Write Enable */
  k_mpc_pwpr_b0wi  = (1 << 7), /* PFSWE Bit Write Disable */
} mpc_pwpr_bits_t;

/* PFS Register bits */
typedef enum {
  k_pfs_psel_mask = 0x1F,     /* Peripheral Select mask (bits 0-4) */
  k_pfs_isel      = (1 << 6), /* Interrupt Input Select */
  k_pfs_asel      = (1 << 7), /* Analog Input Select */
} pfs_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MPC_REGS_H */
