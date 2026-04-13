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
| Ch 28 (TPU) | Compare match / PWM output on TIOC pins | 1446-1452, 1461 | TPU TIOR I/O control register allows output on TIOCnA/B pins; STAR only uses phase counting (input). |
| Ch 28 (TPU) | Input capture mode | 1452-1453 | TPU can latch TCNT into TGR on pin edge; STAR uses phase counting only. |
| Ch 28 (TPU) | Cascaded 32-bit counter (TPU1+TPU2 or TPU4+TPU5) | 1479-1480 | TMDR.TCFD bit + TSYR sync start allows 32-bit chained counter; STAR uses them as independent 16-bit phase counters. |
| Ch 28 (TPU) | Buffer operation (TMDR BFA/BFB bits) | 1481, 1498-1499 | TGR buffer registers for glitch-free compare-match/PWM updates; not used in phase counting mode. |
| Ch 28 (TPU) | A/D conversion start trigger (TIER.TTGE) | 1464, 1494 | TIER bit 7 allows TPU compare match to trigger S12AD conversion; not wired up in STAR. |
| Ch 28 (TPU) | DTC/DMAC activation from TPU interrupts | 1495 | TPU TGIA/TGIB/TCIV interrupt sources can activate DTC or DMAC transfers; not used in STAR. |
| Ch 28 (TPU) | ELC event link (start / restart / input capture) | 1513-1519 | TPU can start counting, restart TCNT, or trigger input capture in response to ELC event signals; not used in STAR. |
| Ch 28 (TPU) | Noise filter (NFCR register) | 1469-1470 | Per-channel noise filter on TCLK/TIOC pins; configurable 1/2/4/8 PCLK sampling; STAR does not enable it. |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
