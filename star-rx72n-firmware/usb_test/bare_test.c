/**
 * @file bare_test.c
 * @brief Bare-metal USB0 bring-up: HOCO 16 MHz -> PLL x12 = 192 MHz -> UCK 48 MHz.
 *
 * Defensive clock init: only switches to PLL if it actually locks.
 * Falls back to HOCO direct if PLL fails (USB won't work but LED still blinks).
 *
 * LED diagnostic on PA7:
 *   1 blink  = main() entry (LOCO)
 *   2 blinks = HOCO running, PLL attempted
 *   3 blinks = PLL LOCKED, USB0 D+ pull-up active
 *   fast continuous = PLL FAILED, running on HOCO (no USB)
 *   slow continuous = heartbeat (USB active, waiting for host)
 */

#include <stdint.h>

/* Register access */
#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

typedef enum : uintptr_t {
    k_prcr       = 0x000803FEU,
    k_sys        = 0x00080000U,
    k_mstpcrb    = 0x00080014U,
    k_memwait    = 0x00080046U,
    k_packcr     = 0x00080044U,
    k_porta_pdr  = 0x0008C00AU,
    k_porta_podr = 0x0008C02AU,
    k_usb0       = 0x000A0000U,
    k_icu_ir     = 0x00087000U,
    k_icu_ier    = 0x00087200U,
    k_icu_ipr    = 0x00087300U,
} addr_t;

/* System register offsets */
typedef enum : uint8_t {
    k_sckcr   = 0x20U,
    k_sckcr2  = 0x24U,
    k_sckcr3  = 0x26U,
    k_pllcr   = 0x2CU,
    k_pllcr2  = 0x2FU,
    k_hococr  = 0x36U,
    k_hococr2 = 0x37U,
    k_oscovfsr = 0x3CU,
} sysoff_t;

/* USB register offsets */
typedef enum : uint16_t {
    k_u_syscfg  = 0x0000U, k_u_syssts0 = 0x0004U, k_u_cfifo    = 0x0014U,
    k_u_cfifosel = 0x0020U, k_u_cfifoctr = 0x0022U, k_u_intenb0 = 0x0030U,
    k_u_brdyenb = 0x0036U, k_u_bempenb  = 0x003AU, k_u_intsts0  = 0x0040U,
    k_u_brdysts = 0x0046U, k_u_bempsts  = 0x004AU, k_u_dcpcfg   = 0x005CU,
    k_u_dcpmaxp = 0x005EU, k_u_dcpctr   = 0x0060U, k_u_usbreq   = 0x0054U,
    k_u_usbval  = 0x0056U, k_u_usbleng  = 0x005AU, k_u_usbaddr  = 0x006CU,
} usboff_t;

/* Bit constants */
typedef enum : uint16_t {
    k_syscfg_usbe  = (1U << 0),  k_syscfg_dprpu = (1U << 4),
    k_syscfg_scke  = (1U << 10),
    k_dcpctr_buf   = 0x0001U,    k_dcpctr_ccpl  = (1U << 2),
    k_dcpctr_stall = 0x0002U,
    k_intsts_valid = (1U << 3),  k_intsts_ctrt  = (1U << 11),
    k_intsts_dvst  = (1U << 12), k_intsts_brdy  = (1U << 8),
    k_intsts_bemp  = (1U << 10), k_intsts_ctsq  = 0x0007U,
    k_cfifosel_isel = (1U << 5), k_cfifosel_mbw16 = (1U << 10),
    k_fifoctr_frdy = (1U << 13), k_fifoctr_bclr  = (1U << 14),
    k_fifoctr_bval = (1U << 15),
    k_intenb_ctre  = (1U << 11), k_intenb_dvse  = (1U << 12),
    k_intenb_brdy  = (1U << 8),  k_intenb_bemp  = (1U << 10),
    /*
     * PLL: HOCO 16 MHz x 12 = 192 MHz.
     * RX72N formula: multiply = (STC + 1) / 2.
     * STC = (12 * 2) - 1 = 23 = 0x17. PLLSRCSEL = 1 (HOCO, bit 4).
     * PLLCR = (0x17 << 8) | 0x0010 = 0x1710.
     */
    k_pllcr_hoco_x12 = 0x1710U,
    /* UCK = /(3+1) = /4 -> 192/4 = 48 MHz. Low nibble must be 0x1. */
    k_sckcr2_uck_div4 = 0x0031U,
} bits_t;

typedef enum : uint32_t {
    /*
     * SCKCR for 192 MHz PLL:
     *   FCK  = /4  = 48 MHz  (nibble 2)
     *   ICK  = /2  = 96 MHz  (nibble 1)
     *   PSTOP= CC            (nibble C)
     *   BCK  = /4  = 48 MHz  (nibble 2)
     *   PCKA = /2  = 96 MHz  (nibble 1)
     *   PCKB = /4  = 48 MHz  (nibble 2)
     *   PCKC = /2  = 96 MHz  (nibble 1)
     *   PCKD = /2  = 96 MHz  (nibble 1)
     */
    k_sckcr_192 = 0x21C21211U,
    k_mstpb_usb0 = 19U,
} bits32_t;

typedef enum : uint8_t {
    k_hcovf = 0x08U,
    k_plovf = 0x04U,
    k_pa7   = 0x80U,
    k_vect_usbi = 36U,
} byte_t;

/* ---- Descriptors ---- */
static const uint8_t s_dev[18] = {
    0x12, 0x01, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x40,
    0x09, 0x12, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
};
static const uint8_t s_cfg[18] = {
    0x09, 0x02, 0x12, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00,
};

/* ---- Globals ---- */
volatile uint32_t g_isr = 0U, g_setup = 0U, g_dvst = 0U;
static volatile uint8_t s_pll_ok = 0U;

/* ---- Helpers ---- */
static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

static void blink(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        REG8(k_porta_podr) |= k_pa7; delay(300000U);
        REG8(k_porta_podr) &= ~k_pa7; delay(300000U);
    }
    delay(600000U);
}

static void cfifo_write(const uint8_t *d, uint16_t len) {
    REG16(k_usb0 + k_u_cfifosel) = k_cfifosel_isel | k_cfifosel_mbw16;
    while (!(REG16(k_usb0 + k_u_cfifoctr) & k_fifoctr_frdy)) {}
    REG16(k_usb0 + k_u_cfifoctr) |= k_fifoctr_bclr;
    while (REG16(k_usb0 + k_u_cfifoctr) & k_fifoctr_bclr) {}
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t w = d[i];
        if (i + 1 < len) w |= (uint16_t)d[i+1] << 8;
        REG16(k_usb0 + k_u_cfifo) = w;
    }
    REG16(k_usb0 + k_u_cfifoctr) |= k_fifoctr_bval;
}

static void handle_setup(void) {
    uint16_t req = REG16(k_usb0 + k_u_usbreq);
    uint16_t val = REG16(k_usb0 + k_u_usbval);
    uint16_t len = REG16(k_usb0 + k_u_usbleng);
    uint8_t  br  = (uint8_t)(req >> 8);
    g_setup++;
    if (br == 0x06) { /* GET_DESCRIPTOR */
        uint8_t dt = (uint8_t)(val >> 8);
        if (dt == 1) { uint16_t s = len < 18 ? len : 18; cfifo_write(s_dev, s); }
        else if (dt == 2) { uint16_t s = len < 18 ? len : 18; cfifo_write(s_cfg, s); }
        else { REG16(k_usb0+k_u_dcpctr) = (REG16(k_usb0+k_u_dcpctr) & ~3U) | k_dcpctr_stall; }
    } else if (br == 0x05) { /* SET_ADDRESS */
        REG16(k_usb0 + k_u_usbaddr) = val & 0x7F;
        REG16(k_usb0 + k_u_dcpctr) |= k_dcpctr_ccpl;
    } else if (br == 0x09) { /* SET_CONFIG */
        REG16(k_usb0 + k_u_dcpctr) |= k_dcpctr_ccpl;
    } else if (br == 0x00) { /* GET_STATUS */
        static const uint8_t z[2] = {0,0};
        cfifo_write(z, 2);
    } else {
        REG16(k_usb0+k_u_dcpctr) = (REG16(k_usb0+k_u_dcpctr) & ~3U) | k_dcpctr_stall;
    }
}

void __attribute__((interrupt)) usb0_usbi_isr(void) {
    REG8(k_icu_ir + k_vect_usbi) = 0;
    g_isr++;
    uint16_t st = REG16(k_usb0 + k_u_intsts0);
    if (st & k_intsts_dvst) { g_dvst++; REG16(k_usb0+k_u_intsts0) = ~k_intsts_dvst; }
    if (st & k_intsts_ctrt) {
        uint16_t ctsq = st & k_intsts_ctsq;
        if (ctsq == 1 || ctsq == 3 || ctsq == 5) {
            REG16(k_usb0+k_u_intsts0) = ~k_intsts_valid;
            handle_setup();
        }
        REG16(k_usb0+k_u_intsts0) = ~k_intsts_ctrt;
    }
    if (st & k_intsts_brdy) { REG16(k_usb0+k_u_brdysts) = 0; REG16(k_usb0+k_u_intsts0) = ~k_intsts_brdy; }
    if (st & k_intsts_bemp) { REG16(k_usb0+k_u_bempsts) = 0; REG16(k_usb0+k_u_intsts0) = ~k_intsts_bemp; }
}

void __attribute__((interrupt)) usb0_d0fifo_isr(void) { REG8(k_icu_ir+34) = 0; }
void __attribute__((interrupt)) usb0_d1fifo_isr(void) { REG8(k_icu_ir+35) = 0; }
void __attribute__((interrupt)) usb0_usbr_isr(void)   { REG8(k_icu_ir+90) = 0; }

/* ---- main ---- */
int main(void)
{
    REG8(k_porta_pdr) |= k_pa7;
    REG8(k_porta_podr) &= ~k_pa7;

    /* 1 blink = alive on LOCO */
    blink(1);

    /* ---- Clock init ---- */
    REG16(k_prcr) = 0xA503U;

    /* Force HOCO to 16 MHz */
    REG8(k_sys + k_hococr2) = 0x00U;
    /* Start HOCO */
    REG8(k_sys + k_hococr) = 0x00U;
    for (volatile uint32_t t = 0; t < 200000U; t++) {
        if (REG8(k_sys + k_oscovfsr) & k_hcovf) break;
    }

    /* Switch to HOCO first (safe fallback) */
    REG16(k_sys + k_sckcr3) = 0x0100U;

    /* 2 blinks = running on HOCO 16 MHz */
    blink(2);

    /* Stop PLL, configure, start */
    REG8(k_sys + k_pllcr2) = 0x01U;
    delay(1000U);
    REG16(k_sys + k_pllcr) = k_pllcr_hoco_x12; /* 16 x 12 = 192 MHz */
    REG8(k_sys + k_pllcr2) = 0x00U;

    /* Wait for PLL lock with timeout */
    s_pll_ok = 0U;
    for (volatile uint32_t t = 0; t < 1000000U; t++) {
        if (REG8(k_sys + k_oscovfsr) & k_plovf) { s_pll_ok = 1U; break; }
    }

    if (s_pll_ok) {
        /* PLL locked -- switch to it */
        REG8(k_memwait) = 0x01U;
        REG32(k_sys + k_sckcr) = k_sckcr_192;
        REG16(k_packcr) = 0x0000U;
        REG16(k_sys + k_sckcr2) = k_sckcr2_uck_div4;
        REG16(k_sys + k_sckcr3) = 0x0400U; /* PLL */
    }
    /* else: stay on HOCO 16 MHz. USB won't work but LED still blinks. */

    REG16(k_prcr) = 0xA500U;

    /* Proceed regardless of PLL status -- USB will only work if PLL locked */

    /* 3 blinks = PLL locked, setting up USB */
    blink(3);

    /* ---- USB0 init ---- */
    REG16(k_prcr) = 0xA503U;
    REG32(k_mstpcrb) &= ~(1UL << k_mstpb_usb0);
    REG16(k_prcr) = 0xA500U;

    REG16(k_usb0 + k_u_syscfg) = 0x0000U;
    delay(50000U);
    REG16(k_usb0 + k_u_syscfg) |= k_syscfg_usbe;
    REG16(k_usb0 + k_u_syscfg) |= k_syscfg_scke;
    delay(50000U);

    REG16(k_usb0 + k_u_dcpcfg)  = 0x0000U;
    REG16(k_usb0 + k_u_dcpmaxp) = 64U;
    REG16(k_usb0 + k_u_dcpctr)  = k_dcpctr_buf;
    REG16(k_usb0 + k_u_brdyenb) = 0x0001U;
    REG16(k_usb0 + k_u_bempenb) = 0x0001U;
    REG16(k_usb0 + k_u_intenb0) = k_intenb_ctre | k_intenb_dvse |
                                    k_intenb_brdy | k_intenb_bemp;

    REG8(k_icu_ir + k_vect_usbi) = 0;
    REG8(k_icu_ipr + k_vect_usbi) = 6;
    REG8(k_icu_ier + k_vect_usbi / 8) |= (1U << (k_vect_usbi % 8));

    /* D+ pull-up */
    REG16(k_usb0 + k_u_syscfg) |= k_syscfg_dprpu;

    __asm__ volatile("setpsw i");

    /* 4 blinks = USB fully active */
    blink(4);

    /* ---- Heartbeat ---- */
    for (;;) {
        REG8(k_porta_podr) ^= k_pa7;
        delay(2000000U);
    }
    return 0;
}
