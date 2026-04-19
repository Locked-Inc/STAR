/**
 * bulk_debug.c -- bulk_in_fix.c with SCI9 debug output inlined.
 * Prints pipe/fifo state every ~second so we can see why bulk IN NAKs.
 */

#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

static void delay(volatile uint32_t n) { while (n--) { __asm__ volatile("nop"); } }

static const uint8_t s_dev[18] = {
    0x12, 0x01, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x40,
    0x09, 0x12, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01
};

static const uint8_t s_cfg[25] = {
    0x09, 0x02, 0x19, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00,
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00   /* EP 0x82 bulk IN */
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
#define PIPESEL    (USB0 + 0x64U)
#define PIPECFG    (USB0 + 0x68U)
#define PIPEBUF    (USB0 + 0x6AU)
#define PIPEMAXP   (USB0 + 0x6CU)
#define PIPE1CTR   (USB0 + 0x70U)
#define PIPE1TRE   (USB0 + 0x90U)
#define PIPE1TRN   (USB0 + 0x92U)

#define LED_INIT()  do { REG8(0x8C00A) |= 0x80U; } while (0)
#define LED_ON()    do { REG8(0x8C02A) |= 0x80U; } while (0)
#define LED_OFF()   do { REG8(0x8C02A) &= ~0x80U; } while (0)
#define LED_TGL()   do { REG8(0x8C02A) ^= 0x80U; } while (0)

static void blink(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        LED_ON();  delay(40000U);
        LED_OFF(); delay(40000U);
    }
    delay(100000U);
}

/* === SCI9 polled TX on PB7.  PCKB = 48 MHz, 115200 8N1, BRR=12. === */
#define SCI9_BASE  0x000D0020U
#define SCI9_SMR   (SCI9_BASE + 0)
#define SCI9_BRR   (SCI9_BASE + 1)
#define SCI9_SCR   (SCI9_BASE + 2)
#define SCI9_TDR   (SCI9_BASE + 3)
#define SCI9_SSR   (SCI9_BASE + 4)

static void sci9_init(void)
{
    /* Unlock PRC1, clear MSTPCRB.MSTPB22 (SCI9), lock. */
    REG16(0x000803FEU) = 0xA502U;
    REG32(0x00080014U) &= ~(1UL << 22);
    REG16(0x000803FEU) = 0xA500U;

    /* MPC PB7 = TXD9 (function 0x0A).  Unlock PFSWE and PFS register. */
    REG8(0x0008C11FU) = 0x00U;   /* PWPR: clear B0WI */
    REG8(0x0008C11FU) = 0x40U;   /* PWPR: set PFSWE */
    REG8(0x0008C19FU) = 0x0AU;   /* PB7PFS = 0x0A (TXD9) @ base+0x9F */
    REG8(0x0008C11FU) = 0x00U;   /* PWPR: clear PFSWE */
    REG8(0x0008C11FU) = 0x80U;   /* PWPR: set B0WI */

    /* PORT.B: PDR bit 7 = output, PMR bit 7 = peripheral. */
    REG8(0x0008C00BU) |= 0x80U;  /* PDR.PB7 = 1 (output) */
    REG8(0x0008C06BU) |= 0x80U;  /* PMR.PB7 = 1 (peripheral) */

    /* SCI9 setup: SCR=0, SMR=0 (8N1 async), BRR=12, settle, SCR=TE. */
    REG8(SCI9_SCR) = 0x00U;
    REG8(SCI9_SMR) = 0x00U;
    /* Try CKS=3 (PCLK/64 clock source) for 9600 baud.
     * BRR = PCLK / (2 * 2^(2*CKS-1) * baud * 16) - 1 for async
     * With CKS=3: clock divisor 64. BRR = 48000000/(64*16*2^5*9600)... hmm
     * Easier: just use 9600 @ CKS=0 with BRR=155 (48M/(32*9600)-1). */
    REG8(SCI9_BRR) = 155U;  /* 9600 @ 48 MHz, CKS=0 -> 48M/(32*156)=9615 */
    REG8(SCI9_SMR) = 0x00U;  /* CKS=00, 8N1 */
    for (volatile uint32_t i = 0; i < 10000U; i++) { __asm__ volatile("nop"); }
    REG8(SCI9_SCR) = 0x20U;  /* TE=1 */
}

static void sci9_putc(char c)
{
    /* Bounded wait; bail if TDRE never goes high. */
    for (uint32_t i = 0; i < 100000U; i++) {
        if ((REG8(SCI9_SSR) & 0x80U) != 0U) break;
    }
    REG8(SCI9_TDR) = (uint8_t)c;
}

static void sci9_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') sci9_putc('\r');
        sci9_putc(*s++);
    }
}

static void sci9_puthex16(uint16_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    sci9_putc('0'); sci9_putc('x');
    for (int8_t s = 12; s >= 0; s -= 4) {
        sci9_putc(hex[(v >> s) & 0xF]);
    }
}

static void sci9_puthex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    sci9_putc('0'); sci9_putc('x');
    for (int8_t s = 28; s >= 0; s -= 4) {
        sci9_putc(hex[(v >> s) & 0xF]);
    }
}

static void cfifo_write_current(const uint8_t *data, uint16_t len)
{
    while (!(REG16(CFIFOCTR) & (1U << 13))) {}
    REG16(CFIFOCTR) |= (1U << 14);
    while (REG16(CFIFOCTR) & (1U << 14)) {}
    for (uint16_t i = 0; i < len; i++) {
        REG8(CFIFO) = data[i];
    }
    REG16(CFIFOCTR) |= (1U << 15);
}

static volatile int      g_configured = 0;
static volatile uint32_t g_setup_count = 0;
static volatile uint16_t g_last_req    = 0;
static volatile uint32_t g_bemp_count  = 0;
static volatile uint32_t g_brdy_count  = 0;
static volatile uint32_t g_nrdy_count  = 0;

static void handle_setup(void)
{
    uint16_t st = REG16(INTSTS0);
    uint16_t c = st & 7U;

    if (c != 1U && c != 3U && c != 5U) return;

    REG16(INTSTS0) = (uint16_t) ~(1U << 3);
    REG16(DCPCTR) |= 0x0001U;

    uint16_t req = REG16(USBREQ);
    uint16_t val = REG16(USBVAL);
    uint16_t len = REG16(USBLENG);
    uint8_t  br  = (uint8_t)(req >> 8);
    g_setup_count++;
    g_last_req = req;

    REG16(CFIFOSEL) = (1U << 5);

    if (br == 0x06U) {
        uint8_t dt = (uint8_t)(val >> 8);
        if (dt == 1U) {
            uint16_t s = (len < 18U) ? len : 18U;
            cfifo_write_current(s_dev, s);
            REG16(DCPCTR) |= (1U << 2);
        } else if (dt == 2U) {
            uint16_t s = (len < 25U) ? len : 25U;
            cfifo_write_current(s_cfg, s);
            REG16(DCPCTR) |= (1U << 2);
        } else {
            REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
        }
    } else if (br == 0x05U) {
        REG16(USBADDR) = val & 0x7FU;
        REG16(DCPCTR) |= (1U << 2);
    } else if (br == 0x09U) {
        if ((val & 0xFFU) == 1U) {
            REG16(PIPESEL)  = 1U;
            /* Try writing PIPEBUF BEFORE PIPECFG. */
            REG16(PIPEBUF)  = 0x0008U;
            REG16(PIPECFG)  = 0x4012U;  /* TYPE=bulk, DIR=IN, EPNUM=2 */
            REG16(PIPEMAXP) = 64U;
            REG16(USB0 + 0x6EU) = 0U;
            REG16(PIPESEL) = 0U;

            /* CORRECT PIPEnCTR bit positions per Renesas FIT r_usb_bitdefine.h:
             *   BSTS=b15, INBUFM=b14, CSCLR=b13, CSSTS=b12, ATREPM=b10,
             *   ACLRM=b9, SQCLR=b8, SQSET=b7, SQMON=b6, PBUSY=b5,
             *   PID=[1:0] only (2 bits, mask=0x0003)
             * Prior session's "fix" used DCPCTR bit layout (wrong for PIPE1-9). */
            REG16(PIPE1CTR) = (REG16(PIPE1CTR) & ~0x3U) | 0x0U;  /* PID=NAK */
            REG16(PIPE1CTR) |= (1U << 8);   /* SQCLR (bit 8) */
            REG16(PIPE1CTR) |= (1U << 9);   /* ACLRM set */
            REG16(PIPE1CTR) &= (uint16_t)~(1U << 9);   /* ACLRM clear */
            REG16(PIPE1CTR) |= (1U << 13);  /* CSCLR (bit 13) */
            REG16(PIPE1CTR) = (REG16(PIPE1CTR) & ~0x3U) | 0x1U;  /* PID=BUF */

            REG16(BRDYENB) |= (1U << 1);
            REG16(BEMPENB) |= (1U << 1);
            REG16(USB0 + 0x38U) |= (1U << 1);  /* NRDYENB.PIPE1NRDYE */
            g_configured = 1;
        }
        REG16(DCPCTR) |= (1U << 2);
    } else if (br == 0x00U) {
        static const uint8_t zeros[2] = { 0, 0 };
        cfifo_write_current(zeros, 2U);
        REG16(DCPCTR) |= (1U << 2);
    } else if (br == 0xFEU) {
        /* Custom: return PIPE1CTR + related registers.  16 bytes:
         *   [0-1] PIPE1CTR
         *   [2-3] SYSCFG
         *   [4-5] SYSSTS0
         *   [6-7] INTSTS0
         *   [8-9] DVSTCTR0
         *  [10-11] BEMPSTS
         *  [12-13] BRDYSTS
         *  [14-15] NRDYSTS
         */
        static uint8_t dbg[16];
        const uint16_t p1 = REG16(PIPE1CTR);
        /* Read PIPECFG/PIPEBUF/PIPEMAXP via PIPESEL=1, then restore PIPESEL=0. */
        REG16(PIPESEL) = 1U;
        const uint16_t pcfg  = REG16(PIPECFG);
        const uint16_t pbuf  = REG16(PIPEBUF);
        const uint16_t pmaxp = REG16(PIPEMAXP);
        REG16(PIPESEL) = 0U;
        const uint16_t bs = REG16(BEMPSTS);
        const uint16_t rs = REG16(BRDYSTS);
        const uint16_t ns = REG16(USB0 + 0x48U);  /* NRDYSTS */
        const uint16_t fs = REG16(CFIFOSEL);
        dbg[0]  = (uint8_t)(p1);    dbg[1]  = (uint8_t)(p1 >> 8);
        dbg[2]  = (uint8_t)(pcfg);  dbg[3]  = (uint8_t)(pcfg >> 8);
        dbg[4]  = (uint8_t)(pbuf);  dbg[5]  = (uint8_t)(pbuf >> 8);
        dbg[6]  = (uint8_t)(pmaxp); dbg[7]  = (uint8_t)(pmaxp >> 8);
        dbg[8]  = (uint8_t)(bs);    dbg[9]  = (uint8_t)(bs >> 8);
        dbg[10] = (uint8_t)(rs);    dbg[11] = (uint8_t)(rs >> 8);
        dbg[12] = (uint8_t)(ns);    dbg[13] = (uint8_t)(ns >> 8);
        dbg[14] = (uint8_t)(fs);    dbg[15] = (uint8_t)(fs >> 8);
        cfifo_write_current(dbg, 16U);
        REG16(DCPCTR) |= (1U << 2);
    } else {
        REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
    }
}

int main(void)
{
    /* Clear stale DPRPU */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;
    REG16(SYSCFG) = 0x0000U;
    delay(10000U);

    LED_INIT();
    blink(1);

    /* MOSC 24 MHz * 10 = 240 MHz PLL, UCK /5 = 48 MHz exact.  Match production. */
    REG16(0x803FE) = 0xA503U;
    /* MOSCWTCR = 0x53, MOFCR = 0 (crystal) */
    REG8(0x000800A2U) = 0x53U;
    REG8(0x0008C293U) = 0x00U;
    /* MOSCCR = 0 enable main OSC */
    REG8(0x00080032U) = 0x00U;
    while ((REG8(0x8003C) & 0x01U) == 0) { __asm__ volatile("nop"); }  /* MOOVF wait */
    /* PLLCR: STC=(10*2-1)=19=0x13, PLLSRCSEL=0 (MOSC), PLIDIV=/1 -> 24*10=240 MHz */
    REG16(0x80028) = 0x1300U;
    REG8(0x8002A) = 0x00U;  /* PLLCR2 PLLEN=0 */
    while ((REG8(0x8003C) & 0x04U) == 0) { __asm__ volatile("nop"); }  /* PLOVF wait */
    /* MEMWAIT=1 for ICLK>120 MHz */
    REG8(0x0008601CU) = 0x01U;
    REG32(0x80020) = 0x21C21211U;
    REG16(0x80044) = 0x0000U;   /* PACKCR UPLLSEL=0 -> main PLL */
    REG16(0x80024) = 0x0041U;   /* SCKCR2 UCK=4 (/5) */
    REG16(0x80026) = 0x0400U;   /* SCKCR3 CKSEL=PLL */
    REG16(0x803FE) = 0xA500U;

    blink(2);

    /* STEP 3 bisect: MSTP + MPC + PDR + PMR (port config) */
    REG16(0x000803FEU) = 0xA502U;
    REG32(0x00080014U) &= ~(1UL << 22);
    REG16(0x000803FEU) = 0xA500U;
    /* PFS: PB6 = RXD9, PB7 = TXD9, function 0x0A */
    REG8(0x0008C11FU) = 0x00U;   /* PWPR clear B0WI */
    REG8(0x0008C11FU) = 0x40U;   /* PWPR set PFSWE */
    REG8(0x0008C19EU) = 0x0AU;   /* PB6PFS = 0x0A */
    REG8(0x0008C19FU) = 0x0AU;   /* PB7PFS = 0x0A */
    REG8(0x0008C11FU) = 0x00U;   /* PWPR clear PFSWE */
    REG8(0x0008C11FU) = 0x80U;   /* PWPR set B0WI */
    /* PDR: PB7 out, PB6 in */
    REG8(0x0008C00BU) |= 0x80U;
    REG8(0x0008C00BU) &= (uint8_t)~0x40U;
    /* PMR: PB6 + PB7 peripheral */
    REG8(0x0008C06BU) |= (uint8_t)(0x80U | 0x40U);
    REG8(SCI9_SCR) = 0x00U;
    REG8(SCI9_SMR) = 0x00U;
    REG8(SCI9_BRR) = 12U;
    for (volatile uint32_t i = 0; i < 1000U; i++) { __asm__ volatile("nop"); }
    REG8(SCI9_SCR) = 0x20U;
    blink(5);

    /* USB0 init */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;
    REG16(SYSCFG) = 0x0000U;
    delay(100000U);
    REG16(SYSCFG) |= (1U << 0);
    REG16(SYSCFG) |= (1U << 10);
    delay(100000U);
    REG16(DCPCFG) = 0x0000U;
    REG16(DCPMAXP) = 64U;
    REG16(DCPCTR) = 0x0001U;
    REG16(BRDYENB) = 0x0001U;
    REG16(BEMPENB) = 0x0001U;
    REG16(INTENB0) = (1U << 15) | (1U << 12) | (1U << 11) | (1U << 10) | (1U << 9) | (1U << 8);

    /* Pre-configure pipe 1 BEFORE DPRPU attach.  Some RX72N USB0 variants
     * require PIPEBUF writes to happen while SYSCFG.DPRPU=0. */
    REG16(PIPESEL)  = 1U;
    REG16(PIPECFG)  = 0x0000U;   /* clear first */
    REG16(PIPEBUF)  = 0x0008U;
    REG16(PIPECFG)  = 0x4012U;  /* TYPE=bulk, DIR=IN, EPNUM=2 */
    REG16(PIPEMAXP) = 64U;
    REG16(USB0 + 0x6EU) = 0U;    /* PIPEPERI */
    REG16(PIPESEL) = 0U;
    REG16(PIPE1CTR) = 0U;
    REG16(PIPE1CTR) |= (1U << 9);  /* SQCLR */
    REG16(PIPE1CTR) |= (1U << 10); /* ACLRM */
    REG16(PIPE1CTR) &= (uint16_t)~(1U << 10);
    REG16(PIPE1CTR) = 0x0001U;   /* PID=BUF */
    REG16(BRDYENB) |= (1U << 1);
    REG16(BEMPENB) |= (1U << 1);

    blink(3);
    REG16(SYSCFG) |= (1U << 4);
    /* SCI9 output after DPRPU up -- REMOVED for test */
    blink(4);

    volatile uint32_t pc = 0;
    uint32_t tx_counter = 0;
    uint32_t print_counter = 0;

    for (;;) {
        pc++;
        if ((pc & 0x7FFFFU) == 0) {
            LED_TGL();
        }

        uint16_t st = REG16(INTSTS0);

        if (st & (1U << 12)) {
            REG16(INTSTS0) = (uint16_t) ~(1U << 12);
        }
        if (st & (1U << 11)) {
            handle_setup();
            REG16(INTSTS0) = (uint16_t) ~(1U << 11);
        }
        if (st & (1U << 8)) {
            g_brdy_count++;
            REG16(BRDYSTS) = 0;
            REG16(INTSTS0) = (uint16_t) ~(1U << 8);
        }
        if (st & (1U << 9)) {
            g_nrdy_count++;
            REG16(INTSTS0) = (uint16_t) ~(1U << 9);
        }
        if (st & (1U << 10)) {
            g_bemp_count++;
            REG16(BEMPSTS) = 0;
            REG16(INTSTS0) = (uint16_t) ~(1U << 10);
        }

        /* Gate tx on INBUFM (bit 14 per FIT).  1 = data pending, 0 = empty. */
        if (g_configured) {
            const uint16_t p1ctr = REG16(PIPE1CTR);
            const uint16_t inbufm = p1ctr & (1U << 14);
            if (inbufm == 0U) {
                /* (1) PID=NAK (mask 0x03) */
                REG16(PIPE1CTR) = (uint16_t)((REG16(PIPE1CTR) & ~0x0003U) | 0x0000U);
                /* (2) clear BEMP + BRDY + NRDY status bit 1 (pipe 1) */
                REG16(BEMPSTS) = (uint16_t)~(1U << 1);
                REG16(BRDYSTS) = (uint16_t)~(1U << 1);
                REG16(USB0 + 0x48U) = (uint16_t)~(1U << 1);  /* NRDYSTS */
                /* (3) chg_curpipe: write RCNT|CURPIPE, spin for CURPIPE latch */
                REG16(CFIFOSEL) = (uint16_t)((REG16(CFIFOSEL) & ~0x403FU) | (1U << 14) | 1U);
                while ((REG16(CFIFOSEL) & 0x003FU) != ((1U << 5) == 0 ? 1U : 1U)) {
                    /* Just check CURPIPE bits [3:0] == 1 */
                    if ((REG16(CFIFOSEL) & 0x000FU) == 1U) break;
                }
                /* (4) FRDY wait with timeout */
                uint32_t to = 100000U;
                while (((REG16(CFIFOCTR) & (1U << 13)) == 0U) && (to-- != 0U)) { /* spin */ }
                if (to != 0U) {
                    static uint8_t buf[12] = "HELLO WORLD\n";
                    tx_counter++;
                    for (uint16_t i = 0; i < 12; i++) {
                        REG8(CFIFO) = buf[i];
                    }
                    REG16(CFIFOCTR) |= (1U << 15);  /* BVAL */
                    /* PID mask is 2 bits [1:0] per FIT. */
                    REG16(PIPE1CTR) = (uint16_t)((REG16(PIPE1CTR) & ~0x0003U) | 0x0001U);
                }
                /* CFIFO path only. */
                delay(5000U);
            }
        }

        if ((pc & 0x3FFFFFU) == 0U) {
            print_counter++;
            sci9_puts("[");
            sci9_puthex32(print_counter);
            sci9_puts("] setup=");
            sci9_puthex32(g_setup_count);
            sci9_puts(" req=");
            sci9_puthex16(g_last_req);
            sci9_puts(" cfg=");
            sci9_puthex32((uint32_t)g_configured);
            sci9_puts(" tx=");
            sci9_puthex32(tx_counter);
            sci9_puts(" brdy=");
            sci9_puthex32(g_brdy_count);
            sci9_puts(" nrdy=");
            sci9_puthex32(g_nrdy_count);
            sci9_puts(" bemp=");
            sci9_puthex32(g_bemp_count);
            sci9_puts(" p1ctr=");
            sci9_puthex16(REG16(PIPE1CTR));
            sci9_puts(" fctr=");
            sci9_puthex16(REG16(CFIFOCTR));
            sci9_puts(" fsel=");
            sci9_puthex16(REG16(CFIFOSEL));
            sci9_puts(" bsts=");
            sci9_puthex16(REG16(BEMPSTS));
            sci9_puts(" rsts=");
            sci9_puthex16(REG16(BRDYSTS));
            sci9_puts(" intsts=");
            sci9_puthex16(REG16(INTSTS0));
            sci9_puts("\n");
        }
    }
    return 0;
}
