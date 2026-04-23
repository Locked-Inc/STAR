/**
 * @file sci9_debug.c
 * @brief Minimal polled-mode SCI9 (TXD9 = PB7, RXD9 = PB6) debug UART for encoder_test.
 *
 * @details
 * The STAR board routes SCI9 to the on-board CY7C65213 USB-to-UART bridge
 * which appears as /dev/ttyACM0 on the host (or COMx on Windows).  This
 * file provides just enough init + putc/puts to emit ASCII status from
 * the firmware so we can debug the USB CDC bring-up without needing the
 * USB device to actually enumerate.
 *
 * Uses the rx_hal struct accessors (mpc(), portb(), system_regs(), prcr_reg())
 * so the addresses are all compiler-verified via static_assert.
 *
 * Baud = 115200. SCI9 is in the SCI7-11 group that uses PCLKA (120 MHz),
 * not PCLKB. At CKS=0, ABCS=0:
 *   BRR = (PCLKA / (32 * baud)) - 1 = 120000000 / 3686400 - 1 = 31.55 -> 31
 *   Actual baud = PCLKA / (32 * (BRR+1)) = 117187 Hz (+1.7% vs 115200, in tolerance).
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* ==========================================================================
 * SCI9 register access via raw addresses (rx72n_sci_regs.h doesn't expose
 * the per-channel struct as cleanly as we'd like for a quick polled driver).
 * The base 0x000D0020 is taken from the verified header.
 * Register offsets per RX72N HW manual: SMR=0, BRR=1, SCR=2, TDR=3, SSR=4.
 * ========================================================================== */
typedef enum : uintptr_t {
    k_sci9_base = 0x000D0020U,
} sci9_addrs_t;

typedef enum : uint8_t {
    k_psel_sci9        = 0x0AU, /**< SCI9 RXD/TXD function */
    k_pb6_mask         = (uint8_t)(1U << 6U),
    k_pb7_mask         = (uint8_t)(1U << 7U),
    k_mstpc_sci9_bit   = 26U, /* MSTPCRC.MSTPC26 per RX72N HW manual page 410 */
    k_pwpr_unlock_b0   = 0x00U,
    k_pwpr_pfswe       = 0x40U,
    k_pwpr_b0wi        = 0x80U,
    k_brr_115200       = 31U, /* SCI9 uses PCLKA=120 MHz (SCI7-11 group); BRR=31 @ CKS=0 ABCS=0 -> ~117 kbaud (1.7% over 115200) */
    k_smr_8n1_async    = 0x00U,
    k_scr_te_only      = 0x20U, /**< CKE=00, MPIE=0, RIE=0, TE=1, RE=0 */
    k_scr_txrx_enabled = 0x30U, /**< CKE=00, MPIE=0, RIE=0, TE=1, RE=1 */
    k_scr_disabled     = 0x00U,
    k_ssr_tdre         = 0x80U,
    k_scmr_default     = 0xF2U,
    k_semr_default     = 0x00U,
    k_hex_nibble_mask  = 0x0FU,
} sci9_cfg_t;

typedef enum : uint16_t {
    k_brr_settle_loops = 1000U, /**< Match production uart.c (~8.68 us @ 115200) */
} sci9_settle_t;

typedef enum : uint8_t {
    k_hex_shift_init32 = 28U,
    k_hex_shift_init16 = 12U,
    k_hex_shift_step   = 4U,
} hex_shifts_t;

/* ==========================================================================
 * SCI9 register accessors (raw addresses since the header doesn't give us a
 * per-channel struct for the SCIi/SCIj quirks).
 * ========================================================================== */
static inline volatile uint8_t *sci9_smr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 0U);
}
static inline volatile uint8_t *sci9_brr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 1U);
}
static inline volatile uint8_t *sci9_scr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 2U);
}
static inline volatile uint8_t *sci9_tdr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 3U);
}
static inline volatile uint8_t *sci9_ssr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 4U);
}
static inline volatile uint8_t *sci9_scmr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 6U);
}
static inline volatile uint8_t *sci9_semr(void)
{
    return (volatile uint8_t *)(k_sci9_base + 7U);
}

/* ==========================================================================
 * Initialise SCI9 for 115200 8N1 polled output on PB7 (TXD9).
 * ========================================================================== */
void sci9_debug_init(void)
{
    /* Step 1: release SCI9 from module stop. SCI9 = MSTPCRC bit 26 per
     * RX72N HW manual page 410 (NOT MSTPCRB.bit22 -- that's PDC). */
    *prcr_reg() = k_rx_prcr_unlock_prc0_prc1;
    system_regs()->mstpcrc &= ~(uint32_t)(1UL << (uint32_t)k_mstpc_sci9_bit);
    *prcr_reg() = k_rx_prcr_lock;

    /* Step 2: configure PB6 (RXD9) and PB7 (TXD9) via MPC.  Use the struct
     * accessor so the compiler computes the correct PFS offsets via
     * static_assert-checked layout. */
    mpc()->pwpr   = k_pwpr_unlock_b0; /* clear B0WI */
    mpc()->pwpr   = k_pwpr_pfswe;     /* allow PFS writes */
    mpc()->pb6pfs = k_psel_sci9;
    mpc()->pb7pfs = k_psel_sci9;
    mpc()->pwpr   = k_pwpr_unlock_b0; /* clear PFSWE */
    mpc()->pwpr   = k_pwpr_b0wi;      /* re-protect */

    /* Step 3: PDR before PMR (production uart.c does this).  PB7 = output
     * for TX, PB6 = input for RX.  Then flip PMR to peripheral mode. */
    portb()->pdr |= (uint8_t)k_pb7_mask;
    portb()->pdr &= (uint8_t) ~k_pb6_mask;
    portb()->pmr |= (uint8_t)(k_pb6_mask | k_pb7_mask);

    /* Step 4: SCI9 init.  Match the production order:
     *   SCR=0 -> SMR -> BRR -> settle -> SCR=TE|RE -> SEMR.
     * Don't touch SCMR (reset default 0xF2 is correct). */
    *sci9_scr() = k_scr_disabled;
    *sci9_smr() = k_smr_8n1_async;
    *sci9_brr() = k_brr_115200;

    /* Wait roughly one bit time before enabling TX/RX. */
    for (volatile uint32_t i = 0U; i < (uint32_t)k_brr_settle_loops; i++) {
        __asm__ volatile("nop");
    }

    *sci9_scr()  = k_scr_txrx_enabled;
    *sci9_semr() = k_semr_default;
}

/* ==========================================================================
 * Polled write: spin until TDRE then drop byte into TDR.
 * ========================================================================== */
void sci9_debug_putc(char c)
{
    while ((*sci9_ssr() & (uint8_t)k_ssr_tdre) == 0U) {
        /* spin */
    }
    *sci9_tdr() = (uint8_t)c;
}

void sci9_debug_puts(const char *s)
{
    if (s == (const char *)0) {
        return;
    }
    while (*s != '\0') {
        if (*s == '\n') {
            sci9_debug_putc('\r');
        }
        sci9_debug_putc(*s++);
    }
}

void sci9_debug_puthex32(uint32_t v)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    sci9_debug_putc('0');
    sci9_debug_putc('x');
    for (int8_t shift = (int8_t)k_hex_shift_init32; shift >= 0;
         shift -= (int8_t)k_hex_shift_step) {
        sci9_debug_putc(hex_digits[(v >> (uint8_t)shift) & (uint8_t)k_hex_nibble_mask]);
    }
}

void sci9_debug_puthex16(uint16_t v)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    sci9_debug_putc('0');
    sci9_debug_putc('x');
    for (int8_t shift = (int8_t)k_hex_shift_init16; shift >= 0;
         shift -= (int8_t)k_hex_shift_step) {
        sci9_debug_putc(hex_digits[(v >> (uint8_t)shift) & (uint8_t)k_hex_nibble_mask]);
    }
}
