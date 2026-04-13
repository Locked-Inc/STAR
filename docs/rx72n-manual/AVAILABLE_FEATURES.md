# RX72N Available Features (Not Implemented in STAR)

This file documents RX72N chip capabilities that exist in the hardware but are not
currently used in STAR firmware. It is **informational only** -- no code should be
added based on this file without an explicit decision to implement the feature.

**How to use:** When working through the manual and you see a peripheral or feature
that the chip supports but STAR does not implement, add a row to the table below.
Do NOT add any code. Just document it here.

**Format:**
```
| Manual Chapter | Feature Name | Manual Pages | Notes |
```

---

## Available Peripherals / Features

| Manual Chapter | Feature | Manual Pages | Notes |
|----------------|---------|--------------|-------|
| Ch 3 (SYSCR0) | External bus mode (SYSCR0.EXBE=1) -- On-chip ROM disabled/enabled extended bus mode | 197, 199-202 | STAR uses single-chip mode only (EXBE=0); SYSCR0 struct field and rome/exbe bit constants exist but external bus mode is never activated |
| Ch 8 (LVDA) | Voltage Monitor 0 (Vdet0) | 316, 327 | Configured via OFS1 option-setting memory (not a runtime register); always-on reset source; STAR does not configure OFS1.LVDAS or OFS1.VDSEL |
| Ch 8 (LVDA) | LVDA ELC Event Link Output | 334 | LVD1/LVD2 can output Vdet passage detection events to ELC for cross-peripheral triggering; STAR code does not configure ELC for LVDA events |
| Ch 9 (Clock Generation) | MOSCWTCR -- Main Clock Oscillator Wait Control Register (0x00080A2) | 360 | Controls stabilization wait time for main clock oscillator; STAR uses default wait time |
| Ch 9 (Clock Generation) | SOSCWTCR -- Sub-Clock Oscillator Wait Control Register (0x00080A3) | 361 | Controls stabilization wait time for sub-clock oscillator; STAR does not use sub-clock |
| Ch 9 (Clock Generation) | MOFCR -- Main Clock Oscillator Forced Oscillation Control Register (0x0008C293) | 362 | Configures oscillator drive strength (MODRV[1:0]) and oscillation mode (MOSEL); STAR uses default |
| Ch 9 (Clock Generation) | HOCOPCR -- HOCO Power Supply Control Register (0x0008C294) | 363 | Can power down HOCO oscillator to save current; STAR does not power it down |
| Ch 9 (Clock Generation) | PACKCR -- Specific-Use Clock Control Register (0x00080044) | 365 | Selects USB clock source (UPLLSEL: 0=PPLL, 1=PLL) and Ethernet clock divider (BCLKDIV); STAR does not configure PACKCR |
| Ch 9 (Clock Generation) | PPLLCR3 -- PPLL Control Register 3 (0x0008004B) | 368 | Sets PPLL output frequency divider (PPLLCR3[1:0]); STAR expects 48 MHz USB but PPLLCR3 is never written in firmware |
| Ch 9 (Clock Generation) | Oscillation Stop Detection (OSTDCR/OSTDSR) | 369-371 | Hardware can detect main-clock oscillation failure and generate NMI/reset; OSTDCR/OSTDSR registers are defined in code but the feature is not enabled (OSTDE bit never set) |
| Ch 9 (Clock Generation) | CLKOUT pin output (CKOCR) | 363-364 | Chip can output any internal clock on the CLKOUT pin via CKOCR; register is defined in code but CKOUT is never enabled (CKOSEL/CKOEN fields never written) |
| Ch 9 (Clock Generation) | SDCLK pin output (SCKCR.PSTOP0) | 340 | SDCLK can be driven onto a pin for SDRAM use; STAR does not use SDRAM so PSTOP0 is left at default (SDCLK output disabled) |
| Ch 9 (Clock Generation) | Sub-clock oscillator (SOSCCR / SOSCK) | 346, 361 | 32.768 kHz sub-clock oscillator is available (used for RTC in other applications); STAR does not start the sub-clock |
| Ch 11 (SBYCR) | Software Standby mode -- SBYCR.SSBY (b15) enters standby; SBYCR.OPE (b14) controls bus pin state in standby | 405, 436-442 | sbycr struct field exists at correct offset 0x0C but no named bit constants (k_sbycr_ssby, k_sbycr_ope) are defined; STAR never executes the WAIT instruction with SSBY=1 |
| Ch 11 (MSTPCRA) | All-Module Clock Stop mode -- MSTPCRA.ACSE (b31) enables the mode; requires all modules set to module-stop state | 406-407, 438-439 | MSTPCRA struct field exists; individual module-stop bits (MSTPA0-31) can be set; but ACSE bit is not defined as a named constant and the all-module clock stop entry sequence is not implemented |
| Ch 21: ELC | ELC link to RTC (periodic event, 0x2Eh) | 839 | RTC not implemented in STAR |
| Ch 21: ELC | ELC link to IWDT underflow/refresh error (0x31h) | 839 | IWDT present but event link unused |
| Ch 21: ELC | ELC link to SCI5 events (0x3A-0x3Dh: error, rx full, tx empty, tx end) | 839 | SCI not implemented in STAR |
| Ch 21: ELC | ELC link to RIIC0 events (0x4E-0x51h: error, rx full, tx empty, tx end) | 839 | RIIC present but event link unused |
| Ch 21: ELC | ELC link to RSPI0 events (0x52-0x56h: error, idle, rx full, tx empty, tx end) | 840 | RSPI present but event link unused |
| Ch 21: ELC | ELC link to LVD1/LVD2 voltage detection (0x5B-0x5Ch) | 840 | LVD present but event link unused |
| Ch 21: ELC | ELC link to oscillation stop detection (0x62h) | 840 | Clock generation circuit, unused |
| Ch 21: ELC | ELC link to DOC data operation condition met (0x6Ah) | 840 | DOC not implemented in STAR |
| Ch 21: ELC | ELC link to Ethernet EPTPC STCA timer events (0xA0-0xABh) | 841 | Ethernet not implemented in STAR |
| Ch 21: ELC | ELOPF register bit definitions (TPU0-3 operation select) | 845 | TPU present but ELOPF bit enums not defined |
| Ch 21: ELC | ELOPH register bit definitions (CMTW0 operation select) | 846 | CMTW not implemented; ELOPH accessor exists but no bit enum |
| Ch 21: ELC | Port group event I/O (Port B / Port E via PGRn/PGCn/PDBFn) | 847-849 | Port event I/O not used in STAR |
| Ch 21: ELC | Single port event link (PEL0-3) | 850 | Port event link not used in STAR |
| Ch 21: ELC | Software event generation (ELSEGR) | 851 | Accessor defined but feature unused in STAR |
| Ch 25 (POE3a) | MTU-based PWM emergency stop via external POE# pins | 1199-1235 | STAR uses GPTW+POEG instead; POE3a would add hi-Z control for MTU0/3/4/6/7 complementary outputs via ICSR1-6 (POE0#, POE4#, POE8#, POE10#, POE11# inputs) |
| Ch 25 (POE3a) | Overcurrent detection hi-Z (OCSR1, OCSR2) | 1214-1216 | Hardware OC-pin triggered output disable for MTU outputs; complements pin-based ICSR detection |
| Ch 25 (POE3a) | Configurable alternative output level (ALR1) | 1219 | Set output state when hi-Z triggered (instead of tristate); could drive a known-safe level |
| Ch 25 (POE3a) | Software output disable (SPOER) | 1220 | Software-only MTU hi-Z without hardware fault; MTUCH0HIZ/67HIZ/34HIZ bits |
| Ch 25 (POE3a) | Conditional hi-Z source expansion (POECR4, POECR5) | 1229-1232 | Add extra POE# flag conditions on top of base POECR2/3 hi-Z; ICxADDMT34/67ZE and ICxADDMT0ZE |
| Ch 28 (TPU) | Compare match / PWM output on TIOC pins | 1446-1452, 1461 | TPU TIOR I/O control register allows output on TIOCnA/B pins; STAR only uses phase counting (input). |
| Ch 28 (TPU) | Input capture mode | 1452-1453 | TPU can latch TCNT into TGR on pin edge; STAR uses phase counting only. |
| Ch 28 (TPU) | Cascaded 32-bit counter (TPU1+TPU2 or TPU4+TPU5) | 1479-1480 | TMDR.TCFD bit + TSYR sync start allows 32-bit chained counter; STAR uses them as independent 16-bit phase counters. |
| Ch 28 (TPU) | Buffer operation (TMDR BFA/BFB bits) | 1481, 1498-1499 | TGR buffer registers for glitch-free compare-match/PWM updates; not used in phase counting mode. |
| Ch 28 (TPU) | A/D conversion start trigger (TIER.TTGE) | 1464, 1494 | TIER bit 7 allows TPU compare match to trigger S12AD conversion; not wired up in STAR. |
| Ch 28 (TPU) | DTC/DMAC activation from TPU interrupts | 1495 | TPU TGIA/TGIB/TCIV interrupt sources can activate DTC or DMAC transfers; not used in STAR. |
| Ch 28 (TPU) | ELC event link (start / restart / input capture) | 1513-1519 | TPU can start counting, restart TCNT, or trigger input capture in response to ELC event signals; not used in STAR. |
| Ch 28 (TPU) | Noise filter (NFCR register) | 1469-1470 | Per-channel noise filter on TCLK/TIOC pins; configurable 1/2/4/8 PCLK sampling; STAR does not enable it. |
| Ch 40: USB | DPUSR0R (Deep Standby USB Transceiver Control/Status Register 0) | 1985-1988 | At 0x000A0400; controls/monitors USB transceiver in deep software standby; not in rx_usb_regs_t struct |
| Ch 40: USB | DPUSR1R (Deep Standby USB Suspend/Resume Interrupt Register) | 1989-1993 | At 0x000A0404; configures wakeup-from-deep-standby on USB resume/attach; not in rx_usb_regs_t struct |
| Ch 40: USB | DMA/DTC transfers via D0FIFO/D1FIFO | 2022, 2021 | DREQE bit (b12 in DnFIFOSEL) issues DMA request; DCLRM bit (b13) auto-clears buffer after read; pipes 1-9 supported |
| Ch 40: USB | Isochronous transfers (pipes 1 and 2) | 2026-2033 | PIPECFG.TYPE=10b selects isochronous; PIPEPERIOD.IFIS flush function; interval counter (PIPEPERI.IITV) |
| Ch 40: USB | Transaction counter (pipes 1-5) | 2016 | PIPEnTRE.TRENB/TRCLR bits + PIPEnTRN register count completed transactions; enables automatic NAK at count end |
| Ch 40: USB | Double buffer mode (pipes 1-5) | 2019 | PIPECFG.DBLB=1 enables double-buffering; INBUFM flag monitors write-side buffer status |
| Ch 40: USB | Auto response mode (OUT-NAK, null auto) | 2018 | PIPEnCTR.ATREPM: OUT-NAK mode (DIR=0) or null auto response mode (DIR=1) for bulk pipes 1-5 |
| Ch 40: USB | OTG VBUS comparator monitoring via OVRCR interrupt | 2012 | OVRCR interrupt fires on USB0_OVRCURA/OVRCURB pin level change; OTG comparator state readable via SYSSTS0.OVCMON |
| Ch 41 SCI (41.2.4) | TDRH/TDRL -- 9-bit transmit data registers (+0x0E/+0x0F) | 2037-2216 | All SCI channels; needed only when CHR=1 and SCMR.CHR1=1 for 9-bit mode |
| Ch 41 SCI (41.2.6) | RDRH/RDRL -- 9-bit receive data registers (+0x10/+0x11) | 2037-2216 | All SCI channels; needed only for 9-bit receive mode |
| Ch 41 SCI (41.2.18) | MDDR -- Modulation Duty Register (+0x12) | 2037-2216 | All SCI channels; enables fractional baud rate via SEMR.BRME=1 |
| Ch 41 SCI (41.2.19) | DCCR -- Data Comparison Control Register (+0x13) | 2037-2216 | All SCI channels; hardware data-match detection with CDR register |
| Ch 41 SCI (41.2.x) | SSRFIFO -- FIFO Status Register (SCI7-11 only) | 2037-2216 | Replaces SSR in FIFO mode; exposes FIFO-specific flags (DR, RDF, TDRE, etc.) |
| Ch 41 SCI (41.2.x) | FCR -- FIFO Control Register (SCI7-11 only) | 2037-2216 | Controls FIFO mode: TX/RX trigger levels, FIFO reset, LOOP-back, RSTRG |
| Ch 41 SCI (41.2.x) | FDR -- FIFO Data Count Register (SCI7-11 only) | 2037-2216 | Read-only TX/RX FIFO data count (16-bit: T[3:0] and R[4:0] sub-fields) |
| Ch 41 SCI (41.2.x) | LSR -- Line Status Register (SCI7-11 only) | 2037-2216 | FIFO overrun error counter (ORER and FNUM/PNUM fields) |
| Ch 41 SCI (41.2.x) | FTDR -- FIFO Transmit Data Register (SCI7-11 only) | 2037-2216 | 9-bit FIFO write path; replaces TDR when FIFO mode enabled |
| Ch 41 SCI (41.2.x) | FRDR -- FIFO Receive Data Register (SCI7-11 only) | 2037-2216 | 9-bit FIFO read path; replaces RDR when FIFO mode enabled |
| Ch 41 SCI (41.2.22) | CDR -- Comparison Data Register (+0x1A/+0x1B) | 2037-2216 | All SCI channels; stores pattern for hardware data-match detection |
| Ch 41 SCI (41.2.23) | SPTR -- Serial Port Register (+0x1C) | 2037-2216 | All SCI channels; direct I/O control of TXD/RXD/SCK pins independent of peripheral |
| Ch 41 SCI (41.2.24) | ESMER -- Extended Serial Mode Enable Register (SCI12: 0x0008B320) | 2037-2216 | SCI12 only; enables Manchester/ARIB-STD-B61 extended serial mode |
| Ch 41 SCI (41.2.25+) | CR0-CR3, PCR, ICR, STR, STCR -- Extended control registers (SCI12 only) | 2037-2216 | SCI12 only; configure/status for Manchester and ARIB extended serial modes |
| Ch 41 SCI (41.2.x) | CF0DR, CF0CR, CF0RR, PCF1DR, SCF1DR, CF1CR, CF1RR (SCI12 only) | 2037-2216 | SCI12 only; control field (sync pattern) detection for ARIB-STD-B61 mode |
| Ch 41 SCI (41.2.x) | TCR, TMR, TPRE, TCNT -- Timer registers (SCI12: 0x0008B330-0x0008B333) | 2037-2216 | SCI12 only; on-chip timer for bit-duration measurement in extended serial mode |
| Ch 56 (S12AD) | Group scan mode (A/B/C) | 2819-2821, 2909-2928 | Three groups with independent triggers; ADANSB0/1, ADANSC0/1, ADSTRGR.TRSB/TRSC, ADGCTRGR, ADGSPCR |
| Ch 56 (S12AD) | Continuous scan mode | 2823, 2901-2908 | ADCSR.ADCS[1:0]=10; ADST is not auto-cleared; STAR uses single-scan only |
| Ch 56 (S12AD) | Hardware trigger (synchronous/asynchronous) | 2824-2826, 2940-2941 | ADCSR.TRGE/EXTRG; ADSTRGR.TRSA/TRSB select MTU/GPT/ELC trigger sources |
| Ch 56 (S12AD) | A/D addition/average mode | 2827-2828, 2938-2939 | ADADC/ADADS0/1; 2x/3x/4x/16x oversampling for noise reduction |
| Ch 56 (S12AD) | Self-diagnosis function | 2838-2840, 2894-2896 | ADCER.DIAGM/DIAGLD/DIAGVAL; converts internal reference voltage; result in ADRD |
| Ch 56 (S12AD) | Double trigger mode | 2830-2832, 2897-2900 | ADCSR.DBLE; duplicates conversion data for synchronous trigger pairs; result in ADDBLDR/ADDBLDRA/ADDBLDRB |
| Ch 56 (S12AD) | Window comparison function (A/B) | 2858-2879, 2932-2934 | ADCMPDR0/1, ADWINLLB, ADWINULB, ADCMPSR0/1, ADCMPSER, ADWINMON, ADCMPBNSR, ADCMPBSR, ADCMPCR; interrupt on threshold crossing |
| Ch 56 (S12AD) | Temperature sensor input (S12AD1 only) | 2850, 2897 | ADEXICR.TSSA/TSSB; result in ADTSDR (offset 0x9129); internal die temperature |
| Ch 56 (S12AD) | Internal reference voltage input (S12AD1 only) | 2851, 2897 | ADEXICR.OCSA/OCSB; result in ADOCDR (offset 0x912B); 1.0V reference |
| Ch 56 (S12AD) | Channel-dedicated sample-and-hold circuits | 2847-2848, 2892-2895 | ADSHCR.SHANS[2:0]/SSTSH; AN000-AN002 (S12AD0) or AN100-AN102 (S12AD1) simultaneous sampling |
| Ch 56 (S12AD) | Constant sampling mode | 2847, 2893, 2903, 2905-2906 | ADSHMSR.SHMD; continuous holding between conversions |
| Ch 56 (S12AD) | Disconnection detection assistance | 2853-2854, 2939-2940 | ADDISCR/ADNDIS; precharge/discharge detect open-circuit analog inputs |
| Ch 56 (S12AD) | Extended analog input (ANEX1) | 2931-2932 | ADEXICR.EXSEL/EXOEN; external op-amp multiplexed through AN100-AN107 |
| Ch 56 (S12AD) | Group priority control | 2912-2928 | ADGSPCR.PGS/GBRSCN/LGRRS/GBRP; group A preempts B/C mid-scan |
| Ch 56 (S12AD) | A/D conversion time setting (ADSAM/ADSAMPR) | 2886-2889 | ADSAM/ADSAMPR; protection register for conversion time setting |
| Ch 56 (S12AD) | DTC/DMAC transfer on scan complete | 2942 | S12ADI/S12GBADI/S12GCADI interrupt triggers DTC/DMAC for zero-CPU data transfer |
| Ch 56 (S12AD) | A/D data register auto-clear (ADCER.ACE) | 2938 | ADCER bit 5; clears ADDRn to 0x0000 after CPU/DTC/DMAC read |
| Ch 56 (S12AD) | Sampling time adjustment (ADSSTRn) | 2845-2846, 2935-2936 | ADSSTR0-7 per-channel sampling time; extend for high-impedance sources |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
