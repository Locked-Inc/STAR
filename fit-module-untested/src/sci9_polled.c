/* sci9_polled.c -- polled-mode SCI9 (115200 8N1) on PB6/PB7 -> Cypress -> ttyACM0.
 *
 * Self-contained port of star-rx72n-firmware/motors_rtos/sci9.c, with the
 * production HAL accessors (mpc(), portb(), system_regs(), prcr_reg()) replaced
 * by raw register addresses so this file builds inside the fit-module-untested
 * sandbox without depending on the production tree.
 *
 * BRR calculation note (from motors_rtos/sci9.c:45):
 *   The STAR PCB's effective PCLKB ends up at 120 MHz under the production
 *   clock setup, so BRR=31 gives correct 115200 baud (= 120e6 / (32 * 32)).
 *   Under r_bsp's clock setup we configured BSP_CFG_PCKB_DIV=4 -> nominal
 *   60 MHz, which would want BRR=15. We use a CHOSEN_BRR enum below; if
 *   115200 doesn't appear at the host, try the other value.
 */

#include "sci9_polled.h"

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Register addresses. RX72N HW manual chapter 11 (System Control), 22 (PORT),
 * 23 (MPC), 36 (SCI). Each enum constant carries its source.
 * ============================================================================ */
typedef enum : uintptr_t {
    /* SCI9 register block. SMR=+0, BRR=+1, SCR=+2, TDR=+3, SSR=+4, SCMR=+6, SEMR=+7. */
    k_sci9_base   = 0x000D0020U,

    /* MPC + Port B (PB6 RXD9, PB7 TXD9). */
    k_pwpr_addr   = 0x0008C11FU,
    k_portb_pdr   = 0x0008C00BU,
    k_portb_pmr   = 0x0008C06BU,
    k_pb6pfs_addr = 0x0008C140U + 0x0BU * 8U + 6U,
    k_pb7pfs_addr = 0x0008C140U + 0x0BU * 8U + 7U,

    /* System control: PRCR (write-protect for PRC0/PRC1) + MSTPCRC (SCI9 stop bit). */
    k_prcr_addr      = 0x000803FEU,
    k_mstpcrc_addr   = 0x00080018U,
} sci9_addrs_t;

#define REG8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

typedef enum : uint8_t {
    k_psel_sci9       = 0x0AU,  /* MPC PSEL value for SCI9 RXD/TXD on PB6/PB7. uart_test/sci9.c:38 */
    k_pb6_mask        = (uint8_t)(1U << 6U),
    k_pb7_mask        = (uint8_t)(1U << 7U),

    k_pwpr_clear      = 0x00U,
    k_pwpr_pfswe      = 0x40U,
    k_pwpr_b0wi       = 0x80U,

    /* SCI9 register fields. */
    k_smr_8n1_async   = 0x00U,
    k_scr_disabled    = 0x00U,
    k_scr_te_re       = 0x30U,  /* TE=1, RE=1; no interrupts (polled mode) */
    k_ssr_tdre        = 0x80U,
    k_semr_default    = 0x00U,

    /* BRR for 115200 baud at PCLKB = 60 MHz (FIT r_bsp clock setup with
     * BSP_CFG_PCKB_DIV=4 -> 240/4 = 60 MHz).
     *
     * RX72N async-mode formula (HW manual ch.36, ABCS=0, BGDM=0, CKS=0):
     *   baud = PCLKB / (32 * (BRR + 1))
     *
     *   BRR=15: 60e6 / (32 * 16) = 117188 baud (1.7% over 115200, in-spec)
     *   BRR=7 : 60e6 / (32 *  8) = 234375 baud (way too fast)
     *
     * NOTE: motors_rtos/sci9.c uses BRR=31 because its custom clock.c lands
     * PCLKB at 120 MHz -- the comment in that file ("PCKB is actually 120
     * MHz on STAR config") refers to the production HAL's clock tree, not
     * this FIT-r_bsp tree. With FIT we have PCLKB=60, so BRR is half. */
    k_brr_115200      = 15U,

    k_hex_nibble_mask = 0x0FU,
} sci9_cfg_t;

typedef enum : uint16_t {
    k_brr_settle_loops = 1000U,  /* ~one bit time at 115200 worst case */
    k_prcr_unlock      = 0xA50BU,  /* PRC0+PRC1 unlock; matches imu_test/main.c */
    k_prcr_lock        = 0xA500U,
} sci9_misc_t;

typedef enum : uint32_t {
    k_mstpcrc_sci9_bit = (1UL << 26U),  /* MSTPCRC.MSTPC26 = SCI9 module-stop. RX72N HW manual ch.11 */
} sci9_mstp_t;

/* ============================================================================
 * SCI9 register accessors.
 * ============================================================================ */
static inline volatile uint8_t *sci9_smr (void) { return (volatile uint8_t *)(k_sci9_base + 0U); }
static inline volatile uint8_t *sci9_brr (void) { return (volatile uint8_t *)(k_sci9_base + 1U); }
static inline volatile uint8_t *sci9_scr (void) { return (volatile uint8_t *)(k_sci9_base + 2U); }
static inline volatile uint8_t *sci9_tdr (void) { return (volatile uint8_t *)(k_sci9_base + 3U); }
static inline volatile uint8_t *sci9_ssr (void) { return (volatile uint8_t *)(k_sci9_base + 4U); }
static inline volatile uint8_t *sci9_semr(void) { return (volatile uint8_t *)(k_sci9_base + 7U); }

/* ============================================================================
 * Init -- mirrors motors_rtos/sci9.c:102-140 step-for-step.
 * ============================================================================ */
void sci9_init(void)
{
    /* Step 1: release SCI9 from module stop. PRCR unlock for PRC0+PRC1. */
    REG16(k_prcr_addr)    = k_prcr_unlock;
    REG32(k_mstpcrc_addr) &= ~k_mstpcrc_sci9_bit;
    REG16(k_prcr_addr)    = k_prcr_lock;

    /* Step 2: PFS for PB6/PB7. PWPR sequence per RX72N HW manual ch.23.2.1. */
    REG8(k_pwpr_addr) = k_pwpr_clear;
    REG8(k_pwpr_addr) = k_pwpr_pfswe;
    REG8(k_pb6pfs_addr) = k_psel_sci9;
    REG8(k_pb7pfs_addr) = k_psel_sci9;
    REG8(k_pwpr_addr) = k_pwpr_clear;
    REG8(k_pwpr_addr) = k_pwpr_b0wi;

    /* Step 3: PDR (TX out, RX in) BEFORE PMR=peripheral. uart_test/sci9.c:120-124 */
    REG8(k_portb_pdr) |=  k_pb7_mask;
    REG8(k_portb_pdr) &= (uint8_t)~k_pb6_mask;
    REG8(k_portb_pmr) |= (uint8_t)(k_pb6_mask | k_pb7_mask);

    /* Step 4: SCR off, configure SMR + BRR, settle, then enable TX+RX.
     * Don't touch SCMR (reset default 0xF2 is correct). */
    *sci9_scr() = k_scr_disabled;
    *sci9_smr() = k_smr_8n1_async;
    *sci9_brr() = k_brr_115200;

    for (volatile uint32_t i = 0; i < (uint32_t)k_brr_settle_loops; i++) {
        __asm__ volatile("nop");
    }

    *sci9_scr()  = k_scr_te_re;
    *sci9_semr() = k_semr_default;
}

/* ============================================================================
 * Polled TX. Spin on TDRE, write TDR.
 * ============================================================================ */
void sci9_putc(char c)
{
    while ((*sci9_ssr() & (uint8_t)k_ssr_tdre) == 0U) {
        /* spin */
    }
    *sci9_tdr() = (uint8_t)c;
}

void sci9_puts(const char *s)
{
    if (s == NULL) { return; }
    while (*s != '\0') {
        if (*s == '\n') {
            sci9_putc('\r');
        }
        sci9_putc(*s++);
    }
}

void sci9_write(const uint8_t *buf, size_t len)
{
    if (buf == NULL) { return; }
    for (size_t i = 0; i < len; i++) {
        sci9_putc((char)buf[i]);
    }
}

void sci9_puthex32(uint32_t v)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    sci9_putc('0');
    sci9_putc('x');
    for (int8_t shift = 28; shift >= 0; shift -= 4) {
        sci9_putc(hex_digits[(v >> (uint8_t)shift) & (uint8_t)k_hex_nibble_mask]);
    }
}

void sci9_puthex16(uint16_t v)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    sci9_putc('0');
    sci9_putc('x');
    for (int8_t shift = 12; shift >= 0; shift -= 4) {
        sci9_putc(hex_digits[(v >> (uint8_t)shift) & (uint8_t)k_hex_nibble_mask]);
    }
}
