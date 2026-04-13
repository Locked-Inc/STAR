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

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
