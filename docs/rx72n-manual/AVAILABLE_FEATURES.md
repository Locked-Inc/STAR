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
| Ch 11 (SBYCR) | Software Standby mode -- SBYCR.SSBY (b15) enters standby; SBYCR.OPE (b14) controls bus pin state in standby | 405, 436-442 | sbycr struct field exists at correct offset 0x0C but no named bit constants (k_sbycr_ssby, k_sbycr_ope) are defined; STAR never executes the WAIT instruction with SSBY=1 |
| Ch 11 (MSTPCRA) | All-Module Clock Stop mode -- MSTPCRA.ACSE (b31) enables the mode; requires all modules set to module-stop state | 406-407, 438-439 | MSTPCRA struct field exists; individual module-stop bits (MSTPA0-31) can be set; but ACSE bit is not defined as a named constant and the all-module clock stop entry sequence is not implemented |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
