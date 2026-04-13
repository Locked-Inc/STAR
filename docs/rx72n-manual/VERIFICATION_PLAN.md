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

Manual is **Rev.1.11, Feb 2021** (our code comments said Rev.1.00 Feb 2019 -- minor note only).

| Ch | Topic | PDF Pages | Our Code File | Notes |
|----|-------|-----------|---------------|-------|
| 1 | Overview / Features | 75-154 | N/A | |
| 2 | CPU | 155-194 | N/A (ThreadX) | |
| 3 | Operating Modes | 195-202 | rx72n_lpc_regs.h | SYSCR0, SYSCR1, MDMONR |
| 4 | Address Space | 203-205 | N/A | |
| **5** | **I/O Register Addresses** | **206-282** | **ALL** | **Complete address table -- read first** |
| 6 | Resets | 283-294 | rx72n_system_regs.h | RSTSR0/1/2, SWRR |
| 7 | Option-Setting Memory | 295-315 | N/A | OFS, MDE, FAW -- not runtime regs |
| 8 | LVDA (Voltage Detection) | 316-334 | rx72n_lvda_regs.h | LVD1CR0/1, LVD2CR0/1, LVCMPCR |
| 9 | Clock Generation | 335-389 | rx72n_system_regs.h, rx72n_clock.h | SCKCR, PLL, oscillator regs |
| 10 | CAC | 390-400 | NOT USED | Available feature |
| 11 | Low Power Consumption | 401-449 | rx72n_lpc_regs.h | Deep standby, sleep regs |
| 12 | Battery Backup | 450-452 | NOT USED | Available feature |
| 13 | Register Write Protection | 453-454 | rx72n_system_regs.h | PRCR register |
| 14 | Exception Handling | 455-465 | N/A | CPU-level, ThreadX handles |
| 15 | Interrupt Controller (ICU) | 466-541 | rx72n_icu_regs.h | IR, IER, IPR, SWINTR |
| 16 | Buses | 542-656 | N/A | Bus matrix, no direct regs |
| 17 | MPU | 657-676 | rx72n_mpu_regs.h | RSPAGE, REPAGE, MPEN |
| 18 | DMAC (DMACAa) | 677-717 | rx72n_dmac_regs.h | DMSAR, DMDAR, DMCRA, DMCSL |
| 19 | EXDMAC | 718-785 | NOT USED | Available feature |
| 20 | DTC | 786-834 | rx72n_dtc_regs.h | DTCVBR, DTCST, DTCADMOD |
| 21 | ELC | 835-860 | rx72n_elc_regs.h | ELSR, ELOPA, ELOPB, ELOPC, ELOPD |
| 22 | I/O Ports (GPIO) | 861-881 | rx72n_port_regs.h | PDR, PODR, PIDR, PMR, PCR, DSCR |
| 23 | MPC (Pin Multiplex) | 882-959 | rx72n_mpc_regs.h | PWPR, PFS registers |
| 24 | MTU3a | 960-1198 | rx72n_mtu_regs.h | TCR, TMDR, TIORH/L, TIER, TSR, TCNT |
| 25 | POE3a | 1199-1235 | rx72n_poe3_regs.h | ICSR, OCSR, ALR, SPOER, POECR |
| 26 | GPTW | 1236-1431 | rx72n_gptw_regs.h | GTCR, GTST, GTBER, GTCNT, GTPR |
| 27 | POEG | 1432-1441 | rx72n_poeg_regs.h | POEGG0-3 |
| 28 | TPU (TPUa) | 1442-1519 | rx72n_tpu_regs.h | TCR, TMDR, TIOR, TIER, TSR, TCNT |
| 29 | PPG | 1520-1552 | NOT USED | Available feature |
| 30 | TMR (8-bit) | 1553-1580 | NOT USED | Available feature |
| 31 | CMT | 1581-1588 | rx72n_cmt_regs.h | CMSTR0/1, CMCR, CMCNT, CMCOR |
| 32 | CMTW | 1589-1615 | NOT USED | Available feature |
| 33 | RTC | 1616-1665 | NOT USED | Available feature |
| 34 | WDT | 1666-1679 | rx72n_wdt_regs.h | WDTRR, WDTCR, WDTSR, WDTRCR |
| 35 | IWDT | 1680-1698 | rx72n_iwdt_regs.h | IWDTRR, IWDTCR, IWDTSR, IWDTRCR |
| 36 | Ethernet (ETHERC) | 1699-1729 | NOT USED | Available feature |
| 37 | PTP (EPTPC) | 1730-1871 | NOT USED | Available feature |
| 38 | EDMA (Ethernet DMA) | 1872-1913 | NOT USED | Available feature |
| 39 | PMGI | 1914-1927 | NOT USED | Available feature |
| 40 | USB 2.0 FS | 1928-2036 | rx72n_usb_regs.h | SYSCFG, SYSSTS0, DVSTCTR0, CFIFO |
| 41 | SCI | 2037-2216 | rx72n_sci_regs.h | SMR, BRR, SCR, TDR, SSR, RDR, SCMR |
| 42 | RIIC (I2C) | 2217-2293 | rx72n_riic_regs.h | ICCR1/2, ICMR1/2/3, ICIER, ICSR1/2 |
| 43 | CAN | 2294-2348 | NOT USED | Available feature |
| 44 | RSPI | 2349-2433 | rx72n_rspi_regs.h | SPCR, SPSR, SPDR, SPSCR, SPBR |
| 45 | QSPI | 2434-2479 | NOT USED | Available feature |
| 46 | CRC | 2480-2487 | rx72n_crc_regs.h | CRCCR, CRCDIR, CRCDOR |
| 47 | SSIE | 2488-2528 | NOT USED | Available feature |
| 48 | SDHI | 2529-2582 | NOT USED | Available feature |
| 49 | MMCIF | 2583-2626 | NOT USED | Available feature |
| 50 | PDC | 2627-2653 | NOT USED | Available feature |
| 51 | GLCDC | 2654-2774 | NOT USED | Available feature |
| 53 | Boundary Scan | 2775-2797 | NOT USED | |
| 54 | TFU | 2798 | NOT USED | Available feature |
| 55 | TSIP | 2799-2962 | NOT USED | Available feature |
| 58 | Temperature Sensor | 2963-2969 | NOT USED | Available feature |
| 59 | DOC | 2970-2976 | NOT USED | Available feature |
| 60 | RAM | 2977-2990 | rx72n_ram_regs.h | RAMMODE, EXRAMMODE, ECCRAM regs |
| 61 | Standby RAM | 2991 | NOT USED | Available feature |
| 62 | Flash Memory | 2992-3233 | rx72n_flash_regs.h | FSTATR, FENTRYR, FRESETR, FPCKAR |

| 52 | DRW2D (2D Drawing Engine) | 2745-2774 | NOT USED | Available feature |
| 56 | S12AD (12-bit ADC) | 2809-2949 | rx72n_adc_regs.h | ADCSR, ADANSA, ADCER, ADDR |
| 57 | DAC (R12DAa) | 2950-2962 | NOT USED | Available feature |

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

| Section | Code File | Pages (real) | Status | Issues Found |
|---------|-----------|--------------|--------|--------------|
| Ch 5: I/O Register Addresses | ALL | 206-282 | [x] COMPLETE | Addr table used to verify DTC/DMAC/MPU/CMT/CRC/WDT/IWDT |
| Ch 3+11: Operating Modes / LPC | rx72n_lpc_regs.h | 195-202, 401-449 | [x] COMPLETE | FIXED: system_regs.h doc comment said "Ch 7" instead of "Ch 11" |
| Ch 9: Clock Generation | rx72n_system_regs.h, rx72n_clock.h | 335-389 | [x] COMPLETE | FIXED: k_clock_div_128 prohibited; PSTOP0/1 wrong pin descriptions; k_sckcr3_cksel_ppll invalid value; k_pllcr_plidiv_4 should be plidiv_3; CKODIV /32/64/128 prohibited; k_ppll_config_48mhz lower byte wrong (0x03->0x00); k_ppll_stable_flag wrong bit (0x08->0x20 PPLOVF is bit 5 not 3); oscovfsr comment wrong |
| Ch 8: Voltage Monitor (LVDA) | rx72n_lvda_regs.h | 316-334 | [x] COMPLETE | FIXED: rx_lvd_interrupts_t used uint16_t, corrected to uint8_t (vectors 88/89 fit in uint8_t) |
| Ch 22: GPIO Ports | rx72n_port_regs.h | 861-881 | [x] COMPLETE | All base addrs + offsets + ODR word-addr OK; FIXED: 8 doc refs said "Ch 21", corrected to "Ch 22" |
| Ch 23: MPC Pin Multiplex | rx72n_mpc_regs.h | 882-959 | [x] COMPLETE | All PFS addrs + PWPR + bit fields OK; FIXED: 5 doc refs said "Ch 20"/"Ch23", corrected to "Ch 23" |
| Ch 15: ICU Interrupts | rx72n_icu_regs.h | 466-541 | [x] COMPLETE | All addrs + reserved gaps + DMRSR 4-byte alignment + CMT vectors 28/29 OK |
| Ch 20: DTC | rx72n_dtc_regs.h | 786-834 | [x] COMPLETE | All 9 addrs OK (Ch 5 + header) |
| Ch 18: DMAC | rx72n_dmac_regs.h | 677-717 | [x] COMPLETE | All addrs + offsets OK (Ch 5 + header) |
| Ch 62: Flash Memory | rx72n_flash_regs.h | 2992-3233 | [ ] NOT STARTED | |
| Ch 60: RAM Control | rx72n_ram_regs.h | 2977-2990 | [ ] NOT STARTED | |
| Ch 17: MPU | rx72n_mpu_regs.h | 657-676 | [x] COMPLETE | All region + ctrl addrs OK (Ch 5 + header) |
| Ch 21: ELC | rx72n_elc_regs.h | 835-860 | [x] COMPLETE | FIXED: ELOPA MTU3 shift/mask, ELOPC CMT1 shift/mask, elop_timer_op_t values swapped, pgc_bits_t entire layout wrong, pel_bits_t PSB/PSP/PSM wrong, elc_elsr_reg() formula wrong for n>=33 |
| Ch 31: CMT Timer | rx72n_cmt_regs.h | 1581-1588 | [x] COMPLETE | All addrs + offsets OK (Ch 5 + header) |
| Ch 28: TPU | rx72n_tpu_regs.h | 1442-1519 | [x] COMPLETE | FIXED: k_tpu_tier_all_disabled was 0x00, TIER bit 6 reserved/must-write-1 (reset=0x40), corrected to 0x40 in rx_tpu.c |
| Ch 24: MTU | rx72n_mtu_regs.h | 960-1198 | [x] COMPLETE | FIXED: rx_mtu34_channel_regs_t had completely wrong sparse layout; renamed + rewritten; rx_mtu.c/rx_mtu_encoder.c MTU3/4 cases now return nullptr |
| Ch 26: GPTW (PWM) | rx72n_gptw_regs.h | 1236-1431 | [x] COMPLETE | FIXED: OAE=b8, OBE=b24, init_high=0x16, GTBER enum completely wrong (3 values), rx_gptw.c was disabling instead of enabling buffer |
| Ch 25+27: POE3 / POEG | rx72n_poe3_regs.h, rx72n_poeg_regs.h | 1199-1235, 1432-1441 | [x] COMPLETE | FIXED: ICSR6 OSTSTE b9 not b8; POECR2 16-bit with MT34/67 layout; POECR4/5 ICxADD bits wrong; POEG all OK |
| Ch 56: S12AD (ADC) | rx72n_adc_regs.h | 2809-2949 | [ ] NOT STARTED | |
| Ch 41: SCI | rx72n_sci_regs.h | 2037-2216 | [ ] NOT STARTED | |
| Ch 42: RIIC (I2C) | rx72n_riic_regs.h | 2217-2293 | [x] COMPLETE | All addrs + offsets + bit enums OK |
| Ch 44: RSPI (SPI) | rx72n_rspi_regs.h | 2349-2433 | [x] COMPLETE | FIXED: SPPCR MOIFE/MOIFV bits; SPDCR SPLW/SPRDTD bits |
| Ch 46: CRC | rx72n_crc_regs.h | 2480-2487 | [x] COMPLETE | All addrs + offsets OK (Ch 5 + header) |
| Ch 40: USB | rx72n_usb_regs.h | 1928-2036 | [ ] NOT STARTED | |
| Ch 34: WDT | rx72n_wdt_regs.h | 1666-1679 | [x] COMPLETE | All addrs + offsets OK (Ch 5 + header) |
| Ch 35: IWDT | rx72n_iwdt_regs.h | 1680-1698 | [x] COMPLETE | All addrs + offsets OK (Ch 5 + header) |

**Pass 1 last page verified:** `449 (Ch 3+11 Operating Modes/LPC complete 2026-04-13), 334 (Ch 8 LVDA complete 2026-04-13), 389 (Ch 9 Clock Generation complete 2026-04-13), 860 (Ch 21 ELC complete 2026-04-13), 1441 (Ch 25+27 POE3/POEG complete 2026-04-13), 1519 (Ch 28 TPU complete 2026-04-13)`

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
