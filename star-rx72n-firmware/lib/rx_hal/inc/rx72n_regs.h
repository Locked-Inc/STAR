/* include/rx72n_regs.h */

/**
 * @file rx72n_regs.h
 * @brief RX72N Register Definitions
 *
 * Hardware register definitions for Renesas RX72N (R5F572NNHGFP#30).
 * Based on RX72N Group Hardware Manual.
 */

#ifndef STAR_RX72N_REGS_H
#define STAR_RX72N_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * System Control Registers
 * =============================================================================
 */

/* System Control Register (SYSTEM) - Base: 0x00080000 */
typedef struct {
  volatile uint16_t SYSCR0; /* 0x00 - System Control Register 0 */
  volatile uint16_t SYSCR1; /* 0x02 - System Control Register 1 */
  uint8_t           RESERVED0[2];
  volatile uint16_t SBYCR; /* 0x06 - Standby Control Register */
  uint8_t           RESERVED1[2];
  volatile uint16_t PRCR; /* 0x0A - Protection Register */
  uint8_t           RESERVED1A[2];
  volatile uint32_t MSTPCRA; /* 0x0C - Module Stop Control Register A */
  volatile uint32_t MSTPCRB; /* 0x10 - Module Stop Control Register B */
  volatile uint32_t MSTPCRC; /* 0x14 - Module Stop Control Register C */
  volatile uint32_t MSTPCRD; /* 0x18 - Module Stop Control Register D */
  volatile uint32_t SCKCR;   /* 0x1C - System Clock Control Register */
  volatile uint16_t SCKCR2;  /* 0x20 - System Clock Control Register 2 */
  volatile uint16_t SCKCR3;  /* 0x22 - System Clock Control Register 3 */
  volatile uint16_t PLLCR;   /* 0x24 - PLL Control Register */
  volatile uint8_t  PLLCR2;  /* 0x26 - PLL Control Register 2 */
  uint8_t           RESERVED2[5];
  volatile uint8_t  BCKCR; /* 0x2C - External Bus Clock Control Register */
  uint8_t           RESERVED3[1];
  volatile uint8_t  MOSCCR;  /* 0x2E - Main Clock Oscillator Control */
  volatile uint8_t  SOSCCR;  /* 0x2F - Sub-Clock Oscillator Control */
  volatile uint8_t  LOCOCR;  /* 0x30 - Low-Speed On-Chip Oscillator Control */
  volatile uint8_t  ILOCOCR; /* 0x31 - High-Speed On-Chip Oscillator Control */
  volatile uint8_t  HOCOCR;  /* 0x32 - High-Speed On-Chip Oscillator Control */
  volatile uint8_t  HOCOCR2; /* 0x33 - High-Speed On-Chip Oscillator Control 2 */
  uint8_t           RESERVED4[4];
  volatile uint8_t  OSCOVFSR; /* 0x38 - Oscillation Stabilization Flag */
  uint8_t           RESERVED5[3];
  volatile uint8_t  OSTDCR; /* 0x3C - Oscillation Stop Detection Control */
  volatile uint8_t  OSTDSR; /* 0x3D - Oscillation Stop Detection Status */
} SYSTEM_Type;

#define SYSTEM_BASE ((SYSTEM_Type*)0x00080000)
#define SYSTEM      (*SYSTEM_BASE)

/* =============================================================================
 * Port Registers (GPIO)
 * =============================================================================
 */

/* Port Data Register */
typedef struct {
  volatile uint8_t PDR;  /* Port Direction Register */
  volatile uint8_t PODR; /* Port Output Data Register */
  volatile uint8_t PIDR; /* Port Input Data Register */
  volatile uint8_t PMR;  /* Port Mode Register */
  volatile uint8_t ODR0; /* Open Drain Control Register 0 */
  volatile uint8_t ODR1; /* Open Drain Control Register 1 */
  volatile uint8_t PCR;  /* Pull-up Control Register */
  volatile uint8_t DSCR; /* Drive Capacity Control Register */
} PORT_Type;

/* Port base addresses */
#define PORT0_BASE ((PORT_Type*)0x0008C000)
#define PORT1_BASE ((PORT_Type*)0x0008C008)
#define PORT2_BASE ((PORT_Type*)0x0008C010)
#define PORT3_BASE ((PORT_Type*)0x0008C018)
#define PORT4_BASE ((PORT_Type*)0x0008C020)
#define PORT5_BASE ((PORT_Type*)0x0008C028)
#define PORT6_BASE ((PORT_Type*)0x0008C030)
#define PORT7_BASE ((PORT_Type*)0x0008C038)
#define PORT8_BASE ((PORT_Type*)0x0008C040)
#define PORT9_BASE ((PORT_Type*)0x0008C048)
#define PORTA_BASE ((PORT_Type*)0x0008C050)
#define PORTB_BASE ((PORT_Type*)0x0008C058)
#define PORTC_BASE ((PORT_Type*)0x0008C060)
#define PORTD_BASE ((PORT_Type*)0x0008C068)
#define PORTE_BASE ((PORT_Type*)0x0008C070)
#define PORTF_BASE ((PORT_Type*)0x0008C078)
#define PORTG_BASE ((PORT_Type*)0x0008C080)
#define PORTJ_BASE ((PORT_Type*)0x0008C098)

#define PORT0 (*PORT0_BASE)
#define PORT1 (*PORT1_BASE)
#define PORT2 (*PORT2_BASE)
#define PORT3 (*PORT3_BASE)
#define PORT4 (*PORT4_BASE)
#define PORT5 (*PORT5_BASE)
#define PORT6 (*PORT6_BASE)
#define PORT7 (*PORT7_BASE)
#define PORT8 (*PORT8_BASE)
#define PORT9 (*PORT9_BASE)
#define PORTA (*PORTA_BASE)
#define PORTB (*PORTB_BASE)
#define PORTC (*PORTC_BASE)
#define PORTD (*PORTD_BASE)
#define PORTE (*PORTE_BASE)
#define PORTF (*PORTF_BASE)
#define PORTG (*PORTG_BASE)
#define PORTJ (*PORTJ_BASE)

/* =============================================================================
 * 12-bit A/D Converter (S12ADFa) - For Current Sensing
 * =============================================================================
 */

typedef struct {
  volatile uint16_t ADCSR; /* A/D Control/Status Register */
  uint8_t           RESERVED0[2];
  volatile uint16_t ADANSA0; /* A/D Channel Select Register A0 */
  volatile uint16_t ADANSA1; /* A/D Channel Select Register A1 */
  volatile uint16_t ADADS0;  /* A/D-Converted Value Addition/Average Select Register 0 */
  volatile uint16_t ADADS1;  /* A/D-Converted Value Addition/Average Select Register 1 */
  volatile uint8_t  ADADC;   /* A/D-Converted Value Addition/Average Count Select Register */
  uint8_t           RESERVED1[1];
  volatile uint16_t ADCER;   /* A/D Control Extended Register */
  volatile uint16_t ADSTRGR; /* A/D Start Trigger Select Register */
  uint8_t           RESERVED2[4];
  volatile uint16_t ADDR0; /* A/D Data Register 0 */
  volatile uint16_t ADDR1; /* A/D Data Register 1 */
  volatile uint16_t ADDR2; /* A/D Data Register 2 */
  volatile uint16_t ADDR3; /* A/D Data Register 3 */
  volatile uint16_t ADDR4; /* A/D Data Register 4 */
  volatile uint16_t ADDR5; /* A/D Data Register 5 */
  volatile uint16_t ADDR6; /* A/D Data Register 6 */
  volatile uint16_t ADDR7; /* A/D Data Register 7 */
} S12AD_Type;

#define S12AD0_BASE ((S12AD_Type*)0x00089000)
#define S12AD1_BASE ((S12AD_Type*)0x00089100)

#define S12AD0 (*S12AD0_BASE)
#define S12AD1 (*S12AD1_BASE)

/* =============================================================================
 * Serial Communication Interface (SCI) - For UART/Debug
 * =============================================================================
 */

typedef struct {
  volatile uint8_t SMR;  /* Serial Mode Register */
  volatile uint8_t BRR;  /* Bit Rate Register */
  volatile uint8_t SCR;  /* Serial Control Register */
  volatile uint8_t TDR;  /* Transmit Data Register */
  volatile uint8_t SSR;  /* Serial Status Register */
  volatile uint8_t RDR;  /* Receive Data Register */
  volatile uint8_t SCMR; /* Smart Card Mode Register */
  volatile uint8_t SEMR; /* Serial Extended Mode Register */
} SCI_Type;

#define SCI0_BASE ((SCI_Type*)0x0008A000)
#define SCI1_BASE ((SCI_Type*)0x0008A020)
#define SCI2_BASE ((SCI_Type*)0x0008A040)
#define SCI5_BASE ((SCI_Type*)0x0008A0A0)
#define SCI6_BASE ((SCI_Type*)0x0008A0C0)

#define SCI0 (*SCI0_BASE)
#define SCI1 (*SCI1_BASE)
#define SCI2 (*SCI2_BASE)
#define SCI5 (*SCI5_BASE)
#define SCI6 (*SCI6_BASE)

/* =============================================================================
 * I2C Bus Interface (RIIC) - For I2C/SMBUS Communication
 * =============================================================================
 */

typedef struct {
  volatile uint8_t ICCR1; /* I2C Bus Control Register 1 */
  volatile uint8_t ICCR2; /* I2C Bus Control Register 2 */
  volatile uint8_t ICMR1; /* I2C Bus Mode Register 1 */
  volatile uint8_t ICMR2; /* I2C Bus Mode Register 2 */
  volatile uint8_t ICMR3; /* I2C Bus Mode Register 3 */
  volatile uint8_t ICFER; /* I2C Bus Function Enable Register */
  volatile uint8_t ICSER; /* I2C Bus Status Enable Register */
  volatile uint8_t ICIER; /* I2C Bus Interrupt Enable Register */
  volatile uint8_t ICSR1; /* I2C Bus Status Register 1 */
  volatile uint8_t ICSR2; /* I2C Bus Status Register 2 */
  volatile uint8_t SARL0; /* Slave Address Register L0 */
  volatile uint8_t SARU0; /* Slave Address Register U0 */
  volatile uint8_t SARL1; /* Slave Address Register L1 */
  volatile uint8_t SARU1; /* Slave Address Register U1 */
  volatile uint8_t SARL2; /* Slave Address Register L2 */
  volatile uint8_t SARU2; /* Slave Address Register U2 */
  volatile uint8_t ICBRL; /* I2C Bus Bit Rate Register L */
  volatile uint8_t ICBRH; /* I2C Bus Bit Rate Register H */
  volatile uint8_t ICDRT; /* I2C Bus Transmit Data Register */
  volatile uint8_t ICDRR; /* I2C Bus Receive Data Register */
} RIIC_Type;

#define RIIC0_BASE ((RIIC_Type*)0x00088300)
#define RIIC1_BASE ((RIIC_Type*)0x00088320)
#define RIIC2_BASE ((RIIC_Type*)0x00088340)

#define RIIC0 (*RIIC0_BASE)
#define RIIC1 (*RIIC1_BASE)
#define RIIC2 (*RIIC2_BASE)

/* RIIC Control Register 1 (ICCR1) Bit Definitions */
typedef enum {
  k_riic_iccr1_ice      = (1 << 7), /* I2C Bus Interface Enable */
  k_riic_iccr1_iicrst   = (1 << 6), /* I2C Bus Interface Internal Reset */
  k_riic_iccr1_clk_mask = 0x0F,     /* Clock Select Mask (bits 0-3) */
} riic_iccr1_bits_t;

/* RIIC Control Register 2 (ICCR2) Bit Definitions */
typedef enum {
  k_riic_iccr2_bbsy = (1 << 7), /* Bus Busy Detection Flag */
  k_riic_iccr2_mst  = (1 << 6), /* Controller Mode */
  k_riic_iccr2_trx  = (1 << 5), /* Transmit/Receive Mode (1=TX, 0=RX) */
  k_riic_iccr2_sp   = (1 << 3), /* Stop Condition Issue Request */
  k_riic_iccr2_rs   = (1 << 2), /* Restart Condition Issue Request */
  k_riic_iccr2_st   = (1 << 1), /* Start Condition Issue Request */
} riic_iccr2_bits_t;

/* RIIC Status Register 1 (ICSR1) Bit Definitions */
typedef enum {
  k_riic_icsr1_ackbr = (1 << 0), /* ACK Bit Receive Flag */
} riic_icsr1_bits_t;

/* RIIC Status Register 2 (ICSR2) Bit Definitions */
typedef enum {
  k_riic_icsr2_nackf = (1 << 4), /* NACK Detection Flag */
  k_riic_icsr2_stop  = (1 << 3), /* Stop Condition Detection Flag */
  k_riic_icsr2_start = (1 << 2), /* Start Condition Detection Flag */
  k_riic_icsr2_tdre  = (1 << 7), /* Transmit Data Empty Flag */
  k_riic_icsr2_rdrf  = (1 << 1), /* Receive Data Full Flag */
} riic_icsr2_bits_t;

/* =============================================================================
 * Renesas Serial Peripheral Interface (RSPI) - For SPI to RPi5
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  SPCR;   /* SPI Control Register */
  volatile uint8_t  SSLP;   /* SPI Slave Select Polarity Register */
  volatile uint8_t  SPPCR;  /* SPI Pin Control Register */
  volatile uint8_t  SPSR;   /* SPI Status Register */
  volatile uint32_t SPDR;   /* SPI Data Register */
  volatile uint8_t  SPSCR;  /* SPI Sequence Control Register */
  volatile uint8_t  SPSSR;  /* SPI Sequence Status Register */
  volatile uint8_t  SPBR;   /* SPI Bit Rate Register */
  volatile uint8_t  SPDCR;  /* SPI Data Control Register */
  volatile uint8_t  SPCKD;  /* SPI Clock Delay Register */
  volatile uint8_t  SSLND;  /* SPI Slave Select Negation Delay Register */
  volatile uint8_t  SPND;   /* SPI Next-Access Delay Register */
  volatile uint8_t  SPCR2;  /* SPI Control Register 2 */
  volatile uint16_t SPCMD0; /* SPI Command Register 0 */
} RSPI_Type;

#define RSPI0_BASE ((RSPI_Type*)0x000D0000)
#define RSPI1_BASE ((RSPI_Type*)0x000D0100)
#define RSPI2_BASE ((RSPI_Type*)0x000D0200)

#define RSPI0 (*RSPI0_BASE)
#define RSPI1 (*RSPI1_BASE)
#define RSPI2 (*RSPI2_BASE)

/* RSPI Control Register (SPCR) Bit Definitions */
typedef enum {
  k_rspi_spcr_sprie = (1 << 7), /* Receive Interrupt Enable */
  k_rspi_spcr_spe   = (1 << 6), /* SPI Function Enable */
  k_rspi_spcr_sptie = (1 << 5), /* Transmit Interrupt Enable */
  k_rspi_spcr_speie = (1 << 4), /* Error Interrupt Enable */
  k_rspi_spcr_mstr  = (1 << 3), /* Controller/Peripheral Mode (1=Controller, 0=Peripheral) */
  k_rspi_spcr_modfe = (1 << 2), /* Mode Fault Error Detection Enable */
  k_rspi_spcr_txmd  = (1 << 1), /* Transmit Only Mode */
  k_rspi_spcr_spms  = (1 << 0), /* SPI Mode Select */
} rspi_spcr_bits_t;

/* RSPI Status Register (SPSR) Bit Definitions */
typedef enum {
  k_rspi_spsr_sprf  = (1 << 7), /* Receive Buffer Full Flag */
  k_rspi_spsr_sptef = (1 << 5), /* Transmit Buffer Empty Flag */
  k_rspi_spsr_perf  = (1 << 3), /* Parity Error Flag */
  k_rspi_spsr_modf  = (1 << 2), /* Mode Fault Error Flag */
  k_rspi_spsr_idlnf = (1 << 1), /* Idle Flag */
  k_rspi_spsr_ovrf  = (1 << 0), /* Overrun Error Flag */
} rspi_spsr_bits_t;

/* RSPI Pin Control Register (SPPCR) Bit Definitions */
typedef enum {
  k_rspi_sppcr_moife = (1 << 6), /* COPI Idle Fixed Value Enable */
  k_rspi_sppcr_moifv = (1 << 5), /* COPI Idle Fixed Value */
  k_rspi_sppcr_splp  = (1 << 0), /* Loopback Mode */
} rspi_sppcr_bits_t;

/* RSPI Data Control Register (SPDCR) Bit Definitions */
typedef enum {
  k_rspi_spdcr_sprdtd = (1 << 5), /* Receive Data Ready Detection */
  k_rspi_spdcr_splw   = (1 << 4), /* Word Access Mode (1=Word, 0=Byte) */
} rspi_spdcr_bits_t;

/* =============================================================================
 * Compare Match Timer (CMT) - For ThreadX System Tick
 * =============================================================================
 */

typedef struct {
  volatile uint16_t CMCR;  /* Compare Match Timer Control Register */
  volatile uint16_t CMCNT; /* Compare Match Timer Counter */
  volatile uint16_t CMCOR; /* Compare Match Timer Compare Register */
} CMT_Channel_Type;

typedef struct {
  volatile uint16_t CMSTR0; /* Compare Match Timer Start Register 0 */
  volatile uint16_t CMSTR1; /* Compare Match Timer Start Register 1 */
} CMT_Control_Type;

#define CMT0_BASE     ((CMT_Channel_Type*)0x00088000)
#define CMT1_BASE     ((CMT_Channel_Type*)0x00088008)
#define CMT2_BASE     ((CMT_Channel_Type*)0x00088010)
#define CMT3_BASE     ((CMT_Channel_Type*)0x00088018)
#define CMT_CTRL_BASE ((CMT_Control_Type*)0x00088002)

#define CMT0     (*CMT0_BASE)
#define CMT1     (*CMT1_BASE)
#define CMT2     (*CMT2_BASE)
#define CMT3     (*CMT3_BASE)
#define CMT_CTRL (*CMT_CTRL_BASE)

/* =============================================================================
 * Interrupt Controller (ICU)
 * =============================================================================
 */

/* ICU Register Structure */
typedef struct {
  volatile uint8_t  IR[256];    /* 0x000-0x0FF: Interrupt Request Registers */
  volatile uint8_t  DTCER[256]; /* 0x100-0x1FF: DTC Enable Registers */
  volatile uint8_t  IER[32];    /* 0x200-0x21F: Interrupt Enable Registers */
  uint8_t           RESERVED0[192];
  volatile uint8_t  SWINTR;  /* 0x2E0: Software Interrupt Register */
  volatile uint8_t  SWINT2R; /* 0x2E1: Software Interrupt 2 Register */
  uint8_t           RESERVED1[14];
  volatile uint16_t FIR; /* 0x2F0: Fast Interrupt Register */
  uint8_t           RESERVED2[14];
  volatile uint8_t  IPR[256]; /* 0x300-0x3FF: Interrupt Priority Registers */
  volatile uint8_t  DMRSR[8]; /* 0x400-0x407: DMAC Module Start Registers */
  uint8_t           RESERVED3[248];
  volatile uint8_t  IRQCR[16]; /* 0x500-0x50F: IRQ Control Registers */
  uint8_t           RESERVED4[16];
  volatile uint8_t  IRQFLTE[2]; /* 0x520-0x521: IRQ Filter Enable Registers */
  volatile uint16_t IRQFLTC[2]; /* 0x522-0x525: IRQ Filter Clock Select Registers */
  uint8_t           RESERVED5[58];
  volatile uint32_t NMICR;  /* 0x560: NMI Control Register */
  volatile uint8_t  NMIER;  /* 0x564: NMI Enable Register */
  volatile uint8_t  NMISR;  /* 0x565: NMI Status Register */
  volatile uint8_t  NMICLR; /* 0x566: NMI Clear Register */
  volatile uint8_t  NMIFLT; /* 0x567: NMI Filter Control Register */
} ICU_Type;

#define ICU_BASE_ADDR ((ICU_Type*)0x00087000)
#define ICU           (*ICU_BASE_ADDR)

/* Vector numbers */
#define VECT_CMT0_CMI0 28 /* CMT0 compare match interrupt */
#define VECT_CMT1_CMI1 29 /* CMT1 compare match interrupt */

/* Interrupt Priority Levels (0 = disabled, 1-15 = priority) */
#define IPR_LEVEL_DISABLE 0
#define IPR_LEVEL_MIN     1
#define IPR_LEVEL_MAX     15

/* =============================================================================
 * Independent Watchdog Timer (IWDT)
 *
 * The IWDT runs on a dedicated 120 kHz oscillator, independent of the main
 * system clock. If the main clock fails, the IWDT can still reset the chip.
 *
 * Key Features:
 * - 14-bit down counter with dedicated clock source
 * - Configurable timeout: 128ms to 16384ms
 * - Window protection (optional) to detect too-early refresh
 * - Generates reset or NMI on timeout
 *
 * References:
 * - RX72N Group User's Manual: Hardware, Section 25 (IWDT)
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  IWDTRR; /* 0x00: Refresh Register */
  uint8_t           RESERVED0[1];
  volatile uint16_t IWDTCR;  /* 0x02: Control Register */
  volatile uint16_t IWDTSR;  /* 0x04: Status Register */
  volatile uint8_t  IWDTRCR; /* 0x06: Reset Control Register */
  uint8_t           RESERVED1[1];
  volatile uint8_t  IWDTCSTPR; /* 0x08: Count Stop Control Register */
} IWDT_Type;

#define IWDT_BASE ((IWDT_Type*)0x00088030)
#define IWDT      (*IWDT_BASE)

/* IWDT Refresh Register (IWDTRR) - Write sequence to refresh */
typedef enum {
  k_iwdt_refresh_start = 0x00, /* First write value */
  k_iwdt_refresh_end   = 0xFF, /* Second write value */
} iwdt_refresh_sequence_t;

/* IWDT Control Register (IWDTCR) Bit Definitions */
typedef enum {
  /* Timeout Period Select (TOPS) - bits 1:0 */
  k_iwdt_tops_1024  = 0x0000, /* 1024 cycles (~8.5ms at 120kHz) */
  k_iwdt_tops_4096  = 0x0001, /* 4096 cycles (~34ms) */
  k_iwdt_tops_8192  = 0x0002, /* 8192 cycles (~68ms) */
  k_iwdt_tops_16384 = 0x0003, /* 16384 cycles (~137ms) */

  /* Clock Division Ratio (CKS) - bits 7:4 */
  k_iwdt_cks_div_1   = (0x00 << 4), /* IWDTCLK/1 */
  k_iwdt_cks_div_16  = (0x02 << 4), /* IWDTCLK/16 */
  k_iwdt_cks_div_32  = (0x03 << 4), /* IWDTCLK/32 */
  k_iwdt_cks_div_64  = (0x04 << 4), /* IWDTCLK/64 */
  k_iwdt_cks_div_128 = (0x0F << 4), /* IWDTCLK/128 (~1s at 16384 cycles) */
  k_iwdt_cks_div_256 = (0x05 << 4), /* IWDTCLK/256 */

  /* Window Start Position (RPES) - bits 9:8 */
  k_iwdt_rpes_75 = (0x00 << 8), /* 75% (refresh allowed after 25% elapsed) */
  k_iwdt_rpes_50 = (0x01 << 8), /* 50% */
  k_iwdt_rpes_25 = (0x02 << 8), /* 25% */
  k_iwdt_rpes_0  = (0x03 << 8), /* 0% (refresh allowed immediately) */

  /* Window End Position (RPSS) - bits 13:12 */
  k_iwdt_rpss_100 = (0x00 << 12), /* 100% (no early cutoff) */
  k_iwdt_rpss_75  = (0x01 << 12), /* 75% */
  k_iwdt_rpss_50  = (0x02 << 12), /* 50% */
  k_iwdt_rpss_25  = (0x03 << 12), /* 25% */
} iwdt_iwdtcr_bits_t;

/* IWDT Status Register (IWDTSR) Bit Definitions */
typedef enum {
  k_iwdt_sr_cntval_mask = 0x3FFF,    /* Down counter value (bits 13:0) */
  k_iwdt_sr_undff       = (1 << 14), /* Underflow flag (reset occurred) */
  k_iwdt_sr_refef       = (1 << 15), /* Refresh error flag (window violation) */
} iwdt_iwdtsr_bits_t;

/* IWDT Reset Control Register (IWDTRCR) Bit Definitions */
typedef enum {
  k_iwdt_rcr_rstirqs_mask = (1 << 7), /* Reset/Interrupt Select bit mask */
  k_iwdt_rstirqs_reset    = (1 << 7), /* Generate reset on timeout */
  k_iwdt_rstirqs_nmi      = 0x00,     /* Generate NMI on timeout */
} iwdt_iwdtrcr_bits_t;

/* IWDT Count Stop Control Register (IWDTCSTPR) Bit Definitions */
typedef enum {
  k_iwdt_cstpr_slcstp_mask = (1 << 7), /* Sleep Mode Count Stop bit mask */
  k_iwdt_slcstp_stop       = (1 << 7), /* Stop counting during sleep */
  k_iwdt_slcstp_continue   = 0x00,     /* Continue counting during sleep */
} iwdt_iwdtcstpr_bits_t;

/* IWDT Status Register convenience aliases for implementation */
#define k_iwdt_undff k_iwdt_sr_undff
#define k_iwdt_refef k_iwdt_sr_refef

/* =============================================================================
 * CRC Calculator - Hardware CRC-32 Acceleration
 *
 * This project uses only CRC-32 IEEE 802.3 (polynomial 0x04C11DB7),
 * compatible with Go's crc32.ChecksumIEEE().
 *
 * References:
 * - RX72N Group User's Manual: Hardware (CRC Calculator section)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 *
 * NOTE: The hardware supports other polynomials (CRC-8, CRC-16, CRC-CCITT,
 * CRC-32C) but they are not used in this project.
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  CRCCR; /* 0x00 - CRC Control Register */
  uint8_t           RESERVED0[3];
  volatile uint32_t CRCDIR; /* 0x04 - CRC Data Input Register (32-bit) */
  volatile uint32_t CRCDOR; /* 0x08 - CRC Data Output Register (32-bit) */
} CRC_Type;

#define CRC_BASE ((CRC_Type*)0x00088280)
#define CRC      (*CRC_BASE)

/* CRC Control Register (CRCCR) bit definitions - CRC-32 only */
typedef enum {
  k_crc_crccr_gps_crc32 = 0x03,     /* CRC-32 IEEE 802.3 polynomial */
  k_crc_crccr_lms       = (1 << 6), /* LSB/MSB First (1=LSB first, reflected) */
  k_crc_crccr_dorclr    = (1 << 7), /* Data Output Register Clear */
} crc_crccr_bits_t;

/* Module stop bit for CRC (in MSTPCRB) */
#define MSTPB_CRC 23

/* =============================================================================
 * USB 2.0 Full-Speed Host/Function Module (USB0)
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
 * =============================================================================
 */

/* USB0 Register Structure - Base: 0x000A0000 */
typedef struct {
  volatile uint16_t SYSCFG;   /* 0x00: System Configuration Control */
  volatile uint16_t BUSWAIT;  /* 0x02: CPU Bus Wait Setting */
  volatile uint16_t SYSSTS0;  /* 0x04: System Configuration Status */
  volatile uint16_t PLLSTA;   /* 0x06: PLL Status */
  volatile uint16_t DVSTCTR0; /* 0x08: Device State Control Register 0 */
  uint8_t           RESERVED0[2];
  volatile uint16_t TESTMODE; /* 0x0C: USB Test Mode */
  uint8_t           RESERVED1[2];
  volatile uint16_t D0FBCFG;  /* 0x10: D0FIFO Port Configuration */
  volatile uint16_t D1FBCFG;  /* 0x12: D1FIFO Port Configuration */
  volatile uint32_t CFIFO;    /* 0x14: CFIFO Port (32-bit access) */
  volatile uint32_t D0FIFO;   /* 0x18: D0FIFO Port (32-bit access) */
  volatile uint32_t D1FIFO;   /* 0x1C: D1FIFO Port (32-bit access) */
  volatile uint16_t CFIFOSEL; /* 0x20: CFIFO Port Select */
  volatile uint16_t CFIFOCTR; /* 0x22: CFIFO Port Control */
  uint8_t           RESERVED2[4];
  volatile uint16_t D0FIFOSEL; /* 0x28: D0FIFO Port Select */
  volatile uint16_t D0FIFOCTR; /* 0x2A: D0FIFO Port Control */
  volatile uint16_t D1FIFOSEL; /* 0x2C: D1FIFO Port Select */
  volatile uint16_t D1FIFOCTR; /* 0x2E: D1FIFO Port Control */
  volatile uint16_t INTENB0;   /* 0x30: Interrupt Enable Register 0 */
  volatile uint16_t INTENB1;   /* 0x32: Interrupt Enable Register 1 */
  uint8_t           RESERVED3[2];
  volatile uint16_t BRDYENB; /* 0x36: BRDY Interrupt Enable */
  volatile uint16_t NRDYENB; /* 0x38: NRDY Interrupt Enable */
  volatile uint16_t BEMPENB; /* 0x3A: BEMP Interrupt Enable */
  volatile uint16_t SOFCFG;  /* 0x3C: SOF Output Configuration */
  volatile uint16_t PHYSET;  /* 0x3E: PHY Setting Register */
  volatile uint16_t INTSTS0; /* 0x40: Interrupt Status Register 0 */
  volatile uint16_t INTSTS1; /* 0x42: Interrupt Status Register 1 */
  uint8_t           RESERVED4[2];
  volatile uint16_t BRDYSTS; /* 0x46: BRDY Interrupt Status */
  volatile uint16_t NRDYSTS; /* 0x48: NRDY Interrupt Status */
  volatile uint16_t BEMPSTS; /* 0x4A: BEMP Interrupt Status */
  volatile uint16_t FRMNUM;  /* 0x4C: Frame Number Register */
  volatile uint16_t UFRMNUM; /* 0x4E: uFrame Number Register */
  volatile uint16_t USBADDR; /* 0x50: USB Address Register */
  uint8_t           RESERVED5[2];
  volatile uint16_t USBREQ;  /* 0x54: USB Request Type Register */
  volatile uint16_t USBVAL;  /* 0x56: USB Request Value Register */
  volatile uint16_t USBINDX; /* 0x58: USB Request Index Register */
  volatile uint16_t USBLENG; /* 0x5A: USB Request Length Register */
  volatile uint16_t DCPCFG;  /* 0x5C: DCP Configuration Register */
  volatile uint16_t DCPMAXP; /* 0x5E: DCP Max Packet Size Register */
  volatile uint16_t DCPCTR;  /* 0x60: DCP Control Register */
  uint8_t           RESERVED6[2];
  volatile uint16_t PIPESEL; /* 0x64: Pipe Window Select Register */
  uint8_t           RESERVED7[2];
  volatile uint16_t PIPECFG;  /* 0x68: Pipe Configuration Register */
  volatile uint16_t PIPEBUF;  /* 0x6A: Pipe Buffer Setting Register */
  volatile uint16_t PIPEMAXP; /* 0x6C: Pipe Max Packet Size Register */
  volatile uint16_t PIPEPERI; /* 0x6E: Pipe Cycle Control Register */
  volatile uint16_t PIPE1CTR; /* 0x70: Pipe 1 Control Register */
  volatile uint16_t PIPE2CTR; /* 0x72: Pipe 2 Control Register */
  volatile uint16_t PIPE3CTR; /* 0x74: Pipe 3 Control Register */
  volatile uint16_t PIPE4CTR; /* 0x76: Pipe 4 Control Register */
  volatile uint16_t PIPE5CTR; /* 0x78: Pipe 5 Control Register */
  volatile uint16_t PIPE6CTR; /* 0x7A: Pipe 6 Control Register */
  volatile uint16_t PIPE7CTR; /* 0x7C: Pipe 7 Control Register */
  volatile uint16_t PIPE8CTR; /* 0x7E: Pipe 8 Control Register */
  volatile uint16_t PIPE9CTR; /* 0x80: Pipe 9 Control Register */
  uint8_t           RESERVED8[14];
  volatile uint16_t PIPE1TRE; /* 0x90: Pipe 1 Transaction Counter Enable */
  volatile uint16_t PIPE1TRN; /* 0x92: Pipe 1 Transaction Counter */
  volatile uint16_t PIPE2TRE; /* 0x94: Pipe 2 Transaction Counter Enable */
  volatile uint16_t PIPE2TRN; /* 0x96: Pipe 2 Transaction Counter */
  volatile uint16_t PIPE3TRE; /* 0x98: Pipe 3 Transaction Counter Enable */
  volatile uint16_t PIPE3TRN; /* 0x9A: Pipe 3 Transaction Counter */
  volatile uint16_t PIPE4TRE; /* 0x9C: Pipe 4 Transaction Counter Enable */
  volatile uint16_t PIPE4TRN; /* 0x9E: Pipe 4 Transaction Counter */
  volatile uint16_t PIPE5TRE; /* 0xA0: Pipe 5 Transaction Counter Enable */
  volatile uint16_t PIPE5TRN; /* 0xA2: Pipe 5 Transaction Counter */
  uint8_t           RESERVED9[44];
  volatile uint16_t DEVADD0; /* 0xD0: Device Address 0 Configuration */
  volatile uint16_t DEVADD1; /* 0xD2: Device Address 1 Configuration */
  volatile uint16_t DEVADD2; /* 0xD4: Device Address 2 Configuration */
  volatile uint16_t DEVADD3; /* 0xD6: Device Address 3 Configuration */
  volatile uint16_t DEVADD4; /* 0xD8: Device Address 4 Configuration */
  volatile uint16_t DEVADD5; /* 0xDA: Device Address 5 Configuration */
  uint8_t           RESERVED10[20];
  volatile uint16_t PHYSLEW; /* 0xF0: PHY Cross Point Adjustment */
  uint8_t           RESERVED11[6];
  volatile uint16_t LPCTRL; /* 0xF8: Low Power Control Register */
  volatile uint16_t LPSTS;  /* 0xFA: Low Power Status Register */
} USB0_Type;

#define USB0_BASE ((USB0_Type*)0x000A0000)
#define USB0      (*USB0_BASE)

/* Module stop bit for USB0 (in MSTPCRB) */
#define MSTPB_USB0 19

/* USB0 SYSCFG Register Bit Definitions */
typedef enum {
  k_usb_syscfg_usbe   = (1 << 0),  /* USB Module Enable */
  k_usb_syscfg_uplle  = (1 << 1),  /* USB PLL Enable */
  k_usb_syscfg_ucksel = (1 << 2),  /* USB Clock Select */
  k_usb_syscfg_dprpu  = (1 << 4),  /* D+ Line Resistor Pull-up Control */
  k_usb_syscfg_drpd   = (1 << 5),  /* D+/D- Line Resistor Control */
  k_usb_syscfg_dcfm   = (1 << 6),  /* Controller Function Select (0=Function) */
  k_usb_syscfg_cnen   = (1 << 8),  /* Single-Ended Receiver Enable */
  k_usb_syscfg_scke   = (1 << 10), /* USB Clock Enable */
} usb_syscfg_bits_t;

/* USB0 SYSSTS0 Register Bit Definitions */
typedef enum {
  k_usb_syssts0_lnst_mask = 0x0003,    /* USB Data Line Status Mask */
  k_usb_syssts0_lnst_se0  = 0x0000,    /* SE0 (disconnected) */
  k_usb_syssts0_lnst_fs_j = 0x0001,    /* Full-Speed J-state */
  k_usb_syssts0_lnst_fs_k = 0x0002,    /* Full-Speed K-state */
  k_usb_syssts0_lnst_se1  = 0x0003,    /* SE1 (error) */
  k_usb_syssts0_idmon     = (1 << 2),  /* External ID0 Pin Monitor */
  k_usb_syssts0_sofea     = (1 << 5),  /* SOF Active Monitor */
  k_usb_syssts0_htact     = (1 << 6),  /* USB Host Sequencer Active */
  k_usb_syssts0_ovcmon    = (3 << 14), /* Overcurrent Monitor */
} usb_syssts0_bits_t;

/* USB0 DVSTCTR0 Register Bit Definitions */
typedef enum {
  k_usb_dvstctr0_rhst_mask      = 0x0007,   /* USB Bus Reset Status Mask */
  k_usb_dvstctr0_rhst_undecided = 0x00,     /* Speed undecided */
  k_usb_dvstctr0_rhst_ls        = 0x0001,   /* Low-Speed connection */
  k_usb_dvstctr0_rhst_fs        = 0x0002,   /* Full-Speed connection */
  k_usb_dvstctr0_rhst_reset     = 0x0004,   /* USB bus reset in progress */
  k_usb_dvstctr0_uact           = (1 << 4), /* USB Bus Enable */
  k_usb_dvstctr0_resume         = (1 << 5), /* Resume Signal Output */
  k_usb_dvstctr0_usbrst         = (1 << 6), /* USB Bus Reset Output */
  k_usb_dvstctr0_rwupe          = (1 << 7), /* Wakeup Detection Enable */
  k_usb_dvstctr0_wkup           = (1 << 8), /* Wakeup Output */
} usb_dvstctr0_bits_t;

/* USB0 INTENB0 Register Bit Definitions */
typedef enum {
  k_usb_intenb0_brdye = (1 << 8),  /* Buffer Ready Interrupt Enable */
  k_usb_intenb0_nrdye = (1 << 9),  /* Buffer Not Ready Interrupt Enable */
  k_usb_intenb0_bempe = (1 << 10), /* Buffer Empty Interrupt Enable */
  k_usb_intenb0_ctre  = (1 << 11), /* Control Transfer Stage Enable */
  k_usb_intenb0_dvse  = (1 << 12), /* Device State Transition Enable */
  k_usb_intenb0_sofe  = (1 << 13), /* Frame Number Update Enable */
  k_usb_intenb0_rsme  = (1 << 14), /* Resume Interrupt Enable */
  k_usb_intenb0_vbse  = (1 << 15), /* VBUS Interrupt Enable */
} usb_intenb0_bits_t;

/* USB0 INTSTS0 Register Bit Definitions */
typedef enum {
  k_usb_intsts0_ctsq_mask       = 0x0007,    /* Control Transfer Stage Mask */
  k_usb_intsts0_ctsq_idle       = 0x0000,    /* Idle or setup stage */
  k_usb_intsts0_ctsq_rd_data    = 0x0001,    /* Control read data stage */
  k_usb_intsts0_ctsq_rd_status  = 0x0002,    /* Control read status stage */
  k_usb_intsts0_ctsq_wr_data    = 0x0003,    /* Control write data stage */
  k_usb_intsts0_ctsq_wr_status  = 0x0004,    /* Control write status stage */
  k_usb_intsts0_ctsq_wr_nd      = 0x0005,    /* Control write (no data) status */
  k_usb_intsts0_ctsq_seq_err    = 0x0006,    /* Control sequence error */
  k_usb_intsts0_valid           = (1 << 3),  /* USB Request Reception */
  k_usb_intsts0_dvsq_mask       = (7 << 4),  /* Device State Mask */
  k_usb_intsts0_dvsq_powered    = (0 << 4),  /* Powered state */
  k_usb_intsts0_dvsq_default    = (1 << 4),  /* Default state */
  k_usb_intsts0_dvsq_address    = (2 << 4),  /* Address state */
  k_usb_intsts0_dvsq_configured = (3 << 4),  /* Configured state */
  k_usb_intsts0_dvsq_suspend    = (4 << 4),  /* Suspended state */
  k_usb_intsts0_brdy            = (1 << 8),  /* Buffer Ready Interrupt Status */
  k_usb_intsts0_nrdy            = (1 << 9),  /* Buffer Not Ready Interrupt Status */
  k_usb_intsts0_bemp            = (1 << 10), /* Buffer Empty Interrupt Status */
  k_usb_intsts0_ctrt            = (1 << 11), /* Control Transfer Stage Transition */
  k_usb_intsts0_dvst            = (1 << 12), /* Device State Transition */
  k_usb_intsts0_sofr            = (1 << 13), /* Frame Number Refresh */
  k_usb_intsts0_resm            = (1 << 14), /* Resume Interrupt Status */
  k_usb_intsts0_vbint           = (1 << 15), /* VBUS Interrupt Status */
} usb_intsts0_bits_t;

/* USB0 DCPCFG Register Bit Definitions */
typedef enum {
  k_usb_dcpcfg_shtnak = (1 << 7), /* Pipe Disabled at End of Transfer */
  k_usb_dcpcfg_dir    = (1 << 4), /* Transfer Direction (1=TX, 0=RX) */
} usb_dcpcfg_bits_t;

/* USB0 DCPCTR Register Bit Definitions */
typedef enum {
  k_usb_dcpctr_pid_mask  = 0x0003,    /* Response PID Mask */
  k_usb_dcpctr_pid_nak   = 0x0000,    /* NAK response */
  k_usb_dcpctr_pid_buf   = 0x0001,    /* BUF response (enable) */
  k_usb_dcpctr_pid_stall = 0x0002,    /* STALL response */
  k_usb_dcpctr_ccpl      = (1 << 2),  /* Control Transfer End */
  k_usb_dcpctr_pbusy     = (1 << 5),  /* Pipe Busy */
  k_usb_dcpctr_sqmon     = (1 << 6),  /* Sequence Toggle Bit Monitor */
  k_usb_dcpctr_sqset     = (1 << 7),  /* Sequence Toggle Bit Set */
  k_usb_dcpctr_sqclr     = (1 << 8),  /* Sequence Toggle Bit Clear */
  k_usb_dcpctr_sureqclr  = (1 << 11), /* SUREQ Bit Clear */
  k_usb_dcpctr_sureq     = (1 << 14), /* Setup Token Transmission */
  k_usb_dcpctr_bsts      = (1 << 15), /* Buffer Status */
} usb_dcpctr_bits_t;

/* USB0 PIPECFG Register Bit Definitions */
typedef enum {
  k_usb_pipecfg_epnum_mask = 0x000F,    /* Endpoint Number Mask */
  k_usb_pipecfg_dir        = (1 << 4),  /* Transfer Direction (1=TX, 0=RX) */
  k_usb_pipecfg_shtnak     = (1 << 7),  /* Pipe Disabled at End of Transfer */
  k_usb_pipecfg_dblb       = (1 << 9),  /* Double Buffer Mode */
  k_usb_pipecfg_bfre       = (1 << 10), /* BRDY Interrupt Operation */
  k_usb_pipecfg_type_mask  = (3 << 14), /* Transfer Type Mask */
  k_usb_pipecfg_type_bulk  = (1 << 14), /* Bulk Transfer */
  k_usb_pipecfg_type_int   = (2 << 14), /* Interrupt Transfer */
  k_usb_pipecfg_type_iso   = (3 << 14), /* Isochronous Transfer */
} usb_pipecfg_bits_t;

/* USB0 PIPEnCTR Register Bit Definitions (same for all pipes) */
typedef enum {
  k_usb_pipectr_pid_mask  = 0x0003,    /* Response PID Mask */
  k_usb_pipectr_pid_nak   = 0x0000,    /* NAK response */
  k_usb_pipectr_pid_buf   = 0x0001,    /* BUF response (enable) */
  k_usb_pipectr_pid_stall = 0x0002,    /* STALL response */
  k_usb_pipectr_pbusy     = (1 << 5),  /* Pipe Busy */
  k_usb_pipectr_sqmon     = (1 << 6),  /* Sequence Toggle Bit Monitor */
  k_usb_pipectr_sqset     = (1 << 7),  /* Sequence Toggle Bit Set */
  k_usb_pipectr_sqclr     = (1 << 8),  /* Sequence Toggle Bit Clear */
  k_usb_pipectr_aclrm     = (1 << 9),  /* Auto Buffer Clear Mode */
  k_usb_pipectr_atrepm    = (1 << 10), /* Auto Response Mode */
  k_usb_pipectr_inbufm    = (1 << 14), /* IN Buffer Monitor */
  k_usb_pipectr_bsts      = (1 << 15), /* Buffer Status */
} usb_pipectr_bits_t;

/* USB0 FIFOSEL Register Bit Definitions (CFIFOSEL, D0FIFOSEL, D1FIFOSEL) */
typedef enum {
  k_usb_fifosel_curpipe_mask = 0x000F,    /* Current Pipe Mask */
  k_usb_fifosel_curpipe_dcp  = 0x0000,    /* DCP (Default Control Pipe) */
  k_usb_fifosel_isel         = (1 << 5),  /* Access Direction (1=write, 0=read) */
  k_usb_fifosel_bigend       = (1 << 8),  /* Endian Mode (1=big, 0=little) */
  k_usb_fifosel_mbw_8        = (0 << 10), /* 8-bit access */
  k_usb_fifosel_mbw_16       = (1 << 10), /* 16-bit access */
  k_usb_fifosel_mbw_32       = (2 << 10), /* 32-bit access */
  k_usb_fifosel_rcl          = (1 << 14), /* Read Count Mode */
  k_usb_fifosel_frdy         = (1 << 15), /* FIFO Port Ready */
} usb_fifosel_bits_t;

/* USB0 FIFOCTR Register Bit Definitions (CFIFOCTR, D0FIFOCTR, D1FIFOCTR) */
typedef enum {
  k_usb_fifoctr_dtln_mask = 0x0FFF,    /* Receive Data Length Mask */
  k_usb_fifoctr_frdy      = (1 << 13), /* FIFO Port Ready */
  k_usb_fifoctr_bclr      = (1 << 14), /* CPU Buffer Clear */
  k_usb_fifoctr_bval      = (1 << 15), /* Buffer Memory Valid */
} usb_fifoctr_bits_t;

/* USB Interrupt Vector Numbers */
#define VECT_USB0_D0FIFO 34 /* USB0 D0FIFO interrupt */
#define VECT_USB0_D1FIFO 35 /* USB0 D1FIFO interrupt */
#define VECT_USB0_USBI   36 /* USB0 USBI interrupt */
#define VECT_USB0_USBR   90 /* USB0 USBR (resume) interrupt */

/* USB CDC-ACM Endpoint Configuration */
typedef enum {
  k_usb_cdc_ep_ctrl         = 0,  /* EP0: Control endpoint */
  k_usb_cdc_ep_bulk_in      = 1,  /* EP1: Bulk IN (data to host) */
  k_usb_cdc_ep_bulk_out     = 2,  /* EP2: Bulk OUT (data from host) */
  k_usb_cdc_ep_interrupt_in = 3,  /* EP3: Interrupt IN (notifications) */
  k_usb_cdc_max_packet_fs   = 64, /* Full-Speed max packet size */
} usb_cdc_endpoints_t;

/* =============================================================================
 * Multi-Function Timer Unit (MTU3a)
 * =============================================================================
 */

/* MTU Channel Register Structure (MTU0-MTU4, MTU6-MTU7) */
typedef struct {
  volatile uint8_t  TCR;   /* Timer Control Register */
  volatile uint8_t  TMDR;  /* Timer Mode Register */
  volatile uint8_t  TIORH; /* Timer I/O Control Register H */
  volatile uint8_t  TIORL; /* Timer I/O Control Register L */
  volatile uint8_t  TIER;  /* Timer Interrupt Enable Register */
  volatile uint8_t  TSR;   /* Timer Status Register */
  volatile uint16_t TCNT;  /* Timer Counter */
  volatile uint16_t TGRA;  /* Timer General Register A */
  volatile uint16_t TGRB;  /* Timer General Register B */
  volatile uint16_t TGRC;  /* Timer General Register C */
  volatile uint16_t TGRD;  /* Timer General Register D */
} MTU_Channel_Type;

/* MTU3 and MTU4 have additional registers */
typedef struct {
  volatile uint8_t  TCR;   /* 0x00: Timer Control Register */
  volatile uint8_t  TMDR;  /* 0x01: Timer Mode Register */
  volatile uint8_t  TIORH; /* 0x02: Timer I/O Control Register H */
  volatile uint8_t  TIORL; /* 0x03: Timer I/O Control Register L */
  volatile uint8_t  TIER;  /* 0x04: Timer Interrupt Enable Register */
  volatile uint8_t  TSR;   /* 0x05: Timer Status Register */
  volatile uint16_t TCNT;  /* 0x06: Timer Counter */
  volatile uint16_t TGRA;  /* 0x08: Timer General Register A */
  volatile uint16_t TGRB;  /* 0x0A: Timer General Register B */
  volatile uint16_t TGRC;  /* 0x0C: Timer General Register C */
  volatile uint16_t TGRD;  /* 0x0E: Timer General Register D */
  volatile uint16_t TGRE;  /* 0x10: Timer General Register E (MTU3/4 only) */
  volatile uint16_t TGRF;  /* 0x12: Timer General Register F (MTU3/4 only) */
  volatile uint8_t  TIER2; /* 0x14: Timer Interrupt Enable Register 2 */
  volatile uint8_t  TSR2;  /* 0x15: Timer Status Register 2 */
  volatile uint8_t  TBTM;  /* 0x16: Timer Buffer Transfer Mode Register */
} MTU34_Channel_Type;

/* MTU Start Register */
typedef struct {
  volatile uint8_t TSTR; /* Timer Start Register */
} MTU_TSTR_Type;

#define MTU0_BASE ((MTU_Channel_Type*)0x000D0600)
#define MTU1_BASE ((MTU_Channel_Type*)0x000D0680)
#define MTU2_BASE ((MTU_Channel_Type*)0x000D0700)
#define MTU3_BASE ((MTU34_Channel_Type*)0x000D0200)
#define MTU4_BASE ((MTU34_Channel_Type*)0x000D0201)
#define MTU6_BASE ((MTU_Channel_Type*)0x000D0A00)
#define MTU7_BASE ((MTU_Channel_Type*)0x000D0A80)

#define MTU0 (*MTU0_BASE)
#define MTU1 (*MTU1_BASE)
#define MTU2 (*MTU2_BASE)
#define MTU3 (*MTU3_BASE)
#define MTU4 (*MTU4_BASE)
#define MTU6 (*MTU6_BASE)
#define MTU7 (*MTU7_BASE)

#define MTU_TSTR_BASE ((MTU_TSTR_Type*)0x000D0880)
#define MTU_TSTR      (*MTU_TSTR_BASE)

/* Timer Control Register (TCR) bits */
typedef enum {
  k_mtu_tcr_tpsc_mask = 0x07,        /* Timer Prescaler mask (bits 0-2) */
  k_mtu_tcr_ckeg_mask = 0x18,        /* Clock Edge mask (bits 3-4) */
  k_mtu_tcr_cclr_mask = 0xE0,        /* Counter Clear Source mask (bits 5-7) */
  k_mtu_tcr_tpsc_1    = 0x00,        /* PCLKA/1 */
  k_mtu_tcr_tpsc_4    = 0x01,        /* PCLKA/4 */
  k_mtu_tcr_tpsc_16   = 0x02,        /* PCLKA/16 */
  k_mtu_tcr_tpsc_64   = 0x03,        /* PCLKA/64 */
  k_mtu_tcr_cclr_tgra = (0x01 << 5), /* Clear on TGRA compare match */
} mtu_tcr_bits_t;

/* Timer Mode Register (TMDR) bits */
typedef enum {
  k_mtu_tmdr_md_mask   = 0x0F,     /* Mode select mask (bits 0-3) */
  k_mtu_tmdr_md_normal = 0x00,     /* Normal mode */
  k_mtu_tmdr_md_pwm1   = 0x02,     /* PWM mode 1 */
  k_mtu_tmdr_md_pwm2   = 0x03,     /* PWM mode 2 */
  k_mtu_tmdr_bfa       = (1 << 4), /* Buffer mode A */
  k_mtu_tmdr_bfb       = (1 << 5), /* Buffer mode B */
} mtu_tmdr_bits_t;

/* Timer I/O Control Register (TIOR) bits */
typedef enum {
  k_mtu_tior_ioa_mask  = 0x0F, /* I/O Control A mask (bits 0-3) */
  k_mtu_tior_iob_mask  = 0xF0, /* I/O Control B mask (bits 4-7) */
  k_mtu_tior_init_low  = 0x02, /* Initial output low, compare match high */
  k_mtu_tior_init_high = 0x05, /* Initial output high, compare match low */
  k_mtu_tior_toggle    = 0x03, /* Toggle on compare match */
} mtu_tior_bits_t;

/* Timer Start Register (TSTR) bits */
typedef enum {
  k_mtu_tstr_cst0 = (1 << 0), /* Counter Start 0 */
  k_mtu_tstr_cst1 = (1 << 1), /* Counter Start 1 */
  k_mtu_tstr_cst2 = (1 << 2), /* Counter Start 2 */
  k_mtu_tstr_cst3 = (1 << 6), /* Counter Start 3 */
  k_mtu_tstr_cst4 = (1 << 7), /* Counter Start 4 */
} mtu_tstr_bits_t;

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
} PFS_Type;

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
} MPC_Type;

#define MPC_BASE ((MPC_Type*)0x0008C100)
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

/* =============================================================================
 * Clock Frequencies (assume PLL configured for 240 MHz)
 * =============================================================================
 */

#define ICLK_HZ  240000000UL /* CPU clock */
#define PCLKA_HZ 120000000UL /* Peripheral clock A */
#define PCLKB_HZ 60000000UL  /* Peripheral clock B */
#define PCLKC_HZ 60000000UL  /* Peripheral clock C */
#define PCLKD_HZ 60000000UL  /* Peripheral clock D */
#define BCLK_HZ  120000000UL /* External bus clock */
#define FCLK_HZ  60000000UL  /* Flash clock */

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_REGS_H */
