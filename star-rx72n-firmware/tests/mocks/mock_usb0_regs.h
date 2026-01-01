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
 * STAR Project - Texas A&M University
 * December 2025
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
  uint16_t SYSCFG;   /* 0x00: System Configuration Control */
  uint16_t BUSWAIT;  /* 0x02: CPU Bus Wait Setting */
  uint16_t SYSSTS0;  /* 0x04: System Configuration Status */
  uint16_t PLLSTA;   /* 0x06: PLL Status */
  uint16_t DVSTCTR0; /* 0x08: Device State Control Register 0 */
  uint8_t  RESERVED0[2];
  uint16_t TESTMODE; /* 0x0C: USB Test Mode */
  uint8_t  RESERVED1[2];
  uint16_t D0FBCFG;  /* 0x10: D0FIFO Port Configuration */
  uint16_t D1FBCFG;  /* 0x12: D1FIFO Port Configuration */
  uint32_t CFIFO;    /* 0x14: CFIFO Port (32-bit access) */
  uint32_t D0FIFO;   /* 0x18: D0FIFO Port (32-bit access) */
  uint32_t D1FIFO;   /* 0x1C: D1FIFO Port (32-bit access) */
  uint16_t CFIFOSEL; /* 0x20: CFIFO Port Select */
  uint16_t CFIFOCTR; /* 0x22: CFIFO Port Control */
  uint8_t  RESERVED2[4];
  uint16_t D0FIFOSEL; /* 0x28: D0FIFO Port Select */
  uint16_t D0FIFOCTR; /* 0x2A: D0FIFO Port Control */
  uint16_t D1FIFOSEL; /* 0x2C: D1FIFO Port Select */
  uint16_t D1FIFOCTR; /* 0x2E: D1FIFO Port Control */
  uint16_t INTENB0;   /* 0x30: Interrupt Enable Register 0 */
  uint16_t INTENB1;   /* 0x32: Interrupt Enable Register 1 */
  uint8_t  RESERVED3[2];
  uint16_t BRDYENB; /* 0x36: BRDY Interrupt Enable */
  uint16_t NRDYENB; /* 0x38: NRDY Interrupt Enable */
  uint16_t BEMPENB; /* 0x3A: BEMP Interrupt Enable */
  uint16_t SOFCFG;  /* 0x3C: SOF Output Configuration */
  uint16_t PHYSET;  /* 0x3E: PHY Setting Register */
  uint16_t INTSTS0; /* 0x40: Interrupt Status Register 0 */
  uint16_t INTSTS1; /* 0x42: Interrupt Status Register 1 */
  uint8_t  RESERVED4[2];
  uint16_t BRDYSTS; /* 0x46: BRDY Interrupt Status */
  uint16_t NRDYSTS; /* 0x48: NRDY Interrupt Status */
  uint16_t BEMPSTS; /* 0x4A: BEMP Interrupt Status */
  uint16_t FRMNUM;  /* 0x4C: Frame Number Register */
  uint16_t UFRMNUM; /* 0x4E: uFrame Number Register */
  uint16_t USBADDR; /* 0x50: USB Address Register */
  uint8_t  RESERVED5[2];
  uint16_t USBREQ;  /* 0x54: USB Request Type Register */
  uint16_t USBVAL;  /* 0x56: USB Request Value Register */
  uint16_t USBINDX; /* 0x58: USB Request Index Register */
  uint16_t USBLENG; /* 0x5A: USB Request Length Register */
  uint16_t DCPCFG;  /* 0x5C: DCP Configuration Register */
  uint16_t DCPMAXP; /* 0x5E: DCP Max Packet Size Register */
  uint16_t DCPCTR;  /* 0x60: DCP Control Register */
  uint8_t  RESERVED6[2];
  uint16_t PIPESEL;  /* 0x64: Pipe Window Select Register */
  uint8_t  RESERVED7[2];
  uint16_t PIPECFG;  /* 0x68: Pipe Configuration Register */
  uint16_t PIPEBUF;  /* 0x6A: Pipe Buffer Setting Register */
  uint16_t PIPEMAXP; /* 0x6C: Pipe Max Packet Size Register */
  uint16_t PIPEPERI; /* 0x6E: Pipe Cycle Control Register */
  uint16_t PIPE1CTR; /* 0x70: Pipe 1 Control Register */
  uint16_t PIPE2CTR; /* 0x72: Pipe 2 Control Register */
  uint16_t PIPE3CTR; /* 0x74: Pipe 3 Control Register */
  uint16_t PIPE4CTR; /* 0x76: Pipe 4 Control Register */
  uint16_t PIPE5CTR; /* 0x78: Pipe 5 Control Register */
  uint16_t PIPE6CTR; /* 0x7A: Pipe 6 Control Register */
  uint16_t PIPE7CTR; /* 0x7C: Pipe 7 Control Register */
  uint16_t PIPE8CTR; /* 0x7E: Pipe 8 Control Register */
  uint16_t PIPE9CTR; /* 0x80: Pipe 9 Control Register */
} mock_usb0_regs_t;

/* =============================================================================
 * Mock ICU Register Structure
 * Mirrors rx_icu_regs_t from rx72n_regs.h (simplified for USB testing)
 * =============================================================================
 */

typedef struct {
  uint8_t IR[256];  /* Interrupt Request Registers */
  uint8_t IER[32];  /* Interrupt Enable Registers */
  uint8_t IPR[256]; /* Interrupt Priority Registers */
} mock_icu_regs_t;

/* =============================================================================
 * Mock SYSTEM Register Structure
 * Mirrors rx_system_regs_t from rx72n_regs.h (subset for USB testing)
 * =============================================================================
 */

typedef struct {
  uint16_t PRCR;    /* Protection Register */
  uint32_t MSTPCRA; /* Module Stop Control Register A */
  uint32_t MSTPCRB; /* Module Stop Control Register B */
  uint32_t MSTPCRC; /* Module Stop Control Register C */
  uint32_t MSTPCRD; /* Module Stop Control Register D */
} mock_system_regs_t;

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

extern mock_usb0_regs_t   g_mock_usb0;
extern mock_icu_regs_t    g_mock_icu;
extern mock_system_regs_t g_mock_system;

/* =============================================================================
 * Macros to Map Hardware Names to Mock Instances
 * Allows code to use USB0.xxx syntax with mock registers
 * =============================================================================
 */

#define USB0    g_mock_usb0
#define ICU     g_mock_icu
#define SYSTEM  g_mock_system

/* =============================================================================
 * Pipe Control Register Bits (from rx72n_regs.h)
 * =============================================================================
 */

typedef enum {
  k_usb_pipectr_pid_mask  = 0x0003, /* Response PID */
  k_usb_pipectr_pid_nak   = 0x0000, /* NAK response */
  k_usb_pipectr_pid_buf   = 0x0001, /* BUF response */
  k_usb_pipectr_pid_stall = 0x0002, /* STALL response */
  k_usb_pipectr_pbusy     = (1 << 5), /* Pipe Busy */
  k_usb_pipectr_bsts      = (1 << 15), /* Buffer Status */
} usb_pipectr_bits_t;

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
