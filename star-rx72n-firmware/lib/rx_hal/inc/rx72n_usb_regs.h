/* lib/rx_hal/inc/rx72n_usb_regs.h */

/**
 * @file rx72n_usb_regs.h
 * @brief RX72N USB Register Definitions
 *
 * Register definitions for the USB 2.0 Full-Speed Host/Function Module (USB0).
 *
 * The RX72N USB0 module supports:
 * - USB 2.0 Full-Speed (12 Mbps) host and function (peripheral) modes
 * - 9 pipes (endpoints) for bulk, interrupt, and isochronous transfers
 * - FIFO buffers for efficient data transfer
 * - CDC-ACM class support for virtual COM port functionality
 *
 * This driver uses USB0 in FUNCTION (peripheral) mode to appear as a
 * CDC-ACM device (/dev/ttyACM0) to the Raspberry Pi 5 host.
 *
 * References:
 * - RX72N Group User's Manual: Hardware, Section 32 (USB 2.0 Module)
 * - Renesas RX Family USB Basic Driver Application Note (R01AN2025)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_USB_REGS_H
#define STAR_RX72N_USB_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * USB 2.0 Full-Speed Host/Function Module (USB0)
 * =============================================================================
 */

/** @brief USB register reserved field sizes */
typedef enum : uint8_t {
  k_usb_reserved_after_dvstctr0_bytes = 2,  /**< Reserved bytes after DVSTCTR0 */
  k_usb_reserved_after_testmode_bytes = 2,  /**< Reserved bytes after TESTMODE */
  k_usb_reserved_after_cfifoctr_bytes = 4,  /**< Reserved bytes after CFIFOCTR */
  k_usb_reserved_after_intenb1_bytes  = 2,  /**< Reserved bytes after INTENB1 */
  k_usb_reserved_after_intsts1_bytes  = 2,  /**< Reserved bytes after INTSTS1 */
  k_usb_reserved_after_usbaddr_bytes  = 2,  /**< Reserved bytes after USBADDR */
  k_usb_reserved_after_dcpctr_bytes   = 2,  /**< Reserved bytes after DCPCTR */
  k_usb_reserved_after_pipesel_bytes  = 2,  /**< Reserved bytes after PIPESEL */
  k_usb_reserved_after_pipe9ctr_bytes = 14, /**< Reserved bytes after PIPE9CTR */
  k_usb_reserved_after_pipe5trn_bytes = 44, /**< Reserved bytes after PIPE5TRN */
  k_usb_reserved_after_devadd5_bytes  = 20, /**< Reserved bytes after DEVADD5 */
  k_usb_reserved_after_physlew_bytes  = 6,  /**< Reserved bytes after PHYSLEW */
} usb_reserved_sizes_t;

/**
 * @brief USB0 Register Map
 * @details
 * USB 2.0 Full-Speed Host/Function Module registers.
 * Supports host and function (peripheral) modes with 9 configurable pipes.
 * Base address: 0x000A0000
 */
typedef struct {
  volatile uint16_t syscfg;   /**< System Configuration Control (enable, mode) */
  volatile uint16_t buswait;  /**< CPU Bus Wait Setting */
  volatile uint16_t syssts0;  /**< System Configuration Status */
  volatile uint16_t pllsta;   /**< PLL Status */
  volatile uint16_t dvstctr0; /**< Device State Control Register 0 */
  uint8_t           reserved0[k_usb_reserved_after_dvstctr0_bytes]; /**< Reserved */
  volatile uint16_t testmode;                                       /**< USB Test Mode */
  uint8_t           reserved1[k_usb_reserved_after_testmode_bytes]; /**< Reserved */
  volatile uint16_t d0fbcfg;  /**< D0FIFO Port Configuration */
  volatile uint16_t d1fbcfg;  /**< D1FIFO Port Configuration */
  volatile uint32_t cfifo;    /**< CFIFO Port (32-bit access) */
  volatile uint32_t d0fifo;   /**< D0FIFO Port (32-bit access) */
  volatile uint32_t d1fifo;   /**< D1FIFO Port (32-bit access) */
  volatile uint16_t cfifosel; /**< CFIFO Port Select */
  volatile uint16_t cfifoctr; /**< CFIFO Port Control */
  uint8_t           reserved2[k_usb_reserved_after_cfifoctr_bytes]; /**< Reserved */
  volatile uint16_t d0fifosel;                                      /**< D0FIFO Port Select */
  volatile uint16_t d0fifoctr;                                      /**< D0FIFO Port Control */
  volatile uint16_t d1fifosel;                                      /**< D1FIFO Port Select */
  volatile uint16_t d1fifoctr;                                      /**< D1FIFO Port Control */
  volatile uint16_t intenb0; /**< Interrupt Enable Register 0 */
  volatile uint16_t intenb1; /**< Interrupt Enable Register 1 */
  uint8_t           reserved3[k_usb_reserved_after_intenb1_bytes]; /**< Reserved */
  volatile uint16_t brdyenb;                                       /**< BRDY Interrupt Enable */
  volatile uint16_t nrdyenb;                                       /**< NRDY Interrupt Enable */
  volatile uint16_t bempenb;                                       /**< BEMP Interrupt Enable */
  volatile uint16_t sofcfg;                                        /**< SOF Output Configuration */
  volatile uint16_t physet;                                        /**< PHY Setting Register */
  volatile uint16_t intsts0; /**< Interrupt Status Register 0 */
  volatile uint16_t intsts1; /**< Interrupt Status Register 1 */
  uint8_t           reserved4[k_usb_reserved_after_intsts1_bytes]; /**< Reserved */
  volatile uint16_t brdysts;                                       /**< BRDY Interrupt Status */
  volatile uint16_t nrdysts;                                       /**< NRDY Interrupt Status */
  volatile uint16_t bempsts;                                       /**< BEMP Interrupt Status */
  volatile uint16_t frmnum;                                        /**< Frame Number Register */
  volatile uint16_t ufrmnum;                                       /**< uFrame Number Register */
  volatile uint16_t usbaddr;                                       /**< USB Address Register */
  uint8_t           reserved5[k_usb_reserved_after_usbaddr_bytes]; /**< Reserved */
  volatile uint16_t usbreq;                                        /**< USB Request Type Register */
  volatile uint16_t usbval;  /**< USB Request Value Register */
  volatile uint16_t usbindx; /**< USB Request Index Register */
  volatile uint16_t usbleng; /**< USB Request Length Register */
  volatile uint16_t dcpcfg;  /**< DCP Configuration Register */
  volatile uint16_t dcpmaxp; /**< DCP Max Packet Size Register */
  volatile uint16_t dcpctr;  /**< DCP Control Register */
  uint8_t           reserved6[k_usb_reserved_after_dcpctr_bytes]; /**< Reserved */
  volatile uint16_t pipesel; /**< Pipe Window Select Register */
  uint8_t           reserved7[k_usb_reserved_after_pipesel_bytes]; /**< Reserved */
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
  uint8_t           reserved8[k_usb_reserved_after_pipe9ctr_bytes]; /**< Reserved */
  volatile uint16_t pipe1tre; /**< Pipe 1 Transaction Counter Enable */
  volatile uint16_t pipe1trn; /**< Pipe 1 Transaction Counter */
  volatile uint16_t pipe2tre; /**< Pipe 2 Transaction Counter Enable */
  volatile uint16_t pipe2trn; /**< Pipe 2 Transaction Counter */
  volatile uint16_t pipe3tre; /**< Pipe 3 Transaction Counter Enable */
  volatile uint16_t pipe3trn; /**< Pipe 3 Transaction Counter */
  volatile uint16_t pipe4tre; /**< Pipe 4 Transaction Counter Enable */
  volatile uint16_t pipe4trn; /**< Pipe 4 Transaction Counter */
  volatile uint16_t pipe5tre; /**< Pipe 5 Transaction Counter Enable */
  volatile uint16_t pipe5trn; /**< Pipe 5 Transaction Counter */
  uint8_t           reserved9[k_usb_reserved_after_pipe5trn_bytes]; /**< Reserved */
  volatile uint16_t devadd0; /**< Device Address 0 Configuration */
  volatile uint16_t devadd1; /**< Device Address 1 Configuration */
  volatile uint16_t devadd2; /**< Device Address 2 Configuration */
  volatile uint16_t devadd3; /**< Device Address 3 Configuration */
  volatile uint16_t devadd4; /**< Device Address 4 Configuration */
  volatile uint16_t devadd5; /**< Device Address 5 Configuration */
  uint8_t           reserved10[k_usb_reserved_after_devadd5_bytes]; /**< Reserved */
  volatile uint16_t physlew; /**< PHY Cross Point Adjustment */
  uint8_t           reserved11[k_usb_reserved_after_physlew_bytes]; /**< Reserved */
  volatile uint16_t lpctrl; /**< Low Power Control Register */
  volatile uint16_t lpsts;  /**< Low Power Status Register */
} rx_usb_regs_t;

/** @brief USB base address */
typedef enum : uint32_t {
  k_usb0_base_addr = 0x000A0000, /**< USB0 register base address */
} rx_usb_addresses_t;

/**
 * @brief Get pointer to USB0 registers
 * @return Volatile pointer to USB0 register structure
 */
static inline volatile rx_usb_regs_t* usb0(void)
{
  return (volatile rx_usb_regs_t*)k_usb0_base_addr;
}

/* USB0 SYSCFG Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_syscfg_usbe   = (1 << 0),  /**< USB Module Enable */
  k_usb_syscfg_uplle  = (1 << 1),  /**< USB PLL Enable */
  k_usb_syscfg_ucksel = (1 << 2),  /**< USB Clock Select */
  k_usb_syscfg_dprpu  = (1 << 4),  /**< D+ Line Resistor Pull-up Control */
  k_usb_syscfg_drpd   = (1 << 5),  /**< D+/D- Line Resistor Control */
  k_usb_syscfg_dcfm   = (1 << 6),  /**< Controller Function Select (0=Function) */
  k_usb_syscfg_cnen   = (1 << 8),  /**< Single-Ended Receiver Enable */
  k_usb_syscfg_scke   = (1 << 10), /**< USB Clock Enable */
} usb_syscfg_bits_t;

/* USB0 SYSSTS0 Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_syssts0_lnst_mask = 0x0003,    /**< USB Data Line Status Mask */
  k_usb_syssts0_lnst_se0  = 0x0000,    /**< SE0 (disconnected) */
  k_usb_syssts0_lnst_fs_j = 0x0001,    /**< Full-Speed J-state */
  k_usb_syssts0_lnst_fs_k = 0x0002,    /**< Full-Speed K-state */
  k_usb_syssts0_lnst_se1  = 0x0003,    /**< SE1 (error) */
  k_usb_syssts0_idmon     = (1 << 2),  /**< External ID0 Pin Monitor */
  k_usb_syssts0_sofea     = (1 << 5),  /**< SOF Active Monitor */
  k_usb_syssts0_htact     = (1 << 6),  /**< USB Host Sequencer Active */
  k_usb_syssts0_ovcmon    = (3 << 14), /**< Overcurrent Monitor */
} usb_syssts0_bits_t;

/* USB0 DVSTCTR0 Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_dvstctr0_rhst_mask      = 0x0007,   /**< USB Bus Reset Status Mask */
  k_usb_dvstctr0_rhst_undecided = 0x00,     /**< Speed undecided */
  k_usb_dvstctr0_rhst_ls        = 0x0001,   /**< Low-Speed connection */
  k_usb_dvstctr0_rhst_fs        = 0x0002,   /**< Full-Speed connection */
  k_usb_dvstctr0_rhst_reset     = 0x0004,   /**< USB bus reset in progress */
  k_usb_dvstctr0_uact           = (1 << 4), /**< USB Bus Enable */
  k_usb_dvstctr0_resume         = (1 << 5), /**< Resume Signal Output */
  k_usb_dvstctr0_usbrst         = (1 << 6), /**< USB Bus Reset Output */
  k_usb_dvstctr0_rwupe          = (1 << 7), /**< Wakeup Detection Enable */
  k_usb_dvstctr0_wkup           = (1 << 8), /**< Wakeup Output */
} usb_dvstctr0_bits_t;

/* USB0 INTENB0 Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_intenb0_brdye = (1 << 8),  /**< Buffer Ready Interrupt Enable */
  k_usb_intenb0_nrdye = (1 << 9),  /**< Buffer Not Ready Interrupt Enable */
  k_usb_intenb0_bempe = (1 << 10), /**< Buffer Empty Interrupt Enable */
  k_usb_intenb0_ctre  = (1 << 11), /**< Control Transfer Stage Enable */
  k_usb_intenb0_dvse  = (1 << 12), /**< Device State Transition Enable */
  k_usb_intenb0_sofe  = (1 << 13), /**< Frame Number Update Enable */
  k_usb_intenb0_rsme  = (1 << 14), /**< Resume Interrupt Enable */
  k_usb_intenb0_vbse  = (1 << 15), /**< VBUS Interrupt Enable */
} usb_intenb0_bits_t;

/* USB0 INTSTS0 Register Bit Definitions */
typedef enum : uint16_t {
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
} usb_intsts0_bits_t;

/* USB0 DCPCFG Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_dcpcfg_shtnak = (1 << 7), /**< Pipe Disabled at End of Transfer */
  k_usb_dcpcfg_dir    = (1 << 4), /**< Transfer Direction (1=TX, 0=RX) */
} usb_dcpcfg_bits_t;

/* USB0 DCPCTR Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_dcpctr_pid_mask  = 0x0003,    /**< Response PID Mask */
  k_usb_dcpctr_pid_nak   = 0x0000,    /**< NAK response */
  k_usb_dcpctr_pid_buf   = 0x0001,    /**< BUF response (enable) */
  k_usb_dcpctr_pid_stall = 0x0002,    /**< STALL response */
  k_usb_dcpctr_ccpl      = (1 << 2),  /**< Control Transfer End */
  k_usb_dcpctr_pbusy     = (1 << 5),  /**< Pipe Busy */
  k_usb_dcpctr_sqmon     = (1 << 6),  /**< Sequence Toggle Bit Monitor */
  k_usb_dcpctr_sqset     = (1 << 7),  /**< Sequence Toggle Bit Set */
  k_usb_dcpctr_sqclr     = (1 << 8),  /**< Sequence Toggle Bit Clear */
  k_usb_dcpctr_sureqclr  = (1 << 11), /**< SUREQ Bit Clear */
  k_usb_dcpctr_sureq     = (1 << 14), /**< Setup Token Transmission */
  k_usb_dcpctr_bsts      = (1 << 15), /**< Buffer Status */
} usb_dcpctr_bits_t;

/* USB0 PIPECFG Register Bit Definitions */
typedef enum : uint16_t {
  k_usb_pipecfg_epnum_mask = 0x000F,    /**< Endpoint Number Mask */
  k_usb_pipecfg_dir        = (1 << 4),  /**< Transfer Direction (1=TX, 0=RX) */
  k_usb_pipecfg_shtnak     = (1 << 7),  /**< Pipe Disabled at End of Transfer */
  k_usb_pipecfg_dblb       = (1 << 9),  /**< Double Buffer Mode */
  k_usb_pipecfg_bfre       = (1 << 10), /**< BRDY Interrupt Operation */
  k_usb_pipecfg_type_mask  = (3 << 14), /**< Transfer Type Mask */
  k_usb_pipecfg_type_bulk  = (1 << 14), /**< Bulk Transfer */
  k_usb_pipecfg_type_int   = (2 << 14), /**< Interrupt Transfer */
  k_usb_pipecfg_type_iso   = (3 << 14), /**< Isochronous Transfer */
} usb_pipecfg_bits_t;

/* USB0 PIPEnCTR Register Bit Definitions (same for all pipes) */
typedef enum : uint16_t {
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
} usb_pipectr_bits_t;

/* USB0 FIFOSEL Register Bit Definitions (CFIFOSEL, D0FIFOSEL, D1FIFOSEL) */
typedef enum : uint16_t {
  k_usb_fifosel_curpipe_mask = 0x000F,    /**< Current Pipe Mask */
  k_usb_fifosel_curpipe_dcp  = 0x0000,    /**< DCP (Default Control Pipe) */
  k_usb_fifosel_isel         = (1 << 5),  /**< Access Direction (1=write, 0=read) */
  k_usb_fifosel_bigend       = (1 << 8),  /**< Endian Mode (1=big, 0=little) */
  k_usb_fifosel_mbw_8        = (0 << 10), /**< 8-bit access */
  k_usb_fifosel_mbw_16       = (1 << 10), /**< 16-bit access */
  k_usb_fifosel_mbw_32       = (2 << 10), /**< 32-bit access */
  k_usb_fifosel_rcl          = (1 << 14), /**< Read Count Mode */
  k_usb_fifosel_frdy         = (1 << 15), /**< FIFO Port Ready */
} usb_fifosel_bits_t;

/* USB0 FIFOCTR Register Bit Definitions (CFIFOCTR, D0FIFOCTR, D1FIFOCTR) */
typedef enum : uint16_t {
  k_usb_fifoctr_dtln_mask = 0x0FFF,    /**< Receive Data Length Mask */
  k_usb_fifoctr_frdy      = (1 << 13), /**< FIFO Port Ready */
  k_usb_fifoctr_bclr      = (1 << 14), /**< CPU Buffer Clear */
  k_usb_fifoctr_bval      = (1 << 15), /**< Buffer Memory Valid */
} usb_fifoctr_bits_t;

/* USB Interrupt Vector Numbers */
typedef enum : uint8_t {
  k_vect_usb0_d0fifo = 34, /**< USB0 D0FIFO interrupt */
  k_vect_usb0_d1fifo = 35, /**< USB0 D1FIFO interrupt */
  k_vect_usb0_usbi   = 36, /**< USB0 USBI interrupt */
  k_vect_usb0_usbr   = 90, /**< USB0 USBR (resume) interrupt */
} rx_usb_interrupt_vector_t;

/* USB CDC-ACM Endpoint Configuration */
typedef enum : uint8_t {
  k_usb_cdc_ep_ctrl         = 0,  /**< EP0: Control endpoint */
  k_usb_cdc_ep_bulk_in      = 1,  /**< EP1: Bulk IN (data to host) */
  k_usb_cdc_ep_bulk_out     = 2,  /**< EP2: Bulk OUT (data from host) */
  k_usb_cdc_ep_interrupt_in = 3,  /**< EP3: Interrupt IN (notifications) */
  k_usb_cdc_max_packet_fs   = 64, /**< Full-Speed max packet size */
} usb_cdc_endpoints_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base address matches Hardware Manual */
_Static_assert(k_usb0_base_addr == 0x000A0000, "USB0 base address incorrect");

/* Verify key register offsets in structure */
_Static_assert(offsetof(rx_usb_regs_t, syscfg) == 0x00, "SYSCFG offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, cfifo) == 0x14, "CFIFO offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, intenb0) == 0x30, "INTENB0 offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, intsts0) == 0x40, "INTSTS0 offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, dcpcfg) == 0x5C, "DCPCFG offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, pipesel) == 0x64, "PIPESEL offset incorrect");
_Static_assert(offsetof(rx_usb_regs_t, pipe1ctr) == 0x70, "PIPE1CTR offset incorrect");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_USB_REGS_H */
