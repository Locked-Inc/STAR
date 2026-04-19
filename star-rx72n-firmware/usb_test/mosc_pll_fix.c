/**
 * @file mosc_pll_fix.c
 * @brief USB0: Matches production rx_usb_hw.c init sequence exactly.
 *
 * Changes vs previous versions (matching production rx_usb_hw.c):
 * - DCPCTR: PID_BUF only (0x0001), no SQCLR bit
 * - SET_ADDRESS: simple usbaddr write + CCPL (no DVCHG)
 * - DVST: just clear flag, don't reinit DCP
 * - INTENB0: add VBSE (bit 15)
 * - Clock: MOSC PLL x8=192MHz, SCKCR2 UCK/4=48MHz, CPU on HOCO
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */
#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

static const uint8_t s_dev[18] = {
    0x12, 0x01, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x40,
    0x09, 0x12, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01
};
static const uint8_t s_cfg[18] = {
    0x09, 0x02, 0x12, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00
};

#define USB0       0xA0000U
#define SYSCFG     (USB0 + 0x00U)
#define CFIFO      (USB0 + 0x14U)
#define CFIFOSEL   (USB0 + 0x20U)
#define CFIFOCTR   (USB0 + 0x22U)
#define INTENB0    (USB0 + 0x30U)
#define BRDYENB    (USB0 + 0x36U)
#define BEMPENB    (USB0 + 0x3AU)
#define INTSTS0    (USB0 + 0x40U)
#define BRDYSTS    (USB0 + 0x46U)
#define BEMPSTS    (USB0 + 0x4AU)
#define USBADDR    (USB0 + 0x50U)
#define USBREQ     (USB0 + 0x54U)
#define USBVAL     (USB0 + 0x56U)
#define USBLENG    (USB0 + 0x5AU)
#define DCPCFG     (USB0 + 0x5CU)
#define DCPMAXP    (USB0 + 0x5EU)
#define DCPCTR     (USB0 + 0x60U)

static void cfifo_write(const uint8_t *data, uint16_t len)
{
    /* Match production: select DCP for write, 16-bit access */
    REG16(CFIFOSEL) = (1U << 5) | (1U << 10);  /* ISEL=1, MBW=16 */
    /* Readback verify per manual p1939 */
    while ((REG16(CFIFOSEL) & ((1U << 5) | (1U << 10))) != ((1U << 5) | (1U << 10))) {}
    /* Wait FRDY */
    while (!(REG16(CFIFOCTR) & (1U << 13))) {}
    /* Clear stale buffer */
    REG16(CFIFOCTR) |= (1U << 14);
    while (REG16(CFIFOCTR) & (1U << 14)) {}
    /* Write data as 16-bit words */
    for (uint16_t i = 0; i < len; i += 2U) {
        uint16_t w = data[i];
        if (i + 1U < len) { w |= (uint16_t)data[i + 1U] << 8; }
        REG16(CFIFO) = w;
    }
    /* Mark valid */
    REG16(CFIFOCTR) |= (1U << 15);
}

int main(void)
{
    /* Clear stale DPRPU */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;
    REG16(SYSCFG) = 0x0000U;
    delay(5000U);

    REG8(0x8C00A) |= 0x80U;
    REG8(0x8C02A) |= 0x80U; delay(15000U); REG8(0x8C02A) &= ~0x80U;

    /* ================================================================
     * CLOCK
     * ================================================================ */
    REG16(0x803FE) = 0xA50FU;

    REG8(0x80036) = 0x00U;
    while ((REG8(0x8003C) & 0x08U) == 0) { __asm__ volatile("nop"); }
    REG16(0x80026) = 0x0100U;

    REG8(0x80033) = 0x01U;
    REG8(0x8C426) = 0x00U;
    REG8(0x80032) = 0x00U;
    for (volatile uint32_t i = 0; i < 200000U; i++) { __asm__ volatile("nop"); }

    REG8(0x8002A) = 0x01U;
    REG8(0x8004A) = 0x01U;
    for (volatile uint32_t i = 0; i < 1000U; i++) { __asm__ volatile("nop"); }

    REG16(0x80028) = 0x0F00U;
    REG8(0x8002A) = 0x00U;
    while ((REG8(0x8003C) & 0x04U) == 0) { __asm__ volatile("nop"); }

    REG16(0x80024) = 0x0031U;

    /* Read back PLLCR to embed in descriptor for remote debugging.
     * If PLLCR = 0x0F00 (MOSC x8), bcdDevice will be 0x0F00.
     * If PLLCR = 0x1710 (HOCO x12), bcdDevice will be 0x1710. */
    uint16_t pllcr_rb = REG16(0x80028);
    /* Patch bcdDevice in device descriptor (bytes 12-13, little-endian) */
    static uint8_t s_dev_copy[18];
    for (int i = 0; i < 18; i++) { s_dev_copy[i] = s_dev[i]; }
    s_dev_copy[12] = (uint8_t)(pllcr_rb & 0xFF);
    s_dev_copy[13] = (uint8_t)(pllcr_rb >> 8);

    REG16(0x803FE) = 0xA500U;

    /* ================================================================
     * USB0 -- match production rx_usb_hw.c sequence exactly
     * ================================================================ */
    /* 1. Module clock enable */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;

    /* 2. Disable everything */
    REG16(SYSCFG) = 0x0000U;
    delay(160000U);  /* 10ms at 16 MHz */

    /* 3. Enable USBE first, then SCKE (order matters per production) */
    REG16(SYSCFG) |= (1U << 0);   /* USBE */
    REG16(SYSCFG) |= (1U << 10);  /* SCKE */
    delay(160000U);  /* 10ms stabilization */

    /* 4. DCP setup -- match production: PID_BUF only, NO SQCLR */
    REG16(DCPCFG) = 0x0000U;
    REG16(DCPMAXP) = 64U;
    REG16(DCPCTR) = 0x0001U;  /* PID_BUF only (production: k_dcpctr_pid_buf) */

    /* 5. Interrupt enables -- match production including VBSE */
    REG16(BRDYENB) |= 0x0001U;
    REG16(BEMPENB) |= 0x0001U;
    REG16(INTENB0) = (1U << 15) | (1U << 12) | (1U << 11) | (1U << 8) | (1U << 10);
    /*               VBSE(15)     DVSE(12)      CTRE(11)     BRDYE(8)    BEMPE(10) */

    /* 6. D+ pull-up */
    REG16(SYSCFG) |= (1U << 4);

    /* ================================================================
     * POLL LOOP
     * ================================================================ */
    volatile uint32_t pc = 0;
    for (;;) {
        pc++;
        if ((pc & 0xFFFFFU) == 0) { REG8(0x8C02A) ^= 0x80U; }

        uint16_t st = REG16(INTSTS0);

        /* DVST: just clear flag (match production -- don't reinit DCP) */
        if (st & (1U << 12)) {
            REG16(INTSTS0) = (uint16_t)~(1U << 12);
        }

        /* CTRT */
        if (st & (1U << 11)) {
            uint16_t c = st & 7U;
            if (c == 1U || c == 3U || c == 5U) {
                REG16(INTSTS0) = (uint16_t)~(1U << 3);  /* clear VALID */
                /* CRITICAL: Hardware sets PID=NAK on SETUP token reception
                 * (manual p2017 sec 40.3.4.6(4)). Must restore PID=BUF so
                 * the DCP can respond to subsequent IN/OUT tokens. */
                REG16(DCPCTR) |= 0x0001U;  /* PID = BUF */
                uint16_t req = REG16(USBREQ);
                uint16_t val = REG16(USBVAL);
                uint16_t len = REG16(USBLENG);
                uint8_t  br  = (uint8_t)(req >> 8);

                if (br == 0x06U) {
                    uint8_t dt = (uint8_t)(val >> 8);
                    const uint8_t *src = (dt == 1U) ? s_dev_copy
                                       : (dt == 2U) ? s_cfg
                                       : (const uint8_t *)0;
                    if (src) {
                        uint16_t s = (len < 18U) ? len : 18U;
                        cfifo_write(src, s);
                    } else {
                        REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
                    }
                } else if (br == 0x05U) {
                    /* SET_ADDRESS: match production -- write usbaddr + CCPL */
                    REG16(USBADDR) = val & 0x7FU;
                    REG16(DCPCTR) |= (1U << 2);
                } else if (br == 0x09U) {
                    REG16(DCPCTR) |= (1U << 2);
                } else if (br == 0x00U) {
                    static const uint8_t zeros[2] = { 0x00U, 0x00U };
                    cfifo_write(zeros, 2U);
                } else {
                    REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
                }
            }
            REG16(INTSTS0) = (uint16_t)~(1U << 11);
        }

        if (st & (1U << 8))  { REG16(BRDYSTS) = 0; REG16(INTSTS0) = (uint16_t)~(1U << 8);  }
        if (st & (1U << 10)) { REG16(BEMPSTS) = 0; REG16(INTSTS0) = (uint16_t)~(1U << 10); }
    }
    return 0;
}
