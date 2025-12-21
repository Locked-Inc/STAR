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
 * Multi-Function Timer Pulse Unit 3a (MTU3a) - For Motor PWM
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  TCR;   /* Timer Control Register */
  volatile uint8_t  TMDR1; /* Timer Mode Register 1 */
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

#define MTU0_BASE ((MTU_Channel_Type*)0x000C1200)
#define MTU1_BASE ((MTU_Channel_Type*)0x000C1280)
#define MTU2_BASE ((MTU_Channel_Type*)0x000C1200)
#define MTU3_BASE ((MTU_Channel_Type*)0x000C1200)
#define MTU4_BASE ((MTU_Channel_Type*)0x000C1200)

#define MTU0 (*MTU0_BASE)
#define MTU1 (*MTU1_BASE)
#define MTU2 (*MTU2_BASE)
#define MTU3 (*MTU3_BASE)
#define MTU4 (*MTU4_BASE)

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
