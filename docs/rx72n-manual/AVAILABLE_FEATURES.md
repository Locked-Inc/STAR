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

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
