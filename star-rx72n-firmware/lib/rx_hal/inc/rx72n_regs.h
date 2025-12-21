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
  volatile uint8_t ICCR1;  /* I2C Bus Control Register 1 */
  volatile uint8_t ICCR2;  /* I2C Bus Control Register 2 */
  volatile uint8_t ICMR1;  /* I2C Bus Mode Register 1 */
  volatile uint8_t ICMR2;  /* I2C Bus Mode Register 2 */
  volatile uint8_t ICMR3;  /* I2C Bus Mode Register 3 */
  volatile uint8_t ICFER;  /* I2C Bus Function Enable Register */
  volatile uint8_t ICSER;  /* I2C Bus Status Enable Register */
  volatile uint8_t ICIER;  /* I2C Bus Interrupt Enable Register */
  volatile uint8_t ICSR1;  /* I2C Bus Status Register 1 */
  volatile uint8_t ICSR2;  /* I2C Bus Status Register 2 */
  volatile uint8_t SARL0;  /* Slave Address Register L0 */
  volatile uint8_t SARU0;  /* Slave Address Register U0 */
  volatile uint8_t SARL1;  /* Slave Address Register L1 */
  volatile uint8_t SARU1;  /* Slave Address Register U1 */
  volatile uint8_t SARL2;  /* Slave Address Register L2 */
  volatile uint8_t SARU2;  /* Slave Address Register U2 */
  volatile uint8_t ICBRL;  /* I2C Bus Bit Rate Register L */
  volatile uint8_t ICBRH;  /* I2C Bus Bit Rate Register H */
  volatile uint8_t ICDRT;  /* I2C Bus Transmit Data Register */
  volatile uint8_t ICDRR;  /* I2C Bus Receive Data Register */
} RIIC_Type;

#define RIIC0_BASE ((RIIC_Type*)0x00088300)
#define RIIC1_BASE ((RIIC_Type*)0x00088320)
#define RIIC2_BASE ((RIIC_Type*)0x00088340)

#define RIIC0 (*RIIC0_BASE)
#define RIIC1 (*RIIC1_BASE)
#define RIIC2 (*RIIC2_BASE)

/* RIIC Control Register 1 (ICCR1) Bit Definitions */
typedef enum {
  k_riic_iccr1_ice   = (1 << 7), /* I2C Bus Interface Enable */
  k_riic_iccr1_iicrst = (1 << 6), /* I2C Bus Interface Internal Reset */
  k_riic_iccr1_clk_mask = 0x0F,   /* Clock Select Mask (bits 0-3) */
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
 * Multi-Function Timer Unit (MTU3a)
 * =============================================================================
 */

/* MTU Channel Register Structure (MTU0-MTU4, MTU6-MTU7) */
typedef struct {
  volatile uint8_t  TCR;    /* Timer Control Register */
  volatile uint8_t  TMDR;   /* Timer Mode Register */
  volatile uint8_t  TIORH;  /* Timer I/O Control Register H */
  volatile uint8_t  TIORL;  /* Timer I/O Control Register L */
  volatile uint8_t  TIER;   /* Timer Interrupt Enable Register */
  volatile uint8_t  TSR;    /* Timer Status Register */
  volatile uint16_t TCNT;   /* Timer Counter */
  volatile uint16_t TGRA;   /* Timer General Register A */
  volatile uint16_t TGRB;   /* Timer General Register B */
  volatile uint16_t TGRC;   /* Timer General Register C */
  volatile uint16_t TGRD;   /* Timer General Register D */
} MTU_Channel_Type;

/* MTU3 and MTU4 have additional registers */
typedef struct {
  volatile uint8_t  TCR;    /* 0x00: Timer Control Register */
  volatile uint8_t  TMDR;   /* 0x01: Timer Mode Register */
  volatile uint8_t  TIORH;  /* 0x02: Timer I/O Control Register H */
  volatile uint8_t  TIORL;  /* 0x03: Timer I/O Control Register L */
  volatile uint8_t  TIER;   /* 0x04: Timer Interrupt Enable Register */
  volatile uint8_t  TSR;    /* 0x05: Timer Status Register */
  volatile uint16_t TCNT;   /* 0x06: Timer Counter */
  volatile uint16_t TGRA;   /* 0x08: Timer General Register A */
  volatile uint16_t TGRB;   /* 0x0A: Timer General Register B */
  volatile uint16_t TGRC;   /* 0x0C: Timer General Register C */
  volatile uint16_t TGRD;   /* 0x0E: Timer General Register D */
  volatile uint16_t TGRE;   /* 0x10: Timer General Register E (MTU3/4 only) */
  volatile uint16_t TGRF;   /* 0x12: Timer General Register F (MTU3/4 only) */
  volatile uint8_t  TIER2;  /* 0x14: Timer Interrupt Enable Register 2 */
  volatile uint8_t  TSR2;   /* 0x15: Timer Status Register 2 */
  volatile uint8_t  TBTM;   /* 0x16: Timer Buffer Transfer Mode Register */
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
  k_mtu_tcr_tpsc_mask  = 0x07, /* Timer Prescaler mask (bits 0-2) */
  k_mtu_tcr_ckeg_mask  = 0x18, /* Clock Edge mask (bits 3-4) */
  k_mtu_tcr_cclr_mask  = 0xE0, /* Counter Clear Source mask (bits 5-7) */
  k_mtu_tcr_tpsc_1     = 0x00, /* PCLKA/1 */
  k_mtu_tcr_tpsc_4     = 0x01, /* PCLKA/4 */
  k_mtu_tcr_tpsc_16    = 0x02, /* PCLKA/16 */
  k_mtu_tcr_tpsc_64    = 0x03, /* PCLKA/64 */
  k_mtu_tcr_cclr_tgra  = (0x01 << 5), /* Clear on TGRA compare match */
} mtu_tcr_bits_t;

/* Timer Mode Register (TMDR) bits */
typedef enum {
  k_mtu_tmdr_md_mask   = 0x0F, /* Mode select mask (bits 0-3) */
  k_mtu_tmdr_md_normal = 0x00, /* Normal mode */
  k_mtu_tmdr_md_pwm1   = 0x02, /* PWM mode 1 */
  k_mtu_tmdr_md_pwm2   = 0x03, /* PWM mode 2 */
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
  volatile uint8_t      : 1; /* Reserved */
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
  k_pfs_psel_mask  = 0x1F, /* Peripheral Select mask (bits 0-4) */
  k_pfs_isel       = (1 << 6), /* Interrupt Input Select */
  k_pfs_asel       = (1 << 7), /* Analog Input Select */
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
