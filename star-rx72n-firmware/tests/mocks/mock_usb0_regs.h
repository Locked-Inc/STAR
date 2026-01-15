/**
 * @file mock_usb0_regs.h
 * @brief Mock USB0/ICU/SYSTEM Register Structures for Host-Side Testing
 *
 * Provides mock register structures that mirror the hardware registers
 * defined in rx72n_regs.h. These allow USB driver code to be tested
 * on the host without actual hardware.
 *
 * Usage:
 * - Include this header in test files
 * - Access g_mock_usb0, g_mock_icu, g_mock_system as if they were hardware
 * - Use helper functions to set up register state for tests
 *
 * @date 2025-12-01
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MOCK_USB0_REGS_H
#define MOCK_USB0_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock USB0 Register Structure
 * Mirrors rx_usb_regs_t from rx72n_regs.h
 * =============================================================================
 */

typedef struct {
  uint16_t syscfg;   /* 0x00: System Configuration Control */
  uint16_t buswait;  /* 0x02: CPU Bus Wait Setting */
  uint16_t syssts0;  /* 0x04: System Configuration Status */
  uint16_t pllsta;   /* 0x06: PLL Status */
  uint16_t dvstctr0; /* 0x08: Device State Control Register 0 */
  uint8_t  reserved0[2];
  uint16_t testmode; /* 0x0C: USB Test Mode */
  uint8_t  reserved1[2];
  uint16_t d0fbcfg;  /* 0x10: D0FIFO Port Configuration */
  uint16_t d1fbcfg;  /* 0x12: D1FIFO Port Configuration */
  uint32_t cfifo;    /* 0x14: CFIFO Port (32-bit access) */
  uint32_t d0fifo;   /* 0x18: D0FIFO Port (32-bit access) */
  uint32_t d1fifo;   /* 0x1C: D1FIFO Port (32-bit access) */
  uint16_t cfifosel; /* 0x20: CFIFO Port Select */
  uint16_t cfifoctr; /* 0x22: CFIFO Port Control */
  uint8_t  reserved2[4];
  uint16_t d0fifosel; /* 0x28: D0FIFO Port Select */
  uint16_t d0fifoctr; /* 0x2A: D0FIFO Port Control */
  uint16_t d1fifosel; /* 0x2C: D1FIFO Port Select */
  uint16_t d1fifoctr; /* 0x2E: D1FIFO Port Control */
  uint16_t intenb0;   /* 0x30: Interrupt Enable Register 0 */
  uint16_t intenb1;   /* 0x32: Interrupt Enable Register 1 */
  uint8_t  reserved3[2];
  uint16_t brdyenb; /* 0x36: BRDY Interrupt Enable */
  uint16_t nrdyenb; /* 0x38: NRDY Interrupt Enable */
  uint16_t bempenb; /* 0x3A: BEMP Interrupt Enable */
  uint16_t sofcfg;  /* 0x3C: SOF Output Configuration */
  uint16_t physet;  /* 0x3E: PHY Setting Register */
  uint16_t intsts0; /* 0x40: Interrupt Status Register 0 */
  uint16_t intsts1; /* 0x42: Interrupt Status Register 1 */
  uint8_t  reserved4[2];
  uint16_t brdysts; /* 0x46: BRDY Interrupt Status */
  uint16_t nrdysts; /* 0x48: NRDY Interrupt Status */
  uint16_t bempsts; /* 0x4A: BEMP Interrupt Status */
  uint16_t frmnum;  /* 0x4C: Frame Number Register */
  uint16_t ufrmnum; /* 0x4E: uFrame Number Register */
  uint16_t usbaddr; /* 0x50: USB Address Register */
  uint8_t  reserved5[2];
  uint16_t usbreq;  /* 0x54: USB Request Type Register */
  uint16_t usbval;  /* 0x56: USB Request Value Register */
  uint16_t usbindx; /* 0x58: USB Request Index Register */
  uint16_t usbleng; /* 0x5A: USB Request Length Register */
  uint16_t dcpcfg;  /* 0x5C: DCP Configuration Register */
  uint16_t dcpmaxp; /* 0x5E: DCP Max Packet Size Register */
  uint16_t dcpctr;  /* 0x60: DCP Control Register */
  uint8_t  reserved6[2];
  uint16_t pipesel; /* 0x64: Pipe Window Select Register */
  uint8_t  reserved7[2];
  uint16_t pipecfg;  /* 0x68: Pipe Configuration Register */
  uint16_t pipebuf;  /* 0x6A: Pipe Buffer Setting Register */
  uint16_t pipemaxp; /* 0x6C: Pipe Max Packet Size Register */
  uint16_t pipeperi; /* 0x6E: Pipe Cycle Control Register */
  uint16_t pipe1ctr; /* 0x70: Pipe 1 Control Register */
  uint16_t pipe2ctr; /* 0x72: Pipe 2 Control Register */
  uint16_t pipe3ctr; /* 0x74: Pipe 3 Control Register */
  uint16_t pipe4ctr; /* 0x76: Pipe 4 Control Register */
  uint16_t pipe5ctr; /* 0x78: Pipe 5 Control Register */
  uint16_t pipe6ctr; /* 0x7A: Pipe 6 Control Register */
  uint16_t pipe7ctr; /* 0x7C: Pipe 7 Control Register */
  uint16_t pipe8ctr; /* 0x7E: Pipe 8 Control Register */
  uint16_t pipe9ctr; /* 0x80: Pipe 9 Control Register */
} mock_usb0_regs_t;

/* =============================================================================
 * Mock ICU Register Structure
 * Mirrors rx_icu_regs_t from rx72n_regs.h (simplified for USB testing)
 * =============================================================================
 */

typedef struct {
  uint8_t ir[256];  /* Interrupt Request Registers */
  uint8_t ier[32];  /* Interrupt Enable Registers */
  uint8_t ipr[256]; /* Interrupt Priority Registers */
} mock_icu_regs_t;

/* =============================================================================
 * Mock SYSTEM Register Structure
 * Mirrors rx_system_regs_t from rx72n_regs.h (subset for USB testing)
 * =============================================================================
 */

typedef struct {
  uint16_t prcr;    /* Protection Register */
  uint32_t mstpcra; /* Module Stop Control Register A */
  uint32_t mstpcrb; /* Module Stop Control Register B */
  uint32_t mstpcrc; /* Module Stop Control Register C */
  uint32_t mstpcrd; /* Module Stop Control Register D */
} mock_system_regs_t;

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

extern mock_usb0_regs_t   g_mock_usb0;
extern mock_icu_regs_t    g_mock_icu;
extern mock_system_regs_t g_mock_system;

/* =============================================================================
 * Inline Accessor Functions for Mock Registers
 * Matches production code pattern (inline accessors instead of macros)
 * =============================================================================
 */

static inline volatile mock_usb0_regs_t* usb0(void)
{
  return &g_mock_usb0;
}

static inline volatile mock_icu_regs_t* icu(void)
{
  return &g_mock_icu;
}

static inline volatile mock_system_regs_t* system_regs(void)
{
  return &g_mock_system;
}

/* =============================================================================
 * USB Register Bit Definitions (from rx72n_regs.h)
 * =============================================================================
 */

typedef enum {
  /* PIPECTR register bits */
  k_usb_pipectr_pid_mask  = 0x0003,    /**< Response PID mask */
  k_usb_pipectr_pid_nak   = 0x0000,    /**< NAK response */
  k_usb_pipectr_pid_buf   = 0x0001,    /**< BUF response */
  k_usb_pipectr_pid_stall = 0x0002,    /**< STALL response */
  k_usb_pipectr_pbusy     = (1 << 5),  /**< Pipe Busy */
  k_usb_pipectr_bsts      = (1 << 15), /**< Buffer Status */

  /* INTSTS0 register bits */
  k_usb_intsts0_ctsq_mask  = 0x0007, /**< Control Transfer Stage mask */
  k_usb_ctsq_max_value     = 0x07,   /**< Maximum valid CTSQ value (3 bits) */
  k_usb_intsts0_dvsq_mask  = 0x0070, /**< Device State mask */
  k_usb_intsts0_dvsq_shift = 4,      /**< Device State bit shift */

  /* CFIFOCTR register bits */
  k_usb_cfifoctr_dtln_mask = 0x01FF,    /**< Data Length mask */
  k_usb_dtln_max_value     = 0x1FF,     /**< Maximum valid DTLN value (9 bits) */
  k_usb_cfifoctr_frdy      = (1 << 13), /**< FIFO Ready flag */

  /* USB default values */
  k_usb_dcpmaxp_default = 64, /**< Default max packet size for control endpoint */
} usb_register_bits_t;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Initialize all mock registers to default values
 */
void mock_regs_init(void);

/**
 * @brief Clear all mock registers to zero
 */
void mock_regs_clear(void);

/**
 * @brief Set USB0 INTSTS0 register value
 */
void mock_usb0_set_intsts0(uint16_t value);

/**
 * @brief Set device state (DVSQ field in INTSTS0)
 *
 * @param dvsq Device state value (0-4 shifted to bit position)
 */
void mock_usb0_set_dvsq(uint16_t dvsq);

/**
 * @brief Set control transfer stage (CTSQ field in INTSTS0)
 *
 * @param ctsq Control stage value (0-6)
 */
void mock_usb0_set_ctsq(uint16_t ctsq);

/**
 * @brief Set FIFO ready status in CFIFOCTR
 *
 * @param ready True to set FRDY bit, false to clear
 */
void mock_usb0_set_fifo_ready(uint8_t ready);

/**
 * @brief Set data length in CFIFOCTR
 *
 * @param len Data length (bits 0-8)
 */
void mock_usb0_set_fifo_dtln(uint16_t len);

/**
 * @brief Set Pipe 1 busy status
 *
 * @param busy True to set PBUSY bit, false to clear
 */
void mock_usb0_set_pipe1_busy(uint8_t busy);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_USB0_REGS_H */
