/**
 * @file star_board.h
 * @brief Board-variant selection for STAR RX72N firmware.
 *
 * Exactly one of STAR_BOARD_PROD or STAR_BOARD_TOM must be defined,
 * normally via the CMake -DSTAR_BOARD=PROD|TOM option, which forwards
 * `-DSTAR_BOARD_<name>=1`.
 *
 * ## Clock-source acronyms (Renesas RX72N HW manual terminology)
 *
 * | Acronym | Meaning                                                              |
 * |---------|----------------------------------------------------------------------|
 * | XTAL    | eXternal crysTAL -- a physical quartz resonator wired to the MCU's   |
 * |         | EXTAL/XTAL pins.  Stable and accurate (tens of ppm) but needs an     |
 * |         | extra component on the PCB.                                          |
 * | MOSC    | Main clock OSCillator -- the on-chip driver circuit that excites a   |
 * |         | connected XTAL (or accepts an external clock input) and produces    |
 * |         | the main clock signal.  "Oscillator" here is the MCU side of the    |
 * |         | pair, not the crystal itself.                                        |
 * | HOCO    | High-speed On-Chip Oscillator -- an internal RC oscillator (16/18/ |
 * |         | 20 MHz) integrated in the MCU.  Needs no external parts, but is     |
 * |         | less accurate (~+/-1 % = 10,000 ppm) -- fine for timers but marginal|
 * |         | for USB which wants <=500 ppm.                                       |
 * | LOCO    | Low-speed On-Chip Oscillator -- internal ~240 kHz.  Used for IWDT   |
 * |         | (independent watchdog) and reset bring-up before PLL is stable.     |
 * | PLL     | Phase-Locked Loop -- on-chip frequency multiplier.  Takes MOSC or   |
 * |         | HOCO as input and multiplies by an integer (10x, 12x, etc.) to      |
 * |         | produce the high-speed clock (192-240 MHz) that the CPU runs at.    |
 * | PPLL    | Peripheral PLL -- a secondary PLL used for Ethernet/USB peripheral  |
 * |         | clocks when different frequencies are needed from the system PLL.   |
 * | ICLK    | Internal CLocK -- post-PLL divider output that clocks the CPU.      |
 * | PCKA/B  | Peripheral ClocKs (A/B/C/D) -- post-PLL divider outputs for        |
 * |         | different peripheral groups (timers, SPI, SCI, etc.).               |
 * | UCLK    | USB CLocK -- 48 MHz clock derived from PLL (or PPLL) for USB.       |
 *
 * ## Board variants
 *
 * | Variant | Crystal      | PLL input source           | Use                  |
 * |---------|--------------|----------------------------|----------------------|
 * | PROD    | 24 MHz XTAL  | MOSC (clean, within spec)  | STAR production PCB  |
 * | TOM     | none         | HOCO (~16 MHz, ~1 % drift) | Tom's bench PCB      |
 *
 * Downstream code (BSP config, clock init) uses these macros to pick
 * the right oscillator path without having to edit generated BSP files.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#ifndef STAR_BOARD_H
#define STAR_BOARD_H

#if !defined(STAR_BOARD_PROD) && !defined(STAR_BOARD_TOM)
#error "STAR_BOARD variant is not defined. Configure with -DSTAR_BOARD=PROD or -DSTAR_BOARD=TOM."
#endif

#if defined(STAR_BOARD_PROD) && defined(STAR_BOARD_TOM)
#error "Both STAR_BOARD_PROD and STAR_BOARD_TOM are defined. Pick exactly one."
#endif

/* Human-readable accessor.  String literal for log / version output. */
#if defined(STAR_BOARD_PROD)
#define STAR_BOARD_NAME_STR "PROD"
#elif defined(STAR_BOARD_TOM)
#define STAR_BOARD_NAME_STR "TOM"
#endif

#endif /* STAR_BOARD_H */
