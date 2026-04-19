/**
 * @file diag_flash.c
 * @brief USB0 bring-up: MOSC 24 MHz crystal -> PLL x10 = 240 MHz -> UCK 48 MHz.
 *
 * Board WITH external 24 MHz crystal.
 * Polls INTSTS0 for CTRT to handle SETUP packets.
 */
#include <stdint.h>
#define REG8(a)  (*(volatile uint8_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
#define REG32(a) (*(volatile uint32_t*)(a))

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

static const uint8_t s_dev[18] = {
    0x12,0x01,0x00,0x02,0xFF,0x00,0x00,0x40,
    0x09,0x12,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x01
};
static const uint8_t s_cfg[18] = {
    0x09,0x02,0x12,0x00,0x01,0x01,0x00,0x80,0x32,
    0x09,0x04,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x00
};

int main(void)
{
    /* PA7 blink = alive */
    REG8(0x8C00A) |= 0x80U;
    REG8(0x8C02A) |= 0x80U; delay(50000U);
    REG8(0x8C02A) &= ~0x80U; delay(50000U);

    /* Clock: try MOSC 24 MHz crystal, fall back to HOCO if no crystal */
    REG16(0x803FE) = 0xA503U; /* unlock PRCR */

    /* Try MOSC first */
    uint8_t use_mosc = 0U;
    REG8(0x8C293) = 0x00U;   /* MOFCR: crystal, 20-24 MHz */
    REG8(0x800A2) = 0x53U;   /* MOSCWTCR */
    REG8(0x80032) = 0x00U;   /* MOSCCR = start */
    for (volatile uint32_t t = 0; t < 200000U; t++) {
        if (REG8(0x8003C) & 0x01U) { use_mosc = 1U; break; }
    }

    /* 2 blinks = MOSC result (fast=MOSC ok, will be followed by more) */
    for (int i=0; i<2; i++) {
        REG8(0x8C02A) |= 0x80U; delay(use_mosc ? 30000U : 100000U);
        REG8(0x8C02A) &= ~0x80U; delay(use_mosc ? 30000U : 100000U);
    }

    /* Configure PLL based on which oscillator is available */
    REG8(0x8002F) = 0x01U; /* stop PLL */
    delay(1000U);
    if (use_mosc) {
        /* MOSC 24 MHz x10 = 240 MHz. STC=(10*2-1)=19, PLLSRCSEL=0 */
        REG16(0x8002C) = 0x1300U;
    } else {
        /* HOCO 16 MHz x12 = 192 MHz. STC=(12*2-1)=23, PLLSRCSEL=1 */
        REG8(0x80037) = 0x00U; /* HOCOCR2 = 16 MHz */
        REG8(0x80036) = 0x00U; /* HOCOCR = start */
        for (volatile uint32_t t=0; t<200000; t++) {
            if (REG8(0x8003C) & 0x08U) break;
        }
        REG16(0x80026) = 0x0100U; /* SCKCR3 = HOCO temporarily */
        REG16(0x8002C) = 0x1710U;
    }
    REG8(0x8002F) = 0x00U; /* start PLL */
    for (volatile uint32_t t = 0; t < 2000000U; t++) {
        if (REG8(0x8003C) & 0x04U) break;
    }

    /* Flash wait state + dividers */
    REG8(0x80046) = 0x01U;           /* MEMWAIT */
    REG32(0x80020) = 0x21C21211U;    /* SCKCR */
    REG16(0x80044) = 0x0000U;        /* PACKCR: main PLL for UCK */
    if (use_mosc) {
        REG16(0x80024) = 0x0041U;    /* SCKCR2: UCK /5 = 48 MHz (240/5) */
    } else {
        REG16(0x80024) = 0x0031U;    /* SCKCR2: UCK /4 = 48 MHz (192/4) */
    }
    REG16(0x80026) = 0x0400U;        /* SCKCR3: PLL */

    /* 3 blinks = PLL running */
    for (int i=0; i<3; i++) {
        REG8(0x8C02A) |= 0x80U; delay(50000U);
        REG8(0x8C02A) &= ~0x80U; delay(50000U);
    }

    /* USB0 */
    REG32(0x80014) &= ~(1UL << 19); /* MSTPCRB: enable USB0 */
    REG16(0xA0000) = 0x0000U; delay(50000U);
    REG16(0xA0000) |= (1U << 0);  /* USBE */
    REG16(0xA0000) |= (1U << 10); /* SCKE */
    delay(50000U);
    REG16(0xA005C) = 0U;          /* DCPCFG */
    REG16(0xA005E) = 64U;         /* DCPMAXP */
    REG16(0xA0060) = 1U;          /* DCPCTR = PID_BUF */
    REG16(0xA0036) = 0x0001U;     /* BRDYENB: pipe 0 */
    REG16(0xA003A) = 0x0001U;     /* BEMPENB: pipe 0 */
    REG16(0xA0030) = (1U<<11)|(1U<<12)|(1U<<8)|(1U<<10); /* INTENB0 */
    REG16(0xA0000) |= (1U << 4);  /* DPRPU */

    REG16(0x803FE) = 0xA500U; /* lock PRCR */

    /* Poll loop with slow heartbeat so we can see it's alive */
    volatile uint32_t poll_count = 0;
    for (;;) {
        poll_count++;
        if ((poll_count & 0x3FFFFU) == 0) { REG8(0x8C02A) ^= 0x80U; } /* toggle LED ~every 260K polls */
        uint16_t st = REG16(0xA0040);

        if (st & (1U<<12)) { /* DVST */
            REG16(0xA005C) = 0U;
            REG16(0xA005E) = 64U;
            REG16(0xA0060) = 1U;
            REG16(0xA0040) = (uint16_t)~(1U<<12);
        }

        if (st & (1U<<11)) { /* CTRT */
            uint16_t c = st & 7U;
            if (c==1||c==3||c==5) {
                REG16(0xA0040) = (uint16_t)~(1U<<3); /* clear VALID */
                uint16_t req=REG16(0xA0054), val=REG16(0xA0056), len=REG16(0xA005A);
                uint8_t br = (uint8_t)(req>>8);
                if (br==0x06) { /* GET_DESCRIPTOR */
                    uint8_t dt = (uint8_t)(val>>8);
                    const uint8_t *src = (dt==1) ? s_dev : (dt==2) ? s_cfg : (const uint8_t*)0;
                    if (src) {
                        uint16_t s = (len<18) ? len : 18;
                        REG16(0xA0020) = (1U<<5)|(1U<<10); /* CFIFOSEL: ISEL+MBW16 */
                        while (!(REG16(0xA0022) & (1U<<13))) {} /* FRDY */
                        REG16(0xA0022) |= (1U<<14); /* BCLR */
                        while (REG16(0xA0022) & (1U<<14)) {}
                        for (uint16_t i=0; i<s; i+=2) {
                            uint16_t w = src[i];
                            if (i+1<s) w |= (uint16_t)src[i+1]<<8;
                            REG16(0xA0014) = w;
                        }
                        REG16(0xA0022) |= (1U<<15); /* BVAL */
                    } else {
                        REG16(0xA0060) = (REG16(0xA0060)&~3U)|2U; /* STALL */
                    }
                } else if (br==0x05) { /* SET_ADDRESS */
                    REG16(0xA006C) = val & 0x7FU;
                    REG16(0xA0060) |= (1U<<2); /* CCPL */
                } else if (br==0x09) { /* SET_CONFIG */
                    REG16(0xA0060) |= (1U<<2);
                } else if (br==0x00) { /* GET_STATUS */
                    REG16(0xA0020) = (1U<<5)|(1U<<10);
                    while (!(REG16(0xA0022) & (1U<<13))) {}
                    REG16(0xA0022) |= (1U<<14);
                    while (REG16(0xA0022) & (1U<<14)) {}
                    REG16(0xA0014) = 0x0000U;
                    REG16(0xA0022) |= (1U<<15);
                } else {
                    REG16(0xA0060) = (REG16(0xA0060)&~3U)|2U;
                }
            }
            REG16(0xA0040) = (uint16_t)~(1U<<11);
        }

        if (st & (1U<<8))  { REG16(0xA0046)=0; REG16(0xA0040)=(uint16_t)~(1U<<8); }
        if (st & (1U<<10)) { REG16(0xA004A)=0; REG16(0xA0040)=(uint16_t)~(1U<<10); }
    }
    return 0;
}
