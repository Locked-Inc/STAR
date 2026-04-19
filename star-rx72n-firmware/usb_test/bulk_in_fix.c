/**
 * @file bulk_in_fix.c
 * @brief Minimal RX72N USB test: enumerate vendor device with 1 bulk IN pipe
 *        and continuously transmit a known string on it.
 *
 * Builds on hoco_pid_fix.c (which proved DCP enumeration works).  Adds:
 *   - Configuration descriptor advertising 1 interface with 1 bulk IN EP (0x81)
 *   - SET_CONFIGURATION(1) triggers pipe 1 setup:
 *       PIPECFG = bulk IN EP1
 *       PIPEBUF = BUFNMB=8 BUFSIZE=0 (1*64 byte DPRAM slot)
 *       PIPEMAXP = 64
 *       PIPE1CTR = PID_BUF
 *   - Main loop writes "BULK1 " to pipe 1 via CFIFO (CURPIPE=1, ISEL=1)
 *     every host poll so /dev/ttyACMx (or generic device handler) sees
 *     a steady stream of bytes.
 *
 * If this enumerates as 1209:0002 with bulk IN and produces bytes, we
 * know RX72N bulk IN works with this exact sequence and can propagate
 * that sequence back into libs/rx_usb.  If it doesn't produce bytes,
 * RX72N bulk IN requires something we haven't found in the manual yet
 * and the production stack can't be fixed blind.
 */

#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

static void delay(volatile uint32_t n) { while (n--) { __asm__ volatile("nop"); } }

/* 18-byte device descriptor: vendor class, 1209:0002. */
static const uint8_t s_dev[18] = {
    0x12, 0x01, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x40,
    0x09, 0x12, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01
};

/* 25-byte config descriptor:
 *   9-byte config (wTotalLength=25, 1 interface)
 *   9-byte interface (0 alt, 1 endpoint, vendor class)
 *   7-byte endpoint (EP 0x81 IN, bulk, 64 max, 0 interval)
 */
static const uint8_t s_cfg[25] = {
    0x09, 0x02, 0x19, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,       /* config */
    0x09, 0x04, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0x00,       /* interface */
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00                    /* EP 0x81 bulk IN 64B */
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

/* Write data to CFIFO on the current pipe selection, 8-bit MBW. */
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

static volatile int g_configured = 0;

static void handle_setup(void)
{
    uint16_t st = REG16(INTSTS0);
    uint16_t c = st & 7U;

    if (c != 1U && c != 3U && c != 5U) return;

    REG16(INTSTS0) = (uint16_t) ~(1U << 3);
    REG16(DCPCTR) |= 0x0001U;   /* PID=BUF (critical RX72N quirk) */

    uint16_t req = REG16(USBREQ);
    uint16_t val = REG16(USBVAL);
    uint16_t len = REG16(USBLENG);
    uint8_t  br  = (uint8_t)(req >> 8);

    /* Select DCP for CFIFO write. */
    REG16(CFIFOSEL) = (1U << 5);

    if (br == 0x06U) {   /* GET_DESCRIPTOR */
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
            REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;   /* STALL */
        }
    } else if (br == 0x05U) {   /* SET_ADDRESS */
        REG16(USBADDR) = val & 0x7FU;
        REG16(DCPCTR) |= (1U << 2);
    } else if (br == 0x09U) {   /* SET_CONFIGURATION */
        if ((val & 0xFFU) == 1U) {
            /* CORRECT PIPEnCTR (n=1..9) bit layout per Renesas FIT
             * r_usb_bitdefine.h (proven 2026-04-18 via bulk_debug.c):
             *   bit 15 BSTS (RO)
             *   bit 14 INBUFM (IN buffer monitor)
             *   bit 13 CSCLR (control transfer split status clear)
             *   bit 12 CSSTS (c-split status)
             *   bit 10 ATREPM (auto repeat mode)
             *   bit  9 ACLRM  (auto-clear mode trigger)
             *   bit  8 SQCLR  (sequence bit clear)
             *   bit  7 SQSET  (sequence bit set)
             *   bit  6 SQMON  (sequence bit monitor)
             *   bit  5 PBUSY  (pipe busy)
             *   bits [1:0] PID (NAK=0, BUF=1, STALL=2) -- 2 bits not 3
             */
            REG16(PIPESEL)  = 1U;
            REG16(PIPECFG)  = 0x4011U;   /* TYPE=bulk, DIR=IN, EPNUM=1 */
            REG16(PIPEBUF)  = 0x0008U;   /* BUFNMB=8, BUFSIZE=0 (no-op on RX72N IP0) */
            REG16(PIPEMAXP) = 64U;
            REG16(PIPESEL)  = 0U;        /* deselect per FIT pipe_init */

            REG16(PIPE1CTR) = (REG16(PIPE1CTR) & ~0x3U) | 0x0U;     /* PID=NAK */
            REG16(PIPE1CTR) |= (1U << 8);                            /* SQCLR b8 */
            REG16(PIPE1CTR) |= (1U << 9);                            /* ACLRM b9 set */
            REG16(PIPE1CTR) &= (uint16_t)~(1U << 9);                 /* ACLRM clear */
            REG16(PIPE1CTR) |= (1U << 13);                           /* CSCLR b13 */
            REG16(PIPE1CTR) = (REG16(PIPE1CTR) & ~0x3U) | 0x1U;     /* PID=BUF */
            g_configured = 1;
        }
        REG16(DCPCTR) |= (1U << 2);
    } else if (br == 0x00U) {   /* GET_STATUS */
        static const uint8_t zeros[2] = { 0, 0 };
        cfifo_write_current(zeros, 2U);
        REG16(DCPCTR) |= (1U << 2);
    } else {
        REG16(DCPCTR) = (REG16(DCPCTR) & ~3U) | 2U;
    }
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
    blink(1);

    /* HOCO PLL 192 MHz, UCK /4 = 48 MHz (proven path from hoco_pid_fix). */
    REG16(0x803FE) = 0xA503U;
    REG8(0x80036) = 0x00U;
    while ((REG8(0x8003C) & 0x08U) == 0) { __asm__ volatile("nop"); }
    REG16(0x80028) = 0x1710U;
    REG8(0x8002A) = 0x00U;
    while ((REG8(0x8003C) & 0x04U) == 0) { __asm__ volatile("nop"); }
    REG8(0x8101C) = 0x01U;
    REG32(0x80020) = 0x21C21211U;
    REG16(0x80044) = 0x0001U;
    REG16(0x80024) = 0x0031U;
    REG16(0x80026) = 0x0400U;
    REG16(0x803FE) = 0xA500U;

    blink(2);

    /* USB0 init */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;
    REG16(SYSCFG) = 0x0000U;
    delay(100000U);
    REG16(SYSCFG) |= (1U << 0);   /* USBE */
    REG16(SYSCFG) |= (1U << 10);  /* SCKE */
    delay(100000U);
    REG16(DCPCFG) = 0x0000U;
    REG16(DCPMAXP) = 64U;
    REG16(DCPCTR) = 0x0001U;
    REG16(BRDYENB) = 0x0001U;
    REG16(BEMPENB) = 0x0001U;
    REG16(INTENB0) = (1U << 15) | (1U << 12) | (1U << 11) | (1U << 10) | (1U << 8);

    blink(3);
    REG16(SYSCFG) |= (1U << 4);   /* DPRPU */
    blink(4);

    volatile uint32_t pc = 0;
    uint32_t tx_counter = 0;
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

        if (st & (1U << 8))  { REG16(BRDYSTS) = 0; REG16(INTSTS0) = (uint16_t) ~(1U << 8);  }

        /* BEMP: buffer empty → pipe ready for next write. */
        if (st & (1U << 10)) { REG16(BEMPSTS) = 0; REG16(INTSTS0) = (uint16_t) ~(1U << 10); }

        /* Once host has configured us, send a test pattern on pipe 1
         * whenever the buffer is empty (INBUFM bit 14 = 0). */
        if (g_configured && (REG16(PIPE1CTR) & (1U << 14)) == 0U) {
            /* Select CFIFO for pipe 1 IN, 8-bit MBW. */
            REG16(CFIFOSEL) = (1U) | (1U << 5);
            /* Wait for CURPIPE latch. */
            while ((REG16(CFIFOSEL) & 0xFU) != 1U) { /* spin */ }

            static uint8_t buf[16] = "BULK1 0000\r\n\0\0\0";
            tx_counter++;
            buf[6] = (uint8_t)('0' + ((tx_counter / 1000U) % 10U));
            buf[7] = (uint8_t)('0' + ((tx_counter / 100U) % 10U));
            buf[8] = (uint8_t)('0' + ((tx_counter / 10U) % 10U));
            buf[9] = (uint8_t)('0' + (tx_counter % 10U));
            cfifo_write_current(buf, 12);

            /* Small delay so we don't overfill. */
            delay(5000U);
        }
    }
    return 0;
}
