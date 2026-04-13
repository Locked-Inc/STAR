# RX72N Manual Verification Plan

**Status:** IN PROGRESS
**Branch:** `feat/rx72n-manual-verification`
**Manual:** `docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf` (3,233 pages)
**Individual pages:** `docs/rx72n-manual/pages/page-NNNN.pdf`
**Last updated:** 2026-04-13

---

## How to Continue This Work

Tell Claude:
> "Read the verification plan at `docs/rx72n-manual/VERIFICATION_PLAN.md` and continue
> the RX72N manual verification from where it left off. The branch is
> `feat/rx72n-manual-verification`. Follow the rules in the plan exactly."

---

## Two-Pass Verification Strategy

This project requires **two complete passes** through all the material. Both passes are
mandatory and must be thorough -- do not rush either one.

### Pass 1: PDF -> CODE (Manual-Driven)
Read every relevant page of the manual. For each register, offset, bit mask, or constant
described in the manual: find the corresponding value in our code and verify it is correct.
Fix anything wrong. Document anything the chip supports that we don't implement.

### Pass 2: CODE -> PDF (Code-Driven)
Read every source file in the firmware. For each register address, offset, bit mask,
constant, or configuration value written in our code: look it up in the manual and
confirm it is correct. This catches anything Pass 1 missed because the code references
something not obvious from the manual's table-of-contents structure.

Both passes log to VERIFICATION_LOG.md and follow the same fix/commit workflow.

---

## Rules (Mandatory for All Sessions)

1. **Verify-only scope:** Only fix things that are WRONG in existing code.
   - Correct: register base address in our code is 0x000D0100 but manual says 0x000D0200 -> fix it
   - Forbidden: manual shows RSCAN peripheral we don't use -> do NOT add RSCAN code
2. **Document available-but-unused features** in `docs/rx72n-manual/AVAILABLE_FEATURES.md`
   - Entry format: `| Section | Feature | Manual Page(s) | Notes |`
   - This is informational only. No code added.
3. **Fix wrong values immediately** - one commit per logical unit of fixes (e.g., "fix RSPI register offsets")
4. **Commit style:** Natural, human commit messages. No "Claude", "AI", "co-author" attribution.
5. **Do NOT push** to remote at any point.
6. **Log all work** in `docs/rx72n-manual/VERIFICATION_LOG.md` as you go.
7. **Update this file** at the end of every session: set `Last page verified` and `Status`.
8. **Read fully:** Every page of the manual and every line of each source file must be
   read -- do not skim or skip. Thoroughness is the point.

---

## Project Context

**What this is:** STAR is a distributed robotics platform. The RX72N (R5F572NNHxFB,
144-pin LFQFP, 4MB Flash, 512KB SRAM) is the motor controller. It runs ThreadX RTOS,
communicates with a Raspberry Pi 5 over SPI using Protocol Buffers (nanopb), and controls
4x brushed DC motors via DRV8263H H-bridges with Hall-effect encoder feedback.

**What we are checking:** The firmware defines all RX72N hardware registers in header
files under `star-rx72n-firmware/libs/rx_hal/inc/`. Every base address, register offset,
bit-field position, and bit mask in those files must match the Renesas RX72N Hardware
Manual exactly. Bugs here cause silent hardware malfunctions.

**Firmware tree (source files only):**
```
star-rx72n-firmware/libs/
  rx_hal/
    inc/
      hardware.h              -- HAL API (application-facing)
      rx72n_regs.h            -- master aggregator header
      rx72n_system_regs.h     -- SYSTEM block: clocks, module stop, protect
      rx72n_port_regs.h       -- GPIO ports
      rx72n_adc_regs.h        -- S12AD0/S12AD1 (ADC)
      rx72n_sci_regs.h        -- SCI0-12 (UART/SPI/I2C mode)
      rx72n_riic_regs.h       -- RIIC0-2 (I2C dedicated)
      rx72n_rspi_regs.h       -- RSPI0-2 (SPI dedicated)
      rx72n_cmt_regs.h        -- CMT0-3 (compare match timer)
      rx72n_icu_regs.h        -- ICU (interrupt controller)
      rx72n_iwdt_regs.h       -- IWDT (independent watchdog)
      rx72n_wdt_regs.h        -- WDT (watchdog)
      rx72n_crc_regs.h        -- CRC calculator
      rx72n_usb_regs.h        -- USB0 (full-speed)
      rx72n_mtu_regs.h        -- MTU0-8 (multifunction timer, encoders)
      rx72n_gptw_regs.h       -- GPTW0-3 (general PWM timer, motor PWM)
      rx72n_poeg_regs.h       -- POEG0-3 (PWM output emergency stop)
      rx72n_tpu_regs.h        -- TPU0-5 (timer pulse unit)
      rx72n_lvda_regs.h       -- LVDA (voltage detect / brownout)
      rx72n_mpc_regs.h        -- MPC (pin function select)
      rx72n_flash_regs.h      -- FLASH (programming, ROM cache)
      rx72n_ram_regs.h        -- RAM/EXRAM/ECCRAM control
      rx72n_dmac_regs.h       -- DMAC0-7 (DMA controller)
      rx72n_dtc_regs.h        -- DTC (data transfer controller)
      rx72n_mpu_regs.h        -- MPU (memory protection)
      rx72n_lpc_regs.h        -- LPC (low power / deep standby)
      rx72n_elc_regs.h        -- ELC (event link controller)
      rx72n_clock.h           -- clock frequency constants
    src/
      adc.c                   -- ADC HAL implementation
      gpio.c                  -- GPIO HAL implementation
      riic.c                  -- I2C HAL implementation
      rspi.c                  -- SPI HAL implementation
      rx_cmt.c                -- CMT timer HAL implementation
      rx_gptw.c               -- GPTW PWM HAL implementation
      rx_host_irq.c           -- Host-side IRQ handling
      rx_irq_filter.c         -- IRQ filter/debounce
      rx_mtu.h / rx_mtu.c     -- MTU encoder HAL
      rx_tpu.h / rx_tpu.c     -- TPU HAL
      rx_poeg.h / rx_poeg.c   -- POEG emergency stop HAL
  rx_core/                    -- system init, clocks, watchdog, logging
  rx_crc/                     -- CRC-32 (hardware and software)
  rx_dmaca/                   -- DMA channel abstraction
  rx_bus/                     -- bus manager (I2C, SPI, ADC, GPIO, 1-Wire)
  rx_encoder/                 -- encoder interface (TPU + MTU backends)
  rx_motor/                   -- motor control
  rx_drv8263/                 -- DRV8263H H-bridge driver
  rx_pid/                     -- PID controller
  rx_spi_comm/                -- SPI protocol (RPi5 link)
  rx_usb/                     -- USB device stack
  ... (sensors, framing, session, comm manager)
```

---

## Manual Structure and Verification Sections

The manual (r01uh0824ej0111_rx72n-2931480.pdf) is the Renesas RX72N Group User's Manual:
Hardware. The chapter/section structure is standard Renesas layout:

| Chapter | Topic | Pages (approx) | Our Code File |
|---------|-------|----------------|---------------|
| 1 | Overview / Features | 1-30 | N/A |
| 2 | CPU | 31-120 | N/A (ThreadX handles) |
| 3 | Operating Modes | 121-160 | rx72n_lpc_regs.h |
| 4 | Clock Generation | 161-220 | rx72n_system_regs.h, rx72n_clock.h |
| 5 | Power-On Reset / Voltage Monitor | 221-260 | rx72n_lvda_regs.h |
| 6 | I/O Ports | 261-400 | rx72n_port_regs.h |
| 7 | MPC (Pin Multiplex) | 401-500 | rx72n_mpc_regs.h |
| 8 | Interrupt Controller (ICU) | 501-650 | rx72n_icu_regs.h |
| 9 | DTC | 651-720 | rx72n_dtc_regs.h |
| 10 | DMAC | 721-820 | rx72n_dmac_regs.h |
| 11 | Flash Memory | 821-950 | rx72n_flash_regs.h |
| 12 | RAM/EXRAM/ECCRAM | 951-1000 | rx72n_ram_regs.h |
| 13 | MPU | 1001-1050 | rx72n_mpu_regs.h |
| 14 | ELC | 1051-1120 | rx72n_elc_regs.h |
| 15 | CMT | 1121-1160 | rx72n_cmt_regs.h |
| 16 | TPU | 1161-1280 | rx72n_tpu_regs.h |
| 17 | MTU | 1281-1500 | rx72n_mtu_regs.h |
| 18 | GPTW | 1501-1700 | rx72n_gptw_regs.h |
| 19 | POE3 / POEG | 1701-1780 | rx72n_poeg_regs.h |
| 20 | S12AD (ADC) | 1781-1950 | rx72n_adc_regs.h |
| 21 | SCI (UART/SCI) | 1951-2100 | rx72n_sci_regs.h |
| 22 | RIIC (I2C) | 2101-2250 | rx72n_riic_regs.h |
| 23 | RSPI (SPI) | 2251-2400 | rx72n_rspi_regs.h |
| 24 | CRC | 2401-2450 | rx72n_crc_regs.h |
| 25 | USB | 2451-2650 | rx72n_usb_regs.h |
| 26 | Watchdog (WDT/IWDT) | 2651-2750 | rx72n_wdt_regs.h, rx72n_iwdt_regs.h |
| 27 | LVDA | 2751-2800 | rx72n_lvda_regs.h |
| 28+ | Electrical / Package | 2800+ | N/A |

**IMPORTANT:** Page numbers above are approximate estimates from the chapter list.
The actual page numbers must be confirmed by reading the manual TOC pages (approx page-0003
to page-0010). Update this table with real page numbers after reading the TOC.

---

## Verification Progress

### How to Read Pages

Each page is at: `docs/rx72n-manual/pages/page-NNNN.pdf`
Read them with the Read tool (they are PDFs, tool can read them directly).

Example:
```
Read: docs/rx72n-manual/pages/page-0001.pdf
```

### PASS 1: PDF -> CODE Checklist

Statuses: `[ ] NOT STARTED` | `[~] IN PROGRESS (page NNN)` | `[x] COMPLETE`

| Section | Code File | Pages (approx) | Status | Issues Found |
|---------|-----------|----------------|--------|--------------|
| TOC / Overview | -- | ~1-30 | [ ] NOT STARTED | |
| Operating Modes / LPC | rx72n_lpc_regs.h | ~121-160 | [ ] NOT STARTED | |
| Clock Generation (SYSTEM) | rx72n_system_regs.h, rx72n_clock.h | ~161-220 | [ ] NOT STARTED | |
| Voltage Monitor (LVDA) | rx72n_lvda_regs.h | ~221-260, ~2751-2800 | [ ] NOT STARTED | |
| GPIO Ports | rx72n_port_regs.h | ~261-400 | [ ] NOT STARTED | |
| MPC Pin Multiplex | rx72n_mpc_regs.h | ~401-500 | [ ] NOT STARTED | |
| ICU Interrupts | rx72n_icu_regs.h | ~501-650 | [ ] NOT STARTED | |
| DTC | rx72n_dtc_regs.h | ~651-720 | [ ] NOT STARTED | |
| DMAC | rx72n_dmac_regs.h | ~721-820 | [ ] NOT STARTED | |
| Flash Memory | rx72n_flash_regs.h | ~821-950 | [ ] NOT STARTED | |
| RAM Control | rx72n_ram_regs.h | ~951-1000 | [ ] NOT STARTED | |
| MPU | rx72n_mpu_regs.h | ~1001-1050 | [ ] NOT STARTED | |
| ELC | rx72n_elc_regs.h | ~1051-1120 | [ ] NOT STARTED | |
| CMT Timer | rx72n_cmt_regs.h | ~1121-1160 | [ ] NOT STARTED | |
| TPU | rx72n_tpu_regs.h | ~1161-1280 | [ ] NOT STARTED | |
| MTU | rx72n_mtu_regs.h | ~1281-1500 | [ ] NOT STARTED | |
| GPTW (PWM) | rx72n_gptw_regs.h | ~1501-1700 | [ ] NOT STARTED | |
| POE3 / POEG | rx72n_poeg_regs.h | ~1701-1780 | [ ] NOT STARTED | |
| S12AD (ADC) | rx72n_adc_regs.h | ~1781-1950 | [ ] NOT STARTED | |
| SCI (UART) | rx72n_sci_regs.h | ~1951-2100 | [ ] NOT STARTED | |
| RIIC (I2C) | rx72n_riic_regs.h | ~2101-2250 | [ ] NOT STARTED | |
| RSPI (SPI) | rx72n_rspi_regs.h | ~2251-2400 | [ ] NOT STARTED | |
| CRC Calculator | rx72n_crc_regs.h | ~2401-2450 | [ ] NOT STARTED | |
| USB | rx72n_usb_regs.h | ~2451-2650 | [ ] NOT STARTED | |
| WDT / IWDT | rx72n_wdt_regs.h, rx72n_iwdt_regs.h | ~2651-2750 | [ ] NOT STARTED | |

**Pass 1 last page verified:** `none -- not started`

---

### PASS 2: CODE -> PDF Checklist

Begin Pass 2 only after Pass 1 is fully complete.

Each row is one firmware source file. Read the entire file, find every hex constant,
enum address, or hardware value, cross-reference it against the manual.

| Source File | Status | Issues Found |
|-------------|--------|--------------|
| rx_hal/inc/rx72n_system_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_port_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_adc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_sci_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_riic_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_rspi_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_cmt_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_icu_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_iwdt_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_wdt_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_crc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_usb_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_mtu_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_gptw_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_poeg_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_tpu_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_lvda_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_mpc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_flash_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_ram_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_dmac_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_dtc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_mpu_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_lpc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_elc_regs.h | [ ] NOT STARTED | |
| rx_hal/inc/rx72n_clock.h | [ ] NOT STARTED | |
| rx_hal/src/adc.c | [ ] NOT STARTED | |
| rx_hal/src/gpio.c | [ ] NOT STARTED | |
| rx_hal/src/riic.c | [ ] NOT STARTED | |
| rx_hal/src/rspi.c | [ ] NOT STARTED | |
| rx_hal/src/rx_cmt.c | [ ] NOT STARTED | |
| rx_hal/src/rx_gptw.c | [ ] NOT STARTED | |
| rx_hal/src/rx_host_irq.c | [ ] NOT STARTED | |
| rx_hal/src/rx_irq_filter.c | [ ] NOT STARTED | |
| rx_crc/src/rx_crc_hw.c | [ ] NOT STARTED | |
| rx_dmaca/src/rx_dmaca.c | [ ] NOT STARTED | |
| rx_core/src/rx_infrastructure.c | [ ] NOT STARTED | |
| rx_core/src/rx_iwdt.c | [ ] NOT STARTED | |
| rx_core/src/rx_wdt.c | [ ] NOT STARTED | |
| rx_encoder/src/rx_encoder_tpu.c | [ ] NOT STARTED | |
| rx_encoder/src/rx_mtu_encoder.c | [ ] NOT STARTED | |
| rx_bus/src/rx_bus_adc.c | [ ] NOT STARTED | |
| rx_bus/src/rx_bus_i2c.c | [ ] NOT STARTED | |
| rx_bus/src/rx_bus_gpio.c | [ ] NOT STARTED | |

**Pass 2 last file verified:** `none -- not started`

---

## Session Workflow

### Pass 1 session steps:
1. Read this file to know where to resume (find last `[~] IN PROGRESS` or first `[ ] NOT STARTED`)
2. Read the manual pages for that section (use Read tool on `pages/page-NNNN.pdf`)
3. Read the corresponding code file fully
4. For each register/value in the manual: find it in our code and verify it
5. Fix anything wrong immediately
6. If the manual shows a feature we don't use: add it to AVAILABLE_FEATURES.md
7. Log all findings in VERIFICATION_LOG.md (both OK and FIXED entries)
8. Commit fixes (natural message, no AI attribution)
9. Update Pass 1 checklist table (mark `[x] COMPLETE`)
10. Update "Pass 1 last page verified"

### Pass 2 session steps:
1. Read this file to know where to resume (find last `[~] IN PROGRESS` or first `[ ] NOT STARTED` in Pass 2 table)
2. Read the source file fully
3. For each hex constant, address, or hardware value in that file: find it in the manual
4. Fix anything wrong
5. Log all findings in VERIFICATION_LOG.md
6. Commit fixes
7. Update Pass 2 checklist (mark `[x] COMPLETE`)
8. Update "Pass 2 last file verified"

---

## Commit Message Convention

Use the format: `fix(<peripheral>): <what was wrong>`

Examples:
- `fix(rspi): correct RSPI0 base address from 0x000D0000 to 0x000D0100`
- `fix(dmac): correct DMACT register offset from 0x40 to 0x00`
- `fix(cmt): fix CMCR bit positions for CKS field`

No "Generated by", "Co-Authored-By", "Claude", or any AI-related text in commits.

---

## Key Files Quick Reference

| File | Path |
|------|------|
| Master plan (this file) | `docs/rx72n-manual/VERIFICATION_PLAN.md` |
| Available features log | `docs/rx72n-manual/AVAILABLE_FEATURES.md` |
| Verification progress log | `docs/rx72n-manual/VERIFICATION_LOG.md` |
| Manual PDF (full) | `docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf` |
| Manual pages (split) | `docs/rx72n-manual/pages/page-NNNN.pdf` |
| HAL register headers | `star-rx72n-firmware/libs/rx_hal/inc/rx72n_*.h` |
| HAL implementations | `star-rx72n-firmware/libs/rx_hal/src/` |
| CLAUDE.md (coding rules) | `CLAUDE.md` |
