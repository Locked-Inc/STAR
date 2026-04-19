# RX72N Verification Log

Chronological record of every verification session: what was checked, what was found,
what was fixed. Update this file as you work.

**Format per entry:**
```
## YYYY-MM-DD -- <Section Name>

**Pages read:** NNNN-MMMM
**Code file(s):** path/to/file.h

### Findings
- [OK] <register name> = 0xXXXX -- matches manual p.NNN
- [FIXED] <register name>: was 0xXXXX, manual p.NNN says 0xYYYY -- corrected
- [AVAILABLE] <feature name> -- added to AVAILABLE_FEATURES.md

### Commit(s)
- <commit hash> -- <commit message>
```

---

## 2026-04-13 -- Setup

**Pages read:** none yet (PDF split still in progress at session start)
**Code file(s):** N/A

### Findings
- Branch `feat/rx72n-manual-verification` created
- VERIFICATION_PLAN.md, AVAILABLE_FEATURES.md, VERIFICATION_LOG.md created
- PDF split in progress: `pdfseparate` splitting 3,233 pages into
  `docs/rx72n-manual/pages/page-NNNN.pdf`
- Full firmware inventory completed:
  - 28 libraries, 76 source files, 115 header files
  - All peripheral register headers identified (see VERIFICATION_PLAN.md)
  - Approximate chapter-to-page mapping created (needs confirmation from TOC)
- Two-pass strategy finalized:
  - Pass 1: PDF -> CODE (manual-driven, read every manual page)
  - Pass 2: CODE -> PDF (code-driven, read every source file)

### Commits
- 6bd48a498 -- docs: add RX72N manual and verification plan

---

## 2026-04-13 -- Ch 5 I/O Address Table + DTC/DMAC/MPU/CMT/CRC/WDT/IWDT

**Pages read:** 206-282 (Ch 5 complete I/O register address table)
**Code file(s):** rx72n_dtc_regs.h, rx72n_dmac_regs.h, rx72n_mpu_regs.h, rx72n_cmt_regs.h, rx72n_crc_regs.h, rx72n_wdt_regs.h, rx72n_iwdt_regs.h

### Findings
- [OK] DTC: k_dtc_base_addr=0x00082400, all 9 register addresses match manual p.206
- [OK] DMAC: k_dmac0..7_base_addr (0x00082000-0x000821C0, spacing 0x40), all offsets match
- [OK] DMAC: DMAST=0x00082200, DMIST=0x00082204 -- correct
- [OK] MPU: k_mpu_region_base_addr=0x00086400, k_mpu_control_base_addr=0x00086500 -- correct
- [OK] MPU: All RSPAGEn/REPAGEn spacing, MPEN/MPBAC/MPECLR/MPESTS/MPDEA addrs -- correct
- [OK] CMT: CMSTR0=0x00088000, channel bases 0x88002/0x88008/0x88012/0x88018 -- correct
- [OK] CMT: CMCR=+0x00, CMCNT=+0x02, CMCOR=+0x04 -- correct
- [OK] CRC: k_crc_base_addr=0x00088280, CRCCR=+0, CRCDIR=+4, CRCDOR=+8 -- correct
- [OK] WDT: k_wdt_base_addr=0x00088020, WDTRR/CR/SR/RCR offsets 0-6 -- correct
- [OK] IWDT: k_iwdt_base_addr=0x00088030, IWDTRR/CR/SR/RCR/CSTPR offsets 0-8 -- correct

### Commits
- 6bd48a498 -- docs: add RX72N manual and verification plan (previous session)

---

## 2026-04-13 -- Ch 42 RIIC (I2C) Registers

**Pages read:** 2217-2293 (Ch 42 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_riic_regs.h

### Findings
- [OK] RIIC0=0x00088300, RIIC1=0x00088320, RIIC2=0x00088340 (spacing 0x20) -- correct
- [OK] All 20 register offsets: ICCR1=+0x00, ICCR2=+0x01, ICMR1=+0x02, ICMR2=+0x03, ICMR3=+0x04, ICFER=+0x05, ICSER=+0x06, ICIER=+0x07, ICSR1=+0x08, ICSR2=+0x09 -- correct
- [OK] Slave address regs: SARL0=+0x0A, SARU0=+0x0B, SARL1=+0x0C, SARU1=+0x0D, SARL2=+0x0E, SARU2=+0x0F -- correct
- [OK] Bit rate / data: ICBRL=+0x10, ICBRH=+0x11, ICDRT=+0x12, ICDRR=+0x13 -- correct
- [OK] Bit enums: ICCR1 ICE=bit7/IICRST=bit6; ICCR2 BBSY=bit7/MST=bit6/TRS=bit5/SP=bit3/RS=bit2/ST=bit1 -- correct
- [OK] Bit enums: ICSR1 ACKBR=bit0; ICSR2 TDRE=bit7/NACKF=bit4/STOP=bit3/START=bit2/RDRF=bit1 -- correct
- [OK] sizeof(rx_riic_regs_t) == 20 (struct static_assert) -- correct

### Commits
- (none, all OK, no fixes needed)

---

## 2026-04-13 -- Ch 44 RSPI (SPI) Registers

**Pages read:** 2349-2433 (Ch 44 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_rspi_regs.h, star-rx72n-firmware/libs/rx_hal/src/rspi.c

### Findings
- [OK] RSPI0=0x000D0100, RSPI1=0x000D0140, RSPI2=0x000D0300 -- correct
- [OK] All register offsets match: SPCR=+0x00, SSLP=+0x01, SPPCR=+0x02, SPSR=+0x03, SPDR=+0x04, SPSCR=+0x08, SPSSR=+0x09, SPBR=+0x0A, SPDCR=+0x0B, SPCKD=+0x0C, SSLND=+0x0D, SPND=+0x0E, SPCR2=+0x0F, SPCMD0-7=+0x10-0x1E, SPDCR2=+0x20 -- correct
- [OK] SPCR bit enums: SPRIE=bit7/SPE=bit6/SPTIE=bit5/SPEIE=bit4/MSTR=bit3/MODFEN=bit2/TXMD=bit1/SPMS=bit0 -- correct
- [OK] SPSR bit enums: SPRF=bit7/SPTEF=bit5/UDRF=bit4/PERF=bit3/MODF=bit2/IDLNF=bit1/OVRF=bit0 -- correct
- [FIXED] SPPCR: MOIFE was (1<<6)=bit6, MOIFV was (1<<5)=bit5 -- manual p.2356 says MOIFE=bit5, MOIFV=bit4 -- corrected both enum values and all doc comments
- [FIXED] SPDCR: SPRDTD was (1<<5)=bit5, SPLW was (1<<4)=bit4 -- manual p.2367 says SPLW=bit5, SPRDTD=bit4 -- corrected both enum values and all doc comments
- [FIXED] rspi.c: k_rspi_spdcr_splw_pos was 4 -- corrected to 5 (bit position used in actual driver code)
- [FIXED] SPCMD0 struct comment had completely wrong bit assignments: CPOL/CPHA were swapped, SPB was at bits 5:4 instead of 11:8, SSLA at bit 2 instead of 6:4, etc. -- corrected all bit positions and names

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 22 I/O Ports (GPIO) Registers

**Pages read:** 861-881 (Ch 22 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_port_regs.h

### Findings
- [OK] PDR base=0x0008C000, PODR base=0x0008C020, PIDR base=0x0008C040 -- match manual p.863-864
- [OK] PMR base=0x0008C060, ODR0 base=0x0008C080 -- match manual p.865-866
- [OK] PCR base=0x0008C0C0, DSCR base=0x0008C0E0, DSCR2 base=0x0008C128 -- match manual p.870-873
- [OK] Port offsets 0x00-0x0E (ports 0-9, A-F) and 0x12 (port J) -- correct (144-pin: no G, H, K, L, M, N, Q)
- [OK] ODR word-addressing: portN_odr() = k_port_odr0_base + (port_offset * 2) -- all 14 accessors correct
- [OK] Struct offsets: pdr=+0x00, podr=+0x20, pidr=+0x40, pmr=+0x60, pcr=+0xC0, dscr=+0xE0, dscr2=+0x128 -- correct
- [OK] sizeof(rx_port_regs_t) == 0x129 = 297 bytes (static_assert present) -- correct
- [FIXED] 8 doc comments referenced "Chapter 21 (I/O Ports)" -- manual section numbers are "22.3.x", corrected to "Chapter 22"
  - Also corrected "Section 21.2" -> "Section 22.3" and "Section 21.3" -> "Section 22.4" throughout

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 23 MPC (Multi-Function Pin Controller) Registers

**Pages read:** 882-959 (Ch 23 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_mpc_regs.h

### Findings
- [OK] k_mpc_base_addr=0x0008C100 -- PWPR at +0x1F = 0x0008C11F matches manual p.882
- [OK] P00PFS at +0x40 = 0x0008C140, P10PFS=0x0008C148, P20PFS=0x0008C150 -- match manual p.883-884
- [OK] P30PFS=0x0008C158 (5 pins + 3 reserved), P40PFS=0x0008C160 -- correct
- [OK] P50PFS=0x0008C168 (P53 reserved), P60PFS=0x0008C170 (P65 reserved) -- correct
- [OK] P70PFS gap (P70 reserved), P71PFS=0x0008C179 through P77PFS=0x0008C17F -- correct
- [OK] PA0PFS=0x0008C190, PB0PFS=0x0008C198, PC0PFS=0x0008C1A0, PD0PFS=0x0008C1A8, PE0PFS=0x0008C1B0 -- match manual
- [OK] PF0PFS=0x0008C1B8 (PF3-PF4 reserved, PF6-PF7 reserved), PF5PFS=0x0008C1BD -- correct
- [OK] PG0PFS=0x0008C1C0, PH0PFS=0x0008C1C8, PJ0PFS=0x0008C1D0, PJ5PFS=0x0008C1D5 -- correct
- [OK] PWPR bits: B0WI=bit7=(1<<7), PFSWE=bit6=(1<<6) -- match manual p.882
- [OK] PFS bits: ASEL=bit7, ISEL=bit6, PSEL[4:0]=bits 0-4, mask=0x1F -- correct
- [OK] struct sizeof=216=0xD8 (static_assert present) -- correct
- [FIXED] 5 doc comments referenced "Chapter 20" / "Section 20.2" -- manual section numbers are "23.2.x", corrected to "Chapter 23" / "Section 23.2"

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 15 ICU (Interrupt Controller) Registers

**Pages read:** 466-541 (Ch 15 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_icu_regs.h

### Findings
- [OK] k_icu_base_addr=0x00087000 -- matches manual p.467
- [OK] ir[256] at +0x000 (IR016=0x00087010, IR255=0x000870FF) -- correct
- [OK] dtcer[256] at +0x100 (DTCER026=0x0008711A, DTCER255=0x000871FF) -- correct
- [OK] ier[32] at +0x200 (IER02=0x00087202 to IER1F=0x0008721F) -- correct
- [OK] swintr at +0x2E0, swint2r at +0x2E1 -- match manual p.472
- [OK] fir (uint16_t) at +0x2F0 -- match manual p.471
- [OK] ipr[256] at +0x300 (IPR000=0x00087300 to IPR255=0x000873FF) -- correct
- [OK] dmrsr[8] at +0x400 (DMRSR0=0x400, DMRSR1=0x404, ..., DMRSR7=0x41C, 4-byte spacing) -- correct
- [OK] irqcr[16] at +0x500 (IRQCR0=0x00087500 to IRQCR15=0x0008750F) -- correct
- [OK] irqflte[2] (uint8_t) at +0x520, irqfltc[2] (uint16_t) at +0x528 -- correct
- [OK] IRQFLTE0=0x520, IRQFLTE1=0x521, IRQFLTC0=0x528, IRQFLTC1=0x52A -- correct
- [OK] nmisr=+0x580, nmier=+0x581, nmiclr=+0x582, nmicr=+0x583 -- match manual p.479
- [OK] exnmisr=+0x584, exnmier=+0x585, exnmiclr=+0x586 -- correct
- [OK] nmiflte=+0x590, nmifltc=+0x594 -- match manual p.483-484
- [OK] All 9 reserved byte gap sizes verified correct (192,14,14,224,16,6,84,9,3)
- [OK] CMT vector numbers: CMT0_CMI0=28, CMT1_CMI1=29, IER03.IEN4/5 -- match manual p.503

### Commits
- (none, all OK, no fixes needed)

---

## 2026-04-13 -- Ch 24 MTU3a Registers

**Pages read:** 960-1198 (Ch 24) + Ch 5 I/O Address Table (MTU3a section, already read)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_mtu_regs.h,
                  star-rx72n-firmware/libs/rx_hal/src/rx_mtu.c,
                  star-rx72n-firmware/libs/rx_encoder/src/rx_mtu_encoder.c

### Findings

- [FIXED] `rx_mtu34_channel_regs_t`: CRITICAL -- struct assumed MTU3 registers were at
  consecutive byte offsets, but the actual RX72N hardware interleaves MTU3 and MTU4
  registers starting at 0x000C1200/0x000C1201 respectively, with shared MTU registers
  occupying many of the gaps.  Old struct used wrong offsets for EVERY register after TCR:
    - tmdr at +0x01 (was pointing to MTU4.TCR!)
    - tiorh at +0x02 (was pointing to MTU3.TMDR1!)
    - tiorl at +0x03 (was pointing to MTU4.TMDR1!)
    - tier at +0x04 (was pointing to MTU3.TIORH!)
    - tsr at +0x05 (was pointing to MTU3.TIORL!)
    - tcnt at +0x06 (was pointing to MTU4.TIORH!)
    - tgra at +0x08 (was pointing to MTU3.TIER!)
    - tgre at +0x10 (was pointing to MTU3.TCNT!)
    - tgrf field existed but MTU3 has NO TGRF (only MTU4 does)
    - tier2/tsr2 fields at completely wrong positions
  Actual MTU3 hardware offsets (Ch 5 I/O Table): TCR=+0x00, TMDR1=+0x02, TIORH=+0x04,
  TIORL=+0x05, TIER=+0x08, TCNT=+0x10, TGRA=+0x18, TGRB=+0x1A, TGRC=+0x24,
  TGRD=+0x26, TSR=+0x2C, TBTM=+0x38, TCR2=+0x4C, TGRE=+0x72 (total 0x74 bytes)
- [FIXED] Renamed `rx_mtu34_channel_regs_t` -> `rx_mtu3_channel_regs_t`; rewrote struct
  with correct reserved-byte padding (verified each gap against Ch 5 address table);
  added `mtu3_padding_sizes_t` enum with named constants for all padding array sizes
- [FIXED] `mtu3()` return type updated to `volatile rx_mtu3_channel_regs_t*`
- [FIXED] `mtu4()` changed to return `volatile void*` with documentation of MTU4's
  different offsets (MTU4 base=0x000C1201, registers at different relative offsets)
- [FIXED] All static assertions for `rx_mtu3_channel_regs_t` rewritten to verify
  actual hardware offsets; old assertions only checked struct-internal consistency
- [FIXED] Added `mtu_channel_reg_offsets_t`, `mtu3_reg_offsets_t`,
  `mtu12_phase_reg_offsets_t`, `mtu_periph_space_t` enums so static_assert
  comparisons use named constants (no magic numbers)
- [FIXED] Added `mtu12_phase_padding_sizes_t` enum for the 14-byte reserved gap
  in `rx_mtu12_phase_regs_t` between TMDR3 (+0x11) and TCNTLW (+0x20)
- [FIXED] `rx_mtu.c` internal_get_mtu_base(): MTU3/MTU4 cases now return nullptr
  instead of forwarding to mtu3()/mtu4() -- prevents callers from applying
  rx_mtu_channel_regs_t struct offset mapping to hardware that doesn't support it
- [FIXED] `rx_mtu_encoder.c` internal_get_mtu_base(): same fix as rx_mtu.c;
  MTU3/MTU4 encoder use unsupported; internal_is_valid_channel() now correctly
  returns false for those channels
- [OK] MTU0, MTU1, MTU2, MTU6, MTU7: standard rx_mtu_channel_regs_t layout correct
- [OK] rx_mtu12_phase_regs_t (used for STAR encoder 32-bit phase counting): layout
  verified; tcr=+0x00, tmdr=+0x01, tcnt=+0x06, tgra=+0x08, tgrb=+0x0A,
  tmdr3=+0x11, tcntlw=+0x20, tgralw=+0x24, tgrblw=+0x28, sizeof=0x2C -- all correct
- [OK] MTU base addresses: MTU0=0x000C1290, MTU1=0x000C1290, MTU2=0x000C1292,
  MTU3=0x000C1200, MTU4=0x000C1201, MTU6/7=0x000C1A00 -- match Ch 5 table

### Commits
- (see next commit)

---

## 2026-04-19 -- Ch 26 GPTW runtime bug sweep (HAL vs. hardware)

**Pages read:** 1240-1285 (Ch 26, re-read for runtime semantics), Ch 11 (MSTPCR)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/src/rx_gptw.c,
                  star-rx72n-firmware/libs/rx_hal/inc/rx72n_gptw_regs.h,
                  star-rx72n-firmware/libs/rx_hal/inc/rx72n_clock.h,
                  star-rx72n-firmware/pwm_test_hal/main.c

### Context
`pwm_test` (raw-register) proved real 20 kHz PWM works on RX72N + Tom's PCB via
HOCO x12 PLL (PCKA=96 MHz). The HAL variant `pwm_test_hal` initially failed
(output stuck at 0%/stuck HIGH). Diffing the HAL's post-init register state
against the verified raw sequence in `/tmp/pwm_clean.c` exposed four bit-level
bugs in the HAL plus one unlock oversight in `rx_gptw_set_duty_raw`.

### Findings

- [FIXED] **Wrong module-stop bit**: `rx72n_gptw_regs.h` defined
  `k_mstpc_gptw = 6` (MSTPCRC bit 6) and `internal_enable_gptw_module_clock()`
  cleared that bit. Per RX72N Hardware Manual Ch.11 (MSTPCRA @ 0x00080010) and
  Renesas iodefine.h, GPTW is **MSTPCRA bit 7 (MSTPA7)**. Replaced the enum
  with `k_mstpa_gptw = 7`, switched the clear to `system_regs()->mstpcra`, and
  changed the PRCR key from `k_rx_prcr_unlock_prc1_prc3` (0xA50B) to
  `k_rx_prcr_unlock_prc1` (0xA502) since only PRC1 is needed for MSTPCR writes.
  Comment block / @see tags updated accordingly.
- [FIXED] **GTUDDTYC never written** (counter defaulted to down-count):
  `internal_configure_gptw_hardware()` did not touch GTUDDTYC, so the reset
  value 0 persisted (UD=0 = down-count). With GTIOR=0x09 (compare-match-up
  drives the pin LOW), a down-counter never fires compare-match-up and the
  output latches HIGH at end-of-cycle. Added
  `gptw->gtuddtyc = k_gptw_gtuddtyc_ud | k_gptw_gtuddtyc_udf;` after the GTCR
  write to force up-count immediately.
- [FIXED] **GTBER single-buffer swallowed duty writes**:
  `internal_configure_gptw_hardware()` wrote
  `k_gptw_gtber_ccra_single | k_gptw_gtber_ccrb_single` (0x00050000), which
  enables copy-from-GTCCRC->GTCCRA and GTCCRE->GTCCRB at each period boundary.
  GTCCRC/E are 0 at reset, so any duty written to GTCCRA/B was overwritten
  with 0 on the next overflow. Changed to `gptw->gtber = 0U;` so direct
  GTCCRA/B writes take effect immediately. If double-buffered updates are
  ever needed, a separate opt-in API should enable buffering deliberately.
- [FIXED] **Hardcoded PCKA = 120 MHz**: `rx72n_clock.h` defined
  `k_pclka_hz = 120000000UL` from an older 240 MHz ICLK assumption, but the
  STAR firmware clock_init actually runs HOCO 16 MHz x12 PLL = 192 MHz,
  PCKA = 192/2 = **96 MHz**. A 20 kHz request therefore wrote GTPR =
  120M/20k - 1 = 5999 and the real output was 96M/6000 = 16 kHz.
  Changed `k_pclka_hz` to `96000000UL`; updated doc-block for the constant and
  the two "PCLKA = 120 MHz" comments in `rx_gptw.c` (period calc and deadtime
  example). This also silently fixes rx_mtu.c / rx_cmt.c timing math.
- [FIXED] **`rx_gptw_set_duty_raw` left GTWP locked**: GTCCRA/GTCCRB are
  protected by GTWP.WP on RX72N (Ch.26 Table 26.3). After
  `internal_configure_gptw_hardware` re-locks GTWP at the end of init, the
  subsequent `rx_gptw_set_duty_raw` write silently failed. Wrapped the
  GTCCRA/B write in `gtwp = unlock` / `gtwp = lock`; also locks on the
  invalid-output error path.

### Verification

Rebuilt pwm_test_hal with the three HAL files fixed and the four workarounds
removed from `pwm_test_hal/main.c` (`enable_gptw_module_mstpa7`,
`fix_gtpr_for_96mhz_pcka`, `fix_gptw_runtime_state`, plus the related local
MSTPCRA/GPTW register macros). Main now calls only `clock_init` ->
`rx_gptw_init_pwm` -> `repin_gtioc0_to_p23_p17` (board-specific) ->
`rx_gptw_set_duty_raw(..., 2400)` on A and B.

Flashed with rfp-cli, captured with `python3 /tmp/pwm20k_capture.py`:

```
Record: 0.5s @ 2.00MS/s, target 1000000
captured 213568 in 2.52s
DIO0 (P23): rises=2188 falls=2188 duty=50.0% freq=20.49 kHz
   first high-widths (us): ['24.5', '25.0', '24.5', '25.0', '25.0']
DIO1 (P17): rises=2188 falls=2188 duty=50.0% freq=20.49 kHz
   first high-widths (us): ['24.5', '25.0', '24.5', '25.0', '25.0']
```

Both channels: ~20.49 kHz (<= +/-3% of 20 kHz), 50.0% duty, >=2188 rises in
~0.1 s (well above the 1500 threshold). Pass.

### Commits
- (pending) -- fix(rx_gptw): MSTPA7, GTUDDTYC up-count, GTBER=0, PCKA=96MHz, GTWP unlock in set_duty_raw

---

## 2026-04-13 -- Ch 26 GPTW (General PWM Timer) Registers

**Pages read:** 1240-1285 (Ch 26 register descriptions)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_gptw_regs.h,
                  star-rx72n-firmware/libs/rx_hal/src/rx_gptw.c

### Findings

- [FIXED] GTIOR OAE (GTIOCA Output Enable): was `(1 << 6)` = b6 (OADFLT -- output value at
  count stop), manual p.1267 says OAE is **b8** -- corrected to `(1 << 8)`
- [FIXED] GTIOR OBE (GTIOCB Output Enable): was `(1 << 22)` = b22 (OBDFLT), manual p.1267
  says OBE is **b24** -- corrected to `(1 << 24)`
- [FIXED] GTIOR `oa_init_high = 0x0A`: Table 26.4 p.1269 shows 0x0A = b4=0 = initial LOW
  (not HIGH). Correct "initial HIGH, complementary" value is **0x16** (b4=1, low at EOC,
  high at compare match). Renamed comment to reflect actual behavior.
- [FIXED] GTIOR `ob_init_high = (0x0A << 16)`: same issue -- corrected to `(0x16 << 16)`
- [FIXED] GTIOR example comment: "high on compare match" was wrong for 0x09; Table 26.4
  shows 0x09 = b1:b0=01 = LOW at compare match. Corrected to "high at end-of-cycle, low
  at compare match".
- [FIXED] GTBER enum: all three values were wrong:
  - `ccra_buf = (1 << 0)` was BD[0] = GTCCRA/GTCCRB buffer **Disable** (not enable)
  - `ccrb_buf = (1 << 1)` was BD[1] = **GTPR** buffer Disable (not GTCCRB!)
  - `pr_buf = (1 << 16)` was CCRA[0] (GTCCRA mode bit), not GTPR
  Manual p.1277: CCRA single-buffer = `(0x01 << 16)`, CCRB single-buffer = `(0x01 << 18)`,
  PR single-buffer = `(0x01 << 20)`. BD[0:3] at b3:b0 are disable bits (1=disabled).
  Replaced entire enum with BD[0:3] disable bits plus ccra/ccrb/pr single/double constants.
- [FIXED] rx_gptw.c: `gptw->gtber = ccra_buf | ccrb_buf` was setting BD[0]=1 BD[1]=1
  which **disabled** GTCCRA/GTCCRB and GTPR buffering -- opposite of the "Enable buffer
  operation for glitch-free updates" comment intent. Corrected to
  `k_gptw_gtber_ccra_single | k_gptw_gtber_ccrb_single`.
- [FIXED] Added `gptw_padding_sizes_t` enum for struct reserved array sizes (replaces
  magic numbers `reserved[6]` and `reserved0[5]`)
- [FIXED] Added `gptw_ch_reg_offsets_t`, `gptw_common_reg_offsets_t`, and
  `gptw_periph_space_t` enums for all static_assert comparisons (replaces raw hex literals)
- [OK] GTINTAD: GRP[1:0]=b25:b24, GRPDTE=b28, GRPABH=b29, GRPABL=b30 -- match p.1270
- [OK] GTST: TUCF=b15 -- matches p.1273 (session summary erroneously said b14)
- [OK] All 55 channel register offsets and 6 common register offsets -- correct
- [OK] Base addresses: GPTW0=0x000C2000, GPTW1=0x000C2100, GPTW2=0x000C2200,
  GPTW3=0x000C2300, common=0x000C2B00 -- match manual p.1240
- [OK] sizeof(rx_gptw_channel_regs_t) == 0xD8 (216 bytes) -- correct
- [OK] sizeof(rx_gptw_common_regs_t) == 0x2C (44 bytes) -- correct
- [OK] GTCR: CST=b0, MD[2:0]=b18:b16, TPCS[3:0]=b26:b23 -- correct
- [OK] GTUDDTYC, GTDTCR, GTSTR/GTSTP/GTCLR bit definitions -- correct

### Commits
- c6ecedd3d -- fix(gptw_regs): correct GTIOR OAE/OBE bits, GTBER enum, and init_high value

---

## 2026-04-13 -- Ch 25 POE3a Registers

**Pages read:** 1199-1235 (Ch 25 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_poe3_regs.h

### Findings
- [OK] k_poe3_base_addr=0x00008900 -- matches manual p.1199
- [OK] ICSR1 at +0x00, ICSR2 at +0x04, ICSR3 at +0x08, ICSR4 at +0x0C, ICSR5 at +0x10 -- correct
- [OK] ICSR6 at +0x14 -- correct
- [OK] OCSR1 at +0x02, OCSR2 at +0x06 -- correct
- [OK] ALR1 at +0x1A -- correct
- [OK] SPOER at +0x20, POECR1 at +0x21, POECR2 at +0x22, POECR3 at +0x24 -- correct
- [OK] POECR4 at +0x26, POECR5 at +0x28 -- correct
- [OK] ICSR1/2/3 bit enums: PIE=b0, POE0M/4M/8M=b2:b1, PIE6=b6, PIDE=b4, POF=b12 -- correct
- [OK] ICSR4 bits: IC4PIE=b0, IC4POE4M=b2:b1 -- correct
- [OK] ICSR5 bits: IC5PIE=b0, IC5POE4M=b2:b1 -- correct
- [OK] ICSR6 POE10M/11M fields, POE10E/11E enable bits -- correct
- [FIXED] ICSR6.OSTSTE: was 0x0100U (bit 8), manual p.1219 says bit 9 -- corrected to 0x0200U
- [OK] OCSR1/2 bits: OCE=b8, OCF=b9 -- correct
- [OK] SPOER bits: MTUCH0HIZ=b0, MTUCH67HIZ=b1, MTUCH34HIZ=b2 -- correct
- [OK] POECR1 bits: MTU0AZE=b0, MTU0BZE=b1, MTU0CZE=b2, MTU0DZE=b3, MTU0E1ZE=b4, MTU0E2ZE=b5 -- correct
- [FIXED] POECR2: was uint8_t with 3 wrong values; manual p.1221 shows 16-bit register with
  MTU3/4 bits at 10:8 and MTU6/7 bits at 2:0 -- rewritten as uint16_t with 6 correct values:
  k_poecr2_mtu7bdze=0x0001, k_poecr2_mtu7acze=0x0002, k_poecr2_mtu6bdze=0x0004,
  k_poecr2_mtu4bdze=0x0100, k_poecr2_mtu4acze=0x0200, k_poecr2_mtu3bdze=0x0400
- [FIXED] poe3_poecr2_reg() accessor: was volatile uint8_t*, corrected to volatile uint16_t*
- [OK] POECR3 bits: MTU3BDZE=b0, MTU3ACZE=b1, MTU4BDZE=b2, MTU4ACZE=b3 (8-bit reg) -- correct
- [FIXED] POECR4: was wrong MTU6/7 ZE bits; manual p.1229 shows ICxADDMT34/67ZE conditional
  hi-Z source bits -- rewritten with 8 correct values:
  k_poecr4_ic2addmt34ze=0x0004, k_poecr4_ic3addmt34ze=0x0008, k_poecr4_ic4addmt34ze=0x0010,
  k_poecr4_ic5addmt34ze=0x0020, k_poecr4_ic1addmt67ze=0x0200, k_poecr4_ic3addmt67ze=0x0800,
  k_poecr4_ic4addmt67ze=0x1000, k_poecr4_ic5addmt67ze=0x2000
- [FIXED] POECR5: was wrong POE10/11 mapping; manual p.1232 shows ICxADDMT0ZE conditional
  hi-Z source bits -- rewritten with 4 correct values:
  k_poecr5_ic1addmt0ze=0x0002, k_poecr5_ic2addmt0ze=0x0004,
  k_poecr5_ic4addmt0ze=0x0010, k_poecr5_ic5addmt0ze=0x0020
- [AVAILABLE] POE3a peripheral (MTU-based PWM emergency stop) -- added to AVAILABLE_FEATURES.md

### Commits
- f558aeaf0 -- fix(poe3): correct ICSR6 OSTSTE bit, POECR2 size/layout, POECR4/5 bit definitions

---

## 2026-04-13 -- Ch 27 POEG Registers

**Pages read:** 1432-1441 (Ch 27 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_poeg_regs.h,
                  star-rx72n-firmware/libs/rx_hal/src/rx_poeg.c

### Findings
- [OK] k_poegga_base_addr=0x0009E000, k_poeggb_base_addr=0x0009E100,
  k_poeggc_base_addr=0x0009E200, k_poeggd_base_addr=0x0009E300 -- match manual p.1432
- [OK] POEGGn register at offset +0x00 (single 32-bit register per group) -- correct
- [OK] PIDF=b0, IOCF=b1, OSTPF=b2, SSF=b3 (status/fault flags) -- correct
- [OK] PIDE=b4, IOCE=b5, OSTPE=b6 (enable bits) -- correct
- [OK] ST=b16 (pin state read-back) -- correct
- [OK] INV=b28 (input invert), NFEN=b29 (noise filter enable) -- correct
- [OK] NFCS[1:0]=b31:b30 (noise filter clock select) -- correct
- [OK] NFCS values: div1=0x00000000, div8=0x40000000, div32=0x80000000, div128=0xC0000000 -- correct
- [OK] k_poeg_ssf_stop (bit 3 / SSF) -- correct
- [OK] k_poeg_pide_enable, k_poeg_nfen_enable -- correct
- [OK] POEG interrupt vectors 188-191: valid BL2 group software-configurable interrupt B
  assignments per ICU Table 15.7 (range 144-207) -- correct design choice
- [OK] rx_poeg.c init sequence (PIDE+INV+NFEN+NFCS write, GTINTAD linkage, ICU setup) -- correct

### Commits
- (none, all OK, no fixes needed)

---

## 2026-04-13 -- Ch 3 Operating Modes + Ch 11 Low Power Consumption

**Pages read:** 195-202 (Ch 3 complete), 401-449 (Ch 11 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_lpc_regs.h,
                  star-rx72n-firmware/libs/rx_hal/inc/rx72n_system_regs.h

### Findings

**Ch 3 -- Operating Modes (pages 195-202)**

- [OK] MDMONR @ 0x00080000 (struct offset 0x00, 16-bit): k_mdmonr_md=0x0001 (b0=MD pin status) -- matches manual p.196
- [OK] SYSCR0 @ 0x00080006 (struct offset 0x06, 16-bit): k_syscr0_rome=0x0001 (b0), k_syscr0_exbe=0x0002 (b1), k_syscr0_key=0x5A00 (b15:b8) -- matches manual p.197
- [OK] SYSCR1 @ 0x00080008 (struct offset 0x08, 16-bit): k_syscr1_rame=0x0001 (b0), k_syscr1_eccrame=0x0040 (b6), k_syscr1_sbyrame=0x0080 (b7), reserved_mask=0x003E (b5:b1) -- matches manual p.198
- [OK] rx_system_regs_t struct: reserved0[4] @ 0x02-0x05, reserved1[2] @ 0x0A-0x0B, all gap sizes correct -- matches manual address layout
- [AVAILABLE] External bus mode (SYSCR0.EXBE) -- added to AVAILABLE_FEATURES.md

**Ch 11 -- Low Power Consumption (pages 401-449)**

- [OK] SBYCR @ 0x0008000C (struct offset 0x0C, 16-bit): field exists at correct address; reserved2[2] @ 0x0E-0x0F gap correct -- matches manual p.405
- [OK] MSTPCRA @ 0x00080010 (struct offset 0x10, 32-bit): k_mstpa_cmt23=14 (b14=CMT unit 1, CMT2/CMT3) -- matches manual p.406
- [OK] MSTPCRB @ 0x00080014 (struct offset 0x14, 32-bit): k_mstpb_usb0=19 (b19=USB0), k_mstpb_crc=23 (b23=CRC) -- matches manual p.408-409
- [OK] MSTPCRC @ 0x00080018 (struct offset 0x18, 32-bit): field at correct address -- matches manual p.410-411
- [OK] MSTPCRD @ 0x0008001C (struct offset 0x1C, 32-bit): field at correct address -- matches manual p.412-413
- [OK] OPCCR @ 0x000800A0 (8-bit): k_opccr_opcm_highspeed=0x00 (000b), k_opccr_opcm_lowspeed1=0x06 (110b), k_opccr_opcm_lowspeed2=0x07 (111b), k_opccr_opcm_mask=0x07, k_opccr_opcmtsf=0x10 (b4) -- matches manual p.414
- [OK] RSTCKCR @ 0x000800A1 (8-bit): k_rstckcr_rstcksel_hoco=0x01 (001b), k_rstckcr_rstcksel_main=0x02 (010b), k_rstckcr_rstcksel_mask=0x07, k_rstckcr_rstcken=0x80 (b7) -- matches manual p.417
- [OK] DPSBYCR @ 0x0008C280 (8-bit): k_dpsbycr_deepcut_ram_usb_on=0x00, k_dpsbycr_deepcut_ram_usb_off=0x01, k_dpsbycr_deepcut_lvd_off=0x03, k_dpsbycr_deepcut_mask=0x03, k_dpsbycr_iokeep=0x40 (b6), k_dpsbycr_dpsby=0x80 (b7) -- matches manual p.418
- [OK] DPSIER0 @ 0x0008C282 (8-bit): DIRQ0E-DIRQ7E = 0x01-0x80 (b0-b7) -- matches manual p.420
- [OK] DPSIER1 @ 0x0008C283 (8-bit): DIRQ8E-DIRQ15E = 0x01-0x80 (b0-b7) -- matches manual p.421
- [OK] DPSIER2 @ 0x0008C284 (8-bit): DLVD1IE=0x01(b0), DLVD2IE=0x02(b1), DRTCIIE=0x04(b2), DRTCAIE=0x08(b3), DNMIE=0x10(b4), DRIICDIE=0x20(b5), DRIICCIE=0x40(b6), DUSBIE=0x80(b7) -- matches manual p.422
- [OK] DPSIER3 @ 0x0008C285 (8-bit): k_dpsier3_dcanie=0x01 (b0=DCANIE=CRX1-DS) -- matches manual p.423
- [OK] DPSIFR0 @ 0x0008C286 (8-bit): DIRQ0F-DIRQ7F = 0x01-0x80 (b0-b7) -- matches manual p.424
- [OK] DPSIFR1 @ 0x0008C287 (8-bit): DIRQ8F-DIRQ15F = 0x01-0x80 (b0-b7) -- matches manual p.425
- [OK] DPSIFR2 @ 0x0008C288 (8-bit): DLVD1IF=0x01, DLVD2IF=0x02, DRTCIIF=0x04, DRTCAIF=0x08, DNMIF=0x10, DRIICDIF=0x20, DRIICCIF=0x40, DUSBIF=0x80 -- matches manual p.426
- [OK] DPSIFR3 @ 0x0008C289 (8-bit): k_dpsifr3_dcanif=0x01 (b0=DCANIF=CRX1-DS) -- matches manual p.429
- [OK] DPSIEGR0 @ 0x0008C28A (8-bit): DIRQ0EG-DIRQ7EG = 0x01-0x80 (b0-b7, 0=fall/1=rise) -- matches manual p.430
- [OK] DPSIEGR1 @ 0x0008C28B (8-bit): DIRQ8EG-DIRQ15EG = 0x01-0x80 (b0-b7) -- matches manual p.431
- [OK] DPSIEGR2 @ 0x0008C28C (8-bit): DLVD1EG=0x01(b0), DLVD2EG=0x02(b1), DNMIEG=0x10(b4), DRIICDEG=0x20(b5), DRIICCEG=0x40(b6) -- matches manual p.432; b3:b2 and b7 reserved (no code values, correct)
- [OK] DPSIEGR3 @ 0x0008C28D (8-bit): k_dpsiegr3_dcanieg=0x01 (b0=DCANIEG) -- matches manual p.432
- [OK] DPSBKRy @ 0x0008C2A0-0x0008C2BF: k_dpsbkr_base_addr=0x0008C2A0, k_dpsbkr_size=32 -- matches manual p.433
- [OK] rx_dps_regs_t struct: dpsbycr@+0x00, reserved@+0x01, dpsier0@+0x02, ..., dpsiegr3@+0x0D (14 bytes total) -- matches manual register layout
- [OK] All static_assert address checks in lpc_regs.h match manual exactly (all 14 assertions)
- [FIXED] system_regs.h @par Manual References: "Chapter 7: Low Power Modes" was wrong; Chapter 7 is Option-Setting Memory; corrected to "Chapter 11: Low Power Consumption"
- [AVAILABLE] Software Standby Mode (SBYCR.SSBY=b15, SBYCR.OPE=b14) -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] All-Module Clock Stop Mode (MSTPCRA.ACSE=b31) -- added to AVAILABLE_FEATURES.md

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 8 LVDA (Voltage Detection Circuit) Registers

**Pages read:** 316-334 (Ch 8 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_lvda_regs.h

### Findings

- [OK] k_lvd1cr1_addr=0x000800E0 -- matches manual p.319 (section 8.2.1)
- [OK] k_lvd1sr_addr=0x000800E1 -- matches manual p.319 (section 8.2.2)
- [OK] k_lvd2cr1_addr=0x000800E2 -- matches manual p.320 (section 8.2.3)
- [OK] k_lvd2sr_addr=0x000800E3 -- matches manual p.320 (section 8.2.4)
- [OK] k_lvcmpcr_addr=0x0008C297 -- matches manual p.321 (section 8.2.5)
- [OK] k_lvdlvlr_addr=0x0008C298 -- matches manual p.322 (section 8.2.6)
- [OK] 0x0008C299 reserved (no register) -- correctly noted in code comment
- [OK] k_lvd1cr0_addr=0x0008C29A -- matches manual p.323 (section 8.2.7)
- [OK] k_lvd2cr0_addr=0x0008C29B -- matches manual p.324 (section 8.2.8)
- [OK] LVD1CR1/LVD2CR1 bit layout: LVD1IDTSEL[1:0]=b1:b0, LVD1IRQSEL=b2, b7:b3=reserved -- correct
- [OK] LVD1CR1 reset value=0x01 (b0=1) -- matches manual p.319
- [OK] IDTSEL encoding: 00=rise, 01=drop, 10=both (k_lvd_idtsel_rise/drop/both) -- matches p.319
- [OK] IRQSEL: 0=NMI, 1=maskable (k_lvd_irqsel_nmi/maskable at bit 2) -- matches p.319/320
- [OK] LVD1SR/LVD2SR bit layout: LVD1DET=b0, LVD1MON=b1, b7:b2=reserved -- correct
- [OK] LVD1SR reset value=0x02 (b1=1, b0=0) -- matches manual p.319
- [OK] k_lvd_det_clear/detected, k_lvd_mon_below/above -- correct values and bit positions
- [OK] LVCMPCR bit layout: b4:b0=reserved, b5=LVD1E, b6=LVD2E, b7=reserved -- matches p.321
- [OK] k_lvd1e_enable=(1<<5)=0x20, k_lvd2e_enable=(1<<6)=0x40 -- correct
- [OK] LVDLVLR bit layout: b3:b0=LVD1LVL[3:0], b7:b4=LVD2LVL[3:0] -- matches p.322
- [OK] LVDLVLR reset value=0xBB (1011_1011b = both at 2.85V) -- matches p.322
- [OK] k_lvd1lvl_2v99=0x09 (1001b), k_lvd1lvl_2v92=0x0A, k_lvd1lvl_2v85=0x0B -- matches p.322
- [OK] k_lvd2lvl_2v99=0x90, k_lvd2lvl_2v92=0xA0, k_lvd2lvl_2v85=0xB0 -- correct (bits 7:4)
- [OK] LVD1CR0/LVD2CR0 bit layout: RIE=b0, DFDIS=b1, CMPE=b2, b3=reserved, FSAMP=b5:b4, RI=b6, RN=b7 -- matches p.323/324
- [OK] FSAMP encoding: 00=LOCO/2, 01=LOCO/4, 10=LOCO/8, 11=LOCO/16 -- matches p.323
- [OK] RI: 0=interrupt mode, 1=reset mode -- matches p.323
- [OK] RN: 0=negate after VCC>Vdet stabilization, 1=negate after assert stabilization -- matches p.323
- [OK] k_lvd1_irq=88, k_lvd2_irq=89 -- confirmed correct from Table 15.5 p.512 (LVD1=vec88, LVD2=vec89)
- [FIXED] rx_lvd_interrupts_t: used uint16_t for vector numbers 88 and 89 -- values fit in uint8_t; corrected to uint8_t per project coding standard (smallest type that fits all values)
- [OK] No LVDA usage found in rx_hal/src/ -- header-only peripheral definition

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 9 Clock Generation

**Pages read:** 335-389 (Ch 9 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_system_regs.h,
                  star-rx72n-firmware/libs/rx_hal/inc/rx72n_clock.h,
                  star-rx72n-firmware/libs/rx_core/src/rx_infrastructure.c

### Findings

- [OK] rx_system_regs_t base address = 0x00080000 -- matches manual p.335
- [OK] sckcr offset = +0x20 (0x00080020) -- matches manual p.337
- [OK] sckcr2 offset = +0x24 (0x00080024) -- matches manual p.342
- [OK] sckcr3 offset = +0x26 (0x00080026) -- matches manual p.343
- [OK] pllcr offset = +0x28 (0x00080028) -- matches manual p.345
- [OK] pllcr2 offset = +0x2A (0x0008002A) -- matches manual p.347
- [OK] bckcr offset = +0x30 (0x00080030) -- matches manual p.348
- [OK] mosccr offset = +0x32 (0x00080032) -- matches manual p.349
- [OK] sosccr offset = +0x33 (0x00080033) -- matches manual p.350
- [OK] lococr offset = +0x34 (0x00080034) -- matches manual p.351
- [OK] ilococr offset = +0x35 (0x00080035) -- matches manual p.352
- [OK] hococr offset = +0x36 (0x00080036) -- matches manual p.353
- [OK] hococr2 offset = +0x37 (0x00080037) -- matches manual p.353
- [OK] oscovfsr offset = +0x3C (0x0008003C) -- matches manual p.354
- [OK] ckocr offset = +0x3E (0x0008003E) -- matches manual p.363
- [OK] ostdcr offset = +0x40 (0x00080040) -- matches manual p.369
- [OK] ostdsr offset = +0x41 (0x00080041) -- matches manual p.371
- [OK] sckcr SCKCR3[2:0] (CKSEL) at bits 10:8 (shift 8, mask 0x700) -- matches manual p.344
- [OK] sckcr3_cksel LOCO=0x0000, HOCO=0x0100, MAIN=0x0200, SUBCLOCK=0x0300, PLL=0x0400 -- match manual p.344
- [OK] pllcr PLLMUL field at bits 13:8 (k_pllcr_pllmul_x10=0x1300) -- matches manual p.345-346
- [OK] pllcr2 PLLEN bit 0 (0=enable, 1=disable) -- matches manual p.347
- [OK] hococr HCSTP bit 0 (0=operate, 1=stop) -- matches manual p.353
- [OK] mosccr MOSTP bit 0 (0=operate, 1=stop) -- matches manual p.349
- [OK] bckcr BCLKDIV bit 0 (0=/1, 1=/2) -- matches manual p.348
- [OK] rx_clock_frequencies_t enum values: ICLK=240000000, PCLKA=120000000, PCLKB=60000000 -- correct for 24 MHz x10 PLL configuration
- [OK] rx_ppll_addresses_t: PPLLCR=0x00080046, PPLLCR2=0x00080048 -- match manual p.366,367
- [OK] PPLLCR PPLSTC field at bits 13:8 -- matches manual p.366
- [FIXED] sckcr_divider_t: contained k_clock_div_128=8 -- manual p.339-340 shows SCKCR divider fields only accept values 0-6 (/1 through /64); value 7 and above are PROHIBITED. Removed k_clock_div_128. Added k_bck_div_3=9 (/3, valid ONLY for BCK field, binary 1001 per p.339).
- [FIXED] sckcr_bits_t PSTOP0/PSTOP1 comments: code said "PCLKA/PCLKB stop" and "USB clock stop" -- manual p.340 says PSTOP0 controls SDCLK pin output, PSTOP1 controls BCLK pin output. Corrected both comments.
- [FIXED] sckcr3_cksel_t: contained k_sckcr3_cksel_ppll=0x0500 as a valid SCKCR3 CKSEL value -- manual p.344 shows only values 000-100 (LOCO/HOCO/Main/Sub/PLL) are valid; 101-111 are PROHIBITED. PPLL is not a valid system clock source. Removed k_sckcr3_cksel_ppll entirely.
- [FIXED] pllcr_bits_t: k_pllcr_plidiv_4=0x0002 was named as if binary 10 divides by 4 -- manual p.345 PLIDIV table: 00=/1, 01=/2, 10=/3, 11=PROHIBITED. Binary 10 divides by 3, not 4. Renamed to k_pllcr_plidiv_3=0x0002.
- [FIXED] ckocr_bits_t: k_ckocr_ckodiv_32=(5U<<12), k_ckocr_ckodiv_64=(6U<<12), k_ckocr_ckodiv_128=(7U<<12) -- manual p.364 CKODIV table: 000=/1 through 100=/16 (values 0-4 valid); values 5, 6, 7 are PROHIBITED. Removed all three invalid entries. Also added missing k_ckocr_ckosel_ppll=(6U<<8) (PPLL circuit, valid per manual p.364).
- [FIXED] rx_ppll_config_t: k_ppll_config_48mhz=0x0703 -- lower 2 bits (0x03=binary 11) set PPLIDIV to 11 which is PROHIBITED per manual p.366. Correct PPLL config: PPLSTC=7 at bits 13:8, PPLIDIV=00 at bits 1:0 => 0x0700. Fixed to 0x0700.
- [FIXED] rx_ppll_flags_t: k_ppll_stable_flag=0x08 claiming "OSCOVFSR bit 3 = PPLL stable" -- manual p.355 OSCOVFSR bit 3 is HCOVF (HOCO stabilization), not PPLL. PPLOVF (PPLL stabilization) is bit 5 = mask 0x20. Corrected from 0x08 to 0x20. This bug caused the PPLL stability wait loop to check the HOCO stable bit instead of the PPLL stable bit.
- [FIXED] oscovfsr struct member comment: said "Bit 3: PPLL stable" -- manual p.355 lists all 6 OSCOVFSR bits: Bit 0 MOOVF (main), Bit 1 SOOVF (sub), Bit 2 PLOVF (PLL), Bit 3 HCOVF (HOCO), Bit 4 ILCOVF (IWDT clock), Bit 5 PPLOVF (PPLL). Rewrote comment listing all 6 bits with correct symbol names.
- [AVAILABLE] MOSCWTCR (0x00080A2) -- main clock wait control; STAR uses default -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] SOSCWTCR (0x00080A3) -- sub-clock wait control; sub-clock unused -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] MOFCR (0x0008C293) -- main oscillator forced oscillation control; STAR uses default -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] HOCOPCR (0x0008C294) -- HOCO power supply control; STAR does not power down HOCO -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] PACKCR (0x00080044) -- USB/Ethernet specific-use clock control; STAR does not configure -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] PPLLCR3 (0x0008004B) -- PPLL frequency divider; never written in firmware -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] Oscillation stop detection via OSTDCR/OSTDSR; registers defined but OSTDE never enabled -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] CLKOUT pin output via CKOCR; register defined but CKOEN never set -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] SDCLK pin output via SCKCR.PSTOP0; STAR has no SDRAM -- logged to AVAILABLE_FEATURES.md
- [AVAILABLE] Sub-clock oscillator (SOSCCR); STAR does not start sub-clock -- logged to AVAILABLE_FEATURES.md

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 21 ELC (Event Link Controller)

**Pages read:** 835-860 (Ch 21 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_elc_regs.h

### Findings

- [OK] ELCR @ 0x0008B100: ELCON=bit7=0x80, reserved bits 6:0 = 0x7F -- correct
- [OK] All ELSRn base addresses: ELSR0=0x0008B101, ELSR3=0x0008B104, ELSR4=0x0008B105,
  ELSR7=0x0008B108, ELSR10-13=0x0008B10B-10E, ELSR15-16=0x0008B110-111,
  ELSR18-28=0x0008B113-11D -- all match manual p.837
- [OK] ELSR33=0x0008B131, ELSR35-38=0x0008B133-136, ELSR45=0x0008B13D,
  ELSR48-57=0x0008B146-14F -- all match manual p.837
- [OK] ELOPA=0x0008B11F, ELOPB=0x0008B120, ELOPC=0x0008B121, ELOPD=0x0008B122 -- correct
- [OK] ELOPF=0x0008B13F, ELOPH=0x0008B141 -- correct
- [OK] PGR1=0x0008B123, PGR2=0x0008B124, PGC1=0x0008B125, PGC2=0x0008B126 -- correct
- [OK] PDBF1=0x0008B127, PDBF2=0x0008B128, PEL0-3=0x0008B129-12C, ELSEGR=0x0008B12D -- correct
- [OK] ELSRn event values: MTU0 (0x01-0x07), MTU3 (0x10-0x14), MTU4 (0x15-0x1A),
  CMT1 (0x1F), TMR0 (0x22-0x24), S12AD (0x58), S12AD1 (0x6C), DMAC0-3 (0x5D-0x60),
  DTC (0x61), port events (0x63-0x68), software (0x69), GPTW0-3 (0x80-0x9F) -- all correct
- [OK] ELOPB MTU4MD[1:0] at bits 1:0 (shift=0, mask=0x03) -- correct
- [OK] ELOPD TMR0-3MD[1:0] fields at bits 1:0/3:2/5:4/7:6 -- correct
- [OK] ELSEGR SEG=bit0=0x01, WE=bit6=0x40, WI=bit7=0x80 -- correct
- [OK] elc_mstpcr_t: MSTPCRB bit 9 = (1<<9) -- correct
- [OK] PGR1/PGR2 bit layout: PGR0-7 at bits 0-7 -- correct
- [FIXED] ELOPA MTU3MD[1:0]: shift was 4/mask 0x30 -- manual p.843 shows bits 7:6;
  corrected to shift=6/mask=0xC0. Also fixed layout comment (4-bit reserved [5:2],
  MTU3MD at [7:6]; not 2-bit reserved then MTU3 at [5:4]).
- [FIXED] ELOPC CMT1MD[1:0]: shift was 0/mask 0x03 -- manual p.844 shows bits 3:2;
  corrected to shift=2/mask=0x0C. Fixed layout comment (b1:b0 reserved, b3:b2 CMT1MD,
  b7:b4 reserved; not b1:b0 CMT1MD and b7:b2 reserved).
- [FIXED] elop_timer_op_t: values 0x01 and 0x02 were swapped. Manual p.843-846 says
  0x01=counting restarted, 0x02=input capture (MTU/TPU) / event counter (CMT/TMR/CMTW).
  Was: compare_match=0x00, input_capture=0x01, reserved=0x02, disabled=0x03.
  Now: count_start=0x00, count_restart=0x01, input_capture=0x02, disabled=0x03.
- [FIXED] pgc_bits_t: entire register layout was wrong. Manual p.848 shows PGCI[1:0]
  at bits 1:0, PGCOVE at bit 2, PGCO[2:0] at bits 6:4. Code had PGCOVE=0x01 (bit 0),
  spurious PGCOSEL field at bit 1, single-bit PGCO=0x08 (bit 3), PGCI at shift=4/
  mask=0x30. Rewrote with correct positions, added PGCO 3-bit operation values
  (low/high/toggle/buffer/rotate), fixed PGCI edge values (rising=0x00, falling=0x01,
  both=0x02). Removed nonexistent PGCOSEL and wrong AND/OR/XOR labels.
- [FIXED] pel_bits_t: three bugs. (a) PSB mask was 0x03 (2-bit) -- manual p.850 shows
  PSB[2:0] is a 3-bit field for pins 0-7; corrected to 0x07. (b) Port E was 0x04
  (bit 2) -- PSP[1:0] is at bits 4:3; Port E=PSP=10b=bit4=0x10, Port B=0x08; corrected
  to 0x10. (c) ELD/PSM values were wrong: code named PSM=0x00 as "none", 0x20 as
  "rising", 0x40 as "falling" -- manual shows 0x00=rising, 0x20=falling, 0x40=both;
  names and semantics were shifted/swapped. Removed nonexistent "psm_group" field.
  Rewrote with PSB/PSP/PSM naming matching the manual.
- [FIXED] elc_elsr_reg(n): formula was base+1+n for all n, but ELSR33 is at
  0x0008B131=base+0x10+33, not base+1+33=0x0008B122 (ELOPD!). Added elsr_range_adj_t
  enum with adjustments for the three non-contiguous address ranges, and rewrote
  function to select the correct adjustment per range.
- [AVAILABLE] Multiple ELC event sources not declared in elsr_event_t: RTC, IWDT, SCI5,
  RIIC0, RSPI0, LVD1/2, oscillation stop, DOC, Ethernet EPTPC -- logged in
  AVAILABLE_FEATURES.md
- [AVAILABLE] ELOPF bit definitions (TPU0-3 operation select) not defined -- logged
- [AVAILABLE] ELOPH bit definitions (CMTW0 operation select) not defined -- logged
- [AVAILABLE] Port event I/O (PGRn, PGCn, PDBFn, PELm) and software event (ELSEGR)
  registers are defined but not used in STAR firmware -- logged

### Commits
- 5ba8a4338 -- fix(elc): correct ELOPA/ELOPC bit positions, timer op values, PGC/PEL layout, ELSRn formula

---

## 2026-04-13 -- Ch 28 TPU (Timer Pulse Unit) Registers

**Pages read:** 1442-1519 (Ch 28 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_tpu_regs.h,
                  star-rx72n-firmware/libs/rx_hal/src/rx_tpu.c,
                  star-rx72n-firmware/libs/rx_encoder/src/rx_encoder_tpu.c

### Findings

- [OK] k_tpu_base_addr=0x000C1200 (control regs), k_tpu_ext0_base_addr=0x000C1100 (TPU0 ext), k_tpu3_base_addr=0x000C1180 (TPU3) -- match manual p.1442
- [OK] k_tpu1_base_addr=0x000C1108, k_tpu2_base_addr=0x000C1118, k_tpu4_base_addr=0x000C1188, k_tpu5_base_addr=0x000C1198 -- match Ch 5 address table
- [OK] rx_tpu_control_regs_t: tstr=+0x00, tsyr=+0x01, reserved[6]=+0x02, nfcr[6]=+0x08 -- correct (total 14 bytes)
- [OK] rx_tpu_ext_regs_t (TPU0/3): tcr=+0x00, tmdr=+0x01, tiorh=+0x02, tiorl=+0x03, tier=+0x04, tsr=+0x05, tcnt=+0x06, tgra=+0x08, tgrb=+0x0A, tgrc=+0x0C, tgrd=+0x0E -- match manual p.1443-1444
- [OK] rx_tpu_regs_t (TPU1/2/4/5): tcr=+0x00, tmdr=+0x01, tior=+0x02, reserved=+0x03, tier=+0x04, tsr=+0x05, tcnt=+0x06, tgra=+0x08, tgrb=+0x0A, reserved2[4]=+0x0C -- correct
- [OK] TSTR bit values: k_tpu_tstr_cst0=bit0=0x01, cst1=bit1=0x02, cst2=bit2=0x04, cst3=bit3=0x08, cst4=bit4=0x10, cst5=bit5=0x20 -- match manual p.1445
- [OK] TSYR bit values: k_tpu_tsyr_sync0..sync5=bits 0-5 -- match manual p.1446
- [OK] TMDR mode encoding: k_tpu_tmdr_md_normal=0x00, buffer_a=0x04, buffer_ab=0x06, pwm1=0x02, pwm2=0x03, phase_count_1=0x04...phase_count_4=0x07 -- match manual Table 28.5 p.1448
- [OK] TIER bit values: tgiea=bit0=0x01, tgieb=bit1=0x02, tgiec=bit2=0x04, tgied=bit3=0x08, tciev=bit4=0x10, tcieu=bit5=0x20, ttge=bit7=0x80 -- match manual p.1461
- [OK] TSR status bit values: tgfa=bit0=0x01, tgfb=bit1=0x02, tgfc=bit2=0x04, tgfd=bit3=0x08, tcfv=bit4=0x10, tcfu=bit5=0x20, tcfd=bit7=0x80 -- match manual p.1462-1463
- [OK] NFCR bit values: nfaen=bit0=0x01, nfben=bit1=0x02, nfcen=bit2=0x04, nfden=bit3=0x08, nfcs[1:0]=bits 4-5 -- match manual p.1469
- [OK] k_tpu_tmdr_md_phase_count_4=0x07 (phase counting mode 4, 4x resolution) -- match manual Table 28.5 p.1448
- [OK] rx_tpu_init_phase_count() writes TCR=0x00 (free-run, internal clock), TMDR=phase_mode, TCNT=0x0000 -- correct per manual p.1446-1449
- [OK] rx_tpu_start() sets TSTR.CSTn bit to start counter -- correct
- [OK] rx_tpu_stop() clears TSTR.CSTn bit to stop counter -- correct
- [OK] rx_encoder_tpu.c: uses HAL API only, no direct register writes -- clean
- [FIXED] k_tpu_tier_all_disabled: was 0x00, manual p.1461 states TIER bit 6 is reserved
  and "must be written as 1" (reset value 0x40). Writing 0x00 violates specification.
  Corrected to 0x40 in tpu_tier_disable_t enum in rx_tpu.c (affects init path line ~485
  and deinit path line ~846). Updated Doxygen comment to explain the bit 6 constraint.
- [AVAILABLE] TPU PWM output, input capture, cascaded 32-bit counter, buffer operation,
  A/D trigger, DTC/DMAC activation, ELC event link, noise filter -- added to AVAILABLE_FEATURES.md

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 40 USB 2.0 Full-Speed (USBb) Registers

**Pages read:** 1928-2036 (Ch 40 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_usb_regs.h,
                  star-rx72n-firmware/libs/rx_usb/src/rx_usb_hw.c,
                  star-rx72n-firmware/libs/rx_usb/src/rx_usb_isr.c

### Findings

- [OK] k_usb0_base_addr = 0x000A0000 -- matches manual p.1929 (Table 40.1)
- [OK] SYSCFG offset = 0x0000, SYSSTS0 = 0x0004, DVSTCTR0 = 0x0008 -- correct
- [OK] CFIFO = 0x0014, D0FIFO = 0x0018, D1FIFO = 0x001C (16-bit WORD registers) -- correct
- [OK] CFIFOSEL = 0x0020, CFIFOCTR = 0x0022, D0FIFOSEL = 0x0028, D0FIFOCTR = 0x002A -- correct
- [OK] D1FIFOSEL = 0x002C, D1FIFOCTR = 0x002E -- correct
- [OK] INTENB0 = 0x0030, INTENB1 = 0x0032, BRDYENB = 0x0036, NRDYENB = 0x0038, BEMPENB = 0x003A -- correct
- [OK] SOFCFG = 0x003C, PHYSET = 0x003E, INTSTS0 = 0x0040, INTSTS1 = 0x0042 -- correct
- [OK] BRDYSTS = 0x0046, NRDYSTS = 0x0048, BEMPSTS = 0x004A -- correct
- [OK] FRMNUM = 0x004C, DVCHGR = 0x004E, USBADDR = 0x0050, USBREQ = 0x0054 -- correct
- [OK] USBVAL = 0x0056, USBINDX = 0x0058, USBLENG = 0x005A -- correct
- [OK] DCPCFG = 0x005C, DCPMAXP = 0x005E, DCPCTR = 0x0060 -- correct
- [OK] PIPESEL = 0x0064, PIPECFG = 0x0068, PIPEMAXP = 0x006C, PIPEPERI = 0x006E -- correct
- [OK] PIPE1CTR = 0x0070, PIPE2CTR = 0x0072, ..., PIPE9CTR = 0x0080 -- correct
- [OK] PIPE1TRE = 0x0090, PIPE1TRN = 0x0092, ..., PIPE5TRE = 0x0098, PIPE5TRN = 0x009A -- correct
- [OK] DEVADD0 = 0x00D0, DEVADD1 = 0x00D2, ..., DEVADD10 = 0x00E4 -- correct
- [OK] PHYSLEW = 0x00F0 -- correct
- [OK] DCPMAXP.DEVSEL[3:0] = b15:b12; DEVADD.USBSPD[1:0] = b9:b8, HUBPORT[2:0] = b12:b10 -- correct
- [OK] SYSCFG bits: SCKE=b10, HSE=b7, DCFM=b6, DRPD=b5, DPRPU=b4, USBE=b0 -- correct
- [OK] SYSSTS0 bits: LNST[1:0]=b1:b0, SOFEN=b5, OVCMON[1:0]=b14:b15 -- correct
- [OK] DVSTCTR0 bits: RHST[2:0], UACT=b4, RESUME=b5, USBRST=b6, RWUPE=b7, WKUP=b8, VBUSEN=b9, EXICEN=b10, HOSTPC=b11 -- correct
- [OK] INTENB0 bits: all 13 interrupt enable flags at correct bit positions -- correct
- [OK] INTSTS0 bits: CTSQ[2:0], VALID, DVSQ[2:0], VBSTS, BRDY, NRDY, BEMP, CTRT, DVST, SOFR, RESM, VBINT -- correct
- [OK] DCPCFG.DIR=b4; DCPCTR.CCPL=b2, PID[1:0]=b1:b0, BSTS=b15, SQCLR/SQSET/SQMON=b8/b9/b10 -- correct
- [OK] PIPECFG bits: TYPE[1:0]=b15:b14, BFRE=b10, DBLB=b9, SHTNAK=b7, DIR=b4, EPNUM[3:0]=b3:b0 -- correct
- [OK] PIPECTR bits: PID[1:0]=b1:b0, PBUSY=b5, SQMON=b6, SQSET=b7, SQCLR=b8, ACLRM=b9, ATREPM=b10, INBUFM=b14, BSTS=b15 -- correct
- [OK] FIFOSEL bits: CURPIPE[3:0]=b3:b0, ISEL=b5, BIGEND=b8, MBW=b10, DREQE=b12, DCLRM=b13 -- correct
- [OK] FIFOCTR bits: FRDY=b13, BCLR=b14, BVAL=b15 -- correct (b13, not b15)
- [FIXED] usb_fifosel_bits_t: k_usb_fifosel_mbw_mask was (3U<<10) (2-bit mask) -- manual p.1939 shows MBW is a 1-bit field at b10 only; corrected to (1U<<10)
- [FIXED] usb_fifosel_bits_t: k_usb_fifosel_mbw_32 = (2U<<10) -- no 32-bit FIFO access mode exists in this peripheral (full-speed only, 16-bit max); removed
- [FIXED] usb_fifosel_bits_t: k_usb_fifosel_rcl = (1U<<14) -- manual p.1939 names b14 "REW" (Buffer Pointer Rewind), not "RCL"; renamed to k_usb_fifosel_rew
- [FIXED] usb_fifosel_bits_t: k_usb_fifosel_frdy = (1U<<15) -- manual p.1939 names b15 "RCNT" (Read Count Mode); FRDY is in FIFOCTR at b13, not here; renamed to k_usb_fifosel_rcnt
- [FIXED] usb_fifoctr_bits_t: k_usb_fifoctr_dtln_mask = 0x0FFF (12-bit mask) -- manual p.1943 shows DTLN[8:0] is a 9-bit field (b8:b0); corrected to 0x01FF
- [AVAILABLE] DPUSR0R (0x000A0400) -- deep standby USB transceiver control register; added to AVAILABLE_FEATURES.md
- [AVAILABLE] DPUSR1R (0x000A0404) -- deep standby USB wakeup interrupt register; added to AVAILABLE_FEATURES.md
- [AVAILABLE] DMA/DTC via D0FIFO/D1FIFO (DREQE/DCLRM bits) -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] Isochronous transfers (pipes 1-2), transaction counter (pipes 1-5), double buffer, auto response mode -- added to AVAILABLE_FEATURES.md

### Commits
- (see next commit)

---

## 2026-04-13 -- Ch 41 SCI (Serial Communications Interface) Registers

**Pages read:** 2037-2216 (Ch 41 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_sci_regs.h

### Findings

- [OK] All 13 base addresses match manual Table 41.1:
  - SCI0=0x0008A000, SCI1=0x0008A020, SCI2=0x0008A040, SCI3=0x0008A060,
    SCI4=0x0008A080, SCI5=0x0008A0A0, SCI6=0x0008A0C0 (SCIj, PCLKB, 0x20 spacing)
  - SCI7=0x000D00E0, SCI8=0x000D0000, SCI9=0x000D0020, SCI10=0x000D0040,
    SCI11=0x000D0060 (SCIi, PCLKA, non-uniform spacing)
  - SCI12=0x0008B300 (SCIh, PCLKB, standalone)
- [OK] All 14 register offsets within rx_sci_regs_t correct:
  - SMR=+0x00, BRR=+0x01, SCR=+0x02, TDR=+0x03, SSR=+0x04, RDR=+0x05
  - SCMR=+0x06, SEMR=+0x07, SNFR=+0x08, SIMR1=+0x09, SIMR2=+0x0A
  - SIMR3=+0x0B, SISR=+0x0C, SPMR=+0x0D
- [OK] sizeof(rx_sci_regs_t) == 14 (static_assert present) -- correct
- [FIXED] SCMR Doxygen comment had wrong bit positions and missing bits:
  - Was: Bit4=SDIR, Bit3=SINV, Bits2-1=Reserved, Bit0=SMIF
  - Manual p.2072 (sec 41.2.16): Bit7=BCP2, Bits6-5=Reserved(w1), Bit4=CHR1,
    Bit3=SDIR, Bit2=SINV, Bit1=Reserved(w1), Bit0=SMIF
  - Corrected: added BCP2 at b7, moved SDIR to b3, moved SINV to b2, added CHR1 at b4
- [FIXED] SEMR Doxygen comment incorrectly grouped b1 and b0 as both Reserved:
  - Was: "Bits 1-0: Reserved"
  - Manual p.2088 (sec 41.2.17): Bit1=Reserved, Bit0=ACS0 (Asynchronous mode clock
    source select 0)
  - Corrected: split to "Bit 1: Reserved" and "Bit 0: ACS0 - Asynchronous mode clock
    source select 0"
- [FIXED] SPMR Doxygen comment was missing CTSE bit and reserved bit at b3:
  - Was: b7=CKPH, b6=CKPOL, b4=MFF, b2=MSS, b0=SSE (b1 completely absent, b3 absent)
  - Manual p.2097 (sec 41.2.21): also b3=Reserved, b1=CTSE (CTS enable)
  - Corrected: added "Bit 3: Reserved" and "Bit 1: CTSE - CTS enable"
- [AVAILABLE] TDRH/TDRL (+0x0E/+0x0F): 9-bit transmit registers -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] RDRH/RDRL (+0x10/+0x11): 9-bit receive registers -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] MDDR (+0x12): Modulation Duty Register -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] DCCR (+0x13): Data Comparison Control Register -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] SSRFIFO, FCR, FDR, LSR, FTDR, FRDR (SCI7-11 FIFO mode) -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] CDR (+0x1A/+0x1B): Comparison Data Register -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] SPTR (+0x1C): Serial Port Register -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] SCI12 extended serial mode registers (ESMER, CR0-CR3, PCR, ICR, STR, STCR,
  CF0DR, CF0CR, CF0RR, PCF1DR, SCF1DR, CF1CR, CF1RR, TCR, TMR, TPRE, TCNT) -- added
- [NOTE] Pages 2117-2216 (sections 41.3 onward) are entirely operational/behavioral:
  async mode, multi-processor, clock-sync, smart card, simple I2C, simple SPI, bit rate
  modulation, extended serial mode, noise cancellation, interrupt sources, event linking,
  usage notes -- no new register addresses or bit constants to verify

### Commits
- bb64ca644 -- fix(sci): correct SCMR/SEMR/SPMR bit descriptions

---

## 2026-04-13 -- Ch 56 S12AD (12-bit ADC) Registers

**Pages read:** 2809-2949 (Ch 56 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_adc_regs.h,
                  star-rx72n-firmware/libs/rx_hal/src/adc.c,
                  star-rx72n-firmware/libs/rx_bus/src/rx_bus_adc.c

### Findings

- [OK] S12AD0 base address: 0x00089000 -- matches manual p.2816
- [OK] S12AD1 base address: 0x00089100 -- matches manual p.2816
- [OK] ADCSR offset: 0x00 (abs 0x9000) -- correct
- [OK] ADANSA0 offset: 0x04 (abs 0x9004) -- correct
- [OK] ADANSA1 offset: 0x06 (abs 0x9006) -- correct
- [OK] ADADS0 offset: 0x08 (abs 0x9008) -- correct
- [OK] ADADS1 offset: 0x0A (abs 0x900A) -- correct
- [OK] ADADC offset: 0x0C (abs 0x900C) -- correct
- [OK] ADCER offset: 0x0E (abs 0x900E) -- correct
- [OK] ADSTRGR offset: 0x10 (abs 0x9010) -- correct
- [OK] ADANSB0 offset: 0x14 (abs 0x9014) -- correct
- [OK] ADDBLDR offset: 0x18 (abs 0x9018) -- correct
- [OK] ADRD offset: 0x1E (abs 0x901E) -- correct
- [FIXED] ADDR0 offset: was 0x20 in struct, manual p.2816 shows abs 0x9022 = offset 0x22.
  Root cause: missing 2-byte reserved gap (0x20-0x21) after ADRD. Added reserved5[2].
  All ADDR0-7 offsets were 2 bytes too low; sizeof was 0x30 instead of 0x32.
- [FIXED] k_s12ad_offset_addr0: 0x20 -> 0x22; k_s12ad_offset_addr7: 0x2E -> 0x30
- [FIXED] sizeof(rx_s12ad_regs_t) static_assert: 0x30 -> 0x32
- [FIXED] static_assert addr0: 0x20->0x22, addr1: 0x22->0x24, addr2: 0x24->0x26,
  addr3: 0x26->0x28, addr4: 0x28->0x2A, addr5: 0x2A->0x2C, addr6: 0x2C->0x2E, addr7: 0x2E->0x30
- [OK] ADCSR.ADST bit 15 = 0x8000 -- manual p.2818
- [FIXED] k_adc_adcsr_adst: was 4096U (0x1000 = ADIE bit 12); must be 32768U (0x8000 = bit 15).
  This caused adc_read() to write to ADIE (interrupt enable) instead of ADST (start conversion).
- [FIXED] ADCSR ADCS scan mode comment: "00=single, 01=continuous" -> "00=single, 01=group, 10=continuous"
- [OK] ADCER.ADPRC[1:0] values: 00=12-bit, 01=10-bit, 10=8-bit -- manual p.2839
- [OK] ADADC values: 000=1x, 001=2x, 010=3x, 011=4x, 101=16x -- manual p.2828
- [FIXED] ADSTRGR TRSA bit field description: said "[5:0]", actually TRSA[5:0] at bits [13:8],
  TRSB[5:0] at bits [5:0] -- manual p.2824
- [FIXED] ADSTRGR trigger 0x09 comment: said "GPTW0 GTADTRA"; manual Table 56.10 p.2826
  shows 0x09 (001001b) = TRG4AN = MTU4.TADCORA
- [FIXED] Example code comment: `0x4000: ADCS[1:0]=01 (continuous scan)` was wrong on both
  counts -- 0x4000 = bits[14:13]=10 = continuous (not 01=group); corrected comment
- [FIXED] internal_get_adc_base() comment table: S12AD1 base was 0x00089200, is 0x00089100
- [FIXED] internal_read_channel_value() offset table: all ADDR0-7 offsets were 2 bytes low
  (0x020->0x022, 0x022->0x024, ..., 0x02E->0x030)
- [OK] k_adc_mstpra_s12ad0 = 17 (MSTPCRA bit 17), k_adc_mstpra_s12ad1 = 16 -- matches
  documented ADC initialization sequence and cross-referenced with rx_bus_adc.c comments
- [OK] rx_bus_adc.c: callback logic, context structs, voltage scaling -- no register-level issues found
- [AVAILABLE] 18 unused ADC features documented in AVAILABLE_FEATURES.md

### Commits
- 8f5fe255c -- fix(adc): correct ADDR0-7 struct offsets, ADST bit, and documentation errors

---

## 2026-04-13 -- Ch 60 RAM Control Registers

**Pages read:** 2977-2990 (Ch 60 complete)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_ram_regs.h

### Findings

- [OK] Memory regions: RAM=0x00000000-0x0007FFFF (512KB), EXRAM=0x00800000-0x0087FFFF (512KB),
  ECCRAM=0x00FF8000-0x00FFFFFF (32KB) -- all match Table 60.1 p.2977
- [OK] Register base addresses: RAM=0x00081200, EXRAM=0x00081240, ECCRAM=0x000812C0 -- all correct
- [OK] RAMMODE (60.2.1) @ 0x00081200: RAMMODE[1:0] at b1:b0; 0x00=disabled, 0x01=enabled -- correct
- [OK] RAMSTS (60.2.2) @ 0x00081201: RAMERR at b0 -- correct
- [OK] RAMECAD (60.2.3) @ 0x00081208: READ field at b18:b3 -- correct (32-bit reg)
- [OK] RAMPRCR (60.2.4) @ 0x00081204: RAMPRCR at b0, KW[6:0] at b7:b1 -- correct
- [OK] EXRAMMODE (60.2.5) @ 0x00081240: same structure as RAMMODE -- correct
- [OK] EXRAMSTS (60.2.6) @ 0x00081241: EXRAMERR at b0 -- correct
- [OK] EXRAMECAD (60.2.7) @ 0x00081248: same structure as RAMECAD -- correct
- [OK] EXRAMPRCR (60.2.8) @ 0x00081244: same structure as RAMPRCR -- correct
- [OK] ECCRAMMODE (60.2.9) @ 0x000812C0: RAMMOD[1:0] at b1:b0; 00=off, 01=prohibited, 10=ECC no-check, 11=ECC with-check -- correct
- [OK] ECCRAM2STS (60.2.10) @ 0x000812C1: ECC2ERR at b0 -- correct
- [OK] ECCRAM1STSEN (60.2.11) @ 0x000812C2: ECC1STSEN at b0 -- correct
- [OK] ECCRAM1STS (60.2.12) @ 0x000812C3: ECC1ERR at b0 -- correct
- [OK] ECCRAMPRCR (60.2.13) @ 0x000812C4: PRCR at b0, KW[6:0] at b7:b1 -- correct
- [OK] ECCRAM2ECAD (60.2.14) @ 0x000812C8: ECC2EAD field at b14:b3 -- correct (32-bit reg)
- [OK] ECCRAM1ECAD (60.2.15) @ 0x000812CC: ECC1EAD field at b14:b3 -- correct (32-bit reg)
- [OK] ECCRAMPRCR2 (60.2.16) @ 0x000812D0: PRCR2 at b0, KW2[6:0] at b7:b1 -- correct
- [OK] ECCRAMETST (60.2.17) @ 0x000812D4: TSTBYP at b0 -- correct
- [OK] MSTPCRC bits (60.4.1): MSTPC0=RAM, MSTPC2=EXRAM, MSTPC6=ECCRAM -- correct
- [OK] All struct offsets and sizeof assertions -- correct
- [FIXED] RAMPRCR protection register key/unlock/lock values were wrong.
  Manual Ch60 s.60.2.4: KW[6:0] = 1111000b occupies bits[7:1] of the register byte.
  Correct register write values: key=0xF0, unlock=0xF1, lock=0xF0.
  Code had: key=0x78, unlock=0x79, lock=0x78.
  With 0x79, the KW[6:0] field in the register reads as 0111100b (not 1111000b),
  so the hardware silently rejects the write -- RAM parity could never be enabled.
  Confirmed by Figure 60.1 ECC test flow: "Write F1h to the ECCRAM protection register."
- [FIXED] EXRAMPRCR (60.2.8): same wrong key/unlock/lock values -- corrected to 0xF0/0xF1/0xF0
- [FIXED] ECCRAMPRCR (60.2.13): same wrong key/unlock/lock values -- corrected to 0xF0/0xF1/0xF0
- [FIXED] ECCRAMPRCR2 (60.2.16): same wrong key/unlock/lock values -- corrected to 0xF0/0xF1/0xF0
- [FIXED] Three static_assert checks updated from 0x79 to 0xF1 with corrected messages
- [FIXED] @details doc comments for EXRAMPRCR, ECCRAMPRCR, ECCRAMPRCR2 updated from "Write 0x79" to "Write 0xF1"

### Commits
- 98c9ac6b1 -- fix(ram): correct protection register key values from 0x79/0x78 to 0xF1/0xF0

---

## 2026-04-13 -- Ch 62 Flash Memory Registers

**Pages read:** 2992-3123 (Ch 62 complete; pages 3124+ are Ch 63 Electrical Characteristics)
**Code file(s):** star-rx72n-firmware/libs/rx_hal/inc/rx72n_flash_regs.h

### Findings

- [OK] ROM Cache: ROMCE=0x00081000, ROMCIV=0x00081004, NCRG0=0x00081040, NCRC0=0x00081044, NCRG1=0x00081048, NCRC1=0x0008104C -- all correct
- [OK] FWEPROR at 0x0008C296, FLWE[1:0] at bits 1:0, reset=0x02 -- correct
- [OK] All 23 FACI register addresses (0x007FE010-0x007FE0E8 range) -- all correct
- [OK] FASTAT: CFAE=b7, CMDLK=b4, DFAE=b3 -- match manual p.3011
- [OK] FAEINT: CFAEIE=b7, CMDLKIE=b4, DFAEIE=b3, reset=0x98 -- match manual p.3012
- [OK] FSTATR: FLWEERR=b6, PRGSPD=b8, ERSSPD=b9, DBFULL=b10, SUSRDY=b11, PRGERR=b12, ERSERR=b13, ILGLERR=b14, FRDY=b15, OTERR=b20, SECERR=b21, FESETERR=b22, ILGCOMERR=b23, reset=0x00008000 -- all correct
- [OK] FENTRYR: FENTRYC=b0, FENTRYD=b7, key=0xAA00, code_pe=0xAA01, data_pe=0xAA80, read_mode=0xAA00 -- correct
- [OK] FSUINITR: SUINIT=b0, key=0x2D00, init_cmd=0x2D01 -- correct
- [OK] FRESETR: FRESET=b0, key=0x2D00, reset_cmd=0x2D01 -- correct
- [OK] FEXCR, FEADDR, FRSPE, FCMDB, FPEADR: all register address constants and basic bit structure -- correct
- [OK] FBCCNT: BCDIR=b0 -- correct
- [OK] FBCSTAT: BCST=b0 -- correct
- [OK] FAWMON, FCPSR, FSUACR: register addresses correct; bit-field details not fully enumerated in code (see AVAILABLE_FEATURES.md)
- [OK] FPCKAR: PCKA[7:0] mask=0x00FF, key=0x1E00 -- correct
- [OK] EEPFCLK at 0x007FC040 -- correct
- [OK] UIDR0-3 at 0xFE7F7D90/7D94/7D98/7D9C (4-byte spacing) -- correct
- [OK] Code flash range: 0xFFC00000-0xFFFFFFFF (4MB) -- correct
- [OK] Data flash range: 0x00100000-0x00107FFF (32KB) -- correct
- [OK] FACI command area at 0x007E0000 -- correct
- [OK] FACI commands: program=0xE8, block_erase=0x20, pe_suspend=0xB0, pe_resume=0xD0, status_clear=0x50, forced_stop=0xB3, blank_check=0x71, config_set=0x40, d0=0xD0 -- all correct
- [FIXED] FCMDR bit masks/shifts were BACKWARDS (p.3015 sec 62.4.14): manual says b7:b0=PCMDR (prev cmd, low byte), b15:b8=CMDR (cur cmd, high byte). Code had pcmdr_mask=0xFF00/shift=8 and cmdr_mask=0x00FF. Corrected to pcmdr_mask=0x00FF/shift=0 and cmdr_mask=0xFF00/shift=8
- [FIXED] Code flash 32KB block count invariant: was 120, manual Fig 62.2 (p.2995) shows blocks 8-133 = 126 blocks. Corrected @invariant comment.
- [FIXED] FPSADDR register name wrong in 3 places: was "Flash Processing Switch Address"; correct name per manual Table 62.1 is "Data Flash Programming Start Address"
- [FIXED] Removed k_faci_cmd_lockbit_read=0x71: there is no separate lock-bit-read FACI command; 0x71 is exclusively the blank check command. Was a duplicate of k_faci_cmd_blank_check with a misleading name.
- [AVAILABLE] Multi-block erase FACI command (0x21) -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] Dual bank flash mode -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] Trusted Memory (TM) protection -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] Background Operation (BGO) -- added to AVAILABLE_FEATURES.md
- [AVAILABLE] FSUACR.SAS startup area selection -- added to AVAILABLE_FEATURES.md

### Commits
- 0246dd8bc -- fix(flash): correct FCMDR bit masks, block count, FPSADDR name, remove lockbit_read

---

(future sessions go below this line)
