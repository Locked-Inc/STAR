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
| Ch 25 (POE3a) | MTU-based PWM emergency stop via external POE# pins | 1199-1235 | STAR uses GPTW+POEG instead; POE3a would add hi-Z control for MTU0/3/4/6/7 complementary outputs via ICSR1-6 (POE0#, POE4#, POE8#, POE10#, POE11# inputs) |
| Ch 25 (POE3a) | Overcurrent detection hi-Z (OCSR1, OCSR2) | 1214-1216 | Hardware OC-pin triggered output disable for MTU outputs; complements pin-based ICSR detection |
| Ch 25 (POE3a) | Configurable alternative output level (ALR1) | 1219 | Set output state when hi-Z triggered (instead of tristate); could drive a known-safe level |
| Ch 25 (POE3a) | Software output disable (SPOER) | 1220 | Software-only MTU hi-Z without hardware fault; MTUCH0HIZ/67HIZ/34HIZ bits |
| Ch 25 (POE3a) | Conditional hi-Z source expansion (POECR4, POECR5) | 1229-1232 | Add extra POE# flag conditions on top of base POECR2/3 hi-Z; ICxADDMT34/67ZE and ICxADDMT0ZE |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
