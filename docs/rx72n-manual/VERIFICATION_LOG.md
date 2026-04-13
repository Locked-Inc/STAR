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
- (pending)

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

(future sessions go below this line)
