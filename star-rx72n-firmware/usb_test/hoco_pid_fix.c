/**
 * @file hoco_pid_fix.c
 * @brief HOCO PLL (known working) + PID=BUF fix. Simplest possible test.
 *
 * Uses HOCO 16 MHz x12 = 192 MHz PLL, switches CPU to PLL (known OK),
 * UCK /4 = 48 MHz for USB. Adds the critical PID=BUF restoration after
 * SETUP reception (manual p2017).
 *
 * If this enumerates on macOS, the cable is connected and the fix works.
 * If not, the RX72N USB0 port cable is likely not plugged into the Mac.
 */
#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

static void delay(volatile uint32_t n) { while (n--) { __asm__("nop"); } }

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

/* LED PA7 for visible progress indication */
#define LED_INIT()  do { REG8(0x8C00A) |= 0x80U; } while (0)
#define LED_ON()    do { REG8(0x8C02A) |= 0x80U; } while (0)
#define LED_OFF()   do { REG8(0x8C02A) &= ~0x80U; } while (0)
#define LED_TGL()   do { REG8(0x8C02A) ^= 0x80U; } while (0)

/* PB3 = pin 82 = AD2 IO7 for diagnostic output */
#define PB3_INIT()  do { REG8(0x8C00B) |= 0x08U; REG8(0x8C02B) &= ~0x08U; } while (0)
#define PB3_HIGH()  do { REG8(0x8C02B) |=  0x08U; } while (0)
#define PB3_LOW()   do { REG8(0x8C02B) &= ~0x08U; } while (0)

/* Blink N times slowly -- user can see which phase firmware reached */
static void blink(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        LED_ON();  delay(40000U);
        LED_OFF(); delay(40000U);
    }
    delay(100000U);  /* gap */
}

static void cfifo_write(const uint8_t *data, uint16_t len)
{
    /* 8-bit MBW: each write adds exactly 1 byte to DTLN, so odd-length
     * transfers (e.g. Linux's 9-byte GET_DESCRIPTOR(CONFIG)) don't pad
     * to 10 bytes and trigger -75 EOVERFLOW on the host. */
    REG16(CFIFOSEL) = (1U << 5);  /* ISEL=1, MBW=0 (8-bit), CURPIPE=DCP */
    while (!(REG16(CFIFOCTR) & (1U << 13))) {}
    REG16(CFIFOCTR) |= (1U << 14);
    while (REG16(CFIFOCTR) & (1U << 14)) {}
    for (uint16_t i = 0; i < len; i++) {
        REG8(CFIFO) = data[i];
    }
    REG16(CFIFOCTR) |= (1U << 15);
}

int main(void)
{
    /* Clear stale DPRPU from prior firmware */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;
    REG16(SYSCFG) = 0x0000U;
    delay(10000U);

    LED_INIT();
    PB3_INIT();
    blink(1);  /* PHASE 1: firmware started */

    /* ================================================================
     * CLOCK: HOCO 16 MHz x12 = 192 MHz, UCK /4 = 48 MHz
     * Known working on Linux testing.
     * ================================================================ */
    REG16(0x803FE) = 0xA503U;  /* PRC0+PRC1 */

    /* Start HOCO (may already be running from OFS1=0xFF) */
    REG8(0x80036) = 0x00U;
    while ((REG8(0x8003C) & 0x08U) == 0) { __asm__ volatile("nop"); }

    /* PLL: HOCO x12 = 192 MHz.
     * STC=(12*2-1)=23=0x17, PLLSRCSEL=1(HOCO), PLIDIV=/1.
     * PLLCR = 0x1710. */
    REG16(0x80028) = 0x1710U;
    REG8(0x8002A) = 0x00U;  /* PLLCR2: enable */
    while ((REG8(0x8003C) & 0x04U) == 0) { __asm__ volatile("nop"); }

    /* MEMWAIT (harmless at 96 MHz, required if ICLK > 120) */
    REG8(0x8101C) = 0x01U;

    /* Dividers: ICLK=/2=96MHz, FCK=/4=48MHz, PCKB=/4=48MHz */
    REG32(0x80020) = 0x21C21211U;

    /* USB clock path: main PLL, UCK /4 = 48 MHz */
    REG16(0x80044) = 0x0001U;  /* PACKCR: default, UPLLSEL=0 main PLL */
    REG16(0x80024) = 0x0031U;  /* SCKCR2: UCK=3 -> /4 */

    /* Switch CPU to PLL */
    REG16(0x80026) = 0x0400U;  /* SCKCR3: CKSEL=PLL */

    REG16(0x803FE) = 0xA500U;

    blink(2);  /* PHASE 2: clocks configured, CPU on PLL */

    /* ================================================================
     * USB0 init
     * ================================================================ */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);  /* enable USB0 */
    REG16(0x803FE) = 0xA500U;

    REG16(SYSCFG) = 0x0000U;
    delay(100000U);   /* ~10ms at 96 MHz actually ~1ms, but OK */
    REG16(SYSCFG) |= (1U << 0);    /* USBE */
    REG16(SYSCFG) |= (1U << 10);   /* SCKE */
    delay(100000U);

    REG16(DCPCFG) = 0x0000U;
    REG16(DCPMAXP) = 64U;
    REG16(DCPCTR) = 0x0001U;  /* PID=BUF */

    REG16(BRDYENB) = 0x0001U;
    REG16(BEMPENB) = 0x0001U;
    REG16(INTENB0) = (1U << 15) | (1U << 12) | (1U << 11) | (1U << 8) | (1U << 10);

    blink(3);  /* PHASE 3: USB configured, about to assert DPRPU */

    /* D+ pull-up -- host should detect device NOW */
    REG16(SYSCFG) |= (1U << 4);

    blink(4);  /* PHASE 4: DPRPU asserted */

    /* ================================================================
     * POLL LOOP -- with PID=BUF fix after SETUP reception
     * ================================================================ */
    volatile uint32_t pc = 0;
    volatile uint32_t ctrt_count = 0;  /* Count SETUP receptions */
    volatile uint32_t get_desc_count = 0;
    for (;;) {
        pc++;
        if ((pc & 0x7FFFFU) == 0) {
            LED_TGL();  /* fast heartbeat */
            /* PB3 pulses: count of GET_DESCRIPTOR handlers entered.
             * - Long HIGH (50% duty) = DVSQ 3 (Configured)
             * - Short pulses = count of CTRTs processed (mod 16)
             * Encode get_desc_count as PB3 state. */
            uint8_t dvsq = (uint8_t)((REG16(INTSTS0) >> 4) & 0x7U);
            if (dvsq >= 3U) {
                PB3_HIGH();  /* Configured! */
            } else {
                /* Toggle PB3 based on ctrt_count low bit
                 * (so rate of toggle = CTRT frequency) */
                if ((ctrt_count & 1U) == 0) { PB3_LOW(); } else { PB3_HIGH(); }
            }
        }

        uint16_t st = REG16(INTSTS0);

        if (st & (1U << 12)) {
            REG16(INTSTS0) = (uint16_t)~(1U << 12);
        }

        if (st & (1U << 11)) {
            ctrt_count++;
            uint16_t c = st & 7U;
            if (c == 1U || c == 3U || c == 5U) {
                REG16(INTSTS0) = (uint16_t)~(1U << 3);
                /* CRITICAL FIX: restore PID=BUF after SETUP reception
                 * Hardware sets PID=NAK on SETUP (manual p2017). */
                REG16(DCPCTR) |= 0x0001U;

                uint16_t req = REG16(USBREQ);
                uint16_t val = REG16(USBVAL);
                uint16_t len = REG16(USBLENG);
                uint8_t  br  = (uint8_t)(req >> 8);

                if (br == 0x06U) {
                    get_desc_count++;
                    uint8_t dt = (uint8_t)(val >> 8);
                    const uint8_t *src = (dt == 1U) ? s_dev
                                       : (dt == 2U) ? s_cfg
                                       : (const uint8_t *)0;
                    if (src) {
                        uint16_t s = (len < 18U) ? len : 18U;
                        cfifo_write(src, s);
                        /* Control Read: set CCPL to accept status stage OUT */
                        REG16(DCPCTR) |= (1U << 2);
                    } else {
                        REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
                    }
                } else if (br == 0x05U) {
                    /* SET_ADDRESS: hardware handles USBADDR via CCPL */
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
