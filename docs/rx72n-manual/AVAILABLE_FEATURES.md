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
| Ch 40: USB | DPUSR0R (Deep Standby USB Transceiver Control/Status Register 0) | 1985-1988 | At 0x000A0400; controls/monitors USB transceiver in deep software standby; not in rx_usb_regs_t struct |
| Ch 40: USB | DPUSR1R (Deep Standby USB Suspend/Resume Interrupt Register) | 1989-1993 | At 0x000A0404; configures wakeup-from-deep-standby on USB resume/attach; not in rx_usb_regs_t struct |
| Ch 40: USB | DMA/DTC transfers via D0FIFO/D1FIFO | 2022, 2021 | DREQE bit (b12 in DnFIFOSEL) issues DMA request; DCLRM bit (b13) auto-clears buffer after read; pipes 1-9 supported |
| Ch 40: USB | Isochronous transfers (pipes 1 and 2) | 2026-2033 | PIPECFG.TYPE=10b selects isochronous; PIPEPERIOD.IFIS flush function; interval counter (PIPEPERI.IITV) |
| Ch 40: USB | Transaction counter (pipes 1-5) | 2016 | PIPEnTRE.TRENB/TRCLR bits + PIPEnTRN register count completed transactions; enables automatic NAK at count end |
| Ch 40: USB | Double buffer mode (pipes 1-5) | 2019 | PIPECFG.DBLB=1 enables double-buffering; INBUFM flag monitors write-side buffer status |
| Ch 40: USB | Auto response mode (OUT-NAK, null auto) | 2018 | PIPEnCTR.ATREPM: OUT-NAK mode (DIR=0) or null auto response mode (DIR=1) for bulk pipes 1-5 |
| Ch 40: USB | OTG VBUS comparator monitoring via OVRCR interrupt | 2012 | OVRCR interrupt fires on USB0_OVRCURA/OVRCURB pin level change; OTG comparator state readable via SYSSTS0.OVCMON |

---

## Notes

- STAR currently uses: GPIO, SPI (RSPI), I2C (RIIC), ADC (S12AD), CMT, MTU, GPTW,
  TPU, POEG, CRC, USB, DMAC, IWDT, WDT, MPC, SYSTEM/clocks, RAM/EXRAM/ECCRAM,
  FLASH, MPU, LPC, ELC, LVDA, DTC
- Peripherals confirmed NOT in use: anything added to this table above
