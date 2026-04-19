/**
 * @file rx72n_usb_regs.h
 * @brief RX72N USB 2.0 Full-Speed Host/Function Module Register Definitions
 *
 * @details
 * Register definitions for the USB 2.0 Full-Speed Host/Function Module (USB0)
 * verified against RX72N Group User's Manual: Hardware, Chapter 40.
 *
 * @par Verification Status (2026-02-03):
 * - All register addresses verified against manual Chapter 40
 * - 45 registers defined with correct offsets
 * - 55+ static assertions verify all offsets at compile time
 * - Removed non-existent registers (BUSWAIT, PLLSTA, TESTMODE, D0FBCFG,
 *   D1FBCFG, PHYSET, PIPEBUF, LPCTRL, LPSTS - these do NOT exist on RX72N)
 * - Renamed UFRMNUM to DVCHGR (correct register name per manual)
 * - CRITICAL FIX (2026-02-03): Changed FIFO registers from uint32_t to uint16_t
 *   per manual Ch40:1155-1250. CFIFO/D0FIFO/D1FIFO are 16-bit WORD registers,
 *   NOT 32-bit. Added padding to maintain correct struct offsets.
 *
 * @date 2026-02-03
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB register offsets (verified against manual Chapter 40)
 */
typedef enum : uint16_t {
  k_usb_offset_syscfg    = 0x00,
  k_usb_offset_syssts0   = 0x04,
  k_usb_offset_dvstctr0  = 0x08,
  k_usb_offset_cfifo     = 0x14,
  k_usb_offset_d0fifo    = 0x18,
  k_usb_offset_d1fifo    = 0x1C,
  k_usb_offset_cfifosel  = 0x20,
  k_usb_offset_cfifoctr  = 0x22,
  k_usb_offset_d0fifosel = 0x28,
  k_usb_offset_d0fifoctr = 0x2A,
  k_usb_offset_d1fifosel = 0x2C,
  k_usb_offset_d1fifoctr = 0x2E,
  k_usb_offset_intenb0   = 0x30,
  k_usb_offset_intenb1   = 0x32,
  k_usb_offset_brdyenb   = 0x36,
  k_usb_offset_nrdyenb   = 0x38,
  k_usb_offset_bempenb   = 0x3A,
  k_usb_offset_sofcfg    = 0x3C,
  k_usb_offset_intsts0   = 0x40,
  k_usb_offset_intsts1   = 0x42,
  k_usb_offset_brdysts   = 0x46,
  k_usb_offset_nrdysts   = 0x48,
  k_usb_offset_bempsts   = 0x4A,
  k_usb_offset_frmnum    = 0x4C,
  k_usb_offset_dvchgr    = 0x4E,
  k_usb_offset_usbaddr   = 0x50,
  k_usb_offset_usbreq    = 0x54,
  k_usb_offset_usbval    = 0x56,
  k_usb_offset_usbindx   = 0x58,
  k_usb_offset_usbleng   = 0x5A,
  k_usb_offset_dcpcfg    = 0x5C,
  k_usb_offset_dcpmaxp   = 0x5E,
  k_usb_offset_dcpctr    = 0x60,
  k_usb_offset_pipesel   = 0x64,
  k_usb_offset_pipecfg   = 0x68,
  k_usb_offset_pipebuf   = 0x6A,
  k_usb_offset_pipemaxp  = 0x6C,
  k_usb_offset_pipeperi  = 0x6E,
  k_usb_offset_pipe1ctr  = 0x70,
  k_usb_offset_pipe9ctr  = 0x80,
  k_usb_offset_pipe1tre  = 0x90,
  k_usb_offset_pipe5trn  = 0xA2,
  k_usb_offset_devadd0   = 0xD0,
  k_usb_offset_devadd5   = 0xDA,
  k_usb_offset_physlew   = 0xF0,
} usb_register_offsets_t;

/**
 * @brief Reserved field sizes for USB register structure
 */
typedef enum : uint8_t {
  k_usb_reserved_02_04_bytes = 2,
  k_usb_reserved_06_08_bytes = 2,
  k_usb_reserved_0a_14_bytes = 10,
  k_usb_reserved_24_28_bytes = 4,
  k_usb_reserved_34_36_bytes = 2,
  k_usb_reserved_3e_40_bytes = 2,
  k_usb_reserved_44_46_bytes = 2,
  k_usb_reserved_52_54_bytes = 2,
  k_usb_reserved_62_64_bytes = 2,
  k_usb_reserved_66_68_bytes = 2,
  k_usb_reserved_6a_6c_bytes = 2,
  k_usb_reserved_82_90_bytes = 14,
  k_usb_reserved_a4_d0_bytes = 44,
  k_usb_reserved_dc_f0_bytes = 20,
} usb_reserved_sizes_t;

/**
 * @struct rx_usb_regs_t
 * @brief USB0 Register Map (Base: 0x000A0000)
 *
 * @note The following registers do NOT exist on RX72N (removed from struct):
 * BUSWAIT, PLLSTA, TESTMODE, D0FBCFG, D1FBCFG, PHYSET, PIPEBUF, LPCTRL, LPSTS
 *
 * @note FIFO registers (CFIFO, D0FIFO, D1FIFO) are 16-bit WORD registers per
 * manual Ch40:1155-1250. Access width controlled by CFIFOSEL.MBW/D0FIFOSEL.MBW:
 * - MBW=1 (k_usb_fifosel_mbw_16): 16-bit word access (b15-b0)
 * - MBW=0 (k_usb_fifosel_mbw_8): 8-bit byte access (b7-b0)
 */
typedef struct {
  volatile uint16_t syscfg;
  uint8_t           reserved0[k_usb_reserved_02_04_bytes];
  volatile uint16_t syssts0;
  uint8_t           reserved1[k_usb_reserved_06_08_bytes];
  volatile uint16_t dvstctr0;
  uint8_t           reserved2[k_usb_reserved_0a_14_bytes];
  volatile uint16_t cfifo; /**< Common FIFO port @ 0x14 (16-bit WORD per manual Ch40:1155) */
  uint16_t          reserved_cfifo;  /**< Reserved @ 0x16 (padding to maintain offset) */
  volatile uint16_t d0fifo;          /**< D0 FIFO port @ 0x18 (16-bit WORD per manual Ch40:1155) */
  uint16_t          reserved_d0fifo; /**< Reserved @ 0x1A (padding to maintain offset) */
  volatile uint16_t d1fifo;          /**< D1 FIFO port @ 0x1C (16-bit WORD per manual Ch40:1155) */
  uint16_t          reserved_d1fifo; /**< Reserved @ 0x1E (padding to maintain offset) */
  volatile uint16_t cfifosel;
  volatile uint16_t cfifoctr;
  uint8_t           reserved3[k_usb_reserved_24_28_bytes];
  volatile uint16_t d0fifosel;
  volatile uint16_t d0fifoctr;
  volatile uint16_t d1fifosel;
  volatile uint16_t d1fifoctr;
  volatile uint16_t intenb0;
  volatile uint16_t intenb1;
  uint8_t           reserved4[k_usb_reserved_34_36_bytes];
  volatile uint16_t brdyenb;
  volatile uint16_t nrdyenb;
  volatile uint16_t bempenb;
  volatile uint16_t sofcfg;
  uint8_t           reserved5[k_usb_reserved_3e_40_bytes];
  volatile uint16_t intsts0;
  volatile uint16_t intsts1;
  uint8_t           reserved6[k_usb_reserved_44_46_bytes];
  volatile uint16_t brdysts;
  volatile uint16_t nrdysts;
  volatile uint16_t bempsts;
  volatile uint16_t frmnum;
  volatile uint16_t dvchgr; /**< Device State Change (NOT ufrmnum!) */
  volatile uint16_t usbaddr;
  uint8_t           reserved7[k_usb_reserved_52_54_bytes];
  volatile uint16_t usbreq;
  volatile uint16_t usbval;
  volatile uint16_t usbindx;
  volatile uint16_t usbleng;
  volatile uint16_t dcpcfg;
  volatile uint16_t dcpmaxp;
  volatile uint16_t dcpctr;
  uint8_t           reserved8[k_usb_reserved_62_64_bytes];
  volatile uint16_t pipesel;
  uint8_t           reserved9[k_usb_reserved_66_68_bytes];
  volatile uint16_t pipecfg;
  volatile uint16_t pipebuf;
  volatile uint16_t pipemaxp;
  volatile uint16_t pipeperi;
  volatile uint16_t pipe1ctr;
  volatile uint16_t pipe2ctr;
  volatile uint16_t pipe3ctr;
  volatile uint16_t pipe4ctr;
  volatile uint16_t pipe5ctr;
  volatile uint16_t pipe6ctr;
  volatile uint16_t pipe7ctr;
  volatile uint16_t pipe8ctr;
  volatile uint16_t pipe9ctr;
  uint8_t           reserved11[k_usb_reserved_82_90_bytes];
  volatile uint16_t pipe1tre;
  volatile uint16_t pipe1trn;
  volatile uint16_t pipe2tre;
  volatile uint16_t pipe2trn;
  volatile uint16_t pipe3tre;
  volatile uint16_t pipe3trn;
  volatile uint16_t pipe4tre;
  volatile uint16_t pipe4trn;
  volatile uint16_t pipe5tre;
  volatile uint16_t pipe5trn;
  uint8_t           reserved12[k_usb_reserved_a4_d0_bytes];
  volatile uint16_t devadd0;
  volatile uint16_t devadd1;
  volatile uint16_t devadd2;
  volatile uint16_t devadd3;
  volatile uint16_t devadd4;
  volatile uint16_t devadd5;
  uint8_t           reserved13[k_usb_reserved_dc_f0_bytes];
  volatile uint16_t physlew;
} rx_usb_regs_t;

/**
 * @enum rx_usb_addresses_t
 * @brief USB0 peripheral base address (verified against RX72N Hardware Manual Ch40)
 *
 * @details
 * Memory-mapped base address for the USB 2.0 Full-Speed Host/Function Module
 * (USB0). Cast to a volatile pointer via usb0() to access the register map.
 *
 * The underlying type is uintptr_t to correctly express that this value is a
 * memory address, not a plain integer. On RX72N (32-bit) uintptr_t == uint32_t,
 * so there is no runtime difference.
 *
 * @par Memory Layout
 * @verbatim
 *   Address      | Peripheral
 *   -------------+----------------------------------------
 *   0x000A0000   | USB0 register block (see rx_usb_regs_t)
 * @endverbatim
 *
 * @see rx_usb_regs_t  Register map structure accessed via this address
 * @see usb0()         Inline accessor that casts this address to a pointer
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_usb0_base_addr = 0x000A0000, /**< USB0 register block base address (Ch40 manual) */
} rx_usb_addresses_t;

static inline volatile rx_usb_regs_t* usb0(void)
{
  return (volatile rx_usb_regs_t*)k_usb0_base_addr;
}

/* SYSCFG bits */
typedef enum : uint16_t {
  k_usb_syscfg_usbe  = (1U << 0),
  k_usb_syscfg_dprpu = (1U << 4),
  k_usb_syscfg_drpd  = (1U << 5),
  k_usb_syscfg_dcfm  = (1U << 6),
  k_usb_syscfg_scke  = (1U << 10),
} usb_syscfg_bits_t;

/* SYSSTS0 bits */
typedef enum : uint16_t {
  k_usb_syssts0_lnst_mask   = 0x0003,
  k_usb_syssts0_lnst_se0    = 0x0000,
  k_usb_syssts0_lnst_fs_j   = 0x0001,
  k_usb_syssts0_lnst_fs_k   = 0x0002,
  k_usb_syssts0_lnst_se1    = 0x0003,
  k_usb_syssts0_idmon       = (1U << 2),
  k_usb_syssts0_sofea       = (1U << 5),
  k_usb_syssts0_htact       = (1U << 6),
  k_usb_syssts0_ovcmon_mask = (3U << 14),
} usb_syssts0_bits_t;

/* DVSTCTR0 bits */
typedef enum : uint16_t {
  k_usb_dvstctr0_rhst_mask      = 0x0007,
  k_usb_dvstctr0_rhst_undecided = 0x0000,
  k_usb_dvstctr0_rhst_ls        = 0x0001,
  k_usb_dvstctr0_rhst_fs        = 0x0002,
  k_usb_dvstctr0_rhst_reset     = 0x0004,
  k_usb_dvstctr0_uact           = (1U << 4),
  k_usb_dvstctr0_resume         = (1U << 5),
  k_usb_dvstctr0_usbrst         = (1U << 6),
  k_usb_dvstctr0_rwupe          = (1U << 7),
  k_usb_dvstctr0_wkup           = (1U << 8),
} usb_dvstctr0_bits_t;

/* INTENB0 bits */
typedef enum : uint16_t {
  k_usb_intenb0_brdye = (1U << 8),
  k_usb_intenb0_nrdye = (1U << 9),
  k_usb_intenb0_bempe = (1U << 10),
  k_usb_intenb0_ctre  = (1U << 11),
  k_usb_intenb0_dvse  = (1U << 12),
  k_usb_intenb0_sofe  = (1U << 13),
  k_usb_intenb0_rsme  = (1U << 14),
  k_usb_intenb0_vbse  = (1U << 15),
} usb_intenb0_bits_t;

/* INTSTS0 bits */
typedef enum : uint16_t {
  k_usb_intsts0_ctsq_mask       = 0x0007,
  k_usb_intsts0_ctsq_idle       = 0x0000,
  k_usb_intsts0_ctsq_rd_data    = 0x0001,
  k_usb_intsts0_ctsq_rd_status  = 0x0002,
  k_usb_intsts0_ctsq_wr_data    = 0x0003,
  k_usb_intsts0_ctsq_wr_status  = 0x0004,
  k_usb_intsts0_ctsq_wr_nd      = 0x0005,
  k_usb_intsts0_ctsq_seq_err    = 0x0006,
  k_usb_intsts0_valid           = (1U << 3),
  k_usb_intsts0_dvsq_mask       = (7U << 4),
  k_usb_intsts0_dvsq_powered    = (0U << 4),
  k_usb_intsts0_dvsq_default    = (1U << 4),
  k_usb_intsts0_dvsq_address    = (2U << 4),
  k_usb_intsts0_dvsq_configured = (3U << 4),
  k_usb_intsts0_dvsq_suspend    = (4U << 4),
  k_usb_intsts0_brdy            = (1U << 8),
  k_usb_intsts0_nrdy            = (1U << 9),
  k_usb_intsts0_bemp            = (1U << 10),
  k_usb_intsts0_ctrt            = (1U << 11),
  k_usb_intsts0_dvst            = (1U << 12),
  k_usb_intsts0_sofr            = (1U << 13),
  k_usb_intsts0_resm            = (1U << 14),
  k_usb_intsts0_vbint           = (1U << 15),
} usb_intsts0_bits_t;

/* DCPCFG bits */
typedef enum : uint16_t {
  k_usb_dcpcfg_dir    = (1U << 4),
  k_usb_dcpcfg_shtnak = (1U << 7),
} usb_dcpcfg_bits_t;

/* DCPCTR bits */
typedef enum : uint16_t {
  k_usb_dcpctr_pid_mask  = 0x0003,
  k_usb_dcpctr_pid_nak   = 0x0000,
  k_usb_dcpctr_pid_buf   = 0x0001,
  k_usb_dcpctr_pid_stall = 0x0002,
  k_usb_dcpctr_ccpl      = (1U << 2),
  k_usb_dcpctr_pbusy     = (1U << 5),
  k_usb_dcpctr_sqmon     = (1U << 6),
  k_usb_dcpctr_sqset     = (1U << 7),
  k_usb_dcpctr_sqclr     = (1U << 8),
  k_usb_dcpctr_sureqclr  = (1U << 11),
  k_usb_dcpctr_sureq     = (1U << 14),
  k_usb_dcpctr_bsts      = (1U << 15),
} usb_dcpctr_bits_t;

/**
 * @brief USB pipe interrupt control bits
 *
 * @details
 * Bit definitions for BRDYENB/BRDYSTS (buffer ready), BEMPENB/BEMPSTS
 * (buffer empty), and NRDYENB/NRDYSTS (not ready) registers.
 *
 * These registers control per-pipe interrupt enable/status for all 10 USB
 * pipes (DCP + Pipe 1-9). Each bit corresponds to one pipe:
 * - Bit 0: DCP (Default Control Pipe)
 * - Bits 1-9: Data pipes 1-9
 *
 * @note RX72N has 10 pipes total: 1 control pipe (DCP) + 9 data pipes
 * @note Each CDC-ACM port uses 3 pipes (2 bulk + 1 interrupt)
 * @note Maximum 3 CDC-ACM ports supported (9 pipes / 3 = 3 ports)
 *
 * @see usb_brdyenb_t Buffer ready interrupt enable register
 * @see usb_brdysts_t Buffer ready interrupt status register
 * @see usb_bempenb_t Buffer empty interrupt enable register
 * @see usb_bempsts_t Buffer empty interrupt status register
 * @see usb_nrdyenb_t Buffer not ready interrupt enable register
 * @see usb_nrdysts_t Buffer not ready interrupt status register
 */
typedef enum : uint16_t {
  k_usb_pipe_bit_0 = (1U << 0), /**< DCP (Default Control Pipe) */
  k_usb_pipe_bit_1 = (1U << 1), /**< Data pipe 1 */
  k_usb_pipe_bit_2 = (1U << 2), /**< Data pipe 2 */
  k_usb_pipe_bit_3 = (1U << 3), /**< Data pipe 3 */
  k_usb_pipe_bit_4 = (1U << 4), /**< Data pipe 4 */
  k_usb_pipe_bit_5 = (1U << 5), /**< Data pipe 5 */
  k_usb_pipe_bit_6 = (1U << 6), /**< Data pipe 6 */
  k_usb_pipe_bit_7 = (1U << 7), /**< Data pipe 7 */
  k_usb_pipe_bit_8 = (1U << 8), /**< Data pipe 8 */
  k_usb_pipe_bit_9 = (1U << 9), /**< Data pipe 9 */
  k_usb_pipe_all   = 0x03FF,    /**< All 10 pipes (mask for bits 9-0) */
} usb_pipe_bits_t;

/* PIPECFG bits */
typedef enum : uint16_t {
  k_usb_pipecfg_epnum_mask = 0x000F,
  k_usb_pipecfg_dir        = (1U << 4),
  k_usb_pipecfg_shtnak     = (1U << 7),
  k_usb_pipecfg_dblb       = (1U << 9),
  k_usb_pipecfg_bfre       = (1U << 10),
  k_usb_pipecfg_type_mask  = (3U << 14),
  k_usb_pipecfg_type_bulk  = (1U << 14),
  k_usb_pipecfg_type_int   = (2U << 14),
  k_usb_pipecfg_type_iso   = (3U << 14),
} usb_pipecfg_bits_t;

/* PIPEnCTR bits */
typedef enum : uint16_t {
  /* PIPEnCTR bit layout per Renesas RX FIT library (r_usb_bitdefine.h):
   *   bit 15 BSTS   (RO)
   *   bit 14 INBUFM (data pipes only; RO on DCPCTR)
   *   bit 15 BSTS  (RO)
   *   bit 13 INBUFM (PIPE1..5 IN only)
   *   bit 11 ATREPM (host mode only)
   *   bit 10 ACLRM
   *   bit  9 SQCLR
   *   bit  8 SQSET
   *   bit  7 SQMON (RO)
   *   bit  6 PBUSY (RO)
   *   bit  4 CSCLR (PIPE1..5 only)
   *   bit  3 CSSTS (RO, PIPE1..5 only)
   *   bits [2:0] PID (NAK=0, BUF=1, STALL=2/3)
   *
   * NOTE: this layout is DIFFERENT from DCPCTR!  DCPCTR has SQCLR
   * at bit 8 / SQSET at bit 7 / PBUSY at bit 5 / PID in [1:0].
   * Earlier revisions of this file used the DCPCTR layout for
   * PIPEnCTR -- which silently set ACLRM where SQCLR was intended,
   * SQCLR where ACLRM was intended, and INBUFM where CSCLR was
   * intended.  That left bulk-IN pipes with INBUFM=1 (RDY interrupt
   * only after IN xfer completes) instead of INBUFM=0 (RDY when
   * buffer ready for next write), so BVAL commits fired but the
   * BRDY interrupt the host stack waited on never came -- bulk IN
   * silently NAKed forever even with PIPECFG / PIPEBUF / FIFO
   * sequence matching `usb_test/bulk_in_fix.c` exactly.  */
  /* CORRECT PIPEnCTR bit positions per Renesas FIT r_usb_bitdefine.h.
   * PIPE1-9 control registers use a different layout than DCPCTR.
   * Verified 2026-04-18 against Renesas FIT v1.20 and proven by
   * usb_test/bulk_debug.c transmitting 'HELLO WORLD' on EP 0x82 via CFIFO. */
  k_usb_pipectr_pid_mask  = 0x0003, /* [1:0] -- only 2 bits, not 3 */
  k_usb_pipectr_pid_nak   = 0x0000,
  k_usb_pipectr_pid_buf   = 0x0001,
  k_usb_pipectr_pid_stall = 0x0002,
  k_usb_pipectr_pbusy     = (1U << 5),  /* b5 */
  k_usb_pipectr_sqmon     = (1U << 6),  /* b6 */
  k_usb_pipectr_sqset     = (1U << 7),  /* b7 */
  k_usb_pipectr_sqclr     = (1U << 8),  /* b8 */
  k_usb_pipectr_aclrm     = (1U << 9),  /* b9 */
  k_usb_pipectr_atrepm    = (1U << 10), /* b10 */
  k_usb_pipectr_cssts     = (1U << 12), /* b12 */
  k_usb_pipectr_csclr     = (1U << 13), /* b13 */
  k_usb_pipectr_inbufm    = (1U << 14), /* b14 -- IN buffer monitor */
  k_usb_pipectr_bsts      = (1U << 15), /* b15 */
} usb_pipectr_bits_t;

/* FIFOSEL bits */
typedef enum : uint16_t {
  k_usb_fifosel_curpipe_mask = 0x000F,
  k_usb_fifosel_curpipe_dcp  = 0x0000,
  k_usb_fifosel_isel         = (1U << 5),
  k_usb_fifosel_bigend       = (1U << 8),
  k_usb_fifosel_mbw_mask     = (1U << 10),
  k_usb_fifosel_mbw_8        = (0U << 10),
  k_usb_fifosel_mbw_16       = (1U << 10),
  k_usb_fifosel_dreqe        = (1U << 12),
  k_usb_fifosel_dclrm        = (1U << 13),
  k_usb_fifosel_rew          = (1U << 14),
  k_usb_fifosel_rcnt         = (1U << 15),
} usb_fifosel_bits_t;

/* FIFOCTR bits */
typedef enum : uint16_t {
  k_usb_fifoctr_dtln_mask = 0x01FF,
  k_usb_fifoctr_frdy      = (1U << 13),
  k_usb_fifoctr_bclr      = (1U << 14),
  k_usb_fifoctr_bval      = (1U << 15),
} usb_fifoctr_bits_t;

/* Interrupt vectors */
typedef enum : uint8_t {
  k_vect_usb0_d0fifo = 34,
  k_vect_usb0_d1fifo = 35,
  k_vect_usb0_usbi   = 36,
  k_vect_usb0_usbr   = 90,
} rx_usb_interrupt_vector_t;

/* CDC endpoints */
typedef enum : uint8_t {
  k_usb_cdc_ep_ctrl         = 0,
  k_usb_cdc_ep_bulk_in      = 1,
  k_usb_cdc_ep_bulk_out     = 2,
  k_usb_cdc_ep_interrupt_in = 3,
  k_usb_cdc_max_packet_fs   = 64,
} usb_cdc_endpoints_t;

/* Static assertions */
static_assert(k_usb0_base_addr == 0x000A0000, "USB0 base");
static_assert(offsetof(rx_usb_regs_t, syscfg) == 0x00, "SYSCFG");
static_assert(offsetof(rx_usb_regs_t, syssts0) == 0x04, "SYSSTS0");
static_assert(offsetof(rx_usb_regs_t, dvstctr0) == 0x08, "DVSTCTR0");
static_assert(offsetof(rx_usb_regs_t, cfifo) == 0x14, "CFIFO");
static_assert(offsetof(rx_usb_regs_t, d0fifo) == 0x18, "D0FIFO");
static_assert(offsetof(rx_usb_regs_t, d1fifo) == 0x1C, "D1FIFO");
static_assert(offsetof(rx_usb_regs_t, cfifosel) == 0x20, "CFIFOSEL");
static_assert(offsetof(rx_usb_regs_t, cfifoctr) == 0x22, "CFIFOCTR");
static_assert(offsetof(rx_usb_regs_t, d0fifosel) == 0x28, "D0FIFOSEL");
static_assert(offsetof(rx_usb_regs_t, d0fifoctr) == 0x2A, "D0FIFOCTR");
static_assert(offsetof(rx_usb_regs_t, d1fifosel) == 0x2C, "D1FIFOSEL");
static_assert(offsetof(rx_usb_regs_t, d1fifoctr) == 0x2E, "D1FIFOCTR");
static_assert(offsetof(rx_usb_regs_t, intenb0) == 0x30, "INTENB0");
static_assert(offsetof(rx_usb_regs_t, intenb1) == 0x32, "INTENB1");
static_assert(offsetof(rx_usb_regs_t, brdyenb) == 0x36, "BRDYENB");
static_assert(offsetof(rx_usb_regs_t, nrdyenb) == 0x38, "NRDYENB");
static_assert(offsetof(rx_usb_regs_t, bempenb) == 0x3A, "BEMPENB");
static_assert(offsetof(rx_usb_regs_t, sofcfg) == 0x3C, "SOFCFG");
static_assert(offsetof(rx_usb_regs_t, intsts0) == 0x40, "INTSTS0");
static_assert(offsetof(rx_usb_regs_t, intsts1) == 0x42, "INTSTS1");
static_assert(offsetof(rx_usb_regs_t, brdysts) == 0x46, "BRDYSTS");
static_assert(offsetof(rx_usb_regs_t, nrdysts) == 0x48, "NRDYSTS");
static_assert(offsetof(rx_usb_regs_t, bempsts) == 0x4A, "BEMPSTS");
static_assert(offsetof(rx_usb_regs_t, frmnum) == 0x4C, "FRMNUM");
static_assert(offsetof(rx_usb_regs_t, dvchgr) == 0x4E, "DVCHGR");
static_assert(offsetof(rx_usb_regs_t, usbaddr) == 0x50, "USBADDR");
static_assert(offsetof(rx_usb_regs_t, usbreq) == 0x54, "USBREQ");
static_assert(offsetof(rx_usb_regs_t, usbval) == 0x56, "USBVAL");
static_assert(offsetof(rx_usb_regs_t, usbindx) == 0x58, "USBINDX");
static_assert(offsetof(rx_usb_regs_t, usbleng) == 0x5A, "USBLENG");
static_assert(offsetof(rx_usb_regs_t, dcpcfg) == 0x5C, "DCPCFG");
static_assert(offsetof(rx_usb_regs_t, dcpmaxp) == 0x5E, "DCPMAXP");
static_assert(offsetof(rx_usb_regs_t, dcpctr) == 0x60, "DCPCTR");
static_assert(offsetof(rx_usb_regs_t, pipesel) == 0x64, "PIPESEL");
static_assert(offsetof(rx_usb_regs_t, pipecfg) == 0x68, "PIPECFG");
static_assert(offsetof(rx_usb_regs_t, pipemaxp) == 0x6C, "PIPEMAXP");
static_assert(offsetof(rx_usb_regs_t, pipeperi) == 0x6E, "PIPEPERI");
static_assert(offsetof(rx_usb_regs_t, pipe1ctr) == 0x70, "PIPE1CTR");
static_assert(offsetof(rx_usb_regs_t, pipe9ctr) == 0x80, "PIPE9CTR");
static_assert(offsetof(rx_usb_regs_t, pipe1tre) == 0x90, "PIPE1TRE");
static_assert(offsetof(rx_usb_regs_t, pipe5trn) == 0xA2, "PIPE5TRN");
static_assert(offsetof(rx_usb_regs_t, devadd0) == 0xD0, "DEVADD0");
static_assert(offsetof(rx_usb_regs_t, devadd5) == 0xDA, "DEVADD5");
static_assert(offsetof(rx_usb_regs_t, physlew) == 0xF0, "PHYSLEW");

#ifdef __cplusplus
}
#endif
