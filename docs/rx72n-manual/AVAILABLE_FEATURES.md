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
| Ch 62 (Flash) | Multi-block erase FACI command (0x21) | 3029 (Table 62.10) | Erases multiple data flash blocks in one command; STAR only uses single block erase (0x20) |
| Ch 62 (Flash) | Dual bank flash mode (MDE.BANKMD, BANKSEL.BANKSWP) | 3053-3056 (sec 62.10.5) | Splits 4MB code flash into two 2MB banks for live update; STAR runs in linear mode |
| Ch 62 (Flash) | Trusted Memory (TM) protection (TMEF, TMINF) | 3116-3121 (sec 62.19) | Hardware execute-only protection for blocks 8-9; prevents data read from sensitive code regions |
| Ch 62 (Flash) | Background Operation (BGO) | 3114-3115 (sec 62.17.2) | Program/erase one half of code flash while executing from the other half; STAR does not use BGO |
| Ch 62 (Flash) | FSUACR.SAS[1:0] startup area selection | 3019 (sec 62.4.22) | Selects which code flash block is the startup program area; STAR uses default (block 0) |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
