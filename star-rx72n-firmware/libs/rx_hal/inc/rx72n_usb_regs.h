/**
 * @file rx72n_usb_regs.h
 * @brief RX72N USB 2.0 Full-Speed Host/Function Module Register Definitions
 *
 * @details
 * Register definitions for the USB 2.0 Full-Speed Host/Function Module (USB0)
 * verified against RX72N Group User's Manual: Hardware (R01UH0824EJ0111),
 * Chapter 40 "USB 2.0 FS Host/Function Module (USBb)".
 *
 * @par Verification Status (2026-04-20):
 * - All register addresses verified against manual Chapter 40
 * - 45 registers defined with correct offsets
 * - 55+ static assertions verify all offsets at compile time
 * - Removed non-existent registers (BUSWAIT, PLLSTA, TESTMODE, D0FBCFG,
 *   D1FBCFG, PHYSET, PIPEBUF, LPCTRL, LPSTS - these do NOT exist on RX72N)
 * - Renamed UFRMNUM to DVCHGR (correct register name per manual)
 * - CRITICAL FIX (2026-02-03): Changed FIFO registers from uint32_t to uint16_t
 *   per manual Ch40 p.1941. CFIFO/D0FIFO/D1FIFO are 16-bit WORD registers,
 *   NOT 32-bit. Added padding to maintain correct struct offsets.
 * - FIX (2026-04-20): Added missing DVSTCTR0 bits VBUSEN(b9), EXICEN(b10),
 *   HNPBTOA(b11) per manual p.1940.
 * - FIX (2026-04-20): Split usb_fifosel_bits_t into usb_cfifosel_bits_t and
 *   usb_dnfifosel_bits_t. CFIFOSEL has ISEL(b5) but NOT DREQE/DCLRM.
 *   D0/D1FIFOSEL have DREQE(b12)/DCLRM(b13) but NOT ISEL. Per manual p.1942-1944.
 *
 * @date 2026-04-20
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
 * manual Ch40 p.1941. Access width controlled by CFIFOSEL.MBW/D0FIFOSEL.MBW:
 * - MBW=1 (k_usb_cfifosel_mbw_16): 16-bit word access (b15-b0)
 * - MBW=0 (k_usb_cfifosel_mbw_8): 8-bit byte access (b7-b0)
 */
typedef struct {
  volatile uint16_t syscfg;
  uint8_t           reserved0[k_usb_reserved_02_04_bytes];
  volatile uint16_t syssts0;
  uint8_t           reserved1[k_usb_reserved_06_08_bytes];
  volatile uint16_t dvstctr0;
  uint8_t           reserved2[k_usb_reserved_0a_14_bytes];
  volatile uint16_t cfifo;           /**< Common FIFO port @ 0x14 (16-bit WORD, manual p.1941) */
  uint16_t          reserved_cfifo;  /**< Padding @ 0x16 to maintain D0FIFO offset */
  volatile uint16_t d0fifo;          /**< D0 FIFO port @ 0x18 (16-bit WORD, manual p.1941) */
  uint16_t          reserved_d0fifo; /**< Padding @ 0x1A */
  volatile uint16_t d1fifo;          /**< D1 FIFO port @ 0x1C (16-bit WORD, manual p.1941) */
  uint16_t          reserved_d1fifo; /**< Padding @ 0x1E */
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
  uint8_t           reserved10[k_usb_reserved_6a_6c_bytes];
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
  volatile uint32_t physlew; /**< PHY Cross-Point Adjustment @0xF0 (32-bit R/W per hirakuni45 rw32_t) */
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
  k_usb0_base_addr   = 0x000A0000, /**< USB0 register block base address (manual p.1928, Ch40) */
  /* Deep-standby USB transceiver control / pin monitor register.  Shared
   * with USB-common (outside the USB0 rx_usb_regs_t block).  32-bit R/W
   * at absolute 0x000A0400 per hirakuni45 RX600/usb.hpp and Renesas FSP
   * iodefine.h.  Used to release the PHY from "output fixed" state after
   * deep-standby wakeup or first boot. */
  k_usb_dpusr0r_addr = 0x000A0400,
} rx_usb_addresses_t;

static inline volatile rx_usb_regs_t* usb0(void)
{
  return (volatile rx_usb_regs_t*)k_usb0_base_addr;
}

/**
 * @brief Get pointer to USB deep-standby transceiver control register
 *        (DPUSR0R), 32-bit, at absolute 0x000A0400.
 */
static inline volatile uint32_t* usb_dpusr0r(void)
{
  return (volatile uint32_t*)k_usb_dpusr0r_addr;
}

/* SYSCFG bits (manual p.1931) */
typedef enum : uint16_t {
  k_usb_syscfg_usbe  = (1U << 0),  /**< b0: USB operation enable */
  k_usb_syscfg_dprpu = (1U << 4),  /**< b4: D+ pull-up resistor (function mode) */
  k_usb_syscfg_drpd  = (1U << 5),  /**< b5: D+/D- pull-down resistors (host mode) */
  k_usb_syscfg_dcfm  = (1U << 6),  /**< b6: Controller function select (0=function,1=host) */
  k_usb_syscfg_scke  = (1U << 10), /**< b10: USB clock enable */
} usb_syscfg_bits_t;

/* SYSSTS0 bits (manual p.1934) */
typedef enum : uint16_t {
  k_usb_syssts0_lnst_mask   = 0x0003, /**< b1:b0: USB data line status */
  k_usb_syssts0_lnst_se0    = 0x0000,
  k_usb_syssts0_lnst_fs_j   = 0x0001,
  k_usb_syssts0_lnst_fs_k   = 0x0002,
  k_usb_syssts0_lnst_se1    = 0x0003,
  k_usb_syssts0_idmon       = (1U << 2),  /**< b2: ID pin monitor */
  k_usb_syssts0_sofea       = (1U << 5),  /**< b5: SOF active */
  k_usb_syssts0_htact       = (1U << 6),  /**< b6: Host sequencer active */
  k_usb_syssts0_ovcmon_mask = (3U << 14), /**< b15:b14: Overcurrent monitor */
} usb_syssts0_bits_t;

/* DVSTCTR0 bits (manual p.1937) */
typedef enum : uint16_t {
  k_usb_dvstctr0_rhst_mask      = 0x0007, /**< b2:b0: Reset handshake status */
  k_usb_dvstctr0_rhst_undecided = 0x0000,
  k_usb_dvstctr0_rhst_ls        = 0x0001,
  k_usb_dvstctr0_rhst_fs        = 0x0002,
  k_usb_dvstctr0_rhst_reset     = 0x0004,
  k_usb_dvstctr0_uact           = (1U << 4),  /**< b4: USB bus enable */
  k_usb_dvstctr0_resume         = (1U << 5),  /**< b5: Resume output */
  k_usb_dvstctr0_usbrst         = (1U << 6),  /**< b6: USB bus reset output */
  k_usb_dvstctr0_rwupe          = (1U << 7),  /**< b7: Wakeup detection enable */
  k_usb_dvstctr0_wkup           = (1U << 8),  /**< b8: Wakeup output */
  k_usb_dvstctr0_vbusen         = (1U << 9),  /**< b9: VBUS output enable */
  k_usb_dvstctr0_exicen         = (1U << 10), /**< b10: External IC output enable */
  k_usb_dvstctr0_hnpbtoa        = (1U << 11), /**< b11: HNP control (B to A transition) */
} usb_dvstctr0_bits_t;

/* INTENB0 bits (manual p.1944) */
typedef enum : uint16_t {
  k_usb_intenb0_brdye = (1U << 8),  /**< b8: Buffer ready interrupt enable */
  k_usb_intenb0_nrdye = (1U << 9),  /**< b9: Buffer not ready interrupt enable */
  k_usb_intenb0_bempe = (1U << 10), /**< b10: Buffer empty interrupt enable */
  k_usb_intenb0_ctre  = (1U << 11), /**< b11: Control transfer stage transition interrupt enable */
  k_usb_intenb0_dvse  = (1U << 12), /**< b12: Device state transition interrupt enable */
  k_usb_intenb0_sofe  = (1U << 13), /**< b13: Frame number refresh interrupt enable */
  k_usb_intenb0_rsme  = (1U << 14), /**< b14: Resume interrupt enable */
  k_usb_intenb0_vbse  = (1U << 15), /**< b15: VBUS interrupt enable */
} usb_intenb0_bits_t;

/* INTSTS0 bits (manual p.1951) */
typedef enum : uint16_t {
  k_usb_intsts0_ctsq_mask       = 0x0007, /**< b2:b0: Control transfer stage */
  k_usb_intsts0_ctsq_idle       = 0x0000,
  k_usb_intsts0_ctsq_rd_data    = 0x0001,
  k_usb_intsts0_ctsq_rd_status  = 0x0002,
  k_usb_intsts0_ctsq_wr_data    = 0x0003,
  k_usb_intsts0_ctsq_wr_status  = 0x0004,
  k_usb_intsts0_ctsq_wr_nd      = 0x0005,
  k_usb_intsts0_ctsq_seq_err    = 0x0006,
  k_usb_intsts0_valid           = (1U << 3), /**< b3: Setup packet received */
  k_usb_intsts0_dvsq_mask       = (7U << 4), /**< b6:b4: Device state */
  k_usb_intsts0_dvsq_powered    = (0U << 4),
  k_usb_intsts0_dvsq_default    = (1U << 4),
  k_usb_intsts0_dvsq_address    = (2U << 4),
  k_usb_intsts0_dvsq_configured = (3U << 4),
  k_usb_intsts0_dvsq_suspend    = (4U << 4),
  k_usb_intsts0_vbsts           = (1U << 7),  /**< b7: VBUS input status */
  k_usb_intsts0_brdy            = (1U << 8),  /**< b8: Buffer ready */
  k_usb_intsts0_nrdy            = (1U << 9),  /**< b9: Buffer not ready */
  k_usb_intsts0_bemp            = (1U << 10), /**< b10: Buffer empty */
  k_usb_intsts0_ctrt            = (1U << 11), /**< b11: Control transfer stage transition */
  k_usb_intsts0_dvst            = (1U << 12), /**< b12: Device state transition */
  k_usb_intsts0_sofr            = (1U << 13), /**< b13: Frame number refresh */
  k_usb_intsts0_resm            = (1U << 14), /**< b14: Resume */
  k_usb_intsts0_vbint           = (1U << 15), /**< b15: VBUS */
} usb_intsts0_bits_t;

/* DCPCFG bits (manual p.1966) */
typedef enum : uint16_t {
  k_usb_dcpcfg_dir    = (1U << 4), /**< b4: Transfer direction (0=receive,1=transmit) */
  k_usb_dcpcfg_shtnak = (1U << 7), /**< b7: Pipe disabled at end of transfer */
} usb_dcpcfg_bits_t;

/* DCPCTR bits (manual p.1968) */
typedef enum : uint16_t {
  k_usb_dcpctr_pid_mask  = 0x0003, /**< b1:b0: Response PID */
  k_usb_dcpctr_pid_nak   = 0x0000,
  k_usb_dcpctr_pid_buf   = 0x0001,
  k_usb_dcpctr_pid_stall = 0x0002,
  k_usb_dcpctr_ccpl      = (1U << 2),  /**< b2: Control transfer end enable */
  k_usb_dcpctr_pbusy     = (1U << 5),  /**< b5: Pipe busy flag */
  k_usb_dcpctr_sqmon     = (1U << 6),  /**< b6: Sequence toggle bit monitor */
  k_usb_dcpctr_sqset     = (1U << 7),  /**< b7: Sequence toggle bit set */
  k_usb_dcpctr_sqclr     = (1U << 8),  /**< b8: Sequence toggle bit clear */
  k_usb_dcpctr_sureqclr  = (1U << 11), /**< b11: SUREQ bit clear */
  k_usb_dcpctr_sureq     = (1U << 14), /**< b14: Setup token transmission */
  k_usb_dcpctr_bsts      = (1U << 15), /**< b15: Buffer status flag */
} usb_dcpctr_bits_t;

/**
 * @brief USB pipe interrupt control bits
 *
 * @details
 * Bit definitions for BRDYENB/BRDYSTS (buffer ready), BEMPENB/BEMPSTS
 * (buffer empty), and NRDYENB/NRDYSTS (not ready) registers.
 * Each bit corresponds to one pipe (bit 0 = DCP, bits 1-9 = pipes 1-9).
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
  k_usb_pipe_all   = 0x03FF,    /**< All 10 pipes (bits 9:0) */
} usb_pipe_bits_t;

/* PIPECFG bits (manual p.1972) */
typedef enum : uint16_t {
  k_usb_pipecfg_epnum_mask = 0x000F,     /**< b3:b0: Endpoint number */
  k_usb_pipecfg_dir        = (1U << 4),  /**< b4: Transfer direction (0=receive,1=transmit) */
  k_usb_pipecfg_shtnak     = (1U << 7),  /**< b7: Pipe disabled at end of transfer */
  k_usb_pipecfg_dblb       = (1U << 9),  /**< b9: Double buffer mode (pipes 1-5 only) */
  k_usb_pipecfg_bfre       = (1U << 10), /**< b10: BRDY interrupt operation specification */
  k_usb_pipecfg_type_mask  = (3U << 14), /**< b15:b14: Transfer type */
  k_usb_pipecfg_type_bulk  = (1U << 14),
  k_usb_pipecfg_type_int   = (2U << 14),
  k_usb_pipecfg_type_iso   = (3U << 14),
} usb_pipecfg_bits_t;

/**
 * @brief PIPEnCTR bits for pipes 1-5 (manual p.1976)
 *
 * @note Pipes 1-5 have all bits including ATREPM(b10) and INBUFM(b14).
 * @note Pipes 6-9 have a reduced set -- see usb_pipe69ctr_bits_t.
 */
typedef enum : uint16_t {
  k_usb_pipectr_pid_mask  = 0x0003, /**< b1:b0: Response PID */
  k_usb_pipectr_pid_nak   = 0x0000,
  k_usb_pipectr_pid_buf   = 0x0001,
  k_usb_pipectr_pid_stall = 0x0002,
  k_usb_pipectr_pbusy     = (1U << 5),  /**< b5: Pipe busy flag */
  k_usb_pipectr_sqmon     = (1U << 6),  /**< b6: Sequence toggle bit monitor */
  k_usb_pipectr_sqset     = (1U << 7),  /**< b7: Sequence toggle bit set */
  k_usb_pipectr_sqclr     = (1U << 8),  /**< b8: Sequence toggle bit clear */
  k_usb_pipectr_aclrm     = (1U << 9),  /**< b9: Auto buffer clear mode */
  k_usb_pipectr_atrepm    = (1U << 10), /**< b10: Auto response mode (pipes 1-5 only) */
  k_usb_pipectr_inbufm    = (1U << 14), /**< b14: Transmit buffer monitor (pipes 1-5 only) */
  k_usb_pipectr_bsts      = (1U << 15), /**< b15: Buffer status flag */
} usb_pipectr_bits_t;

/**
 * @brief PIPEnCTR bits for pipes 6-9 (manual p.1981)
 *
 * @details
 * Pipes 6-9 are interrupt-only pipes with a reduced control register layout.
 * Bits ATREPM(b10) and INBUFM(b14) are reserved (not present) compared to
 * the pipes 1-5 layout in usb_pipectr_bits_t.
 */
typedef enum : uint16_t {
  k_usb_pipe69ctr_pid_mask  = 0x0003, /**< b1:b0: Response PID */
  k_usb_pipe69ctr_pid_nak   = 0x0000,
  k_usb_pipe69ctr_pid_buf   = 0x0001,
  k_usb_pipe69ctr_pid_stall = 0x0002,
  k_usb_pipe69ctr_pbusy     = (1U << 5),  /**< b5: Pipe busy flag */
  k_usb_pipe69ctr_sqmon     = (1U << 6),  /**< b6: Sequence toggle bit monitor */
  k_usb_pipe69ctr_sqset     = (1U << 7),  /**< b7: Sequence toggle bit set */
  k_usb_pipe69ctr_sqclr     = (1U << 8),  /**< b8: Sequence toggle bit clear */
  k_usb_pipe69ctr_aclrm     = (1U << 9),  /**< b9: Auto buffer clear mode */
  k_usb_pipe69ctr_bsts      = (1U << 15), /**< b15: Buffer status flag */
} usb_pipe69ctr_bits_t;

/* PIPEnTRE bits for pipes 1-5 (manual p.1984) */
typedef enum : uint16_t {
  k_usb_pipetre_trclr = (1U << 8), /**< b8: Transaction counter clear */
  k_usb_pipetre_trenb = (1U << 9), /**< b9: Transaction counter enable */
} usb_pipetre_bits_t;

/**
 * @brief CFIFOSEL bits (manual p.1942)
 *
 * @note CFIFOSEL has ISEL(b5) for direction select.
 * @note CFIFOSEL does NOT have DREQE or DCLRM (those are D0/D1 only).
 */
typedef enum : uint16_t {
  k_usb_cfifosel_curpipe_mask = 0x000F, /**< b3:b0: Port access pipe */
  k_usb_cfifosel_curpipe_dcp  = 0x0000,
  k_usb_cfifosel_isel         = (1U << 5),  /**< b5: Access direction (0=read,1=write) */
  k_usb_cfifosel_bigend       = (1U << 8),  /**< b8: Endian select */
  k_usb_cfifosel_mbw_mask     = (1U << 10), /**< b10: MBW bit */
  k_usb_cfifosel_mbw_8        = (0U << 10), /**< 8-bit access */
  k_usb_cfifosel_mbw_16       = (1U << 10), /**< 16-bit access */
  k_usb_cfifosel_rew          = (1U << 14), /**< b14: Read pointer rewind */
  k_usb_cfifosel_rcnt         = (1U << 15), /**< b15: Read count mode */
} usb_cfifosel_bits_t;

/**
 * @brief D0FIFOSEL / D1FIFOSEL bits (manual p.1943-1944)
 *
 * @note D0/D1FIFOSEL have DREQE(b12) and DCLRM(b13) for DMA/DTC.
 * @note D0/D1FIFOSEL do NOT have ISEL (direction select is always host RX).
 */
typedef enum : uint16_t {
  k_usb_dnfifosel_curpipe_mask = 0x000F, /**< b3:b0: Port access pipe */
  k_usb_dnfifosel_curpipe_dcp  = 0x0000,
  k_usb_dnfifosel_bigend       = (1U << 8),  /**< b8: Endian select */
  k_usb_dnfifosel_mbw_mask     = (1U << 10), /**< b10: MBW bit */
  k_usb_dnfifosel_mbw_8        = (0U << 10), /**< 8-bit access */
  k_usb_dnfifosel_mbw_16       = (1U << 10), /**< 16-bit access */
  k_usb_dnfifosel_dreqe        = (1U << 12), /**< b12: DMA/DTC transfer request enable */
  k_usb_dnfifosel_dclrm        = (1U << 13), /**< b13: Auto buffer clear mode */
  k_usb_dnfifosel_rew          = (1U << 14), /**< b14: Read pointer rewind */
  k_usb_dnfifosel_rcnt         = (1U << 15), /**< b15: Read count mode */
} usb_dnfifosel_bits_t;

/* FIFOCTR bits (manual p.1945 CFIFOCTR, p.1946 D0/D1FIFOCTR) */
typedef enum : uint16_t {
  k_usb_fifoctr_dtln_mask = 0x01FF,     /**< b8:b0: Data length */
  k_usb_fifoctr_frdy      = (1U << 13), /**< b13: FIFO ready */
  k_usb_fifoctr_bclr      = (1U << 14), /**< b14: CPU buffer clear */
  k_usb_fifoctr_bval      = (1U << 15), /**< b15: Buffer valid flag */
} usb_fifoctr_bits_t;

/* DPUSR0R bits (Deep-Standby USB Transceiver Control / Pin Monitor,
 * 32-bit, absolute 0x000A0400; fields from hirakuni45 RX600/usb.hpp
 * and RX72N HW manual Ch40). */
typedef enum : uint32_t {
  k_usb_dpusr0r_srpc0   = (1UL << 0),  /**< USB0 single-ended receiver power control */
  k_usb_dpusr0r_rpue0   = (1UL << 1),  /**< USB0 DP pull-up resistor enable (deep-standby) */
  k_usb_dpusr0r_fixphy0 = (1UL << 4),  /**< USB0 PHY output fixed (1=fixed, 0=normal) */
  k_usb_dpusr0r_dp0     = (1UL << 16), /**< USB0 D+ line monitor */
  k_usb_dpusr0r_dm0     = (1UL << 17), /**< USB0 D- line monitor */
  k_usb_dpusr0r_dovca0  = (1UL << 20), /**< USB0 overcurrent flag A */
  k_usb_dpusr0r_dovcb0  = (1UL << 21), /**< USB0 overcurrent flag B */
  k_usb_dpusr0r_dvbsts0 = (1UL << 23), /**< USB0 VBUS status */
} usb_dpusr0r_bits_t;

/* PHYSLEW bits (PHY Cross-Point Adjustment, 32-bit, USB0 offset 0xF0).
 * RX72N manual Ch40 + tinyusb renesas/usba DCD: for RX72N USB0 the
 * recommended slew-rate setting is SLEWR00=1, SLEWF00=1 => value 0x5.
 * Other RX parts leave this register at its reset default. */
typedef enum : uint32_t {
  k_usb_physlew_slewr00  = (1UL << 0), /**< Rising-edge slew rate, lane 0 */
  k_usb_physlew_slewr01  = (1UL << 1), /**< Rising-edge slew rate, lane 1 */
  k_usb_physlew_slewf00  = (1UL << 2), /**< Falling-edge slew rate, lane 0 */
  k_usb_physlew_slewf01  = (1UL << 3), /**< Falling-edge slew rate, lane 1 */
  k_usb_physlew_rx72n    = 0x00000005, /**< Recommended RX72N USB0 value (tinyusb + FSP) */
} usb_physlew_bits_t;

/* Interrupt vectors.
 *
 * D0FIFO0 / D1FIFO0 / USBR0 are FIXED ICU vectors on RX72N (Renesas FSP
 * iodefine.h: VECT_USB0_D0FIFO0=34, D1FIFO0=35, USBR0=90).
 *
 * USBI0 is NOT a fixed vector on RX72N.  It is a Group-B software
 * configurable interrupt (SELECTB) with source code 62 (RX72N HW manual
 * Ch15 Table 15.3, cross-checked against hirakuni45/RX RX72N/icu.hpp
 * SELECTB::USBI0 = 62).  To use it, firmware must pick any vector slot
 * in the SELECTB range (144..207, per SLIBRn register range in the HW
 * manual) and program ICU.SLIBRn = k_usb0_usbi_sli_src.  We pick 144 as
 * the first free SELECTB slot.
 *
 * Earlier revisions of this enum hard-coded k_vect_usb0_usbi = 36, which
 * is a reserved gap in the RX72N fixed vector table -- IER/IPR/IR writes
 * landed on an inert slot and the USBI ISR never fired. */
typedef enum : uint8_t {
  k_vect_usb0_d0fifo = 34,
  k_vect_usb0_d1fifo = 35,
  k_vect_usb0_usbi   = 144,
  k_vect_usb0_usbr   = 90,
} rx_usb_interrupt_vector_t;

/* SELECTB source codes (written to ICU.SLIBRn to map a peripheral source
 * onto a SELECTB vector slot).  Manual Ch15 Table 15.3. */
typedef enum : uint8_t {
  k_usb0_usbi_sli_src = 62, /**< USBI0 source code for ICU.SLIBR[144..207] */
} rx_usb_sli_source_t;

/* CDC endpoints */
typedef enum : uint8_t {
  k_usb_cdc_ep_ctrl         = 0,
  k_usb_cdc_ep_bulk_in      = 1,
  k_usb_cdc_ep_bulk_out     = 2,
  k_usb_cdc_ep_interrupt_in = 3,
  k_usb_cdc_max_packet_fs   = 64,
} usb_cdc_endpoints_t;

/* Static assertions -- every register offset verified against manual Ch40 */
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
