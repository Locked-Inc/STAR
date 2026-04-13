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

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
