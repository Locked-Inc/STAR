/**
 * @file mock_usb0_regs.h
 * @brief Mock USB0/ICU/SYSTEM Register Structures for Host-Side Testing
 *
 * Provides mock register structures that mirror the hardware registers
 * defined in rx72n_usb_regs.h. These allow USB driver code to be tested
 * on the host without actual hardware.
 *
 * Usage:
 * - Include this header in test files
 * - Access g_mock_usb0, g_mock_icu, g_mock_system as if they were hardware
 * - Use helper functions to set up register state for tests
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_USB0_REGS_H
#define MOCK_USB0_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Reserved Field Size Constants
 * =============================================================================
 */

/** @brief USB register reserved field sizes */
typedef enum {
  k_usb_reserved_after_dvstctr0_bytes = 2,  /**< Reserved bytes after DVSTCTR0 */
  k_usb_reserved_after_testmode_bytes = 2,  /**< Reserved bytes after TESTMODE */
  k_usb_reserved_after_cfifoctr_bytes = 4,  /**< Reserved bytes after CFIFOCTR */
  k_usb_reserved_after_intenb1_bytes  = 2,  /**< Reserved bytes after INTENB1 */
  k_usb_reserved_after_intsts1_bytes  = 2,  /**< Reserved bytes after INTSTS1 */
  k_usb_reserved_after_usbaddr_bytes  = 2,  /**< Reserved bytes after USBADDR */
  k_usb_reserved_after_dcpctr_bytes   = 2,  /**< Reserved bytes after DCPCTR */
  k_usb_reserved_after_pipesel_bytes  = 2,  /**< Reserved bytes after PIPESEL */
  k_usb_reserved_after_pipe9ctr_bytes = 14, /**< Reserved bytes after PIPE9CTR */
} mock_usb_reserved_sizes_t;

/* =============================================================================
 * Mock USB0 Register Structure
 * Mirrors rx_usb_regs_t from rx72n_usb_regs.h
 * =============================================================================
 */

/**
 * @brief Mock USB0 Register Map
 * @details Matches the real rx_usb_regs_t structure for testing
 */
typedef struct {
  volatile uint16_t syscfg;   /**< System Configuration Control */
  volatile uint16_t buswait;  /**< CPU Bus Wait Setting */
  volatile uint16_t syssts0;  /**< System Configuration Status */
  volatile uint16_t pllsta;   /**< PLL Status */
  volatile uint16_t dvstctr0; /**< Device State Control Register 0 */
  uint8_t           reserved0[k_usb_reserved_after_dvstctr0_bytes];
  volatile uint16_t testmode; /**< USB Test Mode */
  uint8_t           reserved1[k_usb_reserved_after_testmode_bytes];
  volatile uint16_t d0fbcfg;  /**< D0FIFO Port Configuration */
  volatile uint16_t d1fbcfg;  /**< D1FIFO Port Configuration */
  volatile uint32_t cfifo;    /**< CFIFO Port (32-bit access) */
  volatile uint32_t d0fifo;   /**< D0FIFO Port (32-bit access) */
  volatile uint32_t d1fifo;   /**< D1FIFO Port (32-bit access) */
  volatile uint16_t cfifosel; /**< CFIFO Port Select */
  volatile uint16_t cfifoctr; /**< CFIFO Port Control */
  uint8_t           reserved2[k_usb_reserved_after_cfifoctr_bytes];
  volatile uint16_t d0fifosel; /**< D0FIFO Port Select */
  volatile uint16_t d0fifoctr; /**< D0FIFO Port Control */
  volatile uint16_t d1fifosel; /**< D1FIFO Port Select */
  volatile uint16_t d1fifoctr; /**< D1FIFO Port Control */
  volatile uint16_t intenb0;   /**< Interrupt Enable Register 0 */
  volatile uint16_t intenb1;   /**< Interrupt Enable Register 1 */
  uint8_t           reserved3[k_usb_reserved_after_intenb1_bytes];
  volatile uint16_t brdyenb; /**< BRDY Interrupt Enable */
  volatile uint16_t nrdyenb; /**< NRDY Interrupt Enable */
  volatile uint16_t bempenb; /**< BEMP Interrupt Enable */
  volatile uint16_t sofcfg;  /**< SOF Output Configuration */
  volatile uint16_t physet;  /**< PHY Setting Register */
  volatile uint16_t intsts0; /**< Interrupt Status Register 0 */
  volatile uint16_t intsts1; /**< Interrupt Status Register 1 */
  uint8_t           reserved4[k_usb_reserved_after_intsts1_bytes];
  volatile uint16_t brdysts; /**< BRDY Interrupt Status */
  volatile uint16_t nrdysts; /**< NRDY Interrupt Status */
  volatile uint16_t bempsts; /**< BEMP Interrupt Status */
  volatile uint16_t frmnum;  /**< Frame Number Register */
  volatile uint16_t ufrmnum; /**< uFrame Number Register */
  volatile uint16_t usbaddr; /**< USB Address Register */
  uint8_t           reserved5[k_usb_reserved_after_usbaddr_bytes];
  volatile uint16_t usbreq;  /**< USB Request Type Register */
  volatile uint16_t usbval;  /**< USB Request Value Register */
  volatile uint16_t usbindx; /**< USB Request Index Register */
  volatile uint16_t usbleng; /**< USB Request Length Register */
  volatile uint16_t dcpcfg;  /**< DCP Configuration Register */
  volatile uint16_t dcpmaxp; /**< DCP Max Packet Size Register */
  volatile uint16_t dcpctr;  /**< DCP Control Register */
  uint8_t           reserved6[k_usb_reserved_after_dcpctr_bytes];
  volatile uint16_t pipesel; /**< Pipe Window Select Register */
  uint8_t           reserved7[k_usb_reserved_after_pipesel_bytes];
  volatile uint16_t pipecfg;  /**< Pipe Configuration Register */
  volatile uint16_t pipebuf;  /**< Pipe Buffer Setting Register */
  volatile uint16_t pipemaxp; /**< Pipe Max Packet Size Register */
  volatile uint16_t pipeperi; /**< Pipe Cycle Control Register */
  volatile uint16_t pipe1ctr; /**< Pipe 1 Control Register */
  volatile uint16_t pipe2ctr; /**< Pipe 2 Control Register */
  volatile uint16_t pipe3ctr; /**< Pipe 3 Control Register */
  volatile uint16_t pipe4ctr; /**< Pipe 4 Control Register */
  volatile uint16_t pipe5ctr; /**< Pipe 5 Control Register */
  volatile uint16_t pipe6ctr; /**< Pipe 6 Control Register */
  volatile uint16_t pipe7ctr; /**< Pipe 7 Control Register */
  volatile uint16_t pipe8ctr; /**< Pipe 8 Control Register */
  volatile uint16_t pipe9ctr; /**< Pipe 9 Control Register */
} mock_usb_regs_t;

/* =============================================================================
 * Mock ICU Register Structure
 * =============================================================================
 */

/** @brief ICU register array sizes */
typedef enum {
  k_mock_icu_ir_count  = 256, /**< Number of IR registers */
  k_mock_icu_ier_count = 32,  /**< Number of IER registers */
  k_mock_icu_ipr_count = 256, /**< Number of IPR registers */
} mock_icu_sizes_t;

/**
 * @brief Mock ICU Register Map (simplified for USB testing)
 */
typedef struct {
  volatile uint8_t ir[k_mock_icu_ir_count];   /**< Interrupt Request Registers */
  volatile uint8_t ier[k_mock_icu_ier_count]; /**< Interrupt Enable Registers */
  volatile uint8_t ipr[k_mock_icu_ipr_count]; /**< Interrupt Priority Registers */
} mock_icu_regs_t;

/* =============================================================================
 * Mock SYSTEM Register Structure
 * =============================================================================
 */

/**
 * @brief Mock SYSTEM Register Map (subset for USB testing)
 */
typedef struct {
  volatile uint16_t prcr;    /**< Protection Register */
  volatile uint32_t mstpcra; /**< Module Stop Control Register A */
  volatile uint32_t mstpcrb; /**< Module Stop Control Register B */
  volatile uint32_t mstpcrc; /**< Module Stop Control Register C */
  volatile uint32_t mstpcrd; /**< Module Stop Control Register D */
} mock_system_regs_t;

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

extern mock_usb_regs_t    g_mock_usb0;
extern mock_icu_regs_t    g_mock_icu;
extern mock_system_regs_t g_mock_system;

/* =============================================================================
 * Inline Accessor Functions for Mock Registers
 * Matches production code pattern (inline accessors instead of macros)
 * =============================================================================
 */

/**
 * @brief Get pointer to mock USB0 registers
 * @return Volatile pointer to mock USB0 register structure
 */
static inline volatile mock_usb_regs_t* usb0(void)
{
  return &g_mock_usb0;
}

/**
 * @brief Get pointer to mock ICU registers
 * @return Volatile pointer to mock ICU register structure
 */
static inline volatile mock_icu_regs_t* icu(void)
{
  return &g_mock_icu;
}

/**
 * @brief Get pointer to mock SYSTEM registers
 * @return Volatile pointer to mock SYSTEM register structure
 */
static inline volatile mock_system_regs_t* system_regs(void)
{
  return &g_mock_system;
}

/* =============================================================================
 * USB0 SYSCFG Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_syscfg_usbe   = (1 << 0),  /**< USB Module Enable */
  k_usb_syscfg_uplle  = (1 << 1),  /**< USB PLL Enable */
  k_usb_syscfg_ucksel = (1 << 2),  /**< USB Clock Select */
  k_usb_syscfg_dprpu  = (1 << 4),  /**< D+ Line Resistor Pull-up Control */
  k_usb_syscfg_drpd   = (1 << 5),  /**< D+/D- Line Resistor Control */
  k_usb_syscfg_dcfm   = (1 << 6),  /**< Controller Function Select (0=Function) */
  k_usb_syscfg_cnen   = (1 << 8),  /**< Single-Ended Receiver Enable */
  k_usb_syscfg_scke   = (1 << 10), /**< USB Clock Enable */
} mock_usb_syscfg_bits_t;

/* =============================================================================
 * USB0 SYSSTS0 Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_syssts0_lnst_mask = 0x0003, /**< USB Data Line Status Mask */
  k_usb_syssts0_lnst_se0  = 0x0000, /**< SE0 (disconnected) */
  k_usb_syssts0_lnst_fs_j = 0x0001, /**< Full-Speed J-state */
  k_usb_syssts0_lnst_fs_k = 0x0002, /**< Full-Speed K-state */
  k_usb_syssts0_lnst_se1  = 0x0003, /**< SE1 (error) */
} mock_usb_syssts0_bits_t;

/* =============================================================================
 * USB0 INTENB0 Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_intenb0_brdye = (1 << 8),  /**< Buffer Ready Interrupt Enable */
  k_usb_intenb0_nrdye = (1 << 9),  /**< Buffer Not Ready Interrupt Enable */
  k_usb_intenb0_bempe = (1 << 10), /**< Buffer Empty Interrupt Enable */
  k_usb_intenb0_ctre  = (1 << 11), /**< Control Transfer Stage Enable */
  k_usb_intenb0_dvse  = (1 << 12), /**< Device State Transition Enable */
  k_usb_intenb0_sofe  = (1 << 13), /**< Frame Number Update Enable */
  k_usb_intenb0_rsme  = (1 << 14), /**< Resume Interrupt Enable */
  k_usb_intenb0_vbse  = (1 << 15), /**< VBUS Interrupt Enable */
} mock_usb_intenb0_bits_t;

/* =============================================================================
 * USB0 INTSTS0 Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_intsts0_ctsq_mask       = 0x0007,    /**< Control Transfer Stage Mask */
  k_usb_intsts0_ctsq_idle       = 0x0000,    /**< Idle or setup stage */
  k_usb_intsts0_ctsq_rd_data    = 0x0001,    /**< Control read data stage */
  k_usb_intsts0_ctsq_rd_status  = 0x0002,    /**< Control read status stage */
  k_usb_intsts0_ctsq_wr_data    = 0x0003,    /**< Control write data stage */
  k_usb_intsts0_ctsq_wr_status  = 0x0004,    /**< Control write status stage */
  k_usb_intsts0_ctsq_wr_nd      = 0x0005,    /**< Control write (no data) status */
  k_usb_intsts0_ctsq_seq_err    = 0x0006,    /**< Control sequence error */
  k_usb_intsts0_valid           = (1 << 3),  /**< USB Request Reception */
  k_usb_intsts0_dvsq_mask       = (7 << 4),  /**< Device State Mask */
  k_usb_intsts0_dvsq_powered    = (0 << 4),  /**< Powered state */
  k_usb_intsts0_dvsq_default    = (1 << 4),  /**< Default state */
  k_usb_intsts0_dvsq_address    = (2 << 4),  /**< Address state */
  k_usb_intsts0_dvsq_configured = (3 << 4),  /**< Configured state */
  k_usb_intsts0_dvsq_suspend    = (4 << 4),  /**< Suspended state */
  k_usb_intsts0_brdy            = (1 << 8),  /**< Buffer Ready Interrupt Status */
  k_usb_intsts0_nrdy            = (1 << 9),  /**< Buffer Not Ready Interrupt Status */
  k_usb_intsts0_bemp            = (1 << 10), /**< Buffer Empty Interrupt Status */
  k_usb_intsts0_ctrt            = (1 << 11), /**< Control Transfer Stage Transition */
  k_usb_intsts0_dvst            = (1 << 12), /**< Device State Transition */
  k_usb_intsts0_sofr            = (1 << 13), /**< Frame Number Refresh */
  k_usb_intsts0_resm            = (1 << 14), /**< Resume Interrupt Status */
  k_usb_intsts0_vbint           = (1 << 15), /**< VBUS Interrupt Status */
} mock_usb_intsts0_bits_t;

/* =============================================================================
 * USB0 DCPCTR Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_dcpctr_pid_mask  = 0x0003,   /**< Response PID Mask */
  k_usb_dcpctr_pid_nak   = 0x0000,   /**< NAK response */
  k_usb_dcpctr_pid_buf   = 0x0001,   /**< BUF response (enable) */
  k_usb_dcpctr_pid_stall = 0x0002,   /**< STALL response */
  k_usb_dcpctr_ccpl      = (1 << 2), /**< Control Transfer End */
  k_usb_dcpctr_pbusy     = (1 << 5), /**< Pipe Busy */
  k_usb_dcpctr_sqmon     = (1 << 6), /**< Sequence Toggle Bit Monitor */
  k_usb_dcpctr_sqset     = (1 << 7), /**< Sequence Toggle Bit Set */
  k_usb_dcpctr_sqclr     = (1 << 8), /**< Sequence Toggle Bit Clear */
  k_usb_dcpctr_bsts      = (1 << 15), /**< Buffer Status */
} mock_usb_dcpctr_bits_t;

/* =============================================================================
 * USB0 PIPEnCTR Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_pipectr_pid_mask  = 0x0003,    /**< Response PID Mask */
  k_usb_pipectr_pid_nak   = 0x0000,    /**< NAK response */
  k_usb_pipectr_pid_buf   = 0x0001,    /**< BUF response (enable) */
  k_usb_pipectr_pid_stall = 0x0002,    /**< STALL response */
  k_usb_pipectr_pbusy     = (1 << 5),  /**< Pipe Busy */
  k_usb_pipectr_sqmon     = (1 << 6),  /**< Sequence Toggle Bit Monitor */
  k_usb_pipectr_sqset     = (1 << 7),  /**< Sequence Toggle Bit Set */
  k_usb_pipectr_sqclr     = (1 << 8),  /**< Sequence Toggle Bit Clear */
  k_usb_pipectr_aclrm     = (1 << 9),  /**< Auto Buffer Clear Mode */
  k_usb_pipectr_atrepm    = (1 << 10), /**< Auto Response Mode */
  k_usb_pipectr_inbufm    = (1 << 14), /**< IN Buffer Monitor */
  k_usb_pipectr_bsts      = (1 << 15), /**< Buffer Status */
} mock_usb_pipectr_bits_t;

/* =============================================================================
 * USB0 FIFOCTR Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_fifoctr_dtln_mask = 0x0FFF,    /**< Receive Data Length Mask */
  k_usb_fifoctr_frdy      = (1 << 13), /**< FIFO Port Ready */
  k_usb_fifoctr_bclr      = (1 << 14), /**< CPU Buffer Clear */
  k_usb_fifoctr_bval      = (1 << 15), /**< Buffer Memory Valid */
} mock_usb_fifoctr_bits_t;

/* =============================================================================
 * USB0 FIFOSEL Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_fifosel_curpipe_mask = 0x000F,    /**< Current Pipe Mask */
  k_usb_fifosel_isel         = (1 << 5),  /**< Access Direction (1=write, 0=read) */
  k_usb_fifosel_bigend       = (1 << 8),  /**< Endian Mode (1=big, 0=little) */
  k_usb_fifosel_mbw_32       = (2 << 10), /**< 32-bit access */
} mock_usb_fifosel_bits_t;

/* =============================================================================
 * USB0 PIPECFG Register Bit Definitions
 * =============================================================================
 */

typedef enum {
  k_usb_pipecfg_epnum_mask = 0x000F,    /**< Endpoint Number Mask */
  k_usb_pipecfg_dir        = (1 << 4),  /**< Transfer Direction (1=TX, 0=RX) */
  k_usb_pipecfg_shtnak     = (1 << 7),  /**< Pipe Disabled at End of Transfer */
  k_usb_pipecfg_dblb       = (1 << 9),  /**< Double Buffer Mode */
  k_usb_pipecfg_bfre       = (1 << 10), /**< BRDY Interrupt Operation */
  k_usb_pipecfg_type_mask  = (3 << 14), /**< Transfer Type Mask */
  k_usb_pipecfg_type_bulk  = (1 << 14), /**< Bulk Transfer */
  k_usb_pipecfg_type_int   = (2 << 14), /**< Interrupt Transfer */
  k_usb_pipecfg_type_iso   = (3 << 14), /**< Isochronous Transfer */
} mock_usb_pipecfg_bits_t;

/* =============================================================================
 * USB Interrupt Vector Numbers
 * =============================================================================
 */

typedef enum {
  k_vect_usb0_d0fifo = 34, /**< USB0 D0FIFO interrupt */
  k_vect_usb0_d1fifo = 35, /**< USB0 D1FIFO interrupt */
  k_vect_usb0_usbi   = 36, /**< USB0 USBI interrupt */
  k_vect_usb0_usbr   = 90, /**< USB0 USBR (resume) interrupt */
} mock_usb_interrupt_vector_t;

/* =============================================================================
 * Module Stop Bit Constants
 * =============================================================================
 */

typedef enum {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
} mock_mstpcrb_bits_t;

/* =============================================================================
 * USB CDC-ACM Endpoint Configuration
 * =============================================================================
 */

typedef enum {
  k_usb_cdc_ep_bulk_in      = 1,  /**< EP1: Bulk IN (data to host) */
  k_usb_cdc_ep_bulk_out     = 2,  /**< EP2: Bulk OUT (data from host) */
  k_usb_cdc_ep_interrupt_in = 3,  /**< EP3: Interrupt IN (notifications) */
  k_usb_cdc_max_packet_fs   = 64, /**< Full-Speed max packet size */
} mock_usb_cdc_endpoints_t;

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
 * @param value New INTSTS0 value
 */
void mock_usb0_set_intsts0(uint16_t value);

/**
 * @brief Set device state (DVSQ field in INTSTS0)
 * @param dvsq Device state value (k_usb_intsts0_dvsq_*)
 */
void mock_usb0_set_dvsq(uint16_t dvsq);

/**
 * @brief Set control transfer stage (CTSQ field in INTSTS0)
 * @param ctsq Control stage value (k_usb_intsts0_ctsq_*)
 */
void mock_usb0_set_ctsq(uint16_t ctsq);

/**
 * @brief Set FIFO ready status in CFIFOCTR
 * @param ready 1 to set FRDY bit, 0 to clear
 */
void mock_usb0_set_fifo_ready(uint8_t ready);

/**
 * @brief Set data length in CFIFOCTR
 * @param len Data length (bits 0-11)
 */
void mock_usb0_set_fifo_dtln(uint16_t len);

/**
 * @brief Set Pipe 1 busy status
 * @param busy 1 to set PBUSY bit, 0 to clear
 */
void mock_usb0_set_pipe1_busy(uint8_t busy);

/**
 * @brief Set line status (LNST field in SYSSTS0)
 * @param lnst Line status value (k_usb_syssts0_lnst_*)
 */
void mock_usb0_set_line_status(uint16_t lnst);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_USB0_REGS_H */
