/**
 * @file sci9_polled.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* sci9_polled.h -- self-contained polled-mode SCI9 (115200 8N1) for STAR PCB.
 *
 * Bypasses Renesas FIT r_sci_rx entirely. Direct register I/O like the proven
 * star-rx72n-firmware/motors_rtos/sci9.c, so output is guaranteed visible from
 * the first call -- no interrupt vector question, no ByteQ buffering.
 *
 * Why polled in a sandbox that's supposed to test FIT: motors_rtos has shown
 * polled SCI works on this PCB. FIT r_sci_rx requires the TXI ISR to be
 * correctly wired into r_bsp's interrupt table; if that's the failure mode,
 * we can't even print "TXI dead" via FIT. Polled SCI is the bedrock under
 * any FIT-layer debugging.
 *
 * Pin-mux + MSTPCR for SCI9 are done inside sci9_init(); do NOT also mux
 * PB6/PB7 in hw_init.c.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/* Bring up SCI9: MSTPCRC.MSTPC26=0, PFS for PB6/PB7, SCR/SMR/BRR for 115200
 * 8N1 sourcing PCLKB. Call once after r_bsp boot finishes (clocks must
 * already be at 240/120/60 MHz). */
void sci9_init(void);

/* Send one byte. Spins on SSR.TDRE (~87 us at 115200, worst case). */
void sci9_putc(char c);

/* C string. Translates '\n' to "\r\n". */
void sci9_puts(const char *s);

/* Raw byte buffer (no \n translation). Used for binary frame TX. */
void sci9_write(const uint8_t *buf, size_t len);

/* Print "0xNNNNNNNN" / "0xNNNN" -- for debug checkpoints. */
void sci9_puthex32(uint32_t v);
void sci9_puthex16(uint16_t v);
